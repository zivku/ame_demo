#ifndef UART_H
#define UART_H

#include <stdint.h>  // 用于 uint8_t 类型
#include <termios.h> // 用于 speed_t 类型
#include <stdlib.h>
#include <cstring>

// 定义 UART 缓冲区大小
#define BUF_SIZE 128

/**
 * @brief 通过 UART 发送数据并接收响应数据。
 * @param DEV_PATH UART 设备路径，例如 "/dev/ttyS0"。
 * @param baudRate 波特率，例如 B115200 或 B9600。
 * @param vtime 超时时间，单位为 100ms。
 * @param tx_data 待发送的数据数组。
 * @param tx_len 待发送数据的长度。
 * @param received_bytes 接收数据的字节数，作为输出参数。
 * @return 接收到的数据缓冲区指针，需要调用者释放内存。
 */
uint8_t* uart_transmit(const char* DEV_PATH, speed_t baudRate, int vtime, uint8_t tx_data[], int tx_len, int& received_bytes);

/**
 * @brief 通过 UART 接收数据。
 * @param DEV_PATH UART 设备路径，例如 "/dev/ttyS0"。
 * @param baudRate 波特率，例如 B115200 或 B9600。
 * @param vtime 超时时间，单位为 100ms。
 * @param expected_bytes 預期收滿的byte，收滿即退出
 * @param received_bytes 接收数据的字节数，作为输出参数。
 * @return 接收到的数据缓冲区指针，需要调用者释放内存。
 */
 uint8_t* uart_read(const char* DEV_PATH, speed_t baudRate, int vtime, int expected_bytes, int&received_bytes);

/**
 * @brief 通过 UART 发送数据。
 * @param DEV_PATH UART 设备路径，例如 "/dev/ttyS0"。
 * @param baudRate 波特率，例如 B115200 或 B9600。
 * @param vtime 超时时间，单位为 100ms。
 * @param tx_data 待发送的数据数组。
 * @param tx_len 待发送数据的长度。
 * @return 成功返回 1，失败返回 -1。
 */
int uart_write(const char* DEV_PATH, speed_t baudRate, int vtime, uint8_t tx_data[], int tx_len);

#endif // UART_H
