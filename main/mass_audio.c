#include "audio_pipeline.h"
#include "board.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wifi_internal.h"
#include "esp_wifi_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2s_stream.h"
#include "lwip/err.h"
#include "nvs_flash.h"
#include "raw_stream.h"

#include "receiver.h"
#include "sender.h"

static const char *TAG = "MASS_AUDIO";

esp_err_t event_handler(void *ctx, system_event_t *event) { return ESP_OK; }

void app_main(void)
{
  esp_log_level_set("*", ESP_LOG_INFO);
  esp_log_level_set(TAG, ESP_LOG_DEBUG);

  nvs_flash_init();
  tcpip_adapter_init();
  ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

  ESP_LOGI(TAG, "start codec chip");
  audio_board_handle_t board_handle = audio_board_init();
  audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH,
                       AUDIO_HAL_CTRL_START);


  ESP_LOGI(TAG, "setting up Wifi");
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  // Init dummy AP to specify a channel and get WiFi hardware into
  // a mode where we can send the actual fake beacon frames.
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  wifi_config_t ap_config = {.ap = {.ssid = "hidden",
                                    .ssid_len = 0,
                                    .channel = 1,
                                    .authmode = WIFI_AUTH_OPEN,
                                    .ssid_hidden = 1,
                                    .max_connection = 4,
                                    .beacon_interval = 60000}};

  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

  ESP_LOGI(TAG, "starting rx pipeline");
  init_rx();

  ESP_LOGI(TAG, "starting Wifi");
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
  ESP_ERROR_CHECK(esp_wifi_internal_set_fix_rate(WIFI_IF_AP, true, WIFI_PHY_RATE_MCS7_SGI));

  ESP_LOGI(TAG, "starting tx pipeline");
  init_tx();
}
