#include "crc32.h"

#define CRC32_POLY 0xEDB88320

uint32_t crc32_init(void) {
    return 0xFFFFFFFF;
}

uint32_t crc32_update(uint32_t crc, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
   
    while (len--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++) {
            if (crc & 1) 
                crc = (crc >> 1) ^ CRC32_POLY;
            else 
                crc >>= 1;
        }
   }
   return crc;
}

uint32_t crc32_final(uint32_t crc) {
    return ~crc;
}

uint32_t crc32_compute(const void *data, size_t len) {
    uint32_t crc = crc32_init();
    crc = crc32_update(crc, data, len);
    return crc32_final(crc);
}