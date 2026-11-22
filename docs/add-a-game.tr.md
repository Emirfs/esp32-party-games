# Kendi oyununu ekle

> English: [add-a-game.md](add-a-game.md). Esas metin İngilizcedir; bu sayfa onun çevirisidir.

Bir oyun, kutuda tek bir C++ sınıfı ve web arayüzünde birkaç ekrandan oluşur.
Kurallar ve tüm sırlar kutudadır; telefonlar yalnızca niyetlerini gönderir ve
kendilerine verilen görünümü çizer. Vampir Köylü (`firmware/src/games/vampires.*`,
testleri `firmware/test/` içinde, ekranları `web/src/` içinde) örnek alınacak
uygulamadır: bu rehberle birlikte okuyun.

Aşağıdaki örnek, uydurma bir `spy` oyunudur ("Casus Kim?": casus dışında herkes
aynı gizli kelimeyi alır). Kendi oyununuzla değiştirin.

## Dokunacağınız yerler

| Nerede | Ne |
|---|---|
| `docs/design.md` (+ `.tr.md`) | Kurallar: roller, fazlar, süreler, kazanma koşulu |
| `firmware/src/games/<id>.h`, `<id>.cpp` | `Game` uygulaması |
| `firmware/src/games/registry.cpp` | Oyunu kaydetme |
| `firmware/src/CMakeLists.txt` | `games/<id>.cpp` dosyasını `SRCS`'e ekleme |
| `firmware/test/test_<id>/` | Unity testleri ve bot simülatörü |
| `web/src/` | Oyunun görünümleri için ekranlar; metinler `web/src/i18n.ts` içinde (`en` ve `tr`) |
| `docs/protocol.md` (+ `protocol.tr.md`) | Ayarlar, fazlar, özel görünüm ve masa görünümü |

## 1. Önce kuralları yazın

Fazlara, sürelere, her fazda kimin ne bildiğine ve oyunun nasıl bittiğine karar
verin. Her faz için sorun: *biri gördüğünden, başkalarının ne kadar sürede
oynadığından ya da kimin ekrana dokunduğundan bir sır öğrenebilir mi?* Vampir
Köylü bunu gece herkese bir görev vererek (ekrana dokunmak kimseyi ele vermez),
geceyi en az 25 sn tutarak ve her telefonu aynı desende titreterek çözer.
Kuralları `docs/design.md` içine yeni bir bölüm olarak, çevirisiyle birlikte ekleyin.

## 2. `Game` arayüzü

Sözleşmenin tamamı `firmware/src/core/game.h` dosyasıdır. Oyunlar yalnızca
`core/`'a ve C++ standart kütüphanesine bağlıdır (ESP-IDF yok, Arduino yok);
böylece ESP32-S3 için, native testler için ve host simülatörü için aynı kod derlenir.

Oyuncular **pid** ile tanınır (oda koltuğu 0–14). Oturan pid'lerin kümesi, oyun
boyunca değişmeyen bir `PlayerMask` bit maskesidir: durumunuzu pid'e göre
dizinleyin, `hasPid()` / `countPids()` kullanın.

| Metot | Ne yapmalı |
|---|---|
| `info()` | Statik bir `GameInfo{id, minPlayers, maxPlayers}` döndürür. `id` protokol kimliğidir (`"spy"`); lobi oyunu min/maks ile listeler. |
| `setup(players, options)` | Oyuncu sayısını denetler (`"too_few"` / `"too_many"`), başlatma mesajındaki ayarları okur, eksik ya da aralık dışı değerleri varsayılana **sıkıştırır** (ayar yüzünden asla hata vermez), rolleri `host_.rng()` ile dağıtır ve ilk faza girer. Başarıda `nullptr` döndürür. |
| `onAction(pid, msg)` | Telefonun niyeti; `msg` mesajın tamamıdır (`{"t":"act"\|"vote"\|"ack", "phaseId":n, ...}`). Eskimiş ya da kurala aykırı her şeyi [protocol.tr.md](protocol.tr.md#hata-kodları) içindeki bir hata koduyla (`late`, `bad_msg`, `bad_target`, `not_allowed`) **durumu değiştirmeden** reddeder. Başarıda kaydeder ve `nullptr` döndürür; fazı erken bitirmek yalnızca hiçbir şey sızdırmıyorsa yapılır. |
| `onTimer()` | Faz zamanlayıcısı doldu: mevcut fazı bitirir. |
| `adminSkip()` | Yöneticinin "devam"ı: kuralların izin verdiği yerde fazı erken bitirir, aksi halde `"not_allowed"` döndürür. |
| `privateView(pid, out)` | Bu pid'in bilebileceği her şeyi, fazlasını değil, tek bir JSON nesnesi olarak yazar. İzleyiciler (oyunda olmayan pid'ler) için de çağrılır. |
| `boardView(out)` | Yalnızca herkese açık masa bilgisini tek bir JSON nesnesi olarak yazar. |
| `finished()` / `won(pid)` | Oyun bitti mi; oturan bu oyuncunun tarafı kazandı mı (profil istatistikleri için). |
| `save()` / `load(data, len)` | Tüm durumun (ayarlar dahil) ikili anlık görüntüsü ve tersi; `load()` başka sürüm ya da boyuttaki bir blob için `false` döndürür. |

