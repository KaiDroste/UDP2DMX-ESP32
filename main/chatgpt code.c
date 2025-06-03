🔧 1. Kconfig erweitern
        Füge dies zu deiner Kconfig -
    Datei hinzu(z. B.components / my_wifi / Kconfig) :

                                                       kconfig Kopieren
                                                       Bearbeiten
                                                       menu "Wi-Fi Einstellungen"

                                                       config WIFI_SSID_1
                                                       string "SSID Netzwerk 1" default "WLAN1"

                                                       config WIFI_PASS_1
                                                       string "Passwort Netzwerk 1" default ""

                                                       config WIFI_SSID_2
                                                       string "SSID Netzwerk 2" default "WLAN2"

                                                       config WIFI_PASS_2
                                                       string "Passwort Netzwerk 2" default ""

                                                       config WIFI_SSID_3
                                                       string "SSID Netzwerk 3" default "WLAN3"

                                                       config WIFI_PASS_3
                                                       string "Passwort Netzwerk 3" default ""

                                                       endmenu
                                                       🧱 2. CMakeLists.txt für das Modul
                                                       Datei : components
        / my_wifi / CMakeLists.txt

                    cmake Kopieren Bearbeiten idf_component_register(SRCS "my_wifi.c" INCLUDE_DIRS ".") 📦 3. Header : my_wifi.h c Kopieren Bearbeiten
#pragma once

                                                                                                                       void my_wifi_init(void);
void my_wifi_switch_next_network(void);
📄 4. Implementierung : my_wifi.c
                            c
                                Kopieren
                                    Bearbeiten
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "driver/gpio.h"

#include "my_wifi.h"
#include "sdkconfig.h"

#define MAX_NETWORKS 3
#define WIFI_SWITCH_BUTTON_GPIO 0 // Beispiel: Boot-Button

                        static const char *TAG = "my_wifi";

static int current_network = 0;

typedef struct
{
    const char *ssid;
    const char *password;
} wifi_config_entry_t;

static wifi_config_entry_t wifi_configs[MAX_NETWORKS] = {
    {CONFIG_WIFI_SSID_1, CONFIG_WIFI_PASS_1},
    {CONFIG_WIFI_SSID_2, CONFIG_WIFI_PASS_2},
    {CONFIG_WIFI_SSID_3, CONFIG_WIFI_PASS_3},
};

static void connect_to_wifi(int index)
{
    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid, wifi_configs[index].ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, wifi_configs[index].password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_connect());

    ESP_LOGI(TAG, "Verbinde mit SSID %s ...", wifi_configs[index].ssid);
}

void my_wifi_switch_next_network(void)
{
    current_network = (current_network + 1) % MAX_NETWORKS;
    connect_to_wifi(current_network);
}

static void button_task(void *arg)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << WIFI_SWITCH_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf);

    bool last_state = 1;
    while (1)
    {
        bool state = gpio_get_level(WIFI_SWITCH_BUTTON_GPIO);
        if (!state && last_state)
        {
            my_wifi_switch_next_network();
            vTaskDelay(pdMS_TO_TICKS(500)); // Entprellung
        }
        last_state = state;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void my_wifi_init(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    connect_to_wifi(current_network);

    xTaskCreate(button_task, "wifi_button_task", 2048, NULL, 5, NULL);
}
🧠 5. Verwendung in app_main.c
    c
        Kopieren
            Bearbeiten
#include "my_wifi.h"

    void
    app_main(void)
{
    my_wifi_init();
}
🧪 Optional : Button - GPIO in Kconfig Falls du den Button - Pin auch in menuconfig konfigurieren willst :

    kconfig Kopieren Bearbeiten config WIFI_SWITCH_BUTTON_GPIO int "GPIO für Netzwerkauswahl" default 0 Dann ersetzt du im Code #define WIFI_SWITCH_BUTTON_GPIO 0 durch :

    c Kopieren Bearbeiten
#define WIFI_SWITCH_BUTTON_GPIO CONFIG_WIFI_SWITCH_BUTTON_GPIO
        Wenn du möchtest,
    kann ich dir auch eine.zip - Projektstruktur generieren.Sag einfach Bescheid.