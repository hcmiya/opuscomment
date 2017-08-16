#include <stdio.h>
#include <stddef.h>
#include <wchar.h>

size_t fgetws2(wchar_t *buf, size_t len, FILE *fp);

#if _POSIX_C_SOURCE < 200809L
char *strndup(char const *src, size_t n);
#endif
