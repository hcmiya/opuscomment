SRC=main.c put-tags.c parse-tags.c read.c
HEADER=endianness-check.h global.h
CFLAGS=-D_POSIX_C_SOURCE=200809L
LIBS=-logg -lm

all: opuscomment

opuscomment: $(HEADER) $(SRC)
	c99 -o opuscomment -O2 $(CFLAGS) $(LIBS) $(SRC)

debug: opuscomment.debug

opuscomment.debug: $(HEADER) $(SRC)
	c99 -g -o opuscomment.debug $(CFLAGS) $(LIBS) $(SRC)

endianness-check.h:
	./endianness-check.sh >endianness-check.h

clean:
	rm endianness-check.h opuscomment opuscomment.debug 2>/dev/null || :

