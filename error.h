#include <stdbool.h>

void errorprefix(void);
void mainerror(char const*, ...);
void opuserror(int, ...);
void oserror(void);
void oserror_fmt(char const*, ...);
void fileerror(char const*);

enum err_opus_ {
	err_opus_non_ogg = 1,
	err_opus_interrupted = 2,
	err_opus_border = 3,
	err_opus_non_opus = 4,
	err_opus_bad_stream = 5,
	err_opus_version = 6,
	err_opus_multi = 7,
	err_opus_utf8 = 8,
	err_opus_long_tag = 9,
	err_opus_disconsecutive = 10,
	err_opus_lost_tag = 11,
	err_opus_bad_content = 12,
};
