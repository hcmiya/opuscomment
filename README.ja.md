# opuscomment

## 概要

<dfn>opuscomment</dfn>は[vorbiscomment](https://github.com/xiph/vorbis-tools)と互換のインターフェイスを持つOgg Opus用の編集ユーティリティです。Opusの重要な機能の一つである出力ゲインの編集が出来ることを第一の特徴としています。また、複数行にまたがるタグを常に安全にエスケープすること、他のソフトへの組み込みを容易にすることも目標としています。

## 特徴

### 出力ゲインの編集に対応

Ogg Opusはヘッダに出力ゲイン(output gain)という項目があり、エンコード後にも自在に音量を変更できるという機能が標準で備わっています。opuscommentはその任意書換に対応した2017年現在では数少ないツールです。

例えば作った効果音の音が大き過ぎたので小さくしたい、例えば動画サイトで音量基準を合わせる必要がある、そんな時、他のコーデックなら再エンコードが必要になるでしょう。あるいはサービス毎に互換性の無い方法で音量管理用のデータを維持し、適用させるというプログラムを書く必要があるでしょう。Opusならばヘッダの2バイトを書き換えるだけで良いのです。

### vorbiscommentと互換性のある編集様式

vorbiscommentと互換した書式・エスケープとオプションを備えることにより、Ogg Vorbisからの移行を音声ファイルそのものとそれを扱うアプリケーションに対しても容易にします。例えば、VorbisからOpusへ同じタグを移植するには以下のコマンドで容易に出来ます。

    vorbiscomment -lRe music.oga |opuscomment -wRe music.opus

また、opuscommentの基本的なオプションはvorbiscommentのそれとほぼ同じであり、既存のVorbisに対するスクリプトはコマンド名を書き換えるだけでOpusに対応できるようになるでしょう。

### メモリ使用量の抑制

opuscommentはタグの編集にメモリをその長さに合わせて確保するということはありません。また、それを取り出す必要のあるパケットをメモリに持つということもありません。そのため壊れたファイルや悪意のある編集に対して比較的強く、安全に他のアプリケーションに組み込むことが出来ます。

### タグの安全な取扱い

opuscommentはタグの内容が改行を含む場合、常にそれをエスケープします。改行によって内容が分断されたり、またそれによって次の行が項目の開始と誤認識されることは起こりません。エスケープは改行の後に行頭でタブを続ける直感的に理解しやすい方法と、vorbiscommentと同じ“\n”を使ったプログラマにとって親しみのある方法の2種類です。

2つ目に、編集入力を受け入れる前段のフィルタが不慮に終了し内容が中断してしまった場合でも、それでOpusファイルがタグを失う可能性を軽減することが出来るオプションを用意しました。詳しくは`-V`、`-T`、`-D`及び[hcmiya/opuscomment#6](https://github.com/hcmiya/opuscomment/issues/6)の説明をご覧下さい。

## コンパイル・動作要件

* C99
* POSIX.1-2008またはPOSIX.1-2001 XSI拡張(SUSv3)に対応したAPI
* libogg

尚、libopus、libopusfileは必要ありません。configureスクリプトを使用していないので、上記要件を満たすことや対応するヘッダファイルが存在すること(Debianならば`libogg-dev`をインストール)をコンパイル前にご確認下さい。Makefileのコメントも参照下さい。

## インストール

    $ make

をすると`src/`に`opuscomment`という名前でバイナリが出力されますのでそれを任意の場所にコピーします。

### メッセージカタログ

opuscommentはX/Open NLSを使った地域化を実装しています。`nls/`にカタログのソースを用意していますので、必要なロケールに対して`gencat(1)`を利用してカタログファイルに変換した後に適当なディレクトリに保存して下さい。

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

### バイナリの別名

`opuscomment`はバイナリの名前で編集する対象のコーデックを変更する機能があります。対応コーデックは以下の表の通りです。インストールの際はopuscommentにリンクを張ると良いでしょう。

| argv[0] | コーデック |
|--|--|
| (下記以外の全て) | Opus |
| theoracomment | Theora |
| vorbiscomment | Vorbis |
| daalacomment | Daala |
| speexcomment | Speex |
| oggpcmcomment | PCM |
| ogguvscomment | UVS |

上記のいずれも編集が出来るのはOggに含まれている場合のみです。

## ライセンス

### opuscomment

MITライセンス

### libogg

[libogg](https://www.xiph.org/ogg/)は[Xiph.Org Foundation](https://www.xiph.org/)の著作物です。ライセンスはLICENSE.liboggを参照下さい。
