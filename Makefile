all:
	$(MAKE) -C src -r

debug:
	$(MAKE) -C src debug

clean:
	$(MAKE) -C src clean

updoc: doc/opuscomment.ja.1 doc/opuschgain.ja.1 ;

doc/opuscomment.ja.1 doc/opuschgain.ja.1: ../ocdoc/opuscomment-current.ja.sgml
	../ocdoc/tr.sh
