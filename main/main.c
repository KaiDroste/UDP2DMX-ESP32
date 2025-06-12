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
// #include "protocol_examples_common.h"
#include "lwip/sockets.h"
#include "esp_dmx.h"
#include "driver/gpio.h"

#include "my_wifi.h"
#include "my_led.h"
#include "my_config.h"

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

typedef struct
{
    bool active;
    int start_value;
    int target_value;
    int duration_ms;
    TickType_t start_time;
} fade_state_t;

static fade_state_t fade_states[DMX_UNIVERSE_SIZE] = {};

// Convert speed_S to milliseconds (Using Loxone protocol)
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

static void stop_fade(int ch)
{
    fade_states[ch].active = false;
}

static void start_fade(int ch, int value, int duration_ms)
{
    fade_states[ch].start_value = dmx_data[ch];
    fade_states[ch].target_value = value;
    fade_states[ch].duration_ms = duration_ms;
    fade_states[ch].start_time = xTaskGetTickCount();
    fade_states[ch].active = true;
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

            if (duration <= 0)
            {
                dmx_data[ch] = fade_states[ch].target_value;
                stop_fade(ch);
                updated = true;
                continue;
            }

            int start = fade_states[ch].start_value;
            int target = fade_states[ch].target_value;

            uint8_t new_value;

            if (elapsed >= duration)
            {
                new_value = target;
                stop_fade(ch);
            }
            else
            {
                float t = (float)elapsed / (float)duration;
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

    if (ch < 1 || ch + 1 >= DMX_UNIVERSE_SIZE) // Für L und W brauchen wir 2 Kanäle
    {
        free(copy);
        return;
    }

    switch (mode)
    {
    case 'R':
    {
        int r = (val % 1000);
        int g = ((val / 1000) % 1000);
        int b = ((val / 1000000) % 1000);

        int rgb[3] = {r, g, b};
        set_multi_channels(ch, rgb, 3, fade_ms);

        ESP_LOGI(TAG, "RGB %d: R=%d G=%d B=%d mit Fading %d ms", ch, r, g, b, fade_ms);
        break;
    }
    case 'W':
    {
        int ww = (val / 1000) % 1000;
        int cw = val % 1000;

        ww = ww > 255 ? 255 : (ww < 0 ? 0 : ww);
        cw = cw > 255 ? 255 : (cw < 0 ? 0 : cw);

        int tw[2] = {ww, cw};
        set_multi_channels(ch, tw, 2, fade_ms);

        ESP_LOGI(TAG, "TW %d: WW=%d CW=%d mit Fading %d ms", ch, ww, cw, fade_ms);
        break;
    }
    case 'L':
    {
        // Erwartet: val = 20xxxxxxx (Präfix 20, Helligkeit 3-stellig, Farbtemp 4-stellig)
        if (val < 200000000 || val > 209999999)
        {
            ESP_LOGW(TAG, "Ungültiges L-Kommando: %d", val);
            break;
        }

        int brightness = (val / 10000) % 1000; // z. B. 050
        int color_temp = val % 10000;          // z. B. 6700

        // Begrenzungen
        if (brightness < 0)
            brightness = 0;
        if (brightness > 100)
            brightness = 100;
        int min_ct, max_ct;
        get_ct_range(ch, &min_ct, &max_ct);

        if (color_temp < min_ct)
            color_temp = min_ct;
        if (color_temp > max_ct)
            color_temp = max_ct;

        // Verhältnis WW/CW berechnen
        float ratio = (float)(color_temp - min_ct) / (max_ct - min_ct);
        int cw = (int)(brightness * ratio * 2.55f); // 0–255
        int ww = (int)(brightness * (1.0f - ratio) * 2.55f);

        int tw[2] = {ww, cw};
        set_multi_channels(ch, tw, 2, fade_ms);

        ESP_LOGI(TAG, "Lichtfarbe %dK, Helligkeit %d%% → WW=%d CW=%d (Kanal %d+%d)", color_temp, brightness, ww, cw, ch, ch + 1);
        break;
    }
    case 'P':
        val = (val * 255) / 100;
        __attribute__((fallthrough));
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

    if (sock < 0)
    {
        ESP_LOGE(TAG, "UDP socket creation failed");
        vTaskDelete(NULL);
    }
    if (bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0)
    {
        ESP_LOGE(TAG, "UDP socket bind failed");
        close(sock);
        vTaskDelete(NULL);
    }
    char rx_buffer[DMX_UNIVERSE_SIZE + 1];
    struct sockaddr_in6 source_addr;
    socklen_t socklen = sizeof(source_addr);

    while (1)
    {
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
        ESP_LOGI(TAG, "UDP empfangen, Länge = %d", len);
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
            ESP_LOGI(TAG, "DMX-Befehl empfangen: \"%s\"", rx_buffer); // ← Hier Logausgabe einfügen
            my_led_blink(1, 20);
            handle_udp_command(rx_buffer);
        }
    }
}

void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    dmx_config_t config = DMX_CONFIG_DEFAULT;
    dmx_driver_install(dmx_num, &config, NULL, 0);
    dmx_set_pin(dmx_num, TX_PIN, RX_PIN, EN_PIN);

    memset(dmx_data, 0, sizeof(dmx_data));
    dmx_write(dmx_num, dmx_data, DMX_UNIVERSE_SIZE);

    spiffs_init();
    config_load_from_spiffs("/spiffs/config.json");

    // Tasks starten
    xTaskCreate(udp_server_task, "udp_server", 8192, NULL, 5, NULL);
    xTaskCreate(fade_task, "fade_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "System bereit: DMX aktiv & WiFi verbunden");
    my_led_blink(2, 200);

    TickType_t last = xTaskGetTickCount();
    while (1)
    {
        dmx_send(dmx_num);
        vTaskDelayUntil(&last, pdMS_TO_TICKS(30));
    }
}
