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

static size_t seeked_len, preserved_padding_len;
static char *outtmp;
static bool remove_tmp, toupper_applied;
static uint32_t opus_idx = 2, opus_sno;
static FILE *fpout, *preserved_padding;

static char const
	*OpusHead = "\x4f\x70\x75\x73\x48\x65\x61\x64",
	*OpusTags = "\x4f\x70\x75\x73\x54\x61\x67\x73",
	*mbp = "\x4d\x45\x54\x41\x44\x41\x54\x41\x5f\x42\x4c\x4f\x43\x4b\x5f\x50\x49\x43\x54\x55\x52\x45\x3d"; // "METADATA_BLOCK_PICTURE=" in ASCII
static size_t const mbplen = 23;

void move_file(void) {
	if (fclose(fpout) == EOF) {
		fileerror(O.out ? O.out : outtmp);
	}
	if (O.out) {
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
		oserror();
	}
}

static void put_left(void) {
	size_t l;
	size_t buflen = 1 << 18;
	uint8_t *buf = malloc(buflen);
	
	clearerr(fpopus);
	if (fseek(fpopus, seeked_len, SEEK_SET)) {
		oserror();
	}
	
	while (l = fread(buf, 1, buflen, fpopus)) {
		size_t ret = fwrite(buf, 1, l, fpout);
		if (ret != l) {
			oserror();
		}
	}
	if (ferror(fpopus)) {
		oserror();
	}
	free(buf);
	move_file();
	exit(0);
}

static void write_page(ogg_page *og) {
	size_t ret;
	ret = fwrite(og->header, 1, og->header_len, fpout);
	if (ret != og->header_len) {
		oserror();
	}
	ret = fwrite(og->body, 1, og->body_len, fpout);
	if (ret != og->body_len) {
		oserror();
	}
}

static void append_tag(FILE *fp, char *tag) {
	size_t len = strlen(tag);
	uint32_t tmp32 = oi32(len);
	size_t ret = fwrite(&tmp32, 1, 4, fp);
	if (ret != 4) {
		oserror();
	}
	ret = fwrite(tag, 1, len, fp);
	if (ret != len) {
		oserror();
	}
}

