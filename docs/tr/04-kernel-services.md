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

32-bit adres alanının (4 GiB) tamamını kullanılabilir hale getiren, isteğe bağlı sayfalama (demand paging) ve takılabilir swap arka ucu sunan bir alt sistemdir. Fiziksel RAM'in üstüne çıkmadan, yani ek RAM gerektirmeden büyük sanal tamponlar tahsis edebilirsiniz: her sayfa yalnızca ilk dokunuşta RAM'e bağlanır, RAM dolduğunda ise ikinci şans (second-chance) algoritmasıyla swap alanına taşınır.

### Genel API

```c
int  vmem_init(const vmem_config_t *config);
void vmem_install_fault_handler(void);

int  vmem_swap_set_backend(const vmem_swap_backend_t *backend, uint32_t slots);

void *vmem_alloc(uint32_t size);                       /* WRITE | SWAPPABLE */
void *vmem_alloc_ex(uint32_t size, uint32_t flags);
int  vmem_free(void *ptr);

int  vmem_region_add(uint32_t vaddr, uint32_t size, uint32_t flags);
int  vmem_region_remove(uint32_t vaddr);
int  vmem_region_find(uint32_t vaddr, uint32_t *start, uint32_t *end,
                      uint32_t *flags);

int  vmem_map_page(uint32_t vaddr, uint32_t paddr, uint32_t flags);
int  vmem_unmap_page(uint32_t vaddr);
uint32_t vmem_get_physical(uint32_t vaddr);

void *vmem_map_mmio(uint32_t paddr, uint32_t size, uint32_t flags);
int  vmem_unmap_mmio(void *vaddr, uint32_t size);

void vmem_invlpg(uint32_t vaddr);
void vmem_flush_tlb(void);
void vmem_get_stats(vmem_stats_t *out);
```

`vmem.h` içinde tanımlı bayraklar (Intel SDM, cilt 3):

```c
#define PAGE_PRESENT  0x001u   #define PAGE_PSE     0x080u
#define PAGE_RW       0x002u   #define PAGE_GLOBAL  0x100u
#define PAGE_USER     0x004u   #define PAGE_ACCESSED 0x020u
#define PAGE_PWT      0x008u   #define PAGE_DIRTY   0x040u
#define PAGE_PCD      0x010u
```

Buna ek olarak sanal bellek bölgesi (region) bayrakları:

```c
#define VMEM_REGION_WRITE      0x001u
#define VMEM_REGION_USER       0x002u
#define VMEM_REGION_SWAPPABLE  0x004u
```

### Yaptıkları

`vmem_init()`, `vmem_config_t` ile verilen fiziksel RAM aralığını (`phys_start`-`phys_end`) çerçeve ayırıcısına (frame allocator) kaydeder ve **RAM'in tamamını kimlik eşlemesine (identity map) alır**:

- İlk 4 MiB, 4 KiB'lik sayfa tablosuyla eşlenir (çekirdek, yığın, VGA tamponu buradadır).
- 4 MiB'in üstü, 4 MiB'lik PSE sayfalarıyla eşlenir; böylece kare (frame) ve paylaşım tamponları fiziksel adresleriyle doğrudan erişilebilir kalır.
- Sanal tahsisler için `arena_start`-`arena_end` aralığı ayrılır. `0` verilirse arena, kimlik eşlemeli RAM'in hemen üstüne otomatik yerleştirilir (varsayılan `0xFFFFF000`'e kadar).

Ardından sayfalama açılır ve `install_fault_handler` açıksa 14. vektöre (sayfa hatası) işleyici bağlanır. İşleyici:

1. Hatayı yapan adresi CR2'den okur, bölgeyi bulur.
2. Giriş `VMEM_PTE_SWAP` taşıyorsa swap arka ucundan sayfayı RAM'e geri okur (swap-in).
3. Giriş boşsa sıfırla doldurulmuş yeni bir çerçeve bağlar (demand-zero).
4. Boş çerçeve yoksa ikinci şans algoritmasıyla erişilmiş (accessed) sayfaları atlar, erişilmemiş swappable sayfaları swap'e yazar (swap-out) ve çerçeveyi yeniden kullanır.

Sayfa tabloları istek üzerine oluşturulur ve son kullanıcı kaldırıldığında yıkılır; TLB `vmem_invlpg()` / `vmem_flush_tlb()` ile yönetilir.

### Swap arka ucu sözleşmesi

Sanal bellek katmanı belirli bir disk sürücüsüne bağlı değildir. Swap, aşağıdaki geri çağırma (callback) çiftiyle yapılır:

```c
typedef int (*vmem_swap_read_t)(void *context, uint32_t slot, void *page);
typedef int (*vmem_swap_write_t)(void *context, uint32_t slot, const void *page);

typedef struct {
    vmem_swap_read_t  read;
    vmem_swap_write_t write;
    void *context;
} vmem_swap_backend_t;
```

Kurallar:

