#include <stdio.h>
#include <string.h>
#include "hal_flash.h"
#include "tinynvs.h"

void test_static_wear_leveling() {
    printf("\n====== Test 4: Static Wear Leveling (The Nail Problem) ======\n");

    // 1. 初始化
    for(int i=0; i<4; i++) hal_flash_erase(i * 4096);
    nvs_init();

    // 2. 构造场景
    // Sector 0: 当前活跃，磨损 100
    g_nvs.active_sector_addr = 0x00000000;
    g_nvs.sector_erase_counts[0] = 100;

    // Sector 1: 钉子户 (存储冷数据)，磨损 5 (非常年轻)
    // 我们手动写入一些数据，并标记为 USED
    uint32_t cold_sector = 0x00001000;
    nvs_format_sector(cold_sector, 5, 1); // 模拟 Count=5
    nvs_change_sector_state(cold_sector, SECTOR_STATE_USED);
    nvs_append_entry(cold_sector, 16, "serial", "SN123456", 8); // 写入冷数据
    g_nvs.sector_erase_counts[1] = 5; // RAM 也要同步

    // Sector 2 & 3: 其他空闲扇区，磨损严重 (100)
    g_nvs.sector_erase_counts[2] = 100;
    g_nvs.sector_erase_counts[3] = 100;

    printf("[Setup] Scoreboard:\n");
    printf("  Sec0 (Active): 100\n");
    printf("  Sec1 (Static): 5   <-- The Nail (Should be moved)\n");
    printf("  Sec2 (Free)  : 100\n");
    printf("  Sec3 (Free)  : 100\n");

    // 3. 触发静态磨损检查
    // 阈值是 10，这里差值 100 - 5 = 95 > 10，应该触发
    printf("[Action] Triggering Static WL Check...\n");
    int triggered = nvs_check_and_execute_static_wl();

    if (triggered) {
        printf("  [PASS] Static WL Triggered!\n");
    } else {
        printf("  [FAIL] Static WL NOT Triggered!\n");
    }

    // 4. 验证结果
    // 钉子户 Sector 1 应该被擦除了 (变为空)，计数+1变成6
    // 数据应该被搬到了 Sector 2 或 3 (我们不关心具体去哪了，只要搬走就行)
    
    // 检查 Sector 1 现在的状态
    nvs_sector_header_t header;
    hal_flash_read(cold_sector, &header, sizeof(header));
    
    // 验证 A: Sector 1 被擦除了吗？ (Empty 或 FF)
    // 注意：gc_collect 最后会 erase 源扇区
    // 我们读取前 4 字节，如果是 FF 说明擦除了
    uint32_t first_word;
    hal_flash_read(cold_sector, &first_word, 4);

    if (first_word == 0xFFFFFFFF) {
        printf("  [PASS] Cold Sector 1 was erased and freed!\n");
    } else {
        printf("  [FAIL] Sector 1 is not empty! (0x%08X)\n", first_word);
    }

    // 验证 B: 下一次分配是否会选中 Sector 1？
    // 因为 Sector 1 现在是 6 (5+1)，其他都是 100，它应该最年轻
    uint32_t best = nvs_get_best_free_sector(0);
    if (best == cold_sector) {
        printf("  [PASS] Sector 1 is now the Best Free Sector!\n");
    } else {
        printf("  [FAIL] Allocator picked 0x%08X instead of Sector 1\n", best);
    }
}

int main() {
    if (hal_flash_init() != 0) return -1;
    test_static_wear_leveling();
    return 0;
}