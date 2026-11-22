# OpenKernel Belgeleri

OpenKernel, C ile yazılmış ve kendi işletim sisteminiz tarafından bir taban katmanı olarak yeniden kullanılmak üzere tasarlanmış 32 bit x86 işletim sistemi çekirdeği projesidir. Bu klasör, OpenKernel 4.0 sürümü için tüm belge setini içerir.

Aşağıdaki belgeleri numaralı sırayla okuyun. Her belgenin üstünde ve altında bir "önceki" ve "sonraki" bağlantısı vardır; böylece set boyunca bir kitabın sayfaları gibi ilerleyebilirsiniz.

> **Başlamadan önce:** OpenKernel, kutudan çıktığı gibi derlenir ve bağlanır (`make` komutu `build/os.bin` dosyasını üretir), ancak önyüklemez: kendi önyükleyicinizi ve giriş noktanızı sağlamanızı bekleyen bir çekirdek *kütüphanesi*dir. Amacınız bir şeyi çalışır hale getirmekse [09 - Proje Durumu](09-project-status.md) belgesini erken okuyun.

---

## Okuma sırası

| # | Belge | Kapsadığı konu |
|---|---|---|
| 0 | [Bu dosya](README.md) | Tüm belge dizininin indeksi. |
| 1 | [01 - Giriş](01-introduction.md) | OpenKernel'in ne olduğu, ne olmadığı, hedefleri ve kullandığı teknoloji. |
| 2 | [02 - Başlarken](02-getting-started.md) | Araç zinciri (toolchain) gereksinimleri, ortam kurulumu, kendi işletim sisteminizi baştan kurma ve derleme/çalıştırma. |
| 3 | [03 - Mimari](03-architecture.md) | Dizin düzeni, derleme hattı, bağlama betiği ve sürüm paketinin nasıl üretildiği. |
| 4 | [04 - Çekirdek Hizmetleri](04-kernel-services.md) | Hata işleme, günlükleme, sistem güç kontrolü, sayfalama ve güvenlik denetleyicisi. |
| 5 | [05 - Sistem Kütüphanesi](05-system-library.md) | Bellek ayırma, karakter dizisi işlevleri, port giriş/çıkışı, zaman, görev yönetimi, tür dönüşümü ve iç syscall dağıtıcısı. |
| 6 | [06 - Sürücüler](06-drivers.md) | VGA, CPU, klavye, fare, seri, ses, ATA, AHCI, PCI ve ağ sürücüleri. |
| 7 | [07 - Dosya Sistemleri](07-file-systems.md) | OFS, FAT12, FAT16, FAT32 ve VFS katmanı. |
| 8 | [08 - Çekirdek API](08-kernel-api.md) | Kararlı API arayüz katmanı, evrensel/evrensel olmayan ayrımı ve tam syscall numara tablosu. |
| 9 | [09 - Proje Durumu](09-project-status.md) | Ne çalıştığı, ne kısmi olduğu, ne bir stub olduğu ve mevcut derleme engelleri hakkında dürüst rapor. |
| 10 | [10 - Katkıda Bulunma](10-contributing.md) | Nasıl katkıda bulunulacağı, kodlama kuralları, sürüm süreci ve lisans. |

---

## Belgeler nasıl düzenlenmiştir

Belgeler sorumluluğa göre ayrılmıştır; böylece yalnızca ihtiyacınız olanı arayabilirsiniz:

- **Yönlendirme** - 01 ila 03 numaralı belgeler projeyi, kurulumu ve yapıyı açıklar. Yeniyseniz buradan başlayın.
- **Referans** - 04 ila 08 numaralı belgeler her alt sistemi ve genel işlevlerini tanımlar. Kod yazarken bunları kullanın.
- **Değerlendirme ve süreç** - 09 ve 10 numaralı belgeler kod tabanının gerçek durumunu ve onun üzerinde nasıl çalışılacağını açıklar.

Hiçbir belge, kaynak ağacında var olmayan bir özelliği tanımlamaz. Bir özellik eksik veya bozuksa, belge bunu açıkça belirtir.
