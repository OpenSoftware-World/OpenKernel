# 06 - Drivers

[← System Library](05-system-library.md) | [Documentation Index](README.md) | [Next: File Systems →](07-file-systems.md)

---

`Drivers/` contains the hardware interface layer. This document lists each driver, the hardware it talks to, its public functions, and its real status.

Two rules apply to every driver:

1. **You must call the `*_init()` function first.** Most drivers keep an "enabled" flag and silently do nothing until initialisation.
2. **Almost everything is polled, not interrupt driven.** An IDT, PIC remap and dispatcher exist in `Kernel/Components/InterruptServices/`, and the PIT driver is the only one that registers an IRQ handler (IRQ0) - every other driver still polls.

## Driver status overview

| Driver | Path | Status |
|---|---|---|
| VGA text mode | `Drivers/Vga/` | Works |
| CPU control | `Drivers/Cpu/` | Works (partial feature detection) |
| PIT (8253/8254) | `Drivers/PIT/` | Compiles clean, registers IRQ0, untested on hardware |
| ATA (PIO) | `Drivers/Ata/` | Works |
| Keyboard | `Drivers/Keyboard/` | Partial - polling only, ring buffer wired up |
| Mouse | `Drivers/Mouse/` | Partial - works when polled |
| Sound Blaster 16 | `Drivers/Sound/sb16/` | Basic but functional |
| AHCI | `Drivers/Ahci/` | Compiles clean, reworked, untested on hardware |
| Serial (COM1) | `Drivers/Serial/` | Compiles clean, untested |
| PCI | `Drivers/PCIBus/` | Compiles clean, untested |
| Ethernet layer | `Drivers/Network/Ethernet/` | Skeleton, compiles clean |
| Intel e1000 | `Drivers/Network/e1000/` | Compiles clean, BAR discovery added, untested |
| Realtek RTL8139 | `Drivers/Network/RTL8139/` | Compiles clean, BAR discovery added, untested |

## 1. VGA text mode - `Drivers/Vga/`

The most complete driver in the project. 80x25 text mode at physical address `0xB8000`.

### Functions

```c
void vga_init(void);
void vga_close(void);
void vga_clear_screen(const char *mode);
void vga_set_bg_color(uint8_t color);
void vga_set_text_color(uint8_t color);
void vga_set_color_scheme(uint8_t bg_color, uint8_t text_color);
void vga_print_scr(const char *str);
void vga_print_scr_nw(const char *str);      /* print without newline */
void vga_newline(void);
void vga_bckspc(void);
void vga_set_cursor(uint8_t x, uint8_t y);
void ptchar(char c);
void vga_print_hex(uint32_t n);
void vga_print_dec(int num);
void vga_print_bin(unsigned int num);
void vga_cursor_mode(const char *mode, uint8_t start, uint8_t end);
void screen_init(void);
```

### Behaviour worth knowing

- **`vga_init()` is mandatory.** Every print function checks an internal `vga_enabled_driver` flag and returns immediately if it is false. Forgetting this is the most common reason for a "nothing prints" bug.
- `vga_clear_screen("F")` clears all 25 rows. `vga_clear_screen("C")` clears rows 1 to 24, keeping row 0 as a header.
- `vga_cursor_mode("E", start, end)` shows the caret with the given scan lines; `vga_cursor_mode("D", 0, 0)` hides it.
- A shadow copy `screen_buffer[80*25]` is maintained and refreshed by `screen_init()`. The mouse driver uses it to erase itself.
- `vga_print_scr()` appends a newline; `vga_print_scr_nw()` does not.
- Cursor movement is hardware-driven through CRT controller ports `0x3D4` and `0x3D5`.

### Limitations

- **No `printf`.** You get `print_hex`, `print_dec`, `print_bin`, and raw strings. Formatted output must be assembled by hand into a buffer.
- No true scrolling: when the cursor passes the last row the screen is cleared rather than scrolled, so text at the top is lost.
- Colour is set per cell through `vga_set_bg_color` / `vga_set_text_color`, which update a current-attribute global rather than a whole region.

## 2. CPU control - `Drivers/Cpu/`

```c
void cli(void);              /* disable interrupts */
void sti(void);              /* enable interrupts */
void hlt(void);               /* halt until the next interrupt */
void nop(void);
void cpu_status(const char *mode);   /* "S" = hlt, "A" = sti */
void get_sys_cpuinfo(const char *mode); /* "B" = brand string, "V" = vendor */
void cpuinfo_data(uint32_t *eax, uint32_t *ebx, uint32_t *ecx,
                  uint32_t *edx, uint32_t function_id);
```

