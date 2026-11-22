# 10 - Katkıda Bulunma

[← Proje Durumu](09-project-status.md) | [Belge Dizini](README.md)

---

Bu belge, OpenKernel'in kendisi üzerinde nasıl çalışılacağını kapsar: katkı iş akışı, kodlama kuralları, test beklentileri, sürüm süreci ve lisans.

## 1. Katkıda bulunmadan önce

Önce [09 - Proje Durumu](09-project-status.md) dosyasını okuyun. Ağacın artık tek geçişte derlendiği ve bağlandığı için bir katkı `make` ile doğrulanabilir. Hâlâ eksik olan bir önyükleme yoludur: `_start` yok, multiboot başlığı yok ve `Boot/` dizini yok; bu yüzden henüz hiçbir şey yürütülemez.

Önerilen etki sırası:

1. Projeyi gerçekten çalıştırıp test edebilmek için asgari bir önyükleme yolu ekleyin (multiboot başlığı ve `_start` içeren `Boot/boot.asm` ile `Boot/grub/grub.cfg`). Bu, kalan tek bağlama zamanı engelleyicisidir.
2. Yeni giriş noktasından, `link.ld`'nin zaten sağladığı `_heap_start` / `_heap_end` sembollerini kullanarak `heap_init()`'i çağırın. O zamana kadar `kmalloc()` her istek için `NULL` döndürür.
3. KernelAPI'nin skaler ve işaretçi argüman kuralına karar verin; kalan dört derleyici uyarısının kaynağı budur.
4. Bir test veya CI işi ekleyin. Bugün hiçbir türde otomatik doğrulama yoktur.

## 2. Katkı iş akışı

1. Depoyu fork edin.
2. `main`'den bir özellik dalı oluşturun.
3. Değişikliklerinizi yapın.
4. Onları doğrulayın (bkz. 4. bölüm).
5. Ne değiştiğini ve nedenini açıklayan bir pull request açın.

Hata raporları ve özellik istekleri `.github/ISSUE_TEMPLATE/` içindeki şablonları kullanır:

- `bug-report.md` - hatayı, yeniden üretme adımlarını, beklenen davranışı ve ortamınızı açıklayın.
- `feature_request.md` - sorunu ve önerilen çözümü açıklayın.

Derleme hatası raporlarken derleyici sürümünüzü (`i686-elf-gcc --version`) ve çalıştırdığınız tam komutu ekleyin. Aynı kod GCC 13 ve GCC 14 üzerinde farklı davranır; [09 - Proje Durumu](09-project-status.md) belgesinde açıklandığı gibi.

## 3. Kodlama kuralları

Kod tabanı tutarlı kalsın diye ağacın içinde zaten bulunan kurallara uyun.

### Dil ve derleme modu

- C, `-m32 -ffreestanding -O2 -Wall -Wextra` ile derlenir.
- Standart kütüphane yoktur. `libc` yoktur, `printf` yoktur, kütüphaneden gelen `malloc` yoktur. İhtiyacınız olan her işlev ağatta tanımlanmış olmalıdır.
- Yalnızca 32 bitlik x86 hedeflenir. 64 bit varsayımları getirmeyin.

### Adlandırma

| Tür | Kalıp | Örnek |
|---|---|---|
| Genel (public) işlev | `subsystem_action` veya `subsystem_verb_noun` | `vga_print_scr_nw`, `ata_sys_disk_read` |
| Başlatıcı | `*_init()` | `vga_init`, `heap_init`, `ofs_init` |
| Tür | `*_t` eki | `task_t`, `ofs_file_t`, `pci_dev_t` |
| Makro sabiti | `UPPER_SNAKE_CASE` | `OFS_MAX_FILES`, `SB16_BASE` |
| Modül üst verisi | `<MODULE>_MODULE_*` | `MEM_MODULE_VER`, `KRNL_VER` |

### Başlık biçimi

Her başlığın bir dahil etme koruması olmalı ve şu şekli izlemelidir:

```c
#ifndef SOME_MODULE_H
#define SOME_MODULE_H

#include <OpenKernel/SystemLib/Std/types.h>

/* public declarations */

#define SOME_MODULE_NAME    "..."
#define SOME_MODULE_VER     "1.0"
#define SOME_MODULE_KRNL_VER "3.0"

#endif
```

### Yorumlar

- Yorumları **İngilizce** yazın.
- Açık olmayan her işlevin başına açıklayıcı bir yorum yerleştirin.
- *Ne yaptığını* değil, *nedenini* yorumlayın. Kod zaten ne yaptığını söylemelidir.

