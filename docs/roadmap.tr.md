# Yol haritası

> Bu belge [roadmap.md](roadmap.md)'nin Türkçe çevirisidir; esas metin İngilizcedir.

Fazların, görevlerinin ve bitiş ölçütlerinin tek kaynağı. Tasarım ve kurallar:
[design.tr.md](design.tr.md). Duyuru: [launch-plan.tr.md](launch-plan.tr.md).

**Buradan başla:** Faz 0 saha testi → Faz 1. Duyurudan (Faz 4) önce Faz 1, 2 ve
3 bitmiş olmalı; eklentiler ve yeni oyunlar duyurudan sonra gelir.

| Faz | Hedef | Durum |
|---|---|---|
| 0 | Bağlantı: AP, captive portal, WebSocket test sayfası (artık `#diag`) | Kod bitti, saha testi açık |
| 1 | Lobi, profiller, yönetici, oyun motoru iskeleti | Kod bitti; bitiş kontrolü gerçek telefon ister |
| 2 | Vampir Köylü | Kod bitti, simülatör geçiyor; gerçek oyun açık |
| 3 | Dağıtım: tarayıcıdan yükleme, sürümler, README | Kod bitti; issue'lar, `main` ve ilk etiket açık |
| 4 | Duyuru | Başlanmadı |
| 5 | İsteğe bağlı eklentiler | Başlanmadı |
| 6 | Yeni oyunlar | Başlanmadı |

## Faz 0 — Bağlantı

- [x] 15 telefon için SoftAP (16 DHCP adresi, 0 ESP-NOW anahtarı, 32 soket)
- [x] Captive DNS, işletim sistemi bağlantı yoklamaları, MAC ile portal kabulü
- [x] Arayüz tek bir gzip'li HTML olarak gömülü; ön betik `web/`'i yeniden derler
- [x] `/ws` hello/ping/presence; 15 sn sessiz kalan soket kapatılır
- [x] İngilizce/Türkçe arayüz ve portal
- [ ] Gerçek telefonlarla saha testi: [phase0-field-test.tr.md](phase0-field-test.tr.md) (Android + iPhone, kilit/açılış, ≥ 9 telefon, 60 dk OTG gücü)
- [ ] Sonuçları saha testi tablolarına yaz ve Faz 1 için karar ver: sessizlik süresi, portal yöntemi, ekranı açık tutma gerekli mi, pil mi telefon gücü mü

**Bitiş:** sonuç tabloları dolu, Faz 1 kararları yazılı.

## Faz 1 — Lobi, profiller, yönetici

