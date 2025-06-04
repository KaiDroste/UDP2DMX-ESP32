idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_event esp_netif driver nvs_flash esp_dmx esp_wifi my_wifi)
