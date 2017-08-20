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

void opuserror(char const *e, ...) {
	va_list ap;
	va_start(ap, e);
	errorprefix();
	fputs(catgets(catd, 1, 4, "Opus format error: "), stderr);
	vfprintf(stderr, e, ap);
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
