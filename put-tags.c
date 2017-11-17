#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <locale.h>
#include <langinfo.h>
#include <iconv.h>
#include <errno.h>

#include "opuscomment.h"
#include "global.h"

static bool to_file;
static void puterror(void) {
	if (to_file) {
		fileerror(O.tag_filename);
	}
	else {
		oserror();
	}
}
static void u8error(int nth) {
	opuserror(err_opus_utf8, nth);
}

static FILE *putdest;
static void put_bin(char const *buf, size_t len) {
	size_t ret = fwrite(buf, 1, len, putdest);
	if (ret != len) {
		puterror();
	}
}
static void put_ls(char const *ls) {
	if (fputs(ls, putdest) == EOF) {
		puterror();
	}
}

static uint8_t *esc_oc(uint8_t *src, uint8_t *dest, size_t len) {
	uint8_t *end = src + len;
	while(src < end) {
		*dest++ = *src++;
		if (src[-1] == 0xa) *dest++ = 0x9;
	}
	return dest;
}
static uint8_t *esc_vc(uint8_t *src, uint8_t *dest, size_t len) {
	uint8_t *end = src + len;
	for (; src < end; src++) {
		switch (*src) {
		case 0x00:
			*dest++ = 0x5c;
			*dest++ = 0x30;
			break;
			
		case 0x0a:
			*dest++ = 0x5c;
			*dest++ = 0x6e;
			break;
		case 0x0d:
			*dest++ = 0x5c;
			*dest++ = 0x72;
			break;
			
		case 0x5c:
			*dest++ = 0x5c;
			*dest++ = 0x5c;
			break;
			
		default:
			*dest++ = *src;
			break;
		}
	}
	return dest;
}
void *put_tags(void *fp_) {
	// retrieve_tag() からスレッド化された
	// fpはチャンク化されたタグ
	// [4バイト: タグ長(ホストエンディアン)][任意長: UTF-8タグ]
	FILE *fp = fp_;
	bool to_file = O.tag_filename && strcmp(O.tag_filename, "-") != 0;
	if (to_file) {
		FILE *tmp = freopen(O.tag_filename, "w", stdout);
		if (!tmp) {
			fileerror(O.tag_filename);
		}
	}
	
	putdest = O.tag_deferred ? tmpfile() : stdout;
	
	iconv_t cd;
	char charsetname[128];
	
	if (!O.tag_raw) {
		cd = iconv_new(nl_langinfo(CODESET), "UTF-8");
	}
	size_t buflenunit = (1 << 13);
	size_t buflen = buflenunit * 3;
	uint8_t *buf = malloc(buflen);
	int nth = 1;
	uint8_t* (*esc)(uint8_t*, uint8_t*, size_t) = O.tag_escape ? esc_vc : esc_oc;
	uint8_t *raw = buf + buflenunit * 2;
	
	for (;;) {
		uint8_t *u8;
		char *ls;
		uint32_t len;
		if (!fread(&len, 4, 1, fp)) break;
		size_t left = len, remain = 0;
		while (left) {
			size_t readlen = fill_buffer(raw, left, buflenunit - remain, fp);
			left -= readlen;
			
			size_t tagleft = esc(raw, buf + remain, readlen) - buf;
			
			if (O.tag_raw) {
				put_bin(buf, tagleft);
			}
			else {
				u8 = buf;
				ls = buf + tagleft;
				for (;;) {
					char *lsend = ls;
					size_t bufleft = buflen - ((uint8_t*)ls - buf) - 1;
					size_t iconvret = iconv(cd, (char**)&u8, &tagleft, &lsend, &bufleft);
					int ie = errno;
					errno = 0;
					if (iconvret == (size_t)-1 && ie == EILSEQ) {
						u8error(nth);
					}
					*lsend = '\0';
					// WONTFIX
					// ロケール文字列に \0 が含まれていてもそのままそこで途切れさせる
					put_ls(ls);
					if (iconvret != (size_t)-1 || ie == EINVAL) {
						remain = tagleft;
						if (remain) {
							memcpy(buf, u8, remain);
						}
						break;
					}
				}
			}
		}
		if (remain) {
			u8error(nth);
		}
		if (O.tag_raw) {
			put_bin("\x0a", 1);
		}
		else {
			strcpy(buf, "\x0a");
			left = 2, remain = 200;
			u8 = buf, ls = buf + 2;
			if (iconv(cd, (char**)&u8, &left, &ls, &remain) == (size_t)-1) {
				oserror();
			}
			put_ls(u8);
		}
		nth++;
	}
	fclose(fp);
	
	if (O.tag_deferred) {
		FILE *deferred = putdest;
		putdest = stdout;
		rewind(deferred);
		while(fgets(buf, buflen, deferred)) {
			put_ls(buf);
		}
		fclose(deferred);
	}
	
	if (fclose(stdout) == EOF) {
		puterror();
	}
	return NULL;
}

iconv_t iconv_new(char const *to, char const *from) {
#if defined __GLIBC__ || defined _LIBICONV_VERSION
	char to2[64];
	strcat(strcpy(to2, to), "//TRANSLIT");
#else
	char const *to2 = to;
#endif
	iconv_t cd = iconv_open(to2, from);
	if (cd == (iconv_t)-1) {
		if (errno == EINVAL) {
			oserror_fmt(catgets(catd, 4, 3, "iconv doesn't support converting %s -> %s"), from, to);
		}
		else {
			oserror();
		}
	}
	return cd;
}
