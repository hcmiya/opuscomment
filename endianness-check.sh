#!/bin/sh
val=$(printf '\1\0\0\0' |od -v -An -txI)
if [ $val -eq 1 ]
then
	echo '#define LITTLE_ENDIAN'
elif [ $val -eq 16777216 ]
then
	:
else
	echo '#error 謎のエンディアンネス'
fi
