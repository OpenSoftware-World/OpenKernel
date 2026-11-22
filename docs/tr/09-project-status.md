# 09 - Proje Durumu

[← Çekirdek API](08-kernel-api.md) | [Belge Dizini](README.md) | [Sonraki: Katkıda Bulunma →](10-contributing.md)

---

Bu belge, kod tabanının depoda bugünkü hali hakkındaki dürüst bir değerlendirmesidir. Varlığı, sizin bu sorunları birer birer derleme hatası şeklinde keşfetmemeniz içindir.

Aşağıdaki her şey kaynak ağacına karşı doğrulanmıştır. Bir iddia çalıştırmaya değil statik okumaya dayanıyorsa, bu belirtilmiştir.

## 1. Derleme doğrulama sonuçları

Test ortamı: `i686-elf-gcc` 14.2.0, `nasm` 2.16.01, `i686-elf-ld` (binutils) 2.42, bayraklar `-m32 -ffreestanding -O2 -Wall -Wextra -I .`; tüm 34 `.c` dosyası üzerinde çalıştırıldı.

| Sonuç | Adet |
|---|---:|
| Sözdizimi denetiminden geçer (`-fsyntax-only`) | 34 |
| Sözdizimi denetiminden geçemez | 0 |
| Tam derlemeden geçer (`-c`) | 34 |
| Hâlâ uyarı üreten dosyalar | 1 |

Geçen: depodaki her `.c` dosyası.

Hâlâ uyarı veren tek dosya:

| Dosya | Uyarı | Neden |
|---|---|---|
| `SystemLib/KernelFunc/kernel_func.c` | `'kfunc_s' defined but not used` | Dosya kapsamındaki ölü değişken. Kozmetik. |

`SystemLib/SysCalls/syscall.c` dosyasının eskiden verdiği dört `cast from pointer to integer of different size` uyarısı gitti: KernelAPI argüman kuralı düzeltildi (bkz. 5. bölüm, 4. madde) ve dağıtıcı artık `arg`'ı dönüştürmek yerine çözümlüyor.

Bağlama sonucu: `i686-elf-ld -m elf_i386 -T link.ld <objects>` **0** ile çıkar ve 53 KiB'lik bir imaj üretir. Tek tanısal mesaj `warning: cannot find entry symbol _start` uyarısıdır (aşağıdaki engelleyiciye bkz.).

`make` **0** ile çıkar ve `build/os.bin` dosyasını üretir.

Include kökü üzerine not: Yukarıdaki testler `-I .` kullanır, çünkü her dosya başlık dosyalarını `<OpenKernel/...>` biçiminde içerir. `Makefile` zaten `-I ..` geçirir; depo kökünden `-I .` kullanmak, depo kökünün *doğrudan* `OpenKernel` olması nedeniyle neredeyse her dosyayı eksik include nedeniyle başarısız kılar.

## 2. Derleme durumu

Bu belgenin önceki sürümleri beş engelleyici listeliyordu. Dördü düzeltildi. Kalan önce listeleniyor.

### Kalan engelleyici - giriş noktası yok

`link.ld` `ENTRY(_start)` bildirimini yapar, ancak depada hiçbir yerde `_start`, `kernel_main` veya multiboot başlığı yoktur. `isr.asm` yalnızca kesme stub'larını sağlar.

Sonuçlar, onlarla karşılaşma sıranıza göre:

1. Bağlama `warning: cannot find entry symbol _start; defaulting to 00100000` uyarısıyla tamamlanır. İmajın tanımlı bir giriş noktası yoktur, bu nedenle önyüklenemez.
2. `Boot/` dizini yoktur; bu nedenle bağlama başarılı olsa bile `make iso`, `cp Boot/grub/grub.cfg /boot/grub/` adımında başarısız olur.
3. Depada hiçbir zaman `vga_init()`, `pit_init()`, `pci_init()`, `heap_init()` veya `ofs_init()` çağıran bir şey yoktur. Önyüklenebilir bir imaj bile hiçbir başlatma yürütmez.

