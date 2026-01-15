#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>
#include <stddef.h>

uint32_t crc32_init(void);
//更新 CRC (输入上一轮的 crc 值和新数据，返回新的 crc)
uint32_t crc32_update(uint32_t crc, const void *data, size_t len);
//结束计算 (取反，得到最终结果)
uint32_t crc32_final(uint32_t crc);
//辅助函数：一次性计算 (内部封装了上面三个步骤)
uint32_t crc32_compute(const void *data, size_t len);

#endif