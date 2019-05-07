# コンパイル時の注意
# コンパイルの際にはPOSIX.1-2008またはSUSv3のAPIが動作する環境であることを確認して下さい。
# 動作環境としてSUSv3を選択する場合はCFLAGSを_XOPEN_SOURCE=600を含むものに切り替えて下さい。
# 特に、iconv(3)がサポートされていることを確認して下さい。(POSIX.1-2008ではcatopen(3)と共に必須)
# iconv(3)は昔のFreeBSDの様に別ライブラリになっている可能性もあるので、適宜LIBSを編集するようお願いします。
# NLSに対応していない、あるいは必要がない場合は、CFLAGSから-DNLSを除くことで無効に出来ます。

SRCS=put-tags.c parse-tags.c read.c read-flac.c ocutil.c retrieve-tags.c select-codec.c
CONFSRCS=endianness.c error.c main.c
OBJS=$(SRCS:.c=.o) $(CONFSRCS:.c=.o)
HEADERS=global.h ocutil.h limit.h error.h
ERRORDEFS=errordef/opus.tab errordef/main.tab
CFLAGS+=-D_POSIX_C_SOURCE=200809L -DNLS
#CFLAGS+=-D_XOPEN_SOURCE=600
LDFLAGS+=
LIBS=-logg -lm -lpthread
CC=c99

all: opuscomment opuspictotag;

opuscomment: $(OBJS)
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $(LIBS) $^

opuspictotag: picture.c opuspictotag.c
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $(LIBS) $^

.SUFFIXES:
.SUFFIXES: .c .o

.c.o:
	$(CC) $(CFLAGS) -c $<

$(SRCS): $(HEADERS)
	@touch $@

endianness.c: endianness-check.sh
	./endianness-check.sh >endianness.c

error.c: $(ERRORDEFS) $(HEADERS)
	@touch $@

main.c: $(HEADERS) version.h
	@touch $@

clean:
	rm endianness.c opuscomment opuspictotag $(OBJS) 2>/dev/null || :
