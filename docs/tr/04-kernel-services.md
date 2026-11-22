# 04 - Çekirdek Hizmetleri

[← Mimari](03-architecture.md) | [Belge Dizini](README.md) | [Sonraki: Sistem Kütüphanesi →](05-system-library.md)

---

`Kernel/` çekirdek hizmetleri içerir: hata işleme, günlükleme, güç kontrolü, sayfalama ve çalışma zamanı öz denetimleri (self-check). Bu belge her modülünü, genel (public) işlevlerini ve onları çağırmadan önce ne yapmanız gerektiğini kapsar.

## 1. Hata işleme - `Kernel/ErrorHandler/panic.c`

```c
void kernel_panic(const char *msg, const char *error_code);
```

Hata bandını yazdırdıktan sonra sistemi durdurur.

Yaptıkları, sırayla:

1. `vga_clear_screen("F")` ile ekranı temizler.
2. `vga_cursor_mode("D", 0, 0)` ile metin imleci gizler.
3. Beyaz zemin üzerine kırmızı metin ayarlar.
4. `== OpenKernel PANIC ==` yazar, ardından sizin `msg` değerinizi, ardından `Error Details:` ve `error_code` ile `Error code:` yazar.
5. Sonsuz bir `hlt` döngüsüne girer.

```c
if (something_failed) {
    kernel_panic("Page table allocation failed", "E_MEM_01");
}
```

Gereksinimler ve sınırlamalar:

- `vga_init()` çağrılmış olmalıdır; aksi halde mesaj hiçbir zaman görüntülenmez.
- Kayıt dökümü, yığın izi (stack trace) veya çağıran adresi yoktur. Tam olarak iki dize alırsınız.
- Asla geri dönmez. Ardından temizlik kodu koymayın.

## 2. Günlükleme - `Kernel/KernelServices/LogService/log.c`

```c
void log_message(klog_level_t level, const char *message);
```

VGA konsoluna önekli, renk kodlamalı bir mesaj yazar.

| Düzey | Önek | Renk |
|---|---|---|
| `LOG_SUCCESS` | `[SUCCESS]: ` | Yeşil |
| `LOG_ERROR` | `[ERROR]: ` | Kırmızı |
| `LOG_WARNING` | `[WARNING]: ` | Açık kahverengi |
| `LOG_INFO` | `[INFO]: ` | Açık mavi |
| `LOG_UNKNOWN` | `[UNKNOWN]: ` | Açık gri |

```c
log_message(LOG_INFO, "Keyboard driver initialised");
log_message(LOG_ERROR, "Disk read failed");
```

Gereksinimler ve sınırlamalar:

- `vga_init()` çağrılmış olmalıdır.
- **Temiz derlenir.** Eski hata - enum'ı `} klog_level_t;` ile kapatması ve bunun bir tip takası (type alias) yerine genel bir *değişken* bildirmesi - düzeltildi: `log.h` artık `typedef enum kernel_log_level klog_level_t;` bildirimine sahiptir, `structs.h` ve `syscall_extern.h` enum'ı yeniden bildirmek yerine `log.h` dosyasını içerir ve `log.c`, `syscall.c` ile `security.c` dosyalarının tümü yeniden derlenir. Bkz. [09 - Proje Durumu](09-project-status.md).
- `SystemLib/SystemIO/qemu_io.h` tipi, aynı `log.h` include'u üzerinden çözümler.

## 3. Sistem yönetimi - `Kernel/KernelServices/SystemManagement/sysmng.c`

```c
void sys_next_status(const char *mode, uint32_t seconds);
void exit_program(const char *mode, const char *message);
```

### `sys_next_status`

Güç ve sıfırlama kontrolü. Mod, yalnızca ilk karaktere değil tüm dizeye göre eşleşir.

