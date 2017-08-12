#include <ogg/ogg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <getopt.h>
#include <math.h>
#include <locale.h>
#include <stdarg.h>
#include <iconv.h>
#include <langinfo.h>

#include "opuscomment.h"
#define GLOBAL_MAIN
#include "global.h"

static void usage(void) {
	fprintf(stderr,
"使い方:\n"
"    %1$s [-l] [-epRvV] opusfile\n"
"    %1$s -a|-w [-g gain|-s gain|-n] [-c file|-t NAME=VALUE ...] [-eGprRvV] opusfile [output]\n"
"\n"
"オプション:\n"
"    -l            タグ出力モード\n"
"    -a            タグ追記モード\n"
"    -w            タグ書き込みモード\n"
"    -R            タグ入出力にUTF-8を使う。このオプションがない場合はロケールによる文字符号化との変換が行われる\n"
"                  (vorbiscomment(1)互換)\n"
"    -e            バックスラッシュ、改行、復帰、ヌルにそれぞれ\\\\, \\n, \\r, \\0のエスケープを使用する\n"
"                  (vorbiscomment(1)互換)\n"
"    -g gain       出力ゲインをdBで指定する\n"
"    -s gain       出力ゲインをPCMサンプルの倍率で指定する。1で等倍。0.5で半分(コマンド内でdBに変換)\n"
"    -n            出力ゲインを0にする\n"
"    -r            出力ゲインの指定を内部の設定に対する相対値とする\n"
"    -G            出力ゲインが内部形式にした時に0になる場合は[-+]1/256 dBを設定する\n"
"    -p            METADATA_BLOCK_PICTUREの出力または削除をしない\n"
"    -v            出力ゲインの編集_前_の値を以下の形式で標準エラー出力に出力する\n"
"                  \"%%.8g\\n\", <output gain in dB, floating point>\n"
"    -V            出力ゲインの編集_前_の値を以下の形式で標準エラー出力に出力する\n"
"                  \"%%d\\n\", <output gain in Q7.8, integer>\n"
"    -c file       出力モード時、タグをfileに書き出す。書き込み・追記モード時、fileからタグを読み出す\n"
"                  複数回指定された時は最後の指定のみが有効\n"
"    -t NAME=VALUE 引数をタグとして追加する\n"
	, program_name);
	exit(1);
}

static void parse_args(int argc, char **argv) {
	int c;
	iconv_t cd = (iconv_t)-1;
	while ((c = getopt(argc, argv, "lwag:s:nrGpRevVc:t:")) != -1) {
		switch (c) {
			case 'g':
			case 's':
				O.gain_fix = true;
			{
				double f;
				char *endp;
				f = strtod(optarg, &endp);
				if (optarg == endp) {
					mainerror("ゲイン値のパース失敗");
				}
				if (!isfinite(f)) {
					mainerror("ゲイン値の範囲外");
				}
				if (c == 's') {
					if (f <= 0) {
						mainerror("ゲイン値の範囲外");
					}
					f = 20 * log10(f);
				}
				if (f > 128 || f < -128) {
					mainerror("ゲイン値の範囲外");
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
				{
					char *ls = optarg;
					size_t l = mbstowcs(NULL, ls, 0);
					if (l == (size_t)-1) {
						oserror();
					}
					wchar_t *str = malloc((l + 1) * sizeof(*str));
					mbstowcs(str, ls, l + 1);
					
					validate_tag(str);
					free(str);
					
					size_t u8left = l * 8;
					l = strlen(ls) + 1;
					char *u8, *u8buf;
					u8buf = u8 = malloc(u8left);
					if (cd == (iconv_t)-1) {
#if defined __GLIBC__ || defined _LIBICONV_VERSION
						cd = iconv_open("UTF-8//TRANSLIT", nl_langinfo(CODESET));
#else
						cd = iconv_open("UTF-8", nl_langinfo(CODESET));
#endif
						if (cd == (iconv_t)-1) {
							oserror_fmt("iconvが%s→UTF-8の変換に対応していない", nl_langinfo(CODESET));
						}
					}
					iconv(cd, &ls, &l, &u8, &u8left);
					for (u8 = u8buf; *u8 != 0x3d; u8++) {
						if (*u8 >= 0x61 && *u8 <= 0x7a) {
							*u8 -= 32;
						}
					}
					add_tag(u8buf);
				}
				
				break;
		}
	}
	if (tag_edit && O.edit == EDIT_LIST) {
		mainerror("タグ出力時は-tを使用できない");
	}
	if (tag_edit && !O.edit) {
		mainerror("タグ編集時は-a|-wの指定が必要");
	}
	if (!O.gain_fix && !O.edit) {
		O.edit = EDIT_LIST;
	}
	else if (O.gain_fix) {
		if (O.edit == EDIT_LIST) {
			mainerror("ゲイン調整のオプションは-lと同時に使用できない");
		}
		else if (!O.edit) {
			if (O.tag_filename/* || tag_edit*/) {
				mainerror("タグ編集時は-a|-wの指定が必要");
			}
		}
	}
	if (O.edit == EDIT_APPEND) {
		O.tag_ignore_picture = false;
	}
	if (cd != (iconv_t)-1) {
		iconv_close(cd);
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
	if (argc == 1) usage();
	
	parse_args(argc, argv);
	if (!argv[optind]) {
		mainerror("ファイル指定がない");
	}
	if (argv[optind + 1]) {
		if (O.edit == EDIT_LIST) {
			mainerror("ファイル指定が多い");
		}
		if (argv[optind + 2]) {
			mainerror("ファイル指定が多い");
		}
		O.out = argv[optind + 1];
	}
	
	O.in = argv[optind];
	fpopus = fopen(O.in, "r");
	if (!fpopus) {
		fileerror(O.in);
	}
	switch (O.edit) {
		case EDIT_APPEND:
		case EDIT_WRITE:
			parse_tags();
			if (!tag_edit) {
				tag_edit = malloc(sizeof(*tag_edit));
			}
			tag_edit[tagnum_edit] = NULL;
			/* FALLTHROUGH */
			
		case EDIT_LIST:
		case EDIT_NONE:
			fclose(stdin);
			break;
	}
	
	uint8_t *fbp;
	ogg_sync_state oy;
	ogg_sync_init(&oy);
	
	size_t buflen = 1 << 17;
	uint8_t *buf = ogg_sync_buffer(&oy, buflen);
	size_t len = fread(buf, 1, buflen, fpopus);
	if (len == (size_t)-1) oserror();
	if (len < 4) {
		opuserror("Oggではない");
	}
	if (strncmp(buf, "\x4f\x67\x67\x53", 4) != 0) {
		opuserror("Oggではない");
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
		opuserror("ヘッダが途切れている");
	}
	
	move_file();
	return 0;
}
