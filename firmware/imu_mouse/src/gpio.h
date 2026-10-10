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

/* Called from the system workqueue on each debounced press (true) and release (false). */
typedef void (*im_gpio_button_cb_t)(bool pressed);
bool im_gpio_button_init(im_gpio_button_cb_t cb);

bool im_gpio_led_toggle(unsigned int leds);
bool im_gpio_led_set(unsigned int leds, int value);
bool im_gpio_led_set_only(unsigned int leds);
bool im_gpio_led_toggle_only(unsigned int leds);

#endif