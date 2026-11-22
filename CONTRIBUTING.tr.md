# Katkıda bulunma

[English](CONTRIBUTING.md) · **Türkçe**

> Esas metin İngilizce [CONTRIBUTING.md](CONTRIBUTING.md)'dir; bu sayfa onun çevirisidir.

Yardımınız için teşekkürler! Yeni oyunlar, çeviriler, gerçek oyun gecelerinden
hata raporları ve donanım notları; hepsi memnuniyetle karşılanır. Önce
[CONTEXT.md](CONTEXT.md) (dosya haritası ve sözleşmeler),
[docs/design.tr.md](docs/design.tr.md) ve [docs/roadmap.tr.md](docs/roadmap.tr.md)
dosyalarını okuyun. `good first issue` etiketli konular iyi bir başlangıçtır;
oyun mu eklemek istiyorsunuz? [docs/add-a-game.tr.md](docs/add-a-game.tr.md)'yi okuyun.

## Kurulum

| Araç | Sürüm | Ne için |
|---|---|---|
| [PlatformIO Core](https://platformio.org/install/cli) | Python 3.13 veya daha eskisiyle | firmware, native testler, host simülatörü |
| Node.js | 20+ | web arayüzü (Vite + Preact + TypeScript) |
| Bilgisayar için C++ derleyicisi | C++17 destekli g++ | `native` ve `host` ortamları (Linux/macOS: sistemdeki g++ veya clang; Windows: `PATH`'te MinGW-w64, örn. WinLibs veya MSYS2) |
| ESP32-S3 kart | ≥ 8 MB flash | isteğe bağlı: Wi-Fi dışında her şey bilgisayarda çalışır |

```sh
git clone https://github.com/Emirfs/esp32-party-games
cd esp32-party-games/web && npm ci
cd ../firmware
pio test -e native     # birim testleri + bot simülatörü
pio run -e host        # arayüz çalışması için host simülatörü
pio run                # firmware (önce web/ derlenip gömülür)
pio run -t upload      # karta yükle
```

Kart olmadan arayüz geliştirme:
[README → Kart olmadan arayüz geliştirme](README.tr.md#kart-olmadan-arayüz-geliştirme).
PlatformIO yanlış Python sürümüyle kurulup bozulduysa
`~/.platformio/penv/.espidf-5.5.3` klasörünü silip yeniden derleyin; repo
klasörünü taşıdıysanız `firmware/.pio` klasörünü silin.

## Repo kuralları

- **Önce İngilizce.** Kod, tanımlayıcılar, yorumlar, commit'ler, issue'lar ve PR'lar İngilizcedir.
- **İki dilli arayüz.** Oyuncunun gördüğü her metin `web/src/i18n.ts` içinde hem
  `en` hem `tr` olarak eklenir (captive portalın kendi satır içi tablosu
  `web/public/portal.html` içindedir). Varsayılan ve yedek dil İngilizcedir.
  Türkçe bilmiyor musunuz? En iyi tahmininizi yazın ve PR'da belirtin; bir
  inceleyen düzeltir.
- **İki dilli belgeler.** Esas metin İngilizce `X.md`'dir; onu değiştiren commit
  `X.tr.md` çevirisini de günceller (ya da PR'da çeviri için yardım isteyin).
- **Kutu her zaman haklıdır.** Oyun kuralları ve gizli bilgi firmware'dedir. Telefon
  yalnızca niyetini bildirir ve yalnızca kendi görünümünü alır; bir telefona,
  oyuncusunun bilmemesi gereken hiçbir şey gönderilmez.
- **Eklentiler isteğe bağlıdır.** Hoparlör, LED halka, OLED, buton, pil ve NFC
  ekstradır: firmware yalın kartta açılmalı, her oyun yalın kart ve telefonlarla oynanabilmelidir.
- **Protokol değişiklikleri tek commit'te.** [docs/protocol.md](docs/protocol.md)
  (ve çevirisi), firmware (`firmware/src/core/`, `firmware/src/ws_hub.h` içindeki
  özet) ve istemci (`web/src/connection.ts`) birlikte değişir.
- **Kapasite ayarlarını düşürmeyin:** `firmware/sdkconfig.defaults` içindeki DHCP
  kiraları, soket ve ESP-NOW anahtar sayıları 15 telefon testi yeniden yapılmadan düşürülmez.
- **Oyun mantığı taşınabilir kalır.** `firmware/src/core/` ve `firmware/src/games/`
  yalnızca C++ standart kütüphanesine ve birbirine bağlıdır — ESP-IDF yok — böylece
  native testlerde ve host simülatöründe çalışırlar.
- Açık kaynak (MIT): repoda gizli bilgi, kişisel veri veya makineye özgü yol olmaz.
- Satır sonları LF.

## Testler

- `cd firmware && pio test -e native` — `core/` ve her oyun için Unity testleri,
  ayrıca bot simülatörü (5–15 botla binlerce rastgele oyun: her oyun biter, hiçbir
  kural çiğnenmez). Testler `firmware/test/<test_adı>/` içindedir.
- `cd web && npm run build` — arayüzün tip denetimi ve üretim derlemesi.
- `cd firmware && pio run` — firmware ESP32-S3 için hâlâ derleniyor mu.
- Telefonların gördüğü bir şeyi mi değiştirdiniz? Host simülatöründe her oyuncu
  için bir sekmeyle (`?dev=ada`, `?dev=bob`, …), kartınız varsa gerçek telefonlarla deneyin.

CI bunların hepsini her push ve pull request'te çalıştırır.

## Commit'ler

`git log`'daki gibi [Conventional Commits](https://www.conventionalcommits.org/):

```
feat(firmware): add SoftAP, captive portal server and WebSocket hub

- Plain ESP-IDF 5.5 so sdkconfig can raise DHCP leases (8 -> 16), sockets and SoftAP slots.
- /ws hello/ping/presence hub that closes sockets silent for 15 s.
```

- Türler: `feat`, `fix`, `docs`, `refactor`, `test`, `build`, `ci`, `chore`.
- Yararlıysa kapsam: `firmware`, `web` ya da `vampires` gibi bir oyun kimliği.
- Konu satırı emir kipinde, küçük harfle, sonunda nokta olmadan; gövde ne ve neden
  olduğunu anlatan `- ` maddeleri.
- Uyumu bozan değişiklikler: `tür!:` ve bir `BREAKING CHANGE:` alt satırı (örneğin
  değişen bir Wi-Fi varsayılanı ya da protokol sürümü).
- Her commit tek bir mantıksal değişiklik; bir belge ve çevirisi aynı commit'te.

## Pull request kontrol listesi

- [ ] `pio test -e native` geçiyor; yeni oyun mantığının testleri var.
- [ ] `web/` içinde `npm run build` geçiyor.
- [ ] Oyuncunun gördüğü her yeni metin hem `en` hem `tr` olarak var.
- [ ] Belgeler `.tr.md` çevirileriyle birlikte değişti.
- [ ] Protokol değişiklikleri: `docs/protocol.md`, firmware ve `web/src/connection.ts` tek commit'te.
- [ ] Gizli bilgi kutuda kalıyor; görünümler her telefona yalnızca oyuncusunun bilebileceğini gönderiyor.
- [ ] Yalın kartta çalışıyor: hiçbir eklenti şart değil.
- [ ] `sdkconfig.defaults` içindeki kapasite ayarları düşürülmedi.
- [ ] Telefonlarda ya da host simülatöründe denendi (PR'da hangisi olduğunu yazın).

## Sürümler (bakımcılar için)

1. `vX.Y.Z` etiketi push edin (`-rc.N` ekli etiketler ön sürüm olur). CI sürümü
   firmware'e aktarır (`PG_VERSION` → `firmware/CMakeLists.txt` içindeki
   `PROJECT_VER`), her şeyi derler, `esp32-party-games-vX.Y.Z.bin` (bootloader +
   bölüm tablosu + uygulama, 0 adresine yazılır) ve yalnız uygulamayı içeren
   `firmware.bin` dosyalarını GitHub Releases'a yükler, tarayıcıdan yükleme
   sayfasını (`flasher/`) üretilen `manifest.json` ile GitHub Pages'e yayınlar.
2. Repoda bir kerelik ayar: Settings → Pages → Source: **GitHub Actions**; ayrıca
   Settings → Environments → `github-pages` → izin verilen dal ve etiketlere `v*` etiket kuralını ekleyin.
