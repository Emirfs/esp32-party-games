#include "nvs_store.h"

#include "esp_log.h"
#include "nvs.h"

namespace {

constexpr char kTag[] = "store";
constexpr char kNamespace[] = "pg";

}  // namespace

size_t NvsStore::load(const char* key, void* buf, size_t cap) {
  nvs_handle_t h;
  if (nvs_open(kNamespace, NVS_READONLY, &h) != ESP_OK) return 0;
  size_t len = 0;
  esp_err_t err = nvs_get_blob(h, key, nullptr, &len);
  if (err == ESP_OK && len <= cap) err = nvs_get_blob(h, key, buf, &len);
  nvs_close(h);
  return err == ESP_OK && len <= cap ? len : 0;
}

void NvsStore::save(const char* key, const void* data, size_t len) {
  nvs_handle_t h;
  esp_err_t err = nvs_open(kNamespace, NVS_READWRITE, &h);
  if (err == ESP_OK) {
    err = nvs_set_blob(h, key, data, len);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
  }
  if (err != ESP_OK) ESP_LOGW(kTag, "save %s (%u bytes) failed: %s", key, static_cast<unsigned>(len), esp_err_to_name(err));
}

void NvsStore::erase(const char* key) {
  nvs_handle_t h;
  if (nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) return;
  if (nvs_erase_key(h, key) == ESP_OK) nvs_commit(h);
  nvs_close(h);
}
