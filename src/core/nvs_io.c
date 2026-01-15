#include <string.h>
#include "hal_flash.h"
#include "tinynvs_def.h"
#include "crc32.h"

int nvs_append_entry(uint32_t sector_addr, uint32_t current_offset, const char *key, const void* data, uint16_t len) {
    uint8_t key_len = strlen(key);

    //对齐后的总大小
    uint32_t payload_len = key_len + len;
    uint32_t total_size = sizeof(nvs_entry_header_t) + ALIGN_UP(payload_len, 4);

    if (current_offset + total_size > NVS_SECTOR_SIZE) {
        return -1;
    }

    uint32_t write_addr = sector_addr + current_offset;

    uint32_t check_crc = crc32_init();
    check_crc = crc32_update(check_crc, key, key_len);
    check_crc = crc32_update(check_crc, data, len);
    check_crc = crc32_final(check_crc);

    nvs_entry_header_t header;
    header.key_len = key_len;
    header.type = 0;
    header.data_len = len;
    header.crc = check_crc;
    header.state = ENTRY_STATE_VALID;

    uint32_t payload_addr = write_addr + sizeof(header);

    hal_flash_write(payload_addr, key , key_len);
    hal_flash_write(payload_addr +  key_len, data, len);
    hal_flash_write(write_addr, &header, sizeof(header));

    return current_offset + total_size;
}
