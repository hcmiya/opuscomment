#!/bin/sh
val=$(printf '\1\2\3\4' |od -v -An -tuI)
if [ $val -eq $((0x04030201)) ]
then
	echo '#define LITTLE_ENDIAN'
elif [ $val -eq $((0x01020304)) ]
then
	:
else
	echo '#error 謎のエンディアンネス'
fi
