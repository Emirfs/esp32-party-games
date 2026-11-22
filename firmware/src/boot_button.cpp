#include "boot_button.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ws_hub.h"

namespace boot_button {
namespace {

constexpr gpio_num_t kPin = GPIO_NUM_0;
constexpr int kPollMs = 100;
constexpr int kHoldMs = 5000;

void task(void*) {
  int heldMs = 0;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(kPollMs));
    if (gpio_get_level(kPin) != 0) {
      heldMs = 0;
      continue;
    }
    heldMs += kPollMs;
    if (heldMs == kHoldMs) {  // once per press
      ESP_LOGI("boot_button", "held %d ms, resetting admin", kHoldMs);
      ws_hub::reset_admin();
    }
  }
}

}  // namespace

void start() {
  gpio_config_t io = {};
  io.pin_bit_mask = 1ull << kPin;
  io.mode = GPIO_MODE_INPUT;
  io.pull_up_en = GPIO_PULLUP_ENABLE;
  ESP_ERROR_CHECK(gpio_config(&io));
  xTaskCreate(task, "boot_btn", 2048, nullptr, 2, nullptr);
}

}  // namespace boot_button
