#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "lwip/sockets.h"
#include "esp_dmx.h"
#include "driver/gpio.h"

#define TX_PIN 17
#define RX_PIN 16
#define EN_PIN 21
#define DEBUG_LED_GPIO 2

#define DMX_UNIVERSE_SIZE DMX_PACKET_SIZE
#define UDP_PORT 6454
#define FADE_INTERVAL_MS 10

static const char *TAG = "udp_dmx";
static uint8_t dmx_data[DMX_UNIVERSE_SIZE] = {};
static dmx_port_t dmx_num = DMX_NUM_1;
static bool wifi_connected = false;
static bool dmx_error = false;

void blink_debug_led(int times, int delay_ms)
{
    for (int i = 0; i < times; ++i)
    {
        gpio_set_level(DEBUG_LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        gpio_set_level(DEBUG_LED_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

static void led_status_task(void *arg)
{
    while (1)
    {
        if (dmx_error)
        {
            // Schnelles Blinken bei DMX-Fehler
            gpio_set_level(DEBUG_LED_GPIO, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(DEBUG_LED_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        else if (!wifi_connected)
        {
            // Langsames Blinken bei fehlendem WiFi
            gpio_set_level(DEBUG_LED_GPIO, 1);
            vTaskDelay(pdMS_TO_TICKS(500));
            gpio_set_level(DEBUG_LED_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        else
        {
            // LED aus, wenn alles OK
            gpio_set_level(DEBUG_LED_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

typedef struct
{
    bool active;
    int start_value;
    int target_value;
    int duration_ms;
    TickType_t start_time;
} fade_state_t;

static fade_state_t fade_states[DMX_UNIVERSE_SIZE] = {};

// Convert speed_S to milliseconds
static int speed_to_ms(int speed)
{
    if (speed == 255)
        return 0;
    if (speed >= 1 && speed <= 98)
        return speed * 591;
    if (speed >= 101 && speed <= 104)
        return (speed - 100) * 146 + 1;
    if (speed >= 201 && speed <= 254)
        return (speed - 200) * 72;
    if (speed == 254)
        return 3691;
    return 0;
}

static void fade_task(void *arg)
{
    while (1)
    {
        TickType_t now = xTaskGetTickCount();
        bool updated = false;

        for (int ch = 0; ch < DMX_UNIVERSE_SIZE; ++ch)
        {
            if (!fade_states[ch].active)
                continue;

            int elapsed = (now - fade_states[ch].start_time) * portTICK_PERIOD_MS;
            int duration = fade_states[ch].duration_ms;
            int start = fade_states[ch].start_value;
            int target = fade_states[ch].target_value;

            uint8_t new_value;

            if (elapsed >= duration)
            {
                new_value = target;
                fade_states[ch].active = false;
            }
            else
            {
                float t = (float)elapsed / duration;
                float interpolated = start + (target - start) * t;
                new_value = (uint8_t)roundf(interpolated);
            }

            if (dmx_data[ch] != new_value)
            {
                dmx_data[ch] = new_value;
                updated = true;
            }
        }

        if (updated)
        {
            dmx_write(dmx_num, dmx_data, DMX_UNIVERSE_SIZE);
        }

        vTaskDelay(pdMS_TO_TICKS(FADE_INTERVAL_MS));
    }
}

static void set_channel(int ch, int value, int fade_ms)
{
    if (fade_ms > 0 && dmx_data[ch] != value)
    {
        start_fade(ch, value, fade_ms);
    }
    else
    {
        stop_fade(ch);
        dmx_data[ch] = value;
        dmx_write(dmx_num, dmx_data, DMX_UNIVERSE_SIZE);
    }
}

static void set_multi_channels(int ch, int *values, int count, int fade_ms)
{
    if (ch + count > DMX_UNIVERSE_SIZE)
        return;

    if (fade_ms > 0)
    {
        for (int i = 0; i < count; ++i)
        {
            start_fade(ch + i, values[i], fade_ms);
        }
    }
    else
    {
        for (int i = 0; i < count; ++i)
        {
            stop_fade(ch + i);
            dmx_data[ch + i] = values[i];
        }
        dmx_write(dmx_num, dmx_data, DMX_UNIVERSE_SIZE);
    }
}

static void handle_udp_command(const char *cmd)
{
    char *copy = strdup(cmd);
    char *type = strtok(copy + 3, "#");
    char *arg1 = strtok(NULL, "#");
    char *arg2 = strtok(NULL, "#");

    if (!type || !arg1)
    {
        ESP_LOGW(TAG, "Ungültiges Kommando: %s", cmd);
        free(copy);
        return;
    }

    char mode = type[0];
    int ch = atoi(type + 1);
    int val = atoi(arg1);
    int fade_ms = speed_to_ms(arg2 ? atoi(arg2) : 255);

    if (ch < 1 || ch >= DMX_UNIVERSE_SIZE)
    {
        free(copy);
        return;
    }

    switch (mode)
    {
    case 'R':
    {
        int r = roundf((val % 1000) * 2.55f);
        int g = roundf(((val / 1000) % 1000) * 2.55f);
        int b = roundf(((val / 1000000) % 1000) * 2.55f);
        int rgb[3] = {r, g, b};
        set_multi_channels(ch, rgb, 3, fade_ms);
        ESP_LOGI(TAG, "RGB %d: R=%d G=%d B=%d", ch, r, g, b);
        break;
    }
    case 'W':
    {
        int ww = roundf((val / 1000) * 2.55f);
        int cw = roundf((val % 1000) * 2.55f);
        int tw[2] = {ww, cw};
        set_multi_channels(ch, tw, 2, fade_ms);
        ESP_LOGI(TAG, "TW %d: WW=%d CW=%d", ch, ww, cw);
        break;
    }
    case 'P':
        val = (val * 255) / 100;
    case 'C':
        set_channel(ch, val, fade_ms);
        ESP_LOGI(TAG, "Kanal %d gesetzt auf %d", ch, val);
        break;
    default:
        ESP_LOGW(TAG, "Unbekannter Modus: %c", mode);
        break;
    }

    free(copy);
}

void udp_server_task(void *arg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    struct sockaddr_in bind_addr = {.sin_family = AF_INET, .sin_port = htons(UDP_PORT), .sin_addr.s_addr = htonl(INADDR_ANY)};
    bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr));

    char rx_buffer[DMX_UNIVERSE_SIZE + 1];
    struct sockaddr_in6 source_addr;
    socklen_t socklen = sizeof(source_addr);

    while (1)
    {
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
        if (len == DMX_UNIVERSE_SIZE)
        {
            memcpy(dmx_data, rx_buffer, DMX_UNIVERSE_SIZE);
            for (int i = 0; i < DMX_UNIVERSE_SIZE; ++i)
                stop_fade(i);
            dmx_write(dmx_num, dmx_data, DMX_UNIVERSE_SIZE);
        }
        else if (len > 4 && strncmp(rx_buffer, "DMX", 3) == 0)
        {
            rx_buffer[len] = '\0';
            blink_debug_led(1, 20);
            handle_udp_command(rx_buffer);
        }
    }
}

// # alt
// void udp_server_task(void *pvParameters)
// {
//     char rx_buffer[DMX_UNIVERSE_SIZE + 1];
//     struct sockaddr_in6 source_addr;
//     socklen_t socklen = sizeof(source_addr);

//     int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
//     if (sock < 0)
//     {
//         ESP_LOGE(TAG, "Fehler beim Öffnen des Sockets");
//         vTaskDelete(NULL);
//     }

//     struct sockaddr_in bind_addr = {
//         .sin_family = AF_INET,
//         .sin_port = htons(UDP_PORT),
//         .sin_addr.s_addr = htonl(INADDR_ANY),
//     };

//     if (bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0)
//     {
//         ESP_LOGE(TAG, "Bind fehlgeschlagen");
//         close(sock);
//         vTaskDelete(NULL);
//     }

//     ESP_LOGI(TAG, "UDP Server hört auf Port %d", UDP_PORT);

//     while (1)
//     {
//         int len = recvfrom(sock, rx_buffer, DMX_UNIVERSE_SIZE, 0,
//                            (struct sockaddr *)&source_addr, &socklen);
//         if (len == DMX_UNIVERSE_SIZE)
//         {
//             memcpy(dmx_data, rx_buffer, DMX_UNIVERSE_SIZE);

//             // Alle laufenden Fades stoppen, da neue Daten explizit alles setzen
//             for (int ch = 0; ch < DMX_UNIVERSE_SIZE; ++ch)
//             {
//                 fade_states[ch].active = false;
//             }

//             dmx_write(dmx_num, dmx_data, DMX_UNIVERSE_SIZE);
//             for (int i = 1; i < DMX_UNIVERSE_SIZE; ++i)
//             {
//                 if (dmx_data[i] != 0)
//                 {
//                     ESP_LOGI(TAG, "DMX-Daten aktualisiert, Kanal %d = %d", i, dmx_data[i]);
//                     break;
//                 }
//             }
//         }
//         else if (len > 4 && strncmp(rx_buffer, "DMX", 3) == 0)
//         {
//             rx_buffer[len] = '\0';
//             ESP_LOGI(TAG, "Empfangenes Kommando: %s", rx_buffer);
//             blink_debug_led(1, 20);

//             char *type = strtok(rx_buffer + 3, "#");
//             char *arg1 = strtok(NULL, "#");
//             char *arg2 = strtok(NULL, "#");
//             char *arg3 = strtok(NULL, "#");

//             if (type && arg1)
//             {
//                 char mode = type[0];
//                 int channel = atoi(type + 1);
//                 int value = atoi(arg1);
//                 int speed = (arg2 ? atoi(arg2) : 255);
//                 int fade_ms = speed_to_ms(speed);

//                 if (channel >= 1 && channel < DMX_UNIVERSE_SIZE)
//                 {
//                     if (mode == 'R')
//                     {
//                         // value is expected to be R + G*1000 + B*1000000 (e.g. 3066012)
//                         int rgb_value = value;
//                         int r_percent = rgb_value % 1000;
//                         int g_percent = (rgb_value / 1000) % 1000;
//                         int b_percent = (rgb_value / 1000000) % 1000;

//                         int r = (int)roundf(r_percent * 2.55f);
//                         int g = (int)roundf(g_percent * 2.55f);
//                         int b = (int)roundf(b_percent * 2.55f);

//                         if (channel <= DMX_UNIVERSE_SIZE - 3)
//                         {
//                             if (fade_ms > 0)
//                             {
//                                 for (int i = 0; i < 3; i++)
//                                 {
//                                     fade_states[channel + i].start_value = dmx_data[channel + i];
//                                     fade_states[channel + i].target_value = (i == 0) ? r : (i == 1) ? g
//                                                                                                     : b;
//                                     fade_states[channel + i].duration_ms = fade_ms;
//                                     fade_states[channel + i].start_time = xTaskGetTickCount();
//                                     fade_states[channel + i].active = true;
//                                 }

//                                 ESP_LOGI(TAG, "Fading RGB @%d → R=%d G=%d B=%d (%d ms)", channel, r, g, b, fade_ms);
//                             }
//                             else
//                             {
//                                 dmx_data[channel] = r;
//                                 dmx_data[channel + 1] = g;
//                                 dmx_data[channel + 2] = b;

//                                 fade_states[channel].active = false;
//                                 fade_states[channel + 1].active = false;
//                                 fade_states[channel + 2].active = false;

//                                 dmx_write(dmx_num, dmx_data, DMX_UNIVERSE_SIZE);
//                                 ESP_LOGI(TAG, "RGB direkt gesetzt @%d → R=%d G=%d B=%d", channel, r, g, b);
//                             }
//                         }
//                         else
//                         {
//                             ESP_LOGW(TAG, "Nicht genug Kanäle für RGB ab Kanal %d", channel);
//                         }
//                     }
//                     else if (mode == 'W')
//                     {
//                         // Erwartet: 6-stelligen Wert WWWCW, z. B. 070050 für WW=70%, CW=50%

//                         if (value < 0 || value > 100100)
//                         {
//                             ESP_LOGW(TAG, "Ungültiger W-Wert: %d", value);
//                             return;
//                         }

//                         int ww_percent = value / 1000;
//                         int cw_percent = value % 1000;

//                         if (ww_percent > 100 || cw_percent > 100)
//                         {
//                             ESP_LOGW(TAG, "W-Wert außerhalb des Bereichs: WW=%d CW=%d", ww_percent, cw_percent);
//                             return;
//                         }

//                         int ww = (int)roundf(ww_percent * 2.55f);
//                         int cw = (int)roundf(cw_percent * 2.55f);

//                         if (channel <= DMX_UNIVERSE_SIZE - 2)
//                         {
//                             if (fade_ms > 0)
//                             {
//                                 for (int i = 0; i < 2; i++)
//                                 {
//                                     fade_states[channel + i].start_value = dmx_data[channel + i];
//                                     fade_states[channel + i].target_value = (i == 0) ? ww : cw;
//                                     fade_states[channel + i].duration_ms = fade_ms;
//                                     fade_states[channel + i].start_time = xTaskGetTickCount();
//                                     fade_states[channel + i].active = true;
//                                 }

//                                 ESP_LOGI(TAG, "Fading TW @%d → WW=%d CW=%d (%d ms)", channel, ww, cw, fade_ms);
//                             }
//                             else
//                             {
//                                 dmx_data[channel] = ww;
//                                 dmx_data[channel + 1] = cw;

//                                 fade_states[channel].active = false;
//                                 fade_states[channel + 1].active = false;

//                                 dmx_write(dmx_num, dmx_data, DMX_UNIVERSE_SIZE);
//                                 ESP_LOGI(TAG, "TW direkt gesetzt @%d → WW=%d CW=%d", channel, ww, cw);
//                             }
//                         }
//                     }
//                     else
//                     {
//                         int target_value = value;
//                         if (mode == 'P')
//                             target_value = (value * 255) / 100;
//                         else if (mode == 'C')
//                             target_value = value;

//                         if (fade_ms > 0 && dmx_data[channel] != target_value)
//                         {
//                             fade_states[channel].start_value = dmx_data[channel];
//                             fade_states[channel].target_value = target_value;
//                             fade_states[channel].duration_ms = fade_ms;
//                             fade_states[channel].start_time = xTaskGetTickCount();
//                             fade_states[channel].active = true;

//                             ESP_LOGI(TAG, "Fading Kanal %d von %d → %d in %d ms", channel, dmx_data[channel], target_value, fade_ms);
//                         }
//                         else
//                         {
//                             fade_states[channel].active = false;
//                             dmx_data[channel] = target_value;
//                             dmx_write(dmx_num, dmx_data, DMX_UNIVERSE_SIZE);
//                             ESP_LOGI(TAG, "Kanal %d direkt auf %d gesetzt", channel, target_value);
//                         }
//                     }
//                 }
//             }

//             else
//             {
//                 ESP_LOGW(TAG, "Falsche Paketlänge: %d", len);
//             }
//         }

//         // close(sock);
//         // vTaskDelete(NULL);
//     }
// }

void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Debug LED konfigurieren
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << DEBUG_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf);
    gpio_set_level(DEBUG_LED_GPIO, 1); // LED an beim Start

    // WLAN verbinden
    ESP_ERROR_CHECK(example_connect());
    wifi_connected = true;

    // DMX Setup
    dmx_config_t config = DMX_CONFIG_DEFAULT;
    dmx_driver_install(dmx_num, &config, NULL, 0);
    dmx_set_pin(dmx_num, TX_PIN, RX_PIN, EN_PIN);

    memset(dmx_data, 0, sizeof(dmx_data));
    dmx_write(dmx_num, dmx_data, DMX_UNIVERSE_SIZE);

    // Tasks starten
    xTaskCreate(udp_server_task, "udp_server", 8192, NULL, 5, NULL);
    xTaskCreate(fade_task, "fade_task", 4096, NULL, 5, NULL);
    xTaskCreate(led_status_task, "led_status_task", 2048, NULL, 3, NULL);

    ESP_LOGI(TAG, "System bereit – DMX aktiv & WiFi verbunden");
    blink_debug_led(2, 200); // 2x blinken zur Bestätigung

    TickType_t last = xTaskGetTickCount();
    while (1)
    {
        dmx_send(dmx_num);
        vTaskDelayUntil(&last, pdMS_TO_TICKS(30));
    }
}
