# 06 - Sürücüler

[← Sistem Kütüphanesi](05-system-library.md) | [Belge Dizini](README.md) | [Sonraki: Dosya Sistemleri →](07-file-systems.md)

---

`Drivers/` donanım arayüz katmanını içerir. Bu belge her sürücüyü, konuştuğu donanımı, genel (public) işlevlerini ve gerçek durumunu listeler.

Her sürücü için iki kural geçerlidir:

1. **Önce `*_init()` işlevini çağırmalısınız.** Çoğu sürücü bir "etkin" (enabled) bayrağı tutar ve başlatma tamamlanana kadar sessizce hiçbir şey yapmaz.
2. **Girdi, zamanlayıcı, seri ve disk sürücüleri kesme ile sürülür.** Bir IDT, PIC yeniden haritalama ve dağıtıcı `Kernel/Components/InterruptServices/` içinde yer alır; dağıtıcı vektör başına bir işleyici *listesi* tutar, böylece aygıtlar bir PCI IRQ hattını paylaşabilir. PIT (IRQ0), Klavye (IRQ1), Seri (IRQ4), ATA (IRQ14), Fare (IRQ12) ve iki ağ kartı (yapılandırmadaki `0x3C` PCI kesme hattı) her biri `*_init()` içinde bir işleyici kaydeder ve hattının maskesini kaldırır. AHCI ve SB16 hâlâ yoklamalı (poll) kalır - nedenleri için bkz. ilgili bölümler.

## Sürücü durumu özeti

| Sürücü | Yol | Durum |
|---|---|---|
| VGA metin modu | `Drivers/Vga/` | Çalışıyor |
| CPU denetimi | `Drivers/Cpu/` | Çalışıyor (kısmi özellik algılama) |
| PIT (8253/8254) | `Drivers/PIT/` | Hatasız derleniyor, IRQ0'i kaydediyor, donanımda test edilmedi |
| ATA (PIO) | `Drivers/Ata/` | Hatasız derleniyor; IRQ14 bekleme + yoklama yedeği, test edilmedi |
| Klavye | `Drivers/Keyboard/` | IRQ1 ile sürülen, halka tampon bağlanmış, değiştirici tuşlar yok |
| Fare | `Drivers/Mouse/` | IRQ12 ile sürülen, paket birleştirme işleyicide |
| Sound Blaster 16 | `Drivers/Sound/sb16/` | Temel ama işlevsel - yoklamalı (poll) kalır (protokol el sıkışması) |
| AHCI | `Drivers/Ahci/` | Hatasız derleniyor, yeniden düzenlendi, donanımda test edilmedi - yoklamalı kalır (bkz. 8. bölüm) |
| Seri (COM1) | `Drivers/Serial/` | IRQ4 ile sürülen TX + RX halkaları, yoklama yedeği ile, test edilmedi |
| PCI | `Drivers/PCIBus/` | Hatasız derleniyor, kesme hattı/iğnesini de tarıyor, test edilmedi |
| Ethernet katmanı | `Drivers/Network/Ethernet/` | İskelet, hatasız derleniyor |
| Intel e1000 | `Drivers/Network/e1000/` | PCI kesmesi kuruldu (IMS/ICR), RX işleyicide boşaltılıyor, test edilmedi |
| Realtek RTL8139 | `Drivers/Network/RTL8139/` | PCI kesmesi kuruldu (IMR/ISR), RX işleyicide boşaltılıyor, test edilmedi |
| Framebuffer | `Drivers/Framebuffer/` | Çalışıyor - önyükleyici lineer FB + VGA mod 13h yedeği, QEMU'da doğrulandı (GRUB multiboot1 ve host harness) |

## 1. VGA metin modu - `Drivers/Vga/`

Projenin en eksiksiz sürücüsü. Fiziksel adres `0xB8000` üzerinde 80x25 metin modu.

### İşlevler

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

### Bilinmesi gereken davranış

- **`vga_init()` zorunludur.** Her yazdırma işlevi dahili bir `vga_enabled_driver` bayrağını kontrol eder ve bu yanlışsa hemen geri döner. Bunu unutmak, "hiçbir şey yazmıyor" hatalarının en yaygın nedenidir.
- `vga_clear_screen("F")` tüm 25 satırı temizler. `vga_clear_screen("C")` 1'den 24'e kadar olan satırları temizler, 0. satırı başlık olarak korur.
- `vga_cursor_mode("E", start, end)` imleci verilen tarama çizgileriyle gösterir; `vga_cursor_mode("D", 0, 0)` onu gizler.
- `screen_buffer[80*25]` gölge bir kopya tutulur ve `screen_init()` tarafından yenilenir. Fare sürücüsü kendini silmek için bunu kullanır.
- `vga_print_scr()` yeni satır ekler; `vga_print_scr_nw()` eklemez.
- İmleç hareketi CRT denetleyici portları `0x3D4` ve `0x3D5` üzerinden donanım tarafından yapılır.

### Sınırlamalar

- **`printf` yoktur.** `print_hex`, `print_dec`, `print_bin` ve ham metinleriniz vardır. Biçimlendirilmiş çıktı elle bir tampona derlenmelidir.
- Gerçek kaydırma yoktur: imleç son satırı geçtiğinde ekran kaydırılmaz, silinir; böylece üstteki metin kaybolur.
- Renk `vga_set_bg_color` / `vga_set_text_color` ile hücre bazında ayarlanır; bunlar bir bütün bölgeyi değil, güncel öznitelik (attribute) genel değişkenini günceller.

## 2. CPU denetimi - `Drivers/Cpu/`

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

`get_sys_cpuinfo("B")` marka (brand) dizgisi için CPUID yaprakları `0x80000002` ile `0x80000004` arası okur; `"V"` üretici (vendor) dizgisi için yaprak 0'ı okur.

Sınırlamalar:

- `sys_cpuinfo_t`, **hiçbir zaman doldurulmayan** 17 özellik bayrağı bildirir (`sse`, `sse2`, `avx`, `mmx`, `avx2`, `vmx`, `sgx` ve diğerleri). Özellik algılama henüz yoktur.
- `cpuinfo_data()` `void`'tur: CPUID dört çıkış işaretçisi üzerinden yazar, dolayısıyla döndürülecek bir şey yoktur. Eskiden `return` olmayan `uint32_t` olarak bildirilmişti ve bu, düzeltilmek yerine bir `#pragma GCC diagnostic` ile susturulmuştu.

