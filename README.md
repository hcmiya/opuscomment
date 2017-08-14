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

## マニュアル

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
<dt>-U</dt>
<dd>Opusファイル内のタグのフィールド名に小文字が含まれていた場合、大文字に変換する。他のソフトウェアで編集されたファイルのために用意されているオプションであり、opuscommentは編集入力のタグのフィールド名を常に大文字に変換する。</dd>
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

### 環境変数

<dl>
<dt>LANG</dt>
<dd>vorbis commentのUTF-8とロケールの文字符号化方式との変換に影響を受ける</dd>
<dt>LC_NUMERIC</dt>
<dd>出力ゲイン編集に使う浮動小数点数の書式に影響を受ける</dd>
<dt>LC_MESSAGES, NLSPATH</dt>
<dd>メッセージカタログの処理に関わる</dd>
</dl>

### 返り値

Opusファイルの編集に成功した場合は`0`、オプションや編集タグ入力の文法に誤りがあった場合は`1`、Opusファイルのフォーマットに誤りがあった場合は`2`、ファイル入出力などシステム起因のエラーが発生した場合は`3`を返す。

### 文法

opuscommentで扱うタグ入出力の文法について、個々のレコードはvorbis commentの内部形式と同じで`NAME=VALUE`のようにキー名と値を`=`で繋いだだけの簡単なものである。但し、`VALUE`は改行を含む可能性があり、opuscommentは2つの方法で改行をエスケープする。

1. opuscommentが定義する方法: 「改行の次にタブが続いた場合、改行後の行は先頭のタブを除き前の行の値の続きとして扱う」
2. `-e`オプションを用いた時の`vorbiscomment(1)`との互換のある方法: 「バックスラッシュを使ったエスケープシーケンスで改行を表す」

opuscommentではこのいずれかの改行のエスケープが**常に**適用されており、適切なオプションを指定と編集があれば改行が欠落することはない。具体的に、次の内容を持つレコード:

    フィールド名:
    COMMENT
    
    内容:
    荒川智則のライブ
    2017-08-12録音

これは1つ目の`opuscomment`の方法だと

    COMMENT=荒川智則のライブ<newline>
    <tab>2017-08-12録音

2つ目の`vorbiscomment`互換形式だと

    COMMENT=荒川智則のライブ\n2017-08-12録音

となる。

### 例

#### opuscomment方式のエスケープを編集する場合

エンコードのやり直しのために同じタグを別のOpusファイルにコピーするという状況を考える。この時、opuscomment同士を直接パイプで繋いでタグの受け渡しを行うことは安全である。

    # 安全な例
    opuscomment old.opus |opuscomment -w re-encoded.opus

しかし、行の削除を含む編集をするフィルタを挟むことは安全ではなくなる可能性がある。なぜなら、もし削除したいレコードが複数行からなっていた場合、そのフィールド名を含む行だけ削除をすると残りの行が1つ前のレコードの続きと見做されてしまうからである。

    # 安全ではない例
    opuscomment old.opus |sed '/^COMMENT=/d' |opuscomment -w re-encoded.opus

これを防ぐためには、レコードが複数行に跨ることを考慮してフィルタを設計する必要がある。

    # 複数行のレコードを考慮した削除の例1
    opuscomment old.opus |sed '/^COMMENT=/{:loop; N; s/.*\n<tab>//; t loop; D;}' |
      opuscomment -w re-encoded.opus

より単純には、-eオプションのエスケープを使用することである。

    # 複数行のレコードを考慮した削除の例2
    opuscomment -e old.opus |sed '/^COMMENT=/d' |opuscomment -we re-encoded.opus

#### Opusファイルの同時編集

シェルスクリプトの一般論として、1つのファイルをパイプを繋いで同時に編集しようとすると書き込みのタイミングにより内容が消えてしまうため、結果を一度別ファイルにリダイレクトしてリネームするという処理をするのが定石である。

    sed 's/dog/cat/g' <animal.txt >animal.txt.1
    mv -f animal.txt.1 animal.txt

しかし、opuscommentはタグの読み込みが終わるまでOpusファイルを書き込み用として開かないため、フィルタの前後で同じファイルを開いていても同時に編集されることはなく内容は失われる事は無い。

    # 一時ファイルを作らなくてもsome.opusからDISCTOTALとDISCNUMBERタグを消す編集が意図通り適用される。
    opuscomment -e some.opus |grep -vE '^DISC(TOTAL|NUMBER)=' |opuscomment -we some.opus

### 注意

#### NULの扱い

opuscommentは文字「NUL」が入力された場合は一切エラーとする。また`vorbiscomment(1)`との互換として`\0`のエスケープを解釈するが、その後ろに続く文字列が切り捨てられたような動作をする。これはvorbis commentがUTF-8テキストを格納するものでバイナリを受け入れるべきではないという設計に基いており、初版作者はその設計思想を受け継いでバイナリファイルが入力された時にテキストファイルが壊れてしまうという動作を意図的に発現させているためである。この動作は初版作者によって修正されない。

#### 出力ゲインとR128_TRACK_GAIN、R128_ALBUM_GAINの編集

Opus仕様を定めた[RFC 7845](https://tools.ietf.org/html/rfc7845)によれば、出力ゲインを編集した場合、併せて`R128_TRACK_GAIN`、`R128_ALBUM_GAIN`の更新ないし削除をしなければならない(MUST)、とある。しかし、opuscommentは今の所その規定に基く処理は未実装である。opuscommentの利用者はこの規定を念頭に置いてゲイン調整の編集をスクリプトに組み込む必要がある。

### 関連項目

`opusenc(1)`, `opusinfo(1)`, `vorbiscomment(1)`, `metaflac(1)`, `op_set_gain_offset(3)`

## ライセンス

MITライセンス
