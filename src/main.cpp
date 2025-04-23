#include "main.h"
void resend_wifi_FailedLog(const std::string& fail_log_filename, int wifi_batch_size) {
    std::lock_guard<std::mutex> lock(fail_log_mutex);
    std::fstream fail_log_file(fail_log_filename, std::ios::in | std::ios::out | std::ios::app);
    if (!fail_log_file.is_open()) {
        std::cerr << "Failed to open fail log file!" << std::endl;
        return;
    }
    if (fail_log_file.peek() == EOF) {
        std::cerr << "Fail log file is empty. Exiting function." << std::endl;
        fail_log_file.close();
        return;
    }
    std::string line;
    std::ofstream temp_file("temp_fail_log.csv");
    std::vector<std::string> failed_lines;
    int line_count = 0;

    while (std::getline(fail_log_file, line) && line_count < wifi_batch_size) {
        std::istringstream line_stream(line);
        std::string timestamp, ct_str, current_str, status;
        
        std::getline(line_stream, timestamp, ',');
        std::getline(line_stream, ct_str, ',');
        std::getline(line_stream, current_str, ',');
        std::getline(line_stream, status);

        timestamp.erase(0, timestamp.find_first_not_of(" "));
        timestamp.erase(timestamp.find_last_not_of(" ") + 1);

        uint32_t timestamp_value = convertDateTimeToUnixTimestamp(timestamp);
        uint8_t ct_num = static_cast<uint8_t>(std::stoi(ct_str));
        float current = std::stof(current_str);
        uint16_t current_value = static_cast<uint16_t>(current * 10);

        uint8_t transmit_current[] = {
            ct_num,
            0x80,
            static_cast<uint8_t>((timestamp_value >> 24) & 0xFF),
            static_cast<uint8_t>((timestamp_value >> 16) & 0xFF),
            static_cast<uint8_t>((timestamp_value >> 8) & 0xFF),
            static_cast<uint8_t>(timestamp_value & 0xFF),
            static_cast<uint8_t>(current_value >> 8),
            static_cast<uint8_t>(current_value & 0xFF),
            0x00, 0x00
        };

        uint16_t crc = calculate_crc16(transmit_current, sizeof(transmit_current) - 2);
        transmit_current[sizeof(transmit_current) - 2] = crc & 0xFF;
        transmit_current[sizeof(transmit_current) - 1] = (crc >> 8) & 0xFF;

        size_t transmit_current_len = sizeof(transmit_current);
        speed_t speed = B115200;
        int vtime = 0 * 10;
        int received_bytes = 0;
        uint8_t* recive = uart_transmit(WIFI_UART1_PATH, speed, vtime, transmit_current, transmit_current_len, received_bytes);
        std::string response(reinterpret_cast<char*>(recive), received_bytes);

        // 失敗的資料記錄
        if (response.find("send ok") == std::string::npos) {
            failed_lines.push_back(line);
        }

        line_count++;
    }

    // 打開臨時文件寫回失敗資料
    for (const auto& failed_line : failed_lines) {
        temp_file << failed_line << std::endl;
    }
    while (std::getline(fail_log_file, line)) {
        temp_file << line << std::endl;
    }

    fail_log_file.close();
    temp_file.close();

    // 刪除原始的 log 檔案
    if (std::remove(fail_log_filename.c_str()) != 0) {
        std::cerr << "Failed to remove original log file: " << fail_log_filename << std::endl;
        perror("Error");
    } else {
        std::cout << "Original log file removed successfully." << std::endl;
    }

    // 重命名臨時文件為原始 log 檔案名稱
    if (std::rename("temp_fail_log.csv", fail_log_filename.c_str()) != 0) {
        std::cerr << "Failed to rename temporary log file to: " << fail_log_filename << std::endl;
        perror("Error");
    } else {
        std::cout << "Temporary log file renamed successfully." << std::endl;
    }
}

int uartTransmitCurrentToWiFi(int ct_num, float current){
    uint16_t current_value = static_cast<uint16_t>(current * 10);

    TransmitCurrentPacket packet;
    packet.ct_num = static_cast<uint8_t>(ct_num);
    packet.function = 0x03;
    packet.data_len = 0x02;
    packet.high_byte = current_value >> 8;
    packet.low_byte = current_value & 0xFF;

    uint8_t* raw_ptr = reinterpret_cast<uint8_t*>(&packet);
    uint16_t crc = calculate_crc16(raw_ptr, sizeof(packet) - 2);
    packet.crc_low = crc & 0xFF;
    packet.crc_high = (crc >> 8) & 0xFF;

    speed_t speed = B115200;
    int vtime = 0 * 10;
    int received_bytes = 0;

    uint8_t *recive = uart_transmit(WIFI_UART1_PATH, speed, vtime, raw_ptr, sizeof(packet), received_bytes);
    std::string response(reinterpret_cast<char*>(recive), received_bytes);
    std::cout << "Received: " << response << std::endl;
    if (response.find("send ok") == std::string::npos) { 
        std::cerr << "Transmission failed! Logging failed data.\n\r" << std::endl;
        return -1;
    }
    else {
        std::cerr << "Transmis OK!\n\r" << std::endl;
        return 1;
    }
}
int copy_csv_to_sd(const std::string& target_dir) {
    for (const auto& entry : fs::directory_iterator(".")) {
        const auto& path = entry.path();
        std::string filename = path.filename().string();

        if (path.extension() == ".csv" && filename.rfind("current", 0) == 0) {
            std::string target_file = target_dir + "/" + filename;
            std::uintmax_t file_size = fs::file_size(path);

            // 嘗試確保空間足夠
            while (fs::space(target_dir).available < file_size) {
                // 找最舊的檔案刪除
                std::optional<fs::directory_entry> oldest;
                for (const auto& f : fs::directory_iterator(target_dir)) {
                    if (f.path().extension() != ".csv") continue;
                    if (!oldest || fs::last_write_time(f) < fs::last_write_time(*oldest)) {
                        oldest = f;
                    }
                }

                if (oldest) {
                    std::error_code ec;
                    fs::remove(oldest->path(), ec);
                    if (ec) {
                        std::cerr << "Failed to delete file: " << oldest->path() << " (" << ec.message() << ")" << std::endl;
                        return -1;
                    } else {
                        std::cout << "Deleted old file to free space: " << oldest->path().filename().string() << std::endl;
                    }
                } else {
                    std::cerr << "No files to delete. Not enough space for: " << filename << std::endl;
                    return -1;
                }
            }

            // 複製檔案
            std::lock_guard<std::mutex> lock(log_mutex);
            std::ifstream src(path, std::ios::binary);
            std::ofstream dest(target_file, std::ios::binary);

            if (src && dest) {
                dest << src.rdbuf();
                if (dest.fail()) {
                    std::cerr << "Write failed (disk may be full): " << filename << std::endl;
                    return -1;
                }
                std::cout << "Copy log : " << filename << std::endl;
            } else {
                std::cerr << "Copy log fail : " << filename << std::endl;
                return -1;
            }
        }
    }
    return 0;
}
bool is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}
int days_in_month(int year, int month) {
    static const int days_per_month[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    
    if (month < 1 || month > 12) return 0;
    if (month == 2 && is_leap_year(year)) return 29;
    return days_per_month[month - 1];
}
bool isNetworkAvailable()
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(80);
    inet_pton(AF_INET, "8.8.8.8", &server.sin_addr);

    bool success = (connect(sock, (struct sockaddr*)&server, sizeof(server)) == 0);
    close(sock);
    return success;
}
std::string format_date_time(const DateTime& dt) {
    std::ostringstream oss;
    oss << std::setw(4) << std::setfill('0') << dt.year << "."
        << std::setw(2) << std::setfill('0') << dt.month << "."
        << std::setw(2) << std::setfill('0') << dt.day << " "
        << std::setw(2) << std::setfill('0') << dt.hour << ":"
        << std::setw(2) << std::setfill('0') << dt.minute << ":"
        << std::setw(2) << std::setfill('0') << dt.second;
    return oss.str();
}
DateTime get_current_time() {
    auto now = std::chrono::system_clock::now();
    std::time_t current_time = std::chrono::system_clock::to_time_t(now);

    std::tm* tm_info = std::localtime(&current_time);

    DateTime dt;
    dt.year = tm_info->tm_year + 1900;  
    dt.month = tm_info->tm_mon + 1;     
    dt.day = tm_info->tm_mday;
    dt.hour = tm_info->tm_hour;
    dt.minute = tm_info->tm_min;
    dt.second = tm_info->tm_sec;

    return dt;
}
std::string get_current_date_string()
{
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm time_info;
    localtime_r(&time_t_now, &time_info);

    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d", &time_info);
    return std::string(buffer);
}
std::string get_current_time_string()
{
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm time_info;
    localtime_r(&time_t_now, &time_info);

    char buffer[80];
    strftime(buffer, sizeof(buffer), "%H:%M:%S", &time_info);
    return std::string(buffer);
}
uint32_t convertDateTimeToUnixTimestamp(const std::string& date_time_str) {
    struct tm tm = {};
    if (strptime(date_time_str.c_str(), "%Y-%m-%d %H:%M:%S", &tm) == nullptr) {
        std::cerr << "Error parsing date time!" << std::endl;
        return 0;
    }
    time_t timestamp = mktime(&tm);
    if (timestamp == -1) {
        std::cerr << "Error converting to timestamp!" << std::endl;
        return 0;
    }
    return static_cast<uint32_t>(timestamp);
}

