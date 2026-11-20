# OBSMUSIC

Discord の Go Live でPCゲームを配信しながら、PCで流しているBGM も一緒に流したい人のための OBS Studio プラグイン（Windows 用）です。

Go Live は Discord のボイスチャンネルで、自分の画面やゲームをほかの人に見せる機能です。

- 曲名カード: 再生中の曲の曲名、アーティスト、ジャケットを、OBS の画面に出します。YouTube Music など、Windows のメディア操作（音量キーで出る再生中の表示）に対応したアプリの曲を読みます。
- 音声の診断: 音が二重に聞こえる設定になっていないかを確かめます。
- モニタリングデバイスの設定: 配信用の音を流すデバイスを、一覧から選んで設定します。

## 動作環境

- Windows 10 / 11（64bit）
- OBS Studio 33（33.0.1 で確認）

## 入れ方

1. [最新版のダウンロードページ](https://github.com/N-Hayashi-v8/OBSMUSIC/releases/latest) を開きます。ページの下のほうにある「Assets」の一覧から、`obsmusic-<バージョン>-windows-x64.zip` を押すと、ダウンロードが始まります。
2. OBS を閉じます。
3. ダウンロードした zip をダブルクリックして開きます。
4. エクスプローラーの上のアドレス欄に `C:\ProgramData` と入力して、Enter キーを押します。ProgramData は、ふだんは表示されないフォルダですが、この方法で開けます。
5. ProgramData の中の `obs-studio` フォルダを開き、その中の `plugins` フォルダを開きます。どちらもなければ、その名前で新しいフォルダを作ってください。
6. 3 で開いた zip の中の `obsmusic` フォルダを、`plugins` フォルダの中にコピーします。管理者の権限を求める画面が出たら、「続行」を押してください。
   次の形になれば正解です。
   ```
   C:\ProgramData\obs-studio\plugins\obsmusic\bin\64bit\obsmusic.dll
   C:\ProgramData\obs-studio\plugins\obsmusic\data\locale\en-US.ini
   ```
7. OBS を開きます。

## 音の出ないデバイスを用意する

Go Live で BGM を届けるには、OBS が拾った BGM を「モニタリング」で鳴らし、Discord にその音を拾わせます。そのままでは、BGM が自分のスピーカーから二重に聞こえます。モニタリングの音を音の出ないデバイスに流せば、二重に聞こえなくなります。

音の出ないデバイスには無料の仮想デバイス **VB-CABLE** をおすすめします。

1. 公式サイト <https://vb-audio.com/Cable/> から、Windows 用のパッケージ（zip）をダウンロードします。似た名前のサイトもあるので、配布元が vb-audio.com であることを確かめてください。
2. zip をすべて展開し、中のセットアップを管理者として実行します。
3. PC を再起動します。
4. Windows の既定の再生デバイスが VB-CABLE に切り替わって、PC の音が聞こえなくなったときは、既定をいつものスピーカーに戻してください。

VB-CABLE は寄付制のソフトウェアで、使うだけなら無料です。

モニターの HDMI 出力など、スピーカーをつないでいない出力も、音の出ないデバイスとして使えます。

## 使い方

### 曲名カードを出す

1. OBS の「ソース」の「＋」から「OBSMUSIC 曲名」を選んで追加します。
2. 曲を流すと、カードが出ます。再生を止めると消えます。
3. ソースをダブルクリックすると、フォント、文字と背景の色、幅を変えられます。「幅を曲名の長さに合わせる」がオンのときは、カードの幅が曲名に合わせて変わります。このとき「幅」はその上限になります。

### BGM を Go Live で共有する

1. BGM を流しているアプリ（ブラウザなど）を、OBS のウィンドウキャプチャやアプリケーション音声キャプチャで取り込みます。
2. その入力の「オーディオの詳細プロパティ」で、音声モニタリングを「モニターと出力」にします。
3. OBS の「ツール」から「OBSMUSIC モニタリングデバイスの設定」を開き、音の出ないデバイス（VB-CABLE なら「CABLE Input」）を選んで「OK」を押します。
4. 「ツール」の「OBSMUSIC 音声の診断」で、設定を確かめられます。
5. Discord の Go Live で OBS を共有します。

## 制限

- 曲は、Windows が「今のメディア」として扱っている1つだけを読みます。複数のアプリで同時に再生していると、目的の曲にならないことがあります。
- ジャケットは、曲名が変わったときに1回だけ読みます。

## ソースからビルドする

Visual Studio 2022（C++ のビルドツール）と CMake が要ります。

```
cd plugin
cmake --preset windows-x64
cmake --build --preset windows-x64
```

できた `plugin/build_x64/rundir/RelWithDebInfo/obsmusic.dll` を、上の「入れ方」の場所に置きます。このプラグインは [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate) をもとに作っています。

## ライセンス

GPL-2.0 以降（[LICENSE](LICENSE)）。OBS Studio（libobs）が GPL のため、このプラグインも GPL で公開しています。
