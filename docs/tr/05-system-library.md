# 05 - Sistem Kütüphanesi

[← Çekirdek Hizmetleri](04-kernel-services.md) | [Belge Dizini](README.md) | [Sonraki: Sürücüler →](06-drivers.md)

---

`SystemLib/`, çekirdeğin her parçasının bağımlı olduğu rutinleri içerir: bellek, dizgeler, port giriş/çıkışı, zaman, görevler, dönüştürme ve dahili işlev ile syscall kayıt tabloları.

## 1. Bellek - `SystemLib/Memory/mem.c`

### İşlevler

```c
void* memset(void* dst, uint8_t val, uint32_t size);
void* memcpy(void* dst, const void* src, uint32_t size);
int   memcmp(const void* a, const void* b, uint32_t size);

void  heap_init(uint32_t start, uint32_t size);
void* kmalloc(uint32_t size);
void  kfree(void* ptr);
```

`memset`, `memcpy` ve `memcmp`, standart C karşılıkları gibi davranır ve ağaç boyunca kullanılır.

### Heap tahsisi

`heap_init()`, dosya-statik üç değer kaydeder: `heap_start`, `heap_end` ve `heap_current = start`.

`kmalloc()` bir **bump allocator** tekniğini kullanır:

1. `0xFFFFFFFC` üzeri boyutları reddeder; bu boyutlar 4 baytlık hizalama yuvarlamasında taşarak sınır kontrolünden geçen küçük bir değere sarılır (wrap).
2. İstenen boyutu yukarı yuvarlayarak 4'ün katına çevirir.
3. Belleğin başını aşacak şekilde sarılacak imleci reddeder.
4. Güncel işaretçiyi döndürür ve `heap_current`'i yuvarlanmış boyut kadar ileri taşır.
5. İstek `heap_end`'e ulaşıp geçecekse `NULL` döndürür.

```c
heap_init(0x100000, 1024 * 1024);   /* 1 MiB heap */

int* a = (int*)kmalloc(4);
int* b = (int*)kmalloc(16);
```

### Kritik sınırlamalar

- **`kfree()` hiçbir şey yapmaz.** Gövdesinin tamamı `(void)ptr;` satırıdır. `kmalloc` ile ayrılan bellek asla geri kazanılmaz. Her tahsis kalıcı olarak heap alanını tüketir.
- Serbest liste yok, blok başlığı yok, birleştirme (coalescing) yok ve yeniden kullanım yok.
- Hizalama yalnızca 4 bayttır. DMA yetenekli tamponlar (örneğin AHCI sürücüsünün ihtiyaç duyduğu tamponlar) daha güçlü bir hizalama garantisi almaz.
- **`heap_init()` depoda hiçbir yerde çağrılmaz.** Çalışmadan önce `heap_start`, `heap_end` ve `heap_current`'in tamamı 0'dır; bu yüzden her `kmalloc()` sınır kontrolünde başarısız olur ve `NULL` döndürür - `0x0`'dan başlayan adresler **vermez**. Bu güvenli bir sonuçtur, ancak bugün hiçbir dinamik tahsisin çalışmaması anlamına gelir: özellikle `e1000_init()` ilk tahsisinde vazgeçer.
- Bağlama betiği, `_heap_start` ve `_heap_end` sembolleriyle 1 MiB'lik bir `.heap` bölgesi ayırır; ancak bunları `heap_init()`'e hiçbir şey otomatik olarak bağlamaz.

## 2. Dizge işlevleri - `SystemLib/Std/std.c`

```c
int  strcmp(const char *s1, const char *s2);
int  strncmp(const char *s1, const char *s2, unsigned int n);
int  strlen(const char *str);
void strcpy(char *dst, const char *src);
void strncpy(char *dst, const char *src, unsigned int n);
```

Standart anlamlar: `strcmp`/`strncmp` negatif, sıfır veya pozitif döndürür; `strlen` sonlandırıcıyı hariç tutarak uzunluğu döndürür.

**Bu projede hiçbir yerde `printf`, `sprintf` veya `snprintf` yoktur.** Hiçbir varargs desteği yoktur. Biçimli çıktı yazdırmak için çıktıyı kendiniz bir tampona biçimlendirmeli ve ardından VGA işlevlerini çağırmalısınız.

Bunun yerine biçimlendirme yardımcı işlevleri VGA sürücüsünde bulunur:

```c
vga_print_hex(0xB8000);   /* prints hex */
vga_print_dec(-42);       /* prints decimal */
vga_print_bin(0x0F);      /* prints binary */
```

Keyfi biçimlendirmeye ihtiyaç duyan alt sistemler (`security.c`, `ahci.c`, `time.c`) her biri kendi yerel `itoa` tarzı yardımcı işlevini taşır; bu, birleştirmek isteyebileceğiniz yinelenen koddur.