void manage_log_files(const std::string& log_prefix, size_t max_files) {
    std::vector<fs::path> log_files;

    for (const auto& entry : fs::directory_iterator(".")) {
        if (entry.is_regular_file() && entry.path().filename().string().find(log_prefix) == 0) {
            log_files.push_back(entry.path());
        }
    }

    if (log_files.size() > max_files) {
        std::sort(log_files.begin(), log_files.end());

        while (log_files.size() > max_files) {
            std::cout << "Deleting old log file: " << log_files.front() << std::endl;
            fs::remove(log_files.front());
            log_files.erase(log_files.begin());
        }
    }
}
void manage_fail_log_size(const std::string& fail_log_filename, size_t max_lines) {
    std::ifstream file(fail_log_filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open fail log file!" << std::endl;
        return;
    }

    // 讀取所有行到 vector 中
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();

    // 檢查行數是否超過最大限制
    if (lines.size() > max_lines) {
        // 刪除最舊的行
        size_t lines_to_remove = lines.size() - max_lines;
        lines.erase(lines.begin(), lines.begin() + lines_to_remove);
    }

    // 重寫檔案，保證檔案行數不超過 max_lines
    std::ofstream out_file(fail_log_filename, std::ios::trunc);
    if (!out_file.is_open()) {
        std::cerr << "Failed to open fail log file for writing!" << std::endl;
        return;
    }

    // 寫回最新的資料
    for (const auto& l : lines) {
        out_file << l << std::endl;
    }
    out_file.close();
}
void create_log_file_if_not_exists(const std::string& filename) {
    std::ofstream log_file(filename, std::ios::app);
    if (!log_file.is_open()) {
        std::cerr << "open log file error!\n";
    }
}
void log_current_data(const std::string& filename, const std::string& fail_log_filename,const std::vector<std::vector<std::vector<std::string>>>& data_sets, int log_retention_days, int max_failed_log_cache) {
    manage_log_files("current_data_log_", log_retention_days);
    std::lock_guard<std::mutex> lock(log_mutex);
    std::ofstream log_file(filename, std::ios::app);
    if (!log_file.is_open()) {
        std::cerr << "open log file error!\n" << std::endl;
        return;
    }
    std::vector<std::string> failed_transmissions;
    log_file.seekp(0, std::ios::end);
    if (log_file.tellp() == 0) {
        log_file << "Timestamp, ";
        for (int i = 1; i <= 30; ++i) {
            log_file << "CT Group " << i << ", CT Data " << i << ", Current Value " << i;
            if (i != 30) log_file << ", ";
        }
        log_file << "\n";
    }

    std::string timestamp = get_current_date_string() + " " + get_current_time_string();
    log_file << timestamp << ", ";

    for (size_t set_index = 0; set_index < data_sets.size(); ++set_index) {
        const auto& current_data = data_sets[set_index];

        if (current_data.size() < 2) {
            std::cerr << "Invalid data format!" << std::endl;
            continue;
        }

        size_t column_count = current_data[0].size();
        std::string con_label = "CON" + std::to_string(set_index + 1);

        for (size_t j = 0; j < column_count; ++j) {
            std::string ct_name = (j < current_data[0].size()) ? current_data[0][j] : "";
            std::string data = (j < current_data[1].size()) ? current_data[1][j] : "";

            if (data.size() >= 2 && data.substr(data.size() - 2) == " A") {
                data.erase(data.size() - 2);
            }
            
            log_file << con_label << ", " << ct_name << ", " << data << ", ";
            float current = std::stof(data);
            int ct_num = std::stoi(ct_name.substr(2));
            int uart_to_wifi_ret = uartTransmitCurrentToWiFi(ct_num, current);
            if (uart_to_wifi_ret < 0) {
                failed_transmissions.push_back(timestamp + ", " + std::to_string(ct_num) + ", " + std::to_string(current) + ", Transmission failed\n");
            }
        }
    }

    log_file << "\n";
    log_file.close();
    if (!failed_transmissions.empty()) {
        //manage_fail_log_size(fail_log_filename, 10);
        std::lock_guard<std::mutex> lock(fail_log_mutex);
        std::ofstream fail_log_file(fail_log_filename, std::ios::app);
        if (!fail_log_file.is_open()) {
            std::cerr << "Failed to open fail log file!" << std::endl;
            return;
        }

        
        for (const auto& fail_record : failed_transmissions) {
            fail_log_file << fail_record;
        }

        fail_log_file.close();
        manage_fail_log_size(fail_log_filename, 10);
    }
}
std::string extract_mode_text(const std::string& input_text)
{
    size_t pos = input_text.find(":");
    if (pos != std::string::npos)
    {
        return input_text.substr(pos + 2);
    }
    return "";
}
void set_system_time(const DateTime& new_time) {
    
    std::tm tm_info = {};
    tm_info.tm_year = new_time.year - 1900; 
    tm_info.tm_mon = new_time.month - 1; 
    tm_info.tm_mday = new_time.day;
    tm_info.tm_hour = new_time.hour;
    tm_info.tm_min = new_time.minute;
    tm_info.tm_sec = new_time.second;

    std::time_t new_time_t = std::mktime(&tm_info);
    if (new_time_t == -1) {
        std::cerr << "Failed to convert DateTime to time_t." << std::endl;
        return;
    }

    struct timeval tv = {};
    tv.tv_sec = new_time_t;
    tv.tv_usec = 0; 

    if (settimeofday(&tv, nullptr) == -1) {
        perror("Failed to set system time");
    }
}

