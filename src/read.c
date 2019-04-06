#include <ogg/ogg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "opuscomment.h"

static size_t seeked_len;
static char *outtmp;
static bool remove_tmp;
static FILE *non_opus_stream;

void move_file(void) {
	if (fclose(built_stream) == EOF) {
		fileerror(O.out ? O.out : outtmp);
	}
	if (O.out) {
	}
	else {
		int fd = fileno(stream_input);
		struct stat st;
		fstat(fd, &st);
		if (!rename(outtmp, O.in)) {
			// rename(2)は移動先が存在していてもunlink(2)の必要はない
			remove_tmp = false;
			if (!chmod(O.in, st.st_mode)) {
				return;
			}
		}
		oserror();
	}
}

void put_left(size_t rew) {
	clearerr(stream_input);
	if (fseek(stream_input, seeked_len - rew, SEEK_SET)) {
		oserror();
	}
	
	size_t l;
	uint8_t buf[STACK_BUF_LEN];
	while (l = fread(buf, 1, STACK_BUF_LEN, stream_input)) {
		size_t ret = fwrite(buf, 1, l, built_stream);
		if (ret != l) {
			oserror();
		}
	}
	if (ferror(stream_input)) {
		oserror();
	}
	move_file();
	exit(0);
}

bool flac_make_tag_packet(ogg_page *og, FILE *fptag, FILE **built_stream);

static void store_tags(ogg_page *np, struct rettag_st *rst, struct edit_st *est, bool packet_break_in_page) {
	FILE *fptag = rst->tag;
	size_t const pagebuflen = 65536; // oggページの最大長 = 65307
	uint8_t pagebuf[pagebuflen];
	
	// タグ個数書き込み
	fseek(fptag, rst->tagbegin, SEEK_SET);
	*(uint32_t*)pagebuf = oi32(rst->num + est->num);
	fwrite(pagebuf, 4, 1, fptag);
	fseek(fptag, 0, SEEK_END);
	
	// 編集入力書き込み
	rewind(est->str); rewind(est->len);
	while (fread(pagebuf, 4, 1, est->len)) {
		// ここでpagebufはOgg構造関係なく単なる移し替え用のバッファ
		size_t reclen = *(uint32_t*)pagebuf;
		*(uint32_t*)pagebuf = oi32(reclen);
		fwrite(pagebuf, 4, 1, fptag);
		while (reclen) {
			size_t readlen = fill_buffer(pagebuf, reclen, pagebuflen, est->str);
			fwrite(pagebuf, 1, readlen, fptag);
			reclen -= readlen;
		}
	}
	fclose(est->str); fclose(est->len);
	
	if (rst->padding && rst->part_of_comment) {
		// パディングがある時はfptagの後ろに移す
		rewind(rst->padding);
		size_t readlen;
		while (readlen = fread(pagebuf, 1, pagebuflen, rst->padding)) {
			fwrite(pagebuf, 1, readlen, fptag);
		}
		fclose(rst->padding);
		rst->padding = NULL;
	}
	long commentlen = ftell(fptag);
	if (commentlen > TAG_LENGTH_LIMIT__OUTPUT) {
		exceed_output_limit();
	}
	
	rewind(fptag); 
	ogg_page og;
	og.header_len = 282;
	og.header = pagebuf;
	og.body_len = 255 * 255;
	og.body = pagebuf + 282;
	memcpy(og.header, "\x4f\x67\x67\x53\x0\x0", 6);
	set_granulepos(&og, -1);
	*(uint32_t*)&og.header[14] = oi32(opus_sno);
	memset(og.header + 26, 0xff, 256);
	
	uint32_t idx = 1;
	while (commentlen >= 255 * 255) {
		fread(og.body, 1, 255 * 255, fptag);
		set_pageno(&og, idx++);
		ogg_page_checksum_set(&og);
		write_page(&og, built_stream);
		commentlen -= 255 * 255;
		if (!og.header[5]) og.header[5] = 1;
	}
	
	// パケットの最後のページ生成
	og.header[5] = idx != 1;
	set_granulepos(&og, 0);
	set_pageno(&og, idx++);
	og.header[26] = commentlen / 255 + 1;
	og.header[26 + og.header[26]] = commentlen % 255;
	fread(og.body, 1, commentlen, fptag);
	fclose(fptag);
	og.header_len = 27 + og.header[26];
	og.body_len = commentlen;
	ogg_page_checksum_set(&og);
	write_page(&og, built_stream);
	
	// 出力するタグ部分のページ番号が入力の音声開始部分のページ番号に満たない場合、
	// 空のページを生成して開始ページ番号を合わせる
	og.header_len = 27;
	og.header[5] = 0;
	set_granulepos(&og, -1);
	og.header[26] = 0;
	og.body_len = 0;
	while (idx < opus_idx - packet_break_in_page) {
		set_pageno(&og, idx++);
		ogg_page_checksum_set(&og);
		write_page(&og, built_stream);
	}
	
	if (packet_break_in_page) {
		uint8_t *lace_tag = &np->header[27];
		while (*lace_tag == 255) {
			lace_tag++;
			np->body_len -= 255;
			np->body += 255;
		}
		np->body_len -= *lace_tag;
		np->body += *lace_tag;
		lace_tag++;
		np->header_len -= lace_tag - &np->header[27];
		np->header[5] = 0;
		np->header[26] -= lace_tag - &np->header[27];
		memmove(&np->header[27], lace_tag, np->header[26]);
		set_pageno(np, idx++);
		ogg_page_checksum_set(np);
		write_page(np, built_stream);
	}
	
	{
		rewind(non_opus_stream);
		size_t readlen;
		while (readlen = fread(pagebuf, 1, pagebuflen, non_opus_stream)) {
			if (fwrite(pagebuf, 1, readlen, built_stream) != readlen) {
				oserror();
			}
		}
		fclose(non_opus_stream);
		non_opus_stream = NULL;
	}
	if (idx == opus_idx) {
		put_left(0);
		/* NOTREACHED */
	}
	opus_idx_diff = idx - opus_idx;
	opst = PAGE_SOUND;
}

