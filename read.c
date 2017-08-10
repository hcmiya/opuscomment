#include <ogg/ogg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "opuscomment.h"
#include "global.h"

#include "endianness-check.h"
#ifdef LITTLE_ENDIAN

static inline uint16_t oi16(uint16_t i) {
	return i;
}
static inline uint32_t oi32(uint32_t i) {
	return i;
}
static inline uint64_t oi64(uint64_t i) {
	return i;
}
#else

#define M 255ULL

static inline uint16_t oi16(uint16_t i) {
	return i << 8 | i >> 8;
}
static inline uint32_t oi32(uint32_t i) {
	return i << 24 | (i & (M << 8)) << 8 | (i & (M << 16)) >> 8 | i >> 24;
}
static inline uint64_t oi64(uint64_t i) {
	return i << 56 | (i & (M << 8)) << 40 | (i & (M << 16)) >> 24 | (i & (M << 24)) >> 8
		|(i & (M << 32)) >> 8 | (i & (M << 40)) >> 24 | (i & (M << 48)) >> 40 | i >> 56;
}
#undef M
#endif

static size_t seeked_len;
static ogg_stream_state ios, oos;
static char *outtmp;
static bool remove_tmp;
static uint32_t opus_pidx, opus_sno;
static FILE *fpout;

static char const *OpusHead = "\x4f\x70\x75\x73\x48\x65\x61\x64",
	*OpusTags = "\x4f\x70\x75\x73\x54\x61\x67\x73",
	*mbp = "\x4d\x45\x54\x41\x44\x41\x54\x41\x5f\x42\x4c\x4f\x43\x4b\x5f\x50\x49\x43\x54\x55\x52\x45\x3d"; // "METADATA_BLOCK_PICTURE="
staic size_t const mbplen = 23;

void move_file(void) {
	if (O.out) {
		if (strcmp(O.out, "-") != 0) {
			FILE *tmp = freopen(O.out, "w", stdout);
			if (!tmp) {
				fileerror();
			}
		}
		size_t buflen = 1 << 18;
		size_t readlen, ret;
		uint8_t *buf = malloc(buflen);
		rewind(fpout);
		remove(outtmp); // sigpipe対策で先に一時ファイルをunlinkする
		remove_tmp = false;
		while ((readlen = fread(buf, 1, buflen, fpout))) {
			ret = fwrite(buf, 1, readlen, stdout);
			if (ret != readlen) {
				fileerror();
			}
		}
		if (!ferror(fpout)) {
			return;
		}
	}
	else {
		int fd = fileno(fpopus);
		struct stat st;
		fstat(fd, &st);
		if (!rename(outtmp, O.in)) {
			// rename(2)は移動先が存在していてもunlink(2)の必要はない
			remove_tmp = false;
			if (!chmod(O.in, st.st_mode)) {
				return;
			}
		}
	}
	fileerror();
}

static void put_left(void) {
	size_t l;
	size_t buflen = 1 << 18;
	uint8_t *buf = malloc(buflen);
	
	clearerr(fpopus);
	if (fseek(fpopus, seeked_len, SEEK_SET)) {
		fileerror();
	}
	
	while (l = fread(buf, 1, buflen, fpopus)) {
		size_t ret = fwrite(buf, 1, l, fpout);
		if (ret != l) {
			fileerror();
		}
	}
	if (ferror(fpopus)) {
		fileerror();
	}
	free(buf);
	move_file();
	exit(0);
}

static void write_page(ogg_page *og) {
	size_t ret;
	ret = fwrite(og->header, 1, og->header_len, fpout);
	if (ret != og->header_len) {
		fileerror();
	}
	ret = fwrite(og->body, 1, og->body_len, fpout);
	if (ret != og->body_len) {
		fileerror();
	}
}

static void append_tag(FILE *fp, char *tag) {
	size_t len = strlen(tag);
	uint32_t tmp32 = oi32(len);
	size_t ret = fwrite(&tmp32, 1, 4, fp);
	if (ret != 4) {
		fileerror();
	}
	ret = fwrite(tag, 1, len, fp);
	if (ret != len) {
		fileerror();
	}
}


