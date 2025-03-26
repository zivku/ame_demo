#ifndef GPIO_CONTROL_H
#define GPIO_CONTROL_H

#include <linux/gpio.h>


void gpio_output(const char* gpio_path, int gpio_pin, int value);


int gpio_input(const char* gpio_path, int gpio_pin, int mode);

#endif 
