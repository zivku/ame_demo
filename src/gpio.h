#ifndef GPIO_CONTROL_H
#define GPIO_CONTROL_H

#include <linux/gpio.h>

int gpio_init_output(const char* gpio_path, int gpio_pin);
void gpio_set_value_fast(int line_fd, int value);
int gpio_init_input(const char* gpio_path, int gpio_pin, int mode);
int gpio_get_value_fast(int line_fd);


void gpio_output(const char* gpio_path, int gpio_pin, int value);


int gpio_input(const char* gpio_path, int gpio_pin, int mode);

#endif 
