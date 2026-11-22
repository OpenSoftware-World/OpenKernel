# 07 - Dosya Sistemleri

[← Sürücüler](06-drivers.md) | [Belge Dizini](README.md) | [Sonraki: Çekirdek API →](08-kernel-api.md)

---

OpenKernel dört dosya sistemi uygulaması ve bir dağıtıcı katmanı sunar:

| Bileşen | Yol | Yetenekler |
|---|---|---|
| OFS (Open File System) | `OFS/` | Oluşturma, okuma, yazma, silme, listeleme |
| FAT12 | `FAT12/` | Oluşturma, okuma, yazma, silme, listeleme (kök dizin) |
| FAT16 | `FAT16/` | Oluşturma, okuma, yazma, silme, listeleme (kök dizin) |
| FAT32 | `FAT32/` | Oluşturma, okuma, yazma, silme, listeleme (kök veya herhangi bir dizin kümesi) |
| VFS | `VFS/` | Tek bir etkin dosya sistemi seçer |

FAT sürücüleri dosya yazabilir ancak biçimlendiremez: projede hiçbir yerde bir FAT biçimlendirici yoktur, dolayısıyla birim yine de `mkfs.fat` gibi ana makine (host) araçlarıyla oluşturulmak zorundadır.

## 1. OFS - Open File System

Bu proje için tasarlanmış, bilerek basit bir düz (flat) dosya sistemi.

### Disk üstü düzen

| Bölge | Sektörler | İçerik |
|---|---|---|
| Superblock | 0 | İmza `"OFS1"`, `total_sectors`, `file_table_start`, `data_start`, `version` |
| Dosya tablosu | 1'den 11'e | 11 sektör (5.632 bayt), tam olarak 128 x 44 baytlık girdi için yeterli |
| Veri | 12'den itibaren | Her dosya sabit 8 sektörlik (4 KiB) bir yuva kaplar |

Her dosya girdisi şöyledir:

```c
typedef struct {
    char     name[32];        /* OFS_NAME_MAX_LENGTH */
    uint32_t start_sector;
    uint32_t size_bytes;
    uint8_t  used;
    uint8_t  flags;
    uint8_t  reserved;
} ofs_file_t;
```

Yuvalar sabit boyutlu olduğundan, bir dosyanın sektör indeksi basitçe `data_start + index * 8` olur. Boş alan haritası yoktur, tahsis bit haritası yoktur ve bir dosyanın 4 KiB'ı aşması mümkün değildir.

### İşlevler

```c
void ofs_init(void);
void ofs_create_file(const char* name, uint32_t size);
void ofs_read_file(const char* name, uint8_t* buffer);
void ofs_write_file(const char* name, const uint8_t* data, uint32_t size);
void ofs_delete_file(const char* name);
void ofs_list_files(void);
```

### `ofs_init()` davranışı

1. Sektör 0'ı 512 baytlık bir tampona okur ve 20 baytlık superblock'u oradan kopyalar (eski kod 512 baytı doğrudan 20 baytlık yapıya okuyor, böylece tablonun ilk 11 girdisinin üzerine yazıyordu).
2. Superblock'u doğrular: imza `"OFS1"`, sıfırdan farklı bir `file_table_start` ve `data_start >= file_table_start + 11`.
3. Doğrulama başarısız olursa biçimlendirir: yeni bir superblock yazar, tabloyu bellekte sıfırlar ve 11 tablo sektörünü diske boşaltır. Bir durum iletisi VGA katmanı üzerinden yazdırılır (VGA sürücüsü devre dışıyken bu hiçbir şey yapmaz).
4. Aksi halde superblock'u içeri kopyalar ve 11 tablo sektörünü yükler.

Böylece boş bir diskteki ilk kullanım diski kendiliğinden biçimlendirir ve önceki 8 sektörlik düzenle yazılmış bir görüntü, veri alanı tabloyun üzerine sessizce binmek yerine yeniden biçimlendirilir.

Her değişiklik (`ofs_create_file()`, `ofs_write_file()`, `ofs_delete_file()`) artık aynı boşaltma (flush) yardımcısını çağırır; dolayısıyla üstveri bir yeniden başlatmadan sağ çıkar.

### Kullanım

```c
ofs_init();

ofs_create_file("hello.txt", 128);

const char* msg = "Hello, OFS!";
ofs_write_file("hello.txt", (const uint8_t*)msg, 11);

uint8_t buf[128];
ofs_read_file("hello.txt", buf);

ofs_list_files();
ofs_delete_file("hello.txt");
```