Depo ile çalışabilir bir sistem arasında duran tek şey budur.

### Düzeltildi - Makefile yolları

Her `C_SRC` kaydı, `Makefile`'ın `OpenKernel/` içinde bulunmasına rağmen `OpenKernel/` önekiyle başlıyordu ve iki yol her düzende hatalıydı (`Drivers/Sound/sb16.c` ile gerçekteki `Drivers/Sound/sb16/sb16.c` ve `rtl8139/` ile gerçekteki `RTL8139/`). Tüm önekler kaldırıldı, iki yol da düzeltildi ve `-I .`, `-I ..` olarak değiştirildi. `make` artık uçtan uca derliyor.

### Düzeltildi - derlenmeyen dosyalar

Önceki 11 dosyalık hata listesi çözüldü. Nedenlere göre gruplandırılmıştır:

| Neden | Dosyalar | Düzeltme |
|---|---|---|
| `ata_read_sector()` yoktur | `fat16.c`, `fat32.c` | `ata_sys_disk_read()` çağrılır |
| `pci_device_t` / `pci_device_count` ad uyuşmazlıkları | `pci.c` | `pci_dev_t` / `pci_dev_count` kullanır |
| Yinelenen `serial_read_char()` tanımı, `COMR_MCR` yazım hatası | `serial.c` | Bir tanım tutuldu, yazım hatası düzeltildi |
| Void olmayan işlevlerde `return;` | `keyboard.c` | Uygun değeri döndürür; aynı anda bir ring buffer eklendi |
| Hiçbir tip başlığı içeri alınmamış | `vfs.h` | `Std/types.h` dosyasını içerir |
| `fat12_list_root()` hiç bildirilmemiş | `fat12.h` | Bildirim eklendi |
| `extern ... vgabuffer = ...` bir tanımdır | `mouse.c` | Salt bir `extern` bildirimine indirildi |

`type_conv.c`, bu sürüm yazıldığında zaten geçiyordu; iki sözdizimi hatası hakkındaki daha eski iddia eskimişti.

### Düzeltildi - bağlama anı hataları

1. **`multiple definition of 'vgabuffer'`** - `mouse.c` artık sembolü tanımlamıyor.
2. **`undefined reference to 'ata_read_sector'`** - yukarıdaki yeniden adlandırma ile gitti.
3. **`undefined reference to 'vfs_list'`, `'fat32_list_dir'`, `'fat12_list_root'`, `'get_char'`, `'pci_*'`** - tümü çözüldü; bağlama, `_start` dışında temizdir.

## 3. Özellik bazında durum

Gösterge: **Çalışır** = başlatıldıktan sonra yalın olarak işlevseldir. **Kısmen** bazı koşullarla kullanılabilir. **Stub** = yazıldığı gibi işlev göremez. **Hiç yok** = hiç mevcut değil.

Bu bölümdeki hiçbir şey çalıştırılmamıştır. "Çalışır" ifadesi "temiz derleniyor ve mantık doğru okunuyor" demektir; "çalışma zamanında doğrulanmıştır" demek değildir.

### Çalışır

