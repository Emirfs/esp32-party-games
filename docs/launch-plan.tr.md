# Duyuru planı

> Bu belge [launch-plan.md](launch-plan.md)'nin Türkçe çevirisidir; esas metin İngilizcedir.

Çoğu kanal bir projeye tek bir gerçek ilgi şansı verir. Ziyaretçi birkaç dakika
içinde oyun oynayabilecek hale gelmeden, repo bir bağlantı testiyken duyurma.

## Tek cümle

> Jackbox-style party games with no app and no internet, on a $5 ESP32.
> (Uygulamasız, internetsiz, 5 dolarlık bir ESP32 üzerinde Jackbox tarzı parti oyunları.)

## Duyurudan önce hazır olmalı (yol haritası Faz 1–3)

| Hazırlık | Neden |
|---|---|
| Oynanabilir Vampir Köylü | İnsanlar bağlantı testini değil oyunu paylaşır |
| "Flash from browser" (GitHub Pages'te ESP Web Tools) | En büyük engel PlatformIO kurmak; tak, tıkla, oyna |
| 30 saniyelik video | Kafede 6–8 kişi telefonla bağlanıyor, uygulama yok, internet yok, vampir gece birini öldürüyor. İkna eden görüntü bu |
| README'nin en üstünde GIF/video, ardından 2 dakikalık hızlı başlangıç | Ziyaretçi birkaç saniyede karar verir |
| Hazır `.bin` sürümleri ve paylaşım kapak görseli | Hiçbir şey derlemeyecek kişiler için |
| "Kendi oyununu ekle" rehberi + `good first issue`'lar | Oyun ekleyen katkıcılar projeyi büyütür |

## Kanallar

| Kanal | Ne paylaşılır | Not |
|---|---|---|
| Hackaday (tips@hackaday.com'a e-posta) | Kısa açıklama, video, repo linki | ESP32 projelerini sık haber yapıyorlar; çıkan yazı başka sitelerden link getirir |
| Hacker News — Show HN | "Show HN: Party games on a $5 ESP32, no app, no internet" | Salı–perşembe, ABD sabahı; ilk saatlerde yorumlara kendin cevap ver |
| Reddit | r/esp32, r/arduino (teknik); r/boardgames (oyun) | Önce her topluluğun reklam kuralını oku |
| Hackster.io, Instructables | Malzeme listesi, bağlantı şeması, kutu yapımı | Kutulu sürümün yeri; Instructables'ta yarışmalar var |
| Espressif | X/Mastodon'da @EspressifSystem'i etiketle | Topluluk projelerini paylaşıyorlar |
| Awesome listeleri | `awesome-esp` gibi listelere PR | Az ama düzenli ziyaretçi |
| TikTok, Reels, YouTube Shorts | Kafe videosunun dikey hali | Mühendis olmayanlara ulaşır |
| Türkiye | LinkedIn yazısı; Technopat ve DonanımHaber forumları; Türkçe Medium/dev.to yazısı | Türkçe arayüz burada avantaj |

## Sıra

1. Faz 1 ve 2'yi bitir, arkadaşlarla gerçek oyun oyna ve videoyu çek.
2. Faz 3'ü bitir: tarayıcıdan yükleme, sürümler, README.
3. Aynı hafta: Hackaday e-postası, Show HN, Reddit, LinkedIn.
4. İlk haftanın issue'larına hızlı cevap ver; yıldız ve katkı kararı o hafta verilir.
5. Her yeni oyunda yeni kısa video ve küçük bir duyuru.
