#ifndef IM_GPIO_H
#define IM_GPIO_H

enum im_gpio_led
{
    IM_GPIO_LED_NONE  = 0,
    IM_GPIO_LED_RED   = 1 << 0,
    IM_GPIO_LED_GREEN = 1 << 1,
    IM_GPIO_LED_BLUE  = 1 << 2
};

void im_gpio_thread_start(void);
bool im_gpio_init(void);

bool im_gpio_led_toggle(unsigned int leds);
bool im_gpio_led_set(unsigned int leds, int value);
bool im_gpio_led_set_only(unsigned int leds_on);

#endif