static void cleanup(void) {
	if (remove_tmp) {
		unlink(outtmp);
	}
}

void open_output_file(void) {
	if (O.edit == EDIT_LIST) {
		built_stream = fopen("/dev/null", "w");
		if (!built_stream) {
			fileerror(O.out);
		}
		remove_tmp = false;
	}
	else if (O.out) {
		if (strcmp(O.out, "-") == 0) {
			built_stream = stdout;
		}
		else {
			built_stream = fopen(O.out, "w");
			if (!built_stream) {
				fileerror(O.out);
			}
		}
		remove_tmp = false;
	}
	else {
		char const *tmpl = "opuscomment.XXXXXX";
		outtmp = calloc(strlen(O.in) + strlen(tmpl) + 1, 1);
		char *p = strrchr(O.in, '/');
		if (p) {
			p++;
			strncpy(outtmp, O.in, p - O.in);
			strcpy(outtmp + (p - O.in), tmpl);
		}
		else {
			strcpy(outtmp, tmpl);
		}
		int fd = mkstemp(outtmp);
		if (fd == -1) {
			oserror();
		}
		remove_tmp = true;
		atexit(cleanup);
		built_stream = fdopen(fd, "w+");
	}
	non_opus_stream = tmpfile();
}

static void exit_without_sigpipe(void) {
	if (!error_on_thread) {
		return;
	}
	signal(SIGPIPE, SIG_IGN);
}

static void check_header_is_single_page(ogg_page *og) {
	uint8_t lace_num = og->header[26];
	if (!lace_num) opuserror(err_opus_border);
	uint8_t const *lace = &og->header[27];
	for (int_fast16_t i = 0; i < lace_num - 1; i++) {
		if (*lace++ != 255) {
			opuserror(err_opus_border);
		}
	}
	if (*lace == 255) {
		opuserror(err_opus_border);
	}
	// 最後のlacing valueはチェックしない
}

