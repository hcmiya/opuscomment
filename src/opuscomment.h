#ifndef OPUSCOMMENT_H
#define OPUSCOMMENT_H

#include <ogg/ogg.h>
#include <stdbool.h>
#include <iconv.h>

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

void parse_opt_tag(int, char const*);
void pticonv_close(void);
void read_page(ogg_sync_state*);
noreturn void read_flac(void);
noreturn void put_left(long rew);
void move_file(void);
iconv_t iconv_new(char const *to, char const *from);
void open_output_file(void);

void *retrieve_tags(void*);
void *parse_tags(void*);
void tag_output_close(void);

void store_tags(ogg_page *np, struct rettag_st *rst, struct edit_st *est, bool packet_break_in_page);

#endif
