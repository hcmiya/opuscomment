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
#include <errno.h>
#include <limits.h>

#define GLOBAL_MAIN
#include "opuscomment.h"
#include "version.h"

static struct codec_parser const *default_codec;
static void usage(void) {
	char revision[64];
	strftime(revision, 64, "%x", &(struct tm){
		.tm_year = OPUSCOMMENT_REVISION_YEAR,
		.tm_mon = OPUSCOMMENT_REVISION_MONTH,
		.tm_mday = OPUSCOMMENT_REVISION_DAY,
	});
	fprintf(stderr, catgets(catd, 6, 3, "Version: %s (rev. %s)\n"), OPUSCOMMENT_VERSION, revision);
	fputc('\n', stderr);
	
	fprintf(stderr, catgets(catd, 6, 4,
"Synopsys:\n"
"    %1$s [-l] [-C codec] [-i idx] [-DepQRUv] srcfile\n"
"    %1$s {-a|-w} [-C codec] [-i idx] [-g gain|-s scale] [-0e] [-c tagfile] [-t NAME=VALUE ...] [-d NAME[=VALUE] ...] [-1DQprRUv] srcfile [output]\n"
"    %1$s [-h]\n"
"\n"
"Options:\n"
"    -l    List mode\n"
"    -a    Append mode\n"
"    -w    Write mode\n"
"    -h    Print this message and exit\n"
"    -C codec\n"
"          Select codec\n"
"    -i idx\n"
"          Specify %2$s index for editing in multiplexed Ogg stream\n"
"          (1-origin, without non-%2$s stream)\n"
"    -R    Assume editing IO to be encoded in UTF-8\n"
"    -e    Use escape sequence; \\\\, \\n, \\r and \\0\n"
"    -0    Use '\\0' separation for editing IO\n"
"    -t NAME=VALUE\n"
"          Add the argument as editing item\n"
"    -c tagfile\n"
"          In list mode, write tags to tagfile.\n"
"          In append/write mode, read tags from tagfile.\n"
"    -d NAME[=VALUE]\n"
"          Delete tags matched with the argument from srcfile.\n"
"          When VALUE is omitted, All of NAME is removed. Implies -U\n"
"    -p    Supress editing for METADATA_BLOCK_PICTURE\n"
"    -U    Convert field name stored in srcfile to uppercase\n"
"    -V    Verify Tags stored in srcfile\n"
"    -T    Error when editing input is not terminated by line feed\n"
"    -D    Defer editing IO; implies -V, -T\n"
	), program_name, default_codec->name);
	if (!default_codec->prog) {
		fprintf(stderr, catgets(catd, 6, 5,
"    -g gain\n"
"          Specify output gain in dB\n"
"    -s scale\n"
"          Specify output gain in scale for PCM samples.\n"
"          1 for same scale. 0.5 for half.\n"
"    -r    Specify that the gain is relative to internal value\n"
"    -1    When output gain becomes 0 by converting to internal representation,\n"
"          set [+-]1/256 dB instead\n"
"    -Q    Use Q7.8 format for editing output gain\n"
"    -v    Put output gain to stderr\n"
		));
	}
	exit(1);
}

static void fail_to_parse(int c) {
	opterror(c, catgets(catd, 7, 1, "failed to parse value"));
}

static void out_of_range(int c) {
	opterror(c, catgets(catd, 7, 2, "the value is out of range"));
}

void select_codec(void);
bool select_codec_by_name(char const*);
void set_parser_type(void);

