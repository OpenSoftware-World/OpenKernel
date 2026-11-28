#ifndef VMEM_H
#define VMEM_H

#include <OpenKernel/SystemLib/Std/types.h>

/* Page directory / page table entry flags (Intel SDM volume 3). */
#define PAGE_PRESENT    0x001u
#define PAGE_RW         0x002u
#define PAGE_USER       0x004u
#define PAGE_PWT        0x008u
#define PAGE_PCD        0x010u
#define PAGE_ACCESSED   0x020u
#define PAGE_DIRTY      0x040u
#define PAGE_PSE        0x080u
#define PAGE_GLOBAL     0x100u

/* Bits 12..31 hold the physical frame of a present entry. */
#define PAGE_FRAME_MASK 0xFFFFF000u

/*
 * Software-only bits in a non-present page table entry. The CPU ignores
 * bits 9..11, so they survive a page being swapped out.
 */
#define VMEM_PTE_SWAP   0x200u   /* entry holds a swap slot instead of a frame */
#define VMEM_PTE_OWNED  0x400u   /* frame came from the VM frame allocator    */

/* Flags accepted by vmem_alloc_ex() and vmem_region_add(). */
#define VMEM_REGION_WRITE      0x001u   /* pages are writable                  */
#define VMEM_REGION_USER       0x002u   /* pages are reachable from ring 3     */
#define VMEM_REGION_SWAPPABLE  0x004u   /* pages may be evicted to swap space  */

/*
 * Configuration for vmem_init().
 *
 * phys_start/phys_end describe the usable physical RAM; phys_start must be
 * the first frame the kernel does not use (normally the page-aligned end of
 * the kernel image and its heap). Every frame in that range is managed by
 * the frame allocator and identity-mapped.
 *
 * arena_start/arena_end describe the free virtual range reserved for
 * demand-paged allocations and device mappings. Pass 0 to let vmem_init()
 * place the arena directly above the identity-mapped RAM; that leaves the
 * whole 32-bit address space above physical memory available for virtual
 * allocations while keeping the classic physical == virtual mapping.
 *
 * install_fault_handler registers the VMem page fault handler on vector 14.
 * Call interrupt_init() first, or at least make sure the IDT is loaded
 * before paging can fault.
 */
typedef struct {
    uint32_t phys_start;
    uint32_t phys_end;
    uint32_t arena_start;          /* 0 = automatic */
    uint32_t arena_end;            /* 0 = automatic (0xFFFFF000) */
    bool_t   install_fault_handler;
} vmem_config_t;

/* Read-only snapshot of the VM counters. */
typedef struct {
    uint32_t total_frames;         /* frames managed by the allocator          */
    uint32_t free_frames;          /* frames currently free                    */
    uint32_t used_frames;          /* total_frames - free_frames               */
    uint32_t page_tables;          /* 4 KiB page tables in use                 */
    uint32_t mapped_pages;         /* resident pages in the virtual arena      */
    uint32_t swapped_pages;        /* pages currently living in swap space     */
    uint32_t swap_slots;           /* swap slots configured by the backend     */
    uint32_t swap_slots_used;      /* swap slots currently holding a page      */
    uint32_t page_faults;          /* page faults handled since vmem_init()    */
    uint32_t demand_pages;         /* pages allocated on first touch           */
    uint32_t evictions;            /* pages written to swap space              */
    uint32_t arena_start;
    uint32_t arena_end;
    uint32_t arena_used;           /* bytes reserved from the arena            */
} vmem_stats_t;

/*
 * Swap backend. The VM calls read()/write() for exactly one 4 KiB page and
 * treats a 0 return value as success. Both calls must work with interrupts
 * disabled (the page fault handler runs through an interrupt gate) and
 * must not themselves take a page fault.
 */
typedef int (*vmem_swap_read_t)(void *context, uint32_t slot, void *page);
typedef int (*vmem_swap_write_t)(void *context, uint32_t slot, const void *page);

typedef struct {
    vmem_swap_read_t  read;
    vmem_swap_write_t write;
    void *context;
} vmem_swap_backend_t;

/* Legacy interface: identity-map the first 4 MiB and enable paging. */
void paging_init(void);
void load_page_dir(uint32_t *pd);
void enable_paging(void);

/*
 * Build the full virtual memory subsystem and enable paging. Returns 0 on
 * success and -1 when the configuration is invalid or the CPU lacks PSE.
 * Call once, after the IDT/GDT are usable if install_fault_handler is set.
 */
int vmem_init(const vmem_config_t *config);

/* Attach the page fault handler to vector 14. Idempotent. */
void vmem_install_fault_handler(void);

/*
 * Register the swap backend. Must be called before the first eviction;
 * swapping cannot be reconfigured while slots are in use. slots is clamped
 * to the implementation maximum (1 GiB of 4 KiB pages).
 */
int vmem_swap_set_backend(const vmem_swap_backend_t *backend, uint32_t slots);

/*
 * Reserve size bytes of virtual address space and register a demand-paged
 * region there. The returned pointer is valid immediately, but pages are
 * only backed by RAM on first access. Returns NULL when the arena or the
 * region table is exhausted.
 *
 * vmem_alloc() defaults to writable and swappable. Use vmem_alloc_ex()
 * with VMEM_REGION_WRITE but without VMEM_REGION_SWAPPABLE for memory that
 * must stay resident (stacks, interrupt-context buffers, code).
 */
void *vmem_alloc(uint32_t size);
void *vmem_alloc_ex(uint32_t size, uint32_t region_flags);

/* Unmap every page of the region starting at ptr and release its frames. */
int vmem_free(void *ptr);

/* Register or remove a demand-paged region at a caller-chosen address. */
int vmem_region_add(uint32_t vaddr, uint32_t size, uint32_t region_flags);
int vmem_region_remove(uint32_t vaddr);
int vmem_region_find(uint32_t vaddr, uint32_t *start, uint32_t *end,
                     uint32_t *region_flags);

/* Map/unmap one 4 KiB page. paddr is not owned by the VM. */
int vmem_map_page(uint32_t vaddr, uint32_t paddr, uint32_t flags);
int vmem_unmap_page(uint32_t vaddr);

/* Translate a virtual address; returns 0 when the page is not resident. */
uint32_t vmem_get_physical(uint32_t vaddr);

/*
 * Reserve virtual address space and map physical device memory into it.
 * Returns the virtual base address, or NULL on failure. The returned range
 * is not a demand-paged region; vmem_unmap_mmio() releases the mappings.
 */
void *vmem_map_mmio(uint32_t paddr, uint32_t size, uint32_t flags);
int vmem_unmap_mmio(void *vaddr, uint32_t size);

/* TLB maintenance. */
void vmem_invlpg(uint32_t vaddr);
void vmem_flush_tlb(void);

/* Copy the current counters into out. */
void vmem_get_stats(vmem_stats_t *out);

#define VMEM_MODULE_NAME "Virtual Memory Service"
#define VMEM_MODULE_DESC "Demand paging and swap for OpenKernel"
#define VMEM_MODULE_VER "2.0"
#define VMEM_MODULE_AUTHOR "OpenSoftware-World"
#define VMEM_MODULE_FILE_NAME "vmem.service"
#define VMEM_MODULE_KRNL_VER "2.0"

#endif
