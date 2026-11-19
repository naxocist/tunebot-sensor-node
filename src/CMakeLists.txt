idf_component_register(
    SRCS "main.c" "audio.c" "yin.c" "tuning.c" "mpu6050.c" "espnow_link.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES driver esp_adc esp_timer esp_wifi esp_netif esp_event nvs_flash
)
