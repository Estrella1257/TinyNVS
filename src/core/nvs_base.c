#include "hal_flash.h"
#include "tinynvs_def.h"
#include <stddef.h>

int nvs_format_sector(uint32_t sector_addr, uint32_t old_erase_count, uint32_t seq_id) {
    if (hal_flash_erase(sector_addr) != 0) return -1;

    nvs_sector_header_t header;
    header.magic = NVS_MAGIC;
    header.erase_count = old_erase_count + 1;
    header.state = SECTOR_STATE_EMPTY;
    header.reserved = 0xFFFFFFFF;
    header.seq_id = seq_id;

    return hal_flash_write(sector_addr, &header, sizeof(header));
}

int nvs_change_sector_state(uint32_t sector_addr, nvs_sector_state_t new_state) {
    uint32_t state_addr = sector_addr + offsetof(nvs_sector_header_t, state);

    return hal_flash_write(state_addr, &new_state, sizeof(new_state));
}