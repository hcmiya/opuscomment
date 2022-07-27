#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "opuscomment.h"

void errorprefix(void)
{
    fprintf(stderr, catgets(catd, 1, 3, "%s: "), program_name);
}

noreturn void mainerror(int e, ...)
{
    va_list ap;
    va_start(ap, e);
    errorprefix();
    char const *msg[] =
    {
#define LIST(I, E, S) S,
#include "errordef/opuscomment.tab"
#undef LIST
    };
    vfprintf(stderr, catgets(catd, 2, e, msg[e]), ap);
    fputc('\n', stderr);
    exit(1);
}

noreturn void opuserror(int e, ...)
{
    va_list ap;
    va_start(ap, e);
    errorprefix();

    struct
    {
        bool report_page;
        char const *default_message;
    } msg[] =
    {
#define LIST(I, B, E, S) {B, S},
#include "errordef/opus.tab"
#undef LIST
    };
    if (msg[e].report_page && codec->type != CODEC_FLAC)
    {
        fprintf(stderr, catgets(catd, 1, 4, "%s format error: page %u: "), codec->name, opus_idx);
    }
    else
    {
        fprintf(stderr, catgets(catd, 1, 8, "%s format error: "), codec->name);
    }
    vfprintf(stderr, catgets(catd, 3, e, msg[e].default_message), ap);
    fputc('\n', stderr);
    exit(2);
}

noreturn void oserror(void)
{
    perror(program_name);
    exit(3);
}

noreturn void oserror_fmt(char const *e, ...)
{
    va_list ap;
    va_start(ap, e);
    errorprefix();
    vfprintf(stderr, e, ap);
    fputc('\n', stderr);
    exit(3);
}

noreturn void fileerror(char const *file)
{
    errorprefix();
    fprintf(stderr, catgets(catd, 1, 5, "%s: "), file);
    perror(NULL);
    exit(3);
}

noreturn void opterror(int c, char const *e, ...)
{
    va_list ap;
    va_start(ap, e);
    errorprefix();
    fprintf(stderr, catgets(catd, 1, 7, "-%c: "), c);
    vfprintf(stderr, e, ap);
    fputc('\n', stderr);
    exit(1);
}


noreturn void exceed_output_limit(void)
{
    mainerror(err_main_output_limit, TAG_LENGTH_LIMIT__OUTPUT >> 20);
}
