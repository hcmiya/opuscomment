#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ogg/ogg.h>
#include <stdbool.h>

static bool test_tag_field_keepcase(uint8_t *line, size_t n, bool *on_field) {
	size_t i;
	bool valid = true;
	for (i = 0; i < n && line[i] != 0x3d; i++) {
		if (!(line[i] >= 0x20 && line[i] <= 0x7e)) {
			valid = false;
		}
	}
	if (i < n) *on_field = false;
	return valid;
}
bool test_tag_field(uint8_t *line, size_t n, bool upcase, bool *on_field, bool *upcase_applied_) {
	// フィールドの使用文字チェック・大文字化
	if (!upcase) {
		return test_tag_field_keepcase(line, n, on_field);
	}
	
	bool dummy_, *upcase_applied;
	upcase_applied = upcase_applied_ ? upcase_applied_ : &dummy_;
	
	size_t i;
	bool valid = true;
	for (i = 0; i < n && line[i] != 0x3d; i++) {
		if (!(line[i] >= 0x20 && line[i] <= 0x7e)) {
			valid = false;
		}
		if (line[i] >= 0x61 && line[i] <= 0x7a) {
			line[i] -= 32;
			*upcase_applied = true;
		}
	}
	if (i < n) *on_field = false;
	return valid;
}

size_t fill_buffer(void *buf, size_t left, size_t buflen, FILE *fp) {
	size_t filllen = left > buflen ? buflen : left;
	size_t readlen = fread(buf, 1, filllen, fp);
	return filllen;
}

#if _POSIX_C_SOURCE < 200809L
size_t strnlen(char const *src, size_t n) {
	char const *endp = src + n, *p = src;
	while (p < endp && *p) p++;
	return p - src;
}
#endif
