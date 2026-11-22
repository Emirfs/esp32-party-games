# WebSocket protokolü v1

> English: [protocol.md](protocol.md). Esas metin İngilizcedir; bu sayfa onun çevirisidir.

Her telefon için `ws://192.168.4.1/ws` adresinde bir WebSocket, JSON metin
çerçeveleri; telefondan gelen mesaj en çok 1024 bayt. Her mesajın bir tür alanı
`t` vardır. Firmware tarafı `firmware/src/core/` içindedir (`hub.cpp`,
`room.cpp`, `games/*`); `firmware/src/ws_hub.h` içindeki özet ve
`web/src/connection.ts` içindeki istemci bu dosyayla aynı commit'te değişir.

**Kutu her zaman haklıdır.** Telefon yalnızca bir niyet bildirir; kutu onu
denetler ve her durum değişiminden sonra her telefona kendi eksiksiz `view`'ini
gönderir. Fark (diff) yoktur: yeniden bağlanan telefon tek mesajla toparlanır.

**Oyuncu kimlikleri.** Telefonun `clientId`'si (rastgele,
`[A-Za-z0-9_-]{1,23}`, `localStorage` içinde `pg.clientId` olarak saklanır) bir
sırdır: onu bilen o oyuncudur. Başka telefonlara asla gönderilmez. Diğer herkes
oyuncunun herkese açık `pid`'ini (oda koltuğu 0–14) görür.

## Bağlantı

| Telefon → kutu | Anlamı |
|---|---|
| `{"t":"hello","id":"<clientId>"}` | Her soketteki ilk mesaj. Aynı kimlikle açılan ikinci soket eskisini kapatır. |
| `{"t":"ping","c":<ms>}` | 3 sn'de bir. 15 sn sessiz kalan soketi kutu kapatır. |

| Kutu → telefon | Anlamı |
|---|---|
| `{"t":"welcome","proto":1}` | hello'ya yanıt; ardından bir `view` gelir. |
| `{"t":"pong","c":<ms>}` | `c`'nin yankısı. |
| `{"t":"view","seq":<n>,"view":{…}}` | Telefonun tüm durumu. `seq` artar; son alınandan küçük `seq`'li görünümü atın. |
| `{"t":"err","code":"<code>"}` | Son istek reddedildi; hiçbir şey değişmedi. |
| `{"t":"buzz","p":[<ms on>,<ms off>,…]}` | Şimdi titre (iPhone: ekranı yakıp söndür). |

`hello`'dan önce gelen mesajlar, ping'ler de dahil, `err` `hello_first` alır;
telefon bunun üzerine `hello`'yu yeniden gönderir (hello'su kaybolmuştur, ör. kutu yeniden başladı).

## Lobi ve yönetici istekleri

| Mesaj | Kim | Etkisi |
|---|---|---|
| `{"t":"profile","name":"Ada","avatar":"🦊","color":3}` | herkes | Profili oluşturur ya da düzenler ve odaya katılır. Ad 1–16 karakter, avatar tek emoji (≤ 16 bayt), renk 0–11. |
| `{"t":"ready","on":true}` | üye, lobi | "Sonraki oyunda varım". |
| `{"t":"claimAdmin"}` | üye | Yalnızca kutunun yöneticisi yokken. |
| `{"t":"transfer","pid":3}` | yönetici | Yönetici yetkisini 3 numaralı üyeye devreder. |
| `{"t":"kick","pid":3}` | yönetici, lobi | 3 numaralı üyeyi odadan çıkarır (yeniden katılabilir). |
| `{"t":"settings","wifiPass":"…"}` | yönetici | Yeni Wi-Fi şifresi (8–63 yazdırılabilir ASCII karakter), yeniden başlatmadan sonra geçerli olur. |
| `{"t":"restart"}` | yönetici | Kutuyu yeniden başlatır. |
| `{"t":"start","game":"vampires","options":{…}}` | yönetici, lobi | Hazır olan tüm üyelerle bir oyun başlatır. |
| `{"t":"skip"}` | yönetici, oyun | Kuralların izin verdiği yerde mevcut fazı erken bitirir. |
| `{"t":"end"}` | yönetici, oyun | Lobiye döner. Biten oyun istatistiklere sayılır, yarıda kesilen sayılmaz. |

