# Aoto VST Router

ゲーム実況・歌枠用の「VST をかけて出力するだけ」の軽量 VST ホスト（Windows）。
DAW を立ち上げずに、マイク → VST → 配信用出力（RME のループバック）に流せます。録音機能はありません。

## できること

- **Main チャンネル**：マイクに EQ / コンプ / ディエッサー などを上から順にかける
- **FX チャンネル**：Main の VST 後の声を受けて、エコー / リバーブをかける（フェーダーで量を調整）
- VST3 の読み込み・ON/OFF・並べ替え（左の ⠿ をドラッグ）・差し替え・プラグイン画面の表示
- 配信出力（ループバックする出力）とモニター出力（ヘッドホン）を別々に指定
- ステレオ / モノラル出力の切り替え
- プリセット保存・読み込み（`ドキュメント\Aoto VST Router\Presets`）
- **前回終了時の状態を自動復元**（起動するだけで配信できる）
- ASIO 対応・レイテンシー表示（設定画面）
- Windows 起動時に自動起動（設定画面でオン/オフ）。オーディオインターフェースの準備ができるまで待ってから接続

## 信号の流れ

```
マイク入力 → Main の VST → Main フェーダー/パン ─┐
             └→ FX の VST → FX フェーダー/パン ───┴→ 配信出力（ループバック）
                                                    （ヘッドホンボタン ON のものだけ → モニター出力）
```

FX のエコー / リバーブはプラグイン側の Mix を **100% Wet** にすると扱いやすいです。

## 低レイテンシー設定（Studio One と同じくらいにする）

1. 左メニュー「設定」→ デバイスの種類を **ASIO**、デバイスを RME のドライバ（例：ASIO Fireface USB）にする
2. 「Control Panel」ボタン → RME の設定で **Buffer Size を 96** にする（RME はドライバ側で決まる）
3. 設定画面のレイテンシー表示で、オーディオラウンドトリップが Studio One（約 9ms / 431 サンプル）と同じくらいになっていれば OK

このアプリはプロセスブロックを追加で分割しないので、遅延は「ドライバ ＋ 先読みするプラグインの分」だけです。

## RME（TotalMix）でのループバック配信

1. 「出力（ルーティング）」の出力デバイスを、TotalMix で Loopback を ON にした出力（例：AN 1/2）にする
2. OBS / TikTok LIVE Studio のマイク入力に、その出力と同じ番号の録音デバイス（例：Analog 1+2）を選ぶ
3. マイクの生音がループバックに混ざらないよう、TotalMix 側でその出力へのマイク直送りは下げておく

## ビルド方法

### GitHub Actions（おすすめ）
このフォルダを GitHub リポジトリに push すると、Actions が Windows 用 exe をビルドします。
Actions → 最新の実行 → Artifacts の `AotoVSTRouter-Windows-x64` をダウンロード。

### 自分の PC でビルド
Visual Studio 2022（C++ デスクトップ開発）と CMake を入れて：
```
cmake -S . -B build -A x64
cmake --build build --config Release
```
`build\AotoVSTRouter_artefacts\Release\Aoto VST Router.exe` ができます。

JUCE 9.0.4（自動ダウンロード）を使用。ASIO は JUCE 同梱のヘッダーを使っています。
