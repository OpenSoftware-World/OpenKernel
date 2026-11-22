# 03 - Mimari

[← Başlarken](02-getting-started.md) | [Belge Dizini](README.md) | [Sonraki: Çekirdek Hizmetleri →](04-kernel-services.md)

---

Bu belge, deponun nasıl düzenlendiğini, derlemenin nasıl çalıştığını ve çekirdek bağlandıktan sonra belleğin nasıl yerleştiğini açıklar.

## 1. Dizin yapısı

```
OpenKernel/
├── Kernel/                          Kernel core
│   ├── ErrorHandler/panic.c         kernel_panic()
│   ├── Components/
│   │   ├── InterruptServices/       IDT, PIC remap, exception/IRQ dispatch
│   │   ├── LogService/log.c         Coloured log output
│   │   ├── SecurityChecker/         Runtime self-checks (PSC)
│   │   ├── SystemManagement/        Shutdown / reboot / exit
│   │   └── VirtualMem/              Paging setup
│   └── openkernel_information.h     Name, version, architecture
│
├── SystemLib/                       Reusable kernel-side library
│   ├── Memory/mem.c                 memset/memcpy/memcmp, kmalloc/kfree
│   ├── Std/std.c                    String functions (no printf)
│   ├── SystemIO/io.h                Port I/O inline functions
│   ├── SystemIO/qemu_io.h           QEMU debug port 0xE9 (unused)
│   ├── TimeMng/time.c               RTC reading, sleep, uptime
│   ├── TaskMng/task.c               Task list structures
│   ├── TypeConversion/type_conv.c   itoa, atoi, hex, BCD
│   ├── KernelFunc/kernel_func.c     Named kernel function registry
│   ├── SysCalls/syscall.c           Internal dispatch table
│   └── openkernel_systemlib_desc.h  Module metadata (unused)
│
├── Drivers/                         Hardware drivers
│   ├── Vga/                         Text mode 80x25
│   ├── Cpu/                         CPUID, cli/sti/hlt
│   ├── PIT/                         8253/8254 interval timer, 1 kHz IRQ0
│   ├── Keyboard/                    PS/2 keyboard (IRQ1 driven)
│   ├── Mouse/                       PS/2 mouse (IRQ12 driven)
│   ├── Serial/                      COM1 16550
│   ├── Sound/sb16/                  Sound Blaster 16
│   ├── Ata/                         PIO mode disk access
│   ├── Ahci/                        SATA controller
│   ├── PCIBus/                      PCI configuration space
│   ├── Network/Ethernet/            Ethernet frame layer
│   ├── Network/e1000/               Intel e1000
│   ├── Network/RTL8139/             Realtek RTL8139
│   └── openkernel_drivers_desc.h    Module metadata (unused)
│
├── KernelAPI/                       Stable API for downstream OSes
│   ├── OpenKernelUniversalAPI/      Always-available functions
│   ├── nonuniversal_api_defs.*      Optional/subsystem functions
│   ├── OpenKernelEnvironment/       Types and syscall declarations
│   └── Information/                 API version metadata
│
├── OFS/                             Open File System (read/write, flat)
├── FAT12/ FAT16/ FAT32/             FAT readers (read-only)
├── VFS/                             Single active-file-system switch
│
├── Makefile                         Build rules
├── link.ld                          Linker script
├── install_pkg.sh                   Host setup (Linux)
├── notes.txt                        Bootstrapping instructions
├── version.txt                      Release version (5.0)
└── .github/workflows/               Release packaging and CodeQL
```

Kaba boyut dağılımı (C kaynağı ve başlık dosyaları, toplam yaklaşık 5.350 satır):

| Alan | Satır | Pay |
|---|---:|---:|
| `Drivers/` | 2.158 | %40 |
| `SystemLib/` | 1.463 | %27 |
| `Kernel/` | 694 | %13 |
| `KernelAPI/` | 341 | %6 |
| Dosya sistemleri (`OFS`, `FAT*`, `VFS`) | 596 | %11 |

En büyük tek dosyalar, `Kernel/KernelServices/SecurityChecker/security.c` (602 satır) ve `Drivers/Ahci/ahci.c` (500 satır) dosyalarıdır.

## 2. Include kuralı

Neredeyse her dosya, paket dizinine köklenmiş mutlak yollu başlık dosyalarını içerir:

```c
#include <OpenKernel/SystemLib/Std/types.h>
```

