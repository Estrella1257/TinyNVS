// #include <stdio.h>
// #include "hal_flash.h"
// #include "tinynvs_def.h"

// // 声明外部函数 (未来放在 tinynvs.h 中)
// int nvs_format_sector(uint32_t sector_addr, uint32_t old_erase_count);
// int nvs_change_sector_state(uint32_t sector_addr, nvs_sector_state_t new_state);

// void test_sector_lifecycle() {
//     uint32_t sector = 0;
//     nvs_sector_header_t header;

//     printf("[Test] Formatting sector 0...\n");
//     nvs_format_sector(sector, 10); // 假设之前擦除过 10 次

//     // 验证初始化
//     hal_flash_read(sector, &header, sizeof(header));
//     if (header.magic == NVS_MAGIC && header.state == SECTOR_STATE_EMPTY) {
//         printf("  -> Format OK. State: EMPTY\n");
//     }

//     // 状态流转 -> USED
//     printf("[Test] Changing state to USED...\n");
//     nvs_change_sector_state(sector, SECTOR_STATE_USED);
    
//     hal_flash_read(sector, &header, sizeof(header));
//     if (header.state == SECTOR_STATE_USED) {
//         printf("  -> OK. State: USED\n");
//     }
    
//     // 状态流转 -> FULL
//     printf("[Test] Changing state to FULL...\n");
//     nvs_change_sector_state(sector, SECTOR_STATE_FULL);
    
//     hal_flash_read(sector, &header, sizeof(header));
//     if (header.state == SECTOR_STATE_FULL) {
//         printf("  -> OK. State: FULL\n");
//     }
// }

// int main() {
//     if (hal_flash_init() != 0) {
//         printf("Flash init failed!\n");
//         return -1;
//     }
    
//     test_sector_lifecycle();
//     return 0;
// }

#include <stdio.h>
#include <string.h>
#include "hal_flash.h"
#include "tinynvs_def.h"

// 声明外部函数
int nvs_format_sector(uint32_t sector_addr, uint32_t old_erase_count);
int nvs_change_sector_state(uint32_t sector_addr, nvs_sector_state_t new_state);
int nvs_append_entry(uint32_t sector_addr, uint32_t current_offset, 
                     const char *key, const void *data, uint16_t len);

void test_kv_write() {
    uint32_t sector = 0;
    
    // 1. 初始化环境
    printf("\n=== Testing KV Write ===\n");
    nvs_format_sector(sector, 1);
    nvs_change_sector_state(sector, SECTOR_STATE_USED);

    // 初始偏移量：跳过扇区头部 (16字节)
    uint32_t offset = sizeof(nvs_sector_header_t);

    // 2. 写入第一条数据
    char *key1 = "wifi_ssid";
    char *val1 = "MyHomeWiFi";
    printf("Writing Key: %s, Val: %s\n", key1, val1);
    
    int new_offset = nvs_append_entry(sector, offset, key1, val1, strlen(val1));
    if (new_offset > 0) {
        printf("  -> Write OK. New offset: %d\n", new_offset);
        offset = new_offset;
    } else {
        printf("  -> Write FAILED\n");
    }

    // 3. 写入第二条数据 (测试连续写入)
    char *key2 = "boot_count";
    uint32_t val2 = 12345;
    printf("Writing Key: %s, Val: %d\n", key2, val2);
    
    new_offset = nvs_append_entry(sector, offset, key2, &val2, sizeof(val2));
    if (new_offset > 0) {
        printf("  -> Write OK. New offset: %d\n", new_offset);
        offset = new_offset;
    }

    // 4. 读取验证 (简单验证内存是否写进去了)
    // 我们读取刚才写入第一条的位置
    nvs_entry_header_t read_header;
    hal_flash_read(sector + sizeof(nvs_sector_header_t), &read_header, sizeof(read_header));
    
    printf("\n[Verification] Reading 1st Entry Header:\n");
    printf("  Key Len: %d (Expected %ld)\n", read_header.key_len, strlen(key1));
    printf("  Data Len: %d (Expected %ld)\n", read_header.data_len, strlen(val1));
    printf("  State: 0x%08X (Expected VALID 0xFFFF0000)\n", read_header.state);
}

int main() {
    if (hal_flash_init() != 0) return -1;
    test_kv_write();
    return 0;
}