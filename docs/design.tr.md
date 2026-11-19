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
  src/      wifi_ap, dns_server, http_server, ws_hub; sonra core/ (oturum, profil, oda, motor) ve games/
web/        Vite + Preact + TS → tek HTML dosyası, gzip → firmware'e gömülür
docs/       tasarım, saha testleri
```

- **Kutu her zaman haklıdır.** Telefon niyet bildirir (`act`, `vote`); geçerliliğe
  ve sonuca firmware karar verir. Her durum değişikliğinde her telefona yalnızca
  kendi görünümü baştan gönderilir (fark değil; 15 kişide mesaj başına ~1 KB).
  Hile yapılamaz, kopan telefon tek mesajla eski haline döner.
- **Tek dosya web:** JS ve CSS HTML'e gömülü; telefon arayüzü tek TCP
  bağlantısıyla yükler (soket havuzu küçük). Arayüz firmware'e gömülür; tek
  `.bin` ile kurulum (ileride tarayıcıdan flaşlama).
- **Oyun modülü arayüzü** (oyun mantığı Arduino'ya/ESP-IDF'e bağlı değil, PC'de test edilir):

```cpp
struct Game {
  virtual void setup(const PlayerList&, const Options&, Rng&) = 0;
  virtual Result onAction(PlayerId, const Action&) = 0;   // geçersiz hamle reddedilir
  virtual void onTimer(TimerId) = 0;
  virtual void privateView(PlayerId, JsonObject out) const = 0;
  virtual void boardView(JsonObject out) const = 0;       // masa / yönetici sekmesi
  virtual bool finished() const = 0;
};
```

## 6. Kimlik ve kopmalar

- Telefon ilk açılışta rastgele kimlik üretir (`localStorage`); profil (ad,
  avatar, renk, istatistik) kutuda NVS'te.
- Her bağlantıda `hello{id}`; aynı kimlikle gelen yeni soket eskisini kapatır.
- 15 sn ses vermeyen soket kapatılır (kilitli telefon ping atmaz); masa
  görünümünde "bağlantı yok". Oyun beklemez: süre dolunca oy "çekimser", gece hamlesi "boş".
- Her aşama geçişinde oyun durumu kalıcı belleğe yazılır; güç kesilirse kaldığı yerden devam.

## 7. WebSocket mesajları

Faz 0 protokolünün tek kaynağı `firmware/src/ws_hub.h`. Oyun mesajları (Faz 1+):

| Yön | Mesaj |
|---|---|
| Telefon → kutu | `hello{id}` · `profile{name, avatar}` · `ready` · `start{game, options}` (yönetici) · `act{phaseId, kind, target}` · `vote{phaseId, target}` |
| Kutu → telefon | `view{seq, view}` · `buzz{pattern}` · `sound{id}` (yönetici / hoparlör) · `err{code}` |

`phaseId` geç gelen hamleyi reddeder; `seq` eski görünümü attırır.

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

| Faz | Teslim | Doğrulama |
|---|---|---|
| 0 | AP + captive portal + gömülü web + WebSocket ping/presence test sayfası | `docs/phase0-field-test.tr.md` (Android + iPhone, kilit/açılış, ≥ 9 telefon, OTG gücü) |
| 1 | Yönetici, profil, lobi, masa görünümü, Wi-Fi QR, token ile yeniden bağlanma, güç kesintisinde devam | Telefonlarla el testi + PC'de motor testleri |
| 2 | Vampir Köylü (4 rol) | PC'de 15 bot ile binlerce rastgele oyun (her oyun biter, kural ihlali yok) + gerçek oyun |
| 3 | Eklenti sürücüleri (hoparlör, LED, OLED, buton, pil), anlatıcı kayıtları, titreşim desenleri | Her eklenti tek başına açık/kapalı + eklentisiz tam oyun |
| 4 | İkinci oyun: Casus Kim | Motor arayüzünün genelliği |
| 5 | Açık kaynak dağıtımı: tarayıcıdan flaşlama (ESP Web Tools), sürüm `.bin`'leri, kurulum rehberi | Temiz bir kartı yalnız tarayıcıyla kurmak |
