# 02 - Başlarken

[← Giriş](01-introduction.md) | [Belge Dizini](README.md) | [Sonraki: Mimari →](03-architecture.md)

---

Bu belge, neyin kurulu olması gerektiğini, bir Linux ana makinesinin nasıl kurulacağını ve OpenKernel'i *kendi* işletim sisteminizin önyüklenebilir bir görüntüsüne dönüştürmek için gerekli tam adımları açıklar.

## 1. Gereksinimler

### Gerekli araçlar

| Araç | Neden gerekli |
|---|---|
| `i686-elf-gcc` | 32 bit freestanding x86 için çapraz derleyici. Çekirdek nesnelerini üretir. |
| `i686-elf-ld` | `link.ld` uygulayan bağlayıcı. Genellikle çapraz derleyiciyle birlikte kurulur. |
| `nasm` | Önyükleme kodunuzu `elf32` nesnelerine çevirir. |
| `make` | Derleme sürecini yürütür. |
| `qemu-system-i386` | `make run` tarafından kullanılan emülatör. |
| `grub-mkrescue` | Önyüklenebilir ISO'yu oluşturur. |
| `xorriso` | `grub-mkrescue` tarafından altyapıda kullanılır. |
| `grub-pc-bin` (Debian) / `grub2-pc-modules` (Fedora) / `grub` (Arch) | `grub-mkrescue` için 32 bit GRUB modüllerini sağlar. |

### İsteğe bağlı araçlar

| Araç | Neden |
|---|---|
| `git` | Sürüm kontrolü ve sürüm değişiklik günlüğü. |
| PulseAudio istemci kütüphanesi | `make run` komutu, `-audiodev pa` üzerinden bir Sound Blaster 16 cihazı bağlar. Çalışan bir ses arka ucu olmadan QEMU bu cihazda uyarı verebilir veya başarısız olabilir. |

### Platform

`install_pkg.sh` yalnızca **Linux**'u destekler (apt, dnf, pacman). Windows veya macOS'ta araçları elle kurmak zorundasınız.

## 2. Ana makine ortamının kurulması

```bash
chmod +x install_pkg.sh
sudo ./install_pkg.sh
```

Betik şunları yapar:

1. Paket yöneticisini algılar (`apt`, `dnf` veya `pacman`).
2. Paket listelerini yeniler. Arch Linux'ta kısmi yükseltmeler desteklenmediği için her zaman tam bir yükseltme çalışır; apt ve dnf'de tam sistem yükseltmesi yalnızca `--upgrade` verdiğinizde çalışır.
3. `Makefile`'ın ihtiyaç duyduğu her şeyi kurar: `nasm`, `make`, `xorriso`, 32 bit GRUB modülleriyle birlikte `grub-mkrescue`, `qemu-system-i386`, `qemu-img`, `git` ve QEMU'nun `-audiodev pa` için kullandığı PulseAudio istemci kütüphanesi.
4. Zaten kurulu değilse GNU kaynaklarından `i686-elf` çapraz derleyicisini (`i686-elf-gcc` + `i686-elf-ld`) derler. Bu adım binutils ve GCC'yi indirir ve `/usr/local` içine derler; bugüne kadarki en yavaş kısımdır, ilk çalıştırmada yaklaşık 15-40 dakika sürer ve sonraki çalıştırmalarda atlanır. Varsayılanlar GCC 14.2.0 ve binutils 2.43'tür; proje bu sürümlerle test edilir; gerekirse `OPENKERNEL_GCC_VERSION` ve `OPENKERNEL_BINUTILS_VERSION` ile değiştirebilirsiniz. Derleme, `/var/tmp` içinde yaklaşık 4 GB boş disk alanı gerektirir (konumu `OPENKERNEL_CROSS_BUILD_DIR` ile değiştirebilirsiniz).
5. Her aracı doğrular ve bir kontrol listesi yazdırır. Bir gerekli araç hâlâ eksikse betik sıfırdan farklı bir durumla çıkar; böylece denemeden önce `make`'in çalışacağını bilirsiniz.

Seçenekler:

| Seçenek | Etki |
|---|---|
| `--skip-update` | Kurulumdan önce paket listelerini yenileme |
| `--upgrade` | Kurulumdan önce ayrıca tam sistem yükseltmesi de yapma |
| `--skip-cross` | Çapraz derleyici derlemesini atla (`make` olmadan başarısız olur) |
| `--verify-only` | Yalnızca kontrol listesini yazdırır; hiçbir şey kurmaz ve root gerektirmez |

Herhangi bir şey kurmadan araçları istediğiniz zaman yeniden denetlemek için:

```bash
./install_pkg.sh --verify-only
```

## 3. İki dizin düzenini anlamak

Bu, projenin en çok kafa karıştıran bölümüdür, bu yüzden dikkatli okuyun.

### Düzen A - git deposu (klonladığınız şey) - Makefile'ın şu anda hedeflediği düzen

```
OpenKernel/          <- repository root, Makefile lives here
├── Makefile
├── link.ld
├── notes.txt
├── install_pkg.sh
├── version.txt
├── Kernel/
├── Drivers/
├── SystemLib/
├── ...
```

Bu düzen içinde, `Makefile`'daki `C_SRC` değişkenindeki her yol `OpenKernel/` ile başlar, örneğin `OpenKernel/Drivers/Vga/vga.c`. Makefile zaten `OpenKernel` klasörünün *içinde* olduğundan bu yol çözümlenmez. Burada `make` çalıştırmak şu hatayla başarısız olur:

```bash
make          # produces build/os.bin
```

Bu düzen baştan sona derlenir. Kalan tek bağlantı uyarısı `cannot find entry symbol _start`'dir; çünkü depo hiçbir önyükleme kodu içermez.

### Düzen B - sürüm paketi - şu anda derlenmiyor

```
project/                     <- you would run make here
├── Makefile
├── link.ld
├── notes.txt
├── install_pkg.sh
└── OpenKernel/              <- the kernel source tree
    ├── Kernel/
    ├── Drivers/
    ├── SystemLib/
    └── ...
```

`.github/workflows/release.yml` hâlâ tam olarak bu ZIP'i üretir: `Makefile`'ı bir `OpenKernel/` klasörünün yanına kopyalar; bu, önekli `C_SRC` yollarının ve `-I .`'ın varsayıdığı düzendi.

**Güncel `Makefile` artık bu düzene uymuyor.** İki şey bozulur:

- `C_SRC` yollarında `OpenKernel/` öneki yoktur; bu nedenle `project/`'e göreli olarak `OpenKernel/Drivers/Vga/vga.c` dosyası yoktur.
- Include yolu `-I .`'dır ve `project/`'in kendisi yerine `project/` dışını gösterir.

Sürüm paketini derlemek için `project/OpenKernel/` içinden `make` çalıştırın veya önekli yolları ve `-I .`'ı geri getirin. Her iki düzeni aynı anda desteklemek, `Makefile`'ın hangi düzende olduğunu algılamasını gerektirir (örneğin `VPATH := OpenKernel` artı koşullu bir include dizini); bu yapılmamıştır.

**Düzen B'yi elde etme:** GitHub Releases sayfasından bir sürüm ZIP'i indirin veya `.github/workflows/release.yml` yaptığı gibi kendiniz derleyin:

```bash
mkdir -p package/OpenKernel
rsync -av --exclude='.git' --exclude='.github' --exclude='version.txt' \
     --exclude='package' --exclude='install_pkg.sh' --exclude='link.ld' \
     --exclude='Makefile' --exclude='notes.txt' \
     ./ package/OpenKernel/
cp Makefile link.ld notes.txt install_pkg.sh package/
```

Ardından `package/` içinden `make` çalıştırın.

> Düzen B'de bile derleme bugün tam olarak başarılı olamıyor. Düzenden bağımsız olarak iki `C_SRC` yolu hatalıdır ve 31 C dosyasının 11'inde derleme hataları vardır. Bkz. [09 - Proje Durumu](09-project-status.md).

## 4. Kendi işletim sisteminizi baştan kurma

`notes.txt`, amaçlanan iş akışını tanımlar. Temel fikir: **OpenKernel önyükleme kodu sunmaz; onu siz yazarsınız.**

### Adım 1 - Bir önyükleyici veya multiboot başlığı yazın

İki seçenek:

- `link.ld`'de bildirilen `ENTRY(_start)` sembolünü izleyerek assembly ile ham bir önyükleyici yazın.
- GRUB'un çekirdeği doğrudan yüklemesi için bir GRUB multiboot başlığı yazın.

Asgari bir multiboot giriş noktası şöyledir (bu, bu depodan alınmış bir kod değil, bir kalıptır - depo hiçbir assembly içermez):

```nasm
; boot.asm
MBALIGN  equ 1 << 0
MEMINFO  equ 1 << 1
FLAGS    equ MBALIGN | MEMINFO
MAGIC    equ 0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .bss
align 16
stack_bottom: resb 16384
stack_top:

section .text
global _start
extern kernel_main
_start:
    mov esp, stack_top
    call kernel_main
    cli
.hang:
    hlt
    jmp .hang
```

### Adım 2 - Assembly dosyasını kaydedin