`get_sys_cpuinfo("B")` reads CPUID leaves `0x80000002` to `0x80000004` for the brand string; `"V"` reads leaf 0 for the vendor string.

Limitations:

- `sys_cpuinfo_t` declares 17 feature flags (`sse`, `sse2`, `avx`, `mmx`, `avx2`, `vmx`, `sgx`, and so on) that are **never populated**. Feature detection does not exist yet.
- `cpuinfo_data()` is `void`: CPUID writes through the four output pointers, so there is nothing to return. It used to be declared `uint32_t` with no `return`, which was silenced with a `#pragma GCC diagnostic` instead of being fixed.

## 3. Keyboard - `Drivers/Keyboard/`

PS/2 keyboard on ports `0x60` (data) and `0x64` (status).

```c
void    kbd_init(void);
char    get_char(void);
uint8_t kb_check(void);
void    kbd_hndlr(uint8_t scancode);
void    kbd_close(void);
```

`kbd_hndlr()` ignores break codes, toggles Caps Lock on scancode `0x3A`, maps make codes through a 128-entry US-layout table and pushes the result into a ring buffer (`kbf[KEY_BF_SIZE]` with `buf_head` / `buf_tail`). `get_char()` drains that buffer first and only polls port `0x64` when it is empty; `kb_check()` reports whether anything is pending.

Limitations:

- **Polling only.** No IRQ1, so `get_char()` burns CPU while waiting for the controller.
- **The ring buffer drops characters when full** rather than blocking or evicting.
- Only the US layout. No Shift, Alt, or Ctrl handling, so symbols requiring Shift never appear correctly.
- `kbd_init()` is never called anywhere in the repository, so `kbd_enabled_driver` stays false and every entry point returns 0.

## 4. Mouse - `Drivers/Mouse/`

PS/2 auxiliary mouse on the same ports.

```c
void mouse_init(void);
void mouse_poll(void);
void mouse_draw(void);
void mouse_restore(void);
void mouse_update(void);     /* poll, then redraw if the position changed */
void mouse_close(void);
```

`mouse_init()` enables the auxiliary device (`outb(0x64, 0xA8)`), sets bit 1 of the Compaq status byte, sends the default configuration command (`0xF6`) and the enable command (`0xF4`) through the `0xD4` prefix, then drains the buffer.

`mouse_poll()` assembles the standard 3-byte packet, validates the `bit3` sync bit, updates `mouse_x` / `mouse_y` (Y axis inverted), and clamps the position to the 80x25 screen. `mouse_draw()` writes a block character `0xDB` with attribute `0x1F`; `mouse_restore()` puts back whatever `screen_init()` captured.

Limitations:

- **No IRQ12.** Everything is polled.
- Button presses are received as part of the packet but no click handling exists.
- The cursor is drawn by overwriting VGA cells directly, so it can cover text.
- `mouse.c` used to declare `extern uint16_t *vgabuffer = (uint16_t *)0xB8000;`, which is a *definition* with an initializer and collided with `vga.c` at link time. It is now a plain `extern` declaration; the definition stays in `vga.c`.
- `mouse_clear()`, `mouse_put_char()`, and `prev_cell` are declared in the header but never defined.

## 5. Serial - `Drivers/Serial/`

COM1 at `0x3F8`, 38400 baud, 8N1.

```c
void serial_init(void);
int  serial_is_transmit_empty(void);
void serial_print_char(char c);
void serial_print_string(const char *string);   /* converts \n to \r\n */
char serial_read_char(void);
int  serial_received(void);
int  serial_try_read_char(char *c);
int  serial_has_error(void);
void serial_clear_errors(void);
void serial_log/success/error/warn/info/unknown(const char *msg);
void serial_set_status(bool_t val);
void serial_loopback_test(void);
```

`serial_init()` performs the classic 16550 sequence: disable interrupts, set the DLAB bit, program divisor 3 (38400), configure 8N1, enable and clear the FIFO (`0xC7`), set MCR to `0x0B`, run a loopback test writing `0xAE` and reading it back, then leave the modem in normal mode.

Status:

- **Compiles clean.** `serial_read_char()` had been defined twice and line 121 referenced `COMR_MCR`; one definition was removed and the typo corrected. The header's `serial_get_error()` and the source now agree.
- Never executed. `make run` already passes `-serial stdio`, so output would appear on your terminal as soon as there is a boot path.

## 6. Sound Blaster 16 - `Drivers/Sound/sb16/`

Base I/O address `0x220`.

| Register | Address |
|---|---|
| DSP reset | `0x226` |
| DSP read | `0x22A` |
| DSP write | `0x22C` |
| DSP status | `0x22E` |