| Özellik | Kanıt |
|---|---|
| VGA metin çıktısı | `vga.c` temiz derlenir, eksiksiz işlev seti |
| CPU kontrolü (`cli`, `sti`, `hlt`, CPUID) | `cpu.c` temiz derlenir; `cpuinfo_data()` artık `void` |
| ATA PIO tek sektör okuma/yazma | `ata.c` temiz derlenir; LBA28, sınırlı bekleme, ERR/DF işleme |
| Dize işlevleri | `std.c` derlenir, standart anlamlar |
| Port giriş/çıkışı yardımcıları | `io.h`, yalnızca başlık dosyası, doğru satır içi montaj |
| Bump tahsis edici | `mem.c` derlenir; `kmalloc()` artık taşacak veya saracak (wrap) boyutları reddeder. Yine de `heap_init()` gerekir |
| RTC zaman okuma | `time.c` derlenir, CMOS erişimi doğru |
| Kilitlemeli işlev kayıt listesi | `kernel_func.c` derlenir (bir kullanılmayan değişken uyarısı) |
| Panik işleyicisi | `panic.c` derlenir |
| Kapatma / yeniden başlatma | `sysmng.c` derlenir; syscall sarmalayıcıları artık `"shutdown"` / `"reboot"` geçirdiğinden dal erişilebilir durumda |
| 4 MiB için kimlik eşlemeli (identity map) sayfalama | `vmem.c` derlenir, hiç çağrılmaz |
| OFS oluşturma/okuma/yazma/silme/listeleme | `ofs.c` derlenir; işaretçi aritmetiği, superblock giriş/çıkışı ve tablo kalıcılığı düzeltildi |
| Kesme stub'ları ve dağıtım | `isr.asm` nasm ile derlenir, `interrupt.c` / `pic.c` derlenir |
| SB16 doğrudan DAC oynatma | `sb16.c` derlenir, DMA veya IRQ yok |
| KernelAPI sarmalayıcıları | Her iki `.c` dosyası da derlenir, ancak derlemede değildir |

### Kısmen

| Özellik | Eksik veya hatalı olan |
|---|---|
| Klavye sürücüsü | Artık IRQ1 tarafından sürülen (ring buffer işleyici tarafından yazılır, `get_char()` `hlt()` içinde uyur), ancak hâlâ Shift/Alt/Ctrl yok ve `kbd_init()` hiç çağrılmaz |
| Fare sürücüsü | Artık IRQ12 tarafından sürülen; bağlamayı bozan yinelenen `vgabuffer` gitti. Tıklama işleme yok |
| AHCI sürücüsü | Yapı assert'leri ve sınırlı zaman aşımıyla temiz derlenir; donanımda test edilmemiştir. `ahci_init()` çağıran bir şey yoktur ve bilinçli olarak anlık sorgulama (polling) modunda kalır (bkz. [06 - Sürücüler](06-drivers.md) 8. bölüm) |
| Seri port sürücüsü | Temiz derlenir, TX/RX ring'leri ve anlık sorgulama yedeğiyle IRQ4 tarafından sürülen. 16550 mantığı doğrudur ve `make run` zaten `-serial stdio` geçirir, ancak hiç çalıştırılmamıştır |
| Güvenlik denetleyicisi | Temiz derlenir, yedi denetimin tümü gerçek; hiç çağrılmamış, çalışma zamanında test edilmemiş |
| VFS | Beş işlev (`init`, `list`, `read`, `create`/`write`/`delete`), tek bir genel dosya sistemi, yol veya tutamaç yok; derleniyor ancak `boot_lba` `0`'a sabit kodlanmış |
| FAT12 / FAT16 / FAT32 | Paylaşılan `FATLib/fatlib.c` çekirdeği üzerinden kök dizin dosyalarının okunması **ve** yazılması, yalnızca 8.3 adlar; dört dosyanın tümü derlenir, geometri doğrulanır, aktarımlar sınırlıdır. Hiç çalıştırılmamıştır |
| Görev yöneticisi | Yalnızca veri yapıları; bağlam değişimi yok, öncelik alma (preemption) yok. Liste dolaşmaları artık sınırlıdır ve baştan çıkarma doğrudur |
| KernelAPI | Argüman kuralı, dönüş değerleri ve eksik `sys_next_status` sarmalayıcısı düzeltildi (bkz. [08 - Çekirdek API](08-kernel-api.md) 5. bölüm); hâlâ varsayılan derlemede değil |
| Bellek yönetimi | `kfree`'si gereksiz işlem (no-op) olan bump tahsis edici; `heap_init()` hiç çağrılmaz, bu nedenle `kmalloc()` bugün NULL döndürür |
| PCI veriyolu | Derler, tarar, BAR'ları çözümler ve komut bitlerini etkinleştirir; hiç çalıştırılmamış, dolayısıyla hiçbir BAR değeri gözlenmemiştir |
| e1000 / RTL8139 | BAR keşfi, cihaz kimliği denetimleri, NULL korumaları (guard) ve etkinleştirilmiş bir PCI kesmesi (IMS/IMR + işleyici) yerinde; MAC okuma yok, bağlantı kurulumu yok, hiç çalıştırılmamış |
| Zaman kütüphanesi | `sleep()` artık `pit_sleep_ms()`'e devreder; PIT çalışmıyorsa veya IF temizse hemen döner |

