#ifndef MAIN_H
#define MAIN_H
#include <egt/ui>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <condition_variable>
#include <cstdlib>
#include <vector>
#include <planes/plane.h>
#include "egt/detail/screen/kmsoverlay.h"
#include "egt/detail/screen/kmsscreen.h"
#include <sys/time.h>
#include <string.h>
#include <dirent.h>
#include <thread>
#include <unistd.h>
#include <limits.h>
#include <filesystem>
#include <optional>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <cstdio>
#include <random>
#include <mutex>
#include "uart.h"
#include "sd.h"
#include "gpio.h"
#include "math.h"
#include "adc.h"
#define PROJECT_NAME "holystone_ame"
#define VERSION "v2.0"
#define CONTROL_BOARD_BUFFER_SIZE 1024  // 緩衝區大小
#define CONTROL_BOARD_MAX_PACKET_SIZE 64     // 假設最大封包大小

uint8_t control_board_buffer[CONTROL_BOARD_BUFFER_SIZE];  // 用來儲存從 UART 讀取的資料
size_t control_board_buffer_len = 0;  // 當前緩衝區的資料長度

const char* GPIO_PATH = "/dev/gpiochip0";
const char* RTC_BAT_PATH = "/sys/bus/iio/devices/iio:device0/in_voltage6_raw";
const char* WIFI_UART1_PATH = "/dev/ttyS1";
const char* CONTROL_UART2_PATH = "/dev/ttyS2";
const std::string sd_device = "/dev/mmcblk0p1";
const std::string mount_point = "/mnt/sd";
int BUZZER_GPIO = 76;
int WIFI_RESET_GPIO = 86;
int CONNTEC_GPIO = 95;
int SYSTEM_LED_GPIO = 96;
std::mutex fail_log_mutex;
std::mutex log_mutex;
std::mutex ct_data_mutex;
namespace fs = std::filesystem;
using namespace std;
using namespace egt;
bool page31_received = false;
bool page32_received = false;
bool page33_received = false;
bool has_ct_data = false;
bool CT_group1_status = false;
bool CT_group2_status = false;
bool CT_group3_status = false;
int current_ct_group;
int current_ct_item;
std::string wifi_status_text = "WiFi status not set";
bool wifi_reset_status = true;
bool backlight_status = false;
bool network_ok = true;


static egt::Image img_battery_empty("icon:battery_empty.png;64");
static egt::Image img_battery_low("icon:battery_low.png;64");
static egt::Image img_battery_normal("icon:battery.png;64");
static egt::Image img_clouda("icon:clouda.png;64");
static egt::Image img_warning("icon:warning.png;64");
static egt::Image img_cancel("icon:cancel.png;64");
static egt::Image img_wifi("icon:wifi.png;64");
static egt::Image img_white("file:image/white_rectangle.png");
static egt::Image img_green("file:image/green_rectangle.png");
std::vector<std::vector<std::string>> ct_current_data_list1 = {
    {"CT1", "CT2", "CT3","CT4","CT5","CT6","CT7","CT8"},
    {"0.00 A", "0.00 A", "0.00 A", "0.00 A", "0.00 A", "0.00 A", "0.00 A", "0.00 A"}
};
std::vector<std::vector<std::string>> ct_current_data_list2 = {
    {"CT9", "CT10", "CT11","CT12","CT13","CT14","CT15","CT16","CT17","CT18"},
    {"0.00 A", "0.00 A", "0.00 A", "0.00 A", "0.00 A", "0.00 A", "0.00 A", "0.00 A", "0.00 A", "0.00 A"}
};
std::vector<std::vector<std::string>> ct_current_data_list3 = {
    {"CT19", "CT20", "CT21","CT22","CT23","CT24","CT25","CT26","CT27","CT28","CT29","CT30"},
    {"0.00 A", "0.00 A", "0.00 A", "0.00 A", "0.00 A", "0.00 A", "0.00 A", "0.00 A", "0.00 A", "0.00 A", "0.00 A", "0.00 A"}
};
typedef struct {
    int log_time_delay = 5;           // 日誌延遲時間 (預設值)
    int log_retention_days = 7;       // 日誌保存天數 (預設值)
    int network_detection_delay = 1;  // 網絡檢測延遲時間 (預設值)
    int wifi_batch_size = 30;         // WiFi 批次大小 (預設值)
    int max_failed_log_cache = 10;
    int backlight_sleep_timer = 30;
    int backlight_sleep_level = 6;
    int poweroff_timer = 2;
} config_t;
enum class TIMESET_TYPE { DATE, TIME };
struct DateTime {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
};
typedef enum {
    STATUS_EMPTY,
    STATUS_LOW,
    STATUS_NORMAL
} VoltageStatus;
typedef enum {
    NOT_IN_TIME_SETTING,
    IN_TIME_SETTING
} TimeSettingState;
typedef struct {
    TimeSettingState current;
    TimeSettingState previous;
    bool initialized;
} PageState;
enum TransmitResult {
    TRANSMIT_OK = 1,                            // 傳送成功
    TRANSMIT_SEND_FAIL = -1,                    // 傳送失敗
    TRANSMIT_CHECKSUM_ERROR = -2,               // CRC 錯誤
    TRANSMIT_WIFI_ERROR_NOT_INIT = -11,         // Wi-Fi 未初始化
    TRANSMIT_WIFI_ERROR_NOT_STARTED = -12,      // Wi-Fi 未啟動
    TRANSMIT_WIFI_ERROR_NOT_CONNECTED = -13,    // Wi-Fi 未連接 AP
    TRANSMIT_WIFI_ERROR_INVALID_ARG = -14,      // 無效參數
    TRANSMIT_WIFI_ERROR_UNKNOWN = -15,          // 未知 Wi-Fi 錯誤
    TRANSMIT_UNKNOWN_RESPONSE = -4              // 回應不明
};
typedef struct __attribute__((packed)) {
    uint8_t ct_num;
    uint8_t function;
    uint8_t data_len;
    uint8_t high_byte;
    uint8_t low_byte;
    uint8_t crc_low;
    uint8_t crc_high;
} TransmitCurrentPacket;

