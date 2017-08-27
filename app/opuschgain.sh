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

pregain=$("$OC" -VvQ -- "$src" 2>&1 >/dev/null)

get_gain_tag() {
	name=R128_${1}_GAIN
	tag="$("$OC" -eU -- "$src" |sed -n /^$name=/p)"
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

"$OC" -$mode ${gainhasval:+"$gainval"} ${idx:+-i "$idx"} ${q78:+-Q} ${relative:+-r} -- "$src" ${dest:+"$dest"}
[ $out = other ] && src="$dest" || :
postgain=$("$OC" -vQ -- "$src" 2>&1 >/dev/null)

add1=
if [ $postgain = 0 -a -n "$not0" ]
then
	add1=y
	postgain=1
fi

diff=$((pregain - postgain))
sedscr="${trackgain:+"s/^R128_TRACK_GAIN=.*/R128_TRACK_GAIN=$((trackgain + diff))/"}
${albumgain:+"s/^R128_ALBUM_GAIN=.*/R128_ALBUM_GAIN=$((albumgain + diff))/"}"

"$OC" -U -- "$src" |sed "$sedscr" |"$OC" -w ${add1:+-Qg1}-- "$src"
