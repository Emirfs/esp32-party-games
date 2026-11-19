#pragma once

namespace dns_server {

// Captive DNS: answers every A query with the AP address so any hostname a
// phone probes lands on our HTTP server. Runs in its own task.
void start();

}  // namespace dns_server
