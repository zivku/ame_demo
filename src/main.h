#ifndef MAIN_H
#define MAIN_H
#include <egt/ui>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
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
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <cstdio>
#include <random>
#include "uart.h"
#include "sd.h"
#include "gpio.h"
#include "math.h"
const char* WIFI_UART1_PATH = "/dev/ttyS1";
const std::string sd_device = "/dev/mmcblk0p1";
const std::string mount_point = "/mnt/sd";
namespace fs = std::filesystem;
using namespace std;
using namespace egt;
std::vector<std::vector<std::string>> ct_current_data_list1 = {
    {"CT1", "CT2", "CT3","CT4","CT5","CT6","CT7","CT8"},
    {"11.1 A", "11.2 A", "11.3 A", "11.4 A", "11.5 A", "11.6 A", "11.7 A", "11.8 A"}
};
std::vector<std::vector<std::string>> ct_current_data_list2 = {
    {"CT9", "CT10", "CT11","CT12","CT13","CT14","CT15","CT16","CT17","CT18"},
    {"21.1 A", "21.2 A", "21.3 A", "21.4 A", "21.5 A", "21.6 A", "21.7 A", "21.8 A", "21.9 A", "22.0 A"}
};
std::vector<std::vector<std::string>> ct_current_data_list3 = {
    {"CT19", "CT20", "CT21","CT22","CT23","CT24","CT25","CT26","CT27","CT28","CT29","CT30"},
    {"31.1 A", "31.2 A", "31.3 A", "31.4 A", "31.5 A", "31.6 A", "31.7 A", "31.8 A", "31.9 A", "32.0 A", "32.1 A", "32.2 A"}
};
enum class TIMESET_TYPE { DATE, TIME };
struct DateTime {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
};
void printFailedLog(const std::string& fail_log_filename);
int uartTransmitCurrentToWiFi(int ct_num, float current);
void copy_csv_to_sd(const std::string& target_dir);
std::string get_current_date_string(void);
std::string get_current_time_string(void);
uint32_t convertDateTimeToUnixTimestamp(const std::string& date_time_str);
void log_current_data(const std::string& filename, const std::string& fail_log_filename,const std::vector<std::vector<std::vector<std::string>>>& data_sets, int log_retention_days);
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