static void parse_args(int argc, char **argv) {
	int c;
	bool added_tag = false, del_tag = false;
	int gainfmt;
	double gv;
	
	while ((c = getopt(argc, argv, "01ac:C:d:Deg:hi:lpqQrRs:t:TUvVw")) != -1) {
		switch (c) {
		case 'l':
			O.edit = EDIT_LIST;
			break;
			
		case 'w':
			O.edit = EDIT_WRITE;
			break;
			
		case 'a':
			O.edit = EDIT_APPEND;
			break;
			
		case 'h':
			usage();
			break;
			
		case 'g':
		case 's':
			gainfmt = c;
			O.gain_fix = true;
			{
				char *endp;
				gv = strtod(optarg, &endp);
				if (optarg == endp) {
					fail_to_parse(c);
				}
				if (!isfinite(gv)) {
					out_of_range(c);
				}
				if (gv > 65536 || gv < -65536) {
					out_of_range(c);
				}
			}
			break;
			
		case 'r':
			O.gain_relative = true;
			break;
			
		case 'Q':
			O.gain_q78 = true;
			break;
			
		case '1':
			O.gain_not_zero = true;
			break;
			
		case 'p':
			O.tag_ignore_picture = true;
			break;
			
		case 'R':
			O.tag_raw = true;
			break;
			
		case 'e':
			O.tag_escape = TAG_ESCAPE_BACKSLASH;
			break;
			
		case '0':
			O.tag_escape = TAG_ESCAPE_NUL;
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
			parse_opt_tag(c, optarg);
			added_tag = true;
			break;
			
		case 'd':
			parse_opt_tag(c, optarg);
			del_tag = true;
			break;
			
		case 'D':
			O.tag_deferred = true;
			O.tag_verify = true;
			O.tag_check_line_term = true;
			break;
		case 'T':
			O.tag_check_line_term = true;
			break;
		case 'V':
			O.tag_verify = true;
			break;
		case 'i':
			{
				char *endp;
				unsigned long val = strtoul(optarg, &endp, 10);
				if (endp == optarg || *endp != '\0') {
					fail_to_parse(c);
				}
				if (errno == ERANGE || val == 0 || val > INT_MAX) {
					out_of_range(c);
				}
				O.target_idx = (int)val;
			}
			break;
			
		case 'q':
			// 何もしない
			break;
			
		case 'C':
			if (!select_codec_by_name(optarg)) {
				opterror(c, catgets(catd, 7, 4, "unknown codec \"%s\""), optarg);
			}
			break;
			
		case '?':
			exit(1);
			break;
		}
	}
	// オプションループ抜け
	if (del_tag || added_tag) {
		pticonv_close();
	}
	if (del_tag) {
		switch (O.edit) {
		case EDIT_NONE:
			O.edit = EDIT_APPEND;
			break;
		case EDIT_APPEND:
			break;
		default:
			mainerror(err_main_del_without_a);
		}
	}
	if (added_tag && O.edit == EDIT_LIST) {
		mainerror(err_main_tag_with_l);
	}
	if (added_tag && !O.edit) {
		mainerror(err_main_tag_without_aw);
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
			mainerror(err_main_gain_with_l);
		}
		else if (!O.edit) {
			if (O.tag_filename/* || added_tag*/) {
				mainerror(err_main_tag_without_aw);
			}
		}
	}
	if (O.edit == EDIT_APPEND) {
		O.tag_ignore_picture = false;
	}
	if (codec->prog) {
		O.gain_fix = false;
		O.gain_put = false;
	}
}

static void interrupted(int sig) {
	exit(1);
}

static void read_ogg(void) {
	ogg_sync_state oy;
	ogg_sync_init(&oy);
	
	size_t buflen = 1 << 17;
	uint8_t *buf = ogg_sync_buffer(&oy, buflen);
	size_t len = fread(buf, 1, buflen, stream_input);
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
		len = fread(buf, 1, buflen, stream_input);
		if (!len) {
			break;
		}
		ogg_sync_wrote(&oy, len);
		read_page(&oy);
	}
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
	select_codec();
	default_codec = codec;
	
#ifdef NLS
#define co catd = catopen("opuscomment", NL_CAT_LOCALE)
#ifdef DEFAULT_NLS_PATH
	char const *nlspath = getenv("NLSPATH");
	if (nlspath && *nlspath) {
		co;
	}
	else {
		setenv("NLSPATH", DEFAULT_NLS_PATH, true);
		co;
		if (catd == (nl_catd)-1) {
			unsetenv("NLSPATH");
			co;
		}
	}
#else
	co;
#endif
#undef co
#endif
	if (argc == 1) usage();
	
	parse_args(argc, argv);
	if (!argv[optind]) {
		mainerror(err_main_no_file);
	}
	if (argv[optind + 1]) {
		if (O.edit == EDIT_LIST) {
			mainerror(err_main_many_args);
		}
		if (argv[optind + 2]) {
			mainerror(err_main_many_args);
		}
		O.out = argv[optind + 1];
	}
	
	O.in = argv[optind];
	stream_input = fopen(O.in, "r");
	if (!stream_input) {
		fileerror(O.in);
	}
	
	open_output_file();
	if (codec->type == CODEC_FLAC) read_flac();
	else read_ogg();
	
	if (opst < PAGE_SOUND) {
		opuserror(err_opus_interrupted);
	}
	
	move_file();
	return 0;
}
