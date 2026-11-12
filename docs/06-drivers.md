# 06 - Drivers

[← System Library](05-system-library.md) | [Documentation Index](README.md) | [Next: File Systems →](07-file-systems.md)

---

`Drivers/` contains the hardware interface layer. This document lists each driver, the hardware it talks to, its public functions, and its real status.

Two rules apply to every driver:

1. **You must call the `*_init()` function first.** Most drivers keep an "enabled" flag and silently do nothing until initialisation.
2. **Almost everything is polled, not interrupt driven.** There is no IDT, no PIC programming, and no IRQ handlers anywhere in the project.

## Driver status overview

| Driver | Path | Status |
|---|---|---|
| VGA text mode | `Drivers/Vga/` | Works |
| CPU control | `Drivers/Cpu/` | Works (partial feature detection) |
| ATA (PIO) | `Drivers/Ata/` | Works |
| Keyboard | `Drivers/Keyboard/` | Partial - polling only, compile errors |
| Mouse | `Drivers/Mouse/` | Partial - works, but breaks the link |
| Sound Blaster 16 | `Drivers/Sound/sb16/` | Basic but functional |
| AHCI | `Drivers/Ahci/` | Substantial, one cast bug and an ordering bug |
| Serial (COM1) | `Drivers/Serial/` | Logic sound, does not compile |
| PCI | `Drivers/PCIBus/` | Does not compile |
| Ethernet layer | `Drivers/Network/Ethernet/` | Skeleton, does not compile |
| Intel e1000 | `Drivers/Network/e1000/` | Placeholder, does not compile |
| Realtek RTL8139 | `Drivers/Network/RTL8139/` | Placeholder, does not compile |

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
- `vga_clear_screen("F")` clears all 25 rows. `vga_clear_screen("C")` clears rows 1 to 23, keeping row 0 as a header.
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
uint32_t cpuinfo_data(uint32_t *eax, uint32_t *ebx, uint32_t *ecx,
                      uint32_t *edx, uint32_t function_id);
