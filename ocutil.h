#include <stdio.h>
#include <stddef.h>
#include <wchar.h>
#include <stdint.h>

uint16_t oi16(uint16_t);
uint32_t oi32(uint32_t);
uint64_t oi64(uint64_t);
size_t fgetws2(wchar_t *buf, size_t len, FILE *fp);

#if _POSIX_C_SOURCE < 200809L
char *strndup(char const *src, size_t n);
size_t strnlen(char const *src, size_t n);
#endif
