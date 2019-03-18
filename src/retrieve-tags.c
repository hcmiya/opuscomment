#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#include "opuscomment.h"

static void rtread(void *p, size_t len, FILE *fp) {
	size_t readlen = fread(p, 1, len, fp);
	if (readlen != len) opuserror(err_opus_lost_tag);
}

static uint32_t rtchunk(FILE *fp) {
	uint32_t rtn;
	rtread(&rtn, 4, fp);
	return oi32(rtn);
}

static size_t rtfill(void *buf, size_t left, size_t buflen, FILE *fp) {
	size_t rl = left > buflen ? buflen : left;
	rtread(buf, rl, fp);
	return rl;
}

static bool test_mbp(uint8_t *buf, size_t len) {
	// "METADATA_BLOCK_PICTURE=" in ASCII
	uint8_t const *mbp = "\x4d\x45\x54\x41\x44\x41\x54\x41\x5f\x42\x4c\x4f\x43\x4b\x5f\x50\x49\x43\x54\x55\x52\x45\x3d";
	size_t const mbplen = 23;
	if (len < mbplen) {
		return false;
	}
	while (*mbp) {
		uint8_t c = *buf++;
		if (c >= 0x61 && c <= 0x7a) { // a-z in ASCII
			c -= 32;
		}
		if (*mbp != c) {
			break;
		}
		mbp++;
	}
	return !*mbp;
}

static size_t tagpacket_total = 0;
void check_tagpacket_length(size_t len) {
	tagpacket_total += len;
	if (tagpacket_total > TAG_LENGTH_LIMIT__OUTPUT) {
		exceed_output_limit();
	}
}


static bool rtcopy_write(FILE *packet_input, void *fptag_) {
	FILE *fptag = fptag_;
	uint32_t len = rtchunk(packet_input);
	uint8_t buf[STACK_BUF_LEN];
	bool first = true;
	bool field = true;
	bool copy;
	while (len) {
		size_t rl = rtfill(buf, len, STACK_BUF_LEN, packet_input);
		if (first) {
			first = false;
			
			switch (O.edit) {
			case EDIT_WRITE:
				copy = O.tag_ignore_picture && test_mbp(buf, len);
				break;
			case EDIT_APPEND:
				copy = true;
				break;
			}
			if (copy) {
				uint8_t chunk[4];
				*(uint32_t*)chunk = oi32(len);
				fwrite(chunk, 4, 1, fptag);
				check_tagpacket_length(4);
			}
		}
		if (copy) {
			if (O.tag_toupper && field) {
				for (uint8_t *p = buf, *endp = buf + rl; p < endp; p++) {
					if (*p >= 0x61 && *p <= 0x7a) {
						*p -= 32;
					}
					if (*p == 0x3d) {
						field = false;
						break;
					}
				}
			}
			fwrite(buf, 1, rl, fptag);
			check_tagpacket_length(rl);
		}
		len -= rl;
	}
	return copy;
}