## 3. Klavye - `Drivers/Keyboard/`

Portlar `0x60` (veri) ve `0x64` (durum) üzerinde PS/2 klavye. **IRQ1 üzerinde kesme ile sürülür** (vektör 33).

```c
void    kbd_init(void);
char    get_char(void);
uint8_t kb_check(void);
void    kbd_hndlr(uint8_t scancode);
void    kbd_close(void);
```

`kbd_init()`, `PIC_IRQ_BASE + KBD_IRQ_LINE` için `kbd_irq_handler()`'ı kaydeder ve IRQ1'in maskesini kaldırır (önce `interrupt_init()` gerekir; aynı işleyiciyi iki kez kaydetmek hiçbir şey yapmaz). Bundan sonra işleyici 8042 çıkış tamponunu boşaltır, her baytı durum yazmacındaki yardımcı (auxiliary) bayrağına göre yönlendirir - bu bayrak olmayan bir bayt klavyeye aittir ve `kbd_hndlr()`'a gider, yardımcı bir bayt `mouse_handle_byte()`'a gider - ve `interrupt_dispatch()` EOI gönderir.

`kbd_hndlr()` kopuş (break) kodlarını yok sayar, `0x3A` scancode'unda Caps Lock'u değiştirir, yapma (make) kodlarını 128 girdili ABD düzeni tablosu üzerinden eşler ve sonucu bir halka tampona iter (`buf_head` / `buf_tail` ile `kbf[KEY_BF_SIZE]`; ikisi de `volatile` çünkü IRQ1 onları yazar). `get_char()` o tamponu boşaltır ve boşken, IRQ1 doldurana kadar `hlt()` içinde uyur - kesmeler kapalıyken bunun yerine döner (spin), çünkü o zaman IRQ gelmez. `kb_check()` bekleyen bir şey olup olmadığını bildirir.

Sınırlamalar:

- **Halka tampon doluyken karakterleri engellemek (block) veya atmak yerine düşürür.**
- Yalnızca ABD düzeni. Shift, Alt veya Ctrl işleme yoktur; dolayısıyla Shift gerektiren semboller hiçbir zaman doğru görünmez.
- `get_char()`, sürücü hâlâ etkinken IRQ1 tekrar maskelenirse (örn. `kbd_close()` ile) sonsuza kadar bloklanır.
- Klavye ve fare sürücüleri birbirlerinin bayt işleyicisini çağırır çünkü tek bir 8042 denetleyicisini paylaşırlar; birini kaldırırsanız her iki dosyayı da `C_SRC`'te tutun (veya yönlendirmeyi stub'layın).
- `kbd_init()` depoda hiçbir yerde çağrılmaz; dolayısıyla `kbd_enabled_driver` yanlış kalır ve `kernel_main()`'iniz onu çağırana kadar her giriş noktası 0 döndürür.

## 4. Fare - `Drivers/Mouse/`

Aynı portlarda PS/2 yardımcı (auxiliary) faresi. **IRQ12 üzerinde kesme ile sürülür** (vektör 44).

```c
void mouse_init(void);
void mouse_handle_byte(uint8_t data);   /* one byte from the 8042 */
void mouse_draw(void);
void mouse_restore(void);
void mouse_update(void);                /* redraw if IRQ12 moved the cursor */
void mouse_close(void);
```

`mouse_init()` yardımcı aygıtı etkinleştirir (`outb(0x64, 0xA8)`), Compaq durum yazmacının 1. bitini ayarlar, `0xD4` öneki üzerinden varsayılan yapılandırma komutunu (`0xF6`) ve etkinleştirme komutunu (`0xF4`) gönderir, tamponu boşaltır, IRQ12 için `mouse_irq_handler()`'ı kaydeder ve hattın maskesini kaldırır.

IRQ işleyicisi paylaşılan 8042 çıkış tamponunu boşaltır ve her baytı yardımcı bayrağına göre yönlendirir: yardımcı bir bayt `mouse_handle_byte()`'a gider, klavye baytı `kbd_hndlr()`'a. `mouse_handle_byte()` standart 3 baytlık paketi birleştirir, `bit3` senkron bitini doğrular, `mouse_x` / `mouse_y`'i günceller (Y ekseni ters çevrilmiştir) ve konumu 80x25 ekrana kırpır (clamp). `mouse_draw()` `0x1F` özniteliğiyle `0xDB` blok karakterini yazar; `mouse_restore()` `screen_init()`'in yakaladığı ne varsa geri koyar. `mouse_update()` yalnızca işleyici imleci hareket ettirdiğinde yeniden çizer - artık yoklanacak bir şey yoktur.

Sınırlamalar:

- Düğme basmaları paketin parçası olarak alınır ancak bir tıklama işleme yoktur.
- İmleç VGA hücrelerinin doğrudan üzerine yazarak çizilir; dolayısıyla metnin üzerine binebilir.
- `mouse.c` eskiden `extern uint16_t *vgabuffer = (uint16_t *)0xB8000;` bildiriyordu; bu bir başlatıcılı (initializer) bir *tanımdır* ve bağlantı (link) zamanında `vga.c` ile çakışıyordu. Artık sıradan bir `extern` bildirimdir; tanım `vga.c`'de kalır.
- `mouse_clear()`, `mouse_put_char()` ve `prev_cell` başlıkta bildirilir ama hiç tanımlanmaz.
- Eski `mouse_poll()` giriş noktası artık yok: paket birleştirme artık IRQ işleyicisinde oluyor.

## 5. Seri - `Drivers/Serial/`

`0x3F8` üzerinde COM1, 38400 baud, 8N1. **IRQ4 üzerinde kesme ile sürülür** (vektör 36): hem gönderim hem alım halka tamponlardan geçer.

