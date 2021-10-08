/*
 * led.h
 *
 *  Created on: 2017年12月11日
 *      Author: ai-thinker
 */

#ifndef MAIN_LED_H_
#define MAIN_LED_H_

#include "driver/gpio.h"

#ifdef __cplusplus 
extern "C" {
#endif

uint8_t _led_seq_1[2];
uint8_t _led_seq_2[4];

void led_init(gpio_num_t led_gpio);
void led_set_seq(uint8_t seq_50ms[], uint8_t len);
bool get_light_state(void) ;

void led_sethigh(void);
void led_setlow(void);
void led_toggle(void);

#ifdef __cplusplus 
}
#endif

#endif /* MAIN_LED_H_ */
