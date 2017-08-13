#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <locale.h>
#include <langinfo.h>
#include <iconv.h>
#include <wchar.h>
#include <stdarg.h>
#include <errno.h>

#include "opuscomment.h"
#include "global.h"

void add_tag(char *tag) {
	if (tagnum_edit % 32 == 0) {
		char **tmp = realloc(tag_edit, sizeof(*tag_edit) * (33 + tagnum_edit));
		if (!tmp) oserror();
		tag_edit = tmp;
	}
	tag_edit[tagnum_edit++] = tag;
}

static void tagerror(char *e, ...) {
	va_list ap;
	va_start(ap, e);
	fprintf(stderr, "%s: タグ入力%zu番目: ", program_name, tagnum_edit + 1);
	vfprintf(stderr, e, ap);
	fputc('\n', stderr);
	exit(1);
}

void validate_tag(wchar_t *tag) {
	static wchar_t const *accept = L" !\"#$%&\'()*+,-./01234565789:;<>\?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}";
	static wchar_t const *upper = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	static wchar_t const *lower = L"abcdefghijklmnopqrstuvwxyz";

	if (!wcschr(tag, L'=')) {
		tagerror("フィールドの区切りが存在しない");
	}
	size_t fieldlen = wcsspn(tag, accept);
	if (tag[fieldlen] != L'=') {
		tagerror("フィールド名が不正");
	}
	if (fieldlen == 0) {
		tagerror("空のフィールド名");
	}
	
	wchar_t *p;
	for (p = tag; *p != L'='; p++) {
		wchar_t *q = wcschr(lower, *p);
		if (q) {
			*p = upper[q - lower];
		}
	}
}

static iconv_t cd = (iconv_t)-1;
static void w_add(wchar_t *tag) {
	static char *ls = NULL;
	static size_t lsalloced = 4096;
	if (!tag) {
		free(ls);
		ls = NULL;
		return;
	}
	if (!ls) {
		ls = malloc(lsalloced);
		if (!ls) oserror();
	}
	
	if (wcslen(tag) == wcsspn(tag, L"\t\n\r ")) {
		free(tag);
		return;
	}
	validate_tag(tag);
	
	size_t lslen = wcstombs(NULL, tag, 0);
// fgetws()が全て成功しているので失敗しない
// 	if (lslen == (size_t)-1) {
// 		perror(NULL);
// 		exit(1);
// 	}
	if (lslen + 1 > lsalloced) {
		char *tmp = realloc(ls, lslen + 4096);
		ls = tmp;
		lsalloced = lslen + 4096;
	}
	wcstombs(ls, tag, lslen + 1);
	
	if (cd == (iconv_t)-1) {
#if defined __GLIBC__ || defined _LIBICONV_VERSION
		cd = iconv_open("UTF-8//TRANSLIT", nl_langinfo(CODESET));
#else
		cd = iconv_open("UTF-8", nl_langinfo(CODESET));
#endif
		if (cd == (iconv_t)-1) {
			oserror_fmt("iconvが%s→UTF-8の変換に対応していない", nl_langinfo(CODESET));
		}
	}
	
	char *u8buf, *u8p, *lsp;
	size_t u8left, lsleft, rtn;
	u8left = wcslen(tag) * 6 + 1;
	u8buf = malloc(u8left);
	u8p = u8buf;
	lsp = ls;
	lsleft = lslen + 1;
	rtn = iconv(cd, &lsp, &lsleft, &u8p, &u8left);
	
	add_tag(u8buf);
}

