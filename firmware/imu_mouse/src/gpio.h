#ifndef IM_GPIO_H
#define IM_GPIO_H

enum im_gpio_led
{
    IM_GPIO_LED_RED = 0,
    IM_GPIO_LED_GREEN,
    IM_GPIO_LED_BLUE
};

bool im_gpio_init(void);
bool im_gpio_led_toggle(enum im_gpio_led index);
bool im_gpio_led_set(enum im_gpio_led index, int value);
bool im_gpio_led_set_all(int value_red, int value_green, int value_blue);

#endif