Oda her kabul edilen hamleden, zamanlayıcıdan ve atlamadan sonra her telefonun
görünümünü yeniden üretir; görünümleri siz göndermezsiniz.

### Durum: pid'e göre dizinlenmiş tek bir düz struct

Tüm durumu tek bir trivially copyable struct içinde tutun: `kMaxPlayers`
boyutlu sabit diziler, küçük tamsayı türleri, alt türü açıkça belirtilmiş
enum'lar; işaretçi, `std::string` ya da `std::vector` yok. Böylece `save()` bir
sürüm etiketi, bir boyut ve bir `memcpy`'dir; `load()` geri kopyalamadan önce
ikisini de denetler:

```cpp
struct State {
  Phase phase;
  uint32_t phaseId;          // bumped on every phase change
  PlayerMask seated, alive;
  int8_t spy;                // pid
  int8_t vote[kMaxPlayers];  // -2 = not voted, -1 = nobody
  // ...
};
static_assert(std::is_trivially_copyable<State>::value, "save() copies State as bytes");
```

Struct her değiştiğinde kayıt sürümünü artırın. Durumda **mutlak zaman
tutmayın**: oda, bekleyen zamanlayıcının kalan süresini anlık görüntüyle
birlikte kaydeder ve `load()`'dan sonra yeniden kurar. Struct'ı küçük tutun
(birkaç yüz bayt); her faz değişiminde flash'a yazılır.

### Fazlar ve `phaseId`

Her faz değişimi, `phaseId`'yi artıran, faz zamanlayıcısını kuran ve host'a
haber veren tek bir yardımcıdan geçer:

```cpp
void Spy::enter(Phase p) {
  s_.phase = p;
  ++s_.phaseId;                 // phones echo it; older ids get "late"
  host_.setTimer(phaseMs(p));   // 0 = no timer
  host_.phaseChanged();         // the room snapshots the game to flash
}
```

### `GameHost` hizmetleri

- `rng()` — tek rastgelelik kaynağı (`next()`, `below(n)`, `shuffle()`). Asla
  `rand()` ya da saat kullanmayın: testler ve bot simülatörü tekrarlanabilir
  oyunlar için onu sabit tohumla başlatır.
- `setTimer(ms)` — tek bir faz zamanlayıcısı; `onTimer()` bir kez çalışır. Yeni
  çağrı bekleyenin yerini alır, `0` iptal eder. Telefonlar kalan süreyi `endsIn` olarak görür.
- `phaseChanged()` — her faz değişiminden sonra çağırın; elektrik kesilirse oyun aynı fazdan sürer.
- `buzz(players, pattern)` — telefonları `"[200,100,200]"` gibi açık/kapalı
  milisaniyelerden oluşan bir JSON dizisiyle titretir (iPhone'da ekran yanıp
  söner). Titreşim bir rolü ele verebilecekse herkesi aynı desenle titretin.
- `online(pid)` — yalnızca "bağlantı koptu" rozetleri için. **Çevrimdışı
  oyuncuları asla beklemeyin**: süre dolunca verilmemiş oy "çekimser", verilmemiş
  gece seçimi "yok" sayılır.

### Gizli bilgi kuralı

- Her sır kutuda belirlenir ve kutuda saklanır.
- `privateView(pid)` yalnızca o oyuncunun bilebileceklerini içerir: kendi rolü,
  kuralların açıkladığı müttefikleri, kendi seçimleri ve sonuçları. Başkalarının
  sırları hiç yer almaz (`null` da değil, gizlenmiş hali de değil).
- `boardView()` tamamen herkese açıktır: yönetici onu **oynarken** görür, oyuna
  katılmayan üyeler de görür. "Kim hamle yaptı" sayıları yalnızca herkesin
  görevi varsa uygundur.
- İzleyiciler (`seated: false`) herkese açık alt kümeyi alır.
- `end` fazında her görünümde her şey açıklanır.
- Her pid'in görünümünü üretip hiçbir sırrın görünmediğini denetleyen bir test yazın (bkz. 4. adım).

## 3. Oyunu kaydedin

- `firmware/src/games/registry.cpp`: başlık dosyanızı ekleyin, oyun listesine
  `&Spy::kInfo` ekleyin ve `create()` içine `new Spy(host)` döndüren bir dal ekleyin.
- `firmware/src/CMakeLists.txt`: `SRCS`'e `"games/spy.cpp"` ekleyin (ESP-IDF
  derlemesi kaynakları tek tek listeler; PlatformIO `native` ve `host` ortamları
  `src/games/` altındaki her şeyi kendiliğinden alır).

