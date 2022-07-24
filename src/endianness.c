#include <stdint.h>
int endian_test()
{
    uint32_t sample = {0x01020304};
    uint8_t *test = (uint8_t *)&sample;
    if (test[0] == 4)
        return -1;
    else if (test[0] == 1)
        return 1;
    else
        return 0;
}

uint16_t oi16(uint16_t i)
{
    int et = endian_test();
    if (et == -1)
        return i;
    else if (et == 1)
        return (i << 8 | i >> 8);
    else
    {
        uint8_t *val = (uint8_t *)&i;
        uint8_t out[2];
        out[0] = val[0];
        out[1] = val[1];
        return *(uint16_t*)out;
    }
}
uint32_t oi32(uint32_t i)
{
    int et = endian_test();
    if (et == -1)
        return i;
    else if (et == 1)
        return (i << 24
               | (i & (255ULL << 8)) << 8
               | (i & (255ULL << 16)) >> 8
               | i >> 24);
    else
    {
        uint8_t *val = (uint8_t *)&i;
        uint8_t out[4];
        out[0] = val[0];
        out[1] = val[1];
        out[2] = val[2];
        out[3] = val[3];
        return *(uint32_t*)out;
    }
}
uint64_t oi64(uint64_t i)
{
    int et = endian_test();
    if (et == -1)
        return i;
    else if (et == 1)
        return (i << 56
               | (i & (255ULL << 8)) << 40
               | (i & (255ULL << 16)) >> 24
               | (i & (255ULL << 24)) >> 8
               | (i & (255ULL << 32)) >> 8
               | (i & (255ULL << 40)) >> 24
               | (i & (255ULL << 48)) >> 40
               | i >> 56);
    else
    {
        uint8_t *val = (uint8_t *)&i;
        uint8_t out[8];
        out[0] = val[0];
        out[1] = val[1];
        out[2] = val[2];
        out[3] = val[3];
        out[4] = val[4];
        out[5] = val[5];
        out[6] = val[6];
        out[7] = val[7];
        return *(uint64_t*)out;
    }
}