```c
void    sb16_init(void);
int     sb16_reset(void);
void    sb16_set_sample_rate(uint32_t hz);
void    sb16_play_buffer(uint8_t *data, uint32_t size);
void    sb16_stop(void);
void    sb16_write_dsp(uint8_t value);
uint8_t sb16_read_dsp(void);
int     sb16_wait_ready(void);
void    sb16_gen_flat_sound(uint8_t level);
void    sb16_test(void);
```

`sb16_reset()` pulses the reset bit and waits for the `0xAA` confirmation byte. `sb16_play_buffer()` uses DSP command `0x14`, an 8-bit single-cycle DAC direct output, then pushes the sample bytes.

Limitations:

- **No DMA and no IRQ.** Only the DMA-less direct-output path is implemented, so buffer sizes are limited and CPU overhead is high.
- `sb16.initialized` is set during init but never checked before playback.
- No volume control, no stereo, no 16-bit samples.
- The `Makefile` lists the path as `Drivers/Sound/sb16.c`, but the real file is `Drivers/Sound/sb16/sb16.c`.

## 7. ATA - `Drivers/Ata/`

PIO mode on the primary channel, ports `0x1F0` to `0x1F7` with the control register at `0x3F6`.

```c
void ata_sys_disk_init(void);
void ata_sys_disk_read(uint32_t lba, uint8_t *buffer);
void ata_sys_disk_write(uint32_t lba, const uint8_t *buffer);
void ata_soft_reset(void);
void ata_reinit(uint8_t drive, uint16_t *identify_buf);
void ata_identify(uint8_t drive, uint16_t *buf);
```

Reads and writes are single-sector LBA28 operations: the drive-select register receives `0xE0 | (lba >> 24)` (bit 5 selects LBA mode and bit 4 the drive; the old value `0x0E` was not a valid drive-select word), the sector count is set to 1, the three LBA bytes are written, then command `0x20` (read) or `0x30` (write) is issued and 256 words are transferred over `0x1F0`.

Status:

- **Functioning (compile-verified only).** `ata_wait_busy()` and `ata_wait_drq()` poll with a bounded iteration limit and abort on the `ERR` / `DF` status bits, so a dead or absent drive returns instead of hanging the kernel. A short delay after the drive-select write covers the 400 ns settle time the spec requires.
- A failed read leaves a zeroed sector in the caller's buffer rather than stack garbage; a failed write leaves the sector on disk untouched.
- `ata_sys_disk_init()` has an **empty body** - it is a stub.
- Single sector only, no multi-sector transfers, no DMA, no 48-bit LBA (so no disks above 128 GiB).
- All three FAT drivers call `ata_sys_disk_read()`, which is the correct name.

## 8. AHCI - `Drivers/Ahci/`

The most sophisticated driver in the repository (500 lines). It targets SATA controllers exposing an AHCI ABAR through PCI BAR5.

```c
int  ahci_init(void);
int  ahci_scan_ports(void);
int  ahci_read_sector(int port, uint32_t lba, uint32_t count, uint8_t *buf);
int  ahci_write_sector(int port, uint32_t lba, uint32_t count, const uint8_t *buf);
int  ahci_get_port_count(void);
ahci_port_state_t* ahci_get_port(int index);
void ahci_print_info(void);
```

`ahci.h` models the full register set: `CAP`, `GHC`, `CAP2`, `IS`, `PI`, `VS`, and the per-port `CLB`, `FB`, `CMD`, `TFD`, `SIG`, `SSTS`, `SCTL`, `SERR`, `SACT`, `CI`, `SNTF`, `FBS`, plus FIS types and a limit of 32 ports and 32 command slots. `ahci_read_sector()` / `ahci_write_sector()` return `0` on success and `-1` on failure (bad port, timeout, device error).

`ahci_init()` scans PCI for class `01:06:01` (SATA), reads BAR5 (validating that it is a memory BAR), masks off the flag bits, enables bus mastering and memory decoding in PCI command, and sets `GHC.AE`. `ahci_scan_ports()` iterates the `PI` register, matches the SATA signature `0x00000101`, stops and restarts each port (bounded waits), and only then identifies the drive with a real IDENTIFY (`0xEC`) command issued through the command-slot machinery. IDENTIFY words 60 and 61 select LBA48 versus LBA28; if the device reports no LBA support the port stays unused rather than reporting a bogus sector count.

Behaviour worth knowing:

