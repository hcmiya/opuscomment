#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#include "opuscomment.h"
#include "global.h"

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


static bool rtcopy_write(FILE *fp, void *fptag_) {
	FILE *fptag = fptag_;
	uint32_t len = rtchunk(fp);
	uint8_t buf[STACK_BUF_LEN];
	bool first = true;
	bool field = true;
	bool copy;
	while (len) {
		size_t rl = rtfill(buf, len, STACK_BUF_LEN, fp);
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
static bool rtcopy_delete(FILE *fp, void *fptag_) {
	static int idx = 1;
	FILE *fptag = fptag_;
	FILE *src = rtcd_src, *dstr = dellist_str, *dlen = dellist_len;
	
	// 一旦レコードを全て一時ファイルsrcに保存し、-dの引数と一つずつ比較していく
	rewind(src);
	uint32_t srclen = rtchunk(fp);
	uint8_t buf[STACK_BUF_LEN];
	size_t len = srclen;
	while (len) {
		// フィールド名にいる間のループ
		bool field = true;
		size_t rl = rtfill(buf, len, STACK_BUF_LEN, fp);
		test_tag_field(buf, rl, true, &field, &upcase_applied);
		fwrite(buf, 1, rl, src);
		len -= rl;
		if (!field) break;
	}
	while (len) {
		// フィールド名を抜けた後のループ
		size_t rl = rtfill(buf, len, STACK_BUF_LEN, fp);
		fwrite(buf, 1, rl, src);
		len -= rl;
	}
	
	// 削除リストのループ
	rewind(dstr); rewind(dlen);
	bool copy = true;
	bool matched = false;
	while (fread(buf, 1, 5, dlen)) {
		uint32_t cmplen = *(uint32_t*)buf;
		bool field_only = buf[4];
		
		size_t const bufhalf = STACK_BUF_LEN / 2;
		uint8_t *cmp = buf + bufhalf;
		rewind(src);
		// 比較
		if (field_only && srclen > cmplen || cmplen == srclen) {
			// フィールド名のみ比較でソースが削除指定より長い または
			// 全比較でソースと削除の長さが一致する時
			matched = true;
			while (cmplen) {
				size_t rl = fill_buffer(buf, cmplen, bufhalf, src);
				fread(cmp, 1, rl, dstr);
				cmplen -= rl;
				if (memcmp(buf, cmp, rl) != 0) {
					matched = false;
					break;
				}
			}
			if (matched) {
				if (field_only) {
					fread(buf, 1, 1, src);
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
			cmplen -= fill_buffer(buf, cmplen, STACK_BUF_LEN, dstr);
		}
	}
	// 削除するものと一致しなかったらfptagにコピー
	rewind(src);
	*(uint32_t*)buf = oi32(srclen);
	fwrite(buf, 4, 1, fptag);
	check_tagpacket_length(4);
	while (srclen) {
		size_t rl = fill_buffer(buf, srclen, STACK_BUF_LEN, src);
		fwrite(buf, 1, rl, fptag);
		srclen -= rl;
		check_tagpacket_length(rl);
	}
	idx++;
MATCHED:
	return !matched;
}

static bool rtcopy_list(FILE *fp, void *listfd_) {
	static size_t idx = 1;
	int listfd = *(int*)listfd_;
	uint32_t len = rtchunk(fp);
	uint8_t buf[STACK_BUF_LEN];
	bool first = true;
	bool field = true;
	bool copy;
	while (len) {
		size_t rl = rtfill(buf, len, STACK_BUF_LEN, fp);
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
void *retrieve_tags(void *fp_) {
	// parse_header_border() からスレッド化された
	// fp はタグパケットの読み込みパイプ
	FILE *fp = fp_;
	uint8_t buf[STACK_BUF_LEN];
	
	struct rettag_st *rtn = calloc(1, sizeof(*rtn));
	
	rtread(buf, 8, fp);
	char const *OpusTags = "\x4f\x70\x75\x73\x54\x61\x67\x73";
	if (memcmp(buf, OpusTags, 8) != 0) {
		opuserror(err_opus_bad_content);
	}
	FILE *fptag = rtn->tag = tmpfile();
	fwrite(buf, 1, 8, fptag);
	
	// ベンダ文字列
	uint32_t len = rtchunk(fp);
	*(uint32_t*)buf = oi32(len);
	fwrite(buf, 4, 1, fptag);
	check_tagpacket_length(codec->commagic_len + 4);
	while (len) {
		size_t rl = rtfill(buf, len, STACK_BUF_LEN, fp);
		fwrite(buf, 1, rl, fptag);
		check_tagpacket_length(rl);
		len -= rl;
	}
	rtn->tagbegin = tagpacket_total;
	
	// レコード数
	size_t recordnum = rtn->del = rtchunk(fp);
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
		rtn->num += rtcopy(fp, wh);
		recordnum--;
	}
	rtn->del -= rtn->num;
	
	if (O.edit == EDIT_LIST) {
		// タグ出力スレッド合流
		close(pfd[1]);
		pthread_join(putth, NULL);
		exit(0);
	}
	if (rtcd_src) {
		fclose(rtcd_src); fclose(dellist_str); fclose(dellist_len);
	}
	
	len = fread(buf, 1, 1, fp);
	if (len && (*buf & 1)) {
		rtn->padding = tmpfile();
		fwrite(buf, 1, 1, rtn->padding);
		check_tagpacket_length(1);
		size_t n;
		while ((n = fread(buf, 1, STACK_BUF_LEN, fp))) {
			fwrite(buf, 1, n, rtn->padding);
			check_tagpacket_length(n);
		}
	}
	else {
		size_t n;
		while ((n = fread(buf, 1, STACK_BUF_LEN, fp))) {}
	}
	fclose(fp);
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
