#include <stdio.h>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <algorithm>
#include <sys/ioctl.h>
#include <linux/gpio.h>
#include "gpio.h"

int gpio_init_output(const char* gpio_path, int gpio_pin) {
    int fd = open(gpio_path, O_RDWR);
    if (fd == -1) {
        perror("GPIO open chip error");
        return -1;
    }

    struct gpiohandle_request req = {0};
    req.lines = 1;
    req.lineoffsets[0] = gpio_pin;
    req.flags = GPIOHANDLE_REQUEST_OUTPUT;

    int ret = ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, &req);
    
    // 關鍵：拿到 Line Handle 後，原本的 chip fd 就可以關掉了
    close(fd); 

    if (ret == -1) {
        perror("GPIO get linehandle error");
        return -1;
    }

    return req.fd; // 返回這個持久的 Line Handle
}
void gpio_set_value_fast(int line_fd, int value) {
    if (line_fd < 0) return;

    struct gpiohandle_data data = {0};
    data.values[0] = value;
    if (ioctl(line_fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data) == -1) {
        perror("GPIO set value error");
    }
}
int gpio_init_input(const char* gpio_path, int gpio_pin, int mode) {
    int fd = open(gpio_path, O_RDWR);
    if (fd == -1) {
        perror("GPIO open chip error");
        return -1;
    }

    struct gpiohandle_request req = {0};
    req.lines = 1;
    req.lineoffsets[0] = gpio_pin;
    
    // 根據 mode 設定 Flags
    if (mode == 0) {
        req.flags = GPIOHANDLE_REQUEST_INPUT | GPIOHANDLE_REQUEST_ACTIVE_LOW;
    } else {
        req.flags = GPIOHANDLE_REQUEST_INPUT;
    }

    int ret = ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, &req);
    
    // 拿到 Line Handle 後就關閉 chip fd
    close(fd);

    if (ret == -1) {
        perror("GPIO get input linehandle error");
        return -1;
    }

    return req.fd;
}

/**
 * @brief 快速讀取 GPIO 數值
 * @param line_fd 已經由 gpio_init_input 獲取的 FD
 * @return 返回 0 或 1，失敗返回 -1
 */
int gpio_get_value_fast(int line_fd) {
    if (line_fd < 0) return -1;

    struct gpiohandle_data data = {0};
    if (ioctl(line_fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) == -1) {
        perror("GPIO get value error");
        return -1;
    }

    return data.values[0];
}
void gpio_output(const char* gpio_path, int gpio_pin, int value)
{
    int fd = open(gpio_path, O_RDWR);
    if (fd == -1) {
        perror("Error opening GPIO chip");
        return; // 不要 exit(1)，避免整個程序在出錯時導致資源回收不完全
    }

    gpiohandle_request req = {0};
    req.lines = 1;
    req.lineoffsets[0] = gpio_pin;
    req.flags = GPIOHANDLE_REQUEST_OUTPUT;

    int ret = ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, &req);
    if (ret == -1) {
        perror("Error in GPIO_GET_LINEHANDLE_IOCTL (output)");
        close(fd); // 必須補上！
        return;
    }

    gpiohandle_data data = {0};
    data.values[0] = value;
    ret = ioctl(req.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
    if (ret == -1) {
        perror("Error in GPIOHANDLE_SET_LINE_VALUES_IOCTL");
    }

    close(req.fd);
    close(fd);
}

int gpio_input(const char* gpio_path, int gpio_pin, int mode) {
    int fd = open(gpio_path, O_RDWR);
    if (fd == -1) {
        perror("Error opening GPIO chip");
        return -1;
    }

    gpiohandle_request req = {0};
    req.lines = 1;
    req.lineoffsets[0] = gpio_pin;
    if (mode == 0) {
        req.flags = GPIOHANDLE_REQUEST_INPUT | GPIOHANDLE_REQUEST_ACTIVE_LOW;
    } else {
        req.flags = GPIOHANDLE_REQUEST_INPUT;
    }

    int ret = ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, &req);
    if (ret == -1) {
        perror("Error in GPIO_GET_LINEHANDLE_IOCTL (input)");
        close(fd); // 必須補上！
        return -1;
    }

    gpiohandle_data data = {0};
    ret = ioctl(req.fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data);
    if (ret == -1) {
        perror("Error in GPIOHANDLE_GET_LINE_VALUES_IOCTL");
    }

    int gpio_value = data.values[0];
    close(req.fd);
    close(fd);
    return gpio_value;
}