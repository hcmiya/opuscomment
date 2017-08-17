#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>

size_t fgetws2(wchar_t *buf, size_t len, FILE *fp) {
	if (len <= 1) {
		wchar_t c = fgetwc(fp);
		if (c == WEOF) {
			return (size_t)-1;
		}
		ungetwc(c, fp);
		return 0;
	}
	memset(buf, 1, len * sizeof(*buf));
	if (!fgetws(buf, len, fp)) return (size_t)-1;
	while (buf[--len]) {}
	return len;
}

#if _POSIX_C_SOURCE < 200809L
char *strndup(char const *src, size_t n) {
	char *rtn = malloc(n + 1);
	if (rtn) {
		strncpy(rtn, src, n);
		rtn[n] = '\0';
	}
	return rtn;
}
size_t strnlen(char const *src, size_t n) {
	char const *endp = src + n, *p = src;
	while (p < endp && *p) p++;
	return p - src;
}
#endif
