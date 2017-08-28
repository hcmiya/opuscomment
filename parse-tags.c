#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <locale.h>
#include <langinfo.h>
#include <iconv.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

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

static size_t tagnum;
static void tagerror(char *e) {
	errorprefix();
	fprintf(stderr, catgets(catd, 1, 6, "editing input #%zu: "), tagnum + 1);
	fputs(e, stderr);
	fputc('\n', stderr);
	exit(1);
}

static void err_nosep(void) {
	tagerror(catgets(catd, 5, 1, "no field separator"));
}
static void err_name(void) {
	tagerror(catgets(catd, 5, 2, "invalid field name"));
}
static void err_empty(void) {
	tagerror(catgets(catd, 5, 3, "empty field name"));
}
static void err_bin(void) {
	tagerror(catgets(catd, 5, 4, "binary file"));
}
static void err_esc(void) {
	tagerror(catgets(catd, 5, 5, "invalid escape sequence"));
}
static void err_utf8(void) {
	tagerror(catgets(catd, 5, 6, "invalid UTF-8 sequence"));
}
static void err_noterm(void) {
	mainerror(err_main_no_term);
}

static void toutf8(int fdu8) {
	size_t const buflen = STACK_BUF_LEN / 2;
	char ubuf[STACK_BUF_LEN];
	char *lbuf = ubuf + buflen;
	
	iconv_t cd = iconv_new("UTF-8", O.tag_raw ? "UTF-8" : nl_langinfo(CODESET));
	size_t readlen, remain, total;
	remain = 0; total = 0;
	while ((readlen = fread(&lbuf[remain], 1, buflen - remain, stdin)) != 0) {
		total += readlen;
		if (total > TAG_LENGTH_LIMIT__INPUT) {
			mainerror(err_main_long_input);
		}
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
				case E2BIG:
					break;
				}
				memcpy(lbuf, lp, remain);
			}
			write(fdu8, ubuf, up - ubuf);
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
	write(fdu8, ubuf, up - ubuf);
	close(fdu8);
	if (O.edit == EDIT_WRITE && !total && O.tag_deferred) {
		mainerror(err_main_no_input);
	}
}

static FILE *strstore, *strcount;
static size_t editlen;

static bool first_call = true;
static uint32_t recordlen;
static void finalize_record(void) {
	fwrite(&recordlen, 4, 1, strcount);
	tagnum++;
	first_call = true;
}

static size_t wsplen;
static bool on_field, keep_blank;
static bool test_blank(uint8_t *line, size_t n, bool lf) {
	if (first_call) {
		// = で始まってたらすぐエラー(575)
		if (*line == 0x3d) err_nosep();
		first_call = false;
		on_field = true;
		keep_blank = true;
		wsplen = 0;
		recordlen = 0;
	}
	if (keep_blank) {
		size_t i;
		bool blank_with_ctrl = false;
		for (i = 0; i < n; i++) {
			switch (line[i]) {
			case 0x9:
			case 0xa:
			case 0xd:
				blank_with_ctrl = true;
			case 0x20:
				break;
			default:
				keep_blank = false;
			}
			if (!keep_blank) break;
		}
		if (keep_blank) {
			// まだ先頭から空白が続いている時
			if (lf) {
				// 行が終わったが全て空白だったので飛ばして次の行へ
				first_call = true;
			}
			else {
				// 行が続いている
				wsplen += n;
			}
			return true;
		}
		// 空白状態を抜けてフィールド判別状態にある
		if (blank_with_ctrl) {
			//空白がwsp以外を含んでいたらエラー
			err_name();
		}
		editlen += 4 + wsplen;
		if (editlen > TAG_LENGTH_LIMIT__OUTPUT) {
			exceed_output_limit();
		}
		if (wsplen) {
			// 空白と見做していた分を書き込み
			uint8_t buf[STACK_BUF_LEN];
			memset(buf, 0x20, STACK_BUF_LEN);
			recordlen = wsplen;
			while (wsplen) {
				size_t wlen = wsplen > STACK_BUF_LEN ? STACK_BUF_LEN : wsplen;
				fwrite(buf, 1, wlen, strstore);
				wsplen -= wlen;
			}
		}
	}
	return false;
}

static void append_buffer(uint8_t *line, size_t n) {
	editlen += n;
	if (editlen > TAG_LENGTH_LIMIT__OUTPUT) {
		exceed_output_limit();
	}
	recordlen += n;
	fwrite(line, 1, n, strstore);
}