static FILE *rtcd_src, *dellist_str, *dellist_len;
static bool upcase_applied;
static bool rtcopy_delete(FILE *packet_input, void *fptag_) {
	static int idx = 1;
	FILE *fptag = fptag_;
	
	// -dの引数と一つずつ比較していくための準備として、レコード1つを一時ファイルrtcd_srcに書き出す
	rewind(rtcd_src);
	uint32_t srclen = rtchunk(packet_input);
	uint8_t buf[STACK_BUF_LEN];
	size_t len = srclen;
	while (len) {
		// フィールド名を大文字化する
		bool field = true;
		size_t rl = rtfill(buf, len, STACK_BUF_LEN, packet_input);
		test_tag_field(buf, rl, true, &field, &upcase_applied);
		fwrite(buf, 1, rl, rtcd_src);
		len -= rl;
		if (!field) break;
	}
	while (len) {
		// フィールド名を抜けた後のループ
		size_t rl = rtfill(buf, len, STACK_BUF_LEN, packet_input);
		fwrite(buf, 1, rl, rtcd_src);
		len -= rl;
	}
	
	// 削除リストのループ
	rewind(dellist_str); rewind(dellist_len);
	bool matched = false;
	while (fread(buf, 1, 5, dellist_len)) {
		uint32_t cmplen = *(uint32_t*)buf;
		bool field_only = buf[4];
		
		size_t const bufhalf = STACK_BUF_LEN / 2;
		uint8_t *cmp = buf + bufhalf;
		rewind(rtcd_src);
		// 比較
		if (field_only && srclen > cmplen || cmplen == srclen) {
			// フィールド名のみ比較でソースが削除指定より長い または
			// 全比較でソースと削除の長さが一致する時
			matched = true;
			while (cmplen) {
				size_t rl = fill_buffer(buf, cmplen, bufhalf, rtcd_src);
				fread(cmp, 1, rl, dellist_str);
				cmplen -= rl;
				if (memcmp(buf, cmp, rl) != 0) {
					matched = false;
					break;
				}
			}
			if (matched) {
				if (field_only) {
					fread(buf, 1, 1, rtcd_src);
					if (*buf == 0x3d) {
						goto MATCHED;
					}
					matched = false;
				}
				else {
					goto MATCHED;
				}
			}
		}
		// 次の削除チャンクに進む
		while (cmplen) {
			cmplen -= fill_buffer(buf, cmplen, STACK_BUF_LEN, dellist_str);
		}
	}
	// 削除するものと一致しなかったらfptagにコピー
	rewind(rtcd_src);
	*(uint32_t*)buf = oi32(srclen);
	fwrite(buf, 4, 1, fptag);
	check_tagpacket_length(4);
	while (srclen) {
		size_t rl = fill_buffer(buf, srclen, STACK_BUF_LEN, rtcd_src);
		fwrite(buf, 1, rl, fptag);
		srclen -= rl;
		check_tagpacket_length(rl);
	}
	idx++;
MATCHED:
	return !matched;
}

static bool rtcopy_list(FILE *packet_input, void *listfd_) {
	static size_t idx = 1;
	int listfd = *(int*)listfd_;
	uint32_t len = rtchunk(packet_input);
	uint8_t buf[STACK_BUF_LEN];
	bool first = true;
	bool field = true;
	bool copy;
	while (len) {
		size_t rl = rtfill(buf, len, STACK_BUF_LEN, packet_input);
		if (first) {
			if (*buf == 0x3d) opuserror(err_opus_bad_tag, idx);
			first = false;
			copy = !O.tag_ignore_picture || !test_mbp(buf, len);
			if (copy) {
				write(listfd, &len, 4);
			}
		}
		if (copy) {
			if (field) {
				if(!test_tag_field(buf, rl, O.tag_toupper, &field, &upcase_applied) && O.tag_verify) {
					opuserror(err_opus_bad_tag, idx);
				}
			}
			write(listfd, buf, rl);
		}
		len -= rl;
	}
	if (O.tag_verify && field) {
		opuserror(err_opus_bad_tag, idx);
	}
	idx++;
	return copy;
}