## Oyun istekleri

`phaseId` mevcut görünümden alınır; eskimiş olan `err` `late` alır.

| Mesaj | Etkisi |
|---|---|
| `{"t":"ack","phaseId":n}` | Rol kartında "Gördüm". |
| `{"t":"act","phaseId":n,"target":pid}` | Gece seçimi (görev türü rolden anlaşılır). Gece bitene kadar değiştirilebilir. |
| `{"t":"vote","phaseId":n,"target":pid}` | Oy; `-1` = kimse. Oylar açıklanana kadar değiştirilebilir. |

## Hata kodları

`hello_first`, `bad_msg`, `no_profile`, `bad_name`, `bad_avatar`,
`bad_color`, `room_full`, `not_admin`, `admin_taken`, `bad_pid`, `in_game`,
`no_game`, `bad_game`, `too_few`, `too_many`, `bad_pass`, `late`,
`bad_target`, `not_allowed`.

## Görünüm

*İsteğe bağlı* diye işaretlenmedikçe her anahtar her zaman vardır.

```jsonc
{
  "me": 2,                 // my pid, -1 = not a member yet (show the profile screen)
  "profile": {"name": "Ada", "avatar": "🦊", "color": 3, "played": 4, "won": 2},  // or null
  "admin": 0,              // admin's pid, -1 = nobody (offline admins keep it)
  "isAdmin": false,
  "full": false,           // room has 15 members; a new profile is refused
  "players": [             // room members, by pid
    {"pid": 0, "name": "Ada", "avatar": "🦊", "color": 3, "online": true, "ready": true, "inGame": false}
  ],
  "games": [{"id": "vampires", "min": 5, "max": 15}],
  "endsIn": 23000,         // ms left on the game's phase timer, -1 = none
  "box": {"ssid": "PartyGames-1A2B", "wifiPass": "partygames"},  // optional: admin only
  "game": {…},             // optional: a game exists (running or finished); the game's private view
  "board": {…}             // optional: admin, or a member who is not in the game; public table view
}
```

Kod bloklarındaki yorumlar İngilizce kaynakla birebir aynı tutulur. Kısaca:
`me` benim pid'im (-1 = henüz üye değil, profil ekranını göster); `profile` ya
da `null`; `admin` yöneticinin pid'i (-1 = kimse; çevrimdışı yönetici yetkisini
korur); `full` odada 15 üye var, yeni profil reddedilir; `players` pid'e göre
oda üyeleri; `endsIn` oyunun faz zamanlayıcısında kalan ms (-1 = yok); `box`
isteğe bağlı, yalnızca yöneticiye; `game` isteğe bağlı, bir oyun varsa (sürüyor
ya da bitmiş) oyunun özel görünümü; `board` isteğe bağlı, yöneticiye ya da
oyunda olmayan üyeye herkese açık masa görünümü.

Ekran görünümden çıkar: `me == -1` → profil; `game` var ve `game.seated` →
oyun ekranları; aksi halde lobi (başkaları oynarken `board` ile birlikte).

## Vampir Köylü (`"vampires"`)

