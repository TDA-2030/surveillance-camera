/*
 * led.h
 *
 *  Created on: 2017年12月11日
 *      Author: ai-thinker
 */

#ifndef MAIN_LED_H_
#define MAIN_LED_H_


#ifdef __cplusplus 
extern "C" {
#endif

void led_init() ;
void led_flash_set(uint8_t times);
bool get_light_state(void) ;

void led_sethigh(void);
void led_setlow(void);
void led_toggle(void);

#ifdef __cplusplus 
}
#endif

#endif /* MAIN_LED_H_ */
