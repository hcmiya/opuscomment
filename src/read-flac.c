#include <ogg/ogg.h>
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
#include <iconv.h>

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

void store_tags(ogg_page *np, struct rettag_st *rst, struct edit_st *est, bool packet_break_in_page);

static void read_comment(size_t left) {
	// ここから read.c の parse_info_border() と大体一緒
	// スレッド間通信で使っているパイプがエラー後のexit内での始末にSIGPIPEを発するのでその対策
	atexit(exit_without_sigpipe);
	error_on_thread = true;
	
	pthread_t retriever_thread, parser_thread;
	if (O.edit != EDIT_LIST) {
		// 編集入力タグパースを別スレッド化 parse_tags.c へ
		pthread_create(&parser_thread, NULL, parse_tags, NULL);
	}
	
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
		store_tags(NULL, rst, est, false);
		uint32_t left = ftell(rst->tag);
		*(uint32_t*)gbuf = ntohl(left);
		gbuf[0] = 4;
		write_buffer(gbuf, 4, built_stream);
		rewind(rst->tag);
		size_t readlen;
		while (readlen = fread(gbuf, 1, gbuflen, rst->tag)) {
			write_buffer(gbuf, readlen, built_stream);
		}
	}
	error_on_thread = false;
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

static uint8_t const * const b64tab_ascii =
	"\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f" // A-O
	"\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5a" // P-Z
	"\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f" // a-o
	"\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7a" // p-z
	"\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39" // 0-9
	"\x2b\x2f\x3d" // + / =
	;

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
	
	write_buffer((uint8_t[]){ O.tag_escape == TAG_ESCAPE_NUL ? 0 : 0xa }, 1, ascii_out);
	
	if (!O.tag_raw) {
		fclose(ascii_out);
		pthread_join(loctr_th, NULL);
	}
}

void put_left(size_t rew);

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
	bool met_comment = false;
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
		switch (type) {
		case 4:
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
			else {
			}
			// FALLTHROUGH
		default:
			if (type != 1) write_buffer(gbuf, 4, built_stream); // パディングを削除
			while (left) {
				size_t readlen = left > gbuflen ? gbuflen : left;
				size_t readret = fread(gbuf, 1, readlen, stream_input);
				if (readlen != readret) oserror();
				if (type != 1) write_buffer(gbuf, readlen, built_stream);
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
	write_buffer("\x81\0\0", 4, built_stream); // 「最後のヘッダ」標識を立てたパディングで〆
	put_left(-(size_t)ftell(stream_input)); // "seeked_len(0) - rew" でfseek()するので
}
