# 要件メモ

ユーザーの箇条書き（2026-11-19）をそのまま整理したもの。まだ増える予定。

## 困りごと

1. Discord でゲーム配信とBGM共有をするとき、次の設定が全部わかりづらく面倒。簡単にできるようにしたい。
   - モニタースピーカーに空のサウンドデバイスを設定する（音が二重にならないようにするため）
   - 画面をいい感じに切り貼りする
   - プレビュー画面をフルスクリーンで出す
2. 画面に曲名を出すとき、YouTube Music の画面を切り貼りすると横の比率が変わって面倒。

## 前提（回答済み）

- 配信は Discord の Go Live。
- 音が二重になるのは、OBS が拾った音と、同じ音が直接スピーカーから鳴るため。OBS に拾わせる音は仮想デバイスに流す必要がある。
- 空のデバイスは VB-CABLE ではなく Yamaha SYNCROOM Driver。OBS のモニタリングデバイスにこれを指定している（スクリーンショットで確認）。

## 現状の設定（2026-11-19 回答）

- OBS: 設定 → 音声 → モニタリングデバイス = ライン (Yamaha SYNCROOM Driver (WDM))
- 音源「ウィンドウキャプチャ2」: 音声モニタリング有効、トラック1〜6
- Discord: アプリの音声を拾う形
- 現状でおおむね満足。困りごとは、YouTube Music のブラウザアプリを取得できなくなることがある点。

- 「ウィンドウキャプチャ2」は映像と音の両方の取り込みに使っている。
- 取得できなくなったときは、毎回このソースのウィンドウを YouTube Music に選び直している。
- 他の人も使えるように、アプリとして配布したい（ゲーム中の BGM 共有を、他の人にもできるようにしたい）。

## 調べたこと（obs-websocket 公式 protocol.md、2026-11-19）

- あるもの: GetInputList, GetInputSettings, SetInputSettings, GetInputPropertiesListPropertyItems, SetInputAudioMonitorType, OpenSourceProjector, OpenVideoMixProjector。
- ないもの: OBS 全体のモニタリングデバイスを変える要求。ここは自動化できず、利用者が一度手で設定する前提になる。
- 各要求の項目は未読。実装前に該当部分を読む。
- 「取得できなくなる」原因がウィンドウタイトルの変化かどうかは未確認。

## 音声の診断（実装済み、読むだけ）

- `dotnet run -- audio` で、OBS のモニタリングデバイスと各入力のモニタリング設定、Windows の再生デバイスを並べる。モニタリングが既定のデバイスなら、二重に聞こえると知らせる。
- モニタリングデバイスは obs-websocket から読み書きできないので、`%APPDATA%\obs-studio\user.ini` の `ProfileDir` と、そのプロファイルの `basic.ini` の `MonitoringDeviceId` / `MonitoringDeviceName` を読んでいる。OBS 起動中に書いても反映されない見込みなので、書き換えはしない。
- 入力ごとのモニタリング設定（NONE / MONITOR_ONLY / MONITOR_AND_OUTPUT）は obs-websocket で読み書きできる。診断では読むだけ。
- 本人の環境では、ゲームと BGM の入力が MONITOR_AND_OUTPUT、モニタリングデバイスが SYNCROOM。

## モニタリング設定の書き換え（実装済み）

- `dotnet run -- monitor 入力名 none|monitor|both` で、入力のモニタリング設定を変える（monitor はモニターのみ、both はモニターと出力）。すでに同じ値なら何もしない。
- 動作確認は、音のない入力（notion1）で none → monitor → none と往復して行った。none には戻せた。
- 気になる点: monitor を設定した直後の読み戻しが MONITOR_AND_OUTPUT だった（OBS の画面が「有効にする」の1つのチェックになっていることと関係がありそうだが、原因は未確認）。monitor を使う前に確かめる。
- モニタリングデバイスは書き換えない（上の「音声の診断」を参照）。

## 未確認

- 取得できなくなる原因（ウィンドウタイトルが曲ごとに変わるためか）
- 配布時に空のデバイスをどう用意してもらうか