Bu, derleyicinin `OpenKernel/` klasörünü *içeren* dizinden `-I .` ile çağrılması sayesinde çalışır. Depo kökünün içinden derlerseniz bunun yerine `-I ..` kullanmanız gerekir.

Az sayıda dosya (`vga.c`, `ahci.c`, `mouse.c`, `pci.c`, `ata.c`, `fat16.c`, `fat32.c`), `"../../SystemLib/SystemIO/io.h"` gibi göreli include'lar da karıştırır. Her iki kural bir arada var olur; dosya taşırsanız dikkatli olun.

## 3. Makefile

### Araç değişkenleri

```make
AS = nasm
CC = i686-elf-gcc
LD = i686-elf-ld
```

### Çıktı konumları

```make
BUILD     = build
ISO       = iso
KERNEL    = $(BUILD)/os.bin
ISOFILE   = your_os.iso
```

Nesne dosyaları, kaynak ağacını `build/` altında aynalar; örneğin `OpenKernel/Drivers/Vga/vga.c` dosyası `build/OpenKernel/Drivers/Vga/vga.o` olur.

### Derleme bayrakları

```make
CFLAGS  = -m32 -ffreestanding -O2 -Wall -Wextra -I .
LDFLAGS = -m elf_i386 -T link.ld
```

- `-m32` - 32 bit kod.
- `-ffreestanding` - ev sahibi libc yok, örtük başlangıç dosyaları yok.
- `-O2` - hız için optimize eder.
- `-Wall -Wextra` - eksiksiz uyarı seti.
- `-I .` - include kökü.

### Kaynak listeleri

```make
ASM_SRC = OpenKernel/Kernel/Components/InterruptServices/isr.asm
C_SRC   = OpenKernel/Drivers/Vga/vga.c \
          OpenKernel/Drivers/Cpu/cpu.c \
          ...                 # 32 entries
```

`ASM_SRC`, CPU istisna ve IRQ giriş stub'larını oluşturan `isr.asm` dosyasını içerir. İçinde `_start` bulunmadığı için bağlama işlemi hâlâ `cannot find entry symbol _start` uyarısı verir.

`C_SRC`, 34 C dosyasından 32'sini listeler. Eksik olan iki kayıt:

- `KernelAPI/nonuniversal_api_defs.c`
- `KernelAPI/OpenKernelUniversalAPI/universal_api_defs.c`

Tüm yollar, `Makefile`'ın bulunduğu dizine görelidir ve `CFLAGS`, `#include <OpenKernel/...>` ifadesinin çözümlenmesi için `-I .` geçirir. Bunun sürüm paketi düzeni için ne anlama geldiğini görmek için [02 - Başlarken](02-getting-started.md) 3. bölümüne bkz.

### Derleme hedefleri

| Hedef | İşlem |
|---|---|
| `make` veya `make all` | Tüm kaynakları derler ve `build/os.bin` dosyasını bağlar. |
| `make iso` | Çekirdeği ve `Boot/grub/grub.cfg` dosyasını `iso/` klasörüne kopyalar, `grub-mkrescue` çalıştırır, `disk.img` yoksa oluşturur. |
| `make run` | ISO'yu derler ve QEMU'da önyükler. |
| `make clean` | `build/`, `iso/` klasörlerini ve ISO dosyasını siler. |

## 4. Bağlama betiği (`link.ld`)

```
ENTRY(_start)
```

Giriş sembolü `_start`'tır. Montaj önyükleme kodunuz tarafından tanımlanmalıdır.

### Bellek düzeni

Çekirdek, gerçek mod bellek haritasının üzerindeki geleneksel adres olan fiziksel adres **1 MiB** (`0x100000`) adresine bağlanır. Bölümler şu şekilde sıralanır ve hizalanır:

| Bölüm | Hizalama | İçerik |
|---|---|---|
| `.multiboot` | 4 KiB | GRUB multiboot başlığı (bağlayıcının onu asla düşürmemesi için `KEEP` işaretli) |
| `.text` | 4 KiB | Makine kodu |
| `.rodata` | 4 KiB | Salt okunur veri, `__ro_start` ile `__ro_end` arasında |
| `.data` | 4 KiB | Başlatılmış veri |
| `.bss` | 4 KiB | Sıfırla başlatılmış veri, `_bss_start`'den `_bss_end`'e |
| `.stack` | 16 bayt | 16 KiB'lik stack, `_stack_start`'den `_stack_end`'e |
| `.heap` | 16 bayt | 1 MiB'lik heap, `_heap_start`'den `_heap_end`'e |

