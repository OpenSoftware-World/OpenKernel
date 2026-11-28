#include "vmem.h"
#include <OpenKernel/Drivers/Cpu/cpu.h>
#include <OpenKernel/Kernel/Components/InterruptServices/interrupt.h>
#include <OpenKernel/Kernel/ErrorHandler/panic.h>
#include <OpenKernel/SystemLib/Memory/mem.h>
#include <OpenKernel/SystemLib/SystemIO/io.h>

/*
 * Virtual memory for 32-bit x86.
 *
 * Physical RAM is identity-mapped: the first 4 MiB with 4 KiB pages (the
 * kernel image, the VGA window and the heap live there) and everything
 * above with 4 MiB PSE pages. The identity map makes every frame the
 * allocator hands out directly addressable, so page tables, swap buffers
 * and zero-fill code never need a temporary mapping.
 *
 * The free virtual range above physical memory is the "arena". Pages in an
 * arena region are not backed by RAM until they are touched (demand paging).
 * When the frame allocator runs dry and a swap backend is registered, cold
 * pages are written to swap and their frames reused, so the machine can
 * address more anonymous memory than it has RAM.
 */

/* Physical memory above this limit is not managed: the top 8 MiB of the
   32-bit address space stay available as virtual arena even on a board
   that claims to populate 4 GiB. */

#define VMEM_PHYS_LIMIT        0xFF800000u
#define VMEM_MAX_FRAMES        (VMEM_PHYS_LIMIT / PAGE_SIZE)
#define VMEM_FRAME_BITMAP_WORDS (VMEM_MAX_FRAMES / 32u)

/* Swap space is limited to 1 GiB of 4 KiB pages (256 Ki slots, 32 KiB of
   bitmap). Swap slot 0 is valid; NONE is the allocation failure sentinel. */

#define VMEM_MAX_SWAP_SLOTS    262144u
#define VMEM_SWAP_BITMAP_WORDS (VMEM_MAX_SWAP_SLOTS / 32u)
#define VMEM_SWAP_SLOT_NONE    0xFFFFFFFFu

#define VMEM_MAX_REGIONS       64u

#define VMEM_4MIB              0x00400000u
#define VMEM_4MIB_MASK         0xFFC00000u
#define VMEM_PTE_ADDR_MASK     0xFFFFF000u
#define VMEM_PTE_SLOT_MASK     0xFFFFFu
#define VMEM_PTE_SLOT_SHIFT    12u
#define VMEM_PAGE_SHIFT        12u

typedef struct {
    uint32_t start;
    uint32_t end;
    uint32_t flags;
} vmem_region_t;

static uint32_t page_dir[1024] __attribute__((aligned(4096)));
static uint32_t page_dir_alt[1024] __attribute__((aligned(4096)));
static uint32_t first_page_table[1024] __attribute__((aligned(4096)));
static uint32_t first_page_table_alt[1024] __attribute__((aligned(4096)));

/* The directory currently described by the helpers. vmem_init() builds the
   next directory in the buffer CR3 does not point at, so re-initialising
   with paging already on cannot wipe the page tables the CPU is walking. */
static uint32_t *page_directory = page_dir;

/* Same idea for the 4 KiB table that identity-maps the first 4 MiB. */
static uint32_t *low_page_table = first_page_table;

static uint32_t frame_bitmap[VMEM_FRAME_BITMAP_WORDS];
static uint32_t swap_bitmap[VMEM_SWAP_BITMAP_WORDS];

/* Number of non-zero entries in each 4 KiB page table. A table is freed as
   soon as the last mapping (resident or swapped) is removed. */
static uint16_t page_table_refs[1024];

static vmem_region_t region_table[VMEM_MAX_REGIONS];
static uint32_t region_count;

static vmem_swap_backend_t swap_backend;
static bool_t swap_configured;
static uint32_t swap_slot_count;
static uint32_t swap_slots_used;

static bool_t vmem_ready;
static uint32_t total_managed_frames;
static uint32_t free_managed_frames;
static uint32_t frame_hint_word;

static uint32_t identity_end;
static uint32_t arena_start;
static uint32_t arena_end;
static uint32_t arena_next;
static uint32_t arena_bytes_used;

static uint32_t mapped_page_count;
static uint32_t swapped_page_count;
static uint32_t page_table_count;
static uint32_t page_fault_count;
static uint32_t demand_page_count;
static uint32_t eviction_count;

/* Clock state for the second-chance reclaim scan. Reset whenever the
   region table changes. */
static uint32_t reclaim_region;
static uint32_t reclaim_page;

static uint32_t vmem_reclaim_frame_locked(void);
static uint32_t *vmem_pte_get_locked(uint32_t vaddr);
static uint32_t *vmem_pte_ensure_locked(uint32_t vaddr);
static int vmem_unmap_page_locked(uint32_t vaddr);
static vmem_region_t *vmem_region_find_locked(uint32_t vaddr);
static void vmem_page_fault(registers_t *regs);

