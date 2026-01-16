#include <stdio.h>
#include <string.h>
#include "hal_flash.h"
#include "tinynvs.h"


#define SECTOR_0_ADDR  0x00000000
#define SECTOR_1_ADDR  0x00001000

void test_garbage_collection() {
    printf("\n====== Level 5: Garbage Collection Test ======\n");

    // 1. 初始化 Sector 0
    hal_flash_erase(SECTOR_0_ADDR);
    nvs_format_sector(SECTOR_0_ADDR, 1);
    nvs_change_sector_state(SECTOR_0_ADDR, SECTOR_STATE_USED);
    
    // 2. 写入一些数据 (包含旧版本数据)
    printf("[Step 1] Filling Sector 0...\n");
    
    // 维护一个当前写入位置的指针
    uint32_t current_offset = sizeof(nvs_sector_header_t);
    uint32_t next_offset;

    // --- 写入 Entry 1: Version 1.0 (旧数据) ---
    printf("  -> Writing 'cfg_ver' (v1.0) at offset %d\n", current_offset);
    next_offset = nvs_append_entry(SECTOR_0_ADDR, current_offset, "cfg_ver", "v1.0", 4);
    
    // 先注册当前的 offset，再更新 next_offset
    nvs_index_update("cfg_ver", current_offset); 
    current_offset = next_offset; // 指针后移


    // --- 写入 Entry 2: Wifi Config (有效数据) ---
    printf("  -> Writing 'wifi' at offset %d\n", current_offset);
    next_offset = nvs_append_entry(SECTOR_0_ADDR, current_offset, "wifi", "TP-LINK", 7);
    
    nvs_index_update("wifi", current_offset);
    current_offset = next_offset;


    // --- 写入 Entry 3: Version 2.0 (新数据) ---
    printf("  -> Writing 'cfg_ver' (v2.0) at offset %d\n", current_offset);
    next_offset = nvs_append_entry(SECTOR_0_ADDR, current_offset, "cfg_ver", "v2.0", 4);
    
    // 覆盖旧的索引
    nvs_index_update("cfg_ver", current_offset);
    current_offset = next_offset;

    
    // 3. 执行 GC (从 Sector 0 搬运到 Sector 1)
    printf("\n[Step 2] Triggering GC (Sector 0 -> Sector 1)...\n");
    printf("  -> Expectation: 'v1.0' should be dropped. 'v2.0' & 'wifi' moved.\n");
    
    // 执行 GC
    nvs_gc_collect(SECTOR_0_ADDR, SECTOR_1_ADDR);

    // 4. 验证
    printf("\n[Step 3] Verification\n");
    
    // 验证 A: 读取数据
    char buf[32];
    int len = nvs_read_value(SECTOR_1_ADDR, "cfg_ver", buf, sizeof(buf));
    if (len > 0) {
        buf[len] = 0;
        printf("  [PASS] Read 'cfg_ver': %s (Expected v2.0)\n", buf);
    } else {
        printf("  [FAIL] Read 'cfg_ver' failed: %d\n", len);
    }
    
    len = nvs_read_value(SECTOR_1_ADDR, "wifi", buf, sizeof(buf));
    if (len > 0) {
        buf[len] = 0;
        printf("  [PASS] Read 'wifi': %s\n", buf);
    }

    // 验证 B: 检查 Sector 0 是否已被擦除
    uint32_t first_word;
    hal_flash_read(SECTOR_0_ADDR, &first_word, 4);
    if (first_word == 0xFFFFFFFF) {
        printf("  [PASS] Sector 0 is erased (All 0xFF)\n");
    } else {
        printf("  [FAIL] Sector 0 is NOT erased! (0x%08X)\n", first_word);
    }
}

int main() {
    if (hal_flash_init() != 0) return -1;
    test_garbage_collection();
    return 0;
}