typedef struct __attribute__((packed)) {
    uint8_t power_status : 1;    // Bit 0: 開關機狀態
    uint8_t action_status : 1;   // Bit 1: 動作中
    uint8_t learning_mode : 1;   // Bit 2: 學習中
    uint8_t test_mode : 1;       // Bit 3: 測試模式
    uint8_t sos_mode : 1;        // Bit 4: SOS 模式
    uint8_t pause_output : 1;    // Bit 5: 暫停輸出
    uint8_t error_output : 1;    // Bit 6: 錯誤輸出
    uint8_t sys_time : 1;        // Bit 7: 系統時間
} Flag1_t;

typedef struct __attribute__((packed)) {
    uint8_t sw1_setting : 1;     // Bit 0: 設定SW_1
    uint8_t sw2_setting : 1;     // Bit 1: 設定SW_2
    uint8_t sw3_setting : 1;     // Bit 2: 設定SW_3
    uint8_t reserved : 5;        // Bit 3-7: 保留位
} Flag2_t;

typedef struct __attribute__((packed)) {
    uint8_t reserved : 8;         // Bit 0-7: 都是保留位
} Flag3_t;
typedef enum {
    CMD_NONE = 0x00,            // 什麼都不做
    CMD_ON = 0x01,              // 開啟
    CMD_OFF = 0x02,             // 關閉
    CMD_STOP = 0x09,            // 停止
    CMD_ASK = 0x0A,             // 詢問命令
    CMD_RETURN = 0x0B,          // 回傳命令
    CMD_SEND = 0x0C,            // 傳送命令
    CMD_LEARN_MODE = 0x10,      // 學習模式
    CMD_LEARN_SUCCESS = 0x11,   // 學習成功
    CMD_EMERGENCY_LV1 = 0x1D,   // 緊急LEVEL 1
    CMD_EMERGENCY_LV2 = 0x1E,   // 緊急LEVEL 2
    CMD_EMERGENCY_LV3 = 0x1F,   // 緊急LEVEL 3（強制結束）
    CMD_READ_CURRENT = 0x23,    // 讀取電流
    CMD_SYS_TIME = 0x40,        // 系統時間
    CMD_MAX = 0x4F              // 最大命令代碼
} Command_t;
typedef struct __attribute__((packed)) {
    uint16_t start;           // 0xAA55 (Little-endian: 低位在前）
    uint8_t length;           // 資料總長（含 header 以後）
    uint16_t host_id;         // 主機 ID（Little-endian: 低位在前）
    uint8_t customer_code;    // 客戶代號
    uint8_t model_code;       // 機型代號
    uint8_t signal_mode;      // 信號模式 A0/Bx/Cx
    uint8_t page_number;      // 資料頁面
    uint8_t error_code;       // 錯誤碼
    uint8_t command;        // 命令 Function
    uint8_t process_flag; 
    Flag1_t flag1;            // 解析 data[0]
    Flag2_t flag2;            // 解析 data[1]
    Flag3_t flag3;            // 解析 data[2]
    uint8_t data[24];         // 剩餘資料
    uint8_t checksum;         // CRC 檢查碼
} ControlPacket_t;

typedef struct __attribute__((packed)) {
    uint16_t start;           // 0xAA55 (little-endian: 實際送出為 0x55, 0xAA)
    uint8_t length;           // 資料長度 (例如: 0x11)
    uint16_t host_id;         // 主機 ID（Little-endian: 低位在前）0x1E25
    uint8_t customer_code;    // 客戶代號 0x03
    uint8_t model_code;       // 機型代號 0x29
    uint8_t signal_mode;      // 信號模式 A0/Bx/Cx  0xB9
    uint8_t page_number;      // 資料頁面           0x11
    uint8_t error_code;       // 錯誤碼             0x00
    uint8_t command;          // 命令 Function      0x40
    uint8_t hour;             // 時
    uint8_t minute;           // 分
    uint8_t second;           // 秒
    uint8_t reserved2[5];     // 0x00 x5
    uint8_t checksum;         // 加總 [0~18] + 0x66
    uint8_t tail;             // 0x00
} SetTime_ControlPacket_t;