/*
 * Interrupt-safe lock. The saved EFLAGS decide whether interrupts are
 * re-enabled on release, so the same helpers work in process context and
 * inside the page fault handler (which enters through an interrupt gate
 * with IF already clear).
 */
static uint32_t vmem_lock(void) {
    uint32_t flags = eflags_r();
    cli();
    return flags;
}

static void vmem_unlock(uint32_t flags) {
    if (flags & EFLAGS_IF) sti();
}

/* Detect the Page Size Extension used for the 4 MiB identity map. */
static bool_t vmem_cpu_has_pse(void) {
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;

    cpuinfo_data(&eax, &ebx, &ecx, &edx, 0);
    if (eax < 1) return false;

    cpuinfo_data(&eax, &ebx, &ecx, &edx, 1);
    return (edx & (1u << 3)) != 0;
}

static bool_t vmem_bit_test(const uint32_t *bitmap, uint32_t bit) {
    return (bitmap[bit >> 5] & (1u << (bit & 31u))) != 0;
}

static void vmem_bit_clear(uint32_t *bitmap, uint32_t bit) {
    bitmap[bit >> 5] &= ~(1u << (bit & 31u));
}

/*
 * Physical frame allocator. Frame 0 is never handed out, so 0 is a safe
 * failure sentinel. When the bitmap is empty the swap clock is given one
 * chance to evict a cold page; an evicted frame stays marked used and is
 * returned directly to the caller.
 */
static uint32_t vmem_frame_alloc_locked(void) {
    uint32_t scanned;

    /* Avoid a full bitmap scan when the last allocation already put the
       system under memory pressure. */
    if (free_managed_frames == 0) {
        return vmem_reclaim_frame_locked();
    }

    for (scanned = 0; scanned < VMEM_FRAME_BITMAP_WORDS; scanned++) {
        uint32_t word = (frame_hint_word + scanned) % VMEM_FRAME_BITMAP_WORDS;
        uint32_t bits = frame_bitmap[word];

        if (bits != 0xFFFFFFFFu) {
            uint32_t bit = (uint32_t)__builtin_ctz(~bits);
            uint32_t index = word * 32u + bit;

            if (index >= total_managed_frames) break;

            frame_bitmap[word] |= (1u << bit);
            free_managed_frames--;
            frame_hint_word = word;
            return index * PAGE_SIZE;
        }
    }

    return vmem_reclaim_frame_locked();
}

static void vmem_frame_free_locked(uint32_t frame) {
    uint32_t index;
    uint32_t word;

    if (frame == 0 || frame >= VMEM_PHYS_LIMIT) return;

    index = frame >> VMEM_PAGE_SHIFT;
    if (index >= total_managed_frames) return;

    word = index >> 5;

    /* Only frames that are marked used can be freed. This rejects double
       frees and keeps the reserved low-memory frames reserved. */
    if (vmem_bit_test(frame_bitmap, index)) {
        vmem_bit_clear(frame_bitmap, index);
        free_managed_frames++;
        if (word < frame_hint_word) frame_hint_word = word;
    }
}

static uint32_t vmem_swap_slot_alloc_locked(void) {
    uint32_t scanned;

    if (!swap_configured || swap_slots_used >= swap_slot_count) {
        return VMEM_SWAP_SLOT_NONE;
    }

    for (scanned = 0; scanned < VMEM_SWAP_BITMAP_WORDS; scanned++) {
        uint32_t word = swap_bitmap[scanned];

        if (word != 0xFFFFFFFFu) {
            uint32_t bit = (uint32_t)__builtin_ctz(~word);
            uint32_t slot = scanned * 32u + bit;

            if (slot >= swap_slot_count) break;

            swap_bitmap[scanned] |= (1u << bit);
            swap_slots_used++;
            return slot;
        }
    }

    return VMEM_SWAP_SLOT_NONE;
}

static void vmem_swap_slot_free_locked(uint32_t slot) {
    if (slot >= swap_slot_count) return;

    if (vmem_bit_test(swap_bitmap, slot)) {
        vmem_bit_clear(swap_bitmap, slot);
        if (swap_slots_used > 0) swap_slots_used--;
    }
}

/*
 * Page table helpers. A present PDE without PAGE_PSE points at a 4 KiB
 * page table; with PAGE_PSE it is a direct 4 MiB identity page and has no
 * page table to walk.
 */
static uint32_t *vmem_pte_get_locked(uint32_t vaddr) {
    uint32_t pde = page_directory[vaddr >> 22];
    uint32_t *table;

    if (!(pde & PAGE_PRESENT) || (pde & PAGE_PSE)) return NULL;

    table = (uint32_t *)(pde & VMEM_PTE_ADDR_MASK);
    return &table[(vaddr >> VMEM_PAGE_SHIFT) & 0x3FFu];
}