FILE *fptag;
static void store_tags(size_t lastpagelen) {
	size_t len;
	uint32_t tmp32;
	len = tagnum_edit + tagnum_file;
	tmp32 = oi32(len);
	fwrite(&tmp32, 1, 4, fptag);
	
	char *tagbuf = malloc(65307);
	rewind(fpedit);
	while ((len = fread(tagbuf, 1, 65307, fpedit))) {
		fwrite(tagbuf, 1, len, fptag);
	}
	fclose(fpedit);
	if (preserved_padding) {
		rewind(preserved_padding);
		while ((len = fread(tagbuf, 1, 65307, preserved_padding))) {
			fwrite(tagbuf, 1, len, fptag);
		}
		fclose(preserved_padding);
	}
	size_t commentlen = ftell(fptag);
	
	rewind(fptag); 
	ogg_page og;
	og.header_len = 282;
	og.header = tagbuf;
	og.body_len = 255 * 255;
	og.body = tagbuf + 282;
	memcpy(og.header,
		"\x4f\x67\x67\x53"
		"\0"
		"\0"
		"\0\0\0\0\0\0\0\0", 14);
// 		"S.NO"
// 		"SEQ "
// 		"CRC ", 26);
	*(uint32_t*)&og.header[14] = oi32(opus_sno);
	memset(og.header + 26, 0xff, 256);
	
	uint32_t idx = 1;
	while (commentlen >= 255 * 255) {
		fread(og.body, 1, 255 * 255, fptag);
		*(uint32_t*)&og.header[18] = oi32(idx++);
		ogg_page_checksum_set(&og);
		write_page(&og);
		og.header[5] = 1;
		commentlen -= 255 * 255;
	}
	
	*(uint32_t*)&og.header[18] = oi32(idx++);
	og.header[26] = commentlen / 255 + 1;
	og.header[26 + og.header[26]] = commentlen % 255;
	fread(og.body, 1, commentlen, fptag);
	fclose(fptag);
	og.header_len = 27 + og.header[26];
	og.body_len = commentlen;
	
	if (!preserved_padding && idx < opus_idx) {
		// 出力するタグ部分のページ番号が入力の音声開始部分のページ番号に満たない場合、
		// 無を含むページを生成して開始ページ番号を合わせる
		uint8_t segnum = og.header[26];
		uint8_t lastseglen = og.header[26 + segnum];
		uint8_t lastseg[255];
		
		if (segnum == 1) {
			og.header[27] = 255;
			memcpy(lastseg, og.body, lastseglen);
			lastseg[lastseglen] = 0;
			og.body = lastseg;
			og.body_len = 255;
			ogg_page_checksum_set(&og);
			write_page(&og);
			memset(lastseg, 0, 255);
		}
		else {
			og.header[26]--;
			og.header_len--;
			og.body_len -= lastseglen;
			ogg_page_checksum_set(&og);
			write_page(&og);
			memcpy(og.body, &og.body[og.body_len], lastseglen);
			og.body[lastseglen] = 0;
		}
		
		og.header[5] = 1;
		og.header[26] = 1;
		og.header[27] = 255;
		og.header_len = 28;
		og.body_len = 255;
		for (uint32_t m = opus_idx - 1; idx < m; idx++) {
			*(uint32_t*)&og.header[18] = oi32(idx);
			ogg_page_checksum_set(&og);
			write_page(&og);
		}
		*(uint32_t*)&og.header[18] = oi32(idx);
		og.header[26] = 1;
		og.header[27] = lastseglen;
		og.header_len = 28;
		og.body_len = lastseglen;
		ogg_page_checksum_set(&og);
		write_page(&og);
		
		seeked_len -= lastpagelen;
		put_left();
		/* NOTREACHED */
	}
	else {
		ogg_page_checksum_set(&og);
		write_page(&og);
		
		if (idx == opus_idx) {
			seeked_len -= lastpagelen;
			put_left();
			/* NOTREACHED */
		}
	}
	free(tagbuf);
	opus_idx = idx;
}

static void comment_aborted(void) {
	opuserror(catgets(catd, 3, 2, "ヘッダが途切れている"));
}

static void invalid_border(void) {
	opuserror(catgets(catd, 3, 3, "ヘッダのページとパケットの境界が変"));
}

static void not_an_opus(void) {
	opuserror(catgets(catd, 3, 4, "Opusではない"));
}

static void invalid_stream(void) {
	opuserror(catgets(catd, 3, 5, "異常なOpusストリーム"));
}

static void cleanup(void) {
	if (remove_tmp) {
		remove(outtmp);
	}
}

static int test_break(ogg_page *og) {
	uint16_t plen;
	switch (page_breaks(og, 1, &plen)) {
	case 0:
		return -1;
	case 1:
		if (plen == og->body_len) return plen;
	}
	invalid_border();
}