| Mod | Davranış |
|---|---|
| `"shutdown"` | `seconds` kadar uyur, ardından I/O portu `0x604`'e `0x00` yazar (QEMU/ICH9 ACPI güç düğmesi), port `0x80` üzerinden bekler, ardından port `0x605`'e `0x20` yazar. |
| `"reboot"` | Kesmeleri devre dışı bırakır, 8042 klavye denetleyicisi giriş tamponunun temizlenmesini bekler (port `0x64`, bit 1), ardından bir CPU sıfırlaması tetiklemek için port `0x64`'e `0xFE` yazar. |

```c
sys_next_status("shutdown", 3);   /* power off after 3 seconds */
sys_next_status("reboot", 0);     /* restart immediately */
```

Notlar:

- Kapatma yolu QEMU ve benzeri PC firmware'ini hedefler. Gerçek donanımda ACPI portları farklıdır ve bu işlem hiçbir şey yapmayabilir.
- `seconds` değeri, `pit_sleep_ms()`'e devreden `sleep()` fonksiyonuna geçirilir (bkz. [05 - Sistem Kütüphanesi](05-system-library.md)). IRQ0 milisaniye sayacında bekler; bu nedenle bir tik kesinliğindedir, ancak bir şeyi askıya almak yerine meşgul bekler (busy-wait) yapar - ve PIT başlatılmamışsa veya kesmeler kapalıysa hemen geri döner.

### `exit_program`

```c
void exit_program(const char *mode, const char *message);
```

İki mod:

- `"D"` - ekstra hiçbir şey yazmaz, ardından durur.
- `"C"` - `message` yazar, ardından durur.

İkisi de sonsuz bir `hlt` döngüsüyle biter ve asla geri dönmez.

## 4. Sanal bellek - `Kernel/KernelServices/VirtualMem/vmem.c`

```c
void paging_init(void);
void load_page_dir(uint32_t *pd);
void enable_paging(void);
```

`vmem.h` içinde tanımlı bayraklar:

```c
#define PAGE_PRESENT 0x1
#define PAGE_RW      0x2
#define PAGE_USER    0x4
```

### Yaptıkları

`paging_init()`, hem `__attribute__((aligned(4096)))` ile bildirilen statik dizi olan tek bir sayfa dizini hem de bir sayfa tablosu oluşturur. **İlk 4 MiB'yi kimlik eşlemesine (identity map) alır** - sanal adres N, fiziksel adres N'ye eşlenir - ardından:

1. Sayfa dizini girişini (giriş 0) ilk sayfa tablosuna yönlendirir.
2. Dizini `load_page_dir()` ile CR3'e yükler.
3. `enable_paging()` ile CR0'un 31. bitini (sayfalama etkinleştirme) ayarlar.

### Yapmadıkları

- 1024 olası sayfa dizini girişinden yalnızca 1'ini eşler; dolayısıyla adres alanının yalnızca 4 MiB'si geçerlidir. Bundan sonraki erişimler sayfa hatası (page fault) verir.
- `PAGE_USER` hiçbir girişe uygulanmaz; kullanıcı/yönetici ayrımı yoktur.
- Henüz bir sayfa hatası (page fault) işleyicisi yoktur. Artık bir IDT vardır (6. bölüm); bu nedenle `interrupt_register_handler(14, ...)` ile biri eklenebilir; bu olana kadar bir hata, varsayılan dağıtım yolu üzerinden çekirdeği paniğe sokar.
- Dinamik tahsis için eşleme/kaldırma (map/unmap) işlevi yoktur, üst yarı (higher-half) düzeni yoktur ve TLB yönetimi yoktur.
- **Depoda `paging_init()`'i çağıran hiçbir şey yoktur.** Sayfalama etkinleştirmek tamamen giriş noktanıza bağlıdır ve bunu yalnızca bir hata işleyiciniz olduğunda ya da tüm erişimlerinizin eşlenmiş aralıkta olduğundan eminseniz yapmalısınız.

## 5. Güvenlik denetleyicisi - `Kernel/KernelServices/SecurityChecker/security.c`