### Stub veya yer tutucu

| Öğe | Kanıt |
|---|---|
| Ethernet katmanı | 23 satırlık mantık, kaynak MAC `0xAA`'ya sabit kodlanmış ve altı kez tekrarlanıyor, `ethernet_init()` bildirilmiş ancak hiç tanımlanmamış, gönderim her zaman cihaz listesinin başına gidiyor |
| `kfree()` | Gövdenin tamamı `(void)ptr;` |
| `ata_sys_disk_init()` | Boş bir gövdeydi; şimdi IRQ14'e bağlanır ve nIEN'i temizler, ancak depada bunu henüz çağıran bir şey yok |
| `schedule()` / `yield()` | Bağlam değişimi montaj kodu yok; `yield()` görev işlevini sürdürmek yerine yeniden başlatır |
| `qemu_io.h` | Hiçbir dosya tarafından hiç içerilmemiş |
| `openkernel_drivers_desc.h`, `openkernel_systemlib_desc.h` | Dosya kapsamında geçersiz C (işlev dışında atamalar), hiç içerilmemiş, yazım hataları içeriyor |
| `heap_init()` | Tanımlanmış ancak hiç çağrılmamış; `link.ld` zaten `_heap_start` / `_heap_end` ile 1 MiB'lik `.heap` bölümünü ayırıyor |

### Tamamen yok

- Önyükleyici, `_start`, multiboot başlığı, `kernel_main`, `Boot/` dizini
- GDT veya APIC programlama (IDT, 8259A PIC yeniden eşlemesi ve istisna/IRQ dağıtımı `Kernel/Components/InterruptServices/` içinde yer alır)
- Çekirdek başına zamanlayıcı; IRQ0 PIT tarafından sürülen, APIC yoktur
- Gerçek öncelik alanlı (preemptive) çoklu görev veya bağlam değişimi
- Kullanıcı alanı syscall kapısı (`int 0x80`, `sysenter`, ring geçişleri)
- `printf` veya herhangi bir varargs biçimlendirme
- Serbest liste bellek tahsisi, bellek geri kazanımı, sanal-fiziksel eşleme API'si
- Sayfa hatası (page fault) işleme
- OFS'te dizin desteği, FAT12/FAT16'da alt dizin okumaları ve FAT sürücüleri üzerinden alt dizin oluşturma
- FAT biçimlendirme - FAT sürücüleri mevcut bir birimi bağlar ve yazar, ancak projede önyükleme sektörünü yazan bir şey yok
- Bağlama tablosu veya gerçek bir VFS soyutlaması
- Gerçek donanıma ulaşan herhangi bir ağ gönderme veya alma yolu
- Herhangi bir otomatik test paketi veya CI derleme işlemi

## 4. README iddiaları ile gerçeklik

Proje README'si bir özellik kontrol listesi sunar. Kodla karşılaştırması şöyle:

| README iddiası | Hüküm |
|---|---|
| VGA metin modu çıktı sistemi | Doğru |
| Temel CPU kontrolü (CLI, HLT vb.) | Doğru |
| Çekirdek hizmet altyapısı | Kısmen - kod derleniyor ancak hizmetleri çağıran bir şey yok |
| Bellek yönetimi | Abartılmış - `kfree`'si gereksiz işlem (no-op) olan bir bump tahsis edici ve `heap_init()` hiç çağrılmaz |
| Kesme işleme (IDT) | Doğru - `Kernel/Components/InterruptServices/` IDT'yi, PIC yeniden eşlemesini ve dağıtımı sağlar; işleyici kayıt listesi zincirlidir ve PIT, Klavye, Seri port, ATA, Fare ile her iki ağ sürücüsü bir işleyici kaydeder |
| Klavye sürücüsü | Kısmen - ring buffer ile IRQ1 tarafından sürülen, ancak değiştirme tuşları yok ve `kbd_init()` hiç çağrılmaz |
| Dosya sistemi (OFS) | Kısmen - OFS derleniyor ve tablosunu artık kalıcı tutuyor; hâlâ dizinleri yok ve okumada tampon uzunluğu parametresi yok |
| Proje yapısı bir `Boot/` klasörü listeliyor | Yanlış - depoda `Boot/` dizini yok |
| `make` ardından `make run` | Kısmen - `make` çalışıyor; `make iso` var olmayan bir `Boot/grub/grub.cfg`'ye ihtiyaç duyuyor ve `make run` önce o imaja ihtiyaç duyuyor |

## 5. Önce düzeltilmeye değer bilinen hatalar

Diğer çalışmaları ne kadar engellediklerine göre kabaca sıralanmıştır.

1. **Giriş noktası yok** - multiboot başlıklı ve `_start` içeren `Boot/boot.asm` dosyasını yazın, ayrıca `Boot/grub/grub.cfg`. Bu olmadan başka hiçbir şey doğrulanamaz.
2. **`heap_init()` hiç çağrılmaz** - `link.ld` zaten `_heap_start` / `_heap_end` ayırıyor. Bağlanana kadar `kmalloc()` her talep için NULL döndürür; bu artık güvenlidir (e1000 sürücüsü NULL işaret etmek yerine geri çekilir) ancak hiçbir dinamik tahsisin çalışmadığı anlamına gelir.
3. **`kfree()` gereksiz işlem (no-op)** - onsuz heap, sert bir tavanı olan tüketilebilir bir kaynaktır.
4. **KernelAPI skaler argüman kuralı** - düzeltildi. `syscall_handler()` artık `arg`'ı dönüştürmek yerine çözümlüyor; sarmalayıcıların birleşik `syscall(id, &arg)` kuralıyla eşleşiyor ve değer döndüren çağrılar sonuçlarını iletir. Bu ayrıca `syscall.c` içindeki kalan dört derleyici uyarısını da kaldırdı. Dağıtım yolu QEMU altında çalıştırıldı (bkz. 7. bölüm).
5. **OFS `ofs_read_file()` tampon uzunluğu almıyor** - sektör sayısı yalnızca `size_bytes`'den gelir; bu nedenle çağıran, tamponun yeterince büyük olduğunu garanti etmek zorundadır. Güvenli olması için bir `size` parametresi gerekir.
6. **OFS disk biçimi değişti** - ayrılmış dosya tablosu alanı 8 sektörden (4.096 bayt; 128 x 44 baytlık girişler için çok küçük) türetilmiş 11 sektöre (5.632 bayt) çıktı. `ofs_init()`, `data_start >= file_table_start + 11` koşulunu doğrular ve uymayan bir imajı yeniden biçimlendirir; bu nedenle önceki kodla yazılmış bir disk ilk bağlamada içeriğini kaybeder. Bu kasıtlıdır; tabloyu veri alanıyla sessizce çakıştırmaktan daha iyidir.
7. **`e1000_send_packet()` `TDT = tx_index` yazar** - kuyruk tanımlayıcı yazmacı, sürücünün doldurduğu son tanımlayıcının *ilerisini* işaret edir; bu nedenle NIC, az önce kuyruğa alınan paketi hiçbir zaman göndermez. Bu turda değiştirilmedi; doğrulamak için bir donanım testi gerekir.
8. **RTL8139 alma tamponu hizalaması** - NIC, 8 KiB'lik RX tamponunun 256 baytlık sınırda olmasını ister. `rx_buffer`, hizalama özniteliği olmayan sıradan bir statik dizidir.
9. **`ethernet_init()`** - bildirilmiş, hiç tanımlanmamış. Bugün zararsızdır çünkü hiçbir şey çağırmaz, ancak oluşmayı bekleyen bir bağlama hatasıdır. (`openkernel_nonuniversal_api_sys_next_status()` aynı soruna sahipti ve artık `SYSCALL_SYS_NEXT_STATUS`, 43 üzerinde uygulanmıştır.)
10. **Önyükleme yolu yok, test yok, CI yok** - bu depoda hiçbir şey hiç çalıştırılmamıştır.