static uint32_t *vmem_pte_ensure_locked(uint32_t vaddr) {
    uint32_t index = vaddr >> 22;
    uint32_t pde = page_directory[index];
    uint32_t *table;

    if (pde & PAGE_PRESENT) {
        if (pde & PAGE_PSE) return NULL;
        table = (uint32_t *)(pde & VMEM_PTE_ADDR_MASK);
        return &table[(vaddr >> VMEM_PAGE_SHIFT) & 0x3FFu];
    }

    /* There is no page table for this 4 MiB slot yet. The new table lives
       in identity-mapped RAM, so it is writable right away. */
    {
        uint32_t frame = vmem_frame_alloc_locked();

        if (frame == 0) return NULL;

        memset((void *)frame, 0, PAGE_SIZE);
        page_directory[index] = frame | PAGE_PRESENT | PAGE_RW | PAGE_USER;
        page_table_refs[index] = 0;
        page_table_count++;
        table = (uint32_t *)frame;
    }

    return &table[(vaddr >> VMEM_PAGE_SHIFT) & 0x3FFu];
}

/*
 * Write one resident page to swap and turn its PTE into a swap descriptor.
 * The frame stays marked used in the bitmap and is handed back to the
 * caller; it is never freed in between.
 */
static uint32_t vmem_swap_out_locked(uint32_t vaddr, uint32_t *pte) {
    uint32_t slot;
    uint32_t frame;
    uint32_t entry;
    uint32_t saved_flags;

    if (!swap_configured) return 0;

    slot = vmem_swap_slot_alloc_locked();
    if (slot == VMEM_SWAP_SLOT_NONE) return 0;

    frame = *pte & VMEM_PTE_ADDR_MASK;
    entry = *pte;

    if (swap_backend.write(swap_backend.context, slot,
                           (const void *)frame) != 0) {
        vmem_swap_slot_free_locked(slot);
        return 0;
    }

    saved_flags = entry & (PAGE_RW | PAGE_USER | VMEM_PTE_OWNED);
    *pte = ((slot << VMEM_PTE_SLOT_SHIFT) & VMEM_PTE_ADDR_MASK) |
           VMEM_PTE_SWAP | saved_flags;

    vmem_invlpg(vaddr);

    if (mapped_page_count > 0) mapped_page_count--;
    swapped_page_count++;
    eviction_count++;

    return frame;
}

/*
 * Second-chance clock over every swappable region. The first pass clears
 * the accessed bit of hot pages; a page that was touched again has it set
 * once more and is skipped, so only consistently cold pages are written
 * out. The scan budget is two full passes, bounded by the number of pages
 * in swappable regions.
 */
static uint32_t vmem_reclaim_frame_locked(void) {
    uint32_t pages = 0;
    uint32_t budget;
    uint32_t i;

    if (!swap_configured) return 0;

    for (i = 0; i < region_count; i++) {
        if (region_table[i].flags & VMEM_REGION_SWAPPABLE) {
            pages += (region_table[i].end - region_table[i].start) / PAGE_SIZE;
        }
    }
    if (pages == 0) return 0;

    budget = pages * 2u;
    if (budget < pages) budget = 0xFFFFFFFFu;

    while (budget-- > 0) {
        vmem_region_t *region;
        uint32_t page_count;
        uint32_t vaddr;
        uint32_t *pte;

        if (region_count == 0) return 0;

        if (reclaim_region >= region_count) {
            reclaim_region = 0;
            reclaim_page = 0;
        }

        region = &region_table[reclaim_region];
        page_count = (region->end - region->start) / PAGE_SIZE;

        if (!(region->flags & VMEM_REGION_SWAPPABLE) ||
            reclaim_page >= page_count) {
            reclaim_region++;
            reclaim_page = 0;
            continue;
        }

        vaddr = region->start + reclaim_page * PAGE_SIZE;
        reclaim_page++;

        pte = vmem_pte_get_locked(vaddr);
        if (pte == NULL || !(*pte & PAGE_PRESENT)) continue;

        if (*pte & PAGE_ACCESSED) {
            *pte &= ~PAGE_ACCESSED;
            /* Drop the TLB entry so the next access re-sets the bit in
               memory. Without this a hot page could look cold. */
            vmem_invlpg(vaddr);
            continue;
        }

        {
            uint32_t frame = vmem_swap_out_locked(vaddr, pte);
            if (frame != 0) return frame;
        }
    }

    return 0;
}

static vmem_region_t *vmem_region_find_locked(uint32_t vaddr) {
    uint32_t i;

    for (i = 0; i < region_count; i++) {
        if (vaddr >= region_table[i].start && vaddr < region_table[i].end) {
            return &region_table[i];
        }
    }
    return NULL;
}

/* Reserve virtual space from the arena. The bump pointer never moves back,
   so a freed range is only reusable after a reboot. */
static uint32_t vmem_arena_reserve_locked(uint32_t size) {
    uint32_t base;

    if (size == 0 || arena_start == 0) return 0;

    base = PAGE_ALIGN(arena_next);
    if (base < arena_start || base >= arena_end) return 0;
    if (size > arena_end - base) return 0;

    arena_next = base + size;
    if (arena_next < base) return 0;

    arena_bytes_used = arena_next - arena_start;
    return base;
}

