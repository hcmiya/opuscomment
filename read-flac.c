#include <ogg/ogg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <arpa/inet.h>

#include "opuscomment.h"
#include "global.h"

static uint8_t init_page_buf[79];
static ogg_page init_page;
static size_t flac_header_num = 0;
static FILE *tyukan;

static void put_streaminfo(void) {
	*(uint16_t*)&init_page.body[7] = ntohs(flac_header_num);
	ogg_page_checksum_set(&init_page);
	fwrite(init_page_buf, 1, 79, fpout);
}

void check_flac(ogg_page *og) {
	// https://xiph.org/flac/ogg_mapping.html
	if (og->body_len != 51) opuserror(err_opus_bad_content);
	if (og->body[5] != 1 || og->body[6] != 0) {
		opuserror(err_opus_version);
	}
	// og->body[7]の2バイトに後でヘッダの数を格納
	// "fLaC", STREAM_INFO (=0), size (=34)
	if (memcmp(&og->body[9], "\x66\x4C\x61\x43" "\x0" "\x0\x0\x22", 8) != 0) {
		opuserror(err_opus_bad_content);
	}
	uint32_t hz = og->body[27] << 12 | og->body[28] << 4 | og->body[29] >> 4;
	if (hz == 0 || hz > 655350) opuserror(err_opus_bad_content);
	init_page.header = init_page_buf;
	init_page.header_len = 28;
	memcpy(init_page_buf, og->header, 26);
	init_page_buf[26] = 1;
	init_page_buf[27] = 51;
	init_page.body = init_page_buf + 28;
	init_page.body_len = 51;
	memcpy(init_page_buf + 28, og->body, 51);
}

void copy_metadata(ogg_page *og) {
	set_pageno(og, opus_idx++);
	ogg_page_checksum_set(og);
	write_page(og, tyukan);
}

static bool last_header;
void flac_next_is_audio(void) {
	last_header = true;
}

bool parse_page_sound(ogg_page *og);
bool parse_info_border(ogg_page *og);
void put_left(size_t);

bool parse_metadata_border(ogg_page *og) {
	if (last_header) {
		if (og->body[0] != 0xff) {
			opuserror(err_opus_bad_content);
		}
		// メタデータの数を確定させてstreaminfoに書き込む
		put_streaminfo();
		uint8_t buf[STACK_BUF_LEN];
		size_t rl;
		// 溜めてたメタデータを出力に移動
		rewind(tyukan);
		while ((rl = fread(buf, 1, STACK_BUF_LEN, tyukan))) {
			if (fwrite(buf, 1, rl, fpout) != rl) oserror();
		}
		fclose(tyukan);
		// 後続の音声データをページ番号をずらしつつ出力
		opst = PAGE_SOUND;
		opus_idx_diff = ogg_page_pageno(og) - opus_idx;
		if (!opus_idx_diff) {
			put_left(og->header_len + og->body_len);
		}
		return parse_page_sound(og);
	}
	uint8_t htype = og->body[0] & 0x7f;
	if (!(htype >= 1 && htype <= 126)) {
		opuserror(err_opus_bad_content);
	}
	last_header = og->body[0] & 0x80;
	if ((opus_idx == 1) ^ (htype == 4)) {
		opuserror(err_opus_bad_content);
	}
	if (htype == 4) {
		return parse_info_border(og);
	}
	opst = PAGE_OTHER_METADATA;
	copy_metadata(og);
	return true;
}

bool parse_metadata(ogg_page *og) {
	if (!ogg_page_continued(og)) {
		flac_header_num++;
		return parse_metadata_border(og);
	}
	copy_metadata(og);
	return true;
}

bool parse_info(ogg_page *og);
bool parse_comment(ogg_page *og);

void parse_flac(ogg_page *og) {
	bool isflac;
	if (opst == PAGE_INFO) {
		isflac = parse_info(og);
	}
	else if ((isflac = test_non_opus(og))) {
		switch (opst) {
		case PAGE_INFO_BORDER:
			parse_metadata_border(og);
			break;
			
		case PAGE_COMMENT:
			parse_comment(og);
			break;
			
		case PAGE_OTHER_METADATA:
			parse_metadata(og);
			break;
			
		case PAGE_SOUND:
			parse_page_sound(og);
			break;
		}
	}
	if (isflac && opst < PAGE_SOUND && ogg_page_eos(og)) {
		opuserror(err_opus_bad_content);
	}
	if (!isflac) {
		if (ogg_page_pageno(og) == 0) {
			write_page(og, fpout);
		}
		else {
			tyukan = tyukan ? tyukan : tmpfile();
			write_page(og, tyukan);
		}
	}
}

void flac_metadata_term_test(ogg_page *og) {
	if (last_header ^ (og->body[0] == 0xff)) {
		// FLACの最終メタデータ指示と音声データ開始が一致しない時エラー
		opuserror(err_opus_bad_content);
	}
}

bool flac_make_tag_packet(ogg_page *og, FILE *tag, FILE **out) {
	uint32_t cplen = ntohl((ftell(tag) - 4) | 0x04000000 | (last_header << 31));
	rewind(tag);
	fwrite(&cplen, 4, 1, tag);
	flac_header_num = 1;
	if (last_header) {
		// vorbis_commentが最後で次のパケットが音声の時、
		// STREAMINFOを書き込んで他のコーデックと同じ処理を続ける
		put_streaminfo();
	}
	else {
		*out = tyukan = tyukan ? tyukan : tmpfile();
	}
	return last_header;
}