static void store_tags(size_t lastpagelen) {
	FILE *fptag = tmpfile();
	fwrite(OpusTags, 1, 8, fptag);
	size_t len = strlen(vendor);
	uint32_t tmp32 = oi32(len);
	fwrite(&tmp32, 1, 4, fptag);
	fwrite(vendor, 1, len, fptag);
	
	char **cp;
	len = tagnum_edit + tagnum_file;
	tmp32 = oi32(len);
	fwrite(&tmp32, 1, 4, fptag);
	for (cp = tag_file; *cp; cp++) {
		append_tag(fptag, *cp);
		free(*cp);
	}
	free(tag_file);
	
	for (cp = tag_edit; *cp; cp++) {
		append_tag(fptag, *cp);
		free(*cp);
	}
	free(tag_edit);
	
	size_t tplen = ftell(fptag);
	size_t tpnum = tplen / (255*255) + 1;
	
	char *tagbuf = malloc(tplen);
	
	rewind(fptag);
	fread(tagbuf, 1, tplen, fptag);
	fclose(fptag);
	
	ogg_packet op;
	op.packet = tagbuf;
	op.bytes = tplen;
	op.b_o_s = 0;
	op.e_o_s = 0;
	op.granulepos = 0;
	ogg_stream_packetin(&oos, &op);
	free(tagbuf);
	
	ogg_page og;
	while(ogg_stream_pageout(&oos, &og)) {
		write_page(&og);
	}
	
	if (oos.pageno + 1 < ios.pageno) {
		// 出力するタグ部分のページ番号が入力の音声開始部分のページ番号に満たない場合、
		// 無を含むページを生成して開始ページ番号を合わせる
		ogg_stream_flush(&oos, &og);
		uint8_t segnum = og.header[26];
		uint8_t lastseglen = og.header[og.header_len - 1];
		uint8_t lastseg[255];
		
		if (segnum == 1) {
			og.header[27] = 255;
			memcpy(lastseg, og.body, lastseglen);
			og.body = lastseg;
			og.body_len = 255;
			ogg_page_checksum_set(&og);
			write_page(&og);
			lastseglen = 0;
			memset(lastseg, 0, 255);
		}
		else {
			og.header[26]--;
			og.header_len--;
			og.body_len -= lastseglen;
			ogg_page_checksum_set(&og);
			write_page(&og);
			memcpy(og.body, &og.body[og.body_len], lastseglen);
		}
		
		og.header[5] = 1;
		og.header[26] = 1;
		og.header[27] = 255;
		og.header_len = 28;
		og.body_len = 255;
		for (uint32_t i = oos.pageno, m = ios.pageno - 1; i < m; i++) {
			*(uint32_t*)&og.header[18] = oi32(i);
			ogg_page_checksum_set(&og);
			write_page(&og);
		}
		*(uint32_t*)&og.header[18] = oi32(ios.pageno - 1);
		og.header[26] = 1;
		og.header[27] = lastseglen;
		og.header_len = 28;
		og.body_len = lastseglen;
		ogg_page_checksum_set(&og);
		write_page(&og);
		
		seeked_len -= lastpagelen;
		put_left();
	}
	else {
		ogg_stream_flush(&oos, &og);
		write_page(&og);
		
		if (oos.pageno == ios.pageno) {
			seeked_len -= lastpagelen;
			put_left();
		}
	}
	
	opus_pidx = oos.pageno;
	ogg_stream_clear(&oos);
}

static void parse_page_sound(ogg_page *og) {
	*(uint32_t*)&og->header[18] = oi32(opus_pidx++);
	ogg_page_checksum_set(og);
	write_page(og);
	if (ogg_page_eos(og)) {
		put_left();
	}
}

static void comment_aborted(void) {
	opuserror("Opusヘッダが途切れている");
}

static void invalid_border(void) {
	opuserror("Opusヘッダのページとパケットの境界が変");
}

static void not_an_opus(void) {
	opuserror("Opusではない");
}

static void invalid_stream(void) {
	opuserror("異常なOpusストリーム");
}