- Register layouts and structure sizes are locked with `STATIC_ASSERT` checks on `sizeof` and `__builtin_offsetof`, so the old silent layout drift (16-byte command headers, 30-byte D2H FIS, wrong `reserved` size placing ports at `0x60` instead of `0x100`) cannot come back.
- Command lists, FIS receive areas, and command tables come from `kmalloc_aligned()` (1 KiB / 256 / 128 byte alignment) built on the project's `kmalloc()` plus the next power-of-two block header trick. They are still never freed.
- Command header `PRDTL` is set from the actual PRD count (`AHCI_CMD_PRDTL(n)`), and the AHCI spec-required `CFL = 5` is set for every command FIS.
- Transfers are split on PRD boundaries (8 KiB pieces) and on the 4 MiB LBA48 range boundary; reads stop if the drive raises `TFES` (`SERR.TF` + `SERR.DIAG`), and every wait loop has a bounded timeout that actually fires.
- The driver's own port/CMD/CI/TD registers are accessed through `volatile` pointers; `prt_dec()` divides a `uint64_t` by a constant, so no `__udivdi3` libgcc dependency is pulled in.

Known limitations:

- **Nothing in the repository calls `ahci_init()`.** It links correctly now that `pci.c` compiles, but nothing invokes it, so it has never run.
- Untested on real hardware or QEMU: the project has no boot code, so verification so far is compile-time only (structure asserts, clean `-Wall -Wextra` build).
- No interrupts, no hot-plug, no ATAPI devices, no NCQ; everything is polled with busy-wait timeouts.

## 9. PCI - `Drivers/PCIBus/`

```c
void     pci_init(void);
uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
void     pci_write(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t val);
void     pci_scan(void);
int      pci_get_dev_count(void);
pci_dev_t* pci_get_device(int index);
```

Configuration accesses build the standard address `0x80000000 | bus<<16 | dev<<11 | func<<8 | (offset & 0xFC)` and use ports `0xCF8` / `0xCFC`. `pci_scan()` walks all 256 buses, 32 devices, and reads vendor, device, class, and BAR0 to BAR5.

An automatic probe helper `pci_dev_init()` (declared in `pci.h`) detects class `0x02` (network), enables I/O space, memory space and bus mastering in the command register, then matches vendor `0x8086` to `e1000_init(pdev)` and `0x10EC` to `rtl8139_init(pdev)`. Only the 16-bit command half of the dword is written back, so the write-1-to-clear status register above it is not disturbed.

Status:

- **Compiles clean.** The `pci_device_t` / `pci_device_count` name mismatches are gone; the file uses `pci_dev_t` and `pci_dev_count`.
- `pci_init()` resets `pci_dev_count` to 0 before scanning, so repeated scans do not double-count.
- **No run-time evidence.** No BAR value has ever been observed, so the decode in `e1000_init()` / `rtl8139_init()` is untested against real hardware.
- Only function 0 of each device is scanned; multi-function devices are not enumerated.

## 10. Network drivers - `Drivers/Network/`

There is still **no working network stack**. All three files compile and link, but none has ever been executed.

### Ethernet layer - `Ethernet/ethernet.c`

```c
void ethernet_init(void);
void ethernet_register_device(net_device_t *dev);
void ethernet_send(uint8_t *dest_mac, uint16_t ethertype, void *payload, uint16_t len);
```

`ethernet_register_device()` maintains a singly linked list of devices. `ethernet_send()` builds a 14-byte Ethernet header with `ETH_TYPE_ARP 0x0806` and `ETH_TYPE_IP4 0x0800` and calls the device's `send` function.

Limitations:

- **Payload is clamped.** `len` is capped at `ETH_MAX_FRAME_SIZE - sizeof(eth_header_t)` (1504 bytes) so a bad length cannot run off the 1518-byte stack buffer or wrap the 16-bit `frame_len`. The caller's excess data is silently dropped, because the function returns `void` and cannot report an error.
- `NULL` `dest_mac` / `payload` are rejected.
- The source MAC is hard-coded to six `0xAA` bytes.
- Transmission always goes through the *head* of the device list, not the device matching the destination.
- `ethernet_init()` is declared but never defined.
- There is no receive path, no ARP resolution, no IP, no checksums.

### Intel e1000 - `e1000/e1000.c`

```c
bool_t e1000_init(pci_dev_t *pdev);
void   e1000_send_packet(void *data, uint16_t len);
void   e1000_receive_poll(void);
```

TX and RX descriptor rings are allocated and the `TDBAL`, `TDLEN`, `TDH`, `TDT`, `RDBAL`, `RDLEN`, `RDH`, `RDT`, `RCTL`, and `TCTL` registers are programmed. `e1000_init()` now:

- rejects anything that is not vendor `0x8086` with device ID `0x100E` (82540EM) or `0x100F` (82545EM);
- reads BAR0, rejects an I/O BAR, a `0` BAR and an unimplemented `0xFFFFFFFF` BAR, masks off the four flag bits, and refuses a 64-bit BAR whose high half sits above 4 GiB (this target has only 32-bit pointers);
- checks every `kmalloc()` result, including each 2048-byte receive buffer - a NULL there would make the NIC DMA to physical address 0;
- resets its own state and returns `false` on any of those failures, so `send` / `receive` become no-ops instead of dereferencing NULL.

Still missing:

- No EEPROM or MAC address read, no link setup, no interrupt enable, no receive address filters.
- `e1000_send_packet()` writes `TDT = tx_index`, but the tail register must point *past* the descriptor that was just filled, so the NIC never transmits the packet just queued. Untested, therefore unfixed.
- `e1000_receive_poll()` consumes a descriptor and then discards the packet.
- No page-table mapping is done for the BAR address, so this only works while paging is off or identity-mapped.

### Realtek RTL8139 - `RTL8139/rtl8139.c`

```c
bool_t rtl8139_init(pci_dev_t *pdev);
void   rtl8139_send_packet(void *data, uint16_t len);
void   rtl8139_receive_poll(void);
```

The old body was `nic = (rtl8139_t*)0xDEADBEEF;` followed by a dereference. `rtl8139_init()` now:

- rejects anything that is not vendor `0x10EC` / device `0x8139`;
- reads BAR0 and requires bit 0 set (an I/O region), rejects `0`, `0xFFFFFFFF` and a zero base, then masks off the two flag bits to get the port base;
- leaves `nic` NULL on any failure, and `rtl8139_send_packet()` / `rtl8139_receive_poll()` return immediately when it is NULL.

Still missing:

- `rx_buffer` is a plain static array with no 256-byte alignment attribute, which the NIC wants for its 8 KiB receive ring.
- The receive poll validates the 4-byte header bounds but still does not hand the packet to anything.
- No EEPROM MAC read, no interrupt, no transmit-status handling.

## 11. PIT - `Drivers/PIT/`

Intel 8253/8254 Programmable Interval Timer. The first driver in the project that uses interrupts instead of polling: it programs channel 0, hooks IRQ0 (vector 32) through `interrupt_register_handler()` and unmasks the line with `pic_clear_mask(0)`.

```c
void     pit_init(uint32_t hz);       /* 1000 = 1 kHz, one tick per ms */
void     pit_stop(void);              /* mask IRQ0, the counter freezes */
uint32_t pit_get_ticks(void);         /* ticks since the last pit_init() */
uint32_t pit_get_frequency(void);     /* actual rate: 1193182 / divisor */
uint32_t pit_get_uptime_ms(void);     /* wraps after about 49.7 days */
void     pit_sleep_ms(uint32_t ms);   /* busy-wait driven by the ticks */
```

### Behaviour worth knowing

- **`pit_init()` requires `interrupt_init()` first**, otherwise the handler is registered into an IDT that has not been loaded.
- **Interrupts must be enabled (`sti`) or the counter never advances.** The driver deliberately does not touch IF - that stays the kernel's decision.
- The requested rate is clamped to `19 .. 1193182` Hz. At the default 1000 Hz the divisor is `1193`, so the real rate is `1193182 / 1193 = 1000.15` Hz. `pit_get_frequency()` reports the nearest whole hertz (1000), but the millisecond accumulator works from the exact divisor, so the uptime does not drift by the missing 0.15 Hz.
- Channel 0 is loaded with command word `0x34` (channel 0, low/high byte access, **mode 2 rate generator**, binary counting). `interrupt_dispatch()` sends the EOI, so the handler never writes to the PIC itself.
- `pit_init()` can be called again with a different rate: the ticks counted at the old rate are folded into the millisecond base first, so `pit_get_uptime_ms()` stays correct.
- `pit_sleep_ms()` waits with `hlt` while IF is set and spins when it is clear, because `hlt` with IF clear would hang until an NMI. It must not be called from an IRQ handler - the ticks it waits for could not be delivered.

### Limitations

- Ticks stop entirely while IF is clear or after `pit_stop()`, so there is no wall-clock guarantee during `cli` sections.
- Channel 0 only; channels 1 and 2 (PC speaker, DRAM refresh) are left untouched.
- No calibration against the RTC, so the crystal error is accepted as is.
- `pit_get_uptime_ms()` returns `uint32_t` and wraps after roughly 49.7 days.

---

[← System Library](05-system-library.md) | [Documentation Index](README.md) | [Next: File Systems →](07-file-systems.md)