static size_t header_packet_pos, header_packet_len;
static long built_header_pos;
bool parse_info(ogg_page *og) {
	static int osidx = 0;
	if (!ogg_page_bos(og) || ogg_page_eos(og)) {
		if (ogg_page_pageno(og) == 0) {
			opuserror(err_opus_bad_stream);
		}
		opuserror(err_opus_no_target_codec);
	}
	if (ogg_page_pageno(og) != 0) {
		// BOSがあるのにページが0でない
		opuserror(err_opus_bad_stream);
	}
	if (ogg_page_granulepos(og) != 0) {
		// ヘッダのgranuleposは必ず0
		opuserror(err_opus_bad_stream);
	}
	check_header_is_single_page(og);
	
	if (og->body_len < codec->headmagic_len || memcmp(og->body, codec->headmagic, codec->headmagic_len) != 0) {
// 		have_multi_streams = true;
		return false;
	}
	osidx++;
	if (O.target_idx && O.target_idx != osidx) {
// 		have_multi_streams = true;
		return false;
	}
	opus_sno = ogg_page_serialno(og);
	header_packet_len = og->header_len + og->body_len;
	header_packet_pos = seeked_len - header_packet_len;
	built_header_pos = ftell(built_stream);
	codec->parse(og);
	if (O.edit != EDIT_LIST && codec->type != CODEC_FLAC) {
		write_page(og, built_stream);
	}
	opus_idx++;
	opst = PAGE_INFO_BORDER;
	return true;
}

static int retriever_fd;
static bool copy_tag_packet(ogg_page *og, bool *packet_break_in_page) {
	static unsigned int total = 0;
	
	int lace_num = og->header[26];
	if (!lace_num) return false;
	
	uint8_t const *bin = og->body;
	
	uint_fast8_t i;
	for (i = 0; i < lace_num - 1 && og->header[27 + i] == 255; i++, bin += 255, total += 255) {
		if (total > TAG_LENGTH_LIMIT__INPUT) {
			opuserror(err_opus_long_tag, TAG_LENGTH_LIMIT__INPUT >> 20);
		}
		write(retriever_fd, bin, 255);
	}
	total += og->header[27 + i];
	if (total > TAG_LENGTH_LIMIT__INPUT) {
		opuserror(err_opus_long_tag, TAG_LENGTH_LIMIT__INPUT >> 20);
	}
	write(retriever_fd, bin, og->header[27 + i]);
	i++;
	bool packet_term = false;
	if (i != lace_num || og->header[27 + i - 1] != 255) {
		packet_term = true;
		*packet_break_in_page = i != lace_num;
	}
	return packet_term;
}

void *retrieve_tags(void*);
void *parse_tags(void*);
bool parse_comment(ogg_page *og);

static pthread_t retriever_thread, parser_thread;
bool parse_info_border(ogg_page *og) {
	// ここにはページ番号1で来ているはず
	if (ogg_page_continued(og)) {
		opuserror(err_opus_border);
	}
	leave_header_packets = true;
	
	if (/*O.gain_fix && */O.edit == EDIT_NONE) {
		// 出力ゲイン編集のみの場合
		if (O.out) {
			// 出力指定が別にある場合残りをコピー
			put_left(og->header_len + og->body_len);
		}
		else {
			// 上書きするなら最初のページのみを直接書き換えようとする
			uint8_t b[header_packet_len];
			FILE *stream_overwrite = freopen(NULL, "r+", stream_input);
			if (!stream_overwrite
				|| fseek(built_stream, built_header_pos, SEEK_SET)
				|| fseek(stream_overwrite, header_packet_pos, SEEK_SET)
				|| header_packet_len != fread(b, 1, header_packet_len, built_stream)
				|| header_packet_len != fwrite(b, 1, header_packet_len, stream_overwrite)
			) {
				oserror();
			}
			exit(0);
		}
		/* NOTREACHED */
	}
	
	// スレッド間通信で使っているパイプがエラー後のexit内での始末にSIGPIPEを発するのでその対策
	atexit(exit_without_sigpipe);
	error_on_thread = true;
	
	if (O.edit != EDIT_LIST) {
		// 編集入力タグパースを別スレッド化 parse_tags.c へ
		pthread_create(&parser_thread, NULL, parse_tags, NULL);
	}
	
	// OpusTagsパケットパースを別スレッド化 retrieve_tags.c へ
	int pfd[2];
	pipe(pfd);
	retriever_fd = pfd[1];
	FILE *retriever = fdopen(pfd[0], "r");
	pthread_create(&retriever_thread, NULL, retrieve_tags, retriever);
	
	// 本スレッドはOpusTagsのパケットを構築する
	opst = PAGE_COMMENT;
	return parse_comment(og);
}