static int vmem_region_add_locked(uint32_t start, uint32_t size,
                                  uint32_t region_flags) {
    uint32_t end;
    uint32_t i;
    uint32_t vaddr;

    if (region_count >= VMEM_MAX_REGIONS) return -1;
    if (size == 0 || size > 0xFFFFF000u) return -1;
    if (start & (PAGE_SIZE - 1u)) return -1;
    if (start < arena_start || start >= arena_end) return -1;
    if (size > 0xFFFFFFFFu - start) return -1;

    end = PAGE_ALIGN(start + size);
    if (end < start || end > arena_end) return -1;

    for (i = 0; i < region_count; i++) {
        if (start < region_table[i].end && end > region_table[i].start) {
            return -1;
        }
    }

    /* Never overlap a page that is already mapped (for example a device
       mapping made with vmem_map_mmio()). */
    for (vaddr = start; vaddr < end; vaddr += PAGE_SIZE) {
        uint32_t *pte = vmem_pte_get_locked(vaddr);

        if (pte != NULL && (*pte & (PAGE_PRESENT | VMEM_PTE_SWAP))) {
            return -1;
        }
    }

    if (end > arena_next) {
        arena_next = end;
        arena_bytes_used = arena_next - arena_start;
    }

    region_table[region_count].start = start;
    region_table[region_count].end = end;
    region_table[region_count].flags = region_flags;
    region_count++;

    reclaim_region = 0;
    reclaim_page = 0;
    return 0;
}

static int vmem_unmap_page_locked(uint32_t vaddr) {
    uint32_t index = vaddr >> 22;
    uint32_t *pte = vmem_pte_get_locked(vaddr);
    uint32_t entry;

    if (pte == NULL) return -1;

    entry = *pte;
    if (!(entry & PAGE_PRESENT) && !(entry & VMEM_PTE_SWAP)) return -1;

    if (entry & PAGE_PRESENT) {
        if (entry & VMEM_PTE_OWNED) {
            vmem_frame_free_locked(entry & VMEM_PTE_ADDR_MASK);
        }
        if (mapped_page_count > 0) mapped_page_count--;
    } else {
        vmem_swap_slot_free_locked(
            (entry >> VMEM_PTE_SLOT_SHIFT) & VMEM_PTE_SLOT_MASK);
        if (swapped_page_count > 0) swapped_page_count--;
    }

    *pte = 0;
    if (page_table_refs[index] > 0) page_table_refs[index]--;
    vmem_invlpg(vaddr);

    /* Drop an empty page table so a region that is created and destroyed
       repeatedly does not leak frames. */
    if (page_table_refs[index] == 0) {
        uint32_t pde = page_directory[index];

        if ((pde & PAGE_PRESENT) && !(pde & PAGE_PSE)) {
            page_directory[index] = 0;
            vmem_frame_free_locked(pde & VMEM_PTE_ADDR_MASK);
            page_table_count--;
            vmem_flush_tlb();
        }
    }

    return 0;
}

static int vmem_region_remove_locked(uint32_t vaddr) {
    vmem_region_t *region = vmem_region_find_locked(vaddr);
    uint32_t index;
    uint32_t i;
    uint32_t page;

    if (region == NULL) return -1;

    for (page = region->start; page < region->end; page += PAGE_SIZE) {
        (void)vmem_unmap_page_locked(page);
    }

    index = (uint32_t)(region - region_table);
    for (i = index; i + 1u < region_count; i++) {
        region_table[i] = region_table[i + 1u];
    }
    region_count--;

    reclaim_region = 0;
    reclaim_page = 0;
    return 0;
}

static int vmem_map_page_locked(uint32_t vaddr, uint32_t paddr,
                                uint32_t flags) {
    uint32_t *pte;
    uint32_t index;
    uint32_t entry;

    if (!vmem_ready) return -1;
    if (vaddr & (PAGE_SIZE - 1u)) return -1;
    if (paddr & (PAGE_SIZE - 1u)) return -1;
    if (vaddr < arena_start || vaddr >= arena_end) return -1;
    if (paddr > 0xFFFFF000u) return -1;

    pte = vmem_pte_ensure_locked(vaddr);
    if (pte == NULL) return -1;

    entry = *pte;
    if (entry & (PAGE_PRESENT | VMEM_PTE_SWAP)) return -1;

    index = vaddr >> 22;
    *pte = (paddr & VMEM_PTE_ADDR_MASK) |
           (flags & (PAGE_RW | PAGE_USER | PAGE_PWT | PAGE_PCD |
                     PAGE_GLOBAL)) |
           PAGE_PRESENT;

    if (page_table_refs[index] < 0xFFFFu) page_table_refs[index]++;
    mapped_page_count++;
    vmem_invlpg(vaddr);
    return 0;
}

