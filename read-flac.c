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

static uint8_t init_page[107];
static size_t flac_header_num = 0;
static FILE *tyukan;

static void write_page(ogg_page *og) {
	write_page_g(og, tyukan);
}

void check_flac(ogg_page *og) {
	// https://xiph.org/flac/ogg_mapping.html
	if (og->body_len != 79) opuserror(err_opus_bad_content);
	if (og->body[5] != 1 || og->body[6] != 0) {
		opuserror(err_opus_version);
	}
	// og->body[7]の2バイトに後でヘッダの数を格納
	// "FLAC", STREAM_INFO (=0), size (=34)
	if (memcmp(&og->body[9], "\x66\x4C\x61\x43" "\x0" "\x0\x0\x22", 8) != 0) {
		opuserror(err_opus_bad_content);
	}
	uint32_t hz = og->body[27] << 13 | og->body[28] << 5 | og->body[29] >> 3;
	if (hz == 0 || hz > 655350) opuserror(err_opus_bad_content);
	memcpy(init_page, og->header, 26);
	memcpy(init_page + 28, og->body, 79);
}

static bool last_header;
void flac_next_is_audio(void) {
	last_header = true;
}

bool parse_page_sound(ogg_page *og);

bool parse_header_border(ogg_page *og) {
	if (last_header) {
		if (og->body[0] != 0xff) {
			opuserror(err_opus_bad_content);
		}
		ogg_page ogtmp = {
			.header = init_page,
			.header_len = 28,
			.body = init_page + 28,
			.body_len = 79
		};
		uint16_t hn = ntohs(flac_header_num);
		*(uint16_t*)&ogtmp.body[7] = hn;
		ogtmp.header[26] = 1;
		ogtmp.header[27] = 79;
		ogg_page_checksum_set(&ogtmp);
		opst = PAGE_SOUND;
		return parse_page_sound(og);
	}
	uint8_t htype = og->body[0] & 0x7f;
	if (!(htype >= 1 && htype <= 126 && htype != 4)) {
		opuserror(err_opus_bad_content);
	}
	last_header = og->body[0] & 0x80;
}

bool parse_header(ogg_page *og) {
	
}

bool parse_info(ogg_page *og);

void parse_flac(ogg_page *og) {
	bool isflac;
	if (opst == PAGE_INFO) {
		isflac = parse_info(og);
	}
	else if ((isflac = test_non_opus(og))) {
		switch (opst) {
		case PAGE_INFO_BORDER:
			parse_header_border(og);
			break;
			
		case PAGE_COMMENT:
			parse_header(og);
			break;
			
		case PAGE_SOUND:
			parse_page_sound(og);
			break;
		}
	}
	if (!isflac) {
		write_page(og);
	}
}


