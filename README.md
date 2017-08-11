# opuscomment

Ogg Opusのタグとゲインを編集するユーティリティ

## 概要

<dfn>opuscomment</dfn>は[vorbiscomment](https://github.com/xiph/vorbis-tools)と互換のインターフェイスを持つOgg Opus用の編集ユーティリティです。Opusの重要な機能の一つである出力ゲインの編集が出来るようになっていることを特徴としています。また、複数行にまたがるタグを常に安全にエスケープすること、書き換えの箇所を必要最小限にして元のOggファイルの構造を極力いじらないようにすることも目標としています。

## コンパイル・動作要件

* C99
* POSIX.1-2008に対応したAPI (`rename(2)`)
* libogg

尚、libopus、libopusfileは必要ありません。configureスクリプトを使用していないので、Debianならば`libogg-dev`などといった対応するヘッダファイルがインストールされていることをコンパイル前にご確認下さい。

## インストール

    $ make

をすると同じディレクトリに`opuscomment`という名前でバイナリが出力されますのでそれを任意の場所にコピーします。

## コマンド説明

### 名前

`opuscomment` — Ogg Opusファイルの出力ゲインとタグを編集する

### 書式

    opuscomment [-l] [-epRvV] opusfile
    opuscomment -a|-w [-g gain|-s gain|-n] [-c file|-t NAME=VALUE ...] [-eGprRvV] opusfile [output]

### 説明

1つめの書式はタグ出力モードであり、OpusファイルのVorbis comment形式のタグを標準出力に出力する。

2つ目の書式はタグ上書き・追記モードであり、標準入力から読み取ったVorbis comment形式のタグをOpusファイルに書き込む。オプションの後のファイルが1つの場合、元のファイルは編集結果によって置き換えられる。2つの場合は編集結果が別ファイルに出力され、元のファイルはそのまま残る。タグファイル指定のオプションがあれば標準入力を使わずそのファイルからタグを読み込む。タグを直接指定するオプションがあればそれを入力として扱う。ゲイン編集のオプションがあればタグ編集と同時にそれも行われる。

いずれの書式でもゲイン出力のオプションの指定があればOpusファイルが持つ編集**前**の出力ゲインを標準エラー出力に出力する。

### オプション

<dl>
<dt>-l</dt>
<dd>タグ出力モード</dd>
<dt>-a</dt>
<dd>タグ追記モード</dd>
<dt>-w</dt>
<dd>タグ書き込みモード</dd>
<dt>-R</dt>
<dd>タグ入出力にUTF-8を使う。このオプションがない場合はロケールによる文字符号化との変換が行われる(<code>vorbiscomment(1)</code>互換)</dd>
<dt>-e</dt>
<dd>バックスラッシュ、改行、復帰、ヌルにそれぞれ\\, \n, \r, \0のエスケープを使用する(<code>vorbiscomment(1)</code>互換)</dd>
<dt>-g gain</dt>
<dd>出力ゲインをdBで指定する</dd>
<dt>-s gain</dt>
<dd>出力ゲインをPCMサンプルの倍率で指定する。1で等倍。0.5で半分(コマンド内でdBに変換)</dd>
<dt>-n</dt>
<dd>出力ゲインを0にする</dd>
<dt>-r</dt>
<dd>出力ゲインの指定を内部の設定に対する相対値とする</dd>
<dt>-G</dt>
<dd>出力ゲインが内部形式にした時に0になる場合は[-+]1/256 dBを設定する</dd>
<dt>-p</dt>
<dd>METADATA_BLOCK_PICTUREの出力または削除をしない</dd>
<dt>-v</dt>
<dd>出力ゲインの編集<strong>前</strong>の値を以下の形式で標準エラー出力に出力する<br/>
     <code>"%.8g\n", &lt;output gain in dB, floating point&gt;</code>
</dd>
<dt>-V</dt>
<dd>出力ゲインの編集<strong>前</strong>の値を以下の形式で標準エラー出力に出力する<br/>
     <code>"%d\n", &lt;output gain in Q7.8, integer&gt;</code>
</dd>
<dt>-c file</dt>
<dd>出力モード時、タグをfileに書き出す。書き込み・追記モード時、fileからタグを読み出す</dd>
<dt>-t NAME=VALUE</dt>
<dd>引数をタグとして追加する</dd>
</dl>

## ライセンス

MITライセンス
