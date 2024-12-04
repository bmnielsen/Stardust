// 1) include "cppcrc.h"
#include "../cppcrc.h"
#include <stdio.h>

int main()
{
    // 2) pick some CRC settings (these are just an example)
    using base_type = uint8_t;
    constexpr base_type poly = 0x12;
    constexpr base_type init_value = 0x34;
    constexpr bool reflect_in = true;
    constexpr bool reflect_out = true;
    constexpr base_type Xor_out = 0xFF;

    // 3) create a new CRC type by filling in the CRC parameters
    using your_crc_name = crc_utils::crc<base_type, poly, init_value, reflect_in, reflect_out, Xor_out>;

    // you can use your new CRC type to calculate a checksum
    uint8_t data[] = {0x12, 0x34, 0x56};
    auto calculated_crc = your_crc_name::calc(data, sizeof(data));
    printf("calculated_crc = 0x%02X\n", calculated_crc);

    // and access the underlying lookup table for your CRC function
    auto &crc_table = your_crc_name::table();
}