/*
 * Page fault handler. The CPU leaves the faulting address in CR2 and the
 * error code in the register frame. A not-present fault inside a managed
 * region is either a first touch (zero-fill) or a swapped page coming
 * back; anything else is a real kernel bug and panics.
 */
static void vmem_page_fault(registers_t *regs) {
    uint32_t fault_addr;
    uint32_t lock_flags;
    vmem_region_t *region;
    uint32_t *pte;
    uint32_t entry;

    __asm__ __volatile__("mov %%cr2, %0" : "=r"(fault_addr));
    page_fault_count++;

    /* Bit 0 of the error code is set for protection faults, clear for a
       missing page. The numbered flags double as the PTE bits here. */
    if (regs->error_code & PAGE_PRESENT) {
        kernel_panic("Page protection violation", "E_VMEM_PF_PROT");
    }

    lock_flags = vmem_lock();

    region = vmem_region_find_locked(fault_addr);
    if (region == NULL) {
        vmem_unlock(lock_flags);
        kernel_panic("Page fault outside a managed region", "E_VMEM_PF_REGION");
    }

    if ((regs->error_code & PAGE_RW) && !(region->flags & VMEM_REGION_WRITE)) {
        vmem_unlock(lock_flags);
        kernel_panic("Write to a read-only virtual region", "E_VMEM_PF_WRITE");
    }

    if ((regs->error_code & PAGE_USER) && !(region->flags & VMEM_REGION_USER)) {
        vmem_unlock(lock_flags);
        kernel_panic("User access to a kernel virtual region", "E_VMEM_PF_USER");
    }

    pte = vmem_pte_ensure_locked(fault_addr);
    if (pte == NULL) {
        vmem_unlock(lock_flags);
        kernel_panic("Cannot allocate a page table", "E_VMEM_PF_PT");
    }

    entry = *pte;

    if (entry & VMEM_PTE_SWAP) {
        uint32_t slot = (entry >> VMEM_PTE_SLOT_SHIFT) & VMEM_PTE_SLOT_MASK;
        uint32_t frame;

        if (!swap_configured || slot >= swap_slot_count) {
            vmem_unlock(lock_flags);
            kernel_panic("Invalid swap slot in a page table", "E_VMEM_SWAP_SLOT");
        }

        frame = vmem_frame_alloc_locked();
        if (frame == 0) {
            vmem_unlock(lock_flags);
            kernel_panic("Virtual memory exhausted", "E_VMEM_OOM_SWAP_IN");
        }

        if (swap_backend.read(swap_backend.context, slot,
                              (void *)frame) != 0) {
            vmem_frame_free_locked(frame);
            vmem_unlock(lock_flags);
            kernel_panic("Swap read failed", "E_VMEM_SWAP_READ");
        }

        *pte = frame | PAGE_PRESENT | VMEM_PTE_OWNED |
               (entry & (PAGE_RW | PAGE_USER | PAGE_ACCESSED | PAGE_DIRTY));

        vmem_swap_slot_free_locked(slot);
        vmem_invlpg(fault_addr);

        if (swapped_page_count > 0) swapped_page_count--;
        mapped_page_count++;
    } else if (entry & PAGE_PRESENT) {
        /* The mapping exists but a stale TLB entry raised the fault.
           Dropping the entry and retrying is the correct recovery. */
        vmem_invlpg(fault_addr);
    } else {
        uint32_t frame = vmem_frame_alloc_locked();
        uint32_t new_entry;

        if (frame == 0) {
            vmem_unlock(lock_flags);
            kernel_panic("Virtual memory exhausted", "E_VMEM_OOM");
        }

        memset((void *)frame, 0, PAGE_SIZE);

        new_entry = frame | PAGE_PRESENT | VMEM_PTE_OWNED;
        if (region->flags & VMEM_REGION_WRITE) new_entry |= PAGE_RW;
        if (region->flags & VMEM_REGION_USER) new_entry |= PAGE_USER;

        *pte = new_entry;
        page_table_refs[fault_addr >> 22]++;
        vmem_invlpg(fault_addr);

        mapped_page_count++;
        demand_page_count++;
    }

    vmem_unlock(lock_flags);
}

void vmem_install_fault_handler(void) {
    interrupt_register_handler(14, vmem_page_fault);
}

void load_page_dir(uint32_t *pd) {
    __asm__ __volatile__("mov %0, %%cr3" :: "r"(pd) : "memory");
}

void enable_paging(void) {
    uint32_t cr0;

    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;
    __asm__ __volatile__("mov %0, %%cr0" :: "r"(cr0) : "memory");
}

/*
 * Pick a page directory buffer that is not the one CR3 currently points at.
 * Building into the inactive buffer makes a second vmem_init() safe even
 * while paging is already enabled.
 */
static uint32_t *vmem_select_page_directory(void) {
    uint32_t cr0;
    uint32_t cr3;
    uint32_t active;

    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));

    if ((cr0 & 0x80000000u) != 0) {
        active = cr3 & VMEM_PTE_ADDR_MASK;
        if (active == (uint32_t)page_dir) return page_dir_alt;
        if (active == (uint32_t)page_dir_alt) return page_dir;
    }

    return page_dir;
}