Dahili olarak PSC (OpenKernel Security Checker) olarak adlandırılan bir öz denetim (self-test) çerçevesidir. 602 satır; projedeki en büyük dosya.

### Tipler

```c
typedef enum { SEC_OK, SEC_WARN, SEC_CRITICAL, SEC_PANIC } securchk_severity_t;

typedef struct {
    int total_checks, passed, warnings, critical;
    int tasks_validated, stack_overflows, heap_corruptions;
    int syscall_violations, memory_leaks;
} securchk_result_t;

typedef struct {
    securchk_severity_t severity;
    const char* module;
    const char* message;
    uint32_t    detail;
} securchk_issue_t;          /* up to SECURCHK_MAX_ISSUES = 32 entries */
```

### Genel (public) işlevler

| İşlev | Amaç |
|---|---|
| `securchk_init(void)` | Sorun günlüğünü başlatır. |
| `securchk_run(securchk_result_t*)` | Otomatik başlatır, sayaçları ve sorun günlüğünü sıfırlar, bir bandı yazdırır, tüm yedi denetimi çalıştırır, bir rapor yazdırır. |
| `securchk_check_task_integrity(...)` | Görev listesinde dolaşır (sınırlıdır; başa döndüğünde durur) ve `entry`, `stack`, durum aralığı, id ile sıfır olmayan bir `esp`/`ebp` değerinin görev stack'inin içinde olduğunu doğrular. |
| `securchk_check_stack_overflow(...)` | Stack'ler için aynı aralık doğrulaması; o da sınırlıdır. |
| `securchk_check_heap_integrity(...)` | Bir sonraki heap bloğunun koruma (guard) magic değerini doğrular, `kmalloc` ile alınmış bir blokta 1 GiB adres aralığı tutarlılık denetimi yapar, serbest bırakılmış bir bloğun yeniden talep edilebilirliğini test eder, ardından serbest bırakır. |
| `securchk_check_syscall_bounds(...)` | `syscall(-1, NULL)` ve `syscall(SYSCALL_PRINT_TIME + 1, NULL)` çağrılarının her ikisinin de `-1` döndürdüğünü doğrular (üst sınır, gerçek syscall tablosuyla senkron tutulur; şu anda 43). |
| `securchk_check_memory_safety(...)` | `memset`, `memcpy`, `memcmp` işlevlerini öz denetimle test eder (uzunluğu sıfır durumları dahil) ve stack hizalaması hakkında uyarı verir. |
| `securchk_check_driver_state(...)` | VGA metin tamponunu bir hücre yazma/geri okuma işlemiyle yoklar (özgün hücreyi kaydedip geri yükler), ardından OK raporlar. |
| `securchk_check_kernel_structures(...)` | Statik assert'ler ve bağlayıcı sembolü tutarlılık denetimi: `PAGE_SIZE == 4096`, işaretçi/tam sayı genişlikleri, `__ro_start <= __bss_start <= __stack_start <= __heap_start <= __kernel_end`. |
| `securchk_print_result(...)` | Sayaçları bir rapor biçiminde düzenler. |
| `securchk_log_issue(...)` | Tek bir sorunu önem derecesi, modülü, mesajı ve ayrıntısıyla sınırlı bir halkaya (ring) kaydeder (dolu olduğunda en eskisini atar). |
| `securchk_get_issue(i)` / `securchk_get_issue_count(void)` | Kayıtlı sorunlara salt okunur erişim. |
| `securchk_get_overall_status(...)` | Sayaçlardan türetilen genel bir hüküm döndürür. |
| `securchk_stack_canary_set/check(...)` | Bir stack alanının ilk ve son kelimesine kanarya değerini (canary) `0xDEADBEEF` yerleştirir veya doğrular. |

### Dürüst değerlendirme

