# ESP32 Party Games

[English](README.md) · **Türkçe**

> Esas metin İngilizce [README.md](README.md)'dir; bu sayfa onun çevirisidir.

ESP32-S3 üzerinde internetsiz parti oyunları. Kart kendi Wi-Fi ağını açar;
masadaki herkes bağlanıp telefon tarayıcısından oynar — uygulama yok, internet
yok. İlk oyun: *Vampir Köylü* (Werewolf/Mafya tarzı); roller, gece hamleleri ve
oylar kutuda tutulur, kimse başkasının kartını göremez. Arayüz İngilizce ve
Türkçe. Tasarım: [docs/design.tr.md](docs/design.tr.md).

## İki kurulum

| | **Yalın** | **Kutu** |
|---|---|---|
| Donanım | Herhangi bir ESP32-S3 kart (≥ 8 MB flash), telefona USB-C kablo | Yalın + istenen eklentiler: hoparlör, LED halka, OLED, buton, LiPo pil, NFC çıkartma |
| Oyunu yöneten | Yönetici telefon (genelde kartı besleyen telefon): başlatma, süreler, ayarlar | Aynı; ek olarak kutu aşamayı/süreyi/QR'ı gösterir, anlatımı kendisi çalar |
| Firmware | Aynı dosya | Aynı dosya; her eklenti yönetici ayarlarından açılır |

Her eklenti isteğe bağlıdır ve telefonda karşılığı vardır; oyun hiçbirine muhtaç değildir.

## Durum

Faz 0 — bağlantı. Firmware erişim noktasını, captive portalı ve tasarımın
dayandığı şeyleri ölçen WebSocket test sayfasını çalıştırır: telefon ekranı
kilitlenip açılınca yeniden bağlanma süresi, 15 telefon kapasitesi, telefondan
USB güç. Saha testi listesi: [docs/phase0-field-test.tr.md](docs/phase0-field-test.tr.md).
Sırada ne var: [docs/roadmap.tr.md](docs/roadmap.tr.md).

## Derleme ve yükleme

Gerekenler: Python 3.13 veya daha eski bir sürümle çalışan
[PlatformIO Core](https://platformio.org/install/cli) (ESP-IDF derleyicisi Python
paketlerini 3.14'te henüz kuramıyor), Node.js 20+.

```sh
cd firmware
pio run -t upload      # önce web/ derlenir, gömülür, karta yüklenir
pio device monitor
```

Sonra `PartyGames-XXXX` Wi-Fi ağına bağlanın (şifre `partygames`) ve `http://192.168.4.1` adresini açın.

Çalışan bir karta karşı arayüz geliştirme: bilgisayarı kartın ağına bağlayın,
sonra `cd web && npm install && npm run dev`.

## Diller

Arayüz telefonun dili Türkçeyse Türkçe, değilse İngilizce açılır; EN/TR
düğmeleri değiştirir. Metinler `web/src/i18n.ts` içinde. Belgeler İngilizce
yazılır, Türkçe çevirileri yanlarında durur (`*.tr.md`).

## Lisans

MIT — bkz. [LICENSE](LICENSE).
