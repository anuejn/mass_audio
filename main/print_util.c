#include "print_util.h"

#include "string.h"
#include "esp_wifi_types.h"

void print_wifi_pkt_rx_ctrl(wifi_pkt_rx_ctrl_t *pkt) {
  printf(" rssi: %d\n rate: %d\n sig_mode: %d\n mcs: %d\n cwb: %d\n smoothing: "
         "%d\n not_sounding: %d\n aggregation: %d\n stbc: %d\n fec_coding: "
         "%d\n sgi: %d\n noise_floor: %d\n ampdu_cnt: %d\n channel: %d\n "
         "secondary_channel: %d\n timestamp: %d\n ant: %d\n noise_floor: %d\n "
         "sig_len: %d\n rx_state: %d\n",
         pkt->rssi, pkt->rate, pkt->sig_mode, pkt->mcs, pkt->cwb,
         pkt->smoothing, pkt->not_sounding, pkt->aggregation, pkt->stbc,
         pkt->fec_coding, pkt->sgi, pkt->noise_floor, pkt->ampdu_cnt,
         pkt->channel, pkt->secondary_channel, pkt->timestamp, pkt->ant,
         pkt->noise_floor, pkt->sig_len, pkt->rx_state);
}

#define COLS 16
void hex_dump(char *buf, unsigned len) {
  for (int i = 0; i < len; i += COLS) {
    printf("%04x \t", i);
    for (int x = 0; ((i + x) < len) && (x < COLS); x++) {
      printf("%02x ", buf[i + x]);
    }
    printf("\n");
  }
}