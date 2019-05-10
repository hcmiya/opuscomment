all:
	$(MAKE) -C src -r

compile-all-catalogs:
	sh -c 'for i in nls/* ; do \
		lang=$${i#nls/} ; \
		ls $${i} |grep -v -e opuscomment.cat -e README |sort -n |sed "s!^!nls/$${lang}/!" \
		|xargs env LANG=$${lang} gencat $${i}/opuscomment.cat ; \
	done'

clean:
	$(MAKE) -C src clean

updoc: doc/opuscomment.ja.1 doc/opuschgain.ja.1 ;

doc/opuscomment.ja.1 doc/opuschgain.ja.1: ../ocdoc/opuscomment-current.ja.sgml
	../ocdoc/tr.sh
