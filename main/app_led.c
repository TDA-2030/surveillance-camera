/*
 * led.c
 *
 *
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "hal/gpio_ll.h"
#include "esp_log.h"
#include "app_led.h"

static const char *TAG = "led";


static bool light_state = true;
static gpio_num_t g_led_gpio = 0;
static uint8_t led_times = 0;
static uint8_t stage = 2;
static uint16_t times_cnt = 0;
static uint16_t times = 0;
static uint16_t cnt = 0;
static uint8_t led_seq[10] = {2, 6};
static uint8_t g_index = 0;
static uint8_t total_num = 2;

uint8_t _led_seq_1[2] = {1, 10};
uint8_t _led_seq_2[4] = {1, 1, 1, 6};


static void led_flash_run(void)
{
    switch (stage) {
    case 0:
        led_setlow();
        if (++cnt > led_seq[g_index]) {
            stage = 1;
            cnt = 0;
            g_index++;
        }

        break;
    case 1:
        led_sethigh();
        if (++cnt > led_seq[g_index]) {
            stage = 0;
            cnt = 0;
            g_index++;
        }
        break;
    default:
        break;
    }

    if (g_index >= total_num) {
        g_index = 0;
    }
}

static void periodic_led_callback(void *arg)
{
    led_flash_run();
}

void led_start_blink(void)
{
    stage = 1;
}

void led_stop_blink(void)
{
    stage = 2;
}

void led_set_seq(uint8_t seq_50ms[], uint8_t len)
{
    if (len > 10) {
        ESP_LOGE(TAG, "length is too large");
        return;
    }

    memcpy(led_seq, seq_50ms, len);
    total_num = len;
    stage = 1;
    cnt = 0;
    g_index=0;
}

void led_init(gpio_num_t led_gpio)
{
    gpio_config_t io_conf={0};
    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = 1ULL << led_gpio;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
    g_led_gpio = led_gpio;

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &periodic_led_callback,
        /* name is optional, but may help identify the timer when debugging */
        .name = "led",
    };

    esp_timer_handle_t periodic_timer;
    esp_timer_create(&periodic_timer_args, &periodic_timer);
    esp_timer_start_periodic(periodic_timer, 50000);
}

void led_sethigh(void)
{
    gpio_ll_set_level(&GPIO, g_led_gpio, 1);
}

void led_setlow(void)
{
    gpio_ll_set_level(&GPIO, g_led_gpio, 0);
}

void led_toggle(void)
{
    GPIO.out1.data ^= (1UL << (g_led_gpio - 32));
}
