#pragma once

namespace boot_button {

// Watches the board's BOOT button (GPIO0, read-only use: it is a strapping
// pin). Held for 5 s it clears the admin, for a lost or locked admin phone.
void start();

}  // namespace boot_button