```c
/* Reads one 512-byte sector using PIO mode; blocks until DRQ is set. */
static void ata_wait_drq(void) { ... }
```

### Tasarım kuralları

- Dosya başına tek sorumluluk tutun. Var olan bir modüle ait sıradan bir yardımcı için yeni dosya eklemeyin.
- Açık başlatmayı tercih edin: diğer çağrıların içinde tembel başlatma yapmak yerine bir `*_init()` işlevini açığa çıkarın.
- Var olan yardımcıları yeniden kullanın. Port giriş/çıkışına ihtiyacınız varsa `SystemIO/io.h` kullanın. Bir dizge işlevine ihtiyacınız varsa yerel bir kopya yazmak yerine `SystemLib/Std/std.c` dosyasını genişletin. Şu anda birkaç dosya `itoa` tarzı yardımcıları yineliyor; bunları birleştirmek hoş bir değişiklik olur.
- Gereksiz soyutlama katmanlarından kaçının. Bu proje genellikten çok okunabilirliği değerli bulur.
- Bir modülün sürümünü değiştirdiğinizde modül üst verisi makrolarını güncel tutun.

### Yapmayın

- Standart kütüphane bağımlılığı eklemeyin.
- Yeni bir derleme sistemi eklemeyin. `make` ve `link.ld` kalır.
- Derleme çıktısını commit etmeyin. `build/`, `iso/`, `*.o` ve `*.iso` zaten `.gitignore` tarafından kapsanıyor.
- Sırları, anahtarları veya kimlik bilgilerini commit etmeyin. `.gitignore` zaten `secrets/`, `credentials/` ve `keys/` dizinlerini hariç tutuyor.
- `version.txt` dosyasını bir sürümün parçası olmadıkça değiştirmeyin (bkz. 6. bölüm).

## 4. Test etme

**Bu depoda otomatik test altyapısı yoktur.** Test dizini yok, test çalıştırıcısı yok ve kodu derleyen bir CI işi yoktur. Bugün "tüm testler geçiyor" demek mümkün değildir.

Bunun yerine ne yapmalısınız:

### Derleme denetimi

Her kaynak dosyayı bir sözdizimi denetiminden geçirin ve sonucu raporlayın:

```bash
for f in $(find . -name '*.c' -not -path './.git/*'); do
    if i686-elf-gcc -m32 -ffreestanding -O2 -Wall -Wextra -I .. -fsyntax-only "$f"; then
        echo "PASS $f"
    else
        echo "FAIL $f"
    fi
done
```

Başlıklar `<OpenKernel/...>` biçiminde dahil edildiği için depo içinden çalıştırırken `-I ..` kullanın. Paket düzeninden çalıştırırken `-I .` kullanın.

Yeni uyarıları hata olarak değerlendirin. Proje zaten `-Wall -Wextra` taşıyor.

### Bağlama denetimi

Makefile yolları düzeltildiğinde eksiksiz bir `make` asıl sınırdır. Aynı anda on şeyi düzeltmek yerine karşılaştığınız ilk hatayı raporlayın.

### Çalışma zamanı denetimi

Önyüklenebilir bir görüntünüz varsa:

```bash
make run
```

QEMU, 512 MiB RAM, 4 sanal CPU, `disk.img` üzerinde bir IDE disk, bir SB16 aygıtı ve stdio üzerinde seri çıktı ile başlatılır. Çalışma zamanı bir sorun raporlarken seri çıktıyı yakalayın.

### İyi bir rapor nasıl görünür

- Derleyici sürümü ve ana makine dağıtımı.
- Tam komut ve çıktısının tamamı.
- Hangi dosyaları değiştirdiğiniz.
- Değişikliğin derleme, bağlama, önyükleme ile doğrulanıp doğrulanmadığı veya hiç doğrulanmadığı.

Test paketi olmayan bir projede son noktada açık olmak her şeyden daha önemlidir.

## 5. Sürekli entegrasyon

GitHub'da iki iş akışı çalışır:

| İş akışı | Tetikleyici | Ne yaptığı |
|---|---|---|
| `release.yml` | `version.txt`'yi değiştiren `main`'e push | Sürüm ZIP'ini paketler ve yayınlar |
| `codeql.yml` | Her push ve pull request, ayrıca haftalık | `build-mode: none` ile C için CodeQL güvenlik analizi |

Önemli: **hiçbir iş akışı kodu derlemez.** CodeQL derleme olmadan çalışır; bu yüzden derleme hatalarını yakalayamaz. Sürüm iş akışı yalnızca dosyaları paketler.