static void line_oc(uint8_t *line, size_t n, bool lf) {
	static bool afterlf = false;
	
	if (!line) {
		if (!first_call) {
			if (O.tag_check_line_term && !afterlf) {
				err_noterm();
			}
			if (!keep_blank) {
				if (on_field) err_nosep();
				finalize_record();
			}
			first_call = true;
			afterlf = false;
		}
		return;
	}
	
	if(test_blank(line, n, lf)) return;
	
	if (lf) n--;
	if (on_field) {
		if(!test_tag_field(line, n, true, &on_field, NULL)) err_name();
		if (on_field && lf) err_name();
	}
	else if (afterlf) {
		afterlf = false;
		if (*line == 0x9) {
			*line = 0xa;
		}
		else {
			// 新行が<tab>で始まってなかったら前の行のレコード確定作業をして再帰
			finalize_record();
			if (lf) n++;
			line_oc(line, n, lf);
			return;
		}
	}
	append_buffer(line, n);
	if (lf) {
		afterlf = true;
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

static void line_vc(uint8_t *line, size_t n, bool lf) {
	static bool escape_pending = false;
	
	if (!line) {
		if (!first_call) {
			if (O.tag_check_line_term) {
				err_noterm();
			}
			if (!keep_blank) {
				if (escape_pending) err_esc();
				if (on_field) err_nosep();
				finalize_record();
			}
			first_call = true;
		}
		return;
	}
	
	if(test_blank(line, n, lf)) return;
	
	if (escape_pending) {
		*line = esctab(*line);
	}
	
	if (lf) n--;
	uint8_t *bs = memchr(line + escape_pending, 0x5c, n - escape_pending);
	escape_pending = false;
	if (bs) {
		uint8_t *unesc = bs, *end = line + n;
		while (bs < end) {
			uint8_t c = *bs++;
			if (c == 0x5c) {
				if (bs == end) {
					if (lf) err_esc();
					escape_pending = true;
					n--;
					break;
				}
				c = esctab(*bs++);
				n--;
			}
			*unesc++ = c;
		}
	}
	if (on_field) {
		if(!test_tag_field(line, n, true, &on_field, NULL)) err_name();
		if (on_field && lf) err_nosep();
	}
	append_buffer(line, n);
	if (lf) {
		finalize_record();
	}
}

void prepare_record(void) {
	strstore = strstore ? strstore : tmpfile();
	strcount = strcount ? strcount : tmpfile();
}

void *split(void *fp_) {
	FILE *fp = fp_;
	prepare_record();
	void (*line)(uint8_t *, size_t, bool) = O.tag_escape ? line_vc : line_oc;
	
	uint8_t tagbuf[STACK_BUF_LEN];
	size_t readlen;
	while ((readlen = fread(tagbuf, 1, STACK_BUF_LEN, fp)) != 0) {
		uint8_t *p1, *p2;
		if (memchr(tagbuf, 0, readlen) != NULL) {
			err_bin();
		}
		p1 = tagbuf;
		while ((p2 = memchr(p1, 0x0a, readlen - (p1 - tagbuf))) != NULL) {
			p2++;
			line(p1, p2 - p1, true);
			p1 = p2;
		}
		size_t left = readlen - (p1 - tagbuf);
		if (left) line(p1, left, false);
	}
	fclose(fp);
	line(NULL, 0, false);
}

void *parse_tags(void* nouse_) {
	from_file = O.tag_filename && strcmp(O.tag_filename, "-") != 0;
	if (from_file) {
		FILE *tmp = freopen(O.tag_filename, "r", stdin);
		if (!tmp) {
			fileerror(O.tag_filename);
		}
	}
	
	// UTF-8化された文字列をチャンク化する処理をスレッド化(化が多い)
	int pfd[2];
	pipe(pfd);
	FILE *fpu8 = fdopen(pfd[0], "r");
	pthread_t split_thread;
	pthread_create(&split_thread, NULL, split, fpu8);
	
	// 本スレッドはstdinをUTF-8化する
	toutf8(pfd[1]);
	pthread_join(split_thread, NULL);
	
	struct edit_st *rtn = calloc(1, sizeof(*rtn));
	rtn->str = strstore;
	rtn->len = strcount;
	rtn->num = tagnum;
	return rtn;
}

void rt_del_args(uint8_t *buf, size_t len, bool term);

static iconv_t optcd = (iconv_t)-1;
void parse_opt_tag(int opt, char const *arg) {
	char *ls = (char*)arg;
	size_t l = strlen(ls);
	
	if (optcd == (iconv_t)-1) {
		optcd = iconv_new("UTF-8", nl_langinfo(CODESET));
	}
	void (*addbuf)(uint8_t *, size_t, bool);
	switch (opt) {
	case 't':
		prepare_record();
		addbuf = line_oc;
		break;
	case 'd':
		addbuf = rt_del_args;
		break;
	}
	char u8buf[STACK_BUF_LEN];
	size_t u8left;
	char *u8;
	while (l) {
		u8left = STACK_BUF_LEN;
		u8 = u8buf;
		size_t ret = iconv(optcd, &ls, &l, &u8, &u8left);
		
		if (ret == (size_t)-1) {
			// 引数処理なのでEINVAL時のバッファ持ち越しは考慮しない
			if (errno != E2BIG) {
				oserror();
			}
		}
		addbuf(u8buf, u8 - u8buf, false);
	}
	u8 = u8buf;
	u8left = STACK_BUF_LEN;
	if (iconv(optcd, NULL, NULL, &u8, &u8left) == (size_t)-1) {
		oserror();
	}
	u8[0] = 0xa;
	addbuf(u8buf, u8 - u8buf + 1, true);
	addbuf(NULL, 0, false);
}

void pticonv_close(void) {
	iconv_close(optcd);
}