Firmware (`firmware/src/core/`; oyun mantığı ESP-IDF'e bağlı değil, PC'de çalışır):
- [x] Protokol v1 ([protocol.tr.md](protocol.tr.md), özeti `ws_hub.h`'de): `hello{id}`, `profile{name, avatar, color}`, `ready`, `start{game, options}`, `view{seq, view}` (her telefon yalnızca kendi tam görünümünü alır), `err{code}`, yönetici istekleri
- [x] Oturum: oyuncu kimliği ↔ soket; aynı kimlikle gelen yeni soket eskisinin yerini alır (`core/hub.*`)
- [x] NVS'te profiller: ad, emoji avatar, renk, istatistik (32 telefon hatırlanır, en eski kullanılan silinir)
- [x] Lobi: oyuncu listesi, hazır durumu, oyuncu çıkarma; oda doluysa çevrimdışı bir üyenin yeri kullanılır
- [x] Yönetici: "Yönetici ol"a ilk basan olur, başka telefona devredilir, BOOT (GPIO0) 5 sn basılı tutulunca sıfırlanır
- [x] Yönetici ayarları: Wi-Fi şifresi (yeniden başlatınca geçerli), oyun seçenekleri, süreler
- [x] Motor iskeleti: `Game` arayüzü (tasarım §5), aşama kimlikleri, zamanlayıcılar, her aşama geçişinde durumun NVS'e yazılması, güç kesintisinden sonra devam
- [x] `core/` için birim testleriyle PlatformIO `native` ortamı; kartsız arayüz için `host` ortamı + `npm run sim`

Web:
- [x] Görünüme göre ekranlar: profil, lobi, yönetici sekmesi, masa görünümü
- [x] Yönetici telefonunda Wi-Fi QR (`WIFI:T:WPA;S:…;P:…;;`) ve adres QR'ı
- [x] Ekranı açık tutma (sessiz döngü video; Wake Lock HTTPS ister)
- [x] `buzz{p}` için titreşim yardımcısı (Android; iPhone'da ekran yanıp söner)
- [x] Her yeni metin hem `en` hem `tr`'de

**Bitiş:** 6+ telefon katılıp profil oluşturur, yönetici deneme oyununu başlatır;
kilit/açılışta her oyuncunun kimliği korunur; elektrik kesilince herkes aynı lobiye döner.

## Faz 2 — Vampir Köylü

- [x] Tasarım §8 kuralları: Vampir, Köylü, Doktor, Gözcü; oyuncu sayısına göre rol sayıları
- [x] Herkesin görevi olan eş zamanlı gece; gece hızdan bağımsız 25–60 sn sürer
- [x] Vampirler birbirinin seçimini canlı görür; eşitlik rastgele bozulur
- [x] Çözüm sırası: koruma → öldürme → gözcü sonucu; doktor aynı hedefi tekrarlayamaz
- [x] Şafak, tartışma (yönetici erken bitirebilir), önce gizli sonra açık oylama, tek eşitlik turu, hüküm, kazanma kontrolü
- [x] Seçenekler: linçte rol açıklama, ruhlar rolleri görür, tartışma süresi, oylama süresi, şüphe istatistiği
- [x] Arayüz: basılı tutunca görünen rol kartı, gece görev ekranları, oylama, sonuçlar; yönetici için masa görünümü (isteğe bağlı sesli anlatım)
- [x] `native` ortamda bot simülatörü: 5–15 botla 5000 rastgele oyun; her oyun biter, hiçbir kural bozulmaz

**Bitiş:** simülatör geçer; arkadaşlarla tam bir gerçek oyun.

## Faz 3 — Dağıtım (duyurudan önce)

- [x] GitHub Actions: web + firmware derlemesi + native testler; etiket atılınca birleşik `.bin` (bootloader + bölümler + uygulama) Releases'a
- [x] ESP Web Tools ile GitHub Pages yükleme sayfası; README'de "Flash from browser" düğmesi
- [x] README: en üstte kapak görseli (`docs/media/hero.png`), 2 dakikalık hızlı başlangıç, paylaşım kapak görseli (`docs/media/social-preview.png`, depo ayarlarından seçilir); gerçek masa videosu duyuru çekimiyle gelir
- [x] `CONTRIBUTING.md`, "Kendi oyununu ekle" rehberi ([add-a-game.tr.md](add-a-game.tr.md))
- [ ] GitHub'da birkaç `good first issue`
- [ ] Özellik dalını `main`'e birleştir ve `main`'i varsayılan dal yap

**Bitiş:** boş bir kart yalnızca tarayıcıyla kurulur.

## Faz 4 — Duyuru

[launch-plan.tr.md](launch-plan.tr.md)'deki planı uygula.

**Bitiş:** plandaki her kanalda bir hafta içinde paylaşıldı; ilk haftanın issue'ları cevaplandı.

## Faz 5 — İsteğe bağlı eklentiler

Her eklenti varsayılan kapalı, yönetici ayarlarından açılır ve pini değiştirilir, telefonda karşılığı vardır (tasarım §4).
- [ ] I2S hoparlör + flash bölümünde anlatıcı kayıtları
- [ ] WS2812B LED halka: aşama rengi, geri sayım, oyuncu başına LED (parlaklık sınırlı)
- [ ] SH1106 OLED: kutunun üstünde Wi-Fi QR ve adres
- [ ] Arcade buton: devam / oylamaya geç
- [ ] ADC ile pil seviyesi
- [ ] NFC çıkartma rehberi (firmware gerekmez)
- [ ] Bağlantı şemalı ve kutulu yapım rehberi

**Bitiş:** her eklenti tek başına çalışır; hiçbiri yokken tam oyun oynanır.

## Faz 6 — Yeni oyunlar

- [ ] Casus Kim (motor arayüzünün genel olduğunu kanıtlar)
- Bekleyenler: butonlu bilgi yarışması, "Kim en çok…?", İki doğru bir yalan, Çiz-tahmin et, Sahtekâr kelime
