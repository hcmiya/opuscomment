#include <ogg/ogg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <math.h>
#include <locale.h>
#include <stdarg.h>
#include <iconv.h>
#include <langinfo.h>
#include <time.h>

#include "opuscomment.h"
#define GLOBAL_MAIN
#include "global.h"
#include "version.h"

static void usage(void) {
	char revision[64];
	strftime(revision, 64, "%x", &(struct tm){
		.tm_year = OPUSCOMMENT_REVISION_YEAR,
		.tm_mon = OPUSCOMMENT_REVISION_MONTH,
		.tm_mday = OPUSCOMMENT_REVISION_DAY,
	});
	fprintf(stderr, catgets(catd, 6, 3, "Version: %s (rev. %s)\n"), OPUSCOMMENT_VERSION, revision);
	fputc('\n', stderr);
	fprintf(stderr, catgets(catd, 6, 1,
"Synopsys:\n"
"    %1$s [-l] [-epRUvV] opusfile\n"
"    %1$s -a|-w [-g gain|-s gain|-n] [-c tagfile] [-t NAME=VALUE ...] [-eGprRUvV] opusfile [output]\n"
	), program_name);
	fputc('\n', stderr);
	fputs(catgets(catd, 6, 2,
"Options:\n"
"    -l    List mode\n"
"    -a    Append mode\n"
"    -w    Write mode\n"
"    -R    Assume editing IO to be encoded in UTF-8\n"
"    -e    Use escape sequence; \\\\, \\n, \\r and \\0\n"
"    -g gain\n"
"          Specify output gain in dB\n"
"    -s gain\n"
"          Specify output gain in scale for PCM samples.\n"
"          1 for same scale. 0.5 for half.\n"
"    -n    Set output gain to 0\n"
"    -r    Specify that the gain is relative to internal value\n"
"    -G    When output gain becomes 0 by converting to internal representation,\n"
"          set [+-]1/256 dB instead\n"
"    -p    Supress editing for METADATA_BLOCK_PICTURE\n"
"    -U    Convert field name in Opus file to uppercase\n"
"    -v    Put output gain of BEFORE editing to stderr by following format:\n"
"          \"%.8g\\n\", <output gain in dB>\n"
"    -V    Put output gain of BEFORE editing to stderr by following format:\n"
"          \"%d\\n\", <output gain in Q7.8>\n"
"    -c tagfile\n"
"          In list mode, write tags to tagfile.\n"
"          In append/write mode, read tags from tagfile\n"
"    -t NAME=VALUE\n"
"          add the argument as editing item\n"
	), stderr);
	exit(1);
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

static void out_of_range(int c) {
	opterror(c, catgets(catd, 2, 3, "gain value is out of range"));
}

static void parse_args(int argc, char **argv) {
	int c;
	while ((c = getopt(argc, argv, "lwag:s:nrGpReUvVc:t:")) != -1) {
		switch (c) {
		case 'g':
		case 's':
			O.gain_fix = true;
			{
				double f;
				char *endp;
				f = strtod(optarg, &endp);
				if (optarg == endp) {
					opterror(c, catgets(catd, 2, 3, "failed to parse gain value"));
				}
				if (!isfinite(f)) {
					out_of_range(c);
				}
				if (c == 's') {
					if (f <= 0) {
						out_of_range(c);
					}
					f = 20 * log10(f);
				}
				if (f > 128 || f < -128) {
					out_of_range(c);
				}
				O.gain_val = f;
			}
			break;
			
		case '?':
			exit(1);
			break;
		}
		switch (c) {
		case 'r':
			O.gain_relative = true;
			break;
			
		case 'n':
			O.gain_fix = true;
			O.gain_relative = false;
			O.gain_val = 0;
			O.gain_not_zero = false;
			break;
			
		case 'G':
			O.gain_not_zero = true;
			break;
			
		case 'l':
			O.edit = EDIT_LIST;
			break;
			
		case 'w':
			O.edit = EDIT_WRITE;
			break;
			
		case 'a':
			O.edit = EDIT_APPEND;
			break;
			
		case 'p':
			O.tag_ignore_picture = true;
			break;
			
		case 'R':
			O.tag_raw = true;
			break;
			
		case 'e':
			O.tag_escape = true;
			break;
			
		case 'U':
			O.tag_toupper = true;
			break;
			
		case 'v':
			O.info_gain = 1;
			break;
			
		case 'V':
			O.info_gain = 2;
			break;
			
		case 'c':
			O.tag_filename = optarg;
			break;
		
		case 't':
			add_tag_from_opt(optarg);
			break;
		}
	}
	if (fpedit && O.edit == EDIT_LIST) {
		mainerror(catgets(catd, 2, 5, "can not use -t with list mode"));
	}
	if (fpedit && !O.edit) {
		mainerror(catgets(catd, 2, 6, "use -a|-w with editing tag"));
	}
	if (!O.gain_fix && !O.edit) {
		O.edit = EDIT_LIST;
	}
	else if (O.gain_fix) {
		if (O.edit == EDIT_LIST) {
			mainerror(catgets(catd, 2, 7, "options of editing gain can not use with list mode"));
		}
		else if (!O.edit) {
			if (O.tag_filename/* || fpedit*/) {
				mainerror(catgets(catd, 2, 6, "use -a|-w with editing tag"));
			}
		}
	}
	if (O.edit == EDIT_APPEND) {
		O.tag_ignore_picture = false;
	}
}

int main(int argc, char **argv) {
	setlocale(LC_ALL, "");
	if (*argv[0]) {
		char *p = strrchr(argv[0], '/');
		program_name = p ? p + 1 : argv[0];
	}
	else {
		program_name = program_name_default;
	}
#ifdef NLS
	catd = catopen("opuscomment.cat", NL_CAT_LOCALE);
#endif
	if (argc == 1) usage();
	
	parse_args(argc, argv);
	if (!argv[optind]) {
		mainerror(catgets(catd, 2, 8, "no file specified"));
	}
	if (argv[optind + 1]) {
		if (O.edit == EDIT_LIST) {
			mainerror(catgets(catd, 2, 9, "too many arguments"));
		}
		if (argv[optind + 2]) {
			mainerror(catgets(catd, 2, 9, "too many arguments"));
		}
		O.out = argv[optind + 1];
	}
	
	O.in = argv[optind];
	fpopus = fopen(O.in, "r");
	if (!fpopus) {
		fileerror(O.in);
	}
	
	uint8_t *fbp;
	ogg_sync_state oy;
	ogg_sync_init(&oy);
	
	size_t buflen = 1 << 17;
	uint8_t *buf = ogg_sync_buffer(&oy, buflen);
	size_t len = fread(buf, 1, buflen, fpopus);
	if (len == (size_t)-1) oserror();
	if (len < 4) {
		opuserror(false, catgets(catd, 3, 1, "not an Ogg"));
	}
	if (strncmp(buf, "\x4f\x67\x67\x53", 4) != 0) {
		opuserror(false, catgets(catd, 3, 1, "not an Ogg"));
	}
	ogg_sync_wrote(&oy, len);
	read_page(&oy);
	
	for (;;) {
		buf = ogg_sync_buffer(&oy, buflen);
		len = fread(buf, 1, buflen, fpopus);
		if (!len) {
			break;
		}
		ogg_sync_wrote(&oy, len);
		read_page(&oy);
	}
	
	if (opst < OPUS_SOUND) {
		opuserror(true, catgets(catd, 3, 2, "header is interrupted"));
	}
	
	move_file();
	return 0;
}
