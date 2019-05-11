#!/bin/sh

set -e

if [ -n "$DEFAULT_NLS_PATH" ]
then CFLAGS="$CFLAGS -DDEFAULT_NLS_PATH=\\\"$DEFAULT_NLS_PATH\\\""
fi

CC="${CC:-c99}"
MAKE="${MAKE:-make}"

case "$1" in
"")
	cat <<_heredoc
Synopsys:
    ./build.sh {release|debug|clean}
    ./build.sh catalog [lang]...

Environment Variables:
    CC, CFLAGS, MAKE, DEFAULT_NLS_PATH
_heredoc
	;;
release)
	CFLAGS="$CFLAGS -DNDEBUG"
	$MAKE CFLAGS="$CFLAGS" CC="$CC"
	;;
debug)
	CFLAGS="$CFLAGS -O0 -g -Wall -Wno-pointer-sign -Wno-parentheses -Wno-switch"
	$MAKE CFLAGS="$CFLAGS" CC="$CC"
	;;
clean)
	$MAKE clean
	rm nls/*/opuscomment.cat 2>/dev/null || :
	;;
catalog)
	shift
	if [ -z "$1" -o x"$1" = xall ]
	then list="$(ls nls)"
	else list="$*"
	fi
	for lang in $list
	do
		ls nls/$lang |grep -v -e opuscomment.cat -e README |sort -n |sed "s|^|nls/$lang/|" \
			|xargs env LANG=$lang gencat nls/$lang/opuscomment.cat
	done
esac
