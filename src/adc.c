#include "adc.h"
float adc(char* adc_path)
{
    static int readings[WINDOW_SIZE] = {0};
    static int index = 0;
    static int count = 0;

    int fd, ret = 0;
    char buf[6] = "";
    float scale = 0.201416015;

    fd = open(adc_path, O_RDONLY, 0);
    if (fd < 0)
    {
        printf("open file error\n");
        return -1;
    }

    ret = read(fd, buf, 5);
    if (ret < 0)
    {
        printf("read error\n");        
        close(fd);
        return -1;
    }

    int analog_single = atoi(buf);
    close(fd);

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
