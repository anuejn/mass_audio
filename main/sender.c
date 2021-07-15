#include "sender.h"
#include "protocol.h"
#include "print_util.h"

#include "raw_stream.h"
#include "i2s_stream.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "audio_pipeline.h"
#include "string.h"

static const char *TAG = "SENDER";

char send_buffer[HEADER_SIZE + PAYLOAD_LEN + 1] = {
    0x08, 0x00,                         // 0-1: Frame Control
    0x00, 0x00,                         // 2-3: Duration
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 4-9: Destination address (broadcast)
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 10-15: Source address
    'm', 'a', 's', 's', '_', '_',       // 16-21: BSSID
    0x00, 0x00,                         // 22-23: Sequence / fragment number
    // end of iee packet header

    0x00, 0x00, // 24 - 26 Our own fragment number
};
void tx_task(void *pvParameter)
{
    audio_pipeline_handle_t pipeline;
    audio_element_handle_t source, sink;
    ESP_LOGI(TAG, "[ 2 ] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);

    ESP_LOGI(TAG, "[3.2] Create i2s stream to read data from codec chip");
    i2s_stream_cfg_t i2s_cfg_read = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg_read.type = AUDIO_STREAM_READER;
    source = i2s_stream_init(&i2s_cfg_read);

    ESP_LOGI(TAG, "[3.3] Create mass audio stream to broadcast");
    raw_stream_cfg_t raw_stream_cfg_read = RAW_STREAM_CFG_DEFAULT();
    raw_stream_cfg_read.type = AUDIO_STREAM_READER;
    sink = raw_stream_init(&raw_stream_cfg_read);
    audio_element_set_output_timeout(sink, portMAX_DELAY);

    ESP_LOGI(TAG, "[3.4] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, source, "i2s_read");
    audio_pipeline_register(pipeline, sink, "wifi_write");

    ESP_LOGI(
        TAG,
        "[3.4] Link it together "
        "[codec_chip]-->i2s_stream_reader-->i2s_stream_writer-->[codec_chip]");
    const char *link_tag[2] = {"i2s_read", "wifi_write"};
    audio_pipeline_link(pipeline, &link_tag[0], 2);

    ESP_LOGI(TAG, "[ 5 ] Start audio_pipeline");
    audio_pipeline_run(pipeline);

    uint16_t pkt_number = 0;
    while (true)
    {
        for (int i = 0; i < PAYLOAD_LEN;)
        {
            int res = raw_stream_read(sink, &send_buffer[i + HEADER_SIZE], PAYLOAD_LEN);

            if (res < 0)
            {
                ESP_LOGE(TAG, "raw stream read error: %d", res);
            }
            else
            {
                i += res;
            }

            if (i != 0)
            {
                vTaskDelay(1); // let other tasks to their stuff
            }
        }
        *((uint16_t*)&send_buffer[24]) = pkt_number++;
        send_buffer[HEADER_SIZE + PAYLOAD_LEN] = 0x13;
        ESP_ERROR_CHECK(esp_wifi_80211_tx(WIFI_IF_AP, send_buffer, sizeof(send_buffer), true));
    }
}
void init_tx()
{
    xTaskCreate(&tx_task, "tx_task", 1024 * 16, NULL, 5, NULL);
}