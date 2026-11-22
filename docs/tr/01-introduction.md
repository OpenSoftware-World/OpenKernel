# 01 - Giriş

[← Belge Dizini](README.md) | [Sonraki: Başlarken →](02-getting-started.md)

---

## OpenKernel nedir?

OpenKernel, 32 bit x86 makineler için çekirdek düzeyinde bir kod tabanıdır. Bir işletim sisteminin ihtiyaç duyduğu düşük düzeydeki bileşenleri sağlar: metin kipli ekran çıktısı, port giriş/çıkışı yardımcıları, bellek ve karakter dizisi rutinleri, zaman okuma, donanım sürücüleri, dosya sistemi okuyucuları ve bir C işlev sicili.

**Yeniden kullanılmak** üzere tasarlanmıştır. OpenKernel'i bir diske yazıp hazır bir ürün olarak önyüklemezsiniz; bunun yerine onu kendi projenizin içine yerleştirir, kendi önyükleyicinizi ve giriş noktanızı yazar ve işlevlerini kendi çekirdek kodunuzdan çağırırsınız.

Güncel sürüm: **4.0** (bkz. `version.txt`).

## OpenKernel ne değildir

Burada net olmak size çok zaman kazandırır:

- **Önyüklenebilir bir işletim sistemi değildir.** Depoda tek bir assembly dosyası vardır (`Kernel/Components/InterruptServices/isr.asm`, kesme stub'ları) ancak multiboot başlığı yoktur, `_start` sembolü yoktur ve `kernel_main` yoktur. `link.ld` betiği `ENTRY(_start)` bildirir, ancak bunu hiçbir şey tanımlamaz; bu nedenle bağlantı `cannot find entry symbol _start` uyarısı verir. Giriş noktasını siz sağlamak zorundasınız.
- **Eksiksiz bir çekirdek değildir.** Artık istisna/IRQ dağıtımı olan bir IDT, bir 8259A PIC yeniden eşlemesi (`Kernel/Components/InterruptServices/`) ve bir PIT zamanlayıcı sürücüsü (`Drivers/PIT/`, IRQ0 üzerinden 1 kHz) var; ancak GDT yoktur, APIC yoktur ve ayrıcalık seviyeleri yoktur. Klavye (IRQ1), fare (IRQ12), seri (IRQ4), ATA (IRQ14) ve her iki ağ kartı da kesme ile çalışır; AHCI ve SB16 ise hâlâ polling yapar.
- **Kullanıcı alanı bir syscall arayüzü değildir.** `syscall()` işlevi, çekirdek içinde sıradan bir C işlev çağrısıdır. `int 0x80` yoktur, `sysenter` yoktur ve ring geçişi yoktur.
- **Kullanıcı programları için genel amaçlı bir kütüphane değildir.** Her şey çekirdek bağlamında çalışır.

## Hedefler

Bu proje, insanların kolayca kendi programlarını yazabildiği ve kodun kolayca okunup değiştirilebildiği bir sistem oluşturmayı hedefler:

- Doğrudan donanım etkileşimini (CPU, bellek, I/O portları) anlamak.
- Mimariyi tek oturumda okunabilecek kadar küçük tutmak.
- Yapıyı modüler tutmak; böylece tek tek alt sistemler değiştirilebilir olsun.
- Türev projelerin OpenKernel yeniden düzenlemesinden hayatta kalması için kararlı bir C API sağlamak.

## Teknoloji

| Alan | Seçim |
|---|---|
| Diller | C, (önyükleme kodu için sizden assembly beklenir) |
| Derleyici | `i686-elf-gcc` freestanding, 32 bit |
| Bağlayıcı | `i686-elf-ld` ve özel bir `link.ld` |
| Assembler | NASM (`nasm -f elf32`) |
| Emülatör | QEMU (`qemu-system-i386`), VMware |
| Önyükleyici | GRUB multiboot (beklenir, içerilmez) |

Çekirdek `-m32 -ffreestanding -O2` ile derlenir. Freestanding, standart C kütüphanesinin kullanılamadığı anlamına gelir: bir kütüphaneden gelen `libc`, `printf`, `malloc` yoktur. Çağırdığınız her şey proje içinde tanımlanmış olmalıdır.

## Tasarım ilkeleri

- **Ağaç dışında bağımlılık yoktur.** Çekirdeğin kullandığı her işlev depoda bulunur.
- **Modül üst bilgi başlıkları.** Çoğu alt sistem başlığı, modülü ve hedeflediği asgari çekirdek sürümünü tanımlayan `*_MODULE_NAME`, `*_MODULE_VER`, `*_MODULE_KRNL_VER` makrolarını taşır.
- **Açık başlatma.** Alt sistemler bir `*_init()` işlevi sunar. Çoğu, siz çağırmazsa sessizce hiçbir şey yapmaz.
- **Kararlı bir API katmanı.** `KernelAPI/`, ham syscall numaralarını adlandırılmış işlevlerin arkasına sarar (wrapper); böylece iç detaylar değiştiğinde kodunuz bozulmaz.

## Projeye bir bakışta yerleşim

```
OpenKernel/
├── Kernel/          Kernel core services (panic, log, paging, power, security)
├── SystemLib/       Memory, strings, I/O, time, tasks, syscalls, conversion
├── Drivers/         Hardware drivers (VGA, CPU, keyboard, mouse, serial, ...)
├── KernelAPI/       Stable C API façade for downstream operating systems
├── OFS/             Open File System (simple flat file system)
├── FAT12/ FAT16/ FAT32/   FAT readers (read-only)
├── VFS/             Thin dispatch layer over the file systems
├── Makefile         Build rules
├── link.ld          Linker script
├── install_pkg.sh   Host environment setup script (Linux only)
├── notes.txt        Short bootstrapping instructions
└── version.txt      Release version
```

Ayrıntılar [03 - Mimari](03-architecture.md) belgesindedir.

## Güncel durumun dürüst özeti

OpenKernel 4.0, 34 C dosyası ve 46 başlıkta yaklaşık 6.800 satır ile birlikte 79 satırlık tek bir assembly dosyası (`isr.asm`) içerir. Birkaç alt sistem izole olarak iyi çalışır (VGA çıktısı, CPU kontrolü, ATA PIO okumaları, karakter dizisi ve port giriş/çıkışı yardımcıları). Diğerleri kısmidir ve bazıları olduğu gibi çalışamayan yer tutuculardır.

Güncel depo durumu itibarıyla:

- **Derleme başarılı olur.** `make`, 34 C dosyasının tamamını derler ve `build/os.bin` dosyasını bağlar. Bkz. [09 - Proje Durumu](09-project-status.md).
- 34 C dosyasının tamamı `-Wall -Wextra` ile derleyici sözdizimi denetiminden geçer; yalnızca ikisi hâlâ uyarı üretir.
- Giriş noktası yoktur; bu nedenle bağlantı kurulmasına rağmen görüntü çalıştırılamaz: `_start` tanımsızdır ve multiboot başlığı yoktur.

Bu kod tabanı üzerinde herhangi bir çalışma planlamadan önce 09 numaralı belgeyi okuyun.

---

[← Belge Dizini](README.md) | [Sonraki: Başlarken →](02-getting-started.md)
