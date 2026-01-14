#include <stdio.h>
#include "hal_flash.h"
#include "tinynvs_def.h"

// 声明外部函数 (未来放在 tinynvs.h 中)
int nvs_format_sector(uint32_t sector_addr, uint32_t old_erase_count);
int nvs_change_sector_state(uint32_t sector_addr, nvs_sector_state_t new_state);

void test_sector_lifecycle() {
    uint32_t sector = 0;
    nvs_sector_header_t header;

    printf("[Test] Formatting sector 0...\n");
    nvs_format_sector(sector, 10); // 假设之前擦除过 10 次

    // 验证初始化
    hal_flash_read(sector, &header, sizeof(header));
    if (header.magic == NVS_MAGIC && header.state == SECTOR_STATE_EMPTY) {
        printf("  -> Format OK. State: EMPTY\n");
    }

    // 状态流转 -> USED
    printf("[Test] Changing state to USED...\n");
    nvs_change_sector_state(sector, SECTOR_STATE_USED);
    
    hal_flash_read(sector, &header, sizeof(header));
    if (header.state == SECTOR_STATE_USED) {
        printf("  -> OK. State: USED\n");
    }
    
    // 状态流转 -> FULL
    printf("[Test] Changing state to FULL...\n");
    nvs_change_sector_state(sector, SECTOR_STATE_FULL);
    
    hal_flash_read(sector, &header, sizeof(header));
    if (header.state == SECTOR_STATE_FULL) {
        printf("  -> OK. State: FULL\n");
    }
}

int main() {
    if (hal_flash_init() != 0) {
        printf("Flash init failed!\n");
        return -1;
    }
    
    test_sector_lifecycle();
    return 0;
}