std::vector<std::string> scan_wifi_devices()
{
    std::vector<std::string> devices;
    
    devices.push_back("WiFi_Device_A");
    devices.push_back("WiFi_Device_B");
    devices.push_back("WiFi_Device_C");
    devices.push_back("WiFi_Device_D");
    devices.push_back("WiFi_Device_E");
    devices.push_back("WiFi_Device_F");
    devices.push_back("WiFi_Device_G");
    return devices;
}
void handle_wifi_selection(const std::string& device, egt::ScrolledView& scrollview, egt::PopupVirtualKeyboard& popup_keyboard)
{
    static auto wifi_password_page_box = std::make_shared<egt::VerticalBoxSizer>(egt::Justification::start);
    wifi_password_page_box->border(1);
    wifi_password_page_box->color(egt::Palette::ColorId::border, egt::Palette::gray);
    wifi_password_page_box->remove_all();
    auto ssid_box = std::make_shared<egt::HorizontalBoxSizer>(egt::Justification::justify);
    ssid_box->border(1);
    ssid_box->padding(50);
    wifi_password_page_box->add(expand(ssid_box));

    auto ssid_label = std::make_shared<egt::Label>("SSID :");
    ssid_label->font(Font("Times New Roman", 32));
    ssid_label->text_align(AlignFlag::left | AlignFlag::center_vertical);

    static auto ssid_text_box = std::make_shared<egt::TextBox>();
    ssid_text_box->align(egt::AlignFlag::left);
    ssid_text_box->font(Font("Times New Roman", 32));
    ssid_text_box->max_length(15);
    ssid_text_box->disable();
    ssid_text_box->text(device);

    ssid_box->add(expand_horizontal(ssid_label));
    ssid_box->add(expand_horizontal(ssid_text_box));
    ssid_box->resize(egt::Size(790, 200));

    auto password_box = std::make_shared<egt::HorizontalBoxSizer>(egt::Justification::justify);
    password_box->border(1);
    password_box->padding(50);
    wifi_password_page_box->add(expand(password_box));

    auto password_label = std::make_shared<egt::Label>("Password :");
    password_label->font(Font("Times New Roman", 32));
    password_label->text_align(AlignFlag::left | AlignFlag::center_vertical);

    static auto password_text_box = std::make_shared<egt::TextBox>();
    password_text_box->align(egt::AlignFlag::left);
    password_text_box->font(Font("Times New Roman", 32));
    password_text_box->max_length(20);
    password_text_box->text("");

    password_box->add(expand_horizontal(password_label));
    password_box->add(expand_horizontal(password_text_box));
    password_box->resize(egt::Size(790, 200));

    scrollview.remove_all();
    scrollview.add(expand(wifi_password_page_box));
    password_text_box->on_event([&](const Event& event) {
        if (event.key().keycode == egt::KeyboardCode::EKEY_ENTER){
            std::string input_text = password_text_box->text();
            std::cout << "input : " << input_text << std::endl;
            popup_keyboard.hide();
        }
    });
    popup_keyboard.on_hide([&]() {
        password_text_box->focus(false);
    });

}
void update_wifi_page(egt::VerticalBoxSizer& main_box, egt::VerticalBoxSizer& wifi_main_box, egt::ScrolledView& scrollview, egt::PopupVirtualKeyboard& popup_keyboard)
{
    // SCAN WIFI
    std::vector<std::string> devices = scan_wifi_devices();

    wifi_main_box.remove_all();
    popup_keyboard.hide();
    int item_index = 1;
    for (const auto& device : devices)
    {
        auto hbox = std::make_shared<egt::HorizontalBoxSizer>(egt::Justification::justify);
        hbox->border(1);
        hbox->color(egt::Palette::ColorId::border, egt::Palette::gray);
        auto label_item = std::make_shared<egt::Label>("Item" + std::to_string(item_index));        
        label_item->font(egt::Font("Times New Roman", 16, egt::Font::Weight::bold));
        label_item->color(egt::Palette::ColorId::label_text, egt::Palette::black);
        hbox->add(expand(label_item));
        
        auto btn = std::make_shared<egt::Button>(device);
        btn->font(egt::Font("Times New Roman", 16, egt::Font::Weight::bold));
        btn->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent);
        btn->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent, egt::Palette::GroupId::active);
        btn->color(egt::Palette::ColorId::button_text, egt::Palette::black);
        btn->color(egt::Palette::ColorId::button_text, egt::Palette::black, egt::Palette::GroupId::active);
        btn->padding(5);
        hbox->add(expand(btn));

        auto label_status = std::make_shared<egt::Label>("Connection Failed");
        label_status->font(egt::Font("Times New Roman", 16, egt::Font::Weight::bold));
        label_status->color(egt::Palette::ColorId::label_text, egt::Palette::red);
        hbox->add(expand(label_status));

        btn->on_click([device, &scrollview, &popup_keyboard](egt::Event & event)
        {
            handle_wifi_selection(device, scrollview, popup_keyboard);
        });
        hbox->resize(egt::Size(790, 68));
        wifi_main_box.add((hbox));
        item_index++;
    }
    
    scrollview.remove_all();
    scrollview.add(expand(wifi_main_box)); 
    scrollview.resize(egt::Size(800, 400));

    main_box.add(expand(scrollview));
}
void send_ct_log_towifi_loop(const std::string& fail_log_filename, int log_retention_days, int max_failed_log_cache) {
    while (true) {
        if(has_ct_data == true){

            std::vector<std::vector<std::vector<std::string>>> ct_data_groups;

            {
                perror("send ct data test start\n\r");
                std::lock_guard<std::mutex> lock(ct_data_mutex);
                perror("send ct data test end\n\r");
                ct_data_groups = {
                    ct_current_data_list1,
                    ct_current_data_list2,
                    ct_current_data_list3
                };
            }
            
            create_log_file_if_not_exists(fail_log_filename);
            log_current_data("current_data_log_" + get_current_date_string() + ".csv", fail_log_filename, ct_data_groups, log_retention_days, max_failed_log_cache);
            sync();
        }
        else{
            std::cout << "[INFO] CT data not received yet, skipping data logging." << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}
void resned_ct_log_towifi_loop(const std::string& fail_log_filename, int wifi_batch_size) {
    while (true) {
        

        resend_wifi_FailedLog(fail_log_filename, wifi_batch_size);
        sync();
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
bool validate_control_packet_checksum(const ControlPacket_t* pkt) {
    if (pkt == NULL) return false;

    const uint8_t* raw = (const uint8_t*)pkt;
    size_t len = sizeof(ControlPacket_t) - 1;
    // for (int i = 0; i < 40; ++i) {
    //     printf("0x%02X ", raw[i]);
    // }
    uint8_t calc_sum = calculate_simple_checksum(raw, len);
    //printf("Calculated checksum: 0x%02X, Packet checksum: 0x%02X\n", calc_sum, pkt->checksum);
    return (calc_sum == pkt->checksum);
}
void print_control_packet(const ControlPacket_t* pkt) {
    if (pkt == NULL) {
        printf("Invalid packet (NULL pointer)\n");
        return;
    }

    printf("ControlPacket: \n");
    printf("  Start: 0x%04X\n", pkt->start);
    printf("  Length: 0x%02X\n", pkt->length);
    printf("  Host ID: 0x%04X\n", pkt->host_id);
    printf("  Customer Code: 0x%02X\n", pkt->customer_code);
    printf("  Model Code: 0x%02X\n", pkt->model_code);
    printf("  Signal Mode: 0x%02X\n", pkt->signal_mode);
    printf("  Page Number: 0x%02X\n", pkt->page_number);
    printf("  Error Code: 0x%02X\n", pkt->error_code);
    printf("  Command: 0x%02X\n", pkt->command);

    // 打印 Process Flag
    printf("  Process Flag: 0x%02X\n", pkt->process_flag);

    // 打印 Flag 1
    printf("  Flag 1:\n");
    printf("    Power Status: %u\n", pkt->flag1.power_status);
    printf("    Action Status: %u\n", pkt->flag1.action_status);
    printf("    Learning Mode: %u\n", pkt->flag1.learning_mode);
    printf("    Test Mode: %u\n", pkt->flag1.test_mode);
    printf("    SOS Mode: %u\n", pkt->flag1.sos_mode);
    printf("    Pause Output: %u\n", pkt->flag1.pause_output);
    printf("    Error Output: %u\n", pkt->flag1.error_output);
    printf("    Sys Time: %u\n", pkt->flag1.sys_time);

    // 打印 Flag 2
    printf("  Flag 2:\n");
    printf("    SW1 Setting: %u\n", pkt->flag2.sw1_setting);
    printf("    SW2 Setting: %u\n", pkt->flag2.sw2_setting);
    printf("    SW3 Setting: %u\n", pkt->flag2.sw3_setting);
    printf("    Reserved: 0x%02X\n", pkt->flag2.reserved);

    // 打印 Flag 3
    printf("  Flag 3:\n");
    printf("    Reserved[%d]: 0x%02X\n", pkt->flag3.reserved);
    

    // 打印 data 資料
    printf("  Data: ");
    for (int i = 0; i < 24; ++i) {
        printf("0x%02X ", pkt->data[i]);
    }
    printf("\n");

    // 打印 Checksum
    printf("  Checksum: 0x%02X\n", pkt->checksum);
}
std::string format_current_value(uint16_t value) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << (value / 10.0);  // 除以10.0來獲得帶小數的值
    return oss.str() + " A";  // 加上單位
}
void read_controlboard_data_thread(){
    speed_t speed = B115200;
    int vtime = 0;
    int received_bytes = 0;
    int expected_bytes = 40;
    while(1) {
        int received_bytes = 0;
        uint8_t *receive = uart_read(CONTROL_UART2_PATH, speed, vtime, expected_bytes, received_bytes);
        if (receive != NULL && received_bytes == 40) {
            ControlPacket_t *pkt = (ControlPacket_t *)receive;
            print_control_packet(pkt);
            if(pkt->start == 0xAA55){
                if (validate_control_packet_checksum(pkt)) {
                    
                    printf("Checksum valid!\n");
                    print_control_packet(pkt);
                    // TODO
                    perror("control ct data test start\n\r");
                    std::lock_guard<std::mutex> lock(ct_data_mutex);
                    perror("control ct data test end\n\r");
                    switch (pkt->command) {
                        case CMD_READ_CURRENT:
                            printf("Handling CMD_READ_CURRENT\n");
                            if(pkt->process_flag == 0x01){
                                for (int i = 0; i < 8; i++) {
                                    uint16_t value = pkt->data[0 + i * 2] * 256 + pkt->data[1 + i * 2];
                                    ct_current_data_list1[1][i] = format_current_value(value);
                                }
                                for (int i = 8; i < 12; i++) {
                                    uint16_t value = pkt->data[0 + i * 2] * 256 + pkt->data[1 + i * 2];
                                    ct_current_data_list2[1][i - 8] = format_current_value(value);
                                }
                            }
                            else if(pkt->process_flag == 0x02){
                                for (int i = 0; i < 6; i++) {
                                    uint16_t value = pkt->data[0 + i * 2] * 256 + pkt->data[1 + i * 2];
                                    ct_current_data_list2[1][i + 4] = format_current_value(value);
                                }
                                for (int i = 6; i < 12; i++) {
                                    uint16_t value = pkt->data[0 + i * 2] * 256 + pkt->data[1 + i * 2];
                                    ct_current_data_list3[1][i - 6] = format_current_value(value);
                                }

                            }
                            else if(pkt->process_flag == 0x03){
                                for (int i = 0; i < 6; i++) {
                                    uint16_t value = pkt->data[0 + i * 2] * 256 + pkt->data[1 + i * 2];
                                    ct_current_data_list3[1][i + 6] = format_current_value(value);
                                }
                                std::cout << "Time: " 
                                        << "Hour: " << pkt->data[12] << ", "
                                        << "Min: " << pkt->data[13] << ", "
                                        << "Sec: " << pkt->data[14] << std::endl;
                                std::cout << "Version: " << pkt->data[24] << std::endl;
                                has_ct_data = true;
                            }
                            
                            break;
                
                        case CMD_SYS_TIME:
                            printf("Handling CMD_SYS_TIME\n");
                            break;
                
                        case CMD_ON:
                            printf("Handling CMD_ON\n");
                            break;
                
                        case CMD_OFF:
                            printf("Handling CMD_OFF\n");
                            break;
                
                        default:
                            printf("Unknown command: 0x%02X\n", pkt->command);
                            break;
                    }
                } 
                else {
                    printf("Checksum error!\n");
                    //continue;
                }
            }
            else{
                printf("ControlBoard start data error. Retrying...\n");
                //continue;
            }
        }
        else{
            printf("Invalid packet size: %d bytes. Skipping...\n", received_bytes);
            //free(receive);
            //continue;
        }
        usleep(1*1000*1000);
        free(receive);
    }
}
void update_state(PageState *state, TimeSettingState new_state, const DateTime& current_time) {
    if (!state->initialized) {
        state->current = new_state;
        state->previous = new_state;
        state->initialized = true;
        return;
    }

    state->previous = state->current;
    state->current = new_state;

    if (state->previous == IN_TIME_SETTING &&
        state->current == NOT_IN_TIME_SETTING) {
        set_system_time(current_time);
    }
}
int main(int argc, char** argv)
{
    PageState page_state = {NOT_IN_TIME_SETTING, NOT_IN_TIME_SETTING, false};
    DateTime dummy_time;
	egt::Application app(argc, argv, "keyboard");
    const auto screen_size = egt::Application::instance().screen()->size();
    egt::add_search_path("/usr/share/libegt");
    Config config;
    config_t cfg;
    if (config.load("config.ini")) {
        cfg.log_time_delay = std::stoi(config.get("parameter", "log_time_delay"));
        cfg.log_retention_days = std::stoi(config.get("parameter", "log_retention_days"));
        cfg.network_detection_delay = std::stoi(config.get("parameter", "network_detection_delay"));
        cfg.wifi_batch_size = std::stoi(config.get("parameter", "wifi_batch_size"));
        cfg.max_failed_log_cache = std::stoi(config.get("parameter", "max_failed_log_cache"));
    }
    egt::TopWindow win;
    OverlayWindow heoWin(egt::Rect(0, 0, 800, 480));
	heoWin.hide_overlay();  
    const auto landscape = screen_size.width() >= screen_size.height();
    auto default_keyboard = std::make_shared<egt::VirtualKeyboard>();
    egt::Size keyboard_size;
    if (!landscape)
        keyboard_size = egt::Size(screen_size.width(), screen_size.height() * 0.25);
    egt::PopupVirtualKeyboard popup_keyboard {default_keyboard, keyboard_size};
    win.add(popup_keyboard);
    
    VerticalBoxSizer main_box(egt::Justification::start);
    win.add(expand(main_box));
    egt::PeriodicTimer log_timer{std::chrono::seconds(cfg.log_time_delay)};
    //========================title box================================
    HorizontalBoxSizer title_main_box(egt::Justification::justify);
    title_main_box.border(2);
    title_main_box.color(Palette::ColorId::border, Palette::black);
    main_box.add(expand_horizontal(title_main_box));

    HorizontalBoxSizer title_left_box(egt::Justification::start);
    title_main_box.add(expand(title_left_box));
    HorizontalBoxSizer title_middle_box(egt::Justification::justify);
    title_main_box.add((title_middle_box));
    HorizontalBoxSizer title_right_box(egt::Justification::ending);
    title_main_box.add(expand(title_right_box));

    auto wifi_toggle = std::make_shared<egt::ToggleBox>();
    wifi_toggle->toggle_text("OFF", "ON");
    wifi_toggle->font(Font("Times New Roman", 24));
    wifi_toggle->resize(Size(150, 64));
    title_middle_box.add(wifi_toggle);
    wifi_toggle->hide();
    
    auto home_button = std::make_shared<egt::ImageButton>(egt::Image("icon:home.png;64"));
    home_button->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent);
    home_button->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent, egt::Palette::GroupId::active);
    title_left_box.add(left(home_button));

    auto clouda_button = std::make_shared<egt::ImageButton>(egt::Image("icon:clouda.png;64"));
    clouda_button->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent);
    clouda_button->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent, egt::Palette::GroupId::active);
    title_right_box.add(right(clouda_button));

    auto wifi_button = std::make_shared<egt::ImageButton>(egt::Image("icon:wifi.png;64"));
    wifi_button->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent);
    wifi_button->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent, egt::Palette::GroupId::active);
    title_right_box.add(right(wifi_button));

    auto power_button = std::make_shared<egt::ImageButton>(egt::Image("icon:power.png;64"));
    power_button->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent);
    power_button->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent, egt::Palette::GroupId::active);
    title_right_box.add(right(power_button));

    auto netork_warning_button = std::make_shared<egt::ImageButton>(egt::Image("icon:warning.png;64"));
    netork_warning_button->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent);
    netork_warning_button->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent, egt::Palette::GroupId::active);

    egt::PeriodicTimer network_timer{std::chrono::seconds(cfg.network_detection_delay)};
    //========================title box================================

    //========================page1 box================================
    VerticalBoxSizer page1_main_box(egt::Justification::justify);
    page1_main_box.border(2);
    page1_main_box.color(Palette::ColorId::border, Palette::red);
    main_box.add(expand(page1_main_box));

    HorizontalBoxSizer page1_top_box(egt::Justification::justify);
    page1_top_box.border(2);
    page1_top_box.padding(20);
    page1_top_box.color(Palette::ColorId::border, Palette::blue);
    page1_top_box.color(Palette::ColorId::bg, Palette::blue);
    page1_main_box.add(expand(page1_top_box));

    std::string item1_mode_text = "Item1 : 30A Mode";
    std::string item2_mode_text = "Item2 : 30A Mode";
    std::string item3_mode_text = "Item3 : 30A Mode";
    std::string combined = item1_mode_text + "\n" + item2_mode_text + "\n" + item3_mode_text;
    Button mode_view_button(combined);
    mode_view_button.font(Font("Times New Roman", 32));
    mode_view_button.resize(Size(330, 120));
    page1_top_box.add(mode_view_button);
    
    Button time_setting_button("TIME");
    time_setting_button.font(Font("Times New Roman", 32));
    time_setting_button.resize(Size(330, 120));
    page1_top_box.add(time_setting_button);

    HorizontalBoxSizer page1_bottom_box(egt::Justification::justify);
    page1_bottom_box.border(2);
    page1_bottom_box.padding(20);
    page1_main_box.add(expand(page1_bottom_box));

    VerticalBoxSizer page1_bottom_group_box1(egt::Justification::justify);
    page1_bottom_box.add(expand(page1_bottom_group_box1));
    VerticalBoxSizer page1_bottom_group_box2(egt::Justification::justify);
    page1_bottom_box.add(expand(page1_bottom_group_box2));
    VerticalBoxSizer page1_bottom_group_box3(egt::Justification::justify);
    page1_bottom_box.add(expand(page1_bottom_group_box3));

    Button group1_setting_button1("1");
    group1_setting_button1.font(Font("Times New Roman", 20));
    group1_setting_button1.resize(Size(60, 60));
    page1_bottom_group_box1.add(group1_setting_button1);

    auto group1_toggle1 = std::make_shared<egt::ToggleBox>();
    group1_toggle1->toggle_text("30A", "50A");
    group1_toggle1->font(Font("Times New Roman", 28));
    group1_toggle1->resize(Size(150, 80));
    page1_bottom_group_box1.add(group1_toggle1);

    Button group1_setting_button2("2");
    group1_setting_button2.font(Font("Times New Roman", 20));
    group1_setting_button2.resize(Size(60, 60));
    page1_bottom_group_box2.add(group1_setting_button2);

    auto group1_toggle2 = std::make_shared<egt::ToggleBox>();
    group1_toggle2->toggle_text("30A", "50A");
    group1_toggle2->font(Font("Times New Roman", 28));
    group1_toggle2->resize(Size(150, 80));
    page1_bottom_group_box2.add(group1_toggle2);

    Button group1_setting_button3("3");
    group1_setting_button3.font(Font("Times New Roman", 20));
    group1_setting_button3.resize(Size(60, 60));
    page1_bottom_group_box3.add(group1_setting_button3);

    auto group1_toggle3 = std::make_shared<egt::ToggleBox>();
    group1_toggle3->toggle_text("30A", "50A");
    group1_toggle3->font(Font("Times New Roman", 28));
    group1_toggle3->resize(Size(150, 80));
    page1_bottom_group_box3.add(group1_toggle3);

    //========================page1 box================================

    //========================wifi page box================================
    VerticalBoxSizer wifi_main_box(egt::Justification::start);
    wifi_main_box.border(2);
    wifi_main_box.color(Palette::ColorId::border, Palette::red);
    auto scrollview = std::make_shared<egt::ScrolledView>();
    scrollview->border(2);
    scrollview->color(egt::Palette::ColorId::border, egt::Palette::gray);

    //========================wifi page box================================

    //========================time setting page box================================
    DateTime current_time;
    TIMESET_TYPE time_set_type;
    int max_days_in_month;
    VerticalBoxSizer time_main_box(egt::Justification::justify);
    time_main_box.border(2);
    time_main_box.color(Palette::ColorId::border, Palette::green);

    HorizontalBoxSizer time_top_box(egt::Justification::middle);
    time_top_box.border(2);
    time_top_box.padding(20);
    time_main_box.add(expand(time_top_box));

    Button time_switch_button("DATE/TIME");
    time_switch_button.font(Font("Times New Roman", 32));
    time_switch_button.resize(Size(330, 120));
    time_top_box.add(time_switch_button);

    HorizontalBoxSizer time_bottom_box(egt::Justification::justify);
    time_bottom_box.border(2);
    time_bottom_box.padding(20);
    time_main_box.add(expand(time_bottom_box));

    VerticalBoxSizer time_bottom_group_box1(egt::Justification::justify);
    time_bottom_box.add(expand(time_bottom_group_box1));
    VerticalBoxSizer time_bottom_group_box2(egt::Justification::justify);
    time_bottom_box.add(expand(time_bottom_group_box2));
    VerticalBoxSizer time_bottom_group_box3(egt::Justification::justify);
    time_bottom_box.add(expand(time_bottom_group_box3));

    auto time_label1 = std::make_shared<egt::Label>("Year()");        
    time_label1->font(egt::Font("Times New Roman", 40, egt::Font::Weight::bold));
    time_label1->color(egt::Palette::ColorId::label_text, egt::Palette::black);
    time_bottom_group_box1.add(expand(time_label1));

    auto time_label2 = std::make_shared<egt::Label>("Month()");        
    time_label2->font(egt::Font("Times New Roman", 40, egt::Font::Weight::bold));
    time_label2->color(egt::Palette::ColorId::label_text, egt::Palette::black);
    time_bottom_group_box2.add(expand(time_label2));

    auto time_label3 = std::make_shared<egt::Label>("Day()");        
    time_label3->font(egt::Font("Times New Roman", 40, egt::Font::Weight::bold));
    time_label3->color(egt::Palette::ColorId::label_text, egt::Palette::black);
    time_bottom_group_box3.add(expand(time_label3));


    HorizontalBoxSizer time_bottom_setting_group_box1(egt::Justification::justify);
    time_bottom_group_box1.add(expand(time_bottom_setting_group_box1));

    auto time_setting_up_button1 = std::make_shared<egt::ImageButton>(egt::Image("icon:arrow_up.png;64"));
    time_setting_up_button1->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent);
    time_setting_up_button1->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent, egt::Palette::GroupId::active);
    time_bottom_setting_group_box1.add(time_setting_up_button1);
    auto time_setting_down_button1 = std::make_shared<egt::ImageButton>(egt::Image("icon:arrow_down.png;64"));
    time_setting_down_button1->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent);
    time_setting_down_button1->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent, egt::Palette::GroupId::active);
    time_bottom_setting_group_box1.add(time_setting_down_button1);


    HorizontalBoxSizer time_bottom_setting_group_box2(egt::Justification::justify);
    time_bottom_group_box2.add(expand(time_bottom_setting_group_box2));

    auto time_setting_up_button2 = std::make_shared<egt::ImageButton>(egt::Image("icon:arrow_up.png;64"));
    time_setting_up_button2->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent);
    time_setting_up_button2->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent, egt::Palette::GroupId::active);
    time_bottom_setting_group_box2.add(time_setting_up_button2);
    auto time_setting_down_button2 = std::make_shared<egt::ImageButton>(egt::Image("icon:arrow_down.png;64"));
    time_setting_down_button2->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent);
    time_setting_down_button2->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent, egt::Palette::GroupId::active);
    time_bottom_setting_group_box2.add(time_setting_down_button2);

    HorizontalBoxSizer time_bottom_setting_group_box3(egt::Justification::justify);
    time_bottom_group_box3.add(expand(time_bottom_setting_group_box3));

    auto time_setting_up_button3 = std::make_shared<egt::ImageButton>(egt::Image("icon:arrow_up.png;64"));
    time_setting_up_button3->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent);
    time_setting_up_button3->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent, egt::Palette::GroupId::active);
    time_bottom_setting_group_box3.add(time_setting_up_button3);
    auto time_setting_down_button3 = std::make_shared<egt::ImageButton>(egt::Image("icon:arrow_down.png;64"));
    time_setting_down_button3->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent);
    time_setting_down_button3->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent, egt::Palette::GroupId::active);
    time_bottom_setting_group_box3.add(time_setting_down_button3);
    
    //========================time setting page box================================

    //========================CT page box================================
    VerticalBoxSizer ct_main_box(egt::Justification::justify);
    //ct_main_box.border(2);
    ct_main_box.padding(20);
    ct_main_box.color(Palette::ColorId::border, Palette::red);
    
    HorizontalBoxSizer ct_top_box(egt::Justification::middle);
    //ct_top_box.border(2);
    //ct_top_box.padding(20);
    ct_main_box.add((ct_top_box));

    Button ct_item_button("1");
    ct_item_button.font(Font("Times New Roman", 20));
    ct_item_button.resize(Size(60, 60));
    ct_top_box.add(ct_item_button);

    auto ct_status1 = std::make_shared<egt::ImageLabel>(egt::Image("file:image/green_rectangle.png"));
    ct_top_box.add(ct_status1);
    auto ct_status2 = std::make_shared<egt::ImageLabel>(egt::Image("file:image/white_rectangle.png"));
    ct_top_box.add(ct_status2);
    auto ct_status3 = std::make_shared<egt::ImageLabel>(egt::Image("file:image/white_rectangle.png"));
    ct_top_box.add(ct_status3);
    auto ct_status4 = std::make_shared<egt::ImageLabel>(egt::Image("file:image/white_rectangle.png"));
    ct_top_box.add(ct_status4);
    auto ct_status5 = std::make_shared<egt::ImageLabel>(egt::Image("file:image/white_rectangle.png"));
    ct_top_box.add(ct_status5);
    auto ct_status6 = std::make_shared<egt::ImageLabel>(egt::Image("file:image/white_rectangle.png"));
    ct_top_box.add(ct_status6);
    auto ct_status7 = std::make_shared<egt::ImageLabel>(egt::Image("file:image/white_rectangle.png"));
    ct_top_box.add(ct_status7);
    auto ct_status8 = std::make_shared<egt::ImageLabel>(egt::Image("file:image/white_rectangle.png"));
    ct_top_box.add(ct_status8);
    auto ct_status9 = std::make_shared<egt::ImageLabel>(egt::Image("file:image/white_rectangle.png"));
    ct_top_box.add(ct_status9);
    auto ct_status10 = std::make_shared<egt::ImageLabel>(egt::Image("file:image/white_rectangle.png"));
    ct_top_box.add(ct_status10);
    auto ct_status11 = std::make_shared<egt::ImageLabel>(egt::Image("file:image/white_rectangle.png"));
    ct_top_box.add(ct_status11);
    auto ct_status12 = std::make_shared<egt::ImageLabel>(egt::Image("file:image/white_rectangle.png"));
    ct_top_box.add(ct_status12);

    VerticalBoxSizer ct_bottom_box(egt::Justification::middle);
    ct_bottom_box.border(2);
    ct_bottom_box.color(Palette::ColorId::border, Palette::red);
    ct_main_box.add((ct_bottom_box));

    HorizontalBoxSizer ct_bottom_group_box1(egt::Justification::middle);
    //ct_bottom_group_box1.border(2);
    ct_bottom_box.add((ct_bottom_group_box1));

    auto ct_mode_label = std::make_shared<egt::Label>("30A Mode");        
    ct_mode_label->font(egt::Font("Times New Roman", 28, egt::Font::Weight::bold));
    ct_mode_label->color(egt::Palette::ColorId::label_text, egt::Palette::black);
    ct_mode_label->margin(15);
    ct_bottom_group_box1.add((ct_mode_label));

    auto ct_time_label = std::make_shared<egt::Label>("");        
    ct_time_label->font(egt::Font("Times New Roman", 28, egt::Font::Weight::bold));
    ct_time_label->color(egt::Palette::ColorId::label_text, egt::Palette::black);
    ct_time_label->margin(15);
    ct_bottom_group_box1.add((ct_time_label));

    HorizontalBoxSizer ct_bottom_group_box2(egt::Justification::middle);
    //ct_bottom_group_box2.border(2);
    ct_bottom_box.add((ct_bottom_group_box2));

    auto ct_current_label = std::make_shared<egt::Label>("CT1");        
    ct_current_label->font(egt::Font("Times New Roman", 46, egt::Font::Weight::bold));
    ct_current_label->color(egt::Palette::ColorId::label_text, egt::Palette::black);
    ct_current_label->margin(30);
    ct_bottom_group_box2.add((ct_current_label));

    auto ct_current_data = std::make_shared<egt::Label>("11.1 A");        
    ct_current_data->font(egt::Font("Times New Roman", 46, egt::Font::Weight::bold));
    ct_current_data->color(egt::Palette::ColorId::label_text, egt::Palette::black);
    ct_current_data->margin(30);
    ct_bottom_group_box2.add((ct_current_data));

    auto ct_next_data_button = std::make_shared<egt::ImageButton>(egt::Image("icon:fastforward.png;64"));
    ct_next_data_button->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent);
    ct_next_data_button->color(egt::Palette::ColorId::button_bg, egt::Palette::transparent, egt::Palette::GroupId::active);
    ct_next_data_button->align(egt::AlignFlag::right);
    ct_bottom_box.add(ct_next_data_button);
    egt::PeriodicTimer ct_timer(std::chrono::milliseconds(500));
    std::string formatted_time;
    
    //========================CT page box================================
    ct_timer.on_timeout([&]()
    {
        current_time = get_current_time();
        formatted_time = format_date_time(current_time);
        ct_time_label->text(formatted_time);
        switch(current_ct_group) {
            case 0:
                ct_current_data->text(ct_current_data_list1[1][current_ct_item]);
            break;
            case 1:
                ct_current_data->text(ct_current_data_list2[1][current_ct_item]);
            break;
            case 2:
                ct_current_data->text(ct_current_data_list3[1][current_ct_item]);
            break;
        }
    });
    ct_next_data_button->on_click([&](egt::Event &)
    {
        switch(current_ct_group) {
            case 0:
                if(current_ct_item < 7) {
                    current_ct_item++;
                }
                else{
                    current_ct_item = 0;
                }
                ct_current_label->text(ct_current_data_list1[0][current_ct_item]);
                ct_current_data->text(ct_current_data_list1[1][current_ct_item]);
            break;
            case 1:
                if(current_ct_item < 9) {
                    current_ct_item++;
                }
                else{
                    current_ct_item = 0;
                }
                ct_current_label->text(ct_current_data_list2[0][current_ct_item]);
                ct_current_data->text(ct_current_data_list2[1][current_ct_item]);
            break;
            case 2:
                if(current_ct_item < 11) {
                    current_ct_item++;
                }
                else{
                    current_ct_item = 0;
                }
                ct_current_label->text(ct_current_data_list3[0][current_ct_item]);
                ct_current_data->text(ct_current_data_list3[1][current_ct_item]);
            break;
        }
        
        
        switch(current_ct_item) {
            case 0:
                ct_status1->image(egt::Image("file:image/green_rectangle.png"));
                ct_status8->image(egt::Image("file:image/white_rectangle.png"));
                ct_status10->image(egt::Image("file:image/white_rectangle.png"));
                ct_status12->image(egt::Image("file:image/white_rectangle.png"));
            break;
            case 1:
                ct_status2->image(egt::Image("file:image/green_rectangle.png"));
                ct_status1->image(egt::Image("file:image/white_rectangle.png"));
            break;
            case 2:
                ct_status3->image(egt::Image("file:image/green_rectangle.png"));
                ct_status2->image(egt::Image("file:image/white_rectangle.png"));
            break;
            case 3:
                ct_status4->image(egt::Image("file:image/green_rectangle.png"));
                ct_status3->image(egt::Image("file:image/white_rectangle.png"));
            break;
            case 4:
                ct_status5->image(egt::Image("file:image/green_rectangle.png"));
                ct_status4->image(egt::Image("file:image/white_rectangle.png"));
            break;
            case 5:
                ct_status6->image(egt::Image("file:image/green_rectangle.png"));
                ct_status5->image(egt::Image("file:image/white_rectangle.png"));
            break;
            case 6:
                ct_status7->image(egt::Image("file:image/green_rectangle.png"));
                ct_status6->image(egt::Image("file:image/white_rectangle.png"));
            break;
            case 7:
                ct_status8->image(egt::Image("file:image/green_rectangle.png"));
                ct_status7->image(egt::Image("file:image/white_rectangle.png"));
            break;
            case 8:
                ct_status9->image(egt::Image("file:image/green_rectangle.png"));
                ct_status8->image(egt::Image("file:image/white_rectangle.png"));
            break;
            case 9:
                ct_status10->image(egt::Image("file:image/green_rectangle.png"));
                ct_status9->image(egt::Image("file:image/white_rectangle.png"));
            break;
            case 10:
                ct_status11->image(egt::Image("file:image/green_rectangle.png"));
                ct_status10->image(egt::Image("file:image/white_rectangle.png"));
            break;
            case 11:
                ct_status12->image(egt::Image("file:image/green_rectangle.png"));
                ct_status11->image(egt::Image("file:image/white_rectangle.png"));
            break;
        }
    });
    group1_setting_button1.on_click([&](egt::Event &)
    {
        ct_item_button.text("1");
        current_ct_group = 0;
        current_ct_item = 0;
        ct_current_label->text(ct_current_data_list1[0][current_ct_item]);
        ct_current_data->text(ct_current_data_list1[1][current_ct_item]);
        ct_top_box.remove_all();
        ct_top_box.add(ct_item_button);
        ct_top_box.add(ct_status1);
        ct_top_box.add(ct_status2);
        ct_top_box.add(ct_status3);
        ct_top_box.add(ct_status4);
        ct_top_box.add(ct_status5);
        ct_top_box.add(ct_status6);
        ct_top_box.add(ct_status7);
        ct_top_box.add(ct_status8);
        ct_status1->image(egt::Image("file:image/green_rectangle.png"));
        ct_status2->image(egt::Image("file:image/white_rectangle.png"));
        ct_status3->image(egt::Image("file:image/white_rectangle.png"));
        ct_status4->image(egt::Image("file:image/white_rectangle.png"));
        ct_status5->image(egt::Image("file:image/white_rectangle.png"));
        ct_status6->image(egt::Image("file:image/white_rectangle.png"));
        ct_status7->image(egt::Image("file:image/white_rectangle.png"));
        ct_status8->image(egt::Image("file:image/white_rectangle.png"));
        std::string ct_label_text = extract_mode_text(item1_mode_text);
        ct_mode_label->text(ct_label_text);
        current_time = get_current_time();
        formatted_time = format_date_time(current_time);
        ct_time_label->text(formatted_time);
        ct_timer.start();
        main_box.remove_all();
        wifi_toggle->hide();
        main_box.add(expand_horizontal(title_main_box));
        main_box.add(expand(ct_main_box));
    });
    group1_setting_button2.on_click([&](egt::Event &)
    {
        ct_item_button.text("2");
        current_ct_group = 1;
        current_ct_item = 0;
        ct_current_label->text(ct_current_data_list2[0][current_ct_item]);
        ct_current_data->text(ct_current_data_list2[1][current_ct_item]);
        ct_top_box.remove_all();
        ct_top_box.add(ct_item_button);
        ct_top_box.add(ct_status1);
        ct_top_box.add(ct_status2);
        ct_top_box.add(ct_status3);
        ct_top_box.add(ct_status4);
        ct_top_box.add(ct_status5);
        ct_top_box.add(ct_status6);
        ct_top_box.add(ct_status7);
        ct_top_box.add(ct_status8);
        ct_top_box.add(ct_status9);
        ct_top_box.add(ct_status10);
        
        ct_status1->image(egt::Image("file:image/green_rectangle.png"));
        ct_status2->image(egt::Image("file:image/white_rectangle.png"));
        ct_status3->image(egt::Image("file:image/white_rectangle.png"));
        ct_status4->image(egt::Image("file:image/white_rectangle.png"));
        ct_status5->image(egt::Image("file:image/white_rectangle.png"));
        ct_status6->image(egt::Image("file:image/white_rectangle.png"));
        ct_status7->image(egt::Image("file:image/white_rectangle.png"));
        ct_status8->image(egt::Image("file:image/white_rectangle.png"));
        ct_status9->image(egt::Image("file:image/white_rectangle.png"));
        ct_status10->image(egt::Image("file:image/white_rectangle.png"));
        std::string ct_label_text = extract_mode_text(item2_mode_text);
        ct_mode_label->text(ct_label_text);
        current_time = get_current_time();
        formatted_time = format_date_time(current_time);
        ct_time_label->text(formatted_time);
        ct_timer.start();
        main_box.remove_all();
        wifi_toggle->hide();
        main_box.add(expand_horizontal(title_main_box));
        main_box.add(expand(ct_main_box));
    });
    group1_setting_button3.on_click([&](egt::Event &)
    {
        ct_item_button.text("3");
        current_ct_group = 2;
        current_ct_item = 0;
        ct_current_label->text(ct_current_data_list3[0][current_ct_item]);
        ct_current_data->text(ct_current_data_list3[1][current_ct_item]);
        ct_top_box.remove_all();
        ct_top_box.add(ct_item_button);
        ct_top_box.add(ct_status1);
        ct_top_box.add(ct_status2);
        ct_top_box.add(ct_status3);
        ct_top_box.add(ct_status4);
        ct_top_box.add(ct_status5);
        ct_top_box.add(ct_status6);
        ct_top_box.add(ct_status7);
        ct_top_box.add(ct_status8);
        ct_top_box.add(ct_status9);
        ct_top_box.add(ct_status10);
        ct_top_box.add(ct_status11);
        ct_top_box.add(ct_status12);
        ct_status1->image(egt::Image("file:image/green_rectangle.png"));
        ct_status2->image(egt::Image("file:image/white_rectangle.png"));
        ct_status3->image(egt::Image("file:image/white_rectangle.png"));
        ct_status4->image(egt::Image("file:image/white_rectangle.png"));
        ct_status5->image(egt::Image("file:image/white_rectangle.png"));
        ct_status6->image(egt::Image("file:image/white_rectangle.png"));
        ct_status7->image(egt::Image("file:image/white_rectangle.png"));
        ct_status8->image(egt::Image("file:image/white_rectangle.png"));
        ct_status9->image(egt::Image("file:image/white_rectangle.png"));
        ct_status10->image(egt::Image("file:image/white_rectangle.png"));
        ct_status11->image(egt::Image("file:image/white_rectangle.png"));
        ct_status12->image(egt::Image("file:image/white_rectangle.png"));
        std::string ct_label_text = extract_mode_text(item3_mode_text);
        ct_mode_label->text(ct_label_text);
        current_time = get_current_time();
        formatted_time = format_date_time(current_time);
        ct_time_label->text(formatted_time);
        ct_timer.start();
        main_box.remove_all();
        wifi_toggle->hide();
        main_box.add(expand_horizontal(title_main_box));
        main_box.add(expand(ct_main_box));
    });
    netork_warning_button->on_event([&](egt::Event &event)
    {
        if (event.id() == egt::EventId::raw_pointer_down)
        {
            netork_warning_button->image(egt::Image("icon:clouda.png;64"));
        }
        else if (event.id() == egt::EventId::raw_pointer_up)
        {
            netork_warning_button->image(egt::Image("icon:warning.png;64"));
        }
    }, {egt::EventId::raw_pointer_down, egt::EventId::raw_pointer_up});
    netork_warning_button->on_click([&](egt::Event &)
    {
        main_box.remove_all();
        wifi_toggle->hide();
        main_box.add(expand_horizontal(title_main_box));
    });

    time_setting_up_button1->on_click([&](egt::Event &)
    {
        
        switch (time_set_type) {
            case TIMESET_TYPE::DATE:
                if (current_time.year < 2100){
                    current_time.year++;
                    max_days_in_month = days_in_month(current_time.year, current_time.month);
                    if (current_time.day > max_days_in_month){
                        current_time.day = max_days_in_month;
                        time_label3->text("Day(" + std::to_string(current_time.day) + ")");
                    }
                    time_label1->text("Year(" + std::to_string(current_time.year) + ")");
                    //set_system_time(current_time);
                }
                break;

            case TIMESET_TYPE::TIME:
                if (current_time.hour < 23) {
                    current_time.hour++;
                    time_label1->text("Hour(" + std::to_string(current_time.hour) + ")");
                    //set_system_time(current_time);
                }
                break;

            default:
                break;
        }
    });
    time_setting_down_button1->on_click([&](egt::Event &)
    {
        switch (time_set_type) {
            case TIMESET_TYPE::DATE:
                if (current_time.year > 2000) {
                    current_time.year--;
                    max_days_in_month = days_in_month(current_time.year, current_time.month);
                    if (current_time.day > max_days_in_month){
                        current_time.day = max_days_in_month;
                        time_label3->text("Day(" + std::to_string(current_time.day) + ")");
                    }
                    time_label1->text("Year(" + std::to_string(current_time.year) + ")");
                    //set_system_time(current_time);
                }
                
                break;

            case TIMESET_TYPE::TIME:
                if (current_time.hour > 0) {
                    current_time.hour--;
                    time_label1->text("Hour(" + std::to_string(current_time.hour) + ")");
                    //set_system_time(current_time);
                }
                break;

            default:
                break;
        }
    });
    time_setting_up_button2->on_click([&](egt::Event &)
    {
        switch (time_set_type) {
            case TIMESET_TYPE::DATE:
                if (current_time.month < 12) {
                    current_time.month++;
                    max_days_in_month = days_in_month(current_time.year, current_time.month);
                    if (current_time.day > max_days_in_month){
                        current_time.day = max_days_in_month;
                        time_label3->text("Day(" + std::to_string(current_time.day) + ")");
                    }
                    time_label2->text("Month(" + std::to_string(current_time.month) + ")");
                    //set_system_time(current_time);
                }
                break;

            case TIMESET_TYPE::TIME:
                if (current_time.minute < 59) {
                    current_time.minute++;
                    time_label2->text("Minute(" + std::to_string(current_time.minute) + ")");
                    //set_system_time(current_time);
                }
                break;

            default:
                break;
        }
    });
    time_setting_down_button2->on_click([&](egt::Event &)
    {
        switch (time_set_type) {
            case TIMESET_TYPE::DATE:
                if (current_time.month > 1) {
                    current_time.month--;
                    max_days_in_month = days_in_month(current_time.year, current_time.month);
                    if (current_time.day > max_days_in_month){
                        current_time.day = max_days_in_month;
                        time_label3->text("Day(" + std::to_string(current_time.day) + ")");
                    }
                    time_label2->text("Month(" + std::to_string(current_time.month) + ")");
                    //set_system_time(current_time);
                }
                break;

            case TIMESET_TYPE::TIME:
                if (current_time.minute > 0) {
                    current_time.minute--;
                    time_label2->text("Minute(" + std::to_string(current_time.minute) + ")");
                    //set_system_time(current_time);
                }
                break;

            default:
                break;
        }
    });
    time_setting_up_button3->on_click([&](egt::Event &)
    {
        switch (time_set_type) {
            case TIMESET_TYPE::DATE:
                if (current_time.day < max_days_in_month) {
                    current_time.day++;
                    time_label3->text("Day(" + std::to_string(current_time.day) + ")");
                    //set_system_time(current_time);
                }
                break;

            case TIMESET_TYPE::TIME:
                if (current_time.second < 59) {
                    current_time.second++;
                    time_label3->text("Second(" + std::to_string(current_time.second) + ")");
                    //set_system_time(current_time);
                }
                break;

            default:
                break;
        }
    });
    time_setting_down_button3->on_click([&](egt::Event &)
    {
        switch (time_set_type) {
            case TIMESET_TYPE::DATE:
                if (current_time.day > 1) {
                    current_time.day--;
                    time_label3->text("Day(" + std::to_string(current_time.day) + ")");
                    //set_system_time(current_time);
                } 
                break;

            case TIMESET_TYPE::TIME:
                if (current_time.second > 0) {
                    current_time.second--;
                    time_label3->text("Second(" + std::to_string(current_time.second) + ")");
                    //set_system_time(current_time);
                }
                break;

            default:
                break;
        }
    });
    time_switch_button.on_click([&](egt::Event &)
    {
        update_state(&page_state, IN_TIME_SETTING, dummy_time);
        switch (time_set_type) {
            case TIMESET_TYPE::DATE:
                time_label1->text("Hour(" + std::to_string(current_time.hour) + ")");
                time_label2->text("Minute(" + std::to_string(current_time.minute) + ")");
                time_label3->text("Second(" + std::to_string(current_time.second) + ")");
                time_set_type = TIMESET_TYPE::TIME;
                break;

            case TIMESET_TYPE::TIME:
                time_label1->text("Year(" + std::to_string(current_time.year) + ")");
                time_label2->text("Month(" + std::to_string(current_time.month) + ")");
                time_label3->text("Day(" + std::to_string(current_time.day) + ")");
                time_set_type = TIMESET_TYPE::DATE;
                break;

            default:
                break;
        }
    });

    time_setting_button.on_click([&](egt::Event &)
    {
        update_state(&page_state, IN_TIME_SETTING, dummy_time);
        current_time = get_current_time();
        max_days_in_month = days_in_month(current_time.year, current_time.month);
        time_set_type = TIMESET_TYPE::DATE;
        main_box.remove_all();
        main_box.add(expand_horizontal(title_main_box));
        time_label1->text("Year(" + std::to_string(current_time.year) + ")");
        time_label2->text("Month(" + std::to_string(current_time.month) + ")");
        time_label3->text("Day(" + std::to_string(current_time.day) + ")");
        main_box.add(expand(time_main_box));
    });

    wifi_button->on_click([&](egt::Event &)
    {
        main_box.remove_all();
        main_box.add(expand_horizontal(title_main_box));
        wifi_toggle->show();
        update_wifi_page(main_box, wifi_main_box, *scrollview, popup_keyboard);
    });
    home_button->on_click([&](egt::Event &)
    {
        update_state(&page_state, NOT_IN_TIME_SETTING, current_time); 
        ct_timer.stop();
        popup_keyboard.hide();
        main_box.remove_all();
        wifi_toggle->hide();
        main_box.add(expand_horizontal(title_main_box));
        main_box.add(expand(page1_main_box));
    });

    group1_toggle1->on_event([&](const egt::Event&){
        if (group1_toggle1->checked()){
            item1_mode_text = "Item1 : 30A Mode";
        }
        else{
            item1_mode_text = "Item1 : 50A Mode";   
        }
        combined = item1_mode_text + "\n" + item2_mode_text + "\n" + item3_mode_text;
        mode_view_button.text(combined);
    });
    group1_toggle2->on_event([&](const egt::Event&){
        if (group1_toggle2->checked()){
            item2_mode_text = "Item2 : 30A Mode";
        }
        else{
            item2_mode_text = "Item2 : 50A Mode";
        }
        combined = item1_mode_text + "\n" + item2_mode_text + "\n" + item3_mode_text;
        mode_view_button.text(combined);
    });

    group1_toggle3->on_event([&](const egt::Event&){
        if (group1_toggle3->checked()){
            item3_mode_text = "Item3 : 30A Mode";
        }
        else{
            item3_mode_text = "Item3 : 50A Mode";
        }
        combined = item1_mode_text + "\n" + item2_mode_text + "\n" + item3_mode_text;
        mode_view_button.text(combined);
    });
    std::string fail_log_filename = "failed_transmissions_log.csv";
    // network_timer.on_timeout([&]()
    // {
    //     if (isNetworkAvailable())
    //     {
    //         title_right_box.remove_all();
    //         title_right_box.add(right(clouda_button));
    //         title_right_box.add(right(wifi_button));
    //         title_right_box.add(right(power_button));
    //     }
    //     else
    //     {
    //         title_right_box.remove_all();
    //         title_right_box.add(right(netork_warning_button));
    //         title_right_box.add(right(wifi_button));
    //         title_right_box.add(right(power_button));
    //     }
    // });
    network_timer.on_timeout([&]()
    {
        static std::atomic<bool> busy{false};
    
        if (busy) return;  // 如果還在跑上一次的就跳過
        busy = true;
        std::thread([&]()
        {
            
            bool network_ok = isNetworkAvailable();
    
            
            if (network_ok)
            {
                title_right_box.remove_all();
                title_right_box.add(right(clouda_button));
                title_right_box.add(right(wifi_button));
                title_right_box.add(right(power_button));
                
            }
            else
            {
                title_right_box.remove_all();
                title_right_box.add(right(netork_warning_button));
                title_right_box.add(right(wifi_button));
                title_right_box.add(right(power_button));
            }
            busy = false;    

        }).detach();  // 確保不會卡住 timer 執行緒
    });
    bool is_sd_mounted = false;
    
    // log_timer.on_timeout([&]()
    // {
    //     if (is_sd_mounted == false) {
    //         if(mount_sd_card(sd_device, mount_point)){
    //             is_sd_mounted = true;
    //         }
    //     }
    //     else {
    //         if(!is_sd_card_inserted(sd_device, mount_point)) {
    //             is_sd_mounted = false;
    //         }
    //     }
    
    //     if (is_sd_mounted == true) {
    //         copy_csv_to_sd(mount_point);
    //     }
    //     else {
    //         printf("no sd\n\r");
    //     }
    //     sync();
    // });
    log_timer.on_timeout([&]() {
        static std::atomic<bool> busy{false};
    
        if (busy) return;  // 如果還在跑上一次的就跳過
        busy = true;
    
        std::thread([&]() {
            if (!is_sd_mounted) {
                if (mount_sd_card(sd_device, mount_point)) {
                    is_sd_mounted = true;
                }
            } else {
                if (!is_sd_card_inserted(sd_device, mount_point)) {
                    is_sd_mounted = false;
                }
            }
    
            if (is_sd_mounted) {
                int ret = copy_csv_to_sd(mount_point);
                if (ret < 0) {
                    // TODO: 容量不足或寫入失敗時要處理的事情
                    std::cerr << "Error: Failed to copy CSV files to SD card." << std::endl;
                }
                sync();
            } else {
                printf("no sd\n\r");
            }
    
            
            busy = false;
        }).detach();
    });

    log_timer.start();
    network_timer.start();
    
    std::thread read_controlboard_thread(read_controlboard_data_thread);
    read_controlboard_thread.detach();
    std::thread resend_ct_thread(resned_ct_log_towifi_loop, fail_log_filename, cfg.wifi_batch_size);
    resend_ct_thread.detach();
    std::thread send_ct_thread(send_ct_log_towifi_loop, fail_log_filename, cfg.log_retention_days, cfg.max_failed_log_cache);
    send_ct_thread.detach();
    
    win.show();

    return app.run();
}

