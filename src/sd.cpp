#include "sd.h"

bool mount_sd_card(const std::string& sd_device, const std::string& mount_point) {
    if (system(("ls " + sd_device).c_str()) != 0) {
        return false;
    }

    std::string mount_cmd = "mount " + sd_device + " " + mount_point;
    if (system(mount_cmd.c_str()) == 0) {
        return true;
    } else {
        std::cerr << "mount fail, format FAT32 try...\n\r" << std::endl;

        std::string format_cmd = "mkfs.vfat " + sd_device;
        if (system(format_cmd.c_str()) == 0) {
            std::cout << "format sucess, remount\n\r" << std::endl;
            if (system(mount_cmd.c_str()) == 0) {
                std::cout << "remount sucess\n\r" << std::endl;
                return true;
            } else {
                std::cerr << "remount fail\n\r" << std::endl;
                return false;
            }
        } else {
            std::cerr << "format fail\n\r" << std::endl;
            return false;
        }
        return false;
    }
}

bool is_sd_card_inserted(const std::string& sd_device, const std::string& mount_point) {
    
    if (system(("ls " + sd_device).c_str()) != 0) {
        std::cout << "SD not found, umount \n\r" << mount_point << std::endl;
        system(("umount " + mount_point).c_str());
        return false;
    }
    return true;
}