```

`get_sys_cpuinfo("B")` reads CPUID leaves `0x80000002` to `0x80000004` for the brand string; `"V"` reads leaf 0 for the vendor string.

Limitations:

- `sys_cpuinfo_t` declares 17 feature flags (`sse`, `sse2`, `avx`, `mmx`, `avx2`, `vmx`, `sgx`, and so on) that are **never populated**. Feature detection does not exist yet.
- `cpuinfo_data()` is declared as returning `uint32_t` but has no return statement; the warning is suppressed with a pragma.

## 3. Keyboard - `Drivers/Keyboard/`

PS/2 keyboard on ports `0x60` (data) and `0x64` (status).

```c
void    kbd_init(void);
char    get_char(void);
uint8_t kb_check(void);
void    kbd_hndlr(uint8_t scancode);
void    kbd_close(void);
```

`get_char()` busy-waits until port `0x64` bit 0 is set, reads the scancode from `0x60`, ignores break codes (`scancode & 0x80`), and maps through a 128-entry US-layout table. Caps Lock (scancode `0x3A`) is handled by `kbd_hndlr()` and upper-cases the result.

Limitations:

- **Polling only.** No IRQ1, so `get_char()` burns CPU while waiting.
- **No ring buffer.** The file declares `kbf[KEY_BF_SIZE]`, `buf_head`, and `buf_tail`, but they are never written or read. Each `get_char()` call consumes one scancode directly; if nothing reads, keystrokes are dropped.
- Only the US layout. No Shift, Alt, or Ctrl handling, so symbols requiring Shift never appear correctly.
- Three functions (`to_upper`, `kb_check`, `get_char`) contain `return;` statements in non-void functions, which is an error on GCC 14.

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
- `mouse.c` declares `extern uint16_t *vgabuffer = (uint16_t *)0xB8000;`. That is a *definition* with an initializer, and `vga.c` defines the same symbol, producing `multiple definition of 'vgabuffer'` at link time.
- `mouse_clear()`, `mouse_put_char()`, and `prev_cell` are declared in the header but never defined.
- `sleep` is used without an include, which is an error on GCC 14.

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

- **Does not compile.** `serial_read_char()` is defined twice in `serial.c`, and line 121 references `COMR_MCR`, which should be `COM1_MCR`.
- The header declares `serial_get_error()` while the source defines `serial_get_errors()`.
- Once fixed, this driver is valuable: `make run` already passes `-serial stdio`, so output would appear on your terminal.

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

Reads and writes are single-sector LBA28 operations: the drive-select register receives `0x0E | (lba >> 24)`, the sector count is set to 1, the three LBA bytes are written, then command `0x20` (read) or `0x30` (write) is issued and 256 words are transferred over `0x1F0`.

Status:

- **Functioning.** `ata_wait_busy()` polls the BSY bit, `ata_wait_drq()` polls DRQ.
- `ata_sys_disk_init()` has an **empty body** - it is a stub.
- Single sector only, no multi-sector transfers, no DMA, no 48-bit LBA (so no disks above 128 GiB).
- FAT16 and FAT32 call a function named `ata_read_sector()`, which does not exist. The real name is `ata_sys_disk_read()`. This is why those two file systems fail to link.

## 8. AHCI - `Drivers/Ahci/`

The most sophisticated driver in the repository (564 lines). It targets SATA controllers exposing an AHCI ABAR through PCI BAR5.

```c
int  ahci_init(void);
int  ahci_scan_ports(void);
void ahci_read_sector(int port, uint32_t lba, uint32_t count, uint8_t *buf);
void ahci_write_sector(int port, uint32_t lba, uint32_t count, const uint8_t *buf);
int  ahci_get_port_count(void);
ahci_port_state_t* ahci_get_port(int index);
void ahci_print_info(void);
```

`ahci.h` models the full register set: `CAP`, `GHC`, `IS`, `PI`, `VS`, and the per-port `CLB`, `FB`, `CMD`, `TFD`, `SIG`, `SSTS`, `SCTL`, `SERR`, `SACT`, `CI`, `SNTF`, `FBS`, plus FIS types and a limit of 32 ports and 32 command slots.

`ahci_init()` scans PCI for class `01:06:01` (SATA), reads BAR5, masks off the flag bits, and sets `GHC.AE`. `ahci_scan_ports()` iterates the `PI` register, matches the SATA signature `0x00000101`, configures each port, and identifies the drive through words 60 and 61 of the IDENTIFY data.

Known problems:

- Line 148 assigns `&cmd_tbl->prdt[0]` to an `ahci_prdt_entry_t*` although `prdt` is declared as `uint8_t[128 * 8]`. A cast is missing; this is a compile error on GCC 14 and a warning on earlier versions.
- **Ordering bug:** `ahci_scan_ports()` issues the IDENTIFY read *before* calling `configure_port()`, so the port has no command list or receive FIS area yet and identification always fails. `total_sectors` ends up as 0.
- Command lists and FIS buffers are allocated with `kmalloc()`, which aligns only to 4 bytes and never frees. AHCI requires 1 KiB-aligned command lists.
- Nothing in the repository calls `ahci_init()`.

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

An automatic probe helper `pci_dev_init()` detects class `0x02` (network), then matches vendor `0x8086` to `e1000_init()` and `0x10EC` to `rtl8139_init()`.

Status:

- **Does not compile.** `pci.c` uses the type name `pci_device_t` while `pci.h` defines `pci_dev_t`, and it references `pci_device_count` where the actual variable is `pci_dev_count`.
- `pci_scan()` never resets the counter it increments, so repeated scans double-count.
- `pci_dev_init()` is not declared in `pci.h` and relies on implicit declarations.

## 10. Network drivers - `Drivers/Network/`

There is **no working network stack**. All three files fail to compile.

### Ethernet layer - `Ethernet/ethernet.c`

```c
void ethernet_init(void);
void ethernet_register_device(net_device_t *dev);
void ethernet_send(uint8_t *dest_mac, uint16_t ethertype, void *payload, uint16_t len);
```

`ethernet_register_device()` maintains a singly linked list of devices. `ethernet_send()` builds a 14-byte Ethernet header with `ETH_TYPE_ARP 0x0806` and `ETH_TYPE_IP4 0x0800` and calls the device's `send` function.

Problems:

- Includes `<OpenKernel/SystemLib/Mem/mem.h>`; the directory is actually `SystemLib/Memory/`, so the include fails on a case-sensitive filesystem.
- The source MAC is hard-coded to six `0xAA` bytes.
- Transmission always goes through the *head* of the device list, not the device matching the destination.
- `ethernet_init()` is declared but never defined.
- There is no receive path, no ARP resolution, no IP, no checksums.

### Intel e1000 - `e1000/e1000.c`

```c
bool_t e1000_init(void);
void   e1000_send_packet(void *data, uint16_t len);
```

TX and RX descriptor rings are allocated and the `TDBAL`, `TDLEN`, `TDH`, `TDT`, `RDBAL`, `RDLEN`, `RDH`, `RDT`, `RCTL`, and `TCTL` registers are programmed. That is where the realism stops:

- `dev->mmio_base` is hard-coded to `0xF0000000`. There is no PCI BAR0 discovery and no page-table mapping for that address, so with paging enabled the accesses fault and with paging disabled they hit an unrelated physical range.
- No EEPROM or MAC address read, no link setup, no interrupt enable, no receive address filters.
- `e1000_receive_poll()` computes a packet pointer and length and then discards both.
- Same broken `SystemLib/Mem/mem.h` include, plus a `bool` vs `bool_t` return type mismatch between the `.c` and `.h`.

### Realtek RTL8139 - `RTL8139/rtl8139.c`

```c
bool_t rtl8139_init(void);
void   rtl8139_send_packet(void *data, uint16_t len);
```

This is the clearest placeholder in the repository:

```c
nic = (rtl8139_t*)0xDEADBEEF;
outb(nic->io_base + RTL_REG_COMMAND, RTL_CMD_RESET);
```

It dereferences a deliberately invalid pointer instead of a PCI BAR, so it would fault immediately. It also uses `RTL_RX_BUFFER_SIZE` and `RTL_REG_COMMAND`, which do not exist (the header defines `RTL_RX_BUF_SIZE` and `RTL_REG_CMD`). The receive functions compute a length and discard it.

---

[← System Library](05-system-library.md) | [Documentation Index](README.md) | [Next: File Systems →](07-file-systems.md)
