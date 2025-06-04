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

// void blink_debug_led(int times, int delay_ms)
// {
//     for (int i = 0; i < times; ++i)
//     {
//         gpio_set_level(DEBUG_LED_GPIO, 1);
//         vTaskDelay(pdMS_TO_TICKS(delay_ms));
//         gpio_set_level(DEBUG_LED_GPIO, 0);
//         vTaskDelay(pdMS_TO_TICKS(delay_ms));
//     }
// }

static void led_status_task(void *arg)
{
    while (1)
    {
        if (!my_led_override_active())
        {
            int delay = dmx_error ? 100 : (!wifi_connected ? 500 : 1000);
            my_led_set(true); // LED an
            vTaskDelay(pdMS_TO_TICKS(delay));
            my_led_set(false); // LED aus
            vTaskDelay(pdMS_TO_TICKS(delay));
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(100));
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
        // fall trough
        __attribute__((fallthrough)); // Ensure we handle 'P' like 'C' (No warnings about fallthrough)
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
            // blink_debug_led(1, 20);
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

    // // Debug LED konfigurieren
    // gpio_config_t io_conf = {
    //     .pin_bit_mask = 1ULL << DEBUG_LED_GPIO,
    //     .mode = GPIO_MODE_OUTPUT,
    //     .pull_up_en = 0,
    //     .pull_down_en = 0,
    //     .intr_type = GPIO_INTR_DISABLE};
    // gpio_config(&io_conf);
    // gpio_set_level(DEBUG_LED_GPIO, 1); // LED an beim Start
    my_led_init(DEBUG_LED_GPIO);

    // WLAN verbinden
    my_wifi_init();
    // ESP_ERROR_CHECK(example_connect());
    // wifi_connected = true;
    my_led_set_wifi_status(true);
    wifi_connected = my_wifi_is_connected();

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
    // blink_debug_led(2, 200); // 2x blinken zur Bestätigung
    my_led_blink(2, 200);

    TickType_t last = xTaskGetTickCount();
    while (1)
    {
        dmx_send(dmx_num);
        vTaskDelayUntil(&last, pdMS_TO_TICKS(30));
    }
}