```c
void serial_init(void);
int  serial_is_transmit_empty(void);
void serial_print_char(char c);           /* queues; IRQ4 drains */
void serial_print_string(const char *string);   /* converts \n to \r\n */
char serial_read_char(void);               /* blocks in hlt() on IRQ4 */
int  serial_received(void);
int  serial_try_read_char(char *c);
int  serial_has_error(void);
void serial_clear_errors(void);
void serial_log/success/error/warn/info/unknown(const char *msg);
void serial_set_status(bool_t val);
void serial_loopback_test(void);
```

`serial_init()` klasik 16550 sırasını uygular: kesmeleri devre dışı bırakır, DLAB bitini ayarlar, böleni 3 (38400) olarak programlar, 8N1'i yapılandırır, FIFO'yu etkinleştirir ve temizler (`0xC7`), MCR'ı `0x0B` olarak ayarlar, `0xAE` yazıp geri okuyan bir döngü (loopback) testi çalıştırır, sonra modemi normal moda bırakır (`0x0F`; bu OUT2'yi ayarlı tutar - PIC yalnızca OUT2 yüksekken COM1'in IRQ'sunu görür). Son olarak IER'e `0x03` yazar (alınan veri + THR boş), IRQ4 için `serial_irq_handler()`'ı kaydeder ve hattın maskesini kaldırır.

Bilinmesi gereken davranış:

- **İşleyici kesme tanımlama yazmacını (IIR) okur**; bu yazmaç hem nedeni bildirir hem de onu temizler: hiç temizlenmeyen bir THRE kesmesi UART'ın INTR hattını yüksek bırakır ve paylaşılan bir IRQ hattındaki sonraki cihazı aç bırakır (seviye tetiklemeli bir PIC'i kesme fırtınasına uğratır). THR boş olduğunda sıradaki kuyruktaki baytı yazar; veri hazır / FIFO zaman aşımında UART'ı alma halkasına boşaltır. `interrupt_dispatch()` EOI gönderir.
- **`serial_print_char()` baytı kuyruğa alır ve vericiyi (transmitter) tetikler.** IRQ4 porta sahipken bu tetikleme kesmeler kapalıyken çalışır (o yazan işleyicinin kendisi) - olmadan boştaki bir UART, gönderim zincirini başlatan kenarı asla üretmez ve kuyruk bu kenar tetiklemeli PIC'te sonsuza kadar takılır. Hiç işleyici kurulu değilse (döngü testi başarısız) kuyruğu orijinal bloklayıcı beklemeyle sonuna kadar boşaltır; böylece bir günlükleme satırı her zaman porttan çıkar. Kuyruk doluysa kesmeler kapalıyken yer açar ve UART hiçbir şeyi hiç kabul etmiyorsa, bir günlükleme çağrısını asılı bırakmak yerine baytı düşürür.
- **`serial_read_char()` IRQ4 UART'a sahipken `hlt()` içinde uyur**; aksi halde UART'ı doğrudan hasat eder (bu, orijinal yoklamalı davranıştır). `serial_received()` / `serial_try_read_char()` halkayı kontrol eder (ve IRQ işleyici kurulu değilse onu da hasat eder).
- Başarısız bir döngü testi `serial_irq_ready`'yi yanlış bırakır: sürücü o zaman çalışamayacak bir IRQ kurmak yerine tamamen yoklamalı kalır.

Durum:

- **Hatasız derleniyor.** `serial_read_char()` iki kez tanımlanmıştı ve 121. satır `COMR_MCR`'a başvuruyordu; bir tanım kaldırıldı ve yazım hatası düzeltildi. Başlıktaki `serial_get_error()` ile kaynak artık uyumlu. `serial_loopback_test()` artık MCR'ı `0x0` yerine `0x0F`'e geri yükler; çünkü OUT2'yi düşürmek IRQ4'ü kalıcı olarak sustururdu.
- Çalışma zamanında hiç çalıştırılmadı. `make run` zaten `-serial stdio` geçiriyor; dolayısıyla bir önyükleme yolu olduğu anda çıktı terminalinizde görünür.

## 6. Sound Blaster 16 - `Drivers/Sound/sb16/`

Temel giriş/çıkışı (I/O) adresi `0x220`.

| Yazmaç | Adres |
|---|---|
| DSP sıfırlama | `0x226` |
| DSP okuma | `0x22A` |
| DSP yazma | `0x22C` |
| DSP durumu | `0x22E` |

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

`sb16_reset()` sıfırlama bitini atar ve `0xAA` onay baytını bekler. `sb16_play_buffer()` DSP komutu `0x14` (8 bit tek çevrim DAC doğrudan çıktısı) kullanır ve sonra örnek baytlarını iter.

Sınırlamalar:

- **DMA ve kesme yoktur.** Yalnızca DMA'sız doğrudan çıktı yolu uygulanmıştır; dolayısıyla tampon boyutları sınırlıdır ve işlemci yükü yüksektir.
- **Bilinçli olarak yoklamalı kalır.** DSP el sıkışması (`sb16_wait_ready()` her bayttan önce) Sound Blaster komut protokolünün parçasıdır: kart bu modda "bana sonraki baytı ver" kesmesine sahip değildir, yalnızca bu sürücüde kimsenin beklemediği bir aktarım sonu IRQ'su (IRQ5/IRQ7) vardır. El sıkışmasını dönüştürmek zaten yoklamak demektir; gerçek bir iyileştirme önce DMA gerektirir.
- `sb16.initialized` başlatma sırasında ayarlanır ama oynatmadan önce hiç kontrol edilmez.
- Ses düzeyi kontrolü yok, stereo yok, 16 bit örnek yok.
- `Makefile` yolu `Drivers/Sound/sb16.c` olarak listeler, ancak gerçek dosya `Drivers/Sound/sb16/sb16.c`'dir.

## 7. ATA - `Drivers/Ata/`

Birincil kanalda PIO modu, portlar `0x1F0` ile `0x1F7` arası, denetim yazmacı `0x3F6`'da. `ata_sys_disk_init()` çalıştıktan sonra **IRQ14 üzerinde kesme ile sürülür** (vektör 46).

```c
void ata_sys_disk_init(void);
void ata_sys_disk_read(uint32_t lba, uint8_t *buffer);
void ata_sys_disk_write(uint32_t lba, const uint8_t *buffer);
void ata_soft_reset(void);
void ata_reinit(uint8_t drive, uint16_t *identify_buf);
void ata_identify(uint8_t drive, uint16_t *buf);
```