static void parse_header(ogg_page *og) {
	opus_sno = ogg_page_serialno(og);
	if (!ogg_page_bos(og) || ogg_page_eos(og)) {
		invalid_stream();
	}
	if (ogg_page_pageno(og) != 0) {
		invalid_stream();
	}
	if (test_break(og) < 0) {
		invalid_border();
	}
	if (og->body_len < 19 || memcmp(og->body, OpusHead, 8) != 0) {
		not_an_opus();
	}
	if ((og->body[8] & 0xf0) != 0) {
		opuserror(catgets(catd, 3, 6, "未対応のバージョン"));
	}
	switch (O.info_gain) {
		case 1:
			fprintf(stderr, "%.8g\n", (int16_t)oi16(*(int16_t*)(&og->body[16])) / 256.0);
			break;
		case 2:
			fprintf(stderr, "%d\n", (int16_t)oi16(*(int16_t*)(&og->body[16])));
			break;
	}
	if (O.gain_fix) {
		int16_t gi;
		bool sign;
		if (O.gain_relative) {
			double gain = (int16_t)oi16(*(int16_t*)(&og->body[16]));
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
		
		*(int16_t*)(&og->body[16]) = oi16(gi);
	}
	if (O.edit != EDIT_LIST) {
		if (O.out) {
			if (strcmp(O.out, "-") == 0) {
				fpout = stdout;
			}
			else {
				fpout = fopen(O.out, "w");
				if (!fpout) {
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
			fpout = fdopen(fd, "w+");
		}
		ogg_page_checksum_set(og);
		write_page(og);
	}
	opst = OPUS_HEADER_BORDER;
}


static void copy_tag_packet(ogg_page *og) {
	fwrite(og->body, 1, og->body_len, fptag);
}

static void parse_header_border(ogg_page *og) {
	if (ogg_page_serialno(og) != opus_sno) {
		opuserror(catgets(catd, 3, 7, "複数論理ストリームを持つOggには対応していない"));
	}
	if (ogg_page_pageno(og) != 1) {
		invalid_stream();
	}
	if (ogg_page_bos(og) || ogg_page_eos(og)) {
		invalid_stream();
	}
	if (ogg_page_continued(og)) {
		invalid_border();
	}
	if (O.gain_fix && O.edit == EDIT_NONE) {
		// 出力ゲイン編集のみの場合
		if (O.out) {
			// 出力指定が別にあれば残りをコピー
			seeked_len -= og->header_len + og->body_len;
			put_left();
		}
		else {
			// 上書きするなら最初のページのみを直接書き換えようとする
			size_t headpagelen = ftell(fpout);
			rewind(fpout);
			uint8_t *b = malloc(headpagelen);
			
			seeked_len -= og->header_len + og->body_len + headpagelen;
			fpopus = freopen(NULL, "r+", fpopus);
			if (!fpopus) {
				oserror();
			}
			if (fseek(fpopus, seeked_len, SEEK_SET)) {
				oserror();
			}
			size_t ret = fread(b, 1, headpagelen, fpout);
			if (ret != headpagelen) {
				oserror();
			}
			ret = fwrite(b, 1, headpagelen, fpopus);
			if (ret != headpagelen) {
				oserror();
			}
			exit(0);
		}
		/* NOTREACHED */
	}
	
	opst = test_break(og) < 0 ? OPUS_COMMENT : OPUS_COMMENT_BORDER;
	fptag = tmpfile();
	copy_tag_packet(og);
}

static void parse_comment(ogg_page *og) {
	if (ogg_page_serialno(og) != opus_sno) {
		opuserror(catgets(catd, 3, 7, "複数論理ストリームを持つOggには対応していない"));
	}
	if (ogg_page_pageno(og) != opus_idx++) {
		invalid_stream();
	}
	
	if (ogg_page_bos(og) || ogg_page_bos(og)) {
		invalid_stream();
	}
	if (!ogg_page_continued(og)) {
		invalid_stream();
	}
	opst = test_break(og) < 0 ? OPUS_COMMENT : OPUS_COMMENT_BORDER;
	copy_tag_packet(og);
}

static void rtread(void *p, size_t len) {
	size_t readlen = fread(p, 1, len, fptag);
	if (readlen != len) comment_aborted();
}

static uint32_t rtchunk(void) {
	uint32_t rtn;
	rtread(&rtn, 4);
	return oi32(rtn);
}

static size_t mbpnum;
static void rtcopy(void) {
	uint32_t len = rtchunk();
	uint8_t buf[512];
	bool first = true;
	bool field = true;
	bool copy;
	while (len) {
		size_t rl = len > 512 ? 512 : len;
		rtread(buf, rl);
		if (first) {
			first = false;
			bool ismbp;
			switch (O.edit) {
			case EDIT_WRITE:
			case EDIT_LIST:
				ismbp = false;
				if (O.tag_ignore_picture && len >= mbplen) {
					uint8_t mbpupper[23];
					memcpy(mbpupper, buf, 23);
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
				uint8_t chunk[4];
				*(uint32_t*)chunk = oi32(len);
				fwrite(chunk, 4, 1, fpedit);
			}
		}
		if (copy) {
			if (O.tag_toupper && field) {
				for (char *p = buf, *endp = buf + rl; p < endp; p++) {
					if (*p >= 0x61 && *p <= 0x7a) {
						*p -= 32;
					}
					if (*p == 0x3d) {
						field = false;
						break;
					}
				}
			}
			fwrite(buf, 1, rl, fpedit);
		}
		len -= rl;
	}
}

static void retrieve_tag() {
	uint8_t buf[512];
	rewind(fptag);
	
	rtread(buf, 8);
	if (memcmp(buf, OpusTags, 8) != 0) {
		not_an_opus();
	}
	
	// ベンダ文字列
	uint32_t len = rtchunk();
	while (len) {
		size_t rl = len > 512 ? 512 : len;
		rtread(buf, rl);
		len -= rl;
	}
	long int tagpacket_end = ftell(fptag);
	
	// レコード数
	size_t tagnum_file = rtchunk();
	size_t recordnum = tagnum_file;
	fpedit = fpedit ? fpedit : tmpfile();
	
	while (recordnum) {
		rtcopy();
		recordnum--;
	}
	
	if (O.edit == EDIT_WRITE) {
		tagnum_file = mbpnum;
	}
	
	len = fread(buf, 1, 1, fptag);
	if (len && (*buf & 1)) {
		preserved_padding = tmpfile();
		fwrite(buf, 1, 1, preserved_padding);
		size_t n;
		while ((n = fread(buf, 1, 512, fptag))) {
			fwrite(buf, 1, n, preserved_padding);
		}
	}
	fseek(fptag, tagpacket_end, SEEK_SET);
	int fdtag = fileno(fptag);
	ftruncate(fdtag, tagpacket_end);
}

static void parse_comment_border(ogg_page *og) {
	if (ogg_page_continued(og)) {
		invalid_border();
	}
	retrieve_tag();
	switch (O.edit) {
	case EDIT_APPEND:
	case EDIT_WRITE:
		parse_tags();
		break;
		
	case EDIT_LIST:
	case EDIT_NONE:
		fclose(stdin);
		break;
	}
	if (O.edit == EDIT_APPEND && !tagnum_edit && !O.out && !O.gain_fix && !toupper_applied) {
		// タグ追記モードで出力が上書き且つ
		// タグ入力、ゲイン調整、大文字化適用が全て無い場合はすぐ終了する
		exit(0);
	}
	else if (O.edit == EDIT_LIST) {
		fclose(fpopus);
		put_tags();
		/* NOTREACHED */
	}
	else {
		store_tags(og->header_len + og->body_len);
	}
	opst = OPUS_SOUND;
}

static void parse_page_sound(ogg_page *og) {
	if (ogg_page_serialno(og) == opus_sno) {
		*(uint32_t*)&og->header[18] = oi32(opus_idx++);
		ogg_page_checksum_set(og);
	}
	write_page(og);
	if (ogg_page_serialno(og) == opus_sno && ogg_page_eos(og)) {
		put_left();
		/* NOTREACHED */
	}
}

static void parse_page(ogg_page *og) {
	ogg_packet op;
	switch (opst) {
	case OPUS_HEADER:
		parse_header(og);
		break;
		
	case OPUS_HEADER_BORDER:
		parse_header_border(og);
		break;
		
	case OPUS_COMMENT:
		parse_comment(og);
		break;
		
	case OPUS_COMMENT_BORDER:
		parse_comment_border(og);
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
