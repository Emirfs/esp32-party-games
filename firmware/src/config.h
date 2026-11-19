#pragma once

#include <cstdint>

namespace cfg {

constexpr char kSsidPrefix[] = "PartyGames-";
// Shared table password, printed in the docs; it keeps passers-by out, not attackers.
constexpr char kApPassword[] = "partygames";
constexpr uint8_t kApChannel = 6;
// ESP32-S3 SoftAP hardware limit; the admin phone counts as one station.
constexpr uint8_t kMaxStations = 15;

// The only origin players use. localStorage (player tokens) is per-origin, so
// every link, QR code and redirect must point here, never at a hostname.
constexpr char kApIp[] = "192.168.4.1";
constexpr char kPortalUrl[] = "http://192.168.4.1/portal";

}  // namespace cfg
