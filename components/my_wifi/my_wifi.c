#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include "mdns.h"

#include "my_wifi.h"
#include "my_led.h"
#include "sdkconfig.h"

#define MAX_NETWORKS 3
#define WIFI_SWITCH_BUTTON_GPIO CONFIG_WIFI_SWITCH_BUTTON_GPIO

static const char *TAG = "wifi";
static bool is_connecting = false;
static char current_hostname[32] = "udp2dmx";

bool my_wifi_is_connected(void);
static int current_network = 0;

static TaskHandle_t reconnect_task_handle = NULL;

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

void my_wifi_set_hostname(const char *new_hostname)
{
    if (!new_hostname || strlen(new_hostname) >= sizeof(current_hostname))
    {
        ESP_LOGW(TAG, "Hostname ungültig oder zu lang: %s", new_hostname);
        return;
    }

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif)
    {
        ESP_LOGE(TAG, "esp_netif nicht gefunden");
        return;
    }

    const char *old_hostname = NULL;
    if (esp_netif_get_hostname(netif, &old_hostname) != ESP_OK)
    {
        ESP_LOGW(TAG, "Alter Hostname konnte nicht gelesen werden");
        old_hostname = NULL;
    }

    // Nur aktualisieren, wenn sich der Hostname ändert
    if (old_hostname && strcmp(old_hostname, new_hostname) == 0)
    {
        ESP_LOGI(TAG, "Hostname ist bereits: %s", old_hostname);
        return;
    }

    // Hostname speichern und setzen
    strncpy(current_hostname, new_hostname, sizeof(current_hostname));
    current_hostname[sizeof(current_hostname) - 1] = '\0';

    esp_netif_set_hostname(netif, current_hostname);
    ESP_LOGI(TAG, "Hostname geändert: %s", current_hostname);

    // mDNS aktualisieren
    mdns_free(); // Alte Instanz freigeben
    mdns_init(); // Neu initialisieren
    mdns_hostname_set(current_hostname);
    ESP_LOGI(TAG, "mDNS-Hostname aktualisiert auf: %s", current_hostname);
}

// const char *my_wifi_get_hostname(void)
// {
//     return current_hostname;
// }

void start_mdns_service(void)
{
    mdns_init();
    mdns_hostname_set(current_hostname); // ergibt esp32.local
    ESP_LOGI(TAG, "mDNS-Hostname gesetzt: %s", current_hostname);
    mdns_instance_name_set("DMX Controller"); // Name in der mDNS-Anfrage
}

static void connect_to_wifi(int index)
{
    if (is_connecting)
    {
        ESP_LOGW(TAG, "Verbindungsversuch läuft bereits – überspringe");
        return;
    }

    if (index < 0 || index >= MAX_NETWORKS)
    {
        ESP_LOGW(TAG, "Ungültiger Netzwerkindex");
        return;
    }
    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid, wifi_configs[index].ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, wifi_configs[index].password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    is_connecting = true;
    ESP_ERROR_CHECK(esp_wifi_connect());

    ESP_LOGI(TAG, "Verbinde mit SSID %s ...", wifi_configs[index].ssid);
}

static void indicate_wifi_selection(int index)
{
    my_led_blink(index + 1, 150); // 1x für SSID_1, 2x für SSID_2 usw.
}

void reconnect_task(void *param)
{
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Warten auf "Signal"
        vTaskDelay(pdMS_TO_TICKS(2000));
        if (!is_connecting)
        {
            connect_to_wifi(current_network);
        }
        // connect_to_wifi(current_network);
    }
}

const char *reason_str(wifi_err_reason_t reason)
{
    switch (reason)
    {
    case WIFI_REASON_AUTH_EXPIRE:
        return "Authentication expired";
    case WIFI_REASON_AUTH_FAIL:
        return "Authentication failed";
    case WIFI_REASON_NO_AP_FOUND:
        return "AP not found";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        return "Handshake timeout";
    // ... weitere Gründe nach Bedarf
    default:
        return "Unbekannter Grund";
    }
}

static void on_wifi_event(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_event_sta_disconnected_t *disconn = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGW("my_wifi", "WLAN getrennt (Grund %d: %s)", disconn->reason, reason_str(disconn->reason));

        my_led_set_wifi_status(false); // LED blinkt
        is_connecting = false;
        xTaskNotifyGive(reconnect_task_handle);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ESP_LOGI("my_wifi", "WLAN verbunden – IP erhalten");
        is_connecting = false;
        my_led_set_wifi_status(true);

        start_mdns_service();
    }
}

void wifi_switch_next_network(void)
{
    current_network = (current_network + 1) % MAX_NETWORKS;
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_LOGI(TAG, "wifi Disconnected");
    // vTaskDelay(pdMS_TO_TICKS(200));
    // connect_to_wifi(current_network);
    // xTaskNotifyGive(reconnect_task_handle);
    indicate_wifi_selection(current_network);
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
    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &on_wifi_event,
                                        NULL,
                                        NULL);
    esp_event_handler_instance_register(IP_EVENT,
                                        IP_EVENT_STA_GOT_IP,
                                        &on_wifi_event,
                                        NULL,
                                        NULL);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    // Für die Verbindung mit TP-Link Routern Kann später entfernt werden @TODO
    // esp_wifi_set_ps(WIFI_PS_NONE);

    connect_to_wifi(current_network);

    xTaskCreate(button_task, "wifi_button_task", 2048, NULL, 5, NULL);
    xTaskCreate(reconnect_task, "reconnect_task", 4096, NULL, 5, &reconnect_task_handle);
}
