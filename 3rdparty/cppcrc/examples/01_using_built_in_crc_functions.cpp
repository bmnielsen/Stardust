// 1) include "cppcrc.h"
#include "../cppcrc.h"
#include <stdio.h>

// 2) use the CRC8/CRC16/CRC32 functions, for example:
int main()
{
    uint8_t data[] = {0x12, 0x5A, 0x23, 0x19, 0x92, 0xF3, 0xDE, 0xC2, 0x5A, 0x1F, 0x91, 0xA3};

    // calculate a checksum:
    uint16_t crc_value = CRC16::CCITT_FALSE::calc(data, sizeof(data));
    printf("crc_value = 0x%04X\n", crc_value);

    // view the lookup table:
    auto &crc_table = CRC16::CCITT_FALSE::table();
    printf("crc_table = {0x%04X", crc_table[0]);
    for (int i = 1; i < 256; i++)
        printf(", 0x%04X", crc_table[i]);
    printf("}\n");
}