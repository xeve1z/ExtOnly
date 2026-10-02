# ExtOnly

エクスプローラーで複数のファイルを選択したとき、右クリックメニューの「選択」から
**拡張子やフォルダーで絞り込める** Windows 用ツールです。

```
ファイルを複数選択 → 右クリック →「選択 ▸ png / jpg / mp4 / folder」
    → 選んだ種類だけが選択された状態になり、それ以外の選択が外れる
```

## 特徴

- 選択中のファイルに含まれる**拡張子だけ**をメニューに並べて絞り込み
- **フォルダー**も1つの種類として絞り込み可能（`folder`）
- 種類が2つ以上あるときだけメニューを表示（絞り込む意味がないときは出ません）
- 大量選択にも対応
- **常駐しません**（エクスプローラーに組み込まれる「シェル拡張」方式。7-Zip と同じ仕組み）
- インストールは**自分専用・管理者権限（UAC）不要**

## 動作環境

- Windows 10（64bit）
- Windows 11 は未検証です

## ダウンロード

[Releases](https://github.com/xeve1z/ExtOnly/releases/latest) から `ExtOnlySetup.exe` をダウンロードしてください。

> 署名を行っていないため、初回起動時に「Windows によって PC が保護されました」という警告が表示されることがあります。
> その場合は「詳細情報」→「実行」で進めてください。

## インストール

1. `ExtOnlySetup.exe` を実行します
2. ウィザードに従います（管理者権限は不要です）
3. 新規インストールの場合、**再起動なしでそのまま使えます**

インストール先：`%LocalAppData%\ExtOnly\`

## 使い方

1. エクスプローラーでファイルを複数選択します
2. 右クリック →「選択」
3. 絞り込みたい拡張子（または `folder`）をクリック
4. その種類のファイルだけが選択された状態になります

## アンインストール

- 設定 → アプリ → ExtOnly → アンインストール
- または `%LocalAppData%\ExtOnly\unins000.exe` を実行

> すでにインストールされている状態で `ExtOnlySetup.exe` を実行すると、
> アンインストールするか確認されます（インストーラーがアンインストーラーとして動作します）。

## 仕組み

ExtOnly は常駐アプリではありません。Windows の**シェル拡張**としてエクスプローラーに組み込まれ、
右クリックされたときだけ動作します。バックグラウンドで常に動くプロセスはありません。

## ソースからビルド

### 必要なもの

- [Visual Studio](https://visualstudio.microsoft.com/)（「C++ によるデスクトップ開発」ワークロード）
- Windows SDK
- [Inno Setup 6](https://jrsoftware.org/isinfo.php)（インストーラーを作る場合）

### ビルド

```powershell
# DLL のみビルド
powershell -ExecutionPolicy Bypass -File .\build.ps1

# インストーラーまで作成（dist\ExtOnlySetup.exe）
powershell -ExecutionPolicy Bypass -File .\build-installer.ps1
```

- `build.ps1 -Debug` でログ出力付きのデバッグ版になります
  （ログ：`%LocalAppData%\ExtOnly\extonly.log`）

### ファイル構成

| ファイル | 役割 |
| --- | --- |
| `src/` | シェル拡張本体（C++） |
| `ExtOnly.def` | DLL のエクスポート定義 |
| `build.ps1` / `build-installer.ps1` | ビルドスクリプト |
| `installer.iss` | インストーラーの定義（Inno Setup） |
| `register.ps1` / `unregister.ps1` | 開発用手動登録・解除 |

### 手動での登録（開発用）

```powershell
# ビルドして登録（エクスプローラーを再起動）
powershell -ExecutionPolicy Bypass -File .\build.ps1
powershell -ExecutionPolicy Bypass -File .\register.ps1 -RestartExplorer

# 登録解除
powershell -ExecutionPolicy Bypass -File .\unregister.ps1
```

## ライセンス

MIT License（[LICENSE](LICENSE) を参照）
