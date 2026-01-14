#include <stdio.h>
#include "hal_flash.h"

void print_hex(char *desc, uint8_t *data, int len) {
    printf("%s: ", desc);
    for(int i=0; i<len; i++) printf("%02X ", data[i]);
    printf("\n");
}

int main() {
    hal_flash_init();

    uint32_t test_addr = 0x0000;
    uint8_t buf[4];
    
    // 1. 初始状态读取 (应该是 FF FF FF FF)
    hal_flash_read(test_addr, buf, 4);
    print_hex("Init Read", buf, 4);

    // 2. 第一次写入: AA (1010 1010)
    uint8_t write_1[] = {0xAA, 0xAA, 0xAA, 0xAA};
    printf("Writing 0xAA...\n");
    hal_flash_write(test_addr, write_1, 4);
    
    hal_flash_read(test_addr, buf, 4);
    print_hex("Read after Write AA", buf, 4);

    // 3. 第二次写入: 55 (0101 0101) - 此时没有擦除！
    // 预期结果：AA (1010 1010) & 55 (0101 0101) = 00
    // 同时 Mock 会报错警告
    uint8_t write_2[] = {0x55, 0x55, 0x55, 0x55};
    printf("Writing 0x55 (No Erase)...\n");
    hal_flash_write(test_addr, write_2, 4);

    hal_flash_read(test_addr, buf, 4);
    print_hex("Read after Write 55", buf, 4); // 应该看到 00 00 00 00

    // 4. 擦除后写入
    printf("Erasing Sector 0...\n");
    hal_flash_erase(0);
    
    printf("Writing 0x55 (After Erase)...\n");
    hal_flash_write(test_addr, write_2, 4);
    
    hal_flash_read(test_addr, buf, 4);
    print_hex("Read after Erase+Write 55", buf, 4); // 应该完美显示 55

    return 0;
}