void *put_tags(void*);
void *retrieve_tags(void *packet_input_) {
	// parse_header_border() からスレッド化された
	FILE *packet_input = packet_input_;
	uint8_t buf[STACK_BUF_LEN];
	
	struct rettag_st *rtn = calloc(1, sizeof(*rtn));
	
	FILE *fptag = rtn->tag = tmpfile();
	rtn->part_of_comment = true;
	uint32_t len;
	rtread(buf, codec->commagic_len, packet_input);
	if (memcmp(buf, codec->commagic, codec->commagic_len) != 0) {
		if (codec->type == CODEC_VP8 && buf[5] != 2) {
			len = codec->commagic_len;
			rtn->part_of_comment = false;
		}
		else opuserror(err_opus_bad_content);
	}
	fwrite(buf, 1, codec->commagic_len, fptag);
	if (!rtn->part_of_comment) {
		// VP8でコメントパケットが無かった時用
		if (O.edit == EDIT_LIST) exit(0);
		uint8_t buf2[8] = "";
		fwrite(buf2, 1, 8, fptag);
		rtn->tagbegin = codec->commagic_len + 4;
		goto NOTCOMMENT;
	}
	
	// ベンダ文字列
	len = rtchunk(packet_input);
	*(uint32_t*)buf = oi32(len);
	fwrite(buf, 4, 1, fptag);
	check_tagpacket_length(codec->commagic_len + 4);
	while (len) {
		size_t rl = rtfill(buf, len, STACK_BUF_LEN, packet_input);
		fwrite(buf, 1, rl, fptag);
		check_tagpacket_length(rl);
		len -= rl;
	}
	rtn->tagbegin = tagpacket_total;
	
	// レコード数
	size_t recordnum = rtn->del = rtchunk(packet_input);
	fwrite(buf, 4, 1, fptag); // レコード数埋め(後でstore_tags()で書き換え)
	check_tagpacket_length(4);
	bool (*rtcopy)(FILE*, void*);
	int pfd[2];
	pthread_t putth;
	void *wh;
	if (O.edit == EDIT_LIST) {
		// タグ出力をスレッド化 put-tags.c へ
		pipe(pfd);
		FILE *fpput = fdopen(pfd[0], "r");
		pthread_create(&putth, NULL, put_tags, fpput);
		rtcopy = rtcopy_list;
		wh = pfd + 1;
	}
	else if (dellist_str) {
		rtcopy = rtcopy_delete;
		wh = fptag;
		rtcd_src = tmpfile();
	}
	else {
		rtcopy = rtcopy_write;
		wh = fptag;
	}
	
	while (recordnum) {
		rtn->num += rtcopy(packet_input, wh);
		recordnum--;
	}
	rtn->del -= rtn->num;
	
	if (O.edit == EDIT_LIST) {
		// タグ出力スレッド合流
		close(pfd[1]);
		pthread_join(putth, NULL);
		exit(0);
	}
	
	len = fread(buf, 1, 1, packet_input);
	if (len && (codec->prog || (*buf & 1))) {
		// codec->prog ← opuscomment 以外は全部パディングを保存
	NOTCOMMENT:
		rtn->padding = tmpfile();
		fwrite(buf, 1, len, rtn->padding);
		check_tagpacket_length(len);
		size_t n;
		while ((n = fread(buf, 1, STACK_BUF_LEN, packet_input))) {
			fwrite(buf, 1, n, rtn->padding);
			check_tagpacket_length(n);
		}
	}
	else {
		size_t n;
		while ((n = fread(buf, 1, STACK_BUF_LEN, packet_input))) {}
	}
	
	if (rtcd_src) {
		fclose(rtcd_src);
	}
	if (dellist_str) {
		fclose(dellist_str); fclose(dellist_len);
	}
	fclose(packet_input);
	rtn->upcase = upcase_applied;
	return rtn;
}


void rt_del_args(uint8_t *buf, size_t len, bool term) {
	static uint32_t recordlen = 0;
	static bool field = true;
	if (!buf) {
		if (!recordlen) {
			opterror('d', catgets(catd, 7, 3, "invalid tag format"));
		}
		recordlen = oi32(recordlen);
		fwrite(&recordlen, 4, 1, dellist_len);
		uint8_t f = field;
		fwrite(&f, 1, 1, dellist_len);
		recordlen = 0;
		field = true;
		return;
	}
	dellist_len = dellist_len ? dellist_len : tmpfile();
	dellist_str = dellist_str ? dellist_str : tmpfile();
	
	len -= term;
	recordlen += len;
	if (field && !test_tag_field(buf, len, true, &field, &upcase_applied)) {
		opterror('d', catgets(catd, 7, 3, "invalid tag format"));
	}
	fwrite(buf, 1, len, dellist_str);
}
