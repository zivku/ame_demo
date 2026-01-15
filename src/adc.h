#ifndef ADC_H
#define ADC_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define WINDOW_SIZE 5  // 移動平均視窗大小
// 說明：讀取 ADC 數值並回傳經平滑處理後的實際電壓（單位：V）
// 參數：adc_path - 指向 ADC 設備檔案路徑的字串，例如 "/sys/bus/iio/devices/iio:device0/in_voltage0_raw"
// 回傳值：回傳經過移動平均後並乘上 scale 的電壓值，錯誤時回傳 -1
float adc(const char* adc_path);
int adc_init(const char* adc_path);
float adc_read_fast(int fd);
#ifdef __cplusplus
}
#endif

#endif // ADC_READER_H
