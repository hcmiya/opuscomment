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

static void puterror(void) {
	if (tag_output_to_file) {
		fileerror(O.tag_filename);
	}
	else {
		oserror();
	}
}

static int nth = 1;
static void u8error(void) {
	opuserror(err_opus_utf8, nth);
}

static void put_bin(uint8_t const *buf, size_t len) {
	size_t ret = fwrite(buf, 1, len, tag_output);
	if (ret != len) {
		puterror();
	}
}
static void put_ls(char const *ls) {
	put_bin(ls, strlen(ls));
}

static uint8_t *esc_oc(uint8_t *src, uint8_t *dest, size_t len) {
	uint8_t *end = src + len;
	while(src < end) {
		*dest++ = *src++;
		if (src[-1] == 0xa) *dest++ = O.tag_escape_tilde ? 0x7e : 0x9;
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

static uint8_t *esc_nul(uint8_t *src, uint8_t *dest, size_t len) {
	return memcpy(dest, src, len) + len;
}

typedef uint8_t* (*esc_func_)(uint8_t*, uint8_t*, size_t);
static esc_func_ esc_func[] = {
	esc_oc, esc_vc, esc_nul
};

static size_t conv_none(iconv_t cd, uint8_t *buf, size_t tagleft) {
	put_bin(buf, tagleft);
	return 0;
}

static size_t conv_ls(iconv_t cd, uint8_t *buf, size_t tagleft) {
	uint8_t *u8 = buf;
	char *ls = buf + tagleft;
	size_t remain = 0;
	for (;;) {
		char *lsend = ls;
		size_t bufleft = STACK_BUF_LEN - ((uint8_t*)ls - buf) - 1;
		size_t iconvret = iconv(cd, (char**)&u8, &tagleft, &lsend, &bufleft);
		int ie = errno;
		errno = 0;
		if (iconvret == (size_t)-1 && ie == EILSEQ) {
			u8error();
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
	return remain;
}

void tag_output_close(void) {
	if (O.tag_deferred) {
		FILE *deferred = tag_output;
		tag_output = stdout;
		rewind(deferred);
		size_t readlen;
		uint8_t buf[STACK_BUF_LEN];
		while(readlen = fread(buf, 1, STACK_BUF_LEN, deferred)) {
			put_bin(buf, readlen);
		}
		fclose(deferred);
	}
	
	if (fclose(stdout) == EOF) {
		puterror();
	}
}

void *put_tags(void *fp_) {
	// retrieve_tag() からスレッド化された
	// fpはチャンク化されたタグ
	// [4バイト: タグ長(ホストエンディアン)][任意長: UTF-8タグ]
	FILE *fp = fp_;
	
	iconv_t cd;
	
	if (!O.tag_raw) {
		cd = iconv_new(nl_langinfo(CODESET), "UTF-8");
	}
	bool nulsep = O.tag_escape == TAG_ESCAPE_NUL;
	size_t const buflenunit = STACK_BUF_LEN / (3 - nulsep);
	uint8_t buf[STACK_BUF_LEN];
	uint8_t *raw = buf + buflenunit * (2 - nulsep);
	uint8_t *(*esc)(uint8_t*, uint8_t*, size_t) = esc_func[O.tag_escape];
	size_t (*conv)(iconv_t cd, uint8_t*, size_t) = O.tag_raw ? conv_none : conv_ls;
	
	for (;;) {
		uint32_t len;
		if (!fread(&len, 4, 1, fp)) break;
		if (nulsep && nth > 1) put_bin("", 1);
		size_t left = len, remain = 0;
		while (left) {
			size_t readlen = fill_buffer(raw, left, buflenunit - remain, fp);
			left -= readlen;
			
			size_t tagleft = esc(raw, buf + remain, readlen) - buf;
			remain = conv(cd, buf, tagleft);
		}
		if (remain) {
			u8error();
		}
		if (O.tag_raw) {
			if (!nulsep) {
				put_bin("\x0a", 1);
			}
		}
		else {
			uint8_t *u8;
			char *ls;
			if (nulsep) {
				*buf = '\0';
				left = 1, remain = 128;
			}
			else {
				memcpy(buf, "\x0a", 2);
				left = 2, remain = 128;
			}
			u8 = buf, ls = buf + left;
			if (iconv(cd, (char**)&u8, &left, &ls, &remain) == (size_t)-1) {
				oserror();
			}
			// u8はここでiconv()前のlsを指していることに注意
			put_ls(u8);
		}
		nth++;
	}
	fclose(fp);
	
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
