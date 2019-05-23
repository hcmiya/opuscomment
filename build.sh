#!/bin/sh

set -e

if [ -n "$DEFAULT_NLS_PATH" ]
then CFLAGS="$CFLAGS -DDEFAULT_NLS_PATH=\\\"$DEFAULT_NLS_PATH\\\""
fi

dir=$(dirname "$0")
CC="${CC:-c99}"
MAKE="${MAKE:-make}"

case "$1" in
release)
	CFLAGS="$CFLAGS -DNDEBUG"
	$MAKE -C "$dir" CFLAGS="$CFLAGS" CC="$CC"
	;;
debug)
	CFLAGS="$CFLAGS -O0 -g -Wall -Wno-pointer-sign -Wno-parentheses -Wno-switch"
	$MAKE -C "$dir" CFLAGS="$CFLAGS" CC="$CC"
	;;
clean)
	$MAKE -C "$dir" clean
	rm "$dir"/nls/*/opuscomment.cat 2>/dev/null || :
	;;
catalog)
	shift
	(
		cd "$dir"/nls
		if [ -z "$1" -o x"$1" = xall ]
		then list="$(ls)"
		else list="$*"
		fi
		for lang in $list
		do
			ls $lang |grep -v -e opuscomment.cat -e README |sort -n |sed "s|^|$lang/|" \
				|xargs env LANG=$lang gencat $lang/opuscomment.cat
		done
	)
	;;
*)
	cat <<_heredoc
Synopsys:
    $0 {release|debug|clean}
    $0 catalog [lang]...

Environment Variables:
    CC, CFLAGS, MAKE, DEFAULT_NLS_PATH
_heredoc
	;;
esac
