# コンパイル時の注意
# コンパイルの際にはPOSIX.1-2008またはPOSIX.1-2001 XSI拡張のAPIが動作する環境であることを確認して下さい。
# 動作環境としてPOSIX.1-2001 XSI拡張を選択する場合はCFLAGSを_XOPEN_SOURCE=600を含むものに切り替えて下さい。
# 特に、iconv(3)がサポートされていることを確認して下さい。(POSIX.1-2008ではcatopen(3)と共に必須)
# iconv(3)は昔のFreeBSDの様に別ライブラリになっている可能性もあるので、適宜LIBSを編集するようお願いします。
# NLSに対応していない、あるいは必要がない場合は、CFLAGSから-DNLSを除くことで無効に出来ます。

SRC=main.c put-tags.c parse-tags.c read.c error.c ocutil.c endianness.c
HEADER=global.h ocutil.h limit.h
CFLAGS=-D_POSIX_C_SOURCE=200809L -DNLS
#CFLAGS=-D_XOPEN_SOURCE=600 -DNLS
LDFLAGS=
LIBS=-logg -lm -lpthread
CC=c99

all: opuscomment ;

opuscomment: $(HEADER) $(SRC)
	$(CC) -o opuscomment -O2 $(CFLAGS) $(LDFLAGS) -DNDEBUG $(LIBS) $(SRC)

debug: opuscomment.debug ;

opuscomment.debug: $(HEADER) $(SRC)
	$(CC) -g -o opuscomment.debug $(CFLAGS) $(LDFLAGS) $(LIBS) $(SRC)

endianness.c: endianness-check.sh
	./endianness-check.sh >endianness.c

clean:
	rm endianness.c opuscomment opuscomment.debug 2>/dev/null || :
