#include "esp_wifi_types.h"

void print_wifi_pkt_rx_ctrl(wifi_pkt_rx_ctrl_t *pkt);
void hex_dump(char *buf, unsigned len);

#define MEASURE_RATE( ) {\
  static unsigned long last = 0, current = 0, callback_counter = 0;\
  static float rate = 0, current_rate = 0;\
  callback_counter++;\
  if (callback_counter % 10 == 0) {\
      current = esp_timer_get_time();\
      current_rate = 1000000.0 / (float)(current - last);\
      last = current;\
      rate = (rate * 10. + current_rate) / 11.0;\
      ESP_LOGI(TAG, "pkt_send current_rate=%7.3f rate=%7.3f Hz", current_rate * 10, rate * 10);\
  }\
}