Okumalar ve yazmalar tek sektörlü LBA28 işlemleridir: disk seçimi yazmacına `0xE0 | (lba >> 24)` yazılır (5. bit LBA modunu, 4. bit diski seçer; eski değer `0x0E` geçerli bir disk seçimi kelimesi değildi), sektör sayısı 1'e ayarlanır, üç LBA baytı yazılır, sonra `0x20` (oku) veya `0x30` (yaz) komutu verilir ve `0x1F0` üzerinden 256 kelime aktarılır.

### Bekleme nasıl çalışır

`ata_sys_disk_init()` cihaz denetim yazmacında nIEN'i temizler (böylece disk tamamlandığında IRQ14'ü yükseltir), `PIC_IRQ_BASE + ATA_IRQ_LINE` için `ata_irq_handler()`'ı kaydeder ve IRQ14'ün maskesini kaldırır. Paylaşılan bekleme çekirdeği sonra şöyle davranır:

- **IRQ bağlanmış ve kesmeler açık:** döngü durum yazmacını bir kez okur ve disk henüz hazır değilse IRQ14 (veya başka bir kesme) uyandırana kadar `hlt()` içinde uyur. Tamamlanma bayrağı kesmeler kapalıyken test edilir ve `sti` onları takip eden `hlt` sonrasına kadar maskeli tutar; böylece duraklamanın hemen öncesine düşen bir IRQ kaybolamaz.
- **IRQ bağlanmamış (`ata_sys_disk_init()` hiç çağrılmamış) veya kesmeler kapalı:** döngü orijinal sınırlı yoklama sınırıyla meşgul bekler - IRQ öncesi davranış; bugüne kadar her dosya sistemi yolu tam olarak bunu görür çünkü henüz kimse `ata_sys_disk_init()` çağırmıyor.
- Her iki mod da `ERR` / `DF` iptalini ve zaman aşımını korur; dolayısıyla bozuk bir disk asılı bırakmak yerine yine hata döndürür. IRQ yolu uyumalarını sayar (10.000 uyandırma, varsayılan 1 kHz PIT hızında yaklaşık 10 s), dolayısıyla uyanmak için *bazı* başka bir kesme kaynağına ihtiyaç duyar - yanında `pit_init()` çalıştırın.

Durum:

- **Çalışıyor (yalnızca derleme ile doğrulandı).** Bekleme yardımcıları `ERR` / `DF` durum bitlerinde iptal eder; dolayısıyla bozuk veya var olmayan bir disk, çekirdeği asılı bırakmak yerine döndürür. Disk seçimi yazmasından sonraki kısa gecikme, şartnamenin gerektirdiği 400 ns yerleşme süresini kapsar.
- Başarısız bir okuma, çağırıcının tamponunda yığın çöpü yerine sıfırlanmış bir sektör bırakır; başarısız bir yazma sektörü diskte olduğu gibi bırakır.
- Yalnızca tek sektör, çok sektörli aktarım yok, DMA yok, 48 bit LBA yok (dolayısıyla 128 GiB üzerinde disk yok).
- Üç FAT sürücüsü de doğru ad olan `ata_sys_disk_read()` çağırır. Bir önyükleme yolu `ata_sys_disk_init()` çağırana kadar yoklamalı modda çalışmaya devam ederler.

## 8. AHCI - `Drivers/Ahci/`

Depodaki en gelişmiş sürücü (500 satır). PCI BAR5 üzerinden bir AHCI ABAR sunan SATA denetleyicilerini hedefler.

```c
int  ahci_init(void);
int  ahci_scan_ports(void);
int  ahci_read_sector(int port, uint32_t lba, uint32_t count, uint8_t *buf);
int  ahci_write_sector(int port, uint32_t lba, uint32_t count, const uint8_t *buf);
int  ahci_get_port_count(void);
ahci_port_state_t* ahci_get_port(int index);
void ahci_print_info(void);
```

`ahci.h` tüm yazmaç kümesini modeller: `CAP`, `GHC`, `CAP2`, `IS`, `PI`, `VS` ve port başına `CLB`, `FB`, `CMD`, `TFD`, `SIG`, `SSTS`, `SCTL`, `SERR`, `SACT`, `CI`, `SNTF`, `FBS`; ayrıca FIS türleri ve 32 port ile 32 komut yuvası sınırı. `ahci_read_sector()` / `ahci_write_sector()` başarıda `0`, başarısızlıkta `-1` döndürür (hatalı port, zaman aşımı, cihaz hatası).

