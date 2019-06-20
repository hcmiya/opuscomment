#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <langinfo.h>

#include "opuscomment.h"

static size_t const gbuflen = 1 << 16;
static uint8_t gbuf[1 << 16];

bool get_metadata_header(uint8_t *type, size_t *len) {
	size_t readlen = fread(gbuf, 1, 4, stream_input);
	if (readlen == (size_t)-1) oserror();
	if (readlen != 4) {
		opuserror(err_opus_non_flac);
	}
	
	bool last = 0x80 & gbuf[0];
	*type = 0x7f & gbuf[0];
	gbuf[0] = 0;
	*len = htonl(*(uint32_t*)gbuf);
	gbuf[0] = *type; // 「最後のヘッダ」標識を削除
	return last;
}

static void write_buffer(void const *buf, size_t len, FILE *fp) {
	size_t wlen = fwrite(buf, 1, len, fp);
	if (wlen != len) oserror();
}

void store_tags_flac(struct rettag_st *rst, struct edit_st *est) {
	size_t readlen;
	if (rst) {
		store_tags(NULL, rst, est, false);
		uint32_t left = ftell(rst->tag);
		*(uint32_t*)gbuf = ntohl(left);
		gbuf[0] = 4;
		write_buffer(gbuf, 4, built_stream);
		rewind(rst->tag);
		while (readlen = fread(gbuf, 1, gbuflen, rst->tag)) {
			write_buffer(gbuf, readlen, built_stream);
		}
	}
	rewind(est->pict);
	while (readlen = fread(gbuf, 1, gbuflen, est->pict)) {
		write_buffer(gbuf, readlen, built_stream);
	}
}

static void read_comment(size_t left) {
	// ここから read.c の parse_info_border() と大体一緒
	pthread_t retriever_thread;
	
	// タグヘッダパースを別スレッド化 retrieve_tags.c へ
	int pfd[2];
	pipe(pfd);
	FILE *retriever = fdopen(pfd[0], "r");
	pthread_create(&retriever_thread, NULL, retrieve_tags, retriever);
	
	// 本スレッドはタグヘッダを送り続ける
	while (left) {
		size_t readlen = left > gbuflen ? gbuflen : left;
		size_t readret = fread(gbuf, 1, readlen, stream_input);
		if (readlen != readret) oserror();
		write(pfd[1], gbuf, readlen);
		left -= readlen;
	}
	
	// ここから read.c の parse_comment_term() と似たようなこと
	struct rettag_st *rst;
	struct edit_st *est;
	close(pfd[1]);
	pthread_join(retriever_thread, (void**)&rst);
	if (O.edit != EDIT_LIST) {
		// 編集入力タグパースのスレッドを合流
		pthread_join(parser_thread, (void **)&est);
		store_tags_flac(rst, est);
	}
}

static void *put_base64_locale(void *tagin_) {
	FILE *tagin = tagin_;
	iconv_t cd = iconv_open(nl_langinfo(CODESET), "us-ascii");
	size_t readlen;
	char buf[128];
	while (readlen = fread(buf, 1, 64, tagin)) {
		size_t asciileft = readlen, locleft = 128 - readlen;
		char *ascii = buf, *loc = buf + readlen;
		iconv(cd, &ascii, &asciileft, &loc, &locleft);
		// このiconv()はPCS範囲内でシフトも発生しないはずなのでバッファ持ち越しがない
		// asciiは元のlocの位置に移った
		write_buffer(ascii, loc - ascii, tag_output);
	}
	iconv_close(cd);
	fclose(tagin);
	return NULL;
}

