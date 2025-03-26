#ifndef CRC16_H
#define CRC16_H

#include <cstdint>
#include <vector>
#include <stdlib.h>
std::vector<uint16_t> generate_crc16_table();
uint16_t calculate_crc16(const uint8_t* data, size_t length, uint16_t crc = 0xFFFF);

#endif // CRC16_H
