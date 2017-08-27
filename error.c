#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "global.h"
#include "error.h"
#include "limit.h"

void errorprefix(void) {
	fprintf(stderr, catgets(catd, 1, 3, "%s: "), program_name);
}

void mainerror(int e, ...) {
	va_list ap;
	va_start(ap, e);
	errorprefix();
	char const *msg[] = {
#define LIST(I, E, S) S,
#include "errordef/main.tab"
#undef LIST
	};
	vfprintf(stderr, catgets(catd, 2, e, msg[e]), ap);
	fputc('\n', stderr);
	exit(1);
}

void opuserror(int e, ...) {
	va_list ap;
	va_start(ap, e);
	errorprefix();
	
	struct {
		bool report_page;
		char const *default_message;
	} msg[] = {
#define LIST(I, B, E, S) {B, S},
#include "errordef/opus.tab"
#undef LIST
	};
	if (msg[e].report_page) {
		fprintf(stderr, catgets(catd, 1, 4, "Opus format error: page %u: "), opus_idx);
	}
	else {
		fprintf(stderr, catgets(catd, 1, 8, "Opus format error: "));
	}
	vfprintf(stderr, catgets(catd, 3, e, msg[e].default_message), ap);
	fputc('\n', stderr);
	exit(2);
}

void oserror(void) {
	perror(program_name);
	exit(3);
}

void oserror_fmt(char const *e, ...) {
	va_list ap;
	va_start(ap, e);
	errorprefix();
	vfprintf(stderr, e, ap);
	fputc('\n', stderr);
	exit(3);
}

void fileerror(char const *file) {
	errorprefix();
	fprintf(stderr, catgets(catd, 1, 5, "%s: "), file);
	perror(NULL);
	exit(3);
}

void opterror(int c, char const *e, ...) {
	va_list ap;
	va_start(ap, e);
	errorprefix();
	fprintf(stderr, catgets(catd, 1, 7, "-%c: "), c);
	vfprintf(stderr, e, ap);
	fputc('\n', stderr);
	exit(1);
}


void exceed_output_limit(void) {
	mainerror(err_main_output_limit, TAG_LENGTH_LIMIT__OUTPUT >> 20);
}
