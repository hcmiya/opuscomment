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
	double gain_val;
	
	int info_gain;
	
	bool tag_ignore_picture;
	bool tag_escape;
	bool tag_raw;
	char *tag_filename;
	
	char *in, *out;
} O;

GLOBAL enum {
	OPUS_HEADER,
	OPUS_HEADER_BORDER,
	OPUS_COMMENT,
	OPUS_COMMENT_BORDER,
	OPUS_SOUND,
	OPUS_REMNANT,
} opst;

GLOBAL char const *program_name;
GLOBAL char const *program_name_default GLOBAL_VAL("opuscomment");

GLOBAL char *vendor;
GLOBAL char **tag_file, **tag_edit;
GLOBAL size_t tagnum_file, tagnum_edit;

GLOBAL FILE *fpopus;
