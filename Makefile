all:
	$(MAKE) -C src -r

clean:
	$(MAKE) -C src clean

updoc: doc/man/ja/man1/opuscomment.1;

doc/man/ja/man1/opuscomment.1: doc/docbook/opuscomment.ja.sgml
	tests/tr.sh