/* Pick the low 4 MiB page table the active directory is not using. */
static uint32_t *vmem_select_low_page_table(void) {
    uint32_t cr0;
    uint32_t cr3;
    uint32_t active_pde0;

    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));

    if ((cr0 & 0x80000000u) != 0) {
        active_pde0 = ((const uint32_t *)(cr3 & VMEM_PTE_ADDR_MASK))[0] &
                      VMEM_PTE_ADDR_MASK;
        if (active_pde0 == (uint32_t)first_page_table) {
            return first_page_table_alt;
        }
        if (active_pde0 == (uint32_t)first_page_table_alt) {
            return first_page_table;
        }
    }

    return first_page_table;
}

/*
 * Legacy initialiser kept for source compatibility: identity-map the first
 * 4 MiB with 4 KiB pages and switch paging on. New code should use
 * vmem_init(), which also maps all RAM and manages the virtual arena.
 */
void paging_init(void) {
    uint32_t i;

    page_directory = vmem_select_page_directory();
    low_page_table = vmem_select_low_page_table();

    for (i = 0; i < 1024; i++) {
        low_page_table[i] = (i * PAGE_SIZE) | PAGE_PRESENT | PAGE_RW;
        page_directory[i] = 0;
    }

    page_directory[0] = (uint32_t)low_page_table | PAGE_PRESENT | PAGE_RW;

    vmem_ready = false;
    arena_start = 0;
    arena_end = 0;
    arena_next = 0;
    arena_bytes_used = 0;

    load_page_dir(page_directory);
    enable_paging();
}

int vmem_init(const vmem_config_t *config) {
    uint32_t lock_flags;
    uint32_t phys_start;
    uint32_t phys_end;
    uint32_t map_end;
    uint32_t addr;
    uint32_t frame;
    uint32_t wanted_arena_start;
    uint32_t wanted_arena_end;

    if (config == NULL) return -1;
    if (!vmem_cpu_has_pse()) return -1;

    phys_start = PAGE_ALIGN(config->phys_start);
    phys_end = PAGE_ALIGN_DOWN(config->phys_end);

    if (phys_start < PAGE_SIZE) phys_start = PAGE_SIZE;
    if (phys_end > VMEM_PHYS_LIMIT) phys_end = VMEM_PHYS_LIMIT;
    if (phys_end <= phys_start) return -1;
    if (phys_start >= VMEM_PHYS_LIMIT) return -1;

    page_directory = vmem_select_page_directory();
    low_page_table = vmem_select_low_page_table();

    lock_flags = vmem_lock();

    /* Reset every bookkeeping structure so a repeated init starts from a
       known state even after the legacy initialiser ran. */
    memset(page_directory, 0, 1024u * sizeof(uint32_t));
    memset(low_page_table, 0, 1024u * sizeof(uint32_t));
    memset(frame_bitmap, 0xFF, sizeof(frame_bitmap));
    memset(swap_bitmap, 0, sizeof(swap_bitmap));
    memset(page_table_refs, 0, sizeof(page_table_refs));
    memset(region_table, 0, sizeof(region_table));
    memset(&swap_backend, 0, sizeof(swap_backend));

    vmem_ready = false;
    swap_configured = false;
    swap_slot_count = 0;
    swap_slots_used = 0;
    region_count = 0;
    total_managed_frames = phys_end / PAGE_SIZE;
    free_managed_frames = 0;
    frame_hint_word = 0;
    mapped_page_count = 0;
    swapped_page_count = 0;
    page_table_count = 0;
    page_fault_count = 0;
    demand_page_count = 0;
    eviction_count = 0;
    reclaim_region = 0;
    reclaim_page = 0;
    arena_bytes_used = 0;

    /* The first 4 MiB stay on 4 KiB pages: this is where the kernel, the
       stack, the heap and the VGA text buffer live. */
    for (frame = 0; frame < 1024; frame++) {
        low_page_table[frame] = (frame * PAGE_SIZE) | PAGE_PRESENT | PAGE_RW;
    }
    page_directory[0] = (uint32_t)low_page_table | PAGE_PRESENT | PAGE_RW;

    /* Everything above 4 MiB is identity-mapped with 4 MiB PSE pages.
       Rounding up keeps the arena from starting inside a mapped page. */
    map_end = phys_end;
    if (map_end & (VMEM_4MIB - 1u)) {
        map_end = (map_end & VMEM_4MIB_MASK) + VMEM_4MIB;
    }
    if (map_end > VMEM_PHYS_LIMIT) map_end = VMEM_PHYS_LIMIT;

    for (addr = VMEM_4MIB; addr < map_end; addr += VMEM_4MIB) {
        page_directory[addr >> 22] = addr | PAGE_PRESENT | PAGE_RW | PAGE_PSE;
    }
    identity_end = map_end;

    /* Free the frames the caller handed over. Everything below
       phys_start stays used, which keeps the kernel image, its heap and
       frame 0 reserved. */
    for (frame = phys_start >> VMEM_PAGE_SHIFT; frame < total_managed_frames;
         frame++) {
        uint32_t index = frame >> 5;
        uint32_t bit = frame & 31u;

        if (frame_bitmap[index] & (1u << bit)) {
            frame_bitmap[index] &= ~(1u << bit);
            free_managed_frames++;
        }
    }

    wanted_arena_start = config->arena_start;
    if (wanted_arena_start == 0) wanted_arena_start = identity_end;
    wanted_arena_start = PAGE_ALIGN(wanted_arena_start);

    wanted_arena_end = config->arena_end;
    if (wanted_arena_end == 0) wanted_arena_end = 0xFFFFF000u;
    wanted_arena_end = PAGE_ALIGN_DOWN(wanted_arena_end);

    if (wanted_arena_end > 0xFFFFF000u) wanted_arena_end = 0xFFFFF000u;
    if (wanted_arena_start < identity_end) wanted_arena_start = identity_end;

    if (wanted_arena_start >= wanted_arena_end) {
        arena_start = 0;
        arena_end = 0;
        arena_next = 0;
    } else {
        arena_start = wanted_arena_start;
        arena_end = wanted_arena_end;
        arena_next = wanted_arena_start;
    }

    vmem_ready = true;

    if (config->install_fault_handler) {
        vmem_install_fault_handler();
    }

    /* Enable 4 MiB pages before loading a directory that uses them, then
       turn paging on. Both structures live below 4 MiB and are already
       reachable through the identity map. */
    {
        uint32_t cr4;

        __asm__ __volatile__("mov %%cr4, %0" : "=r"(cr4));
        cr4 |= (1u << 4);                    /* CR4.PSE */
        __asm__ __volatile__("mov %0, %%cr4" :: "r"(cr4) : "memory");
    }

    load_page_dir(page_directory);
    enable_paging();

    vmem_unlock(lock_flags);
    return 0;
}

