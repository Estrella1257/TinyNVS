#include <stdio.h>
#include <string.h>
#include "hal_flash.h"
#include "tinynvs.h"

#define SECTOR_0_ADDR  0x00000000


void test_delete_feature() {
    printf("\n====== Testing Delete Feature ======\n");

    // 1. 初始化
    hal_flash_erase(SECTOR_0_ADDR);
    nvs_format_sector(SECTOR_0_ADDR, 1);
    nvs_change_sector_state(SECTOR_0_ADDR, SECTOR_STATE_USED);

    // 2. 写入数据
    printf("[Action] Writing 'secret'...\n");
    nvs_append_entry(SECTOR_0_ADDR, 16, "secret", "p@ssword", 8);
    nvs_index_update("secret", 16);

    // 3. 读取确认
    char buf[32];
    int len = nvs_read_value(SECTOR_0_ADDR, "secret", buf, sizeof(buf));
    if (len > 0) {
        buf[len] = 0;
        printf("  [Check] Read before delete: %s (OK)\n", buf);
    }

    // 4. 执行删除
    printf("[Action] Deleting 'secret'...\n");
    int res = nvs_delete(SECTOR_0_ADDR, "secret");
    if (res == 0) {
        printf("  [Check] Delete function returned 0 (Success)\n");
    } else {
        printf("  [Check] Delete failed code: %d\n", res);
    }

    // 5. 再次读取 (应该失败)
    printf("[Action] Reading 'secret' again...\n");
    len = nvs_read_value(SECTOR_0_ADDR, "secret", buf, sizeof(buf));
    if (len == -1) {
        printf("  [PASS] Read failed as expected (Key not found in Index)\n");
    } else {
        printf("  [FAIL] Still read data: %s\n", buf);
    }

    // 6. 验证 RAM 索引重启后的行为 (Mount 测试)
    // 即使 RAM 索引删了，我们也要确保 Flash 上的标记是真的改了。
    // 如果 Flash 没改对，重启后 Mount 扫描时又会把这个死灰复燃的数据加载进来。
    printf("[Action] Rebooting (Mounting again)...\n");
    nvs_mount(SECTOR_0_ADDR); // 重新扫描 Flash

    len = nvs_read_value(SECTOR_0_ADDR, "secret", buf, sizeof(buf));
    if (len == -1) {
        printf("  [PASS] Key stays deleted after reboot (Flash state confirmed)\n");
    } else {
        printf("  [FAIL] Zombie key came back after reboot!\n");
    }
}

int main() {
    if (hal_flash_init() != 0) return -1;
    test_delete_feature();
    return 0;
}