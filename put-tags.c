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
	opuserror(catgets(catd, 3, 8, "%d個目のタグのUTF-8シーケンスが不正"), nth);
}

static void put_bin(char const *buf, size_t len) {
	size_t ret = fwrite(buf, 1, len, stdout);
	if (ret != len) {
		puterror();
	}
}
static char *esc_oc(char *n1, char *end) {
	char *n2 = end;
	while(n1 < end) {
		*n2++ = *n1++;
		if (n1[-1] == 0xa) *n2++ = 0x9;
	}
	return n2;
}
static char *esc_vc(char *n1, char *end) {
	char *n2 = end;
	for (; n1 < end; n1++) {
		switch (*n1) {
		case 0x00:
			*n2++ = 0x5c;
			*n2++ = 0x30;
			break;
			
		case 0x0a:
			*n2++ = 0x5c;
			*n2++ = 0x6e;
			break;
		case 0x0d:
			*n2++ = 0x5c;
			*n2++ = 0x72;
			break;
			
		case 0x5c:
			*n2++ = 0x5c;
			*n2++ = 0x5c;
			break;
			
		default:
			*n2++ = *n1;
			break;
		}
	}
	return n2;
}
void put_tags(void) {
	bool to_file = O.tag_filename && strcmp(O.tag_filename, "-") != 0;
	if (to_file) {
		FILE *tmp = freopen(O.tag_filename, "w", stdout);
		if (!tmp) {
			fileerror(O.tag_filename);
		}
	}
	
	iconv_t cd;
	char charsetname[128];
	
	if (!O.tag_raw) {
		strcpy(charsetname, nl_langinfo(CODESET));
#if defined __GLIBC__ || defined _LIBICONV_VERSION
		strcat(charsetname, "//TRANSLIT");
#endif
		cd = iconv_open(charsetname, "UTF-8");
		if (cd == (iconv_t)-1) {
			if (errno == EINVAL) {
				oserror_fmt(catgets(catd, 4, 2, "iconvがUTF-8→%sの変換に対応していない"), nl_langinfo(CODESET));
			}
			else oserror();
		}
	}
	size_t buflenunit = (1 << 13);
	size_t buflen = buflenunit * 3;
	uint8_t *buf = malloc(buflen);
	int nth = 1;
	char* (*esc)(char*, char*) = O.tag_escape ? esc_vc : esc_oc;
	
	rewind(fpedit);
	for (;;) {
		char *u8, *ls;
		uint32_t len;
		if (!fread(&len, 4, 1, fpedit)) break;
		len = oi32(len);
		size_t left = len, remain = 0;
		while (left) {
			size_t readmax = buflenunit - remain;
			size_t readlen = left > readmax ? readmax : left;
			fread(buf + remain, 1, readlen, fpedit);
			left -= readlen;
			
			char *escbegin = buf + readlen + remain;
			char *escend = esc(buf, escbegin);
			size_t tagleft = escend - escbegin;
			memmove(buf, escbegin, tagleft);
			
			if (O.tag_raw) {
				put_bin(buf, tagleft);
			}
			else {
				u8 = buf;
				ls = buf + tagleft;
				for (;;) {
					char *lsend = ls;
					size_t bufleft = buflen - (ls - (char*)buf) - 1;
					size_t iconvret = iconv(cd, &u8, &tagleft, &lsend, &bufleft);
					int ie = errno;
					errno = 0;
					if (iconvret == (size_t)-1 && ie == EILSEQ) {
						u8error(nth);
					}
					*lsend = '\0';
					if (fputs(ls, stdout) == EOF) {
						puterror();
					}
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
			if (iconv(cd, &u8, &left, &ls, &remain) == (size_t)-1) {
				oserror();
			}
			if (fputs(u8, stdout) == EOF) {
				puterror();
			}
		}
		nth++;
	}
	
	if (fclose(stdout) == EOF) {
		puterror();
	}
	exit(0);
}
