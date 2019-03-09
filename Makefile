all:
	CFLAGS="$(CFLAGS) -DNDEBUG" $(MAKE) -C src -r

debug:
	CFLAGS="$(CFLAGS) -O0 -g" $(MAKE) -C src -r

clean:
	$(MAKE) -C src clean

updoc: doc/opuscomment.ja.1 doc/opuschgain.ja.1 ;

doc/opuscomment.ja.1 doc/opuschgain.ja.1: ../ocdoc/opuscomment-current.ja.sgml
	../ocdoc/tr.sh