bool set_brightness(int level);
uint16_t calibrate_current(uint16_t value, bool sw3_setting, bool ct_group3_status);
VoltageStatus voltage_to_status(float voltage);
void buzzer_on_button_click(void);
void beep(void);
std::string format_current_value(uint16_t value);
void update_state(PageState *state, TimeSettingState new_state, const DateTime& current_time);
void write_controlboard_set_time(void);
void print_control_packet(const ControlPacket_t* pkt);
bool validate_control_packet_checksum(const ControlPacket_t* pkt);
void read_controlboard_data_thread(void);
void send_ct_log_towifi_loop(const std::string& fail_log_filename, int log_retention_days, int max_failed_log_cache);
void resned_ct_log_towifi_loop(const std::string& fail_log_filename, int wifi_batch_size);
void resend_wifi_FailedLog(const std::string& fail_log_filename, int wifi_batch_size);
int uartTransmitCurrentToWiFi(int ct_num, float current);
void WIFI_reset(void);
void WIFI_reset_thread(void);
int copy_csv_to_sd(const std::string& target_dir);
std::string get_current_date_string(void);
std::string get_current_time_string(void);
uint32_t convertDateTimeToUnixTimestamp(const std::string& date_time_str);
void log_current_data(const std::string& filename, const std::string& fail_log_filename,const std::vector<std::vector<std::vector<std::string>>>& data_sets, int log_retention_days, int max_failed_log_cache);
std::string format_date_time(const DateTime& dt);
std::string extract_mode_text(const std::string& input_text);
bool is_leap_year(int year);
int days_in_month(int year, int month);
bool isNetworkAvailable();
DateTime get_current_time();
void set_system_time(const DateTime& new_time);
std::vector<std::string> scan_wifi_devices();
void handle_wifi_selection(const std::string& device, egt::ScrolledView& scrollview, egt::PopupVirtualKeyboard& popup_keyboard);
void update_wifi_page(egt::VerticalBoxSizer& main_box, egt::VerticalBoxSizer& wifi_main_box, egt::ScrolledView& scrollview, egt::PopupVirtualKeyboard& popup_keyboard);

class EGT_API OverlayWindow : public egt::Window
{
public:
    OverlayWindow(const egt::Rect& rect, 
              egt::PixelFormat format_hint = egt::PixelFormat::yuv420,
              egt::WindowHint hint = egt::WindowHint::overlay)
        : egt::Window(rect, format_hint, hint)
    {
        allocate_screen();
        m_overlay = reinterpret_cast<egt::detail::KMSOverlay*>(screen());
		auto screen_ptr = screen();
		if (!screen_ptr)
		{
			std::cerr << "Screen allocation failed" << std::endl;
			return;
		}
		m_overlay = reinterpret_cast<egt::detail::KMSOverlay*>(screen_ptr);
		if (!m_overlay)
		{
			std::cerr << "Overlay casting failed" << std::endl;
			return;
		}
        assert(m_overlay);
        plane_set_pos(m_overlay->s(), 0, 0);
        plane_apply(m_overlay->s());
    }

    egt::detail::KMSOverlay* GetOverlay()
    {
        return m_overlay;
    }
	void hide_overlay()
    {
        if (m_overlay)
        {
            m_overlay->hide(); 
        }
    }
	void show_overlay()
    {
        if (m_overlay)
        {
            m_overlay->show(); 
        }
    }

private:
    egt::detail::KMSOverlay* m_overlay;
};
class Config {
public:
    bool load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open config file!" << std::endl;
            return false;
        }

        std::string line, section;
        while (std::getline(file, line)) {
            line = trim(line);

            if (line.empty() || line[0] == ';' || line[0] == '#')
                continue;

            if (line[0] == '[') {
                size_t pos = line.find(']');
                if (pos != std::string::npos) {
                    section = line.substr(1, pos - 1);
                }
                continue;
            }

            size_t eq_pos = line.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = trim(line.substr(0, eq_pos));
                std::string value = trim(line.substr(eq_pos + 1));
                config_data[section][key] = value;
            }
        }
        file.close();
        return true;
    }

    std::string get(const std::string& section, const std::string& key) {
        if (config_data.find(section) != config_data.end() &&
            config_data[section].find(key) != config_data[section].end()) {
            return config_data[section][key];
        }
        return "";
    }

private:
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> config_data;

    std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\n\r");
        size_t end = str.find_last_not_of(" \t\n\r");
        return (start == std::string::npos || end == std::string::npos) ? "" : str.substr(start, end - start + 1);
    }
};


#endif 