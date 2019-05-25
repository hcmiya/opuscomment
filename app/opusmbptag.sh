#!/bin/sh

set -e

progname="${0##*/}"

## utils

printbinint() (
	val=$1
	i=0
	out=
	while [ $i -lt 4 ]
	do
		val8=$((val%256))
		out=$(printf '\\%o' $val8)$out
		val=$((val/256))
		i=$((i+1))
	done
	printf "$out"
)

E() {
	echo $progname: $* >&2
	exit 1
}

usage() {
	cat <<_heredoc
Synopsis:
    $progname [-t type] [-m mime] [-d desc|-D descfile] [-g WIDTHxHEIGHTxBPP[/COLORS]] [-R] [image|-]

Description:
    $progname generates METADATA_BLOCK_PICTURE tag for Ogg Opus/Vorbis from
    image file.

Options:
    -t type
        Set image type by 0-20 or keywords. Default: 3
    -m mime
        Set MIME type
    -d desc
        Set description
    -D descfile
        Load description from descfile
    -g WIDTHxHEIGHTxBPP[/COLORS]
        Set geometry, bpp and number of colors of the image
    -R
        Raw UTF-8 I/O. Affects -D and output
_heredoc
	exit 1
}

## os specific utils
## select implementations for your environment in install time.

## create_temp (gnu/bsd)
#create_temp() (
#	mktemp -d
#)
###

## create_temp (posix)
create_temp() (
	tmp_="${TMPDIR:-/tmp}/$progname.$$.$(ps -A |cksum)"
	mkdir -m 0700 -- "$tmp_"
	printf '%s\n' "$tmp_"
)
###

## escape_binary (gnu)
#escape_binary() (
#	base64 -w0
#	echo
#)
###

## escape_binary (posix)
## needs sharutils in most linux systems
#escape_binary() (
#	uuencode -m /dev/stdout |sed '1d; $d;' |tr -d '\n'
#	echo
#)
###

## escape_binary (openssl)
escape_binary() (
	openssl base64 |tr -d '\n'
	echo
)
###

## end utils and start here

tmp="$(create_temp)"
cleanup() {
	rm -rf -- "$tmp"
}
trap cleanup EXIT

type=3
outcmd=cat
raw=n
have_description_file=n
mime=
geometry=0x0x0/0
while getopts t:d:D:m:g:R sw
do
	case $sw in
	t)
		type="$OPTARG"
		;;
	d)
		description_arg="$OPTARG"
		have_description_file=n
		;;
	D)
		description_file="$OPTARG"
		have_description_file=y
		;;
	m)
		mime="$OPTARG"
		;;
	g)
		geometry="$OPTARG"
		;;
	R)
		raw=y
		outcmd="iconv -t us-ascii"
		;;
	*)
		usage
		;;
	esac
done
shift $((OPTIND-1))

# check arguments

type="$(echo "$type" |tr -d '\n' |sed 's/[[:space:]_()]//g; y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/')"
case "$type" in
0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20) ;;
icon*) type=2 ;;
coverback|back*) type=4;;
cover*|front*) type=3 ;;
leaflet*) type=5 ;;
media*|medium*) type=6 ;;
lead*|solo*) type=7 ;;
bandlogo*|artistlogo*) type=19 ;;
artist*|performer*) type=8 ;;
conductor*) type=9 ;;
band*|orchestra*) type=10 ;;
composer*) type=11 ;;
lyric*|textwrit* ) type=12 ;;
recordedat*|record*loc*|*studio*) type=13 ;;
*recording*) type=14 ;;
*perform*) type=15 ;;
*screen*|*capture*|*video*|*movie*) type=16 ;;
*fish) type=17 ;; # ???
illust*) type=18 ;;
publisherlogo*|studiologo*) type=20 ;;
*) E unknown picture type ;;
esac

if ! expr x"$mime" : x'[] !"#$%&'\''()*+,./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\^_`abcdefghijklmnopqrstuvwxyz{|}~-]*$' >/dev/null
then E invalid mime type string
fi

if echo $geometry |grep -sqEx '[1-9][0-9]*x[1-9][0-9]*x[1-9][0-9]*(/(0|[1-9][0-9]*))?|0x0x0(/0)?'
then
	echo $geometry |tr x/ '  ' >"$tmp/geometry"
	read width height bpp colors <"$tmp/geometry"
	colors=${colors:-0}
else
	E invalid geometry
fi

description_conv_arg="-t utf-8"
case $have_description_file in
y)
	description_input="$description_file"
	case $raw in
	y) description_conv_arg="-f utf-8 -t utf-8" ;;
	n) ;;
	esac
	;;
n)
	description_input="$tmp/description-input"
	printf '%s\n' "$description_arg" >"$description_input"
	;;
esac
iconv $description_conv_arg <"$description_input" >"$tmp/description"
description_length=$(wc -c <"$tmp/description")
description_last_byte=$(tail -c1 <"$tmp/description" |od -td1 -An -v)
if [ "$description_last_byte" -eq 10 ]
then description_length=$((description_length-1))
fi

case $# in
0) cat >"$tmp/binary" ;;
1) cat -- "$1" >"$tmp/binary" ;;
*) usage ;;
esac

if [ -z "$mime" ]
then
	determine="$(file - <"$tmp/binary")"
	case "$determine" in
	*JPEG*|*JFIF*) mime=image/jpeg ;;
	*GIF*) mime=image/gif ;;
	*PNG*) mime=image/png ;;
	esac
fi
printf %s "$mime" |iconv -t us-ascii >"$tmp/mime"

# generate packet
{
	printf METADATA_BLOCK_PICTURE=
	{
		printbinint $type
		printbinint $(wc -c <"$tmp/mime")
		cat <"$tmp/mime"
		printbinint $description_length
		dd count=$description_length bs=1 <"$tmp/description" 2>/dev/null
		printbinint $width
		printbinint $height
		printbinint $bpp
		printbinint $colors
		printbinint $(wc -c <"$tmp/binary")
		cat <"$tmp/binary"
	} |escape_binary
} |$outcmd
# 	} |hexdump -C |head
# }
