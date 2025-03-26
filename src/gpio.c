#include <stdio.h>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <algorithm>
#include <sys/ioctl.h>
#include <linux/gpio.h>
#include "gpio.h"
using namespace std;
using std::cout;
using std::endl;
using std::reverse;
using std::string;
void gpio_output(char* gpio_path, int gpio_pin, int value)
{
    int fd;
    fd = open(gpio_path, O_RDWR);
    if (fd == -1)
    {
        perror("Error opening GPIO chip");
        exit(1);
    }
    int ret;
    gpiohandle_data data = {0};
    data.values[0] = value;
    gpiohandle_request req = {0};
    req.lines = 1;
    req.lineoffsets[0] = gpio_pin;
    req.flags = GPIOHANDLE_REQUEST_OUTPUT;
    // req.default_values[0] = value;

    ret = ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, &req);
    if (ret == -1)
    {
        perror("Error in GPIO_GET_LINEHANDLE_IOCTL gpio output : ");
        exit(1);
    }
    ret = ioctl(req.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
    if (ret == -1)
    {
        perror("Error in GPIOHANDLE_GET_LINE_VALUES_IOCTL gpio output : ");
        exit(1);
    }

    close(req.fd);
    close(fd);
}
int gpio_input(char* gpio_path, int gpio_pin, int mode) {
    int fd;
    fd = open(gpio_path, O_RDWR);
    if (fd == -1) {
        perror("Error opening GPIO chip");
        exit(1);
    }

    int ret;
    gpiohandle_data data = {0};
    gpiohandle_request req = {0};
    req.lines = 1;
    req.lineoffsets[0] = gpio_pin;
    if (mode == 0) {
        req.flags = GPIOHANDLE_REQUEST_INPUT | GPIOHANDLE_REQUEST_ACTIVE_LOW;
    } else {
        req.flags = GPIOHANDLE_REQUEST_INPUT;
    }

    ret = ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, &req);
    if (ret == -1) {
        perror("Error in GPIO_GET_LINEHANDLE_IOCTL gpio input : ");
        exit(1);
    }

    ret = ioctl(req.fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data);
    if (ret == -1) {
        perror("Error in GPIOHANDLE_GET_LINE_VALUES_IOCTL gpio input : ");
        exit(1);
    }

    int gpio_value = data.values[0];
    close(req.fd);
    close(fd);
    return gpio_value;
}