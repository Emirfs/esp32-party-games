#pragma once

#include "esp_http_server.h"

// WebSocket endpoint /ws: binds the httpd sockets to the portable Hub/Room in
// core/. Protocol v1 (JSON text frames) is specified in docs/protocol.md:
//   phone -> box: hello{id} ping{c} profile ready claimAdmin transfer kick
//                 settings restart start skip end ack act vote
//   box -> phone: welcome{proto} pong{c} view{seq,view} err{code} buzz{p}
// A socket silent for 15 s is closed (a locked phone stops pinging); the next
// hello with the same id replaces any older socket.
namespace ws_hub {

void start(httpd_handle_t server);
esp_err_t handler(httpd_req_t* req);
// Called from the server's close_fn (httpd task) for every closing socket.
void on_close(int fd);
int client_count();
// Safe from any task: clears the admin (BOOT button held for 5 s).
void reset_admin();

}  // namespace ws_hub
