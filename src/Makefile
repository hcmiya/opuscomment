# コンパイル時の注意
# コンパイルの際にはPOSIX.1-2008またはSUSv3のAPIが動作する環境であることを確認して下さい。
# 動作環境としてSUSv3を選択する場合はDEFAULT_MACROSを_XOPEN_SOURCE=600を含むものに切り替えて下さい。
# 特に、iconv(3)がサポートされていることを確認して下さい。(POSIX.1-2008ではcatopen(3)と共に必須)
# iconv(3)は昔のFreeBSDの様に別ライブラリになっている可能性もあるので、適宜LIBSを編集するようお願いします。
# NLSに対応していない、あるいは必要がない場合は、CFLAGSに-UNLSを追加することで無効に出来ます。
# `DEFAULT_NLS_PATH`を文字列でdefineをすると、NLSPATHが無い時に最初にcatopen(3)をする場所を指定することができます。

SRCS=put-tags.c parse-tags.c read.c read-flac.c ocutil.c retrieve-tags.c select-codec.c
CONFSRCS=endianness.c error.c main.c
OBJS=$(SRCS:.c=.o) $(CONFSRCS:.c=.o)
HEADERS=global.h ocutil.h limit.h error.h
ERRORDEFS=errordef/opus.tab errordef/main.tab
DEFAULT_MACROS=-D_POSIX_C_SOURCE=200809L -DNLS
#DEFAULT_MACROS=-D_XOPEN_SOURCE=600 -DNLS
LIBS=-logg -lm -lpthread
CC=c99
PNAME=opuscomment
PREFIX?=/usr/local
DATAPREFIX=$(PREFIX)/share
INSTALL=install
LN=ln -fs

all: opuscomment ;

opuscomment: $(OBJS)
	$(CC) -o opuscomment $(DEFAULT_MACROS) $(CFLAGS) $(LDFLAGS) $(LIBS) $(OBJS)

.SUFFIXES:
.SUFFIXES: .c .o

.c.o:
	$(CC) $(DEFAULT_MACROS) $(CFLAGS) -c $<

$(SRCS): $(HEADERS)
	@touch $@

endianness.c: endianness-check.sh
	./endianness-check.sh >endianness.c

error.c: $(ERRORDEFS) $(HEADERS)
	@touch $@

main.c: $(HEADERS) version.h
	@touch $@

clean:
	rm endianness.c opuscomment $(OBJS) 2>/dev/null || :

install: $(PNAME)
	$(INSTALL) -d $(PREFIX)/bin
	$(INSTALL) -m 0755 $(PNAME) $(PREFIX)/bin/
	$(INSTALL) -d $(DATAPREFIX)/scripts/dash/$(PNAME)
	$(INSTALL) -m 0755 ../app/*.sh $(DATAPREFIX)/scripts/dash/$(PNAME)
	$(LN) $(DATAPREFIX)/scripts/dash/$(PNAME)/opuschgain.sh $(PREFIX)/bin/opuschgain
	$(LN) $(DATAPREFIX)/scripts/dash/$(PNAME)/opusmbptag.sh $(PREFIX)/bin/opusmbptag
	$(INSTALL) -d $(DATAPREFIX)
	$(INSTALL) -d $(DATAPREFIX)/man/man1
	$(INSTALL) -d $(DATAPREFIX)/man/ja/man1
	$(INSTALL) -m 0644 ../doc/man/man1/*.1 $(DATAPREFIX)/man/man1
	$(INSTALL) -m 0644 ../doc/man/ja/man1/*.1 $(DATAPREFIX)/man/ja/man1
	$(INSTALL) -d $(DATAPREFIX)/doc/$(PNAME)
	$(INSTALL) -m 0644 ../LICENSE ../LICENSE.libogg ../README.ja.md ../README.md ../doc/samples/sample_audiobook.opustags $(DATAPREFIX)/doc/$(PNAME)

