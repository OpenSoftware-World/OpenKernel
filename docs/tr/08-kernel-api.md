# 08 - Çekirdek API

[← Dosya Sistemleri](07-file-systems.md) | [Belge Dizini](README.md) | [Sonraki: Proje Durumu →](09-project-status.md)

---

`KernelAPI/`, kendi işletim sistemi kodunuzdan çağırmanız gereken katmandır. Ham syscall numaralarını adlandırılmış C işlevlerinin arkasına sarmalar; böylece OpenKernel iç yapısı projenizi bozmadan değişebilir.

## 1. Tasarım amacı

Şunu yazmak yerine:

```c
syscall(4, &color);                    /* what does 4 mean again? */
```

şunu yazarsınız:

```c
openkernel_universal_api_vga_set_bg_color(0x02);
```

API katmanı size şunları verir:

- Sihirli sayılar yerine **adlandırılmış, kendini belgeleyen işlevler**.
- **Bir kararlık sınırı.** Başlık yorumu, API'nin OpenKernel 3.0 ve sonrasıyla uyumlu olacak şekilde tasarlandığını belirtir; böylece çağrılarınızı yeniden yazmadan çekirdeği yükseltebilirsiniz.
- **Garanti edilen ve isteğe bağlı özellikler arasındaki ayrım**, hemen ardından açıklanmaktadır.

## 2. Evrensel / evrensel olmayan ayrımı

### Evrensel API - `KernelAPI/OpenKernelUniversalAPI/`

Her OpenKernel tabanlı işletim sisteminin güvenebileceği işlevler. Yalnızca her zaman bulunan VGA sürücüsüne ve temel CPU desteğine ihtiyaç duyarlar.

```c
void openkernel_universal_api_vga_set_bg_color(uint8_t color);
void openkernel_universal_api_vga_set_text_color(uint8_t color);
void openkernel_universal_api_vga_newline(void);
void openkernel_universal_api_vga_set_cursor(uint8_t x, uint8_t y);
void openkernel_universal_api_ptchar(char c);
void openkernel_universal_api_vga_print_scr_nw(const char *str);
void openkernel_universal_api_vga_set_color_scheme(uint8_t bg_color, uint8_t text_color);
char openkernel_universal_api_get_char(void);
void openkernel_universal_api_sleep(uint32_t seconds);
```

Dokuz işlev, 4'ten 12'ye kadar syscall numaraları.

### Evrensel olmayan API - `KernelAPI/nonuniversal_api_defs.h`

Hangi alt sistemleri derlediğinize bağlı işlevler: dosya sistemleri, ses sürücüsü, CPU sorgusu, günlükleme, güç kontrolü ve bellek ile dizge yardımcıları. Örneğin FAT sürücülerini bağlamazsanız FAT çağrıları kullanılamaz.

