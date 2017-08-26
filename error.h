#include <stdbool.h>

void errorprefix(void);
void mainerror(int, ...);
void opuserror(int, ...);
void oserror(void);
void oserror_fmt(char const*, ...);
void fileerror(char const*);

enum err_opus_ {
#define LIST(I, B, E, S) err_opus_##E = I,
#include "opuserror.tab"
#undef LIST
};

enum err_main_ {
#define LIST(I, E, S) err_main_##E = I,
#include "mainerror.tab"
#undef LIST
};

void exceed_output_limit(void);