Artık lobi oyunu yöneticiye sunar (`view.games`) ve `"game":"spy"` içeren `start` onu oluşturur.

## 4. Testler

Testler `firmware/test/test_<id>/` altındaki Unity programlarıdır ve
`cd firmware && pio test -e native` ile çalışır (yalnızca sizinkiler için
`-f test_spy` ekleyin). Oyuna, sabit tohumlu bir `Rng` taşıyan ve zamanlayıcıyı,
`phaseChanged()` çağrılarını ve titreşimleri kaydeden küçük bir sahte `GameHost`
verin; sonra oyunu `setup()`, `onAction()` (mesajları `json::Doc` ile
ayrıştırın), `onTimer()` ve `adminSkip()` ile yürütün.

En azından şunları kapsayın:
- oyuncu sayısı sınırları ve ayarların sıkıştırılması;
- tasarım bölümünüzdeki her kural, eşitlikler ve uç durumlar dahil;
- eski `phaseId` → `late`, kurala aykırı hedef → `bad_target`, yanlış faz → `not_allowed`; reddedilen hamlelerin hiçbir şeyi değiştirmediği;
- `save()` → `load()` ile yeni bir örneğe aktarılan oyunun aynı görünümleri verdiği; kesilmiş ya da yanlış sürümlü blob'un reddedildiği;
- **sızıntı yok**: her pid için `privateView()` ve `boardView()` ayrıştırılıp sırların yalnızca izin verilen yerlerde göründüğü.

**Bot simülatörü.** Rastgele oyuncu sayıları (`min` ile `max` arası) ve rastgele
tohumlarla binlerce oyun oynayan bir test ekleyin. Her bot kendi özel
görünümünü okur, rastgele bir geçerli hamle seçer (bazen de reddedilmesi gereken
geçersiz bir hamle), test rastgele zamanlayıcıları ve yönetici atlamalarını
tetikler, arada durumu `save()`/`load()` ile gidip getirir. Her adımdan sonra
değişmezlerin korunduğunu (örn. ölüler hamle yapmaz, sayılar tutar) ve her
oyunun bir adım sınırı içinde `finished()` durumuna ulaştığını doğrulayın.
Vampir Köylü'nün simülatörü iyi bir şablondur.

## 5. Web ekranları

Arayüz ekranı görünümden seçer: `game` varsa ve `game.seated` doğruysa
`game.id` ve `game.phase`'e göre sizin oyununuzun ekranları gösterilir; oyuna
katılmayan üyeler ve yöneticinin masa sekmesi `board` alır.

- Her faz için bileşenleri Vampir Köylü ekranlarının yanına ekleyin; yalnızca
  görünümün söylediğini çizin, telefonda oyun mantığı tutmayın ve niyetleri
  güncel `phaseId` ile gönderin.
- Başlatma ayarlarını yöneticinin oyun seçicisine ekleyin.
- Oyuncunun gördüğü her metin `web/src/i18n.ts` içinde hem `en` hem `tr` olarak eklenir.
- Kartsız deneyin: `cd firmware && pio run -e host`, sonra `cd ../web && npm run sim`
  ve `PG_BOX=http://localhost:7420 npm run dev`; her oyuncu için bir sekme açın (`?dev=ada`, `?dev=bob`, …).

## 6. Protokolü belgeleyin

[protocol.md](protocol.md) dosyasına oyununuz için, Vampir Köylü bölümüne
benzeyen bir bölüm ekleyin: kimlik ve oyuncu aralığı, başlatma ayarları tablosu
(varsayılan ve aralık), fazlar ve süreler, `skip`'in hangi fazlarda çalıştığı,
özel görünüm (yorumlu `jsonc`) ve masa görünümü. `protocol.tr.md` dosyasını aynı
commit'te güncelleyin (metni çevirin, kodu ve JSON'u aynen bırakın) ve oyunu
README'ye ekleyin.

## Kontrol listesi

- [ ] Kurallar `docs/design.md` ve `docs/design.tr.md` içinde
- [ ] `Game` uygulandı; durum tek bir trivially copyable struct; `save`/`load` sürümlü
- [ ] Rastgelelik yalnızca `host_.rng()`'den; hiçbir şey çevrimdışı oyuncuyu beklemiyor
- [ ] Görünümler sızıntı testinden geçiyor; masa görünümü herkese açık
- [ ] `registry.cpp` ve `src/CMakeLists.txt` içinde kayıtlı
- [ ] Unity testleri ve bot simülatörü `pio test -e native` ile geçiyor
- [ ] Ekranlar ve `en`/`tr` metinleri hazır; hiçbir eklenti olmadan yalın kartta oynanabiliyor
- [ ] `docs/protocol.md` ve `docs/protocol.tr.md` güncellendi
