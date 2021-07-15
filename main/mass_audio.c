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

#include "print_util.c"

static const char *TAG = "MASS_AUDIO";

void process_promisc(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type == WIFI_PKT_DATA) {
    wifi_promiscuous_pkt_t *packet = (wifi_promiscuous_pkt_t *)buf;
    wifi_pkt_rx_ctrl_t *header = &packet->rx_ctrl;
    char *payload = (char *)&packet->payload;
    if (memcmp(&payload[16], "mass__", 6) == 0) {
      printf("MASS AUDIO PACKET\n");
      print_wifi_pkt_rx_ctrl(header);
      hex_dump(payload, header->sig_len);
    }
  }
}

audio_element_handle_t massaudio_writer;
#define HEADER_SIZE 24
char send_buffer[HEADER_SIZE + 1024] = {
    0x08, 0x00,                         // 0-1: Frame Control
    0x00, 0x00,                         // 2-3: Duration
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 4-9: Destination address (broadcast)
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 10-15: Source address
    'm',  'a',  's',  's',  '_',  '_',  // 16-21: BSSID
    0x00, 0x00,                         // 22-23: Sequence / fragment number
};
void send_task(void *pvParameter) {
  while (true) {
    for (int i = HEADER_SIZE; i < sizeof(send_buffer);) {
      int res = raw_stream_read(massaudio_writer, &send_buffer[i],
                                sizeof(send_buffer) - i);
      if (res < 0) {
        ESP_LOGE(TAG, "raw stream read error: %d", res);
      } else {
        i += res;
      }
    }
    hex_dump(send_buffer, sizeof(send_buffer));
    ESP_ERROR_CHECK(
        esp_wifi_80211_tx(WIFI_IF_AP, send_buffer, sizeof(send_buffer), true));
    vTaskDelay(2 / portTICK_PERIOD_MS);
  }
}

esp_err_t event_handler(void *ctx, system_event_t *event) { return ESP_OK; }

void app_main(void) {
  esp_log_level_set("*", ESP_LOG_INFO);
  esp_log_level_set(TAG, ESP_LOG_DEBUG);

  nvs_flash_init();
  tcpip_adapter_init();
  ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

  ESP_LOGI(TAG, "[0.1] Setting up Wifi");
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

  ESP_LOGI(TAG, "[0.2] setting up promiscuous mode callback");
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&process_promisc);

  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
  ESP_ERROR_CHECK(
      esp_wifi_internal_set_fix_rate(WIFI_IF_AP, true, WIFI_PHY_RATE_MCS7_SGI));

  ESP_LOGI(TAG, "[ 1 ] Start codec chip");
  audio_pipeline_handle_t pipeline;
  audio_element_handle_t i2s_stream_writer, i2s_stream_reader;

  audio_board_handle_t board_handle = audio_board_init();
  audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_LINE_IN,
                       AUDIO_HAL_CTRL_START);

  ESP_LOGI(TAG, "[ 2 ] Create audio pipeline for playback");
  audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
  pipeline = audio_pipeline_init(&pipeline_cfg);

  ESP_LOGI(TAG, "[3.1] Create i2s stream to write data to codec chip");
  i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
  i2s_cfg.type = AUDIO_STREAM_WRITER;
  i2s_stream_writer = i2s_stream_init(&i2s_cfg);

  ESP_LOGI(TAG, "[3.2] Create i2s stream to read data from codec chip");
  i2s_stream_cfg_t i2s_cfg_read = I2S_STREAM_CFG_DEFAULT();
  i2s_cfg_read.type = AUDIO_STREAM_READER;
  i2s_stream_reader = i2s_stream_init(&i2s_cfg_read);

  ESP_LOGI(TAG, "[3.3] Create mass audio stream to broadcast");
  raw_stream_cfg_t raw_stream_cfg_read = RAW_STREAM_CFG_DEFAULT();
  raw_stream_cfg_read.type = AUDIO_STREAM_READER;
  massaudio_writer = raw_stream_init(&raw_stream_cfg_read);

  ESP_LOGI(TAG, "[3.4] Register all elements to audio pipeline");
  audio_pipeline_register(pipeline, i2s_stream_reader, "i2s_read");
  audio_pipeline_register(pipeline, massaudio_writer, "massaudio_writer");
  xTaskCreate(&send_task, "send_task", 2048, NULL, 5, NULL);

  ESP_LOGI(
      TAG,
      "[3.4] Link it together "
      "[codec_chip]-->i2s_stream_reader-->i2s_stream_writer-->[codec_chip]"
  );
  const char *link_tag[2] = {"i2s_read", "massaudio_writer"};
  audio_pipeline_link(pipeline, &link_tag[0], 2);

  ESP_LOGI(TAG, "[ 4 ] Set up  event listener");
  audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
  audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

  ESP_LOGI(TAG, "[4.1] Listening event from all elements of pipeline");
  audio_pipeline_set_listener(pipeline, evt);

  ESP_LOGI(TAG, "[ 5 ] Start audio_pipeline");
  audio_pipeline_run(pipeline);

  ESP_LOGI(TAG, "[ 6 ] Listen for all pipeline events");
  while (1) {
    audio_event_iface_msg_t msg;
    esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
      continue;
    }

    /* Stop when the last pipeline element (i2s_stream_writer in this case)
     * receives stop event */
    if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT &&
        msg.source == (void *)i2s_stream_writer &&
        msg.cmd == AEL_MSG_CMD_REPORT_STATUS &&
        (((int)msg.data == AEL_STATUS_STATE_STOPPED) ||
         ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
      ESP_LOGW(TAG, "[ * ] Stop event received");
      break;
    }
  }

  ESP_LOGI(TAG, "[ 7 ] Stop audio_pipeline");
  audio_pipeline_stop(pipeline);
  audio_pipeline_wait_for_stop(pipeline);
  audio_pipeline_terminate(pipeline);

  audio_pipeline_unregister(pipeline, i2s_stream_reader);
  audio_pipeline_unregister(pipeline, i2s_stream_writer);

  /* Terminate the pipeline before removing the listener */
  audio_pipeline_remove_listener(pipeline);

  /* Make sure audio_pipeline_remove_listener &
   * audio_event_iface_remove_listener are called before destroying event_iface
   */
  audio_event_iface_destroy(evt);

  /* Release all resources */
  audio_pipeline_deinit(pipeline);
  audio_element_deinit(i2s_stream_reader);
  audio_element_deinit(i2s_stream_writer);
}
