SRC=main.c put-tags.c parse-tags.c read.c error.c ocutil.c endianness.c
HEADER=global.h ocutil.h
CFLAGS=-D_POSIX_C_SOURCE=200809L -DNLS
#CFLAGS=-D_XOPEN_SOURCE=600 -DNLS
LDFLAGS=
LIBS=-logg -lm
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
