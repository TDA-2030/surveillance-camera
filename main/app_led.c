/*
 * led.c
 *
 *  
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"

#include "app_led.h"

#define LED_RED_PIN GPIO_NUM_33


static bool light_state = true;
static uint8_t led_update=0;
static uint8_t led_times=0;


void led_flash_set(uint8_t times)
{
    led_update = 1;
    led_times = times;
}

static void led_flash(void)
{
    const uint16_t high_time = 50/50;
    const uint16_t low_time = high_time;
    const uint16_t ending_time = 300/50;

    static uint8_t stage=0;
    static uint16_t times_cnt = 0;
    static uint16_t times = 0;
    static uint16_t cnt = 0;

    switch (stage)
    {
    case 0:
        if(times_cnt<times)
        {
            led_setlow();
            if(++cnt>high_time)
            {
                stage = 1;
                cnt = 0;
            }
        }else
        {
            stage = 2;
            cnt = 0;
        }
        break;
    case 1:
        led_sethigh();
        if(++cnt>low_time)
        {
            stage = 0;
            cnt = 0;
            times_cnt++;
        }
        break;
    case 2:
        
        if(++cnt>ending_time)
        {
            if(led_update)
            {
                led_update = 0;
                times = led_times;
            }
            stage = 0;
            times_cnt = 0;
            cnt = 0;
        }
        break;
    default:
        break;
    }
}


static void periodic_led_callback(void* arg)
{
    led_flash();
}

void led_init()
{
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = 1ULL << LED_RED_PIN;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    const esp_timer_create_args_t periodic_timer_args = {
            .callback = &periodic_led_callback,
            /* name is optional, but may help identify the timer when debugging */
            .name = "led",
    };

    esp_timer_handle_t periodic_timer;
    esp_timer_create(&periodic_timer_args, &periodic_timer);
	esp_timer_start_periodic(periodic_timer, 50000);

    
}

void open_light(void)
{
    light_state = true;
}

void close_light(void)
{
    light_state = false;
}

bool get_light_state(void)
{
    return light_state;
}



void led_sethigh(void)
{
    gpio_set_level(LED_RED_PIN, 1);
}

void led_setlow(void)
{
    gpio_set_level(LED_RED_PIN, 0);
}

void led_toggle(void)
{
    GPIO.out1.data ^= (1UL << (LED_RED_PIN - 32));   
}