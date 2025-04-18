#ifndef SD_H
#define SD_H
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <cstdlib>
#include <unistd.h> 
#include <sys/mount.h>
namespace fs = std::filesystem;

bool mount_sd_card(const std::string& sd_device, const std::string& mount_point);
bool is_sd_card_inserted(const std::string& sd_device, const std::string& mount_point);
bool file_exists(const std::string& path);

#endif