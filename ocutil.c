#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ogg/ogg.h>
#include <stdbool.h>

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

static bool test_tag_field_keepcase(uint8_t *line, size_t n, bool *on_field) {
	size_t i;
	for (i = 0; i < n && line[i] != 0x3d; i++) {
		if (!(line[i] >= 0x20 && line[i] <= 0x7e)) {
			return false;
		}
	}
	if (i < n) *on_field = false;
	return true;
}
bool test_tag_field(uint8_t *line, size_t n, bool upcase, bool *on_field) {
	// フィールドの使用文字チェック・大文字化
	if (!upcase) {
		return test_tag_field_keepcase(line, n, on_field);
	}
	size_t i;
	for (i = 0; i < n && line[i] != 0x3d; i++) {
		if (!(line[i] >= 0x20 && line[i] <= 0x7e)) {
			return false;
		}
		if (line[i] >= 0x61 && line[i] <= 0x7a) {
			line[i] -= 32;
		}
	}
	if (i < n) *on_field = false;
	return true;
}


#if _POSIX_C_SOURCE < 200809L
size_t strnlen(char const *src, size_t n) {
	char const *endp = src + n, *p = src;
	while (p < endp && *p) p++;
	return p - src;
}
#endif
