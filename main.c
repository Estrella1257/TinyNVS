#include <stdio.h>
#include <string.h>
#include "hal_flash.h"
#include "tinynvs.h"

int nvs_write_kv(const char *key, const void *val, uint16_t len) {
    int new_off = nvs_append_entry(g_nvs.active_sector_addr, g_nvs.write_offset, key, val, len);
    if (new_off > 0) {
        nvs_index_update(key, g_nvs.write_offset);
        g_nvs.write_offset = new_off;
        return 0;
    }
    return -1;
}

// 简易读取封装
int nvs_read_kv(const char *key, void *buf, size_t len) {
    return nvs_read_value(g_nvs.active_sector_addr, key, buf, len);
}

// --- 测试 1: 正常管理流程 ---
void test_normal_manager() {
    printf("\n====== Test 1: Normal Sector Manager ======\n");
    
    // 1. 清空所有扇区 (模拟出厂)
    for(int i=0; i<4; i++) hal_flash_erase(i * 4096);

    // 2. 初始化 (会自动格式化 Sector 0)
    nvs_init();

    // 3. 写入数据
    printf("[Action] Writing 'wifi'='TP-LINK'...\n");
    nvs_write_kv("wifi", "TP-LINK", 7);

    // 4. 验证
    char buf[32];
    memset(buf, 0, sizeof(buf)); // 先清零
    int read_len = nvs_read_kv("wifi", buf, sizeof(buf) - 1); // 留一个位置给 \0
    if (read_len > 0) {
        buf[read_len] = '\0'; // 手动补零，防止乱码
        printf("  [PASS] Read 'wifi': %s (Sector: 0x%08X, Seq: %d)\n", 
               buf, g_nvs.active_sector_addr, g_nvs.current_seq_id);
    }
}

// --- 测试 2: 掉电恢复逻辑 (核心测试) ---
void test_power_loss_recovery() {
    printf("\n====== Test 2: Power Loss Recovery (Split-Brain) ======\n");

    // 1. 制造故障现场：
    // 假设 GC 搬运了一半断电了，此时 Flash 里有两个 USED 扇区
    printf("[Setup] Simulating power loss during GC...\n");
    
    // 扇区 0: 旧的 (Seq ID = 10)
    uint32_t old_sector = 0x00000000;
    hal_flash_erase(old_sector);
    nvs_format_sector(old_sector, 5, 10); // Seq = 10
    nvs_change_sector_state(old_sector, SECTOR_STATE_USED);
    printf("  -> Created Old Sector 0 (Seq=10)\n");

    // 扇区 1: 新的 (Seq ID = 11) - 假设数据已经搬运过来了
    uint32_t new_sector = 0x00001000;
    hal_flash_erase(new_sector);
    nvs_format_sector(new_sector, 1, 11); // Seq = 11 (更大!)
    nvs_change_sector_state(new_sector, SECTOR_STATE_USED);
    printf("  -> Created New Sector 1 (Seq=11)\n");

    // 2. 此时系统处于“脑裂”状态，重启系统
    // 预期：nvs_init 应该识别出 Seq 11 是最新的，并自动擦除 Seq 10
    printf("[Action] Rebooting (nvs_init)...\n");
    
    // 重置全局变量模拟重启
    g_nvs.active_sector_addr = 0xFFFFFFFF;
    g_nvs.current_seq_id = 0;
    
    nvs_init();

    // 3. 验证结果
    printf("[Verify] Checking results...\n");
    
    // 验证 A: 当前激活的是否为 Sector 1 (Seq 11)
    if (g_nvs.active_sector_addr == new_sector && g_nvs.current_seq_id == 11) {
        printf("  [PASS] Active sector is Sector 1 (Seq 11)\n");
    } else {
        printf("  [FAIL] Active sector is 0x%08X (Seq %d)\n", g_nvs.active_sector_addr, g_nvs.current_seq_id);
    }

    // 验证 B: 旧的 Sector 0 是否被擦除
    uint32_t first_word;
    hal_flash_read(old_sector, &first_word, 4);
    if (first_word == 0xFFFFFFFF) {
        printf("  [PASS] Old Sector 0 was automatically erased!\n");
    } else {
        printf("  [FAIL] Old Sector 0 still exists! (0x%08X)\n", first_word);
    }
}

int main() {
    if (hal_flash_init() != 0) return -1;
    
    test_normal_manager();
    test_power_loss_recovery();
    
    return 0;
}