- `read`/`write` tam olarak bir 4 KiB sayfa okur/yazar; `0` dönüşü başarıdır.
- **Kesmeler kapalıyken çağrılırlar** (sayfa hatası işleyicisi interrupt gate üzerinden çalışır) ve kendileri sayfa hatası üretmemelidir. Bu yüzden tamponları ve sürücü durumları önceden RAM'de, tercihen swappable olmayan bellekte olmalıdır.
- `vmem_swap_set_backend()` ilk tahliyeden (eviction) önce çağrılmalıdır. `vmem_init()` arka ucu sıfırladığı için backend **init'ten sonra** kurulmalıdır. Slot sayısı 1 GiB'lik üst sınıra kırpılır.

### Entegrasyon örneği

```c
#include <OpenKernel/Kernel/Components/InterruptServices/interrupt.h>
#include <OpenKernel/Kernel/KernelServices/VirtualMem/vmem.h>

static int disk_swap_read(void *ctx, uint32_t slot, void *page);
static int disk_swap_write(void *ctx, uint32_t slot, const void *page);

void kernel_main(void)
{
    vga_init();
    heap_init(_heap_start, 0x100000);
    interrupt_init();                       /* IDT önce gelmeli */

    vmem_config_t cfg = {
        .phys_start = PAGE_ALIGN((uint32_t)(uintptr_t)_kernel_end),
        .phys_end   = 64u * 1024u * 1024u,  /* bootloader'dan alın */
        .arena_start = 0,                    /* otomatik */
        .arena_end   = 0,
        .install_fault_handler = true,
    };
    if (vmem_init(&cfg) != 0) panic("vmem_init");

    vmem_swap_backend_t swap = {
        .read = disk_swap_read,
        .write = disk_swap_write,
        .context = NULL,
    };
    vmem_swap_set_backend(&swap, 4096);      /* 16 MiB swap alanı */

    /* Büyük tampon: RAM'de yalnızca dokunulan sayfalar durur. */
    uint8_t *buf = vmem_alloc(32u * 1024u * 1024u);
    if (buf == NULL) panic("vmem_alloc");
    buf[0] = 1;                              /* ilk sayfa burada bağlanır */

    /* Kesme bağlamında kullanılacak bellek swap edilmemeli. */
    void *irq_buf = vmem_alloc_ex(PAGE_SIZE, VMEM_REGION_WRITE);

    vmem_stats_t st;
    vmem_get_stats(&st);                     /* st.evictions, st.free_frames ... */

    vmem_free(buf);                          /* çerçeveleri ve swap slotlarını bırakır */
}
```

### Yapmadıkları / sınırlamalar

- Arena bir bump (ileri yürüyen) ayırıcıdır: `vmem_free()` fiziksel çerçeveleri ve swap slotlarını geri verir, ancak **sanal adres aralığını yeniden kullanıma açmaz**. Uzun ömürlü, sürekli ayır/serbest döngüsü için arena tükenebilir.
- Üst yarı (higher-half) çekirdek düzeni yoktur; çekirdek kimlik eşlemeli düşük bellekte kalır.
- `VMEM_REGION_USER` biti PTE'lere yazılır ama ring 3 geçişi, TSS veya ayrı adres alanları bu alt sistemin kapsamında değildir. Süreç (process) izolasyonu yoktur.
- Sayfa değiştirme (page replacement) yalnızca second-chance'tır; çalışma kümesi (working set) analizi veya LRU yoktur. Aşırı yük altında thrashing mümkündür.
- Her şey tek CPU içindir; SMP kilidi yoktur, yalnızca kesme bayrağı koruması (`vmem_lock`) vardır.
- `phys_start`/`phys_end` çağıran tarafından verilir; multiboot bellek haritası okunmaz. Bootloader'ınızın bildirdiği kullanılabilir RAM aralığını siz geçirmelisiniz.
- `paging_init()` geriye uyumluluk için korunmuştur: yalnızca ilk 4 MiB'i kimlik eşler ve sayfalamayı açar. Yeni kod `vmem_init()` kullanmalıdır.

### Dürüst değerlendirme

QEMU üzerinde doğrulanan senaryolar: isteğe bağlı sıfır sayfalar, fiziksel MMIO eşleme, 96 çerçeveli küçük RAM'de 160 sayfalık tamponun swap'e taşınıp veri bütünlüğüyle geri okunması, tekrar `vmem_init()` ve eski `paging_init()` uyumluluğu. Test sonunda çerçeve, sayfa tablosu ve swap slotu sızıntısı gözlenmedi. Yine de bu bir çekirdek alt sistemidir: gerçek disk sürücüsü üzerinde swap, eşzamanlı süreçler ve düşük bellek koşulları henüz test edilmedi.

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
#define KRNL_VER         "5.0"
#define KRNL_ARCH        "x86 (32-bit)"
#define KRNL_LAST_COMMIT "06c18fe"
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

    /* 5. virtual memory: all RAM identity-mapped, demand paging on */
    /* vmem_config_t cfg = { ... }; vmem_init(&cfg);                 */
    /* vmem_swap_set_backend(&swap, slots);  // after vmem_init()   */

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