## 3. Port giriş/çıkışı - `SystemLib/SystemIO/io.h`

Yalnızca başlıktır, tamamı `static inline` olduğundan dahil etmek ek bir maliyet getirmez.

```c
void outb(uint16_t port, uint8_t val);   uint8_t inb(uint16_t port);
void outw(uint16_t port, uint16_t val);  uint16_t inw(uint16_t port);
void outl(uint16_t port, uint32_t val);  uint32_t inl(uint16_t port);

void insb(uint16_t port, void *addr, uint32_t count);
void outsb(uint16_t port, void *addr, uint32_t count);
void insw(uint16_t port, void *addr, uint32_t count);
void outsw(uint16_t port, void *addr, uint32_t count);

void sys_wait(void);      /* writes to port 0x80, a short ISA delay */
void sys_barrier(void);
void mem_fence(void);     /* mfence */
uint32_t eflags_r(void);  /* pushf / pop, reads RFLAGS */
```

Bu, projedeki en düşük seviyeli arayüzdür. Sürücüler bunun üzerine inşa edilir.

### `SystemIO/qemu_io.h`

Port `0xE9` üzerindeki QEMU hata ayıklama konsolunu kullanarak `qemu_print_dbg(char)`, `qemu_str_print_dbg(const char*)` ve `qemu_log_message(...)` işlevlerini sağlar. Depoda bunu hiçbir şey dahil etmez. Artık `klog_level_t`'yi doğru çözümler; çünkü typedef'e sahip olan `log.h` dosyasını dahil eder.

## 4. Zaman - `SystemLib/TimeMng/time.c`

```c
systime get_time(void);
void    uptime(void);
void    print_time(void);
void    sleep(uint32_t seconds);
```

### Saati okuma

`get_time()`, CMOS portları `0x70` ve `0x71` üzerinden RTC'yi okur:

| Kayıt | Alan |
|---|---|
| `0x00` | saniye |
| `0x02` | dakika |
| `0x04` | saat |
| `0x07` | gün |
| `0x08` | ay |
| `0x09` | yıl |

Değerler BCD olarak gelir ve `bcd_to_bin()` ile dönüştürülür. Sonuç genel `sys_time` değişkeninde saklanır ve yıl iki haneli bir yıl artı 2000 olarak yorumlanır; bu nedenle tarihler yalnızca 2000-2099 yılları için doğrudur.

`print_time()`, `DD/MM/YYYY HH:MM:SS` biçimini 20 baytlık bir tampona biçimlendirir ve yazdırır.

### Bekleme

`sleep(seconds)` beklemeyi PIT sürücüsüne devreder:

```c
pit_sleep_ms(seconds * 1000);   /* seconds is clamped so the multiply cannot wrap */
```

Bilmeniz gereken sonuçlar:

- Yalnızca `pit_init()` çalışmış **ve** kesme bayrağı ayarlıysa bekler. Aksi halde `pit_sleep_ms()` hemen döner; bu yüzden başlatılmamış bir sistem askıda kalma yerine işlevsiz (no-op) bir gecikme alır.
- Bekleme IRQ0 milisaniye sayıcısı tarafından sürülür; bu nedenle bir tik hassasiyetindedir (varsayılan 1 kHz'de 1 ms) ve CPU hızına veya ana makine yüküne bağlı değildir. Yine de CPU yakar - tikler arasında `hlt` olan bir busy-wait'tir, askıya alınmış bir görev değildir.
- Bir IRQ işleyicisinin içinden çağrılamaz: beklediği tikler, o işleyici çalışırken teslim edilemez.
- Daha önce `seconds * 50000000ULL` yinelemeli bir `nop()` döngüsüydü; bu, tek bir CPU hızına bağlı bir tahmindi.
- `uptime()` yine de PIT yerine CMOS üzerinden RTC'yi okur; bu yüzden bir çalışma süresi sayacı değil, duvar saati okumasıdır.

## 5. Görev yönetimi - `SystemLib/TaskMng/task.c`

```c
typedef struct task {
    uint32_t id, esp, ebp, eip;
    void (*entry)(void);
    void *stack;
    uint32_t stack_size;
    task_state_t state;     /* TASK_READY, TASK_RUNNING, TASK_BLOCKED, TASK_TERMINATED */
    struct task *next;
} task_t;

void taskmng_init(void);
task_t* task_create(void (*entry)(void), uint32_t stack_size);
void    task_destroy(task_t *task);
void    task_add(task_t *task);
void    task_remove(task_t *task);
void    schedule(void);
void    yield(void);
task_t* task_current(void);
void    task_sleep(uint32_t ms);
void    task_block(void);
void    task_unblock(task_t *task);
```

### Dürüst değerlendirme: bu bir zamanlayıcı değildir

Veri yapıları mevcut; mekanizma mevcut değil.

- **Bağlam değişimi (context switch) yoktur.** `esp`, `ebp` ve `eip` alanları asla kaydedilmez veya geri yüklenmez. Depodaki tek assembler kodu, kesme stub'ları oluşturan `isr.asm` dosyasıdır; x86 üzerinde bir bağlam değişiminin kendi assembler koduna ihtiyacı vardır ve bu kod mevcut değildir.
- `task_create()`, `eip = (uint32_t)entry` ve `esp = 0` değerlerini ayarlar. `kmalloc()` ile ayrılan `stack` tamponu asla bir yığın olarak kullanılmaz.
- `schedule()`, dairesel bir liste üzerinde basitçe `current = current->next;` yapar. `state`'i kontrol etmez; bu yüzden engellenmiş ve sonlandırılmış görevler de seçilir.
- `yield()`, `schedule()`'ı çağırır ve ardından `current->entry()`'yi doğrudan baştan çağırır. Bu, görev işlevini kaldığı yerden sürdürmek yerine *yeniden başlatır*; çoklu görev yerine iç içe ve özyinelemeli yürütme üretir.
- `task_block()`, `current`'i ileri taşır ama bir sonraki görevi asla çalıştırmaz.
- `task_sleep(uint32_t ms)` artık `pit_sleep_ms(ms)` çağırır. Daha önce `sleep(ms)` çağırıyordu; oysa `sleep()` **saniye** alır ve bu 1000 kat yanlıştı.
- `task_create()` artık `NULL` bir giriş noktasını veya sıfır `stack_size` değerini reddeder ve yığın tahsisi başarısız olduğunda görevi serbest bırakıp `NULL` döndürür.
- `task_add()`, listede zaten bulunan bir görevi eklemeyi reddeder ve yürüyüşünü sınırlar; böylece bozuk bir bağlantı çekirdeği askıya alamaz. `task_remove()` daha önce ilk geçişte `prev`'i `cur` ile karşılaştırıyordu; burada her zaman eşit olduklarından başı silmek tüm listeyi siliyordu; test artık "bu liste tek öğeli bir daire mi" şeklindedir.
- `task_destroy()`, işlevsiz olan `kfree()`'yi çağırır; bu yüzden yığınlar sızar.
- `task_list` ve `current` `static` olarak bildirilmiştir; `SecurityChecker`, `task.c`'ye eklenen `task_list_head()` erişim işlevi üzerinden listeye ulaşır; böylece genel değişkenler gizli kalır.
- Depoda `taskmng_init()`, `task_create()`, `yield()` veya `schedule()`'ı çağıran hiçbir şey yoktur.

Bu modülü çalışan çoklu görev olarak değil, **üzerine inşa edebileceğiniz bir iskelet** olarak ele alın. Gerçek önceliklendirme bir IDT, bir zamanlayıcı IRQ'su ve assembler bağlam değişimi kodu gerektirir. IDT ve IRQ teslimi artık mevcuttur (`InterruptServices`), ancak bağlam değişimi assembler kodu mevcut değildir.

## 6. Tür dönüştürme - `SystemLib/TypeConversion/type_conv.c`

```c
char*    itoa(int val, char* str);                 /* decimal */
char*    htoa(uint32_t val, char* str);            /* hexadecimal */
char*    htoan(uint32_t val, char* str, int width);/* hexadecimal, zero padded to width */
int      atoi(const char* str);                    /* decimal */
uint8_t  bcd_to_bin(uint8_t val);                  /* RTC BCD decoding */
```

`htoan()`, sabit genişlikli kayıt dökümleri için kullanışlıdır:

```c
char buf[9];
htoan(0x000A8000, buf, 8);   /* "000a8000" */
```

`bcd_to_bin()`, `((v / 16) * 10) + (v & 0x0F)` hesaplar ve doğrudur; `time.c` buna bağımlıdır.

**Temiz derlenir.** Bu belgenin eski sürümlerinde bildirilen iki sözdizimi hatası (`itoa()` içinde başıboş bir `str1[1] = '\0';` ve `htoan()` içinde eksik bir noktalı virgül) artık kaynak kodda bulunmuyor.

## 7. Çekirdek işlev kayıt tablosu - `SystemLib/KernelFunc/kernel_func.c`

Bir kabuk komut kayıt tablosuna ruhu benzeyen, adlandırılmış bir işlev tablosu.

```c
typedef void (*kfunc_t)(void);

void register_func(const char *name, kfunc_t func);
void exec_func(const char *func_name);
void exec_command(const char *command_name);   /* alias for exec_func */
```

Tablo en fazla 256 kayıt (`MAX_FUNC 256`) tutar ve `strcmp` ile doğrusal olarak aranır.

### Varsayılan olarak kilitli

`kernel_func_enabled` adlı genel bayrak `false` olarak başlar. Ayarlanmadıkça hem `register_func()` hem `exec_func()` hemen döner:

```c
KernelFunc_O();                       /* enable the registry */
register_func("clear", vga_clear_screen_wrapper);
exec_command("clear");               /* runs it */

KernelFunc_C();                       /* disable again */
```

`KernelFunc_O()` kayıt tablosunu açar, `KernelFunc_C()` kapatır ve `kernel_func_init()` tabloyu temizler.

Dikkat edilecek noktalar:

- `kernel_func_init()`, `kernel_func_enabled` değerini **sıfırlamaz**; bu yüzden açık/kapalı durumu yeniden başlatmada hayatta kalır.
- Kayıtlı işlevler `void` alır. Argüman geçirmenin ve geri dönüş değeri almanın bir yolu yoktur.
- Depoda herhangi bir işlevi kaydeden veya çalıştıran hiçbir şey yoktur; bu yüzden kayıt tablosu varsayılan olarak kullanılmaz.

## 8. Dahili syscall dağıtıcı - `SystemLib/SysCalls/`

```c
int syscall(int id, void* arg);
int syscall_handler(int id, void* arg);
```

### Gerçekte nedir

43 syscall numarasına (0'dan 42'ye) sahip bir `switch` ifadesi; her numarayı doğrudan bir C işlevi çağrısına eşler. Bu **dahili bir dağıtım tablosu, gerçek bir syscall mekanizması değildir**:

- `int 0x80` kapısı yok ve `sysenter` yok - IDT'nin kendisi artık mevcuttur (`InterruptServices`), ancak içine hiçbir syscall vektörü kurulmamıştır.
- Ayrıcalık seviyesi geçişi yok; her şey çekirdek modunda çalışır.
- `syscall()`, `syscall_handler()`'e düz bir işlev çağrısıdır.

Yine de bir değeri vardır: `KernelAPI/`'ye tek bir dolaylı geçiş noktası verir; böylece genel API'yi değiştirmeden syscall numaraları yeniden numaralandırılabilir.

### Argüman geçirme

İki kural bir arada var olur:

1. **`struct` argümanları** - çok değerli çağrılar, `SystemLib/SysCalls/Structs/structs.h` içinde tanımlanan bir sarmalayıcı `struct` işaretçisi geçirir; örneğin `vga_set_cursor_t`, `fat16_read_file_t`, `ofs_read_file_t`, `memset_t`, `strcmp_t`. Bunlar doğru şekilde işlenir.
2. **Skaler argümanlar** - işleyici işaretçiyi kendisi dönüştürür; örneğin `vga_set_bg_color((uint8_t)arg)` ve `kmalloc((uint32_t)arg)`; o sırada `KernelAPI` sarmalayıcıları değerin *adresini* geçirir (`syscall(4, &color)`). İki tarafın da doğru olması mümkün değildir; yazıldığı gibi bu çağrılar kırpılmış adresler üzerinde çalışır.

### Dönüş değerleri

Neredeyse her durum, altta yatan işlev ne döndürürse döndürsün `return 0;` ile biter. Sonuç olarak:

- `SYSCALL_KMALLOC`, `kmalloc()`'ı çağırır ve işaretçiyi atar, 0 döndürür.
- `SYSCALL_STRLEN`, `SYSCALL_STRCMP`, `SYSCALL_MEMCMP` ve `SYSCALL_GET_CHR` sonuçlarının tamamını atar; bu yüzden çağıran asla cevabı öğrenemez.

`struct` tabanlı çağrılar bu arayüzün güvenilir yarısıdır.

### Derleme durumu

`syscall.c` temiz derlenir. Daha önce başarısız oluyordu; çünkü `syscall_extern.h`, typedef'ini veren başlığı dahil etmeden `klog_level_t` kullanıyordu; `syscall_extern.h` artık typedef'e sahip olan `log.h` dosyasını dahil ediyor.

## 9. Modül üst veri kalıbı

Başlıkların çoğu şöyle bir blokla biter:

```c
#define MEM_MODULE_NAME     "Memory Library"
#define MEM_MODULE_DESC     "Memory Library for OpenKernel"
#define MEM_MODULE_VER      "1.0"
#define MEM_MODULE_AUTHOR   "OpenSoftware-World"
#define MEM_MODULE_FILE_NAME "mem.lib"
#define MEM_MODULE_KRNL_VER "2.0"
```

Bu makrolar koda gömülü belgelerdir. Hiçbir mantık bunları okumaz. Bazılarının geçerli 4.0'dan daha eski bir çekirdek sürümü bildirdiğini unutmayın; bu, işlevsel bir sorundan çok bilgi kaymasıdır.

---

[← Çekirdek Hizmetleri](04-kernel-services.md) | [Belge Dizini](README.md) | [Sonraki: Sürücüler →](06-drivers.md)
