#include "sd.h"

bool file_exists(const std::string& path) {
    return (access(path.c_str(), F_OK) == 0);
}
bool mount_sd_card(const std::string& sd_device, const std::string& mount_point) {
    if (!file_exists(sd_device)) {
        return false;
    }

    if (mount(sd_device.c_str(), mount_point.c_str(), "vfat", 0, NULL) == 0) {
        return true;
    } else {
        std::cerr << "Mount failed, formatting as FAT32...\n";
        std::string format_cmd = "mkfs.vfat " + sd_device;
        if (system(format_cmd.c_str()) == 0) {
            std::cout << "Format successful, remounting...\n";
            return (mount(sd_device.c_str(), mount_point.c_str(), "vfat", 0, NULL) == 0);
        } else {
            std::cerr << "Format failed.\n";
            return false;
        }
    }
}

bool is_sd_card_inserted(const std::string& sd_device, const std::string& mount_point) {
    if (!file_exists(sd_device)) {
        std::cout << "SD not found, unmounting: " << mount_point << std::endl;

        // 使用 umount() 來卸載 SD 卡
        if (umount(mount_point.c_str()) == 0) {
            std::cout << "Unmount successful\n";
        } else {
            std::cerr << "Unmount failed\n";
        }

        return false;
    }
    return true;
}


