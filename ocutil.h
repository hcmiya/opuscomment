#include <stdio.h>
#include <stddef.h>
#include <wchar.h>
#include <stdint.h>

uint16_t oi16(uint16_t i);
uint32_t oi32(uint32_t i);
uint64_t oi64(uint64_t i);
int page_breaks(ogg_page *og, size_t num, uint16_t *at);

#if _POSIX_C_SOURCE < 200809L
char *strndup(char const *src, size_t n);
size_t strnlen(char const *src, size_t n);
#endif