`ahci_init()` PCI'da `01:06:01` sınıfını (SATA) tarar, BAR5'i okur (bellek BAR'ı olduğunu doğrulayarak), işaret baytlarını maskeler, PCI komutunda bus mastering ve bellek çözümlemeyi etkinleştirir ve `GHC.AE`'yi ayarlar. `ahci_scan_ports()` `PI` yazmacını yineleyerek gezer, SATA imzası `0x00000101` ile eşleştirir, her portu durdurur ve yeniden başlatır (sınırlı beklemeler) ve yalnızca ondan sonra diski, komut yuvası mekanizması üzerinden verilen gerçek bir IDENTIFY (`0xEC`) komutu ile tanımlar. IDENTIFY kelimeleri 60 ve 61 LBA48 ile LBA28'i seçer; cihaz LBA desteği bildirmiyorsa port, sahte bir sektör sayısı bildirmek yerine kullanılmamış kalır.

Bilinmesi gereken davranış:

- Yazmaç düzenleri ve yapı boyutları `sizeof` ve `__builtin_offsetof` üzerinde `STATIC_ASSERT` denetimleriyle sabitlenmiştir; dolayısıyla eski sessiz düzen kayması (16 baytlık komut başlıkları, 30 baytlık D2H FIS, portları `0x100` yerine `0x60`'a koyan yanlış `reserved` boyutu) geri gelemez.
- Komut listeleri, FIS alma alanları ve komut tabloları, projenin `kmalloc()` artı sonraki iki kuvveti blok başlığı hilesi üzerine kurulu `kmalloc_aligned()`'ten gelir (1 KiB / 256 / 128 bayt hizalama). Hâlâ hiç serbest bırakılmazlar.
- Komut başlığı `PRDTL`, gerçek PRD sayısından ayarlanır (`AHCI_CMD_PRDTL(n)`) ve AHCI şartnamesinin gerektirdiği `CFL = 5` her komut FIS'i için ayarlanır.
- Aktarımlar PRD sınırlarında (8 KiB parçalar) ve 4 MiB'lik LBA48 aralık sınırında bölünür; disk `TFES` (`SERR.TF` + `SERR.DIAG`) bayrağını kaldırdığında okumalar durur ve her bekleme döngüsünün gerçekten devreye giren sınırlı bir zaman aşımı vardır.
- Sürücünün kendi port/CMD/CI/TD yazmaçlarına `volatile` işaretçiler üzerinden erişilir; `prt_dec()` bir `uint64_t`'yi sabit bir değere böler; dolayısıyla `__udivdi3` libgcc bağımlılığı çekilmez.

Bilinen sınırlamalar:

- **Depoda `ahci_init()` çağıran hiçbir şey yoktur.** `pci.c` derlendiğinden beri doğru bağlanır, ancak çağıran yoktur; dolayısıyla hiç çalıştırılmamıştır.
- Gerçek donanımda veya QEMU'da test edilmedi: projede önyükleme kodu yoktur, dolayısıyla bugüne kadarki doğrulama yalnızca derleme zamanındadır (yapı assert'leri, temiz `-Wall -Wextra` derlemesi).
- **Kesmeler yok: `ahci_issue()` `CI`'yi sınırlı bir döngü (spin) ile yoklar, `configure_port()` `PxIE`'yi 0'da tutar ve `ahci_init()` `GHC.IE`'yi temizler.** Bu bilinçli olarak yoklamalı bırakıldı: bir AHCI kesmesi denetleyicinin PCI kesme hattı artı port başına bir `PxIE` maskesi gerektirir ve varsayılan `make run` makinesi bir IDE diskine bağlanır (`-drive ...,if=ide`), bir AHCI denetleyicisine değil - dolayısıyla yol çalıştırılamazdı. Etkinleştirmek `GHC.IE`'yi kurmayı, yapılandırılmış her portta `PxIE` ayarlamayı, `pci_dev_t.interrupt_line` içinde yakalanan PCI hattını kaydetmeyi ve komut beklemesini ATA'nın kullandığı aynı yoklama yedeğiyle `hlt()`'ten uyandırmayı gerektirir.
- Sıcak takma (hot-plug) yok, ATAPI aygıtı yok, NCQ yok.

## 9. PCI - `Drivers/PCIBus/`

```c
void     pci_init(void);
uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
void     pci_write(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t val);
void     pci_scan(void);
int      pci_get_dev_count(void);
pci_dev_t* pci_get_device(int index);
```

Yapılandırma erişimleri standart adresi `0x80000000 | bus<<16 | dev<<11 | func<<8 | (offset & 0xFC)` oluşturur ve portlar `0xCF8` / `0xCFC` kullanır. `pci_scan()` 256 yolu, 32 aygıtı gezer; üretici, aygıt, sınıf, BAR0'dan BAR5'e kadar okur; ayrıca **yapılandırmadaki `0x3C` üzerinde Kesme Hattı / Kesme İğnesi çiftini** okur - aygıtın firmware tarafından yönlendirildiği IRQ'yu; e1000 ve RTL8139 sürücüleri işleyicilerini kurmak buna ihtiyaç duyar (`> 15` "bağlı değil" anlamına gelir ve bu başlatmaları başarısız yapar).

Otomatik bir prob yardımcısı `pci_dev_init()` (`pci.h`'te bildirilir) sınıf `0x02`'yi (ağ) algılar, komut yazmacında I/O alanını, bellek alanını ve bus mastering'i etkinleştirir, sonra üretici `0x8086`'yı `e1000_init(pdev)` ile, `0x10EC`'i `rtl8139_init(pdev)` ile eşler. Yalnızca dword'ün 16 bitlik komut yarısı geri yazılır; dolayısıyla üstündeki 1-yaz-temizle (write-1-to-clear) durum yazmacı rahatsız edilmez.

Durum:

- **Hatasız derleniyor.** `pci_device_t` / `pci_device_count` ad uyuşmazlıkları gitti; dosya `pci_dev_t` ve `pci_dev_count` kullanıyor.
- `pci_init()` taramadan önce `pci_dev_count`'u 0'a sıfırlar; dolayısıyla tekrarlanan taramalar iki kez saymaz.
- **Çalışma zamanı kanıtı yok.** Hiçbir BAR değeri hiç gözlenmedi; dolayısıyla `e1000_init()` / `rtl8139_init()` içindeki çözümleme gerçek donanıma karşı test edilmemiştir.
- Yalnızca her aygıtın 0. işlevi taranır; çok işlevli aygıtlar numaralandırılmaz.

## 10. Ağ sürücüleri - `Drivers/Network/`

Hâlâ **çalışan bir ağ yığını yoktur**. Üç dosyanın tamamı derlenir ve bağlanır, ancak hiçbiri hiç çalıştırılmamıştır.

### Ethernet katmanı - `Ethernet/ethernet.c`

```c
void ethernet_init(void);
void ethernet_register_device(net_device_t *dev);
void ethernet_send(uint8_t *dest_mac, uint16_t ethertype, void *payload, uint16_t len);
```

`ethernet_register_device()` cihazların tek yönlü bağlı listesini (singly linked list) sürdürür. `ethernet_send()`, `ETH_TYPE_ARP 0x0806` ve `ETH_TYPE_IP4 0x0800` ile 14 baytlık bir Ethernet başlığı oluşturur ve cihazın `send` işlevini çağırır.

Sınırlamalar:

- **Veri (payload) kırpılır (clamp).** `len`, `ETH_MAX_FRAME_SIZE - sizeof(eth_header_t)` (1504 bayt) ile sınırlandırılır; dolayısıyla hatalı bir uzunluk 1518 baytlık yığın tamponunun dışına çıkamaz veya 16 bitlik `frame_len`'i sarar (wrap). Çağırıcının fazla verisi sessizce atılır; çünkü işlev `void` döndürür ve hata bildiremez.
- `NULL` `dest_mac` / `payload` reddedilir.
- Kaynak MAC altı `0xAA` baytına sabitlenmiştir.
- Gönderim her zaman hedef ile eşleşen cihaz değil, cihaz listesinin *başı* üzerinden olur.
- `ethernet_init()` bildirilir ama hiç tanımlanmaz.
- Alım yolu yok, ARP çözümleme yok, IP yok, sağlama toplamları (checksum) yok.

### Intel e1000 - `e1000/e1000.c`

```c
bool_t e1000_init(pci_dev_t *pdev);
void   e1000_send_packet(void *data, uint16_t len);
void   e1000_handle_rx(void);        /* runs from the NIC's IRQ handler */
```

Gönderim (TX) ve alım (RX) tanımlayıcı (descriptor) halkaları tahsis edilir ve `TDBAL`, `TDLEN`, `TDH`, `TDT`, `RDBAL`, `RDLEN`, `RDH`, `RDT`, `RCTL` ve `TCTL` yazmaçları programlanır. `e1000_init()` artık:

- üretici `0x8086` dışında ve cihaz kimliği `0x100E` (82540EM) veya `0x100F` (82545EM) olmayan her şeyi reddeder;
- BAR0'ı okur, bir I/O BAR'ını, `0` BAR'ını ve uygulanmamış `0xFFFFFFFF` BAR'ını reddeder, dört işaret baytını maskeler ve üst yarısı 4 GiB üzerinde olan 64 bitlik bir BAR'ı reddeder (bu hedef yalnızca 32 bit işaretçiye sahiptir);
- **Kesme Hattı 15 üzerinde olan bir cihazı reddeder** (`0xFF` = bağlı değil), çünkü yönlendirilmiş bir IRQ olmadan kesme yolu yoktur;
- her `kmalloc()` sonucunu, her 2048 baytlık alım tamponu dahil, kontrol eder - oradaki bir NULL, NIC'in fiziksel adres 0'a DMA yapmasına neden olur;
- durumunu sıfırlar ve bu hataların herhangi birinde `false` döndürür; dolayısıyla `send` / `receive`, NULL'i indirgeme (dereference) etmek yerine hiçbir şey yapmaz (no-op) olur;
- **kesmeyi en sonunda kurar:** `IMC = 0xFFFFFFFF` bayat nedenleri temizler, `IMS` alım nedenlerinin (`RXSEQ`, `RXDMT0`, `RXO`, `RXT0`, `RXCFG`) maskesini kaldırır, işleyici `PIC_IRQ_BASE + interrupt_line` için kaydedilir ve hat `pic_clear_mask()` ile maskesiz bırakılır.

`e1000_irq_handler()` `ICR`'yi okur (oku-temizle); sıfır bir değer, paylaşılan hattaki bir cihazın kesmeyi kaldırdığını anlamına gelir; dolayısıyla hiçbir şeye dokunmaz ve zincirdeki sonraki işleyici sırasını alır. Alım nedenleri `e1000_handle_rx()`'ı çağırır; bu, her dolu tanımlayıcıyı tek geçişte tüketir (halka boyutuyla sınırlı) ve sonra paketi atar.

Hâlâ eksik olanlar:

- EEPROM veya MAC adresi okuma yok, bağlantı kurulumu yok, alım adresi filtreleri yok.
- `e1000_send_packet()` `TDT = tx_index` yazar, ancak kuyruk yazmacı az önce doldurulan tanımlayıcının *üzerinde* (past) durmalıdır; dolayısıyla NIC az önce kuyruğa alınan paketi hiç göndermez. Test edilmemiştir, dolayısıyla düzeltilmemiştir.
- Alım yolu, tanımlayıcıyı tükettikten sonra paketi yine atar.
- BAR adresi için hiç sayfa tablosu eşlemesi yapılmaz; dolayısıyla bu yalnızca sayfalama kapalıyken veya kimlik eşlemeli (identity-mapped) çalışır.
- ICR neden bitleri 82540EM numaralandırmasından alınmıştır; QEMU'nun paket varışında hangisini kaldırdığı henüz gözlenmemiştir.

### Realtek RTL8139 - `RTL8139/rtl8139.c`

```c
bool_t rtl8139_init(pci_dev_t *pdev);
void   rtl8139_send_packet(void *data, uint16_t len);
void   rtl8139_handle_rx(void);      /* runs from the NIC's IRQ handler */
```

Eski gövde `nic = (rtl8139_t*)0xDEADBEEF;` ve ardından bir indirgeme idi. `rtl8139_init()` artık:

- üretici `0x10EC` / cihaz `0x8139` olmayan her şeyi reddeder;
- BAR0'ı okur ve 0. bitin ayarlı olmasını şart koşar (bir I/O bölgesi), `0`, `0xFFFFFFFF` ve sıfır tabanı reddeder, sonra port tabanını elde etmek için iki işaret baytını maskeler;
- **Kesme Hattı 15 üzerinde olan bir cihazı reddeder**, e1000 ile aynı nedenle;
- herhangi bir başarısızlıkta `nic`'i NULL bırakır ve `rtl8139_send_packet()` / `rtl8139_handle_rx()` NULL olduğunda hemen döner;
- **kesmeyi alım halkası programlandıktan sonra kurar:** bayat `ISR` bitleri temizlenir, `IMR` `RXOK`, `RXERR`, `TXOK` ve `RXOVW`'nin maskesini kaldırır, işleyici `PIC_IRQ_BASE + interrupt_line` için kaydedilir ve hat maskesiz bırakılır.

`rtl8139_irq_handler()` `ISR`'yi okur; sıfır bir değer, paylaşılan hatta başka bir cihazın kesmeye sahip olduğunu ve hiçbir şeyin temizlenmediğini anlamına gelir. Aksi halde ayarlı bitler geri yazılır (yaz-temizle) ve alım nedenleri sonra halkayı **çip boş rapor edene kadar** boşaltır (`RTL_RX_DRAIN_LIMIT` ile sınırlı): bir kesme, ISR biti yalnızca bir kez temizlenirken birkaç kuyruktaki paketi kapsayabilir ve çip, çoktan yazdığı paketler için `RXOK`'ı yeniden kaldırmaz.

Hâlâ eksik olanlar:

- `rx_buffer`, NIC'in 8 KiB'lik alım halkası için istediği 256 baytlık hizalama özniteliği olmayan sıradan bir statik dizidir.
- Alım işleyicisi 4 baytlık başlık sınırlarını doğrular ama paketi hiçbir şeye teslim etmez.
- **Halka taşma (overflow) kurtarması yok:** `RXOVW` maskesiz bırakılır ve onaylanır, ancak sonrasında çipin `CWR` sıfırlaması verilmez; dolayısıyla bir taşma, aygıt yeniden başlatılana kadar alımı durdurur.
- EEPROM MAC okuması yok, gönderim durumu işleme yok.

## 11. PIT - `Drivers/PIT/`

Intel 8253/8254 Programlanabilir Aralık Zamanlayıcısı (Programmable Interval Timer). Projede yoklama yerine kesme kullanan ilk sürücü ve hâlâ diğerlerinin hattını nasıl bağladıklarının referansı: 0. kanalı programlar, `interrupt_register_handler()` üzerinden IRQ0'i (vektör 32) bağlar ve `pic_clear_mask(0)` ile hattın maskesini kaldırır. Klavye, Seri, ATA, Fare ve her iki ağ sürücüsü artık kendi hatlarında aynı deseni izler.

```c
void     pit_init(uint32_t hz);       /* 1000 = 1 kHz, one tick per ms */
void     pit_stop(void);              /* mask IRQ0, the counter freezes */
uint32_t pit_get_ticks(void);         /* ticks since the last pit_init() */
uint32_t pit_get_frequency(void);     /* actual rate: 1193182 / divisor */
uint32_t pit_get_uptime_ms(void);     /* wraps after about 49.7 days */
void     pit_sleep_ms(uint32_t ms);   /* busy-wait driven by the ticks */
```

### Bilinmesi gereken davranış

- **`pit_init()` önce `interrupt_init()` gerektirir**; aksi halde işleyici henüz yüklenmemiş bir IDT'ye kaydedilir.
- **Kesmeler etkin olmalıdır (`sti`) veya sayaç hiç ilerlemez.** Sürücü bilinçli olarak IF'e dokunmaz - bu, çekirdeğin kararı olarak kalır.
- İstenen hız `19 .. 1193182` Hz arasına sıkıştırılır. Varsayılan 1000 Hz'de bölen `1193`'tür; dolayısıyla gerçek hız `1193182 / 1193 = 1000.15` Hz'dir. `pit_get_frequency()` en yakın tam hertz'i (1000) bildirir, ancak milisaniye birikicisi tam bölen üzerinden çalışır; dolayısıyla çalışma süresi eksik 0,15 Hz kadar sapmaz.
- 0. kanal `0x34` komut kelimesiyle yüklenir (0. kanal, düşük/yüksek bayt erişimi, **mod 2 oranı üreteci**, ikilik sayım). `interrupt_dispatch()` EOI gönderir; dolayısıyla işleyici PIC'in kendisine hiç yazmaz.
- `pit_init()` farklı bir hızla yeniden çağrılabilir: eski hızda sayılan önceki tikler önce milisaniye tabanına katılır; dolayısıyla `pit_get_uptime_ms()` doğru kalır.
- `pit_sleep_ms()` IF ayarlıyken `hlt` ile bekler ve temizken döner (spin); çünkü IF temiz `hlt` bir NMI'ye kadar asılı kalır. Bir IRQ işleyicisinden çağrılmamalıdır - beklediği tikler teslim edilemezdi.

### Sınırlamalar

- Tikler IF temizken veya `pit_stop()`'dan sonra tamamen durur; dolayısıyla `cli` bölümleri sırasında duvar saati garantisi yoktur.
- Yalnızca 0. kanal; 1 ve 2. kanallar (PC hoparlörü, DRAM yenilemesi) olduğu gibi bırakılmıştır.
- RTC'ye karşı kalibrasyon yoktur; dolayısıyla kristal hatası olduğu gibi kabul edilir.
- `pit_get_uptime_ms()` `uint32_t` döndürür ve yaklaşık 49,7 gün sonra sarar (wrap).

## 12. Framebuffer - `Drivers/Framebuffer/`

Gömülü 8x8 yazı tipiyle lineer framebuffer grafiği. Sürücü, önyükleyicinin zaten programladığı moda bağlanır (multiboot1 veya multiboot2 lineer framebuffer) ve önyükleyici framebuffer'ı yoksa VGA mod 13h'ye (320x200x8) düşer; dolayısıyla hem GRUB hem QEMU'nun `-kernel` modu altında çalışır.

Üç kaynak dosya: `framebuffer.c` (bağlanma, mod ayrıştırma, çizim), `framebuffer_vga.c` (mod 13h programlama, DAC, vsync ve metin yazısının kaydedilip geri yüklenmesi), `framebuffer_font.c` (128 glifli yazı tipi ve metin çıktısı).

### İşlevler

```c
/* mod edinme */
bool_t framebuffer_init(const fb_info_t *info);          /* önceden programlanmış moda bağlan */
bool_t framebuffer_init_from_multiboot1(const void *mb_info);
bool_t framebuffer_init_from_multiboot2(const void *mb_info);
bool_t framebuffer_init_from_boot(uint32_t magic, const void *boot_info); /* magic yönlendirme */
bool_t framebuffer_init_vga(void);                       /* VGA mod 13h yedeği */
void   framebuffer_close(void);                          /* mod bize aitse metin moduna döner */

/* durum */
bool_t   framebuffer_is_ready(void);
uint32_t framebuffer_get_width(void);
uint32_t framebuffer_get_height(void);
uint32_t framebuffer_get_pitch(void);     /* satır başına bayt */
uint8_t  framebuffer_get_bpp(void);
void    *framebuffer_get_buffer(void);    /* doğrudan piksel erişimi */

/* renk: 0x00RRGGBB, bilgi alanlarındaki bit konumlarına göre paketlenir */
uint32_t framebuffer_rgb(uint8_t r, uint8_t g, uint8_t b);
void framebuffer_set_palette_entry(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
void framebuffer_set_palette(const uint8_t *rgb, uint8_t first, uint16_t count);

/* çizim - hepsi ekrana kırpılır, hazır değilse no-op */
void     framebuffer_clear(uint32_t color);
void     framebuffer_put_pixel(int32_t x, int32_t y, uint32_t color);
uint32_t framebuffer_get_pixel(int32_t x, int32_t y);   /* ekran dışında 0 */
void     framebuffer_fill_rect(int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t color);
void     framebuffer_draw_rect(int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t color);
void     framebuffer_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
void     framebuffer_draw_circle(int32_t cx, int32_t cy, uint32_t radius, uint32_t color);
void     framebuffer_fill_circle(int32_t cx, int32_t cy, uint32_t radius, uint32_t color);
void     framebuffer_blit(int32_t x, int32_t y, uint32_t w, uint32_t h,
                          const void *src, uint32_t src_pitch, uint8_t src_bpp);
void     framebuffer_scroll_up(uint32_t rows, uint32_t fill_color);

void   framebuffer_wait_vsync(void);       /* 0x3DA portundaki bit 3'ü yoklar */
bool_t framebuffer_selftest(void);         /* geometri + piksel gidip gelme kontrolü */

/* metin: 8x8 yazı tipi, ölçek 1..8, indeks = char & 0x7F */
void        framebuffer_set_font(const uint8_t *font);   /* 128 glif x 8 bayt */
const uint8_t *framebuffer_get_font(void);
void        framebuffer_set_text_scale(uint32_t scale);
uint32_t    framebuffer_get_text_scale(void);
void        framebuffer_draw_char(int32_t x, int32_t y, char c, uint32_t fg, uint32_t bg);
void        framebuffer_draw_string(int32_t x, int32_t y, const char *str, uint32_t fg, uint32_t bg);
uint32_t    framebuffer_text_width(const char *str);
```

Arka plan olarak `FB_COLOR_TRANSPARENT` (24. bit açık) verildiğinde `draw_char` / `draw_string` yalnızca glif piksellerini çizer. `FB_MULTIBOOT1_MAGIC` / `FB_MULTIBOOT2_MAGIC`, `framebuffer_init_from_boot()`'un yönlendirdiği önyükleyici değerleridir.

### Bilinmesi gereken davranış

- **Olağan giriş noktası `framebuffer_init_from_boot()`'tur.** Magic'yi kontrol eder, doğru ayrıştırıcıyı seçer ve önyükleyicinin bildirdiği her şeyi doğrular: boyut en fazla 8192, `pitch >= width * bpp / 8`, `bpp` {8, 15, 16, 24, 32} kümesinde, adres sıfır değil ve 4 GiB altında. Metin modunda bir "framebuffer" veya bozuk bir etiket reddedilir ve işlev false döner; ardından `framebuffer_init_vga()` mantıklı yedektir.
- **İndeksli (8 bpp) modlara `framebuffer_rgb()` ile uyumlu 3-3-2 paleti** VGA DAC'ına yazılır. Önyükleyici kendi paletini sunduysa (multiboot1 renk bilgisi bloğu) onun paleti yüklenir.
- **`framebuffer_close()` yalnızca video modu bu sürücüye aitse metin moduna geri döner.** Önyükleyicinin sunduğu framebuffer'a bağlanmak (adres != `0xA0000`) VGA arka arkasındaki sahipliği devreder; dolayısıyla kapatmak önyükleyicinin grafik moduna dokunmaz. `framebuffer_init_vga()`'dan sonra kapatmak ise registerları, 64 girişlik metin paletini ve CRTC'yi BIOS mod 3'e programlar. Mod 13h pikselleri dört görüntü plakasının tamamında tutar; bu da plaka 2'deki metin yazısını yok eder. Bu yüzden `framebuffer_init_vga()` önce o plakanın görüntüsünü alır (8 KiB, yalnızca VGA metin modundayken) ve geri yükleme onu geri yazar - aksi halde her glif dolu bir blok olarak geri gelirdi.
- Multiboot1'de 110. ötelemedeki renk bloğu **altı bayttır** (özet 0.6.96'ya göre `rpos, rsize, gpos, gsize, bpos, bsize` sırası), bir bit alanı değildir - indeksli modlarda aynı baytlar bir palet adresi ve renk sayısıdır. Her iki düzen de paketlenmiş bir birleşim (union) üzerinden ham baytlardan ayrıştırılır.
- Tüm çizim kırpılır: ekran dışı koordinatlar kesilir ya da yok sayılır ve sürücü hazır değilken her işlev no-op'tur; dolayısıyla çağıran tarafın hiç koruma yazmasına gerek yoktur.
- Heap ve `memcpy` bağımlılığı yok: pikseller bayt bayt yazılır ve yazı tipi glifleri koşu uzunluğu (run-length) parçaları halinde çizilir; sürücü herhangi bir serbest (freestanding) görüntüye bağlanır.
- `framebuffer_wait_vsync()` dikey yenileme bayrağını yoklar; kesme yoktur, dolayısıyla meşgul bekledir (busy wait) ve her yerden çağrılabilir.

### Sınırlamalar

- Mod 13h 320x200'dir ve sürücünün düzeltmediği 4:3 en-boy oranına sahiptir; daha yüksek çözünürlükler yalnızca önyükleyiciden gelir.
- Çift tamponlama veya sayfa çevirme yoktur; yırtılma (tearing) yalnızca kareden önce `framebuffer_wait_vsync()` çağrılırsa azalır.
- Yazı tipi sabit 8x8 ve yalnızca ASCII'dir (glif indeksi `char & 0x7F`); `framebuffer_set_font()` aynı düzende bir yedek kabul eder.
- `framebuffer_blit()` kaynak biçimini argüman olarak alır - piksel biçimleri arasında dönüştürme yapmaz.
- Gerçek donanımda test edilmedi; QEMU'da SeaBIOS + GRUB ile doğrulandı (multiboot1 bağlanma, mod 13h yedeği, kapatma/geri dönüş) ve serbest bir host harness ile.

---

[← Sistem Kütüphanesi](05-system-library.md) | [Belge Dizini](README.md) | [Sonraki: Dosya Sistemleri →](07-file-systems.md)
