#include <stdbool.h>

void errorprefix(void);
noreturn void mainerror(int, ...);
noreturn void opuserror(int, ...);
noreturn void oserror(void);
noreturn void oserror_fmt(char const*, ...);
noreturn void fileerror(char const*);
noreturn void opterror(int, char const*, ...);

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

noreturn void exceed_output_limit(void);