Kurallar: [design.tr.md §8](design.tr.md#8-vampir-köylü-kuralları). 5–15 oyuncu.

Başlatma ayarları (eksik ya da aralık dışı → varsayılan):

| Ayar | Varsayılan | Aralık |
|---|---|---|
| `revealRole` (linç edilen oyuncunun rolü gösterilir) | `true` | bool |
| `ghostsSeeRoles` | `true` | bool |
| `suspicion` (şafakta köylülerin şüphe sayıları gösterilir) | `true` | bool |
| `discussionSec` | `180` | 30–600 |
| `voteSec` | `60` | 20–180 |

Fazlar: `role` → `night` → `dawn` → `discussion` → `vote` → `verdict` → `night`
… → `end`. Eşit biten oylama `vote` → `verdict` (`runoff: true` ile) → `vote`
(2. tur) → `verdict` şeklinde ilerler. Roller: `vampire`, `villager`, `doctor`,
`seer`. Süreler: rol kartı ≤ 90 sn, gece 25–60 sn, şafak 8 sn, tartışma ve oylama
ayarlardan, hüküm 8 sn. Yöneticinin `skip`'i `night` ve `end` dışındaki her fazda çalışır.

Özel görünüm (`game`):

```jsonc
{
  "id": "vampires",
  "phase": "night",        // role | night | dawn | discussion | vote | verdict | end
  "phaseId": 7,
  "round": 2,              // night number, 1-based
  "seated": true,          // false: spectator (only id/phase/phaseId/round/seated/players/winner)
  "options": {"revealRole": true, "ghostsSeeRoles": true, "suspicion": true, "discussionSec": 180, "voteSec": 60},
  "players": [             // everyone in the game
    {"pid": 0, "alive": true, "role": "vampire"}   // "role" only when this phone may know it
  ],
  "role": "seer",          // seated only: my role
  "alive": true,           // seated only
  "acked": false,          // role phase: did I tap "Got it"
  "ackCount": 3, "ackTotal": 7,  // role phase
  "task": {"kind": "inspect", "targets": [0, 1, 4], "pick": -1},  // night, alive: kill | protect | inspect | suspect
  "allies": [{"pid": 4, "pick": 1}],             // night, vampire: the other vampires and their picks
  "seen": [{"pid": 1, "vampire": false}],        // seer: every result so far
  "dawn": {"died": 3, "suspicion": [{"pid": 1, "n": 2}]},  // dawn and discussion; died -1 = nobody; suspicion only with the option
  "vote": {"round": 1, "candidates": [0, 1, 4], "mine": -2, "cast": 2, "voters": 6},  // vote; mine -2 = not voted, -1 = nobody
  "tally": {"round": 1, "votes": [{"voter": 0, "target": 4}], "lynched": 4, "role": "vampire", "runoff": false},  // verdict; abstainers are absent; role only with revealRole
  "winner": "village"      // end only: village | vampires; every role is then in players
}
```

Kısaca: `round` 1'den başlayan gece numarası; `seated: false` izleyici demektir
(yalnızca id/phase/phaseId/round/seated/players/winner); `players` oyundaki
herkes, `role` yalnızca bu telefonun bilebileceği durumda; `role` ve `alive`
yalnızca oturan oyuncuda; `acked`, `ackCount`, `ackTotal` rol fazında ("Gördüm"e
dokundum mu, kaç kişi dokundu); `task` gece ve canlıyken (`kill` | `protect` |
`inspect` | `suspect`); `allies` gece, vampirde: diğer vampirler ve seçimleri;
`seen` gözcüde: şimdiye kadarki tüm sonuçlar; `dawn` şafak ve tartışmada
(`died` -1 = kimse; `suspicion` yalnızca ayar açıkken); `vote` oylamada
(`mine` -2 = oy vermedim, -1 = kimse); `tally` hükümde (çekimserler yer almaz;
`role` yalnızca `revealRole` ile); `winner` yalnızca `end`'de (`village` |
`vampires`; o zaman tüm roller `players` içindedir).

`players` içinde rol görünürlüğü: kendi rolüm; vampirsem diğer vampirler;
`revealRole` açıksa linç edilenlerin rolleri; ölüysem ve `ghostsSeeRoles`
açıksa tüm roller; `end`'de herkes için tüm roller. Gözcünün sonuçları yalnızca
`seen` içindedir.

Masa görünümü (`board`, herkese açık): `id`, `phase`, `phaseId`, `round`,
`players` (roller yalnızca herkese açık olduğu yerde: `revealRole` ile linç
edilenler, `end`'de hepsi), `acted` ve `actors` (gece: kaç canlı oyuncunun seçim
yaptığı, asla neyi seçtiği değil; rol fazı: "Gördüm"ler), `dawn`, `vote`
(`mine` olmadan), `tally`, `winner`.
