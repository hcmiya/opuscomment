#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ogg/ogg.h>

int page_breaks(ogg_page *og, size_t num, uint16_t *at) {
	uint_fast8_t segnum = og->header[26];
	uint16_t breaks[255] = {0};
	if (!segnum) return 0;
	
	uint8_t *endp, *p;
	uint16_t *bp = breaks;
	uint_fast16_t bytes = 0;
	p = og->header + 27;
	endp = p + segnum;
	while (p < endp) {
		bytes += *p;
		if (*p++ != 0xff) {
			*bp++ = bytes;
		}
	}
	int breaknum = bp - breaks;
	memcpy(at, breaks, breaknum * sizeof(*at));
	return breaknum;
}

#if _POSIX_C_SOURCE < 200809L
size_t strnlen(char const *src, size_t n) {
	char const *endp = src + n, *p = src;
	while (p < endp && *p) p++;
	return p - src;
}
#endif