Değerli bir katkı, her pull request'te 4. bölümdeki derleme denetimini çalıştıran üçüncü bir iş akışı olurdu. Bu, şu anda başarısız olan 11 dosyanın tamamını otomatik olarak yakalardı.

## 6. Sürüm süreci

Sürümler tamamen `version.txt` tarafından yönetilir.

1. `version.txt` dosyasını yeni sürüm dizesiyle güncelleyin (şu an `4.0`).
2. `main`'e push edin.

Ardından iş akışı:

1. Sürümü okur.
2. `.git`, `.github`, `version.txt`, `install_pkg.sh`, `link.ld`, `Makefile`, `notes.txt` ve `package` dışarıda bırakarak ağacı `package/OpenKernel/` dizinine kopyalar.
3. `Makefile`, `link.ld`, `notes.txt` ve `install_pkg.sh` dosyalarını `OpenKernel/` klasörünün yanına yerleştirir.
4. `OpenKernel_<version>.zip` üretir.
5. Önceki sürüm artışından bu yana git günlüğünden `CHANGELOG.md` oluşturur.
6. `openkernel<version>` etiketli bir sürüm oluşturur.

Paketleme adımı `Makefile` ve `link.ld` dosyalarını iç klasörden hariç tutup üst düzeyine taşıdığı için ortaya çıkan ZIP, `C_SRC`'nin beklediği düzene sahiptir. Bkz. [02 - Başlarken](02-getting-started.md) 3. bölüm.

İlgili iki bakım görevi sürüm sırasında yapılabilir:

- `Kernel/openkernel_information.h` içindeki `KRNL_LAST_COMMIT` ve `KernelAPI/Information/openkernel_api_info.h` içindeki `KERNEL_API_VER` değerlerini güncelleyin. İkisi de şu anda eskimiş durumda.
- Sürüm değiştiyse `Kernel/openkernel_information.h` içindeki `KRNL_VER` değerini güncelleyin.

## 7. Belgeler

Kullanıcıya görünür davranışı değiştirirseniz `docs/en/` içindeki ilgili dosyayı (ve `docs/tr/` içindeki çevirisini) güncelleyin:

| Değişiklik | Güncellenecek |
|---|---|
| Yeni alt sistem veya genel (public) işlev | İlgili başvuru belgesi (04 ile 08 arası) |
| Derleme süreci veya gereksinimler | `02-getting-started.md` |
| Dizin düzeni veya bağlama betiği | `03-architecture.md` |
| Bir özelliğin durumu | `09-project-status.md` |
| Yeni belge dosyası | `docs/en/README.md` ve `docs/tr/README.md` içindeki okuma sırası tablosu, ayrıca bitişik belgelerdeki önceki/sonraki bağlantıları |

Bu sette kullanılan belge kuralları:

- Dosya başına tek konu: `docs/en/` içinde ve `docs/tr/` içinde çevirisiyle birlikte.
- `docs/README.md` adresinde dil dizini, her dil klasöründe de (`docs/en/README.md`, `docs/tr/README.md`) her dosyanın ne kapsadığını açıklayan okuma sırası dizini.
- Her belgenin üst ve alt kısmında önceki ve sonraki bağlantıları.
- İngilizce ve Türkçe, yalın dil, en az emoji.
- Var olmayan bir özelliği asla belgelemeyin. Bir şey bozuksa, öyle deyin.

## 8. Lisans

OpenKernel, **GNU General Public License, Version 2** altında lisanslanmıştır. Depo kökündeki `LICENSE` dosyasına bakın.

Pratik olarak:

- OpenKernel'i kullanabilir, değiştirebilir ve dağıtabilirsiniz.
- Türetilmiş eserler de GPL-2.0 altında dağıtılmalıdır.
- Garanti yoktur. Proje eğitim ve geliştirme amaçlı kullanım için tasarlanmıştır.

Katkıda bulunursanız, katkınız aynı lisans altında yapılır.

## 9. Sorumluluk reddi

Bu proje eğitim ve geliştirme amaçları için tasarlanmıştır. Kullanıcı, bu yazılımı kullanmaktan kaynaklanabilecek tüm sorun veya hasarlardan tüm sorumluluğu üstlenir.

Önem verdiğiniz verilerin bulunduğu bir yedeği olmadan deneyimsel çekirdekleri çalıştırmayın. Çekirdek hataları diskleri bozabilir ve mevcut kod tabanındaki birkaç alt sistem (özellikle OFS üst verisi yönetimi) istenmeyen bellek konumlarına yazabilir.

---

[← Proje Durumu](09-project-status.md) | [Belge Dizini](README.md)