int vmem_swap_set_backend(const vmem_swap_backend_t *backend, uint32_t slots) {
    uint32_t lock_flags;

    if (backend == NULL || backend->read == NULL || backend->write == NULL) {
        return -1;
    }
    if (slots == 0) return -1;
    if (slots > VMEM_MAX_SWAP_SLOTS) slots = VMEM_MAX_SWAP_SLOTS;

    lock_flags = vmem_lock();

    /* Changing the backend while a swapped page is outstanding would make
       that page unreadable. Refuse until the slots drain. */
    if (swap_slots_used != 0) {
        vmem_unlock(lock_flags);
        return -1;
    }

    memset(swap_bitmap, 0, sizeof(swap_bitmap));
    swap_backend = *backend;
    swap_slot_count = slots;
    swap_slots_used = 0;
    swap_configured = true;

    vmem_unlock(lock_flags);
    return 0;
}

void *vmem_alloc(uint32_t size) {
    return vmem_alloc_ex(size, VMEM_REGION_WRITE | VMEM_REGION_SWAPPABLE);
}

void *vmem_alloc_ex(uint32_t size, uint32_t region_flags) {
    uint32_t lock_flags;
    uint32_t base;
    uint32_t bytes;
    int result;

    if (!vmem_ready || size == 0) return NULL;
    if (size > 0xFFFFF000u) return NULL;

    bytes = PAGE_ALIGN(size);
    if (bytes == 0) return NULL;

    lock_flags = vmem_lock();

    base = PAGE_ALIGN(arena_next);
    if (base < arena_start || base >= arena_end ||
        bytes > arena_end - base) {
        vmem_unlock(lock_flags);
        return NULL;
    }

    result = vmem_region_add_locked(base, bytes, region_flags);

    vmem_unlock(lock_flags);

    if (result != 0) return NULL;
    return (void *)base;
}

int vmem_free(void *ptr) {
    uint32_t lock_flags;
    int result;

    if (!vmem_ready || ptr == NULL) return -1;

    lock_flags = vmem_lock();
    result = vmem_region_remove_locked((uint32_t)ptr);
    vmem_unlock(lock_flags);

    return result;
}

int vmem_region_add(uint32_t vaddr, uint32_t size, uint32_t region_flags) {
    uint32_t lock_flags;
    int result;

    if (!vmem_ready) return -1;

    lock_flags = vmem_lock();
    result = vmem_region_add_locked(vaddr, size, region_flags);
    vmem_unlock(lock_flags);

    return result;
}

int vmem_region_remove(uint32_t vaddr) {
    uint32_t lock_flags;
    int result;

    if (!vmem_ready) return -1;

    lock_flags = vmem_lock();
    result = vmem_region_remove_locked(vaddr);
    vmem_unlock(lock_flags);

    return result;
}

