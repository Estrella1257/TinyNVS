#include <stdio.h>
#include <string.h>
#include "hal_flash.h"
#include "tinynvs.h"

extern nvs_manager_t g_nvs;

// --- 封装写入函数 (带 GC 和 磨损均衡逻辑) ---
int nvs_write_kv(const char *key, const void *val, uint16_t len) {
    // 1. 尝试直接写入
    int new_off = nvs_append_entry(g_nvs.active_sector_addr, g_nvs.write_offset, key, val, len);
    
    // 2. 如果成功，更新索引和偏移
    if (new_off > 0) {
        nvs_index_update(key, g_nvs.write_offset);
        g_nvs.write_offset = new_off;
        return 0; // 写入成功
    }

    // 3. 如果失败 (返回 -1，表示扇区满了)
    if (new_off == -1) {
        printf("[NVS] Sector Full! Triggering Wear-Leveling GC...\n");

        // A. 【选秀】挑选最佳备胎 (Level 7 核心：找最年轻的空闲扇区)
        uint32_t target_sector = nvs_get_best_free_sector();
        
        if (target_sector == 0) {
            printf("[Fatal] No free sectors available!\n");
            return -2;
        }

        // B. 获取目标扇区的索引，以便更新记分牌
        int target_idx = (target_sector - NVS_BASE_ADDR) / 4096;
        
        // C. 执行 GC (数据搬运: Old -> New)
        // 注意：nvs_gc_collect 内部负责 Format 新扇区(seq+1) 和 Erase 旧扇区
        uint32_t next_offset = nvs_gc_collect(g_nvs.active_sector_addr, target_sector);
        
        // D. 更新 RAM 记分牌
        // 目标扇区被 Format 了一次，所以计数+1
        // (注意：如果在 nvs_gc_collect 内部已经更新了 RAM 计数，这里就不需要了。
        //  为了保险起见，这里显式更新一下，确保 RAM 里的数据是最新的)
        g_nvs.sector_erase_counts[target_idx]++; 

        // E. 切换身份 (更改当前活跃扇区)
        printf("[Manager] Switching Active: 0x%08X -> 0x%08X\n", g_nvs.active_sector_addr, target_sector);
        g_nvs.active_sector_addr = target_sector;
        g_nvs.write_offset = next_offset;

        // F. 再次尝试写入 (递归调用)
        // 因为刚才 GC 腾出了空间，这次应该能写进去了
        return nvs_write_kv(key, val, len); 
    }

    return -1; // 未知错误
}

// --- 简易读取封装 ---
int nvs_read_kv(const char *key, void *buf, size_t len) {
    return nvs_read_value(g_nvs.active_sector_addr, key, buf, len);
}

// --- Level 7 测试用例 ---
void test_wear_leveling() {
    printf("\n====== Test 3: Dynamic Wear Leveling ======\n");
    
    // 1. 初始化环境 (模拟全擦除)
    for(int i=0; i<4; i++) hal_flash_erase(i * 4096);
    nvs_init();

    // 2. 【作弊】人为修改 RAM 里的记分牌，制造贫富差距
    // 假设 Sector 0 是当前的活跃扇区
    // 我们把 Sector 1 和 2 的寿命设为 1000 (模拟已经磨损严重)
    // Sector 3 设为 5 (模拟非常年轻)
    printf("[Setup] Manipulating Scoreboard (RAM)...\n");
    g_nvs.sector_erase_counts[1] = 1000;
    g_nvs.sector_erase_counts[2] = 1000;
    g_nvs.sector_erase_counts[3] = 5;
    
    printf("  Sector 1 EraseCount: 1000 (Old)\n");
    printf("  Sector 2 EraseCount: 1000 (Old)\n");
    printf("  Sector 3 EraseCount: 5    (Young)\n");

    // 3. 填满当前扇区，强制触发 GC
    // 为了快速填满，我们不需要真的写 4KB 数据，直接修改 write_offset 指针即可
    // 4096 - 64 意味着只剩下 64 字节，写一个新的 KV 肯定写不下
    printf("[Action] Simulating Sector 0 Full...\n");
    g_nvs.write_offset = 4096; 

    // 4. 写入一条数据，此时扇区已满，应该触发 GC 和 选秀算法
    printf("[Action] Writing 'trigger' data to force GC...\n");
    nvs_write_kv("trigger", "wear_leveling_data", 18);

    // 5. 验证：新的 Active Sector 是谁？
    // 预期：系统应该跳过 1 和 2，选中 Sector 3 (0x3000)，因为它是最年轻的
    printf("\n[Verify] Checking Selection Result...\n");
    if (g_nvs.active_sector_addr == 0x00003000) {
        printf("  [PASS] System selected Sector 3 (The youngest one)!\n");
        printf("  [PASS] Sector 3 EraseCount is now: %d (Should be 6)\n", g_nvs.sector_erase_counts[3]);
    } else {
        printf("  [FAIL] System selected 0x%08X (Bad choice)\n", g_nvs.active_sector_addr);
    }
}

int main() {
    // 硬件初始化
    if (hal_flash_init() != 0) return -1;

    // 运行磨损均衡测试
    test_wear_leveling();
    
    return 0;
}