static void cleanup(void) {
	if (remove_tmp) {
		remove(outtmp);
	}
}

static void parse_header(ogg_page *og) {
	ogg_packet op;
	opus_sno = ogg_page_serialno(og);
	ogg_stream_init(&ios, opus_sno);
	ogg_stream_pagein(&ios, og);
	if (ogg_stream_packetout(&ios, &op) != 1) {
		opuserror("Opusではないっぽい");
	}
	
	if (!op.b_o_s || op.e_o_s) {
		invalid_stream();
	}
	if (ogg_page_pageno(og) != 0) {
		invalid_stream();
	}
	if (op.bytes < 19 || memcmp(op.packet, OpusHead, 8) != 0) {
		not_an_opus();
	}
	if (op.packet[8] != 1) {
		opuserror("未対応のOpusバージョン");
	}
	switch (O.info_gain) {
		case 1:
			fprintf(stderr, "%.8g\n", (int16_t)oi16(*(int16_t*)(&op.packet[16])) / 256.0);
			break;
		case 2:
			fprintf(stderr, "%d\n", (int16_t)oi16(*(int16_t*)(&op.packet[16])));
			break;
	}
	if (O.gain_fix) {
		int16_t gi;
		bool sign;
		if (O.gain_relative) {
			double gain = (int16_t)oi16(*(int16_t*)(&op.packet[16]));
			gain += O.gain_val * 256;
			if (gain > 32767) {
				gain = 32767;
			}
			else if (gain < -32768) {
				gain = -32768;
			}
			gi = (int16_t)gain;
			sign = signbit(gain);
		}
		else {
			gi = (int16_t)(O.gain_val * 256);
			sign = signbit(O.gain_val);
		}
		if (O.gain_not_zero && gi == 0) {
			gi = sign ? -1 : 1;
		}
		
		*(int16_t*)(&op.packet[16]) = oi16(gi);
	}
	ogg_page out;
	if (O.edit != EDIT_LIST) {
		ogg_stream_init(&oos, opus_sno);
		char const *tmpl = "opuscomment.XXXXXX";
		char *outdir = O.out ? O.out : O.in;
		outtmp = calloc(strlen(outdir) + strlen(tmpl) + 1, 1);
		char *p = strrchr(outdir, '/');
		if (p) {
			p++;
			strncpy(outtmp, outdir, p - outdir);
			strcpy(outtmp + (p - outdir), tmpl);
		}
		else {
			strcpy(outtmp, tmpl);
		}
		int fd = mkstemp(outtmp);
		if (fd == -1) {
			fileerror();
		}
		remove_tmp = true;
		atexit(cleanup);
		fpout = fdopen(fd, "w+");
		ogg_stream_packetin(&oos, &op);
		ogg_stream_flush(&oos, &out);
		write_page(&out);
	}
	if (ogg_stream_packetpeek(&ios, NULL) == 1) {
		invalid_border();
	}
	opst = OPUS_HEADER_BORDER;
}

static void parse_header_border(ogg_page *og) {
	if (ogg_page_pageno(og) != 1) {
		invalid_stream();
	}
	if (ogg_page_continued(og)) {
		invalid_border();
	}
	if (O.gain_fix && O.edit == EDIT_NONE) {
		if (O.out) {
			seeked_len -= og->header_len + og->body_len;
			put_left();
		}
		else {
			size_t headpagelen = ftell(fpout);
			rewind(fpout);
			uint8_t *b = malloc(headpagelen);
			
			seeked_len -= og->header_len + og->body_len + headpagelen;
			fpopus = freopen(NULL, "r+", fpopus);
			if (!fpopus) {
				fileerror();
			}
			if (fseek(fpopus, seeked_len, SEEK_SET)) {
				fileerror();
			}
			size_t ret = fread(b, 1, headpagelen, fpout);
			if (ret != headpagelen) {
				fileerror();
			}
			ret = fwrite(b, 1, headpagelen, fpopus);
			if (ret != headpagelen) {
				fileerror();
			}
			exit(0);
		}
	}
	opst = OPUS_COMMENT;
}

