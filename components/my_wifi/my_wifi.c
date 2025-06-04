
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
#include "my_led.h"
#include "sdkconfig.h"

#define MAX_NETWORKS 3
#define WIFI_SWITCH_BUTTON_GPIO CONFIG_WIFI_SWITCH_BUTTON_GPIO

static const char *TAG = "wifi";

bool my_wifi_is_connected(void);
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

static void indicate_wifi_selection(int index)
{
    my_led_blink(index + 1, 150); // 1x für SSID_1, 2x für SSID_2 usw.
}

static void connect_to_wifi(int index)
{
    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid, wifi_configs[index].ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, wifi_configs[index].password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_connect());

    ESP_LOGI(TAG, "Verbinde mit SSID %s ...", wifi_configs[index].ssid);

    my_led_set_wifi_status(true);
}

void wifi_switch_next_network(void)
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
            wifi_switch_next_network();
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
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "Fehler beim Erstellen des Event Loops: %s", esp_err_to_name(err));
        return;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    connect_to_wifi(current_network);

    xTaskCreate(button_task, "wifi_button_task", 2048, NULL, 5, NULL);
}
