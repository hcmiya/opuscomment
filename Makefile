all:
	$(MAKE) -C src -r

clean:
	$(MAKE) -C src clean

updoc-ja: doc/man/ja/man1/opuscomment.1;

doc/man/ja/man1/opuscomment.1: doc/mansrc/opuscomment.ja.sgml
	tests/tr.sh
