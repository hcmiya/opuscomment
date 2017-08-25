#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "global.h"

void errorprefix(void) {
	fprintf(stderr, catgets(catd, 1, 3, "%s: "), program_name);
}

void mainerror(char const *e, ...) {
	va_list ap;
	va_start(ap, e);
	errorprefix();
	vfprintf(stderr, e, ap);
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
#define LIST(I, B, S) {B, S},
		LIST(  0, false, NULL )
		LIST(  1, false, "not an Ogg" )
		LIST(  2, true,  "header is interrupted" )
		LIST(  3, true,  "unexpected page break" )
		LIST(  4, false, "not an Opus" )
		LIST(  5, true,  "invalid Ogg stream" )
		LIST(  6, false, "unsupported version" )
		LIST(  7, false, "not supported for multiple logical stream" )
		LIST(  8, false, "invalid UTF-8 sequence at tag record #%d" )
		LIST(  9, true,  "tag packet is too long (up to %u MiB)" )
		LIST( 10, false, "discontinuous page - encountered p. %u against expectation of p. %u" )
		LIST( 11, false, "tag packet is incomplete" )
		LIST( 12, true,  "invalid header content" )
		LIST( 13, false, "invalid tag format at #%d" )
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
	fprintf(stderr, catgets(catd, 1, 5, "%s: "), program_name, file);
	perror(NULL);
	exit(3);
}
