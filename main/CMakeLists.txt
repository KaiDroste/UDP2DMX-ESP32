idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_event esp_netif driver nvs_flash protocol_examples_common esp_dmx)
