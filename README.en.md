# opuscomment

## Overview

<dfn>opuscomment</dfn> is an editing utility for Ogg Opus with an interface compatible with [vorbiscomment](https://github.com/xiph/vorbis-tools). The first feature is that you can edit the output gain which is one of Opus's important functions. It also aims to always escape tags that span multiple lines safely and to facilitate incorporation into other software.

## Feature

### Compatible with editing output gain

In Ogg Opus, there is an item called output gain in the header, and the function that the volume can be changed freely even after encoding is standard as standard. Opuscomment is one of the few tools as of 2017 corresponding to arbitrary rewriting.

For example, because the sound of the created sound effect is too loud, you want to make it smaller, for example, you need to adjust the volume standard at the movie site. At that time, other codecs will need to be re-encoded. Or you will need to write a program to maintain and apply volume management data in a manner incompatible with each service. In Opus, you only have to rewrite the 2 bytes of the header.

### Edit style compatible with vorbiscomment

Forms compatible with vorbiscomment · Escaping and options make it easy to make the transition from Ogg Vorbis to the audio file itself and applications dealing with it. For example, to port the same tag from Vorbis to Opus, you can easily do with the following command.


    vorbiscomment -lRe music.oga |opuscomment -wRe music.opus

In addition, the basic option of opuscomment is almost the same as that of vorbiscomment, so existing scripts for Vorbis will be able to deal with Opus by rewriting the command name.

### Suppression of memory usage

opuscomment never reserves memory to match its length for editing tags. Moreover, it does not have a packet which needs to take out it in memory. Therefore, it can be relatively strongly and safely incorporated into other applications against broken files and malicious editing.

### Safe handling of tags

opuscomment will always escape it if the content of the tag contains a line break. Contents are broken by line breaks, and the next line will not be falsely recognized as the start of the item. Escape is intuitively understandable way of continuing tabs at the beginning of a line after line breaks and a familiar way for programmers using the same "\ n" as vorbiscomment.

Secondly, even if the filter of the previous stage accepting editing input accidentally terminated and the contents were interrupted, we prepared an option that can reduce the possibility of Opus file losing tags. For details, see the description of `-V`,` -T`, `-D` and [hcmiya/opuscomment#6](https://github.com/hcmiya/opuscomment/issues/6).

## Compiling and operation requirements

* C99
* API corresponding to POSIX.1-2008 or POSIX.1-2001 XSI extension (SUSv3)
* libogg

In addition, libopus and libopusfile are not necessary. Since we are not using the configure script, please check before compilation to comply with the above requirements and that there is a corresponding header file (install Debian if `libogg-dev` is installed). See also Makefile comment.

## Installation

    $ make

If you do, binary will be output as `opuscomment` in `src/`, so copy it to the desired location.

### Message Catalog

opuscomment implements localization using X/Open NLS. Since we have a catalog source in `nls/`, please convert it to a catalog file using `gencat(1)` for the required locale and save it in a suitable directory.

Although it is appropriate, the system standard message catalog location is different for each system, and some systems may not have a standard catalog location in the first place. Therefore, it is recommended that you install the catalog in the data directory for the application and start opuscomment via the shell script that decided NLSPATH there. `%N` of `NLSPATH` will be replaced with `opuscomment`.

    # An example of post-compile installation method when using NLS catalog
    PREFIX=/usr/local
    mkdir -p $PREFIX/libexec/opuscomment $PREFIX/lib/opuscomment/nls $PREFIX/bin
    cp opuscomment $PREFIX/libexec/opuscomment/
    for lang in ja_JP.UTF-8 ja_JP.eucJP
    do env LANG=$lang gencat $PREFIX/lib/opuscomment/nls/$lang.cat nls/$lang/*
    done
    cat <<heredoc >$PREFIX/bin/opuscomment
    #!/bin/sh
    if [ -z "\$NLSPATH" ]
    then NLSPATH=$PREFIX/lib/opuscomment/nls/%L.cat
    fi
    exec env NLSPATH="\$NLSPATH" $PREFIX/libexec/opuscomment/opuscomment "\$@"
    heredoc
    chmod +x $PREFIX/bin/opuscomment

### Alias for binary

`opuscomment` has the function of changing the codec to be edited with the binary name. The corresponding codecs are as shown in the table below. It is a good idea to link to opuscomment when installing.

| argv[0] | codec |
|--|--|
| (all but next) | Opus |
| vorbiscomment | Vorbis |
| speexcomment | Speex |
| vp8comment | VP8 |
| theoracomment | Theora |
| daalacomment | Daala |
| oggpcmcomment | PCM |
| ogguvscomment | UVS |

Either of the above can be edited only when included in Ogg.

## License

### opuscomment

MIT license

### libogg

[libogg](https://www.xiph.org/ogg/) is the work of [Xiph.Org Foundation](https://www.xiph.org/). Please refer to LICENSE.libogg for the license.

## ENGLISH TRANSLATION
[Google translator](https://translate.google.ru)