static bool met_comment, met_picture;
static void read_picture_list(size_t left) {
	pthread_t loctr_th;
	FILE *ascii_out;
	if (!O.tag_raw) {
		int pfd[2];
		pipe(pfd);
		ascii_out = fdopen(pfd[1], "w");
		FILE *tagin = fdopen(pfd[0], "r");
		pthread_create(&loctr_th, NULL, put_base64_locale, tagin);
	}
	else {
		ascii_out = tag_output;
	}
	
	if (O.tag_escape == TAG_ESCAPE_NUL && (met_comment || met_picture)) {
		write_buffer("", 1, ascii_out);
	}
	write_buffer(MBPeq, strlen(MBPeq), ascii_out);
	
	while (left) {
		uint8_t raw[3] = {0};
		uint8_t b64[4];
		size_t readlen = left > 3 ? 3 : left;
		size_t readret = fread(raw, 1, readlen, stream_input);
		if (readret != readlen) {
			if (ferror(stream_input)) oserror();
			else opuserror(err_opus_lost_tag);
		}
		// 00000011 11112222 22333333
		b64[0] = raw[0] >> 2;
		b64[1] = (raw[0] << 4 | raw[1] >> 4) & 0x3f;
		b64[2] = (raw[1] << 2 | raw[2] >> 6) & 0x3f;
		b64[3] = raw[2] & 0x3f;
		switch (readlen) {
		case 1:
			b64[2] = 64;
			// FALLTHROUGH
		case 2:
			b64[3] = 64;
			break;
		}
		for (int_fast8_t i = 0; i < 4; i++) {
			b64[i] = b64tab_ascii[b64[i]];
		}
		write_buffer(b64, 4, ascii_out);
		left -= readlen;
	}
	
	if (O.tag_escape != TAG_ESCAPE_NUL) {
		write_buffer((uint8_t[]){ 0xa }, 1, ascii_out);
	}
	
	if (!O.tag_raw) {
		fclose(ascii_out);
		pthread_join(loctr_th, NULL);
	}
	
	met_picture = true;
}

void read_flac(void) {
	size_t readlen = fread(gbuf, 1, 4, stream_input);
	if (readlen == (size_t)-1) oserror();
	if (readlen != 4) {
		opuserror(err_opus_non_flac);
	}
	if (memcmp(gbuf, "\x66\x4C\x61\x43", 4) != 0) { // fLaC
		opuserror(err_opus_non_flac);
	}
	write_buffer(gbuf, 4, built_stream);
	
	bool last_metadata = false;
	size_t metadata_num = 0;
	while (!last_metadata) {
		uint8_t type;
		size_t left;
		last_metadata = get_metadata_header(&type, &left);
		if (type == 0 && metadata_num != 0
			|| type == 0 && left != 34
			|| type == 127
			|| met_comment && type == 4) {
			opuserror(err_opus_bad_content);
		}
		bool delete_header = O.edit == EDIT_LIST;
		if (type == 1) delete_header = true;
		switch (type) {
		case 4:
			if (O.edit == EDIT_LIST && O.tag_escape == TAG_ESCAPE_NUL && met_picture) {
				write_buffer("", 1, tag_output);
			}
			read_comment(left);
			met_comment = true;
			break;
		case 6:
			if (O.edit == EDIT_LIST) {
				if (!O.tag_ignore_picture) {
					read_picture_list(left);
					break;
				}
			}
			else if (O.edit == EDIT_WRITE) {
				delete_header = !O.tag_ignore_picture;
			}
			// FALLTHROUGH
		default:
			if (!delete_header) write_buffer(gbuf, 4, built_stream);
			while (left) {
				size_t readlen = left > gbuflen ? gbuflen : left;
				size_t readret = fread(gbuf, 1, readlen, stream_input);
				if (readlen != readret) oserror();
				if (!delete_header) write_buffer(gbuf, readlen, built_stream);
				left -= readlen;
			}
			break;
		}
		metadata_num++;
	}
	if (O.edit == EDIT_LIST) {
		tag_output_close();
		exit(0);
	}
	if (!met_comment) {
		struct edit_st *est;
		pthread_join(parser_thread, (void **)&est);
		struct rettag_st *rst = NULL;
		if (est->num) {
			rst = calloc(1, sizeof(*rst));
			rst->tag = tmpfile();
			rst->part_of_comment = true;
			size_t vendorlen = strlen(new_vendor_string_ascii);
			fwrite((uint32_t[]){oi32(vendorlen)}, 4, 1, rst->tag);
			fwrite(new_vendor_string_ascii, 1, vendorlen, rst->tag);
			rst->tagbegin = vendorlen + 4;
			fwrite((uint32_t[]){0}, 4, 1, rst->tag);
		}
		store_tags_flac(rst, est);
	}
	write_buffer("\x81\0\0", 4, built_stream); // 「最後のヘッダ」標識を立てたパディングで〆
	
	opst = PAGE_SOUND;
	while (readlen = fread(gbuf, 1, gbuflen, stream_input)) {
		size_t writelen = fwrite(gbuf, 1, readlen, built_stream);
		if (writelen != readlen) oserror();
	}
	if (ferror(stream_input)) oserror();
}