int vmem_region_find(uint32_t vaddr, uint32_t *start, uint32_t *end,
                     uint32_t *region_flags) {
    uint32_t lock_flags;
    vmem_region_t *region;

    if (!vmem_ready) return -1;

    lock_flags = vmem_lock();
    region = vmem_region_find_locked(vaddr);
    if (region != NULL) {
        if (start != NULL) *start = region->start;
        if (end != NULL) *end = region->end;
        if (region_flags != NULL) *region_flags = region->flags;
    }
    vmem_unlock(lock_flags);

    return (region != NULL) ? 0 : -1;
}

int vmem_map_page(uint32_t vaddr, uint32_t paddr, uint32_t flags) {
    uint32_t lock_flags;
    int result;

    if (!vmem_ready) return -1;

    lock_flags = vmem_lock();
    result = vmem_map_page_locked(vaddr, paddr, flags);
    vmem_unlock(lock_flags);

    return result;
}

int vmem_unmap_page(uint32_t vaddr) {
    uint32_t lock_flags;
    int result;

    if (!vmem_ready) return -1;

    lock_flags = vmem_lock();
    result = vmem_unmap_page_locked(vaddr);
    vmem_unlock(lock_flags);

    return result;
}

uint32_t vmem_get_physical(uint32_t vaddr) {
    uint32_t lock_flags;
    uint32_t pde;
    uint32_t *pte;
    uint32_t physical = 0;

    if (!vmem_ready) return 0;

    lock_flags = vmem_lock();

    pde = page_directory[vaddr >> 22];
    if (pde & PAGE_PRESENT) {
        if (pde & PAGE_PSE) {
            physical = (pde & VMEM_4MIB_MASK) | (vaddr & (VMEM_4MIB - 1u));
        } else {
            pte = vmem_pte_get_locked(vaddr);
            if (pte != NULL && (*pte & PAGE_PRESENT)) {
                physical = (*pte & VMEM_PTE_ADDR_MASK) |
                           (vaddr & (PAGE_SIZE - 1u));
            }
        }
    }

    vmem_unlock(lock_flags);
    return physical;
}

void *vmem_map_mmio(uint32_t paddr, uint32_t size, uint32_t flags) {
    uint32_t lock_flags;
    uint32_t bytes;
    uint32_t base;
    uint32_t offset;

    if (!vmem_ready || size == 0) return NULL;
    if (paddr & (PAGE_SIZE - 1u)) return NULL;
    if (size > 0xFFFFF000u) return NULL;

    bytes = PAGE_ALIGN(size);
    if (bytes == 0) return NULL;
    if (paddr > 0xFFFFF000u - (bytes - 1u)) return NULL;

    lock_flags = vmem_lock();

    base = vmem_arena_reserve_locked(bytes);
    if (base == 0) {
        vmem_unlock(lock_flags);
        return NULL;
    }

    for (offset = 0; offset < bytes; offset += PAGE_SIZE) {
        if (vmem_map_page_locked(base + offset, paddr + offset, flags) != 0) {
            uint32_t undo;

            for (undo = 0; undo < offset; undo += PAGE_SIZE) {
                (void)vmem_unmap_page_locked(base + undo);
            }
            vmem_unlock(lock_flags);
            return NULL;
        }
    }

    vmem_unlock(lock_flags);
    return (void *)base;
}

int vmem_unmap_mmio(void *vaddr, uint32_t size) {
    uint32_t lock_flags;
    uint32_t base;
    uint32_t bytes;
    uint32_t offset;
    int result = 0;

    if (!vmem_ready || vaddr == NULL || size == 0) return -1;

    bytes = PAGE_ALIGN(size);
    if (bytes == 0) return -1;

    base = (uint32_t)vaddr;
    lock_flags = vmem_lock();

    for (offset = 0; offset < bytes; offset += PAGE_SIZE) {
        if (vmem_unmap_page_locked(base + offset) != 0) result = -1;
    }

    vmem_unlock(lock_flags);
    return result;
}

void vmem_invlpg(uint32_t vaddr) {
    __asm__ __volatile__("invlpg (%0)" :: "r"(vaddr) : "memory");
}

void vmem_flush_tlb(void) {
    __asm__ __volatile__(
        "mov %%cr3, %%eax\n\t"
        "mov %%eax, %%cr3"
        :: : "eax", "memory");
}

void vmem_get_stats(vmem_stats_t *out) {
    uint32_t lock_flags;

    if (out == NULL) return;

    lock_flags = vmem_lock();

    out->total_frames = total_managed_frames;
    out->free_frames = free_managed_frames;
    out->used_frames = total_managed_frames - free_managed_frames;
    out->page_tables = page_table_count;
    out->mapped_pages = mapped_page_count;
    out->swapped_pages = swapped_page_count;
    out->swap_slots = swap_slot_count;
    out->swap_slots_used = swap_slots_used;
    out->page_faults = page_fault_count;
    out->demand_pages = demand_page_count;
    out->evictions = eviction_count;
    out->arena_start = arena_start;
    out->arena_end = arena_end;
    out->arena_used = arena_bytes_used;

    vmem_unlock(lock_flags);
}
