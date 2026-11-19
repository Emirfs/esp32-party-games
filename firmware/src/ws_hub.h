#pragma once

#include "esp_http_server.h"

// WebSocket hub at /ws. Phase 0 protocol (JSON text frames):
//   phone -> box: {"t":"hello","id":"<clientId>"}   {"t":"ping","c":<ms>,"rtt":<ms>}
//   box -> phone: {"t":"welcome","fd":n}  {"t":"pong","c":<ms>}
//                 {"t":"presence","clients":[{"id":"..","rtt":n,"age":ms}]}
// A socket silent for kIdleCloseMs is closed: a locked phone stops pinging,
// and the next hello with the same id replaces any older socket.
namespace ws_hub {

void start(httpd_handle_t server);
esp_err_t handler(httpd_req_t* req);
// Called from the server's close_fn (httpd task) for every closing socket.
void on_close(int fd);
int client_count();

}  // namespace ws_hub
