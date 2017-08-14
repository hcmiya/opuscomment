# opuscomment

Ogg Opusのタグとゲインを編集するユーティリティ

## 概要

<dfn>opuscomment</dfn>は[vorbiscomment](https://github.com/xiph/vorbis-tools)と互換のインターフェイスを持つOgg Opus用の編集ユーティリティです。Opusの重要な機能の一つである出力ゲインの編集が出来るようになっていることを特徴としています。また、複数行にまたがるタグを常に安全にエスケープすること、書き換えの箇所を必要最小限にして元のOggファイルの構造を極力いじらないようにすることも目標としています。

## コンパイル・動作要件

* C99
* POSIX.1-2008に対応したAPI (`rename(2)`, `iconv(3)`)
* libogg

尚、libopus、libopusfileは必要ありません。configureスクリプトを使用していないので、Debianならば`libogg-dev`などといった対応するヘッダファイルがインストールされていることをコンパイル前にご確認下さい。

## インストール

    $ make

をすると同じディレクトリに`opuscomment`という名前でバイナリが出力されますのでそれを任意の場所にコピーします。

opuscommentはX/Open仕様に基く地域化を実装しています。`nls/`にカタログのソースを用意していますので、必要なロケールに対して`gencat(1)`を利用してカタログファイルに変換した後に適当なディレクトリに保存して下さい。

## ライセンス

### opuscomment

MITライセンス

### libogg

LICENSE.liboggを参照
