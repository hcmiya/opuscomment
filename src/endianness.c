#include <stdint.h>
int endian_test()
{
    int out[3] = {0, -1, 1}; // {terrible, little, big} endian
    uint32_t sample = {0x02000001};
    uint8_t *test = (uint8_t *)&sample;
    return out[test[0]];
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
        uint16_t sample = {0x0100};
        uint8_t out[2];
        uint8_t *ind = (uint8_t *)&sample;
        uint8_t *val = (uint8_t *)&i;
        for (int j = 0; j < 2; j++)
            out[ind[j]] = val[j];
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
        uint32_t sample = {0x03020100};
        uint8_t out[4];
        uint8_t *ind = (uint8_t *)&sample;
        uint8_t *val = (uint8_t *)&i;
        for (int j = 0; j < 4; j++)
            out[ind[j]] = val[j];
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
        uint64_t sample = {0x0706050403020100};
        uint8_t out[8];
        uint8_t *ind = (uint8_t *)&sample;
        uint8_t *val = (uint8_t *)&i;
        for (int j = 0; j < 8; j++)
            out[ind[j]] = val[j];
        return *(uint64_t*)out;
    }
}
