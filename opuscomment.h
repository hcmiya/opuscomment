
enum edit_ {
	EDIT_NONE,
	EDIT_LIST,
	EDIT_WRITE,
	EDIT_APPEND,
};

enum opst_ {
	OPUS_HEADER,
	OPUS_HEADER_BORDER,
	OPUS_COMMENT,
	OPUS_COMMENT_BORDER,
	OPUS_LEFT
};

#include <ogg/ogg.h>

void parse_tags(void);
void validate_tag(wchar_t*);
void add_tag(char*);
void put_tags(void);
void read_page(ogg_sync_state*);
void mainerror(char*, ...);
void opuserror(char*, ...);
void fileerror(void);
void move_file(void);
