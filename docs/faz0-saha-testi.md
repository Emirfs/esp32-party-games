# Faz 0 — Saha testi

Amaç: tasarımın dayandığı dört varsayımı gerçek telefonlarla ölçmek. Sonuçlar
Faz 1'in yeniden bağlanma, portal ve güç kararlarını belirler.

## Hazırlık

1. `cd firmware && pio run -t upload && pio device monitor`
2. Logda `SoftAP "OyunKutusu-XXXX" up` ve `listening on http://192.168.4.1` görünmeli.
3. Wi-Fi: `OyunKutusu-XXXX`, şifre `oyunkutusu`.
4. Test sayfası `http://192.168.4.1`: bağlantı durumu, gecikme, yeniden bağlanma,
   "Ekran açılınca" süresi, masadakiler, kutu durumu, olay günlüğü.

## Testler

### 1. Portal ve ağda kalma (her işletim sistemi için)

- Ağa bağlan → giriş penceresi kendiliğinden açılıyor mu?
- "Oyuna gir" → test sayfası açılıyor mu? Pencere "Bitti/Done" gösteriyor mu?
- **Mobil veri açıkken** sayfa çalışıyor mu? Android "sınırlı bağlantı" / "internet yok" uyarısı veriyor mu, ağdan çıkıyor mu?
- Tarayıcıyı kapatıp `http://192.168.4.1` yeniden açılınca "cihaz" kimliği aynı mı (kimlik kalıcı mı)?

### 2. Ekran kilidi ve arka plan

Sayfa açıkken kilitle, bekle, aç. "Ekran açılınca" ve olay günlüğünü yaz.

| Bekleme | 10 sn | 30 sn | 2 dk | 10 dk |
|---|---|---|---|---|
| Sonuç (ms / koptu mu) | | | | |

Aynısını başka uygulamaya geçip 1 dk sonra dönerek yap.

### 3. Kapasite

Olabildiğince çok telefon (en az **9**: eski 8 adres sınırını geçmek için).
Her telefon IP alıp sayfayı açabiliyor mu? Kutu kartında Wi-Fi / WebSocket sayıları, RAM en düşük değeri.

### 4. Güç (telefon OTG)

Kartı yalnızca telefondan besle, en az bir telefon bağlıyken 60 dk bırak.
Sonra "Açık kalma" ve "Son açılış nedeni": açık kalma beklenenden kısa ve neden
"güç verildi"/"düşük voltaj" ise telefon gücü kesmiştir. Telefonun OTG otomatik
kapatma ayarını ve pil düşüşünü not et.

### 5. Titreşim

"Titreşimi dene": Android'de titremeli; iPhone'da "desteklemiyor" yazmalı.

## Sonuç tablosu

| Telefon / OS / tarayıcı | Portal açıldı | Oyuna gir sonrası | Mobil veri açıkken | Kimlik kalıcı | Kilit 30 sn | Kilit 2 dk | Titreşim |
|---|---|---|---|---|---|---|---|
| | | | | | | | |

| Kapasite: telefon sayısı | IP alan | Sayfa açan | RAM en düşük |
|---|---|---|---|
| | | | |

| Güç kaynağı | Süre | Kesildi mi | Açılış nedeni |
|---|---|---|---|
| | | | |
