#include "my_led.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

static int led_gpio = 2;
static volatile bool led_override = false;
static volatile bool wifi_connected = false;
static volatile bool dmx_error = false;

// LED-Steuerungs-Task
static void led_status_task(void *arg)
{
    while (1)
    {
        if (!led_override)
        {
            int delay = dmx_error ? 100 : (wifi_connected ? 1000 : 500);
            gpio_set_level(led_gpio, 1);
            vTaskDelay(pdMS_TO_TICKS(delay));
            gpio_set_level(led_gpio, 0);
            vTaskDelay(pdMS_TO_TICKS(delay));
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// Initialisierung
void my_led_init(int gpio)
{
    led_gpio = gpio;

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << led_gpio,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf);

    gpio_set_level(led_gpio, 0); // LED aus

    xTaskCreate(led_status_task, "led_status_task", 2048, NULL, 2, NULL);
}

void my_led_set(bool on)
{
    gpio_set_level(led_gpio, on ? 1 : 0);
}

// Override aktivieren/deaktivieren
void my_led_set_override(bool active)
{
    led_override = active;
}

bool my_led_get_override(void)
{
    return led_override;
}

// Manuelles Blinken (z. B. beim Netzwerkwechsel)
void my_led_blink(int times, int delay_ms)
{
    my_led_set_override(true);

    for (int i = 0; i < times; ++i)
    {
        gpio_set_level(led_gpio, 1);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        gpio_set_level(led_gpio, 0);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }

    my_led_set_override(false);
}

// WLAN-Status setzen
void my_led_set_wifi_status(bool connected)
{
    wifi_connected = connected;
}

// DMX-Fehlerstatus setzen
void my_led_set_dmx_error(bool error)
{
    dmx_error = error;
}
