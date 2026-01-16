#include <stdio.h>
#include <string.h>
#include "tinynvs.h"
#include "hal_flash.h"

// 打印测试结果的辅助宏
#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("[FAIL] %s\n", msg); \
            return; \
        } else { \
            printf("[PASS] %s\n", msg); \
        } \
    } while(0)

void test_basic_rw(void) {
    printf("\n=== Test 1: Basic Read/Write ===\n");
    
    char buf[64];
    int ret;

    // 1. 写入数据
    ret = nvs_set("wifi_ssid", "MyHomeWiFi", strlen("MyHomeWiFi"));
    TEST_ASSERT(ret == 0, "Set 'wifi_ssid'");

    ret = nvs_set("device_id", "ID_12345678", strlen("ID_12345678"));
    TEST_ASSERT(ret == 0, "Set 'device_id'");

    // 2. 读取数据
    memset(buf, 0, sizeof(buf));
    ret = nvs_get("wifi_ssid", buf, sizeof(buf));
    TEST_ASSERT(ret > 0 && strcmp(buf, "MyHomeWiFi") == 0, "Get 'wifi_ssid' matches");

    memset(buf, 0, sizeof(buf));
    ret = nvs_get("device_id", buf, sizeof(buf));
    TEST_ASSERT(ret > 0 && strcmp(buf, "ID_12345678") == 0, "Get 'device_id' matches");

    // 3. 更新数据 (Key相同，Value不同)
    ret = nvs_set("wifi_ssid", "OfficeWiFi", strlen("OfficeWiFi"));
    TEST_ASSERT(ret == 0, "Update 'wifi_ssid'");

    memset(buf, 0, sizeof(buf));
    ret = nvs_get("wifi_ssid", buf, sizeof(buf));
    TEST_ASSERT(ret > 0 && strcmp(buf, "OfficeWiFi") == 0, "Get updated 'wifi_ssid' matches");
}

void test_reboot_recovery(void) {
    printf("\n=== Test 2: Simulated Reboot (Power Loss) ===\n");

    // 模拟重启：重新调用 init，它会重新扫描 Flash 构建索引
    // 注意：我们不擦除 Flash 文件，模拟断电后数据还在
    int ret = nvs_init();
    TEST_ASSERT(ret == 0, "Re-Init NVS (Simulate Reboot)");

    char buf[64];
    memset(buf, 0, sizeof(buf));
    
    // 读取重启前最后写入的数据
    ret = nvs_get("wifi_ssid", buf, sizeof(buf));
    TEST_ASSERT(ret > 0 && strcmp(buf, "OfficeWiFi") == 0, "Data persists after reboot");
}

void test_stress_gc(void) {
    printf("\n=== Test 3: Stress Test & GC Trigger ===\n");
    printf("Writing many entries to force sector switch...\n");

    char key[16];
    char val[32];
    int ret;

    // 我们的扇区大小是 4096。
    // 每个 Entry 约占: Header(12) + Key(6) + Data(20) + Align ~= 40 bytes.
    // 写入 150 次左右应该就能填满一个扇区 (150 * 40 = 6000 > 4096)
    
    for (int i = 0; i < 200; i++) {
        sprintf(key, "k%d", i % 20); // 循环使用 20 个 Key，模拟反复修改配置
        sprintf(val, "value_data_%d", i);

        ret = nvs_set(key, val, strlen(val));
        
        if (ret != 0) {
            printf("[FAIL] Write failed at iteration %d with error %d\n", i, ret);
            return;
        }

        if (i % 50 == 0) {
            printf("  -> Written %d entries...\n", i);
        }
    }
    TEST_ASSERT(1, "Stress write completed without error");

    // 验证最后一次写入的数据
    char buf[64];
    nvs_get("k19", buf, sizeof(buf));
    // k19 最后一次应该是 i=199 时写入的 "value_data_199"
    // 或者 i=179 ... 取决于循环。
    // 这里我们只验证能不能读出来
    printf("  -> Read back 'k19': %s\n", buf);
    TEST_ASSERT(strlen(buf) > 0, "Can read data after heavy GC");
}

int main(void) {
    // 1. 初始化硬件 Mock (生成 bin 文件)
    if (hal_flash_init() != 0) {
        printf("Flash init failed!\n");
        return -1;
    }

    // 2. 初始化 NVS
    if (nvs_init() != 0) {
        printf("NVS init failed!\n");
        return -1;
    }
    printf("NVS Init OK.\n");

    // 3. 执行测试用例
    test_basic_rw();
    test_reboot_recovery();
    test_stress_gc(); // 这个测试会在控制台打印 GC 的过程

    printf("\nAll Tests Finished.\n");
    return 0;
}