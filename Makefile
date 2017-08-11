SRC=main.c put-tags.c parse-tags.c read.c error.c
HEADER=endianness.h global.h
CFLAGS+=-D_POSIX_C_SOURCE=200809L
LIBS=-logg -lm

all: opuscomment

opuscomment: $(HEADER) $(SRC)
	c99 -o opuscomment -O2 $(CFLAGS) $(LDFLAGS) -DNDEBUG $(LIBS) $(SRC)

debug: opuscomment.debug

opuscomment.debug: $(HEADER) $(SRC)
	c99 -g -o opuscomment.debug $(CFLAGS) $(LDFLAGS) $(LIBS) $(SRC)

endianness.h: endianness-check.sh
	./endianness-check.sh >endianness.h

clean:
	rm endianness.h opuscomment opuscomment.debug 2>/dev/null || :