static bool parse_comment_term(ogg_page *og, bool packet_break_in_page);
bool parse_page_sound(ogg_page *og);

bool parse_comment(ogg_page *og) {
	if (ogg_page_eos(og) || opus_idx > 1 && !ogg_page_continued(og)) {
		opuserror(err_opus_bad_stream);
	}
	opus_idx++;
	bool packet_break_in_page;
	if (copy_tag_packet(og, &packet_break_in_page)) {
		return parse_comment_term(og, packet_break_in_page);
	}
	
	return true;
}

static bool parse_comment_term(ogg_page *og, bool packet_break_in_page) {
	// タグパケットのパース処理のスレッドを合流
	struct rettag_st *rst;
	struct edit_st *est;
	close(retriever_fd);
	pthread_join(retriever_thread, (void**)&rst);
	if (O.edit != EDIT_LIST) {
		// 編集入力タグパースのスレッドを合流
		pthread_join(parser_thread, (void **)&est);
	}
	error_on_thread = false;
	
	if (O.edit == EDIT_APPEND && !rst->del && !est->num && !O.out && !O.gain_fix && !rst->upcase) {
		// タグ追記モードで出力が上書き且つ
		// タグ入力、ゲイン調整、タグ削除、大文字化適用が全て無い場合はすぐ終了する
		exit(0);
	}
//	来ない
// 	else if (O.edit == EDIT_LIST) {
// 		exit(0);
// 	}
	store_tags(og, rst, est, packet_break_in_page);
	free(rst);
	free(est);
	
	return true;
}

bool parse_page_sound(ogg_page *og) {
	set_pageno(og, ogg_page_pageno(og) + opus_idx_diff);
	ogg_page_checksum_set(og);
	write_page(og, built_stream);
	if (ogg_page_eos(og)) {
		put_left(0);
		/* NOTREACHED */
	}
	return true;
}

static void parse_page(ogg_page *og) {
	bool isopus;
	if (opst == PAGE_INFO) {
		isopus = parse_info(og);
	}
	else if ((isopus = test_non_opus(og))) {
		switch (opst) {
		case PAGE_INFO_BORDER:
			parse_info_border(og);
			break;
			
		case PAGE_COMMENT:
			parse_comment(og);
			break;
			
		case PAGE_SOUND:
			parse_page_sound(og);
			break;
		}
	}
	if (isopus && opst < PAGE_SOUND && ogg_page_eos(og)) {
		opuserror(err_opus_bad_stream);
	}
	if (!isopus) {
		write_page(og, ogg_page_pageno(og) && non_opus_stream ? non_opus_stream : built_stream);
	}
}

void read_page(ogg_sync_state *oy) {
	int seeklen;
	ogg_page og;
	while ((seeklen = ogg_sync_pageseek(oy, &og)) != 0) {
		if (seeklen > 0) {
			seeked_len += seeklen;
			parse_page(&og);
		}
		else seeked_len += -seeklen;
	}
}
