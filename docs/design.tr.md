# ESP32 Party Games — Tasarım

> Bu belge [design.md](design.md)'nin Türkçe çevirisidir; esas metin İngilizcedir.

ESP32-S3 üzerinde, internetsiz ve uygulamasız oynanan masa oyunu kutusu. Açık
kaynak (MIT): herkes kendi kartıyla kurabilsin diye tek firmware, isteğe bağlı
donanım. Arayüz İngilizce ve Türkçe: telefonun dili Türkçeyse Türkçe, değilse
İngilizce açılır; oyuncu EN/TR düğmesiyle değiştirebilir.

## 1. İki kurulum, tek firmware

| | **Yalın** | **Kutu** |
|---|---|---|
| Donanım | Herhangi bir ESP32-S3 (≥ 8 MB flash) + telefona USB-C kablo | Yalın + isteğe bağlı eklentiler (bölüm 4) |
| Oyunu yöneten | **Yönetici telefon**: oyun seçer, ayarlar, başlatır, süreleri yönetir; masaya açık bilgiyi (aşama, süre, oylar) gösterir, anlatıcı sesini çalar | Aynı yönetici telefon; ek olarak kutu aşamayı/süreyi/QR'ı kendisi gösterir, anlatımı kendi hoparlöründen çalar |
| Oyuncular | Kendi telefonları: gizli kart (rol, gece hamlesi, oy) | Aynı |

Kural: firmware eklentisiz açılır ve her oyun eklentisiz oynanır. Eklentiler
yalnızca deneyimi zenginleştirir; her birinin telefon karşılığı vardır.

**Kapasite:** 15 Wi-Fi istasyonu (ESP32-S3 SoftAP sınırı) → yönetici dahil 15
telefon. Bunun için üç ayar şart (`firmware/sdkconfig.defaults`):

- DHCP sunucusu varsayılan olarak 8 adres dağıtır; 9. telefon ağa girer ama IP
  alamaz → `CONFIG_LWIP_DHCPS_MAX_STATION_NUM=16`.
- SoftAP ile ESP-NOW 17 donanım anahtarını paylaşır; ESP-NOW varsayılanı 7
  anahtar ayırıp SoftAP'ı 10'a indirir → `CONFIG_ESP_WIFI_ESPNOW_MAX_ENCRYPT_NUM=0`.
- Telefon başına 1 WebSocket + sayfa yükleme bağlantıları → soket sınırı 32.

## 2. Yönetici

- Kutu ilk açılışta yöneticisizdir. Lobide **"Yönetici ol"** diyen ilk telefon
  yönetici olur; yetkisi o telefonun token'ına bağlanıp kalıcı bellekte (NVS) tutulur.
- "ESP'nin bağlı olduğu telefon" USB'den tanınamaz: telefon tarayıcısı USB
  üzerinden karta konuşamaz. Pratikte kabloyu takan kişi ilk bağlanan ve
  yönetici olan kişidir.
- Yönetici aynı zamanda oyuncu olabilir. Yönetim kontrolleri gizli bilgi açmaz;
  masa görünümü (herkese açık bilgi) yönetici telefonunda ayrı bir sekmedir.
- Yöneticilik başka telefona devredilebilir. Kayıp/kilitli durumda kart
  üzerindeki **BOOT tuşu 5 sn** basılı tutulunca yönetici sıfırlanır (ek donanım gerekmez).
- Yönetici ayarları: oyun ve kural seçenekleri, süreler, oyuncu çıkarma, Wi-Fi
  şifresi, eklentileri açıp kapatma ve pinlerini değiştirme.

## 3. Bağlanma

- Kutu `PartyGames-XXXX` ağını açar (WPA2, varsayılan şifre `partygames`;
  yönetici değiştirebilir). Yönetici telefonu (veya OLED) Wi-Fi QR kodunu gösterir.
- Sahte DNS her adı `192.168.4.1`'e çözer; telefonun "internet var mı" yoklaması
  portal sayfasına düşer, işletim sistemi giriş penceresini açar.
- Portalda **"Oyuna gir"** → kutu o telefonu (MAC ile) kabul edilmiş sayar; bundan
  sonra yoklamalara "internet var" cevabı verir. Amaç: telefon ağdan kaçmasın,
  mobil veriye dönmesin. Android'in HTTPS yoklaması yine başarısız olur; "sınırlı
  bağlantı" davranışı Faz 0 saha testinde ölçülür.
- Tek köken: **`http://192.168.4.1`**. Oyuncu token'ı `localStorage`'da köken
  başına durduğundan her bağlantı, QR ve yönlendirme bu adresi kullanır; ana
  makine adı kullanılmaz.
