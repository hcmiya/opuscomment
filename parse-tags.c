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
#include <unistd.h>

#include "opuscomment.h"
#include "global.h"

static bool from_file;
static void readerror(void) {
	if (from_file) {
		fileerror(O.tag_filename);
	}
	else {
		oserror();
	}
}

static void tagerror(char *e) {
	fprintf(stderr, catgets(catd, 1, 6, "%s: タグ入力%zu番目: "), program_name, tagnum_edit + 1);
	fputs(e, stderr);
	fputc('\n', stderr);
	exit(1);
}

static void err_nosep(void) {
	tagerror(catgets(catd, 5, 1, "項目名と値の区切りが存在しない"));
}
static void err_name(void) {
	tagerror(catgets(catd, 5, 2, "項目名が不正"));
}
static void err_empty(void) {
	tagerror(catgets(catd, 5, 3, "空の項目名"));
}
static void err_bin(void) {
	tagerror(catgets(catd, 5, 4, "バイナリファイル"));
}
static void err_esc(void) {
	tagerror(catgets(catd, 5, 5, "不正なエスケープシーケンス"));
}
static void err_utf8(void) {
	tagerror(catgets(catd, 5, 6, "不正なUTF-8シーケンス"));
}

void check_tagpacket_length(void) {
	if (tagpacket_total > TAG_LENGTH_LIMIT__OUTPUT) {
		mainerror(catgets(catd, 1, 10, "保存出来るタグの長さが上限を超えた(%uMiBまで)"), TAG_LENGTH_LIMIT__OUTPUT >> 20);
	}
}

static FILE *fpu8;
static FILE *record, *splitted;
static int recordfd;
static void toutf8(void) {
	fpu8 = tmpfile();
	size_t const buflen = 512;
	char ubuf[buflen];
	char lbuf[buflen];
	
	iconv_t cd;
	char *fromcode = O.tag_raw ? "UTF-8" : nl_langinfo(CODESET);
#if defined __GLIBC__ || defined _LIBICONV_VERSION
	cd = iconv_open("UTF-8//TRANSLIT", fromcode);
#else
	cd = iconv_open("UTF-8", fromcode);
#endif
	if (cd == (iconv_t)-1) {
		if (errno == EINVAL) {
			oserror_fmt(catgets(catd, 4, 1, "iconvが%s→UTF-8の変換に対応していない"), fromcode);
		}
		else {
			oserror();
		}
	}
	size_t readlen, remain, total;
	remain = 0;
	while ((readlen = fread(&lbuf[remain], 1, buflen - remain, stdin)) != 0) {
		if (strnlen(&lbuf[remain], readlen) != readlen) {
			err_bin();
		}
		size_t llen = readlen + remain;
		remain = llen;
		for (;;) {
			size_t ulen = buflen;
			char *lp = lbuf, *up = ubuf;
			size_t iconvret = iconv(cd, &lp, &remain, &up, &ulen);
			int ie = errno;
			if (iconvret == (size_t)-1) {
				switch (ie) {
				case EILSEQ:
					oserror();
					break;
				case EINVAL:
					break;
				case E2BIG:
					break;
				}
				memmove(lbuf, lp, remain);
			}
			fwrite(ubuf, 1, up - ubuf, fpu8);
			if (iconvret != (size_t)-1 || ie == EINVAL) break;
		}
	}
	if (fclose(stdin) == EOF) {
		readerror();
	}
	if (remain) {
		errno = EILSEQ;
		readerror();
	}
	char *up = ubuf; readlen = buflen;
	remain = iconv(cd, NULL, NULL, &up, &readlen);
	if (remain == (size_t)-1) oserror();
	iconv_close(cd);
	fwrite(ubuf, 1, up - ubuf, fpu8);
	rewind(fpu8);
}

static void blank_record() {
	rewind(record);
	ftruncate(recordfd, 0);
}
static void write_record(void) {
	uint32_t end = ftell(record);
	rewind(record);
	uint8_t buf[512];
	size_t n;
	
	// 全部空白かどうか
	bool blank = true;
	while ((n = fread(buf, 1, 512, record))) {
		for (size_t i = 0; i < n; i++) {
			if (!strchr("\x9\xa\xd\x20", buf[i])) {
				blank = false;
				goto END_BLANK_TEST;
			}
		}
	}
END_BLANK_TEST:
	if (blank) {
		blank_record();
		return;
	}
	// 空白じゃなかったら編集に採用
	tagpacket_total += 4 + end;
	check_tagpacket_length();
	end = oi32(end);
	fwrite(&end, 4, 1, fpedit);
	rewind(record);
	
	// 最初が = か
	fread(buf, 1, 1, record);
	if (*buf == 0x3d) err_empty();
	rewind(record);
	
	// 項目名がPCS印字文字範囲内か
	while ((n = fread(buf, 1, 512, record))) {
		size_t i;
		for (i = 0; i < n && buf[i] != 0x3d; i++) {
			if (!(buf[i] >= 0x20 && buf[i] <= 0x7e)) {
				err_name();
			}
			if (buf[i] >= 0x61 && buf[i] <= 0x7a) {
				buf[i] -= 32;
			}
		}
		fwrite(buf, 1, n, fpedit);
		if (i < n) break;
	}
	while ((n = fread(buf, 1, 512, record))) {
		fwrite(buf, 1, n, fpedit);
	}
	blank_record();
	tagnum_edit++;
}