```c
/* console extras */
void openkernel_nonuniversal_api_vga_clear_screen(void);
void openkernel_nonuniversal_api_vga_print_hex(uint32_t n);
void openkernel_nonuniversal_api_vga_print_dec(int num);
void openkernel_nonuniversal_api_vga_print_bin(unsigned int num);

/* audio, cpu, logging, power */
void openkernel_nonuniversal_api_sb16_gen_flat_sound(uint8_t level);
void openkernel_nonuniversal_api_sb16_test(void);
void openkernel_nonuniversal_api_get_sys_cpuinfo(const char *mode);
void openkernel_nonuniversal_api_log_message(klog_level_t level, const char *message);
void openkernel_nonuniversal_api_exit_program(const char *mode, const char *message);
void openkernel_nonuniversal_api_sys_next_status(const char *mode, uint32_t seconds);

/* file systems */
int  openkernel_nonuniversal_api_fat12_read_file(const char* file_name, void* buf, uint32_t size);
void openkernel_nonuniversal_api_fat16_list_root(void);
int  openkernel_nonuniversal_api_fat16_read_file(const char* filename, void* buffer, uint32_t size);
int  openkernel_nonuniversal_api_fat32_read_file(const char* filename, void* buffer, uint32_t size);
void openkernel_nonuniversal_api_fat32_list_dir(uint32_t cluster);
void openkernel_nonuniversal_api_ofs_create_file(const char* name, uint32_t size);
void openkernel_nonuniversal_api_ofs_read_file(const char* name, uint8_t* buffer);
void openkernel_nonuniversal_api_ofs_write_file(const char* name, const uint8_t* data, uint32_t size);
void openkernel_nonuniversal_api_ofs_delete_file(const char* name);
void openkernel_nonuniversal_api_ofs_list_files(void);
void openkernel_nonuniversal_api_vfs_list(void);

/* memory and strings */
void* openkernel_nonuniversal_api_memset(void* dst, uint8_t val, uint32_t size);
void* openkernel_nonuniversal_api_memcpy(void* dst, const void* src, uint32_t size);
int   openkernel_nonuniversal_api_memcmp(const void* a, const void* b, uint32_t size);
void* openkernel_nonuniversal_api_kmalloc(uint32_t size);
void  openkernel_nonuniversal_api_kfree(void* ptr);
int   openkernel_nonuniversal_api_strcmp(const char *s1, const char *s2);
int   openkernel_nonuniversal_api_strncmp(const char *s1, const char *s2, unsigned int n);
int   openkernel_nonuniversal_api_strlen(const char *str);
void  openkernel_nonuniversal_api_strcpy(char *dst, const char *src);
void  openkernel_nonuniversal_api_strncpy(char *dst, const char *src, unsigned int n);

/* time */
void openkernel_nonuniversal_api_print_time(void);
```

## 3. Destekleyici başlıklar

### `KernelAPI/OpenKernelEnvironment/SysCalls/syscalls.h`

Kodunuz ile çekirdek arasındaki asgari sözleşme:

```c
extern int syscall(int id, void* arg);
extern int syscall_handler(int id, void* arg);
```

### `KernelAPI/OpenKernelEnvironment/types.h`

API tüketicilerinin tüm `SystemLib` dosyasını çekmek zorunda kalmaması için tamsayı typedef'lerinin (`uint8_t`, `uint32_t`, `bool_t` vb.) kırpılmış bir kopyası.

> **Uyarı:** bu dosya, `SystemLib/Std/types.h` ile aynı `TYPES_H` dahil etme korumasını kullanır. İkisini aynı çeviri birimine dahil etmek, hangisi ikinciyse onu sessizce düşürür. Ayrıca `NULL`'ı `0` olarak tanımlar; `SystemLib/Std/types.h` ise onu `((void*)0)` olarak tanımlar.

### `KernelAPI/Information/openkernel_api_info.h`

```c
#define KERNEL_API_NAME "OpenKernel API"
#define KERNEL_API_VER  "5.0-06c18fe"
#define KERNEL_API_ARCH "x86 (32-bit)"
#define KERNEL_API_AUTHOR "OpenSoftware-World"
```

Yalnızca bilgilendirme amaçlıdır.

## 4. Tam syscall tablosu

Numaralar `SystemLib/SysCalls/syscall.h` içinde tanımlanmıştır.

