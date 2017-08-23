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
		mainerror(catgets(catd, 2, 10, "tag length exceeded the limit of storing (up to %u MiB)"), TAG_LENGTH_LIMIT__OUTPUT >> 20);
	}
}

static bool rtcopy_write(FILE *fp, int nouse_) {
	uint32_t len = rtchunk(fp);
	uint8_t buf[512];
	bool first = true;
	bool field = true;
	bool copy;
	while (len) {
		size_t rl = len > 512 ? 512 : len;
		rtread(buf, rl, fp);
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
			fwrite(buf, 1, rl, fptag);
			check_tagpacket_length(rl);
		}
		len -= rl;
	}
	return copy;
}

static bool rtcopy_list(FILE *fp, int listfd) {
	uint32_t len = rtchunk(fp);
	uint8_t buf[512];
	bool first = true;
	bool field = true;
	bool copy;
	while (len) {
		size_t rl = len > 512 ? 512 : len;
		rtread(buf, rl, fp);
		if (first) {
			first = false;
			copy = !O.tag_ignore_picture || !test_mbp(buf, len);
			if (copy) {
				write(listfd, &len, 4);
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
			write(listfd, buf, rl);
		}
		len -= rl;
	}
	return copy;
}

void *put_tags(void*);
void *retrieve_tags(void *fp_) {
	// parse_header_border() からスレッド化された
	// fp はタグパケットの読み込みパイプ
	FILE *fp = fp_;
	uint8_t buf[512];
	
	rtread(buf, 8, fp);
	char const *OpusTags = "\x4f\x70\x75\x73\x54\x61\x67\x73";
	if (memcmp(buf, OpusTags, 8) != 0) {
		opuserror(err_opus_bad_content);
	}
	fptag = tmpfile();
	fwrite(buf, 1, 8, fptag);
	
	// ベンダ文字列
	uint32_t len = rtchunk(fp);
	*(uint32_t*)buf = oi32(len);
	fwrite(buf, 4, 1, fptag);
	check_tagpacket_length(12);
	while (len) {
		size_t rl = len > 512 ? 512 : len;
		rtread(buf, rl, fp);
		fwrite(buf, 1, rl, fptag);
		check_tagpacket_length(rl);
		len -= rl;
	}
	tagpacket_tagnumpos = tagpacket_total;
	
	// レコード数
	size_t recordnum = rtchunk(fp);
	fwrite(buf, 4, 1, fptag); // レコード数埋め(後でstore_tags()で書き換え)
	check_tagpacket_length(4);
	bool (*rtcopy)(FILE*, int);
	int pfd[2];
	pthread_t putth;
	if (O.edit == EDIT_LIST) {
		// タグ出力をスレッド化 put-tags.c へ
		pipe(pfd);
		FILE *fpput = fdopen(pfd[0], "r");
		pthread_create(&putth, NULL, put_tags, fpput);
		rtcopy = rtcopy_list;
	}
	else {
		rtcopy = rtcopy_write;
	}
	
	while (recordnum) {
		tagnum_file += rtcopy(fp, pfd[1]);
		recordnum--;
	}
	
	if (O.edit == EDIT_LIST) {
		// タグ出力スレッド合流
		close(pfd[1]);
		pthread_join(putth, NULL);
		exit(0);
	}
	
	len = fread(buf, 1, 1, fp);
	if (O.edit != EDIT_LIST && len && (*buf & 1)) {
		preserved_padding = tmpfile();
		fwrite(buf, 1, 1, preserved_padding);
		size_t n;
		while ((n = fread(buf, 1, 512, fp))) {
			fwrite(buf, 1, n, preserved_padding);
			check_tagpacket_length(n);
		}
	}
	else {
		size_t n;
		while ((n = fread(buf, 1, 512, fp))) {}
	}
	fclose(fp);
	return NULL;
}