- HTTPS yok → güvenli bağlam yok: Wake Lock, service worker, push,
  `crypto.randomUUID` kullanılamaz. Ekranı açık tutmak için sessiz döngü video
  yöntemi, rastgelelik için `crypto.getRandomValues`.

## 4. İsteğe bağlı eklentiler

Hepsi varsayılan kapalı; yönetici ayarlarından açılır, pinleri oradan
değiştirilebilir. Aşağıdaki pinler N16R8 DevKitC-1 için varsayılandır.

| Eklenti | Oyunda | Yoksa | Varsayılan pin |
|---|---|---|---|
| I2S amfi (MAX98357A) + hoparlör | Anlatıcı sesi kutudan; kayıtlar flash'ta | Yönetici telefonu çalar | BCLK 4, LRCK 5, DIN 6 |
| WS2812B LED halka (≤ 16 LED) | Aşama rengi, geri sayım, oyuncu başına LED | Yönetici telefonu masa görünümü | DATA 14 |
| OLED 1.3" SH1106 (I2C) | Wi-Fi QR ve adres kutunun üstünde | Yönetici telefonu QR gösterir | SDA 8, SCL 9 |
| Arcade buton | Devam / oylamaya geç; yarışmada basma butonu | Yönetici telefonu butonları | 10 (GND'ye, iç pull-up) |
| LiPo + şarj/boost kartı | Telefon OTG'sinden bağımsız güç; pil seviyesi | Telefondan USB güç | ADC 1 (100k/100k bölücü) |
| NFC çıkartma (NTAG215) | Dokununca `http://192.168.4.1` açılır | QR / elle adres | — (pasif) |

Kullanılmaması gereken pinler: 0/3/45/46 (strapping; 0 = BOOT tuşu, yalnızca
okunur), 19/20 (USB), 26–32 (flash), 33–37 (oktal PSRAM), 43/44 (UART log).

Güç: LED halka tam parlaklıkta 1 A'e, hoparlör anlık 0,6 A'e çıkabilir. LED
parlaklığı yazılımda ~%20 ile sınırlanır; LED + hoparlör varsa pil önerilir.

## 5. Mimari

```
firmware/   PlatformIO + ESP-IDF 5.5 (Arduino yok: sdkconfig sınırları değişebilsin diye)
  src/      wifi_ap, dns_server, http_server, ws_hub (httpd bağdaştırıcısı), nvs_store, boot_button
  src/core/ json, hub (soketler ↔ oyuncular), room (profil, lobi, yönetici, oyun sunucusu), oyun arayüzü
  src/games/ registry, vampires
  src/host/ ana makine simülatörü: core/ + games/ stdin/stdout üzerinden (kartsız arayüz geliştirme)
  test/     native birim testleri ve bot simülatörleri (`pio test -e native`)
web/        Vite + Preact + TS → tek HTML dosyası, gzip → firmware'e gömülür
docs/       tasarım, protokol, saha testleri, yol haritası
```

- **Kutu her zaman haklıdır.** Telefon niyet bildirir (`act`, `vote`); geçerliliğe
  ve sonuca firmware karar verir. Her durum değişikliğinde her telefona yalnızca
  kendi görünümü baştan gönderilir (fark değil; 15 kişide mesaj başına ~1 KB).
  Hile yapılamaz, kopan telefon tek mesajla eski haline döner.
- **Tek dosya web:** JS ve CSS HTML'e gömülü; telefon arayüzü tek TCP
  bağlantısıyla yükler (soket havuzu küçük). Arayüz firmware'e gömülür; tek
  `.bin` ile kurulum, tarayıcıdan da (ESP Web Tools).
- **Oyun modülü arayüzü** (`firmware/src/core/game.h`; oyun mantığı Arduino'ya
  da ESP-IDF'e de bağlı değil, PC'de test edilir). Oyuncular odadaki koltuklardır
  (`pid` 0–14) ve oyun boyunca değişmez; bu yüzden oyun durumu pid'e göre dizinli
  düz bir yapıdır ve tek `memcpy` ile kaydedilir:

```cpp
class Game {
  virtual const char* setup(PlayerMask players, const json::Value& options) = 0;
  virtual const char* onAction(int pid, const json::Value& msg) = 0;  // hata kodu ya da nullptr
  virtual void onTimer() = 0;                                          // tek aşama sayacı
  virtual const char* adminSkip() = 0;
  virtual void privateView(int pid, json::Writer& out) const = 0;
  virtual void boardView(json::Writer& out) const = 0;                 // masa / yönetici sekmesi
  virtual bool finished() const = 0;
  virtual bool won(int pid) const = 0;
  virtual std::string save() const = 0;                                // her aşamada anlık kayıt
  virtual bool load(const uint8_t* data, size_t len) = 0;
};
```

  Oda oyuna bir `GameHost` verir: `rng()`, `setTimer(ms)`, `phaseChanged()`
  (NVS'e kayıt), `buzz(players, pattern)`, `online(pid)`. Oyun ekleme:
  [add-a-game.tr.md](add-a-game.tr.md).

## 6. Kimlik ve kopmalar

- Telefon ilk açılışta rastgele kimlik üretir (`localStorage`); profil (ad,
  avatar, renk, istatistik) kutuda NVS'te.
- Her bağlantıda `hello{id}`; aynı kimlikle gelen yeni soket eskisini kapatır.
- 15 sn ses vermeyen soket kapatılır (kilitli telefon ping atmaz); masa
  görünümünde "bağlantı yok". Oyun beklemez: süre dolunca oy "çekimser", gece hamlesi "boş".
- Her aşama geçişinde oyun durumu kalıcı belleğe yazılır; güç kesilirse kaldığı yerden devam.

## 7. WebSocket mesajları

Tek kaynak [protocol.tr.md](protocol.tr.md) (İngilizce asıl: [protocol.md](protocol.md)):
tüm mesajlar, hata kodları ve görünümün tam şeması. Özetle:

| Yön | Mesaj |
|---|---|
| Telefon → kutu | `hello{id}` · `ping{c}` · `profile{name, avatar, color}` · `ready{on}` · `claimAdmin` · `transfer{pid}` · `kick{pid}` · `settings{wifiPass}` · `restart` · `start{game, options}` · `skip` · `end` · `ack{phaseId}` · `act{phaseId, target}` · `vote{phaseId, target}` |
| Kutu → telefon | `welcome{proto}` · `pong{c}` · `view{seq, view}` · `buzz{p}` · `err{code}` |

`phaseId` geç gelen hamleyi reddeder; `seq` eski görünümü attırır. Yönetici
telefonundaki anlatım tarayıcının konuşma sentezini kullanır; kutu hoparlörü
(`sound{id}`) Faz 5'teki isteğe bağlı eklentiyle gelir.

## 8. Vampir Köylü kuralları

**Roller:** Vampir, Köylü, Doktor, Gözcü. Sonra: Avcı, Soytarı, Âşıklar.

| Oyuncu | 5–6 | 7–9 | 10–12 | 13–15 |
|---|---|---|---|---|
| Vampir | 1 | 2 | 3 | 3 |
| Doktor + Gözcü | 1 + 1 | 1 + 1 | 1 + 1 | 1 + 1 |

**Akış:** `LOBİ → ROL KARTI → GECE → ŞAFAK → TARTIŞMA → OYLAMA → HÜKÜM → (GECE | SONUÇ)`

- **Rol kartı:** kart basılı tutunca görünür; herkes "Gördüm" deyince gece başlar.
- **Gece:** herkes aynı anda oynar ve **herkesin ekranında görev vardır**
  (vampir: kurban, doktor: koru, gözcü: sorgula, köylü: şüphelini işaretle) →
  ekrana dokunmak kimseyi ele vermez. Vampirler birbirini ve seçimlerini canlı
  görür; eşitlikte berabere kalanlardan rastgele. Gece **en az 25, en çok 60 sn**:
  erken bitse de beklenir, yoksa bitiş hızı ipucu verir. Başta ve sonda herkesin
  telefonu aynı desende titrer (iPhone'da ekran rengi/ses).
- **Çözüm sırası:** doktorun koruması → vampirin hedefi (korunmuyorsa ölür) →
  gözcüye özel sonuç. Doktor aynı kişiyi iki gece üst üste koruyamaz; kendini koruyabilir.
- **Şafak:** "X öldü" / "kimse ölmedi"; ayara bağlı olarak köylü şüphe dağılımı (kim işaretledi gösterilmez).
- **Tartışma:** varsayılan 3 dk; yönetici erken bitirebilir.
- **Oylama:** yaşayan herkes birine ya da "kimse"ye oy verir; oylar herkes verene
  kadar gizli, sonra açık gösterilir. En çok oy alan linç edilir; eşitlikte
  yalnız berabere kalanlarla bir tur daha, yine eşitse linç yok.
- **Hüküm:** linç edilenin rolü açıklanır (ayar, varsayılan açık).
- **Kazanma:** tüm vampirler ölünce köy; yaşayan vampir ≥ yaşayan diğerleri olunca vampirler.
- **Ölüler** ruh olur; ayar açıksa (varsayılan) tüm rolleri görür, müdahale edemez.

## 9. Fazlar

Fazlar, görevleri ve bitiş ölçütleri [roadmap.tr.md](roadmap.tr.md)'de; duyuru [launch-plan.tr.md](launch-plan.tr.md)'de.