| # | Sabit | # | Sabit | # | Sabit |
|---:|---|---:|---|---:|---|
| 0 | `SYSCALL_PRINT` | 15 | `SYSCALL_PRINT_BIN` | 30 | `SYSCALL_OFS_LF` |
| 1 | `SYSCALL_CLEAR_SCREEN` | 16 | `SYSCALL_SB16_FLAT` | 31 | `SYSCALL_VFS_L` |
| 2 | `SYSCALL_SHUTDOWN` | 17 | `SYSCALL_SB16_TEST` | 32 | `SYSCALL_MEMSET` |
| 3 | `SYSCALL_REBOOT` | 18 | `SYSCALL_GET_CPU` | 33 | `SYSCALL_MEMCPY` |
| 4 | `SYSCALL_SET_BG_COLOR` | 19 | `SYSCALL_FAT12_RF` | 34 | `SYSCALL_MEMCMP` |
| 5 | `SYSCALL_SET_TXT_COLOR` | 20 | `SYSCALL_FAT16_LIST_R` | 35 | `SYSCALL_KMALLOC` |
| 6 | `SYSCALL_NEWLINE` | 21 | `SYSCALL_FAT16_RF` | 36 | `SYSCALL_KFREE` |
| 7 | `SYSCALL_SET_CURSOR` | 22 | `SYSCALL_FAT32_RF` | 37 | `SYSCALL_STRCMP` |
| 8 | `SYSCALL_PTCHAR` | 23 | `SYSCALL_FAT32_LIST_R` | 38 | `SYSCALL_STRNCMP` |
| 9 | `SYSCALL_PRINT_NW` | 24 | `SYSCALL_LOG_MSG` | 39 | `SYSCALL_STRLEN` |
| 10 | `SYSCALL_SET_COLOR_SCH` | 25 | `SYSCALL_EXIT_PROG` | 40 | `SYSCALL_STRCPY` |
| 11 | `SYSCALL_GET_CHR` | 26 | `SYSCALL_OFS_CREATE_F` | 41 | `SYSCALL_STRNCPY` |
| 12 | `SYSCALL_SLEEP` | 27 | `SYSCALL_OFS_RF` | 42 | `SYSCALL_PRINT_TIME` |
| 13 | `SYSCALL_PRINT_HEX` | 28 | `SYSCALL_OFS_WF` | 43 | `SYSCALL_SYS_NEXT_STATUS` |
| 14 | `SYSCALL_PRINT_DEC` | 29 | `SYSCALL_OFS_DF` | | |

0 ile 43 dışında herhangi bir kimlik `-1` döndürür.

### Argüman geçirme

Çok değerli çağrılar, `SystemLib/SysCalls/Structs/structs.h` dosyasındaki sarmalayıcı `struct`'ları kullanır:

```c
typedef struct { uint8_t x, y; }              vga_set_cursor_t;
typedef struct { uint8_t bg, text; }          vga_set_color_scheme_t;
typedef struct { const char* name; uint32_t size; } ofs_create_file_t;
typedef struct { const char* name; uint8_t* buffer; }  ofs_read_file_t;
typedef struct { const char* s1, *s2; }       strcmp_t;
/* ... plus fat12/16/32 read, log_message, exit_program, memset, memcpy, memcmp, ... */
```

`arg` olarak `struct`'a bir işaretçi geçirin. Bunlar dağıtıcı tarafından doğru şekilde işlenir.

Tek değerli çağrılar aynı "argümanın adresi" kuralını izler: sarmalayıcı, skalerler için `&value`, işaretçi argümanları için `&pointer` geçirir ve `syscall_handler()` buna göre işaretçiyi çözümler. `SYSCALL_SYS_NEXT_STATUS` (43) de iki argüman taşıdığı için `sys_next_status_t` adlı bir `struct` kullanır.

## 5. Bu katmanın sorunları

### 5.1 API kaynak kodları derlenmiyor

Ne `KernelAPI/nonuniversal_api_defs.c` ne de `KernelAPI/OpenKernelUniversalAPI/universal_api_defs.c`, `Makefile` içindeki `C_SRC` listesinde görünür. Depoda API başlıklarını dahil eden başka bir şey de yoktur. Olduğu gibi katman **ne derleniyor ne de kullanılıyor**.

Kullanmak için her iki dosyayı da listenin geri kalanıyla aynı yol biçiminde `C_SRC`'ye ekleyin.

### 5.2 Skaler argümanlar adres olarak geçiriliyordu, sonra değere dönüştürülüyordu (düzeltildi)