### Bilinen kusurlar

OFS'e güvenmeden önce bunları okuyun:

1. **`ofs_read_file()` tampon uzunluğu almaz.** Sektör sayısı yalnızca `size_bytes`'tan gelir (8 sektörlik yuvaya sınırlandırılır), dolayısıyla çağırıcı tamponun en az `size_bytes` büyük olmasını garanti etmelidir. FAT okuyucuları bir `size` alır; bu almaz.
2. **Yazarken oluşturma yoktur.** `ofs_write_file()` yalnızca zaten var olan dosyaları günceller.
3. **`ofs_create_file()` veri alanını sıfırlamaz**, dolayısıyla yeni oluşturulan bir dosya, yazılana kadar bayatlardan (stale) oluşur.
4. **Superblock'taki `total_sectors` hiçbir zaman doldurulmaz.**
5. **Disk üstü düzeni değişti.** Tablo alanı 8 sektörden (4.096 bayt - 128 x 44 baytlık girdi için çok küçük) türetilmiş 11 sektöre geçti. `ofs_init()`, `data_start >= file_table_start + 11` uymayan bir görüntüyü yeniden biçimlendirir; dolayısıyla önceki kodla yazılmış bir disk ilk bağlamada (mount) içeriğini kaybeder.
6. **Dizin yok, boş alan haritası yok, 4 KiB'den büyük dosya yok.**

Bu sürümde düzeltildi, geçmiş kaybolmasın diye burada tutuluyor:

- Tablo eskiden `ofs.file_table + (i * SECTOR_SIZE)` olarak adreslenirdi; bu, `i * 512 * sizeof(ofs_file_t)` bayt ilerletir (yaklaşık her yinelemede 22 KiB) ve nesnenin çok dışında yazıyordu. Artık bayt adreslemesidir.
- Superblock eskiden 20 baytlık bir yapıya 512 baytlık bir okuma ile doldurulur ve aynı şekilde geri yazılırdı. Her iki yönden artık bir sektör boyutlu tampon üzerinden geçer.
- `ofs_create_file()`, `write_file()` ve `ofs_delete_file()` eskiden yalnızca bellekteki tabloyu değiştirirdi; biçimlendirme dışında hiçbir zaman geri yazılmazdı. Artık boşaltıyorlar.
- `ofs_write_file()` eskiden tam 512 baytlık sektörleri doğrudan çağırıcının verisinden aktarırdı; dolayısıyla 512'nin katı olmayan bir boyut, çağırıcının tamponunun sonu dışında bir aşırı okumaya (over-read) neden olurdu. Kısmi sektörler artık bir geçici tamponda oluşturulur ve sıfırla doldurulur, aktarım 8 sektörlik yuvaya sınırlandırılır.

## 2. FAT12, FAT16, FAT32

Üçü de aynı tasarımı izler: BIOS Parametre Bloğunu okur, FAT'i ve kök dizini bulur, sonra kümeleri (cluster) zincirler boyunca izler.

Yalnızca BPB ayrıştırması türe özgüdür. Giriş genişliğine bağlı olmayan her şey, birimi tanımlayan `fatfs_ctx_t` üzerinde çalışan paylaşılan çekirdek `FATLib/fatlib.c` içinde bulunur:

```c
typedef struct {
    fatfs_type_t type;          /* FATFS_TYPE_FAT12 / _FAT16 / _FAT32 */
    uint32_t fat_start_lba;
    uint32_t fat_sectors;
    uint8_t  fat_count;         /* every copy is kept in sync on write */
    uint32_t root_dir_lba;      /* FAT12/FAT16 fixed root */
    uint32_t root_dir_sectors;  /* FAT12/FAT16 only, 0 for FAT32 */
    uint32_t data_start_lba;
    uint32_t root_cluster;      /* FAT32 root, 0 otherwise */
    uint16_t bytes_per_sector;  /* 512 once the volume is usable */
    uint8_t  sectors_per_cluster;
    uint32_t total_clusters;    /* 0 when the BPB does not say */
    uint32_t alloc_hint;        /* where the next free-space scan starts */
} fatfs_ctx_t;
```

`fat12_fs_t`, `fat16_fs_t` ve `fat32_fs_t` artık ayrıştırılmış önyükleyici sektörünü artı bu bağlamı tutar; eski `fat_start_lba` / `root_dir_lba` / `data_start_lba` alanları bunun içine katlandı.