### Dışa aktarılan semboller

| Sembol | Anlamı |
|---|---|
| `_start` | Giriş noktası (siz tarafından sağlanmalıdır) |
| `__ro_start`, `__ro_end` | Salt okunur verinin sınırları |
| `_bss_start`, `_bss_end` | BSS segmentinin sınırları |
| `_stack_start`, `_stack_end` | 16 KiB'lik stack alanı |
| `_heap_start`, `_heap_end` | 1 MiB'lik heap alanı |
| `_end`, `_kernel_end` | Çekirdek imajının sonu |

**Önemli:** heap alanı bağlayıcı tarafından ayrılmıştır ancak hiçbir zaman otomatik olarak bağlanmaz. Giriş noktanızdan `heap_init((uint32_t)_heap_start, ...)` çağrısını kendiniz yapmalısınız. Depoda bunu yapan hiçbir şey yoktur.

## 5. Önyükleme akışı (planlandığı gibi)

Planlanan eksiksiz sıralama:

```
GRUB (multiboot)
   │
   ▼
_start (your assembly)         sets up a stack, checks the magic value
   │
   ▼
kernel_main (your C code)      the only place where OpenKernel is initialised
   │
   ├── vga_init()              screen output becomes available
   ├── heap_init()             kmalloc becomes usable
   ├── kbd_init()              keyboard IRQ1 enabled
   ├── vfs_init(fs) + fs_init  file system ready
   └── your application code
```

Adım 1 ve 2 için depoda kod yoktur. `notes.txt` içindeki belgeler bunun kasıtlı olduğunu doğrular: OpenKernel gömülü (embed) olmak için tasarlanmıştır, bağımsız önyüklenmek için değil.

## 6. Sürüm paketleme

`.github/workflows/release.yml`, yalnızca `main` dalına yapılan bir push `version.txt` dosyasını değiştirdiğinde çalışır.

Şu adımları gerçekleştirir:

1. `version.txt` dosyasından sürüm dizesini okur.
2. Ağacı `.git`, `.github`, `version.txt`, `install_pkg.sh`, `link.ld`, `Makefile`, `notes.txt` ve `package`'ın kendisi hariç tutularak `package/OpenKernel/` içine `rsync` ile kopyalar.
3. `Makefile`, `link.ld`, `notes.txt`, `install_pkg.sh` dosyalarını `OpenKernel/` klasörünün yanına kopyalar.
4. Her şeyi `OpenKernel_<version>.zip` olarak zip dosyasına koyar.
5. Sürüm artışları arasındaki git günlüğünden `CHANGELOG.md` dosyasını oluşturur.
6. `openkernel<version>` etiketli bir GitHub sürümü yayınlar.

**Bu iş akışı hiçbir şeyi derlemez.** Bu bir paketleme işlemidir, derleme işlemi değildir; bu nedenle bozuk bir derleme CI'ı düşürmez.

`.github/workflows/codeql.yml`, her push, pull request ve haftalık zamanlamayla C için `build-mode: none` ile CodeQL güvenlik analizini çalıştırır. Derleme adımı olmadığı için analiz, derlemeye dayalı değil yola (path) dayalıdır.

## 7. Dikkatinizi çekecek kurallar

- **`*_init()` adlandırması.** Her alt sistem bir başlatma işlevi sunar. Çağrılana kadar o alt sistemdeki çoğu işlev gereksiz işlem (no-op) veya güvensizdir.
- **Modül üst veri makroları.** Başlık dosyaları `*_MODULE_NAME`, `*_MODULE_VER`, `*_MODULE_KRNL_VER` ve benzeri sabitleri tanımlar. Bunlar bilgilendiricidir ve hiçbir mantık tarafından başvurulmaz.
- **Karakter tabanlı mod dizgileri.** Bazı işlevler, ilk karaktere göre dağıtım yapan bir `const char*` modu alır; örneğin tam temizlik için `vga_clear_screen("F")`, ilk satırı korumak için `vga_clear_screen("C")` ya da `sys_next_status("shutdown", seconds)`.
- **Genel durum.** Çoğu alt sistem, dosya içi statik bir değişken tutar (`vga_enabled_driver`, `current_fs`, `ofs`, `heap_current`). Örneğe dayalı bir tasarım yoktur.

---

[← Başlarken](02-getting-started.md) | [Belge Dizini](README.md) | [Sonraki: Çekirdek Hizmetleri →](04-kernel-services.md)