- Bu bir **tanı raporu üretecidir, koruma mekanizması değildir.** Durumu gözlemler; hiçbir şeyi dayatmaz.
- Stack kanarya yardımcı işlevleri depoda hiçbir yerde çağrılmaz.
- Artık temiz derlenir (`-Wall -Wextra`) ve statik `task_list`/`current` sembollerine bağlı değildir: görev listesinde dolaşma, `SystemLib/TaskMng/task.c` dosyasına eklenen `task_list_head()` erişim işlevi üzerinden yapılır.
- syscall üst sınırı `SYSCALL_PRINT_TIME`'dan türetilmiştir; bu nedenle bir kez daha gerçek tablodan sessizce sapamaz.
- Heap denetimi, yalnızca "`kmalloc` NULL olmayan bir değer mi döndürdü" sorusuna bakmak yerine koruma (guard) magic değerini, blok aralıklarını ve serbest bırakma/yeniden talep davranışını doğrular.
- Sürücü durumu denetimi artık ekranı silmez; bir VGA hücresine dokunur ve onu geri yükler.
- `securchk_init()` veya `securchk_run()` çağıran hiçbir şey yoktur ve modül çalışma zamanında test edilmemiştir (projede önyükleme kodu yoktur).

## 6. Kesme hizmeti - `Kernel/Components/InterruptServices/`

Projedeki ilk kesme işleme kodu: iki C modülü ve NASM giriş stub'ları; tümü Makefile'da listelenmiştir (`ASM_SRC` / `C_SRC`).

