// espnow_link.c - broadcast tunebot_packet_t to the gateway over ESP-NOW.
// The node never joins Wi-Fi; it only parks its radio on the hotspot's channel.
#include <string.h>
#include "esp_idf_version.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "espnow_link.h"

static const char *TAG = "espnow";
static const uint8_t BCAST[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static uint32_t s_seq = 0;
static volatile uint32_t s_ok = 0, s_fail = 0;

/* ESP-IDF 5.5 changed the send callback's first argument from the MAC to wifi_tx_info_t */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
static void sent_cb(const wifi_tx_info_t *info, esp_now_send_status_t status)
#else
static void sent_cb(const uint8_t *mac, esp_now_send_status_t status)
#endif
{
    if (status == ESP_NOW_SEND_SUCCESS) s_ok++; else s_fail++;
}

esp_err_t espnow_link_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(sent_cb));
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, BCAST, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    ESP_LOGI(TAG, "node %d ready on channel %d, MAC %02x:%02x:%02x:%02x:%02x:%02x",
             NODE_ID, ESPNOW_CHANNEL, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return ESP_OK;
}

esp_err_t espnow_link_send(tunebot_packet_t *p)
{
    p->node_id = NODE_ID;
    p->seq = s_seq++;
    esp_err_t err = esp_now_send(BCAST, (const uint8_t *)p, sizeof *p);
    if (err != ESP_OK) ESP_LOGW(TAG, "send failed: %s", esp_err_to_name(err));
    if (p->seq % 50 == 0) ESP_LOGI(TAG, "seq %lu, radio ok %lu / fail %lu",
                                   (unsigned long)p->seq, (unsigned long)s_ok, (unsigned long)s_fail);
    return err;
}
