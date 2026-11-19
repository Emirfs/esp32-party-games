#include "dns_server.h"

#include <cstring>

#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

namespace dns_server {
namespace {

constexpr char kTag[] = "dns";
constexpr size_t kMaxPacket = 512;
constexpr size_t kHeaderLen = 12;
constexpr size_t kAnswerLen = 16;
constexpr uint16_t kTypeA = 1;
constexpr uint16_t kClassIn = 1;

uint16_t rd16(const uint8_t* p) { return static_cast<uint16_t>(p[0] << 8 | p[1]); }
void wr16(uint8_t* p, uint16_t v) {
  p[0] = v >> 8;
  p[1] = v & 0xFF;
}

// Rewrites the query in place into a reply; returns its length, or 0 to drop it.
size_t build_reply(uint8_t* pkt, size_t len, uint32_t ip_be) {
  if (len < kHeaderLen) return 0;
  const uint16_t flags = rd16(pkt + 2);
  const bool is_query = (flags & 0x8000) == 0;
  const uint8_t opcode = (flags >> 11) & 0xF;
  if (!is_query || opcode != 0 || rd16(pkt + 4) == 0) return 0;

  // First question name: plain labels only (compression is invalid there).
  size_t pos = kHeaderLen;
  while (pos < len && pkt[pos] != 0) {
    if (pkt[pos] & 0xC0) return 0;
    pos += pkt[pos] + 1;
  }
  pos += 1;
  if (pos + 4 > len) return 0;
  const uint16_t qtype = rd16(pkt + pos);
  const uint16_t qclass = rd16(pkt + pos + 2);
  pos += 4;  // reply keeps only this question; extra questions and EDNS records are dropped

  const bool answer = qtype == kTypeA && qclass == kClassIn;
  wr16(pkt + 2, 0x8000 | 0x0400 | (flags & 0x0100) | 0x0080);  // QR, AA, RD copied, RA
  wr16(pkt + 4, 1);
  wr16(pkt + 6, answer ? 1 : 0);
  wr16(pkt + 8, 0);
  wr16(pkt + 10, 0);
  // Non-A queries get NOERROR with no records so AAAA lookups fall back to A fast.
  if (!answer) return pos;
  if (pos + kAnswerLen > kMaxPacket) return 0;

  uint8_t* a = pkt + pos;
  wr16(a, 0xC00C);  // name: pointer to the question
  wr16(a + 2, kTypeA);
  wr16(a + 4, kClassIn);
  // TTL 0: after leaving the network a phone must not keep resolving real hosts to us.
  wr16(a + 6, 0);
  wr16(a + 8, 0);
  wr16(a + 10, 4);
  memcpy(a + 12, &ip_be, 4);
  return pos + kAnswerLen;
}

void task(void*) {
  const int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(53);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (sock < 0 || bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0) {
    ESP_LOGE(kTag, "cannot bind UDP 53 (errno %d)", errno);
    vTaskDelete(nullptr);
    return;
  }

  const uint32_t ip_be = inet_addr(cfg::kApIp);
  uint8_t buf[kMaxPacket];
  for (;;) {
    sockaddr_in from = {};
    socklen_t from_len = sizeof from;
    const int n = recvfrom(sock, buf, sizeof buf, 0, reinterpret_cast<sockaddr*>(&from), &from_len);
    if (n <= 0) continue;
    const size_t out = build_reply(buf, static_cast<size_t>(n), ip_be);
    if (out > 0) sendto(sock, buf, out, 0, reinterpret_cast<sockaddr*>(&from), from_len);
  }
}

}  // namespace

void start() { xTaskCreate(task, "dns", 3072, nullptr, 5, nullptr); }

}  // namespace dns_server
