# opuscomment

Ogg Opusのタグとゲインを編集するユーティリティ

## 概要

<dfn>opuscomment</dfn>は[vorbiscomment](https://github.com/xiph/vorbis-tools)と互換のインターフェイスを持つOgg Opus用の編集ユーティリティです。Opusの重要な機能の一つである出力ゲインの編集が出来るようになっていることを特徴としています。また、複数行にまたがるタグを常に安全にエスケープすること、書き換えの箇所を必要最小限にして元のOggファイルの構造を極力いじらないようにすることも目標としています。

## コンパイル・動作要件

* C99
* POSIX.1-2008またはPOSIX.1-2001 XSI拡張に対応したAPI: `catopen(3)`, `iconv(3)`, `mkstemp(3)`, `strndup(3)`, `strnlen(3)`
* libogg

尚、libopus、libopusfileは必要ありません。configureスクリプトを使用していないので、Debianならば`libogg-dev`などといった対応するヘッダファイルがインストールされていることをコンパイル前にご確認下さい。

## インストール

    $ make

をすると同じディレクトリに`opuscomment`という名前でバイナリが出力されますのでそれを任意の場所にコピーします。

### メッセージカタログ

opuscommentはX/Open仕様に基く地域化を実装しています。`nls/`にカタログのソースを用意していますので、必要なロケールに対して`gencat(1)`を利用してカタログファイルに変換した後に適当なディレクトリに保存して下さい。

適当とは言っても、システム標準のメッセージカタログの置き場所というのはシステム毎にバラバラですし、そもそも標準的なカタログの置き場所を設定していないシステムもあるかも知れません。そのため、アプリケーション用のデータディレクトリにカタログをインストールし、NLSPATHをそこに決め打ちしたシェルスクリプト経由でopuscommentを起動させるというのがおすすめです。`NLSPATH`の`%N`は`opuscomment.cat`に置換されます。

    # NLSカタログを使用する際のコンパイル後のインストール方法の一例
    PREFIX=/usr/local
    mkdir -p $PREFIX/libexec/opuscomment $PREFIX/lib/opuscomment/nls $PREFIX/bin
    cp opuscomment $PREFIX/libexec/opuscomment/
    for lang in ja_JP.UTF-8 ja_JP.eucJP
    do env LANG=$lang gencat $PREFIX/lib/opuscomment/nls/$lang.cat nls/$lang/*
    done
    cat <<heredoc >$PREFIX/bin/opuscomment
    #!/bin/sh
    if [ -z "\$NLSPATH" ]
    then NLSPATH=$PREFIX/lib/opuscomment/nls/%L.cat
    fi
    exec env NLSPATH="\$NLSPATH" $PREFIX/libexec/opuscomment/opuscomment "\$@"
    heredoc
    chmod +x $PREFIX/bin/opuscomment

## ライセンス

### opuscomment

MITライセンス

### libogg

LICENSE.liboggを参照
