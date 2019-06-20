#ifndef OPUSCOMMENT_H
#define OPUSCOMMENT_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <string.h>
#include <strings.h>
#include <langinfo.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <signal.h>
#include <iconv.h>
#include <ogg/ogg.h>

#if __STDC_VERSION__ >= 201112L
#include <stdnoreturn.h>
#else
#define noreturn
#endif

#ifdef NLS
#include <nl_types.h>
#else
#define catgets(catd, set_id, msg_id, s) (s)
#endif

#include "global.h"
#include "ocutil.h"
#include "limit.h"
#include "error.h"

struct rettag_st {
	FILE *tag, *padding;
	long tagbegin;
	size_t num, del;
	bool upcase, part_of_comment;
};
struct edit_st {
	FILE *str, *len, *pict;
	size_t num;
};

// parse-tags.c
void parse_opt_tag(int, char const*);
void pticonv_close(void);
void *parse_tags(void*);

// put-tags.c
iconv_t iconv_new(char const *to, char const *from);
void *put_tags(void*);
void tag_output_close(void);

// read-flac.c
void read_flac(void);

// read.c
void read_page(ogg_sync_state*);
void move_file(void);
void open_output_file(void);
void store_tags(ogg_page *np, struct rettag_st *rst, struct edit_st *est, bool packet_break_in_page);

// retrieve-tags.c
void *retrieve_tags(void*);
void rt_del_args(uint8_t *buf, size_t len, bool term);

#endif
