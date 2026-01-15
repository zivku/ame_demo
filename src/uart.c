#include <stdio.h>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cstring>
#include "uart.h"

#define BUF_SIZE 128

int uart_init(const char* DEV_PATH, speed_t baudRate, int vtime) {
    int fd = open(DEV_PATH, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("UART open error");
        return -1;
    }

    struct termios options;
    if (tcgetattr(fd, &options) < 0) {
        close(fd);
        return -1;
    }

    // 設定通訊參數 (設定一次即可)
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cc[VTIME] = vtime; 
    options.c_cc[VMIN] = 0;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_oflag &= ~OPOST;
    options.c_iflag &= ~(ICRNL | IXON);
    cfsetispeed(&options, baudRate);
    cfsetospeed(&options, baudRate);
    options.c_cflag |= (CLOCAL | CREAD);
    
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &options);
    return fd;
}
int uart_transmit_fast(int fd, uint8_t tx_data[], int tx_len, uint8_t* rx_data, int& received_bytes) {
    if (fd < 0) return -1;

    memset(rx_data, 0, BUF_SIZE);
    int ret = write(fd, tx_data, tx_len);
    if (ret != tx_len) return -1;

    int total = 0;
    while (total < BUF_SIZE - 1) {
        uint8_t byte;
        int r = read(fd, &byte, 1);
        if (r > 0) {
            rx_data[total++] = byte;
            if (byte == '\n') break;
        } else break;
    }
    received_bytes = total;
    rx_data[total] = '\0';
    return 0;
}
int uart_read_fast(int fd, uint8_t* rx_buffer, int expected_bytes, int& received_bytes) {
    if (fd < 0 || rx_buffer == nullptr) return -1;

    received_bytes = 0;
    memset(rx_buffer, 0, BUF_SIZE);

    // 安全讀取：防止超過緩衝區大小 (BUF_SIZE-1 留給 \0)
    while (received_bytes < expected_bytes && received_bytes < BUF_SIZE - 1) {
        int to_read = (expected_bytes < BUF_SIZE - 1) ? expected_bytes : BUF_SIZE - 1;
        int r = read(fd, rx_buffer + received_bytes, to_read - received_bytes);
        
        if (r > 0) {
            received_bytes += r;
        } else if (r == 0) {
            break; // Timeout
        } else {
            perror("uart_read_fast error");
            return -1;
        }
    }
    
    rx_buffer[received_bytes] = '\0'; 
    return 0;
}
int uart_write_fast(int fd, uint8_t tx_data[], int tx_len) {
    if (fd < 0 || tx_data == nullptr) return -1;

    int ret = write(fd, tx_data, tx_len);
    if (ret != tx_len) {
        perror("uart_write_fast error");
        return -1;
    }
    
    return ret; 
}
uint8_t* uart_transmit(const char* DEV_PATH, speed_t baudRate, int vtime, uint8_t tx_data[], int tx_len, int& received_bytes)
{
    int fd = -1;
    received_bytes = 0;
    
    // 先分配記憶體，確保回傳指標不為空（依據原程式邏輯）
    uint8_t* rx_data = new uint8_t[BUF_SIZE];
    memset(rx_data, 0, BUF_SIZE);

    fd = open(DEV_PATH, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        printf("ERROR open %s ret=%d\n\r", DEV_PATH, fd);
        return rx_data; 
    }

    struct termios options;
    if (tcgetattr(fd, &options) < 0) {
        perror("tcgetattr");
        close(fd);
        return rx_data;
    }

    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cc[VTIME] = vtime; 
    options.c_cc[VMIN] = 0;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_oflag &= ~OPOST;
    options.c_iflag &= ~(ICRNL | IXON);
    cfsetispeed(&options, baudRate);
    cfsetospeed(&options, baudRate);
    options.c_cflag |= (CLOCAL | CREAD);
    
    tcflush(fd, TCIFLUSH);
    if (tcsetattr(fd, TCSANOW, &options) < 0) {
        perror("tcsetattr");
        close(fd);
        return rx_data;
    }
    
    int ret = write(fd, tx_data, tx_len);
    if (ret != tx_len) {
        printf("ERROR write ret=%d\n", ret);
        // 即使寫入失敗也嘗試讀取或直接關閉，依據原邏輯繼續往下跑
    }

    int total = 0;
    // 限制 total < BUF_SIZE - 1 留一格給 \0，避免越界
    while (total < BUF_SIZE - 1) {
        uint8_t byte;
        ret = read(fd, &byte, 1);
        if (ret > 0) {
            rx_data[total++] = byte;
            if (byte == '\n') break;
        } else {
            break; // timeout or error
        }
    }

    received_bytes = total;
    rx_data[total] = '\0'; 
    
    close(fd); // 確保關閉
    return rx_data;
}

uint8_t* uart_read(const char* DEV_PATH, speed_t baudRate, int vtime, int expected_bytes, int& received_bytes)
{
    int fd = -1;
    received_bytes = 0;
    
    fd = open(DEV_PATH, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        printf("ERROR open %s ret=%d\n\r", DEV_PATH, fd);
        return nullptr;
    }

    uint8_t* rx_data = new uint8_t[BUF_SIZE];
    memset(rx_data, 0, BUF_SIZE);

    struct termios options;
    if (tcgetattr(fd, &options) < 0) {
        perror("tcgetattr");
        close(fd);
        delete[] rx_data;
        return nullptr;
    }

    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cc[VTIME] = vtime; 
    options.c_cc[VMIN] = expected_bytes;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_oflag &= ~OPOST;
    options.c_iflag &= ~(ICRNL | IXON);
    cfsetispeed(&options, baudRate);
    cfsetospeed(&options, baudRate);
    options.c_cflag |= (CLOCAL | CREAD);
    
    tcflush(fd, TCIFLUSH);
    if (tcsetattr(fd, TCSANOW, &options) < 0) {
        perror("tcsetattr");
        close(fd);
        delete[] rx_data;
        return nullptr;
    }

    // 安全讀取：防止 received_bytes 超過緩衝區大小
    while (received_bytes < expected_bytes && received_bytes < BUF_SIZE - 1) {
        int to_read = (expected_bytes < BUF_SIZE - 1) ? expected_bytes : BUF_SIZE - 1;
        int ret = read(fd, rx_data + received_bytes, to_read - received_bytes);
        if (ret > 0) {
            received_bytes += ret;
        } else {
            break;
        }
    }

    close(fd);
    return rx_data;
}

int uart_write(const char* DEV_PATH, speed_t baudRate, int vtime, uint8_t tx_data[], int tx_len)
{
    int fd = open(DEV_PATH, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        printf("ERROR open %s ret=%d\n\r", DEV_PATH, fd);
        return -1;
    }

    struct termios options;
    if (tcgetattr(fd, &options) < 0) {
        close(fd);
        return -1;
    }

    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cc[VTIME] = vtime; 
    options.c_cc[VMIN] = 0;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_oflag &= ~OPOST;
    options.c_iflag &= ~(ICRNL | IXON);
    cfsetispeed(&options, baudRate);
    cfsetospeed(&options, baudRate);
    options.c_cflag |= (CLOCAL | CREAD);
    
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &options);

    int ret = write(fd, tx_data, tx_len);
    int result = (ret == tx_len) ? 1 : -1;
    
    close(fd); // 確保關閉
    return result;
}