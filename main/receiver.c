#include "receiver.h"
#include "protocol.h"
#include "print_util.h"

#include "raw_stream.h"
#include "i2s_stream.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "audio_pipeline.h"
#include "string.h"

static const char *TAG = "RECEIVER";

audio_element_handle_t source, sink;

uint16_t last_packet_number = 0;
void process_promisc(void *buf, wifi_promiscuous_pkt_type_t type)
{
    wifi_promiscuous_pkt_t *packet = (wifi_promiscuous_pkt_t *)buf;
    wifi_pkt_rx_ctrl_t *header = &packet->rx_ctrl;
    char *payload = (char *)&packet->payload;
    if (memcmp(&payload[16], "mass__", 6) == 0)
    {
        // MEASURE_RATE();
        uint16_t seq_number = *((uint16_t *)&payload[24]);
        // if (last_packet_number + 1 != seq_number) ESP_LOGI(TAG, "not consequtive packet numbers last=%d now=%d", last_packet_number, seq_number);
        last_packet_number = seq_number;
        raw_stream_write(source, &payload[HEADER_SIZE], PAYLOAD_LEN);
    }
}

char dummy_buffer[128];

void init_rx()
{
    audio_pipeline_handle_t pipeline;
    ESP_LOGI(TAG, "create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);

    ESP_LOGI(TAG, "create mass audio stream to receive");
    raw_stream_cfg_t raw_stream_cfg_read = RAW_STREAM_CFG_DEFAULT();
    raw_stream_cfg_read.type = AUDIO_STREAM_WRITER;
    source = raw_stream_init(&raw_stream_cfg_read);
    audio_element_set_output_timeout(source, portMAX_DELAY);

    ESP_LOGI(TAG, "create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    sink = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "register all elements to audio pipeline");
    audio_pipeline_register(pipeline, source, "wifi_read");
    audio_pipeline_register(pipeline, sink, "i2s_write");

    ESP_LOGI(TAG, "link it together");
    const char *link_tag[2] = {"wifi_read", "i2s_write"};
    audio_pipeline_link(pipeline, &link_tag[0], 2);

    ESP_LOGI(TAG, "start audio_pipeline");
    audio_pipeline_run(pipeline);

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&process_promisc);
}