## 6. Önerilen başlangıç noktası

Amacınız mümkün olduğunca çabuk önyüklenebilir bir imaj elde etmekse:

1. multiboot başlıklı ve `_start` içeren `Boot/boot.asm` dosyasını yazın, ayrıca `Boot/grub/grub.cfg`. Bu, kalan tek bağlama anı engelleyicisidir.
2. `vga_init()`, `pit_init(1000)`, `sti()` çağıran ve bir şeyler yazdıran bir `kernel_main()` yazın.
3. `kernel_main()` içinde tahsis yapan her şeyden önce `heap_init((uint32_t)_heap_start, _heap_end - _heap_start)` çağrısını bağlayın.
4. Yeni BAR keşfi yolunu denemek için önce `pci_init()` ardından `pci_dev_init()` çağırın.
5. `make` ile derleyin, ardından `make iso` ve `make run`.
6. Alt sistemleri kademeli olarak ekleyin ve her birinden sonra derlemeyi kontrol edin.

Tüm ağaç artık tek bir geçişte derleniyor ve bağlanıyor; bu nedenle ilerlemek için bir alt kümenizi derlemeniz gerekmiyor.

## 7. Güven notları

- **Derleyici çalıştırılarak doğrulandı:** 34/0 sözdizimi ayrımı, 34/0 nesne derlemesi, temiz bağlama, `make` başarısı, kalan tek uyarı veren dosya ve `_start` / `kernel_main` / `Boot/`'un yokluğu.
- **Çalıştırılarak doğrulandı:** KernelAPI dağıtım yolu, sahte (stub) çekirdek işlevleriyle kullanılan geçici bir multiboot test imajı altında `qemu-system-i386` üzerinde çalıştırıldı. 38 denetimin tümü geçti: her sarmalayıcı argümanlarını değişmeden eşleşen stub'a iletti, dönüş değerleri (`kmalloc`, `memset`, `memcpy`, `memcmp`, `strcmp`, `strncmp`, `strlen`, FAT okumaları, `get_char`) iletildi ve geçersiz syscall kimlikleri hâlâ `-1` döndürüyor. Bu, bu depoda hiç çalıştırılmış ilk parçadır.
- **Kaynak okunarak doğrulandı:** OFS işaretçi aritmetiği ve tablo kalıcılığı kusurları, FAT önyükleme sektörü taşmaları, `0xDEADBEEF` ve `0xF0000000` sabitleri (şimdi BAR keşfiyle değiştirildi), `0x0E` sürücü seçim sözcüğü, `task_sleep()` birim uyuşmazlığı, `task_remove()` baştan çıkarma kusuru ve syscall argüman uyuşmazlığı.
- **Doğrulanmadı:** gerçek donanımda veya QEMU'da çalışma zamanı davranışı. Bu depoda hiçbir şey çalıştırılmamıştır. Yukarıdaki her "Çalışır" girdisi yalnızca derlemeyle doğrulanmış ve mantık açısından incelenmiştir. PCI, ağ, ATA, FAT ve OFS değişiklikleri hiç donanıma dokunmamıştır; bu nedenle BAR değerleri, zaman aşımı ve disk biçimleri doğrulanmamıştır.

---

[← Çekirdek API](08-kernel-api.md) | [Belge Dizini](README.md) | [Sonraki: Katkıda Bulunma →](10-contributing.md)
