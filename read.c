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
#include "global.h"

static size_t seeked_len;
static char *outtmp;
static bool remove_tmp, toupper_applied;
static uint32_t opus_sno;
static FILE *fpout;

static char const
	*OpusHead = "\x4f\x70\x75\x73\x48\x65\x61\x64",
	*OpusTags = "\x4f\x70\x75\x73\x54\x61\x67\x73";

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

static uint8_t *gbuf;
static size_t const gbuflen = 65307; // oggページの最大長
static void prepare_gbuf(void) {
	if (gbuf) return;
	gbuf = malloc(gbuflen);
}

static void put_left(void) {
	size_t l;
	prepare_gbuf();
	
	clearerr(fpopus);
	if (fseek(fpopus, seeked_len, SEEK_SET)) {
		oserror();
	}
	
	while (l = fread(gbuf, 1, gbuflen, fpopus)) {
		size_t ret = fwrite(gbuf, 1, l, fpout);
		if (ret != l) {
			oserror();
		}
	}
	if (ferror(fpopus)) {
		oserror();
	}
	free(gbuf);
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

static int opus_idx_diff;
static void store_tags(size_t lastpagelen) {
	size_t len;
	uint32_t tmp32;
	len = tagnum_edit + tagnum_file;
	tmp32 = oi32(len);
	fwrite(&tmp32, 1, 4, fptag);
	
	prepare_gbuf();
	rewind(fpedit);
	while ((len = fread(gbuf, 1, gbuflen, fpedit))) {
		fwrite(gbuf, 1, len, fptag);
	}
	fclose(fpedit);
	if (preserved_padding) {
		rewind(preserved_padding);
		while ((len = fread(gbuf, 1, gbuflen, preserved_padding))) {
			fwrite(gbuf, 1, len, fptag);
		}
		fclose(preserved_padding);
	}
	long commentlen = ftell(fptag);
	if (commentlen > TAG_LENGTH_LIMIT__OUTPUT) {
		mainerror(catgets(catd, 2, 10, "tag length exceeded the limit of storing (up to %u MiB)"), TAG_LENGTH_LIMIT__OUTPUT >> 20);
	}
	
	rewind(fptag); 
	ogg_page og;
	og.header_len = 282;
	og.header = gbuf;
	og.body_len = 255 * 255;
	og.body = gbuf + 282;
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
		// 余談: ページ番号を埋めるだけなら長さ0のページを作るのも合法だが
		// そうするとエラーになるアプリケーションが多かったので無の生成方式に落ち着いた
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
	opus_idx_diff = idx - opus_idx;
}

static void cleanup(void) {
	if (remove_tmp) {
		unlink(outtmp);
	}
}

static void exit_without_sigpipe(void) {
	if (!error_on_thread) {
		return;
	}
	signal(SIGPIPE, SIG_IGN);
}

void discontinuous_page(int page) {
	opuserror(err_opus_discontinuous, page, opus_idx);
}

static int test_break(ogg_page *og) {
	uint16_t plen;
	switch (page_breaks(og, 1, &plen)) {
	case 0:
		return -1;
	case 1:
		if (plen == og->body_len) return plen;
	}
	opuserror(err_opus_border);
}

static void parse_header(ogg_page *og) {
	opus_sno = ogg_page_serialno(og);
	if (!ogg_page_bos(og) || ogg_page_eos(og)) {
		opuserror(err_opus_bad_stream);
	}
	if (ogg_page_pageno(og) != 0) {
		discontinuous_page(ogg_page_pageno(og));
	}
	if (test_break(og) < 0) {
		opuserror(err_opus_border);
	}
	if (og->body_len < 8 || memcmp(og->body, OpusHead, 8) != 0) {
		opuserror(err_opus_non_opus);
	}
	if (og->body_len < 19) {
		opuserror(err_opus_bad_content);
	}
	if ((og->body[8] & 0xf0) != 0) {
		opuserror(err_opus_version);
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
	opus_idx++;
	opst = OPUS_HEADER_BORDER;
}

static int retriever_fd;
static void copy_tag_packet(ogg_page *og) {
	static unsigned int total = 0;
	total += og->body_len;
	if (total > TAG_LENGTH_LIMIT__INPUT) {
		opuserror(err_opus_long_tag, TAG_LENGTH_LIMIT__INPUT >> 20);
	}
	write(retriever_fd, og->body, og->body_len);
}

static pthread_t retriever_thread;
void *retrieve_tags(void*);
static void parse_header_border(ogg_page *og) {
	if (ogg_page_serialno(og) != opus_sno) {
		opuserror(err_opus_multi);
	}
	if (ogg_page_pageno(og) != 1) {
		discontinuous_page(ogg_page_pageno(og));
	}
	if (ogg_page_bos(og) || ogg_page_eos(og)) {
		opuserror(err_opus_bad_stream);
	}
	if (ogg_page_continued(og)) {
		opuserror(err_opus_border);
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
	
	atexit(exit_without_sigpipe);
	error_on_thread = true;
	
	// OpusTagsパケットパースを別スレッド化 retrieve_tags.c へ
	int pfd[2];
	pipe(pfd);
	retriever_fd = pfd[1];
	FILE *retriever = fdopen(pfd[0], "r");
	pthread_create(&retriever_thread, NULL, retrieve_tags, retriever);
	
	// 本スレッドはOpusTagsのパケットを構築する
	opst = test_break(og) < 0 ? OPUS_COMMENT : OPUS_COMMENT_BORDER;
	copy_tag_packet(og);
	opus_idx++;
}

static void parse_comment(ogg_page *og) {
	if (ogg_page_serialno(og) != opus_sno) {
		opuserror(err_opus_multi);
	}
	if (ogg_page_pageno(og) != opus_idx) {
		discontinuous_page(ogg_page_pageno(og));
	}
	if (ogg_page_bos(og) || ogg_page_bos(og) || !ogg_page_continued(og)) {
		opuserror(err_opus_bad_stream);
	}
	opst = test_break(og) < 0 ? OPUS_COMMENT : OPUS_COMMENT_BORDER;
	copy_tag_packet(og);
	opus_idx++;
}

static void parse_comment_border(ogg_page *og) {
	if (ogg_page_continued(og)) {
		opuserror(err_opus_border);
	}
	// タグパケットのパース処理のスレッドを合流
	close(retriever_fd);
	pthread_join(retriever_thread, NULL);
	error_on_thread = false;
	
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
		exit(0);
	}
	else {
		store_tags(og->header_len + og->body_len);
	}
	opst = OPUS_SOUND;
}

static void parse_page_sound(ogg_page *og) {
	if (ogg_page_serialno(og) == opus_sno) {
		*(uint32_t*)&og->header[18] = oi32(ogg_page_pageno(og) + opus_idx_diff);
		ogg_page_checksum_set(og);
	}
	write_page(og);
	if (ogg_page_serialno(og) == opus_sno && ogg_page_eos(og)) {
		put_left();
		/* NOTREACHED */
	}
}

static void parse_page(ogg_page *og) {
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