Dosyanızı `Makefile`'daki `ASM_SRC` değişkenine ekleyin:

```make
ASM_SRC = Boot/boot.asm
```

Makefile, bunu `nasm -f elf32` ile `build/Boot/boot.o` dosyasına çevirir.

### Adım 3 - Giriş noktanızı yazın

`kernel_main()`'i C'de uygulayın. Alt sistemleri başlattığınız yer burasıdır; çünkü **OpenKernel içindeki hiçbir şey kendini başlatmaz**:

```c
#include <OpenKernel/Drivers/Vga/vga.h>
#include <OpenKernel/SystemLib/Memory/mem.h>
#include <OpenKernel/Drivers/Keyboard/keyboard.h>
#include <OpenKernel/VFS/vfs.h>
#include <OpenKernel/OFS/ofs.h>

void kernel_main(void)
{
    vga_init();                                  /* required before any print */
    heap_init(0x100000, 1024 * 1024);            /* 1 MiB heap */
    kbd_init();
    vfs_init(OFS);
    ofs_init();

    vga_set_color_scheme(0x00, 0x0A);
    vga_print_scr_nw("My OS booted on OpenKernel 5.0");
}
```

Çağrı sırası önemlidir. Örneğin, `vga_init()` çağrılmadıysa her VGA yazma işlevi hemen döner ve `heap_init()` çağrılmadıysa `kmalloc()` her istek için `NULL` döner (geçersiz adresler vermez - ayırmayı reddeder).

### Adım 4 - Kendi kaynaklarınızı ekleyin

`.c` dosyalarınızı `Makefile`'daki `C_SRC` listesinin sonuna ekleyin. OpenKernel'in kendi girişlerini olduğu gibi bırakın.

`notes.txt`, birleştirme çakışmaları olmadan daha yeni bir sürümü sorunsuzca yerine koyabilmeniz için OpenKernel klasörünü değiştirmemenizi ister.

### Adım 5 - Bir GRUB yapılandırması sağlayın

`iso` hedefi `Boot/grub/grub.cfg` dosyasını ISO'nun içine kopyalar. Bu dosya depoda yoktur; bu yüzden onu oluşturmalısınız:

```
set timeout=0
set default=0

menuentry "My OS" {
    multiboot /boot/os.bin
    boot
}
```

### Adım 6 - Derleyin ve çalıştırın

```bash
make        # compile and link -> build/os.bin
make iso    # build ISO -> your_os.iso, create disk.img if missing
make run    # boot it in QEMU
```

`make run` şunu yürütür:

```bash
qemu-system-i386 \
    -m 512M \
    -smp 4 \
    -cdrom your_os.iso \
    -drive file=disk.img,format=raw,if=ide \
    -audiodev pa,id=snd0 \
    -device sb16,audiodev=snd0 \
    -boot d \
    -serial stdio
```

Her bayrak size ne verir:

| Bayrak | Etki |
|---|---|
| `-m 512M` | 512 MiB RAM. |
| `-smp 4` | 4 sanal CPU. OpenKernel'in SMP desteği yoktur; bu nedenle yalnızca zamanlamaya duyarlı meşgul döngüleri etkiler. |
| `-cdrom your_os.iso` | ISO'nuzu önyükler. |
| `-drive file=disk.img,...,if=ide` | 25 MiB'lik ham bir IDE diske bağlar. ATA ve OFS bunun üzerinde çalışır. |
| `-audiodev pa` / `-device sb16` | Port `0x220` üzerinde Sound Blaster 16. |
| `-boot d` | Önce CD'den önyükler. |
| `-serial stdio` | COM1 çıktısını terminalinize yönlendirir. Seri sürücü derlendiğinde kullanışlıdır. |

### Temizleme

```bash
make clean
```

`build/`, `iso/`, `your_os.iso` dosyalarını kaldırır ve `disk.img`'e dokunmaz.

## 5. Disk görüntüsüyle çalışmak

`make iso`, yoksa `disk.img` (25 MiB) dosyasını oluşturur. Ana makineden incelemek veya doldurmak için:

```bash
# list partitions
fdisk -l disk.img

# read the first 64 sectors
xxd -l 32768 disk.img
```

Bu projede bir FAT biçimlendirici yoktur. FAT okuyucularını test etmek istiyorsanız görüntü üzerinde `mkfs.fat` gibi ana makine araçlarıyla bir FAT dosya sistemi oluşturmalı ve `mtools` veya bir loop aygıtıyla bağlamalısınız.

---

[← Giriş](01-introduction.md) | [Belge Dizini](README.md) | [Sonraki: Mimari →](03-architecture.md)
