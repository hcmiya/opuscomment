# コンパイル時の注意
# コンパイルの際にはPOSIX.1-2008またはSUSv2のAPIが動作する環境であることを確認して下さい。
# 動作環境としてSUSv2を選択する場合はCFLAGSを_XOPEN_SOURCE=600を含むものに切り替えて下さい。
# 特に、iconv(3)がサポートされていることを確認して下さい。(POSIX.1-2008ではcatopen(3)と共に必須)
# iconv(3)は昔のFreeBSDの様に別ライブラリになっている可能性もあるので、適宜LIBSを編集するようお願いします。
# NLSに対応していない、あるいは必要がない場合は、CFLAGSから-DNLSを除くことで無効に出来ます。

SRC=main.c put-tags.c parse-tags.c read.c error.c ocutil.c endianness.c retrieve-tags.c select-codec.c
HEADER=global.h ocutil.h limit.h version.h error.h
ERRORDEF=errordef/opus.tab errordef/main.tab
CFLAGS=-D_POSIX_C_SOURCE=200809L -DNLS
#CFLAGS=-D_XOPEN_SOURCE=600 -DNLS
LDFLAGS=
LIBS=-logg -lm -lpthread
CC=c99

all: opuscomment ;

opuscomment: $(HEADER) $(SRC) $(ERRORDEF)
	$(CC) -o opuscomment -O2 $(CFLAGS) $(LDFLAGS) -DNDEBUG $(LIBS) $(SRC)

debug: tests/ocd ;

tests/ocd: $(HEADER) $(SRC) $(ERRORDEF)
	$(CC) -g -o tests/ocd $(CFLAGS) $(LDFLAGS) $(LIBS) $(SRC)

endianness.c: endianness-check.sh
	./endianness-check.sh >endianness.c

clean:
	rm endianness.c opuscomment ocd 2>/dev/null || :

updoc: doc/opuscomment.ja.1 doc/opuschgain.ja.1 ;

doc/opuscomment.ja.1 doc/opuschgain.ja.1: ../ocdoc/opuscomment-current.ja.sgml
	../ocdoc/tr.sh
