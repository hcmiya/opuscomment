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

static void check_opus(ogg_page *og) {
	if (og->body_len < 19) {
		opuserror(err_opus_bad_content);
	}
	if ((og->body[8] & 0xf0) != 0) {
		opuserror(err_opus_version);
	}
	if (O.gain_put) {
		if (O.gain_q78) {
			fprintf(stderr, "%d", (int16_t)oi16(*(int16_t*)(&og->body[16])));
		}
		else {
			fprintf(stderr, "%.8g", (int16_t)oi16(*(int16_t*)(&og->body[16])) / 256.0);
		}
	}
	if (O.gain_fix) {
		int16_t gi;
		if (O.gain_relative) {
			int gain = (int16_t)oi16(*(int16_t*)(&og->body[16]));
			gain += O.gain_val;
			if (gain > 32767) {
				gain = 32767;
			}
			else if (gain < -32768) {
				gain = -32768;
			}
			gi = gain;
		}
		else {
			gi = O.gain_val;
		}
		if (O.gain_not_zero && gi == 0) {
			gi = O.gain_val_sign ? -1 : 1;
		}
		
		*(int16_t*)(&og->body[16]) = oi16(gi);
		ogg_page_checksum_set(og);
		
		if (O.gain_put) {
			if (O.gain_q78) {
				fprintf(stderr, "\t%d", (int)gi);
			}
			else {
				fprintf(stderr, "\t%.8g", gi / 256.0);
			}
		}
	}
	if (O.gain_put) {
		fputc('\n', stderr);
	}
}

static void check_theora(ogg_page *og) {
	if (og->body_len != 42) {
		opuserror(err_opus_bad_content);
	}
	// major 3, minor 2
	if (og->body[7] != 3 || og->body[8] != 2) {
		opuserror(err_opus_version);
	}
}

static void check_vorbis(ogg_page *og) {
// 0-6 "\x1" "\x76\x6f\x72\x62\x69\x73" 
/*7-10   1) [vorbis_version] = read 32 bits as unsigned integer
11   2) [audio_channels] = read 8 bit integer as unsigned
12-15   3) [audio_sample_rate] = read 32 bits as unsigned integer
16   4) [bitrate_maximum] = read 32 bits as signed integer
20   5) [bitrate_nominal] = read 32 bits as signed integer
24   6) [bitrate_minimum] = read 32 bits as signed integer
28.0-3   7) [blocksize_0] = 2 exponent (read 4 bits as unsigned integer)
28.4-7   8) [blocksize_1] = 2 exponent (read 4 bits as unsigned integer)
29   9) [framing_flag] = read one bit
*/
	if (og->body_len != 30) opuserror(err_opus_bad_content);
	if (oi32(*(uint32_t*)&og->body[7]) != 0) opuserror(err_opus_version);
	if (!og->body[11]) opuserror(err_opus_bad_content);
	if (oi32(*(uint32_t*)&og->body[12]) == 0) opuserror(err_opus_bad_content);
	if (!og->body[29]) opuserror(err_opus_bad_content);
}

static void check_daala(ogg_page *og) {
// TODO
// Daalaの仕様文書がないのでマジックナンバー読むぐらいしかしてない
}

static void check_speex(ogg_page *og) {
	if (og->body_len != 80) opuserror(err_opus_bad_content);
	if (oi32(*(uint32_t*)&og->body[28]) != 1) opuserror(err_opus_version);
}

static void check_pcm(ogg_page *og) {
	if (og->body_len != 28) opuserror(err_opus_bad_content);
	if (htons(*(uint16_t*)&og->body[8]) != 0 || htons(*(uint16_t*)&og->body[10]) != 0) {
		opuserror(err_opus_version);
	}
}

static void check_uvs(ogg_page *og) {
	if (og->body_len != 48) opuserror(err_opus_bad_content);
	if (htons(*(uint16_t*)&og->body[8]) != 0 || htons(*(uint16_t*)&og->body[10]) != 0) {
		opuserror(err_opus_version);
	}
}

static struct codec_parser set[] = {
	// "OpusHead", "OpusTags"
	{NULL, "Opus", 8, "\x4f\x70\x75\x73\x48\x65\x61\x64", check_opus, 8, "\x4f\x70\x75\x73\x54\x61\x67\x73"},
	// "\x80" "theora", "\x81" "theora"
	{"theoracomment", "Theora", 7, "\x80" "\x74\x68\x65\x6F\x72\x61", check_theora, 7, "\x81" "\x74\x68\x65\x6F\x72\x61"},
	// "\x1" "vorbis", "\x3" "vorbis"
	{"vorbiscomment", "Vorbis", 7, "\x1" "\x76\x6f\x72\x62\x69\x73", check_vorbis, 7, "\x3" "\x76\x6f\x72\x62\x69\x73"},
	// "\x80" "daala", "\x81" "daala"
	{"daalacomment", "Daala", 6, "\x80" "\x64\x61\x61\x6c\x61", check_daala, 6, "\x81" "\x64\x61\x61\x6c\x61"},
	// "Speex   ", NULL
	{"speexcomment", "Speex", 8, "\x53\x70\x65\x65\x78\x20\x20\x20", check_speex, 0, NULL},
	// "PCM     ", NULL
	{"oggpcmcomment", "PCM", 8, "\x50\x43\x4d\x20\x20\x20\x20\x20", check_pcm, 0, NULL},
	// "UVS     ", NULL
	{"ogguvscomment", "UVS", 8, "\x55\x56\x53\x20\x20\x20\x20\x20", check_uvs, 0, NULL},
	{NULL, NULL, 0, NULL, NULL, 0, NULL},
};

void select_codec(void) {
	for (codec = set + 1; codec->prog != NULL && strcmp(program_name, codec->prog) != 0; codec++) {}
	if (!codec->prog) codec = set;
};