static void parse_comment(ogg_page *og) {
	ogg_packet op;
	if (ogg_page_serialno(og) != opus_sno) {
		opuserror("複数論理ストリームを持つOggには対応していない");
	}
	ogg_stream_pagein(&ios, og);
	if (ogg_stream_packetout(&ios, &op) != 1) return;
	
	if (op.b_o_s || op.e_o_s) {
		invalid_stream();
	}
	if (op.bytes < 16 || memcmp(op.packet, OpusTags, 8) != 0) {
		not_an_opus();
	}
	size_t left = op.bytes - 8;
	char *ptr = op.packet + 8;
	
	uint32_t len = oi32(*(uint32_t*)ptr);
	ptr += 4;
	left -= 4;
	if (len > left) comment_aborted();
	vendor = malloc(len + 1);
	memcpy(vendor, ptr, len);
	vendor[len] = '\0';
	ptr += len;
	left -= len;
	
	if (O.edit == EDIT_WRITE && !O.tag_ignore_picture) {
		tag_file = malloc(sizeof(*tag_file));
		tagnum_file = 0;
		*tag_file = NULL;
	}
	else {
		if (left < 4) comment_aborted();
		len = oi32(*(uint32_t*)ptr);
		ptr += 4;
		left -= 4;
		tagnum_file = len;
		tag_file = malloc((len + 1) * sizeof(*tag_file));
		char **cp = tag_file;
		size_t mbpnum = 0;
		
		for (size_t i = 0; i < tagnum_file; i++) {
			if (left < 4) comment_aborted();
			len = oi32(*(uint32_t*)ptr);
			ptr += 4;
			left -= 4;
			
			if (left < len) comment_aborted();
			bool copy, ismbp;
			switch (O.edit) {
			case EDIT_WRITE:
			case EDIT_LIST:
				ismbp = false;
				if (len >= mbplen) {
					uint8_t mbpupper[23];
					memcpy(mbpupper, ptr, 23);
					for (size_t i = 0; i < 23; i++) {
						if (mbpupper[i] >= 0x61 && mbpupper[i] <= 0x7a) {
							mbpupper[i] -= 32;
						}
					}
					if (memcmp(mbpupper, mbp, mbplen) == 0) {
						ismbp = true;
					}
				}
				break;
			}
			
			switch (O.edit) {
			case EDIT_WRITE:
				copy = ismbp;
				mbpnum += ismbp;
				break;
			case EDIT_LIST:
				copy = !O.tag_ignore_picture || !ismbp;
				break;
			case EDIT_APPEND:
				copy = true;
				break;
			}
			if (copy) {
				*cp = malloc(len + 1);
				memcpy(*cp, ptr, len);
				(*cp)[len] = '\0';
				cp++;
			}
			ptr += len;
			left -= len;
		}
		if (O.edit == EDIT_WRITE) {
			tagnum_file = mbpnum;
		}
		*cp = NULL;
	}
	
	if (ogg_stream_packetpeek(&ios, NULL) == 1) {
		invalid_border();
	}
	opst = OPUS_COMMENT_BORDER;
}

static void parse_page(ogg_page *og) {
	ogg_packet op;
	switch (opst) {
	case OPUS_HEADER:
		parse_header(og);
		break;
		
	case OPUS_HEADER_BORDER:
		parse_header_border(og);
		/* FALLTHROUGH */
		
	case OPUS_COMMENT:
		parse_comment(og);
		break;
		
	case OPUS_COMMENT_BORDER:
		if (ogg_page_continued(og)) {
			invalid_border();
		}
		if (O.edit == EDIT_APPEND && !tagnum_edit && !O.out && !O.gain_fix) {
			// タグ追記モードでタグ入力が無く、出力が上書きでゲイン調整も無い時、すぐ終わる
			exit(0);
		}
		else if (O.edit == EDIT_LIST) {
			fclose(fpopus);
			put_tags();
		}
		else {
			store_tags(og->header_len + og->body_len);
		}
		opst = OPUS_SOUND;
		/* FALLTHROUGH */
		
	case OPUS_SOUND:
		parse_page_sound(og);
		break;
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