| Dosya | Amaç |
|---|---|
| `interrupt.c` / `interrupt.h` | IDT oluşturma, işleyici kayıt listesi ve C dağıtım yolu |
| `pic.c` / `pic.h` | 8259A başlatma (vektör 32-47'ye yeniden eşleme), maskeleme ve EOI |
| `isr.asm` | 32 istisna ve 16 IRQ vektörü için giriş stub'ları (`%rep` ile üretilir) artı paylaşılan `isr_common` kuyruğu |

### Genel (public) işlevler

| İşlev | Amaç |
|---|---|
| `interrupt_init(void)` | Kapıları 0-47 kurar, `pic_init()` çalıştırır, işleyici kayıt listesini sıfırlar ve IDTR'yi `lidt` ile yükler. Ardından IRQ hatları maskelemede kalır. Bir sürücü işleyici kaydetmeden önce çalıştırılmalıdır. |
| `interrupt_register_handler(vector, handler)` | Bir C işleyicisini bir istisna (0-31) veya IRQ vektörüne (32-47) ekler. Vektörler, işleyicilerden oluşan bir *liste* tutar (sabit 32 düğümlü bir havuzdan, kesme bağlamında tahsis yoktur); böylece bir PCI IRQ hattını paylaşan tüm cihazlar dinleyebilir; aynı işlevi iki kez kaydetmek gereksiz bir işlemdir (no-op). |
| `interrupt_dispatch(registers_t*)` | `isr.asm` tarafından çağrılır. İstisnalar işleyicilerini çalıştırır veya `kernel_panic()` çağırır; IRQ'larda kayıtlı her işleyici kayıt sırasıyla çalışır, ardından her iki durumda da EOI gönderilir. |
| `pic_init(void)` | ICW1-ICW4 sırası: IRQ0-15 vektör 32-47'ye eşlenir, ardından tüm hatlar maskelemeye alınır. |
| `pic_set_mask(irq)` / `pic_clear_mask(irq)` | Bir hattı (0-15) maskelemek veya maskeyi kaldırmak. |
| `pic_send_eoi(irq)` | Kesmeyi onaylar; IRQ8-15 için kaskad (cascade) hattı dahil. |

### Çerçeve (frame) sözleşmesi

`interrupt.h` içindeki `registers_t`, `isr.asm` içindeki push işlemleriyle tam olarak eşleşmelidir; iki statik assert, `sizeof(registers_t) == 64` sabitini ve `eip`'in 44. ofsetini sabitler. Stub'lar çerçeveyi normalleştirir; böylece CPU hata kodu taşıyan istisnalar (8, 10-14, 17), taşımayanlarla aynı şekle sahip olur.

### Yapmadıkları

- **Kendi GDT'si yoktur.** Kapılar, multiboot yükleyicisinin CS'de ne bıraktıysa o olan `0x08` seçicisini kullanır; depoda GDT kodu yoktur.
- **Varsayılan olarak hiçbir şey işleyici kaydetmez.** İşlenmeyen bir istisna, vektör mnemonic'i ile panik yapar; işleyicisi olmayan bir IRQ yalnızca onaylanır (denetleyicinin çalışmaya devam etmesi için EOI yine de gönderilir). Kayıt olan sürücüler: PIT (IRQ0), Klavye (IRQ1), Seri port (IRQ4), ATA (IRQ14), Fare (IRQ12), e1000 / RTL8139 (kendi PCI hattı).
- **Tüm IRQ hatları başlangıçta maskeleme durumundadır** - bir sürücü, istediği hat için `pic_clear_mask()` çağırmalıdır.
- İşleyici kaydını *kaldırma* yoktur: bir kez bir vektörün listesine eklendikten sonra bir işleyici orada kalır (hattı maskelemek, bir sürücünün onu durdurma yoludur; `pit_stop()`, `kbd_close()` ve `mouse_close()` yaptığı da budur).
- APIC desteği yoktur, vektör başına öncelik yoktur, DPL3 (kullanıcı) kapıları yoktur ve `int 0x80` syscall kapısı yoktur.
- **Depoda henüz `interrupt_init()`'i çağıran bir şey yoktur** - projede hâlâ önyükleme kodu yoktur.

### Test etme

QEMU (`-kernel`, multiboot) altında denenmiştir: `sidt` ile IDT sınırı/tabanı, yazılım `int $0x2F` ve `int3`, hata koduyla gerçek bir `#GP`, `pic_clear_mask(0)` + `sti()` sonrasında vektör 32'de gerçek bir zamanlayıcı IRQ0 ve `kernel_panic()`'e ulaşan işlenmeyen bir istisna. Test aracı (harness) depoun dışındadır; depo içinde test paketi yoktur.

## 7. Çekirdek bilgisi - `Kernel/openkernel_information.h`

```c
#define KRNL_NAME        "OpenKernel"
#define KRNL_VER         "4.0"
#define KRNL_ARCH        "x86 (32-bit)"
#define KRNL_LAST_COMMIT "960224c"
```

Yalnızca bilgilendirme amaçlıdır. Depoda onu hiçbir dosya içermez ve `KRNL_LAST_COMMIT`, güncel HEAD'e göre eskimiştir.

## 8. Tipik başlatma sırası

Bu hizmetlerin birbirine bağımlılıkları vardır. `kernel_main()`'ınızdan güvenli bir sıra:

```c
void kernel_main(void)
{
    vga_init();                     /* 1. screen - everything below may print */

    heap_init(_heap_start, 0x100000); /* 2. heap - needed by kmalloc users   */

    interrupt_init();               /* 3. IDT + PIC, IRQs start masked       */

    kbd_init();                     /* 4. input                              */

    /* 5. optional: paging only once you have a fault handler */
    /* paging_init();               */

    /* 6. optional: diagnostics */
    /* securchk_result_t r; securchk_run(&r); */
}
```

Genel ilkeler:

- Yazdırmak istediğiniz her şey için önce `vga_init()` gerekir.
- Tahsis yapan her şey için önce `heap_init()` gerekir.
- İşleyici kaydeden veya bir IRQ maskesini kaldıran her şey için önce `interrupt_init()` gerekir; ardından bir sürücü kendi hattı için `pic_clear_mask()` çağırır.
- `log_message()` kullanılabilir: `log.h` artık `klog_level_t` typedef'ine sahiptir (`structs.h` içindeki yinelenen enum, `log.h` dosyasını içmek tercih edilerek kaldırıldı).

---

[← Mimari](03-architecture.md) | [Belge Dizini](README.md) | [Sonraki: Sistem Kütüphanesi →](05-system-library.md)