### Ortak işlevler

```c
void     fat12_init(uint32_t boot_lba);     /* same shape for fat16, fat32 */
int      fat12_read_file(const char* file_name, void* buf, uint32_t size);
bool_t   fat12_create_file(const char* file_name, uint32_t size);
bool_t   fat12_write_file(const char* file_name, const uint8_t* data, uint32_t size);
bool_t   fat12_delete_file(const char* file_name);
uint32_t fat12_get_file_size(const char* file_name);
uint16_t fat12_get_next_cluster(uint16_t cluster);
void     fat12_list_root(void);
```

`fat16_*` aynıdır. `fat32_*` `uint32_t` kümeleri kullanır ve yalnızca kökü değil, keyfi bir dizin kümesini listeleyen `fat32_list_dir(uint32_t cluster)` ekler.

### Okuma nasıl çalışır

1. `*_init(boot_lba)` önyükleyici sektörünü bir sektör boyutlu tampona okur, `bytes_per_sector == 512`, sıfırdan farklı `sectors_per_cluster`, `sectors_per_fat` ve `fat_count` doğrular, sonra bağlamı doldurur. Doğrulamayı başarısız bir önyükleyici sektör bağlamı *hazır değil* bırakır; dolayısıyla sonraki her çağrı diske dokunmadan döner, sıfıra bölme yerine.
2. `fatfs_finalize_geometry()` `total_clusters`'ı BPB toplamlarından türetir ve FAT'in boyutu ile türe özgü rezerv/kötü/EOC işaretçi aralığının adresleyebileceği değere sıkıştırır (clamp); dolayısıyla yanlış bir BPB yalnızca tahsis edilebilir alanı küçültebilir.
3. `fatfs_read_file()` çağırıcının adını ham 11 baytlık 8.3 alanına çevirir, kök dizini tam eşleşme için tarar, kümeler zincirini izler ve her kümeyi çağırıcının tamponuna kopyalar. Kopya `min(size, file_size)` noktasında durur; dolayısıyla bir okuma artık dosyanın sonunu aşamaz.
4. Küme zinciri çözümlemesi türe göre değişir:

| Varyant | Giriş genişliği | Formül |
|---|---|---|
| FAT12 | 12 bit, paketlenmiş | indeks `cluster + cluster/2`, sonra `0x0FFF` maskele |
| FAT16 | 16 bit | indeks `cluster * 2` |
| FAT32 | 32 bit, üst 4 bit rezerv | indeks `cluster * 4`, `0x0FFFFFFF` maskele |

### Yazma

Üç varyant da tek bir uygulamayı paylaşır:

- **`fatfs_create_file()`** adı çevirir, zaten var olduğu için reddeder, bir dizin yuvası ayırır, kümeler zincirini tahsis eder ve **sıfırlar**, sonra tarih alanlarında `FATFS_DATE_EPOCH` (1980-01-01) taşıyan bir arşiv girdisi yazar. Uzunluğu sıfır olan dosya, FAT şartnamesinin gerektirdiği gibi küme 0 alır.
- **`fatfs_write_file()`** dosya yoksa onu oluşturur (OFS ve tFS'in de kullandığı aynı sözleşme), yeni boyutun daha fazla küme gerektirdiğinde zinciri büyütür, veriyi sektör sektör yazar, fazla kümeleri kırpır ve son olarak `file_size`'ı günceller. Her kısmi sektör önce sıfırla doldurulur; dolayısıyla `file_size`'ı geçen baytlar önceki yerleşimcinin bayat verisini dışarı sızdıramaz. Birim dolduğunda eski içerik olduğu gibi bırakılır ve `false` döndürülür.
- **`fatfs_delete_file()`** tüm zinciri serbest bırakır, sonra girdiyi `0xE5` olarak işaretler.
- **`fatfs_alloc_cluster()`** talep edilen kümeyi hemen zincir sonu olarak işaretler (boş göründüğü bir pencere olmaz) ve her taramayı `alloc_hint`'ten sürdürür; dolayısıyla birimi doldurmak doğrusal kalır. `fatfs_fat_set()` FAT'in **her** kopyasını yazar ve oku-değiştir-yaz yolu bir sektör sınırını aşan bir girdiyi tolere eder.
- **Adlar** `fatfs_name_to_83()`'ten geçer: `"readme.txt"` `README  TXT` olur, zaten dolu 11 baytlık noktasız bir alan olduğu gibi alınır, yasaklı 8.3 karakterleri ile denetim karakterleri reddedilir. Adlar yalnızca ASCII'dir; bir LFN yazıcısı yoktur.

### Üçünün de sınırlamaları

- **Yalnızca kök dizin.** FAT12 ve FAT16 başka hiçbir yerde dosya oluşturamaz. FAT32 bir alt dizini küme ile *listeleyebilir* (`fat32_list_dir()`), ancak oluşturma/yazma/silme/okuma her zaman kökü hedefler.
- **Yalnızca 8.3 adları.** Uzun dosya adı desteği yok: LFN girdileri okumada atlanır ve yazmada hiç üretilmez.
- **Biçimlendirici yok.** Sürücüler mevcut bir birimi bağlar; hiçbir zaman önyükleyici sektör yazmaz.
- **Saatten zaman damgası yoktur.** Dosyalar FAT çağ (epoch) tarihini (1980-01-01) ve saat olarak gece yarısını alır; çünkü projede henüz kimse RTC okumasını FAT tarih/saat alanlarına dönüştürmüyor.
- **Döngü koruması.** Dizin ve zincir yürüyüşleri `FAT_MAX_CLUSTER_STEPS` (65.536) adımdan sonra durur. Dosya başına veri yürüyüşü, bayt sayacı katı arttığı için kendiliğinden sona erer.
- **Sınırlı aktarımlar.** Her sektör yerel bir 512 baytlık tampona okunur ve yalnızca hâlâ sığan baytlar kopyalanır; dolayısıyla sektör boyutunun katı olmayan bir `size`, çağırıcının tamponunun dışına yazamaz.
- **Geometri doğrulanır**, ancak beyan edilen türe karşı küme sayısı kontrol edilmez: bir FAT16 görüntüsünde `fat12_init()` çağırmak yine kullanıcı hatasıdır ve FAT'i yanlış okur.
- VFS onları çağırdığı her yerde `boot_lba` `0`'a sabitlenmiştir.

### Derleme durumu

Dört dosyanın tamamı (`fatlib.c` artı üç sürücü) `-Wall -Wextra` ile hatasız derleniyor ve ağaçta tanımsız bir `fatfs_*` sembolü olmadan bağlanıyor. Çalışma zamanı davranışı hiç çalıştırılmadı.

### Bir FAT dosya sistemini test etme

Projenin bir biçimlendiricisi olmadığı için görüntüyü ana makinede oluşturun:

```bash
qemu-img create fatdisk.img 32M
mkfs.fat -F 16 fatdisk.img
mcopy -i fatdisk.img myfile.txt ::
```

Sonra QEMU'da onu bağlayın ve `fat16_init(0)`'ı ona yönlendirin.

## 3. VFS - `VFS/vfs.c`

### Nedir

*Global olarak seçilmiş* bir dosya sistemi üzerinde ince bir dağıtıcı katman.

```c
typedef enum filesystem_list { OFS, FAT12, FAT16, FAT32, tFS } fs_t;

void vfs_init(fs_t fsname);
void vfs_list(void);
void vfs_read_file(const char* name, uint8_t* buffer, void* buf, uint32_t size);
void vfs_create_file(const char* name, uint32_t size);
void vfs_write_file(const char* name, const uint8_t* data, uint32_t size);
void vfs_delete_file(const char* name);
```

`vfs_init()` seçimi statik bir `current_fs` içinde saklar ve 0 önyükleme LBA'sıyla o dosya sisteminin `*_init(0)` işlevini çağırır. Bundan sonra `vfs_list()` ve `vfs_read_file()` `current_fs` üzerinde switch yapar ve seçilen uygulamaya iletir.

```c
vfs_init(FAT16);
vfs_list();

uint8_t buffer[4096];
vfs_read_file("README.TXT", buffer, buffer, 4096);
```

### Ne değildir

Bunu gerçek bir sanal dosya sistemi ile karıştırmayın:

- **Bağlama (mount) tablosu yoktur.** Bir seferde tüm çekirdek için tek bir dosya sistemi etkindir.
- **Bağlama noktaları yok, yol yok, dizin yok.** Çıplak bir dosya adı geçirirsiniz.
- **Dosya tutamağı yok, konum (offset) yok, seek yok.** Okumalar her zaman baştan başlar.
- **Tek `current_fs` genel değişkeni dışında bağlama başına durum yoktur.**
- **Arka uç kayıt mekanizması yoktur.** Bir dosya sistemi eklemek enum'u ve switch ifadelerini düzenlemek demektir.

### Çift tamponlu imza

```c
void vfs_read_file(const char* name, uint8_t* buffer, void* buf, uint32_t size);
```

`buffer` OFS için, `buf` ise FAT varyantları için kullanılır. Bu, `ofs_read_file()` boyut argümanı alırken FAT okuyucularının almasından dolayı vardır. Pratikte çoğu çağırıcı aynı işaretçiyi iki kez geçirir, yukarıda gösterildiği gibi. Bu, tutarsız alt yapı imzalarından kaynaklanan bir tasarım kusurudur (design smell).

### Yazma desteği

`vfs_create_file()`, `vfs_write_file()` ve `vfs_delete_file()` `vfs.h` içinde bildirilir ve `vfs.c` içinde tanımlanır. Her biri `current_fs` üzerinde switch yapar ve seçilen uygulamaya iletir; OFS, tFS **ve** üç FAT varyantının tamamı için aynı şekilde:

```c
vfs_init(FAT16);

vfs_create_file("hello.txt", 64);
vfs_write_file("hello.txt", (const uint8_t*)"Hello, FAT!", 11);

uint8_t buffer[64];
vfs_read_file("hello.txt", buffer, buffer, 64);   /* 11 bytes are copied */

vfs_delete_file("hello.txt");
```

Bir FAT biriminde bu çağrılar dolayısıyla `fat16_create_file()` ve arkadaşlarına ulaşır. FAT arka uçları yalnızca kök dizine ve yalnızca 8.3'e açıktır (bkz. 2. bölüm) ve üç `void` sarmalayıcının hiçbiri hatayı bildirmez: dolu bir birim veya geçersiz bir ad sessizce görmezden gelinir. `bool_t` sonucuna ihtiyacınız varsa doğrudan `fat*_` işlevlerini kullanın.

### Derleme durumu

`vfs.c`, `vfs.h` ve FAT arka uçları hatasız derleniyor. `vfs_read_file()`'da hâlâ bir hata dönüş yolu yoktur: altta yatan `fat*_read_file()` çağrılarının dönüş değerleri atılır ve `ofs_read_file()` `size`'ı tümüyle yok sayar (uzunluk parametresi yoktur).

## 4. Bir dosya sistemi seçmek

| İstediğiniz | Kullanın | Neden |
|---|---|---|
| Kendinizin biçimlendirdiği küçük, kendine yeten bir birim | OFS | İlk bağlamada boş bir diski biçimlendirir. Sabit 4 KiB dosya başına yuvayı ve okumadaki eksik tampon uzunluğu parametresini unutmayın. |
| 4 KiB'ten büyük dosyalar veya ana makine araçlarıyla oluşturulmuş bir görüntü | FAT12/16/32 | `mkfs.fat` ile oluşturabileceğiniz standart biçimler; dosyalar sabit bir yuvayla değil, birimle sınırlıdır. |
| İşletim sistemi kodunuz için tek bir anahtar noktası | VFS | Hangi dosya sisteminin etkin olduğunu gizler, esneklik bedeliyle. |
| VFS üzerinden oluşturabileceğiniz alt dizinler | Yukarıdakilerin hiçbiri | OFS ve tFS'in dizinleri yoktur ve FAT sürücüleri yalnızca köke yazar (FAT32 bir alt dizini küme ile *listeleyebilir*). |

## 5. Dosya sistemleri depolamaya nasıl ulaşır

Her dosya sistemi, port `0x1F0` üzerindeki ATA sürücüsü üzerinden (`ata_sys_disk_read` / `ata_sys_disk_write`) ham sektör giriş/çıkışı (I/O) gerçekleştirir. Sonuçlar:

- Disk birincil IDE kanalında olmalıdır; `make run` bunu `-drive file=disk.img,format=raw,if=ide` ile sağlar.
- Sektör 0 ya bir OFS superblock'u ya da bir FAT önyükleyici sektörü içermelidir.
- AHCI sürücüsü hiçbir dosya sistemine bağlanmamıştır; yalnızca SATA makineler desteklenmez.
- `heap_init()` ve `vga_init()` diğer alt sistemler için ön koşul olduğundan önce onları başlatın; aksi halde yazdıran dosya sistemi işlemleri sessizce hiçbir şey yapmaz.

---

[← Sürücüler](06-drivers.md) | [Belge Dizini](README.md) | [Sonraki: Çekirdek API →](08-kernel-api.md)
