#include "adc.h"

int adc_init(const char* adc_path) {
    int fd = open(adc_path, O_RDONLY);
    if (fd < 0) {
        perror("ADC init open error");
        return -1;
    }
    return fd;
}

// 快速讀取 ADC 函式
float adc_read_fast(int fd) {
    static int readings[WINDOW_SIZE] = {0};
    static int index = 0;
    static int count = 0;

    if (fd < 0) return -1.0f;

    char buf[16];
    float scale = 0.201416015 / 1000.0;

    // --- 關鍵：將讀取位置移回檔案開頭 ---
    lseek(fd, 0, SEEK_SET); 

    int ret = read(fd, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return -1.0f;
    }

    buf[ret] = '\0';
    int analog_single = atoi(buf);

    // 移動平均邏輯
    readings[index] = analog_single;
    index = (index + 1) % WINDOW_SIZE;
    if (count < WINDOW_SIZE) count++;

    int sum = 0;
    for (int i = 0; i < count; i++)
        sum += readings[i];

    float average = (float)sum / count;
    return average * scale;
}

float adc(const char* adc_path)
{
    static int readings[WINDOW_SIZE] = {0};
    static int index = 0;
    static int count = 0;

    int fd, ret = 0;
    char buf[16] = ""; // 稍微放大緩衝區更安全
    float scale = 0.201416015 / 1000.0;

    fd = open(adc_path, O_RDONLY, 0);
    if (fd < 0)
    {
        // 頻繁列印會消耗資源，建議在發生錯誤時才列印
        perror("ADC open file error");
        return -1;
    }

    // 清空緩衝區並讀取
    ret = read(fd, buf, sizeof(buf) - 1);
    if (ret <= 0)
    {
        perror("ADC read error");        
        close(fd);
        return -1;
    }
    
    buf[ret] = '\0'; // 務必補上結尾符，確保 atoi 安全
    int analog_single = atoi(buf);
    close(fd); // 確保每次讀取完畢後關閉 FD

    // 保存到移動平均視窗中
    readings[index] = analog_single;
    index = (index + 1) % WINDOW_SIZE;
    if (count < WINDOW_SIZE) count++;

    // 計算平均
    int sum = 0;
    for (int i = 0; i < count; i++)
        sum += readings[i];

    float average = (float)sum / count;

    return average * scale;
}