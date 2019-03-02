#!/bin/sh

OC=opuscomment
progname="${0##*/}"

if [ $# = 0 ]
then
	cat <<heredoc >&2
Synopsys:
    $progname {-g gain|-s scale|-0} [-i idx] [-1Qr] opusfile [output]

Description:
    Change Opus' output gain with R128 gain tag
heredoc
	exit 1
fi

E() {
	eno=$1
	shift
	printf '%s: %s\n' "$progname" "$*" >&2
	exit $eno
}

mode= q78= val= relative= not0= idx=
while getopts i:g:s:rQ01 sw
do
	case $sw in
	g|s)
		mode=$sw
		gainhasval=y
		gainval="$OPTARG"
		;;
	0)
		mode=0
		gainhasval=
		;;
	i)
		idx="$OPTARG"
		;;
	1)
		not0=y
		;;
	Q)
		q78=y
		;;
	r)
		relative=y
		;;
	*)
		exit 1
		;;
	esac
done
shift $((OPTIND - 1))

[ -n "$mode" ] || E 1 no mode specified
if [ $mode = 0 ]
then
	relative=
	not0=
fi

case $# in
0)
	E 1 no file specified >&2
	;;
1)
	out=overwrite
	src="$1"
	dest=
	;;
2)
	out=other
	src="$1"
	dest="$2"
	;;
*)
	E 1 too many arguments >&2
	;;
esac

[ -n "$src" ] || E 1 empty file name
[ $out = other -a -z "$dest" ] && E 1 empty file name || :

set -e

tmp="$(mktemp -d)"
# mktemp(1) is not in POSIX
# tmp=/tmp/opuschgain.$$
# mkdir /tmp/opuschgain.$$
cleanup() {
	rm -rf "$tmp"
}
trap cleanup EXIT

tags="$tmp/tags"
if "$OC" -VvQUe -- "$src" 2>"$tmp/gval" >"$tags"
then :
else
	e=$?
	sed '/^-\{0,1\}[0-9]/d' <"$tmp/gval" >&2
	exit $e
fi
	
pregain=$(cat "$tmp/gval")

get_gain_tag() {
	name=R128_${1}_GAIN
	tag="$(sed -n /^$name=/p <"$tags")"
	if [ -n "$tag" ]
	then
		[ $(printf "%s\n" "$tag" |wc -l) -eq 1 ] || E 1 $name appears many times
		tagval="${tag#$name=}"
		printf '%s\n' "$tagval" |grep -xsq '[+-]\{0,1\}[0-9]\{1,5\}' || E 1 $name has invalid value
		printf '%s\n' $tagval |sed 's/^[0-9]/+&/; s/\([+-]\)0*/\1/; s/[+-]$/&0/; s/^+//'
	fi
}

trackgain=$(get_gain_tag TRACK)
albumgain=$(get_gain_tag ALBUM)

if [ -z "$trackgain" -a -z "$albumgain" ]
then
	exec "$OC" -$mode ${gainhasval:+"$gainval"} ${idx:+-i "$idx"} ${not0:+-1} ${q78:+-Q} ${relative:+-r} -- "$src" ${dest:+"$dest"}
	exit 1
fi

mod="$tmp/mod"
"$OC" -$mode ${gainhasval:+"$gainval"} ${idx:+-i "$idx"} ${not0:+-1} ${q78:+-Q} ${relative:+-r} -- "$src" "$mod"
[ $out = overwrite ] && dest="$src" || :
postgain=$("$OC" -vQ -- "$mod" 2>&1 >/dev/null)

diff=$((pregain - postgain))
echo "R128_TRACK_GAIN=$((trackgain + diff))
R128_ALBUM_GAIN=$((albumgain + diff))" |"$OC" -a -d R128_TRACK_GAIN -d R128_ALBUM_GAIN ${idx:+-i "$idx"} -- "$mod" "$dest"


