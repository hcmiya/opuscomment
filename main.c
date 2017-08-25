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
#include <signal.h>

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
"    %1$s [-l] [-DepQRUv] opusfile\n"
"    %1$s -a|-w [-g gain|-s scale|-n] [-c tagfile] [-t NAME=VALUE ...] [-DeGQprRUv] opusfile [output]\n"
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
"    -s scale\n"
"          Specify output gain in scale for PCM samples.\n"
"          1 for same scale. 0.5 for half.\n"
"    -n    Set output gain to 0\n"
"    -r    Specify that the gain is relative to internal value\n"
"    -1    When output gain becomes 0 by converting to internal representation,\n"
"          set [+-]1/256 dB instead\n"
"    -v    Put output gain of BEFORE editing to stderr\n"
"    -Q    Use Q7.8 format for editing output gain\n"
"    -p    Supress editing for METADATA_BLOCK_PICTURE\n"
"    -U    Convert field name in Opus file to uppercase\n"
"    -c tagfile\n"
"          In list mode, write tags to tagfile.\n"
"          In append/write mode, read tags from tagfile\n"
"    -t NAME=VALUE\n"
"          add the argument as editing item\n"
"    -V    Verify Tags in source Opus file\n"
"    -D    Defer editing IO; implies -V in list mode\n"
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
	bool added_tag = false;
	int gainfmt;
	double gv;
	while ((c = getopt(argc, argv, "lwag:s:nrGvQpUc:t:VD")) != -1) {
		switch (c) {
		case 'g':
		case 's':
			gainfmt = c;
			O.gain_fix = true;
			{
				char *endp;
				gv = strtod(optarg, &endp);
				if (optarg == endp) {
					opterror(c, catgets(catd, 2, 3, "failed to parse gain value"));
				}
				if (!isfinite(gv)) {
					out_of_range(c);
				}
				if (gv > 65536 || gv < -65536) {
					out_of_range(c);
				}
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
			gainfmt = c;
			break;
			
		case 'Q':
			O.gain_q78 = true;
			break;
			
		case '1':
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
			O.gain_put = true;
			break;
			
		case 'c':
			O.tag_filename = optarg;
			break;
		
		case 't':
			add_tag_from_opt(optarg);
			added_tag = true;
			break;
			
		case 'D':
			O.tag_deferred = true;
			/* FALLTHROUGH */
		case 'V':
			O.tag_verify = true;
			break;
		}
	}
	// オプションループ抜け
	if (added_tag && O.edit == EDIT_LIST) {
		mainerror(catgets(catd, 2, 5, "can not use -t with list mode"));
	}
	if (added_tag && !O.edit) {
		mainerror(catgets(catd, 2, 6, "use -a|-w with editing tag"));
	}
	if (!O.gain_fix && !O.edit) {
		O.edit = EDIT_LIST;
	}
	else if (O.gain_fix) {
		switch (gainfmt) {
		case 'g':
			if (O.gain_q78) {
				O.gain_val = (int)trunc(gv);
			}
			else {
				O.gain_val = (int)trunc(gv * 256);
			}
			break;
		case 's':
			if (gv <= 0) {
				out_of_range(gainfmt);
			}
			O.gain_val = (int)trunc(20 * log10(gv) * 256);
			break;
		}
		O.gain_val_sign = signbit(gv);
		if (!O.gain_relative && (O.gain_val > 32767 || O.gain_val < -32768)) {
			out_of_range(gainfmt);
		}
		
		if (O.edit == EDIT_LIST) {
			mainerror(catgets(catd, 2, 7, "options of editing gain can not use with list mode"));
		}
		else if (!O.edit) {
			if (O.tag_filename/* || added_tag*/) {
				mainerror(catgets(catd, 2, 6, "use -a|-w with editing tag"));
			}
		}
	}
	if (O.edit == EDIT_APPEND) {
		O.tag_ignore_picture = false;
	}
}

static void interrupted(int sig) {
	exit(1);
}

int main(int argc, char **argv) {
	struct sigaction sa;
	sa.sa_handler = interrupted;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGINT);
	sigaddset(&sa.sa_mask, SIGQUIT);
	sigaddset(&sa.sa_mask, SIGTERM);
	sigaddset(&sa.sa_mask, SIGPIPE);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	
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
		opuserror(err_opus_non_ogg);
	}
	if (memcmp(buf, "\x4f\x67\x67\x53", 4) != 0) {
		opuserror(err_opus_non_ogg);
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
		opuserror(err_opus_interrupted);
	}
	
	move_file();
	return 0;
}
