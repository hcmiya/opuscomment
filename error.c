#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "global.h"

void mainerror(char const *e, ...) {
	va_list ap;
	va_start(ap, e);
	fprintf(stderr, "%s: ", program_name);
	vfprintf(stderr, e, ap);
	fputc('\n', stderr);
	exit(1);
}

void opuserror(char const *e, ...) {
	va_list ap;
	va_start(ap, e);
	fprintf(stderr, "%s: %s: ", program_name, "Opusフォーマットエラー");
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
	fprintf(stderr, "%s: ", program_name);
	vfprintf(stderr, e, ap);
	fputc('\n', stderr);
	exit(3);
}

void fileerror(char const *file) {
	fprintf(stderr, "%s: %s: ", program_name, file);
	perror(NULL);
	exit(3);
}

