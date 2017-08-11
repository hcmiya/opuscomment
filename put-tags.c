#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <locale.h>
#include <langinfo.h>
#include <iconv.h>
#include <wchar.h>
#include <errno.h>

#include "opuscomment.h"
#include "global.h"

static void put_bin(char const *buf, size_t len) {
	size_t ret = fwrite(buf, 1, len, stdout);
	if (ret != len) {
		oserror();
	}
}

static void put_tags_r(void) {
	char **cp;
	for (cp = tag_file; *cp; cp++) {
		char *n1, *n2;
		n1 = *cp;
		while ((n2 = strchr(n1, 0x0a))) {
			n2++;
			put_bin(n1, n2 - n1);
			put_bin("\x09", 1);
			n1 = n2;
		}
		n2 = strchr(n1, '\0');
		*n2++ = 0x0a;
		put_bin(n1, n2 - n1);
	}
}

static void put_tags_re(void) {
	char **cp;
	char esc[2] = {0x5c};
	for (cp = tag_file; *cp; cp++) {
		char *n1;
		n1 = *cp;
		while (*n1) {
			size_t noesc = strcspn(n1, "\x0a\x0d\x5c");
			put_bin(n1, noesc);
			n1 += noesc;
			switch (*n1) {
				case 0x0a:
					esc[1] = 0x6e;
					break;
				case 0x0d:
					esc[1] = 0x72;
					break;
				case 0x5c:
					esc[1] = 0x5c;
					break;
			}
			switch (*n1) {
				case 0x0a:
				case 0x0d:
				case 0x5c:
					put_bin(esc, 2);
					n1++;
					break;
			}
		}
		put_bin("\x0a", 1);
	}
}

static void put_tags_w(void) {
	char **cp;
	iconv_t cd;
	char charsetname[128];
	
	strcpy(charsetname, nl_langinfo(CODESET));
#if defined __GLIBC__ || defined _LIBICONV_VERSION
	strcat(charsetname, "//TRANSLIT");
#endif
	cd = iconv_open(charsetname, "UTF-8");
	if (cd == (iconv_t)-1) {
		oserror_fmt("iconvがUTF-8→%sの変換に対応していない", nl_langinfo(CODESET));
	}
	size_t buflen = 1 << 18;
	uint8_t *buf = malloc(buflen);
	
	for (cp = tag_file; *cp; cp++) {
		char *n1, *n2, *u8, *ls;
		size_t taglen = strlen(*cp), bufleft, tagleft;
		if (taglen * 16 > buflen) {
			uint8_t *tmp = realloc(buf, taglen * 16);
			buf = tmp;
			buflen = taglen * 16;
		}
		if (O.tag_escape) {
			for (n1 = *cp, n2 = buf; *n1; n1++) {
				switch (*n1) {
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
			*n2++ = '\0';
			u8 = buf;
			tagleft = strlen(u8) + 1;
			ls = n2;
			bufleft = buflen - tagleft;
		}
		else {
			n1 = *cp;
			u8 = buf;
			while ((n2 = strchr(n1, 0x0a))) {
				n2++;
				strncpy(u8, n1, n2 - n1);
				u8 += n2 - n1;
				n1 = n2;
				*u8++ = 0x09;
			}
			strcpy(u8, n1);
			u8 = buf;
			tagleft = strlen(u8) + 1;
			ls = n2 = u8 + tagleft;
			bufleft = buflen - tagleft;
		}
		
		iconv(cd, &u8, &tagleft, &n2, &bufleft);
		if (tagleft == 0) {
			if (puts(ls) == EOF) {
				oserror();
			}
		}
		else {
			iconv(cd, NULL, NULL, NULL, NULL);
			mainerror("%d個目のタグのUTF-8シーケンスが不正", cp - tag_file + 1);
		}
	}
}

void put_tags(void) {
	if (O.tag_filename) {
		FILE *tmp = freopen(O.tag_filename, "w", stdout);
		if (!tmp) {
			fileerror(O.tag_filename);
		}
	}
	if (O.tag_raw) {
		if (O.tag_escape) {
			put_tags_re();
		}
		else {
			put_tags_r();
		}
	}
	else {
		put_tags_w();
	}
	exit(0);
}
