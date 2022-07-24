# コンパイル時の注意
# コンパイルの際にはPOSIX.1-2008またはSUSv3のAPIが動作する環境であることを確認して下さい。
# 動作環境としてSUSv3を選択する場合はDEFAULT_MACROSを_XOPEN_SOURCE=600を含むものに切り替えて下さい。
# 特に、iconv(3)がサポートされていることを確認して下さい。(POSIX.1-2008ではcatopen(3)と共に必須)
# iconv(3)は昔のFreeBSDの様に別ライブラリになっている可能性もあるので、適宜LIBSを編集するようお願いします。
# NLSに対応していない、あるいは必要がない場合は、CFLAGSに-UNLSを追加することで無効に出来ます。
# `DEFAULT_NLS_PATH`を文字列でdefineをすると、NLSPATHが無い時に最初にcatopen(3)をする場所を指定することができます。

CC=c99
CFLAGS=-D_POSIX_C_SOURCE=200809L -DNLS
LDFLAGS=-logg -lm -lpthread
SRCS=put-tags.c parse-tags.c read.c read-flac.c ocutil.c retrieve-tags.c select-codec.c
SRCP=opuspic2tag.c picture.c
CONFSRCS=endianness.c error.c opuscomment.c
HEADERS=global.h ocutil.h limit.h error.h iconv-impl.h
ERRORDEFS=errordef/opus.tab errordef/opuscomment.tab
OBJS=$(SRCS:.c=.o) $(CONFSRCS:.c=.o)
OBJP=$(SRCP:.c=.o)
RM = rm -f

all: opuscomment opuspic2tag

opuscomment: $(OBJS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

opuspic2tag: $(OBJP)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

.SUFFIXES:
.SUFFIXES: .c .o

.c.o:
	$(CC) $(CFLAGS) -c $<

$(SRCS): $(HEADERS)
	@touch $@

error.c: $(ERRORDEFS) $(HEADERS)
	@touch $@

opuscomment.c: $(HEADERS) version.h
	@touch $@

clean:
	$(RM) opuscomment opuspic2tag $(OBJS) $(OBJP) 2>/dev/null || :
