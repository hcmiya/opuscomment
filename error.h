#include <stdbool.h>

void errorprefix(void);
void mainerror(int, ...);
void opuserror(int, ...);
void oserror(void);
void oserror_fmt(char const*, ...);
void fileerror(char const*);
void opterror(int, char const*, ...);

enum err_opus_ {
#define LIST(I, B, E, S) err_opus_##E = I,
#include "errordef/opus.tab"
#undef LIST
};

enum err_main_ {
#define LIST(I, E, S) err_main_##E = I,
#include "errordef/main.tab"
#undef LIST
};

void exceed_output_limit(void);
