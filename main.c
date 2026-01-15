#include <stdio.h>
#include <string.h>
#include "hal_flash.h"
#include "tinynvs_def.h"

// --- 函数声明 (模拟 tinynvs.h) ---
int nvs_format_sector(uint32_t sector_addr, uint32_t old_erase_count);
int nvs_change_sector_state(uint32_t sector_addr, nvs_sector_state_t new_state);
int nvs_append_entry(uint32_t sector_addr, uint32_t current_offset, const char *key, const void *data, uint16_t len);
int nvs_read_value(uint32_t sector_addr, const char *key, void *out_buf, size_t buf_len);
void nvs_index_update(const char *key, uint32_t offset);
uint32_t nvs_mount(uint32_t sector_addr);
void nvs_index_clear(void); 

// --- 测试逻辑 ---
void full_system_check() {
    uint32_t sector = 0;
    char buf[64];
    int res;

    printf("\n====== TinyNVS Level 4 Verification ======\n");

    // 1. 初始化与写入
    printf("\n[Step 1] Initialize & Write Data\n");
    hal_flash_erase(sector); // 彻底擦除
    nvs_format_sector(sector, 1);
    nvs_change_sector_state(sector, SECTOR_STATE_USED);
    
    // 写入 Key: "device_name"
    // 注意：当前我们还没封装 nvs_write，所以需要手动 append + update index
    printf("  -> Writing 'device_name' = 'TinyKV_Box'\n");
    int off1 = nvs_append_entry(sector, sizeof(nvs_sector_header_t), "device_name", "TinyKV_Box", 10);
    nvs_index_update("device_name", sizeof(nvs_sector_header_t));

    // 写入 Key: "version"
    printf("  -> Writing 'version' = '1.0.4'\n");
    nvs_append_entry(sector, off1, "version", "1.0.4", 5);
    nvs_index_update("version", off1);


    // 2. 模拟重启 (Reboot)
    printf("\n[Step 2] Simulate Reboot (Clear RAM & Mount)\n");
    // 这一步会清空 RAM 索引，强迫系统从 Flash 重新扫描
    nvs_mount(sector); 
    printf("  -> Mount complete.\n");


    // 3. 读取测试 (Happy Path)
    printf("\n[Step 3] Read Verification\n");
    memset(buf, 0, sizeof(buf));
    res = nvs_read_value(sector, "device_name", buf, sizeof(buf));
    if (res > 0) {
        printf("  [PASS] Read 'device_name': %s\n", buf);
    } else {
        printf("  [FAIL] Read error code: %d\n", res);
    }


    // 4. 缓冲区过小测试 (Buffer Too Small)
    printf("\n[Step 4] Small Buffer Test\n");
    char small_buf[2]; // 太小了，装不下 "TinyKV_Box"
    res = nvs_read_value(sector, "device_name", small_buf, sizeof(small_buf));
    if (res == -3) {
        printf("  [PASS] Correctly returned -3 (Buffer too small)\n");
    } else {
        printf("  [FAIL] Expected -3, got %d\n", res);
    }


    // 5. CRC 错误测试 (Data Corruption)
    printf("\n[Step 5] CRC Corruption Test\n");
    // 我们手动去 Flash 里搞破坏。
    // 'device_name' 的数据大概在 Header(16) + EntryHeader(12) + KeyLen(11) 的位置
    // 我们直接把数据区域的一个字节改写成 0x00
    uint32_t corruption_addr = sizeof(nvs_sector_header_t) + sizeof(nvs_entry_header_t) + strlen("device_name");
    uint8_t dirty_byte = 0x00; 
    
    printf("  -> Corrupting Flash byte at offset %d...\n", corruption_addr);
    hal_flash_write(sector + corruption_addr, &dirty_byte, 1);

    // 再次尝试读取
    res = nvs_read_value(sector, "device_name", buf, sizeof(buf));
    if (res == -2) {
        printf("  [PASS] Correctly detected CRC Mismatch (Code -2)\n");
    } else {
        printf("  [FAIL] Security hole! Read garbage data: %d\n", res);
    }
}

int main() {
    if (hal_flash_init() != 0) return -1;
    full_system_check();
    return 0;
}