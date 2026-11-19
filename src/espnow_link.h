// espnow_link.h - node side of the node -> gateway link
#pragma once
#include "esp_err.h"
#include "packet.h"

// >>> Set this to the channel the gateway prints: "Wi-Fi up, IP ..., channel X" <<<
#define ESPNOW_CHANNEL 6
#define NODE_ID        1

esp_err_t espnow_link_init(void);
// Fills node_id and seq, then broadcasts. Returns ESP_OK if handed to the radio.
esp_err_t espnow_link_send(tunebot_packet_t *p);
