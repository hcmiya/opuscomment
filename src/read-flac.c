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
	return last;
}

static void write_buffer(void *buf, size_t len, FILE *fp) {
	size_t wlen = fwrite(buf, 1, len, built_stream);
	if (wlen != len) oserror();
}

static void read_comment(size_t left) {
	// ここから read.c の parse_info_border() と大体一緒
	// スレッド間通信で使っているパイプがエラー後のexit内での始末にSIGPIPEを発するのでその対策
	atexit(exit_without_sigpipe);
	error_on_thread = true;
	
	pthread_t retriever_thread, parser_thread;
	if (O.edit != EDIT_LIST) {
		fprintf(stderr, "flac hensyuu ni wa mada taiou site imasen!\n");
		exit(1);
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
	}
	error_on_thread = false;
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
			break;
//		case 6:
//			break;
		default:
			write_buffer(gbuf, 4, built_stream);
			while (left) {
				size_t readlen = left > gbuflen ? gbuflen : left;
				size_t readret = fread(gbuf, 1, readlen, stream_input);
				if (readlen != readret) oserror();
				write_buffer(gbuf, readlen, built_stream);
				left -= readlen;
			}
			break;
		}
		metadata_num++;
	}
	opst = PAGE_SOUND;
}
