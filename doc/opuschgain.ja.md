# OPUSCOMMENT

`opuschgain` — Ogg Opusファイルの出力ゲインとR128ゲインタグを更新

## 書式

    opuschgain {-g gain|-s scale|-0} [-i idx] [-1Qr] opusfile [output]

## 説明

opuschgainは、Ogg Opusファイルの出力ゲインの編集をする。もしopusfileがR128_TRACK_GAIN・R128_ALBUM_GAINタグを持つ場合、それを併せて更新する。この2つのタグの情報については[RFC 7845 §5.2.1.](https://tools.ietf.org/html/rfc7845#section-5.2.1)を参照。

## オプション

<dl>
<dt>-g gain</dt>
<dd>出力ゲインをdBで指定する</dd>
<dt>-s scale</dt>
<dd>出力ゲインをPCMサンプルの倍率で指定する。1で等倍。0.5で半分(コマンド内でdBに変換)</dd>
<dt>-0</dt>
<dd>出力ゲインを0にする</dd>
<dt>-r</dt>
<dd>出力ゲインの指定を内部の設定に対する相対値とする</dd>
<dt>-1</dt>
<dd>出力ゲインが内部形式にした時に0になる場合は±1/256 dBを設定する</dd>
<dt>-Q</dt>
<dd>出力ゲイン値にQ7.8形式を使う</dd>
<dt>-i idx</dt>
<dd>多重化されたOggストリーム中の編集対象のOpusストリームを、Opus以外のものを除いた1起点の順番で指定する。動画ファイルが吹き替えや副音声などで複数の音声ストリームを持つという状況を想定している</dd>
</dl>

## 関連項目

`opuscomment(1)`

opuschgainはopuscommentがR128関連タグを修正しない動作を補完するためにopuscommentと同時に配布されるスクリプトである。
