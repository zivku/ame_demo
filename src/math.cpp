#include "math.h"

std::vector<uint16_t> generate_crc16_table() {
    std::vector<uint16_t> table(256);
    uint16_t polynomial = 0xA001;
    for (uint16_t i = 0; i < 256; i++) {
        uint16_t crc = i;
        for (uint16_t j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ polynomial;
            else
                crc >>= 1;
        }
        table[i] = crc;
    }
    return table;
}

uint16_t calculate_crc16(const uint8_t* data, size_t length, uint16_t crc) {
    static std::vector<uint16_t> table = generate_crc16_table();
    for (size_t i = 0; i < length; i++) {
        crc = (crc >> 8) ^ table[(crc ^ data[i]) & 0xFF];
    }
    return crc;
}