Sarmalayıcılar her zaman tek tip olmuştur - `syscall(id, &arg)` - ancak `syscall_handler()` daha önce `arg`'ı doğrudan değer türüne dönüştürüyordu (`vga_set_bg_color((uint8_t)arg)` rengi değil, *adresin* alt baytını alıyordu). Bu uyuşmazlık syscall numaraları 0, 4, 5, 8, 9, 12'den 16'ya, 18, 23, 29, 35, 36, 39 ve 41'i etkiliyordu (bu belgenin önceki sürümü 9, 36 ve 39'i kaçırmıştı).

`syscall_handler()` artık `arg`'ı çözümler; böylece tüm ABI tek bir kuralı izler:

- skaler argüman: `arg`, değere işaret eder (`&value`)
- işaretçi argüman: `arg`, işaretçiye işaret eder (`&pointer`)
- çok alanlı çağrı: `arg`, bir sarmalayıcı `struct`'a işaret eder
- argüman yok: `arg`, `NULL`'dır

Bu düzeltme ayrıca `syscall.c` dosyasındaki dört `cast from pointer to integer of different size` uyarısını da kaldırdı.

### 5.3 Dönüş değerleri atılıyordu (düzeltildi)

`syscall_handler()` daha önce neredeyse her durumu `return 0;` ile bitiriyordu; bu yüzden `openkernel_nonuniversal_api_kmalloc()` asla kullanılabilir bir işaretçi döndüremezdi, dizge yardımcıları her zaman `0` raporluyordu ve `SYSCALL_GET_CHR` karakteri çöpe atıyordu.

Değer döndüren çağrılar artık sonucu `syscall()` üzerinden yayıyor: `kmalloc`, `memset`, `memcpy` (işaretçiler `int` dönüşüne sığar çünkü bu 32 bitlik bir hedeftir), `memcmp`, `strcmp`, `strncmp`, `strlen`, FAT12/16/32 okuma çağrıları ve `get_char()`. Bunun bir parçası olarak `openkernel_universal_api_get_char()` `void`'den `char`'a değişti; bu düzeltmedeki tek imza değişimidir.

### 5.4 Bir işlev bildirilmiş ama hiç tanımlanmamıştı (düzeltildi)

`openkernel_nonuniversal_api_sys_next_status()`, `nonuniversal_api_defs.h` içinde uygulaması olmadan görünüyordu. Artık yeni `SYSCALL_SYS_NEXT_STATUS` (43) ve `sys_next_status_t` `struct`'ı üzerine kurulmuştur ve her iki argümanı (`mode` ve `seconds`) doğru şekilde taşıdığı doğrulanmıştır.

### 5.5 API daha önce `klog_level_t` bozukluğunu miras alıyordu (çözüldü)

`openkernel_nonuniversal_api_log_message()`, `klog_level_t` alır. `KernelAPI/nonuniversal_api_defs.h`, `klog_level_t` typedef'inin tek sahibi olan `log.h` dosyasını artık dahil eden `SystemLib/SysCalls/Structs/structs.h` dosyasını dahil eder. Eski çakışma - `structs.h` enum'un typedef'ini yaparken `log.h`'nin `klog_level_t` adlı bir *değişken* bildirmesi - ortadan kalktı; her iki başlık tek bir tanımda uzlaşıyor.

## 6. Önerilen kullanım kalıbı

Argüman kuralı ve dönüş değeri kusurları düzeltilmiştir (5. bölüm); kalan sınırlama 5.1'dir - API kaynak kodları varsayılan derlemede yoktur. Onları `C_SRC`'ye ekleyene kadar bir aşağı akış işletim sistemi şunları yapmalıdır:

1. OpenKernel'in gerçek işlevlerini `KernelAPI` üzerinden geçmek yerine doğrudan çağırın (`vga_print_scr_nw()`, `fat16_read_file()`, `ofs_read_file()`) veya iki `KernelAPI/*.c` dosyasını `C_SRC`'ye ekleyin.
2. Sonradan uygulama koduna dokunmadan `KernelAPI`'ye geçebilmek için kendi ince sarmalayıcı katmanınızı koruyun.

```c
/* Your own wrapper - stable for your codebase */
void os_print(const char *s) {
    vga_print_scr_nw(s);
}
```

---

[← Dosya Sistemleri](07-file-systems.md) | [Belge Dizini](README.md) | [Sonraki: Proje Durumu →](09-project-status.md)