static void r_line(uint8_t *line, size_t n) {
	static bool afterlf = false;
	
	if (!line) {
		write_record();
		afterlf = false;
		return;
	}
	
	bool lf = line[n - 1] == 0xa;
	if (afterlf) {
		if (*line == 0x9) {
			*line = 0xa;
		}
		else {
			write_record();
		}
		afterlf = false;
	}
	fwrite(line, 1, n - lf, record);
	if (lf) {
		afterlf = true;
	}
	else {
		afterlf = false;
	}
}

static int esctab(int c) {
	switch (c) {
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
		err_esc();
		break;
	}
	return c;
}
static void r_line_e(uint8_t *line, size_t n) {
	static bool tagcont = false;
	static bool escape = false;
	
	if (!line) {
		if (escape) err_esc();
		if (tagcont) write_record();
		return;
	}
	
	if (escape) {
		*line = esctab(*line);
		fwrite(line, 1, 1, record);
		escape = false;
		line++;
		n--;
	}
	
	uint8_t *p = line;
	bool lf = n && line[n - 1] == 0x0a;
	uint8_t *endp = line + n - lf;
	for (; p < endp && *p != 0x5c; p++) {}
	fwrite(line, 1, p - line, record);
	if (p < endp) {
		// '\\'があった時
		uint8_t *escbegin = p;
		uint8_t *advp = p;
		for (; advp < endp; p++, advp++) {
			char c;
			if ((c = *advp) == 0x5c) {
				if (++advp == endp) {
					escape = true;
					tagcont = true;
					break;
				}
				c = esctab(*advp);
			}
			*p = c;
		}
		fwrite(escbegin, 1, p - escbegin, record);
	}
	if (lf) {
		write_record();
	}
	tagcont = !lf;
}

void prepare_record(void) {
	if (record) {
		return;
	}
	record = tmpfile();
	recordfd = fileno(record);
}

static void split(void) {
	uint8_t tagbuf[512], *p1, *p2;
	size_t tagbuflen, readlen;
	void (*line)(uint8_t *, size_t);
	
	tagbuflen = 512;
	line = O.tag_escape ? r_line_e : r_line;
	prepare_record();
	
	while ((readlen = fread(tagbuf, 1, tagbuflen, fpu8)) != 0) {
		if (memchr(tagbuf, 0, readlen) != NULL) {
			err_bin();
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
	fclose(record);
}

void parse_tags(void) {
	from_file = O.tag_filename && strcmp(O.tag_filename, "-") != 0;
	if (from_file) {
		FILE *tmp = freopen(O.tag_filename, "r", stdin);
		if (!tmp) {
			fileerror(O.tag_filename);
		}
	}
	toutf8();
	split();
}

void add_tag_from_opt(char const *arg) {
	static iconv_t cd = (iconv_t)-1;
	
	if (!arg) {
		if (cd == (iconv_t)-1) return;
		iconv_close(cd);
		cd = (iconv_t)-1;
		return;
	}
	
	char *ls = (char*)arg;
	size_t l = strlen(ls);
	
	if (cd == (iconv_t)-1) {
#if defined __GLIBC__ || defined _LIBICONV_VERSION
		cd = iconv_open("UTF-8//TRANSLIT", nl_langinfo(CODESET));
#else
		cd = iconv_open("UTF-8", nl_langinfo(CODESET));
#endif
		if (cd == (iconv_t)-1) {
			oserror_fmt(catgets(catd, 4, 1, "iconvが%s→UTF-8の変換に対応していない"), nl_langinfo(CODESET));
		}
	}
	fpedit = fpedit ? fpedit : tmpfile();
	prepare_record();
	char u8buf[512];
	size_t u8left;
	char *u8;
	while (l) {
		u8left = 512;
		u8 = u8buf;
		size_t ret = iconv(cd, &ls, &l, &u8, &u8left);
		
		if (ret == (size_t)-1) {
			if (errno != E2BIG) {
				oserror();
			}
		}
		r_line(u8buf, u8 - u8buf);
	}
	u8 = u8buf;
	u8left = 512;
	if (iconv(cd, NULL, NULL, &u8, &u8left) == (size_t)-1) {
		oserror();
	}
	u8[0] = 0xa;
	r_line(u8buf, u8 - u8buf + 1);
	r_line(NULL, 0);
}