static size_t fgetws2(wchar_t *buf, size_t len, FILE *fp) {
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

static void w_main(void) {
	size_t tagbuflen = 65536;
	wchar_t *tag, *tp;
	wint_t c;
	
	tag = malloc(tagbuflen * sizeof(*tag));
	tp = tag;
	size_t left = tagbuflen, readlen;
	while ((readlen = fgetws2(tp, left, stdin)) != (size_t)-1) {
		if (readlen != wcslen(tp)) {
			tagerror("バイナリファイル");
		}
		tp += readlen;
		left -= readlen;
		wchar_t c = fgetwc(stdin);
		bool cont = false;
		switch (c) {
		case WEOF:
			if (ferror(stdin)) oserror();
			break;
		case L'\0':
			tagerror("バイナリファイル");
			break;
			
		case L'\t':
			cont = true;
			if (tp[-1] != L'\n') {
				ungetwc(c, stdin);
			}
			break;
			
		default:
			cont = tp[-1] != L'\n';
			ungetwc(c, stdin);
			break;
		}
		if (!cont) {
			if (tp[-1] == L'\n') {
				*(--tp) = L'\0';
				if (tp > tag && tp[-1] == L'\r') {
					tp[-1] = L'\0';
				}
			}
			w_add(tag);
			tp = tag;
			left = tagbuflen;
		}
		else {
			if (tp - tag >= 2 && tp[-1] == L'\n' && tp[-2] == L'\r') {
				tp--;
				tp[-1] = L'\n'; tp[0] = L'\0';
				left++;
			}
			if (left < 20) {
				tagbuflen += 65536;
				wchar_t *tmp = realloc(tag, tagbuflen * sizeof(*tag));
				if (!tmp) oserror();
				tp = tmp + (tp - tag);
				tag = tmp;
				left += 65536;
			}
		}
	}
	if (ferror(stdin)) {
		oserror();
	}
	if (tp != tag) {
		w_add(tag);
	}
	free(tag);
	w_add(NULL);
}

static wchar_t *w_unesc(wchar_t *str) {
	wchar_t *wp = wcschr(str, L'\\');
	if (wp) {
		wchar_t *ep = wp;
		while (*ep) {
			if (*ep == L'\\') {
				switch (*(++ep)) {
				case L'0':
					*ep = L'\0';
					break;
					
				case L'n':
					*ep = '\n';
					break;
					
				case L'r':
					*ep = '\r';
					break;
					
				case L'\\':
					*ep = '\\';
					break;
					
				default:
					tagerror("不正なエスケープシーケンス");
					break;
				}
			}
			*wp++ = *ep++;
		}
		*wp = L'\0';
	}
	return str;
}
static void w_main_e(void) {
	size_t tagbuflen = 65536;
	wchar_t *tag, *tp;
	
	tag = malloc(tagbuflen * sizeof(*tag));
	tp = tag;
	size_t left = tagbuflen, readlen;
	bool cont = false;
	while ((readlen = fgetws2(tp, left, stdin)) != (size_t)-1) {
		if (readlen != wcslen(tp)) {
			tagerror("バイナリファイル");
		}
		
		if (tp[readlen - 1] == L'\n') {
			cont = false;
			tp += readlen - 1;
			*tp = L'\0';
			if (tp > tag && tp[-1] == L'\r') {
				tp[-1] = L'\0';
			}
			w_add(w_unesc(tag));
			tp = tag;
		}
		else {
			cont = true;
			tagbuflen += 65536;
			wchar_t *tmp = realloc(tag, tagbuflen * sizeof(*tag));
			tp = tmp + ((tp + readlen) - tag);
			left = left - readlen + 65536;
			tag = tmp;
		}
	}
	if (ferror(stdin)) {
		oserror();
	}
	if (cont) {
		w_add(w_unesc(tag));
	}
	free(tag);
	w_add(NULL);
}

static void invalid_utf8(void) {
	tagerror("不正なUTF-8シーケンス");
}

static void r_add(uint8_t *tag) {
	size_t taglen = strlen(tag);
	if (taglen == strspn(tag, "\x09\x0a\x0d\x20")) {
		free(tag);
		return;
	}
	if (tag[taglen - 1] == 0x0a) {
		tag[--taglen] = '\0';
	}
	uint8_t *p, *endp;
	for (p = tag; *p != '\0'; p++) {
		if (*p < 0x20 || *p > 0x7d) {
			tagerror("フィールド名が不正");
		}
		if (*p == 0x3d) {
			if (p == tag) {
				tagerror("空のフィールド名");
			}
			break;
		}
		if (*p >= 0x61 && *p <= 0x7a) {
			*p -= 32;
		}
	}
	if (*p == '\0') {
		tagerror("フィールドの区切りが存在しない");
	}
	endp = tag + taglen;
	while (p < endp) {
		uint32_t u32 = 0;
		size_t left;
		if (*p < 0x80) {
			p++;
			continue;
		}
		else if (*p < 0xc2) {
			invalid_utf8();
		}
		else if (*p < 0xe0) {
			left = 1;
		}
		else if (*p < 0xf0) {
			left = 2;
		}
		else if (*p < 0xf8) {
			left = 3;
		}
		else {
			invalid_utf8();
		}
		
		if (endp - p < left) {
			invalid_utf8();
		}
		
		u32 = *p & (0x3f >> left);
		for (size_t i = 1; i <= left; i++) {
			if (p[i] < 0x80 || p[i] > 0xbf) {
				invalid_utf8();
			}
			u32 <<= 6;
			u32 |= p[i] & 0x3f;
		}
		switch (left) {
			case 1:
				if (u32 < 0x80 || u32 > 0x7ff) {
					invalid_utf8();
				}
				break;
			case 2:
				if (u32 < 0x800 || u32 > 0xffff) {
					invalid_utf8();
				}
				break;
			case 3:
				if (u32 < 0x10000 || u32 > 0x10ffff) {
					invalid_utf8();
				}
				break;
		}
		p += left + 1;
	}
	add_tag(tag);
}

static bool trimcr(char *tag) {
	bool eol = false;
	size_t l = strlen(tag);
	if (l > 1 && tag[l - 1] == 0x0a) {
		eol = true;
		l--;
		if (l > 1 && tag[l - 1] == 0x0d) {
			tag[l - 1] = 0x0a;
			tag[l] = '\0';
		}
	}
	return eol;
}

static void r_line(uint8_t *line, size_t n) {
	static uint8_t *tag = NULL;
	static bool tagcont = false;
	
	if (!line) {
		if (tag) {
			r_add(tag);
		}
		return;
	}
	
	if (!tag) {
		tag = strndup(line, n);
		tagcont = !trimcr(tag);
		return;
	}
	
	if (tagcont) {
		size_t l = strlen(tag);
		char *tmp = realloc(tag, l + n + 1);
		tag = tmp;
		strncat(tag, line, n);
		tagcont = !trimcr(tag);
	}
	else if (*line == 0x09) {
		size_t l = strlen(tag);
		char *tmp = realloc(tag, l + n);
		tag = tmp;
		strncat(tag, ++line, --n);
		if (n) {
			tagcont = !trimcr(tag);
		}
		else {
			tagcont = true;
		}
	}
	else {
		r_add(tag);
		tag = strndup(line, n);
		tagcont = !trimcr(tag);
	}
}

static void r_line_e(uint8_t *line, size_t n) {
	static uint8_t *tag = NULL;
	static bool tagcont = false;
	
	if (!line) {
		if (tag) {
			r_add(tag);
		}
		return;
	}
	
	if (!tag) {
		tag = strndup(line, n);
		tagcont = !trimcr(tag);
		return;
	}
	
	if (tagcont) {
		size_t l = strlen(tag);
		char *tmp = realloc(tag, l + n + 1);
		tag = tmp;
		strncat(tag, line, n);
		tagcont = !trimcr(tag);
	}
	else {
		uint8_t *p, *endp;
		for (p = tag, endp = p + strlen(tag); p != endp; p++) {
			if (*p == 0x5c) {
				uint8_t c;
				switch (p[1]) {
					case 0x30: // '0'
						c = '\0';
						break;
					case 0x5c: // '\'
						c = 0x5c;
						break;
					case 0x6e: // 'n'
						c = 0x0a;
						break;
					case 0x72: // 'r'
						c = 0x0d;
						break;
					default:
						tagerror("不正なエスケープシーケンス");
						break;
				}
				*p = c;
				memmove(p + 1, p + 2, endp - p);
				endp--;
				p++;
			}
		}
		r_add(tag);
		tag = strndup(line, n);
		tagcont = !trimcr(tag);
	}
}

static void r_main(void) {
	uint8_t *tagbuf, *p1, *p2;
	size_t tagbuflen, taglen, readlen;
	void (*line)(uint8_t *, size_t);
	
	tagbuflen = 65536;
	taglen = 0;
	tagbuf = malloc(tagbuflen);
	line = O.tag_escape ? r_line_e : r_line;
	
	while ((readlen = fread(tagbuf, 1, tagbuflen, stdin)) != 0) {
		if (memchr(tagbuf, 0, readlen) != NULL) {
			tagerror("バイナリファイル");
		}
		p1 = tagbuf;
		while ((p2 = memchr(p1, 0x0a, readlen - (p1 - tagbuf))) != NULL) {
			p2++;
			line(p1, p2 - p1);
			p1 = p2;
		}
		size_t left = readlen - (p1 - tagbuf);
		if (left) line(p1, left);
	}
	line(NULL, 0);
	free(tagbuf);
}

void parse_tags(void) {
	if (tag_edit && !O.tag_filename) {
		return;
	}
	if (O.tag_filename) {
		FILE *tmp = freopen(O.tag_filename, "r", stdin);
		if (!tmp) {
			fileerror(O.tag_filename);
		}
	}
	if (O.tag_raw) {
		r_main();
	}
	else {
		if (O.tag_escape) {
			w_main_e();
		}
		else {
			w_main();
		}
		
		if (cd != (iconv_t)-1) {
			iconv_close(cd);
		}
	}
}
