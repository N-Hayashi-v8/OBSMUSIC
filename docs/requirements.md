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

## 方針転換: OBS プラグインにする（2026-11-20）

- 理由: 毎回アプリを起動する手間をなくし、OBS を開けば使える形にしたい。配布も、プラグインを入れるだけで済ませたい。
- 目的は C# の習得ではなく、Claude Code を使った学習（利用者の回答）。このため言語は C/C++ に変える。
- 単体アプリ（src/ObsMusic）は試作として残す。曲名表示（SMTC の読み取り）と音声の診断は、プラグインでの作り方を決めるときの参考にする。
- 期待できること（未確認）: プラグインなら、obs-websocket にない「OBS 全体のモニタリングデバイスの変更」ができる可能性がある。libobs の公開関数を調べて確かめる。
- 環境の確認結果: OBS Studio 33.0.1、Visual Studio 2022 Build Tools（MSVC 14.44、CMake、Ninja）が入っている。公式 obs-plugintemplate の Windows 要件は Visual Studio 17 2022 と Windows SDK 10.0.22621。SDK の有無は未確認。
- 公式のビルド手順: `cmake --preset windows-x64` → `cmake --build --preset windows-x64`。設定時に OBS の依存ファイルをダウンロードする（量は未確認）。

## 未確認

- 取得できなくなる原因（ウィンドウタイトルが曲ごとに変わるためか）
- 配布時に空のデバイスをどう用意してもらうか

## プラグインのビルド（2026-11-20）

- `plugin/` に公式 obs-plugintemplate を取得した（.git は外した。取得元は github.com/obsproject/obs-plugintemplate）。
- Windows SDK が 10.0.26100 だけなので、`plugin/CMakePresets.json` の `version=10.0.22621` を `10.0.26100` に変えた。これで設定とビルドが通った。
- 手順（cmake は VS Build Tools 同梱）: `cmake --preset windows-x64` → `cmake --build --preset windows-x64`。設定に約2分かかる（OBS 31.1.1 のソースと依存ファイルを取得してビルドする）。
- 結果: 空のプラグイン `plugin/build_x64/RelWithDebInfo/plugintemplate-for-obs.dll` ができた。OBS への読み込みは未確認。
- 注意: テンプレートの対象は OBS 31.1.1。手元の OBS は 33.0.1。

## プラグインの読み込み確認（2026-11-20）

- 結果: OBS 33.0.1 のログに `plugin loaded successfully (version 1.0.0)` が出た。テンプレート（31.1.1 対象）のまま 33.0.1 で読み込める。
- 置き場所: Windows でユーザーが入れるプラグインは `C:\ProgramData\obs-studio\plugins\<名前>\bin\64bit\<名前>.dll`（データは同じ階層の `data\`）。`%APPDATA%\obs-studio\plugins` は探されない（OBS 31.1.1 のソース OBSBasic.cpp で確認）。
- 気になる点: ログに `Failed to load 'en-US' text for module` が出る。ロケールの置き場所の問題と思われるが、原因は未確認（動作には影響しない）。

## プラグインで曲名を読む（2026-11-20）

- `plugin/src/nowplaying.cpp`: 別スレッドで 2 秒ごとに SMTC を読み、曲が変わると OBS のログに `now playing: 曲名 / アーティスト` を出す。C++/WinRT（Windows SDK 同梱）で書き、新しい依存は入れていない。`CMakeLists.txt` に `windowsapp` のリンクを足した。
- 確認: OBS 33.0.1 で、YouTube Music の曲が変わるたびにログへ出た（日本語も文字化けなし）。
- 制限: 「現在のセッション」1つだけを見る（試作と同じ）。
- 次: 読んだ曲を OBS の画面に出す。案は、プラグインが小さな HTTP サーバを持ち、試作の index.html をそのまま配って、OBS のブラウザソースで開く。決めてから作る。
- 更新手順: OBS が dll を使用中だと上書きできない。OBS を閉じてから、ビルドした dll を `C:\ProgramData\obs-studio\plugins\plugintemplate-for-obs\bin\64bit\` に入れる。

## 曲名を描くソース（2026-11-20）

- 表示の方式は、プラグインが専用のソース「OBSMUSIC 曲名」（id: obsmusic_card）を登録し、自分で描く形に決めた（利用者の選択。サーバを使わず OBS の中で完結させるため）。
- `plugin/src/card.cpp`: 曲が変わったときだけ Direct2D と DirectWrite で 720×112 の絵（角丸の半透明の背景、曲名 28px 太字、アーティスト 20px、フォントは Yu Gothic UI）を描き、テクスチャで OBS に渡す。再生中でなければ透明。リンクに d2d1、dwrite、windowscodecs を足した（どれも Windows 標準）。
- 確認: OBS 33.0.1 で、ソースを追加すると表示された（利用者が確認）。
- 未対応: ジャケット、プロパティ画面（文字の大きさや色）、幅の自動調整。
- 見直しで見つけた点: 読み取りの失敗を受け止める範囲がループ全体で、一度失敗すると止まる。OBS の描画スレッドの中で絵を描いている。
- 配布前に決めること: ライセンス（テンプレートは GPL-2.0）、プラグイン名（まだ plugintemplate-for-obs）。
- 調べたい案: obs-browser のイベント送信を使えば、サーバなしで HTML/CSS の見た目にできるかもしれない（未確認）。
