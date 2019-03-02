#ifdef GLOBAL_MAIN
#define GLOBAL
#define GLOBAL_VAL(X) = X
#else
#define GLOBAL extern
#define GLOBAL_VAL(X)
#endif

#include <stdio.h>
#include <ogg/ogg.h>
#include <stdbool.h>
#include <stddef.h>

GLOBAL struct {
	enum {
		EDIT_NONE,
		EDIT_LIST,
		EDIT_WRITE,
		EDIT_APPEND,
	} edit;
	
	bool gain_fix;
	bool gain_relative;
	bool gain_not_zero;
	bool gain_q78;
	int gain_val;
	bool gain_val_sign;
	bool gain_put;
	
	bool tag_ignore_picture;
	bool tag_escape;
	bool tag_raw;
	bool tag_toupper;
	char *tag_filename;
	bool tag_deferred;
	bool tag_verify;
	bool tag_check_line_term;
	
	int target_idx;
	
	char *in, *out;
} O;

GLOBAL enum {
	PAGE_INFO,
	PAGE_INFO_BORDER,
	PAGE_COMMENT,
	PAGE_OTHER_METADATA,
	PAGE_SOUND,
} opst;

GLOBAL struct codec_parser {
	enum {
		CODEC_OPUS,
		CODEC_COMMON,
		CODEC_FLAC,
	} type;
	char const *prog, *name;
	size_t headmagic_len;
	char const *headmagic;
	void (*parse)(ogg_page*);
	size_t commagic_len;
	char const *commagic;
} *codec;

GLOBAL char const *program_name;
GLOBAL char const *program_name_default GLOBAL_VAL("opuscomment");

GLOBAL uint32_t opus_idx, opus_sno, opus_idx_diff;
GLOBAL bool leave_zero, non_opus_appeared;
GLOBAL bool error_on_thread;

GLOBAL FILE *fpopus, *fpout;

#ifdef NLS
#include <nl_types.h>
GLOBAL nl_catd catd;
#endif
