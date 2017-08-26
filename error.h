#include <stdbool.h>

void errorprefix(void);
void mainerror(char const*, ...);
void opuserror(int, ...);
void oserror(void);
void oserror_fmt(char const*, ...);
void fileerror(char const*);

enum err_opus_ {
#define LIST(I, B, E, S) err_opus_##E = I,
#include "opuserror.tab"
#undef LIST
};
