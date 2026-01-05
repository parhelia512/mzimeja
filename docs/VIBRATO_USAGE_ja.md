# Vibrato使用方法ガイド

## 概要

MZ-IME Japanese Input (mzimeja) にVibrato形態素解析エンジンを統合しました。Vibratoは高速で正確な日本語形態素解析を提供するRust製のライブラリです。

## 必要な環境

### 前提条件

- Windows 7以降
- CMake 3.10以降
- C++コンパイラ（MSVC、MinGW、またはClang）
- **Rust/Cargo**（Vibratoサポートを有効にする場合）

### Rustのインストール

Vibratoを使用するには、Rustツールチェーンが必要です：

1. [rustup](https://rustup.rs/)から最新のRustをインストール
2. インストール後、コマンドプロンプトで確認：
   ```cmd
   cargo --version
   ```

## ビルド方法

### Vibratoサポートなし（デフォルト）

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### Vibratoサポートあり

```bash
mkdir build
cd build
cmake .. -DUSE_VIBRATO=ON
cmake --build .
```

`USE_VIBRATO=ON`を指定すると：
1. CMakeがCargoを検出
2. `third_party/vibrato-c`ディレクトリでRustライブラリをビルド
3. 生成された静的ライブラリをIMEにリンク

### ビルドの注意点

- Releaseビルドの場合、Cargoも`--release`フラグでビルドされます
- Debugビルドの場合、Cargoはデバッグモードでビルドされます
- ビルド成果物（`target/`、`Cargo.lock`）は`.gitignore`で除外されています

## 辞書の準備

Vibratoは圧縮された辞書ファイル（`.zst`形式）を使用します。

### MeCab IPA辞書の入手

1. [MeCab IPA辞書](https://github.com/taku910/mecab/tree/master/mecab-ipadic)をダウンロード
2. Vibratoの辞書変換ツールを使用して変換：
   ```bash
   # Vibratoリポジトリから辞書変換ツールを使用
   # 詳細: https://github.com/daac-tools/vibrato
   ```

### 辞書の配置

辞書ファイルを以下のいずれかの場所に配置：
- `<mzimeja.imeのディレクトリ>/vibrato_dict.zst`
- 環境変数で指定したパス
- レジストリで指定したパス

## 使用方法

### プログラムからの使用

```cpp
#include "vibrato_engine.h"

// エンジンのインスタンス作成
VibratoEngine engine;

// 辞書の読み込み
if (engine.Initialize(L"path/to/vibrato_dict.zst")) {
    // 文節変換
    MzConvResult result;
    if (engine.ConvertMultiClause(L"きょうはいいてんきです", result)) {
        // 変換結果を使用
        std::wstring converted = result.get_str();
        // -> "今日はいい天気です"
    }
}
```

### エンジンの機能

#### 1. AnalyzeToLattice
形態素解析を実行し、ラティス構造を生成：

```cpp
Lattice lattice;
engine.AnalyzeToLattice(L"形態素解析", lattice);
```

#### 2. ConvertMultiClause
複数文節の変換：

```cpp
MzConvResult result;
engine.ConvertMultiClause(L"ひらがなのぶんしょう", result);
```

#### 3. ConvertSingleClause
単一文節の変換：

```cpp
MzConvResult result;
engine.ConvertSingleClause(L"たんご", result);
```

## アーキテクチャ

```
MZ-IME (C++)
    ↓
VibratoEngine (C++, 414行)
    ↓
vibrato-c (Rust FFI, 148行)
    ↓
Vibrato (Rust v0.5.2)
```

### データフロー

1. **入力**: UTF-16文字列（`std::wstring`）
2. **変換**: UTF-8文字列（Vibrato用）
3. **解析**: Vibratoによる形態素解析
4. **マッピング**: MeCab品詞 → MZ-IMEja品詞
5. **出力**: Lattice構造 → MzConvResult

## 品詞マッピング

Vibratoは MeCab IPA辞書の品詞体系を使用します。以下のようにMZ-IMEjaの品詞にマッピングされます：

| MeCab品詞 | MZ-IMEja品詞 | 説明 |
|-----------|--------------|------|
| 名詞 | HB_MEISHI | 名詞 |
| 動詞（五段） | HB_GODAN_DOUSHI | 五段動詞 |
| 動詞（一段） | HB_ICHIDAN_DOUSHI | 一段動詞 |
| 動詞（カ変） | HB_KAHEN_DOUSHI | カ変動詞 |
| 動詞（サ変） | HB_SAHEN_DOUSHI | サ変動詞 |
| 形容詞 | HB_IKEIYOUSHI | い形容詞 |
| 形容動詞 | HB_NAKEIYOUSHI | な形容詞 |
| 助詞（格） | HB_KAKU_JOSHI | 格助詞 |
| 助詞（接続） | HB_SETSUZOKU_JOSHI | 接続助詞 |
| 助詞（副） | HB_FUKU_JOSHI | 副助詞 |
| 助詞（終） | HB_SHUU_JOSHI | 終助詞 |
| 助動詞 | HB_JODOUSHI | 助動詞 |
| 副詞 | HB_FUKUSHI | 副詞 |
| 連体詞 | HB_RENTAISHI | 連体詞 |
| 接続詞 | HB_SETSUZOKUSHI | 接続詞 |
| 感動詞 | HB_KANDOUSHI | 感動詞 |
| 接頭詞 | HB_SETTOUJI | 接頭辞 |
| 接尾 | HB_SETSUBIJI | 接尾辞 |
| 記号（句点） | HB_PERIOD | 句点 |
| 記号（読点） | HB_COMMA | 読点 |
| 記号（その他） | HB_SYMBOL | 記号類 |

## トラブルシューティング

### ビルドエラー

**問題**: `Cargo not found`
- **解決**: Rustをインストールし、PATHに`cargo`が含まれていることを確認

**問題**: Vibratoライブラリのリンクエラー
- **解決**: CMakeを再実行し、`VIBRATO_LIBRARY`が正しく設定されていることを確認

### 実行時エラー

**問題**: 辞書の読み込みに失敗
- **解決**: 辞書ファイルのパスが正しいか確認
- **解決**: 辞書ファイルが正しいVibrato形式（`.zst`）か確認

**問題**: 文字化け
- **解決**: 辞書ファイルがUTF-8エンコーディングであることを確認

## パフォーマンス

Vibratoは高速な形態素解析エンジンです：

- **初期化**: 辞書読み込みは初回のみ（数秒）
- **解析速度**: 1文あたり数ミリ秒
- **メモリ使用量**: 辞書サイズに依存（通常50-100MB）

## ライセンス

- **MZ-IMEja**: GPLv3
- **Vibrato**: MIT OR Apache-2.0
- **vibrato-c**: MIT OR Apache-2.0（Vibratoに準拠）

ライセンスの互換性は確認済みです。

## 参考資料

- [Vibrato公式リポジトリ](https://github.com/daac-tools/vibrato)
- [MeCab IPA辞書](https://github.com/taku910/mecab/tree/master/mecab-ipadic)
- [MZ-IMEjaリポジトリ](https://github.com/katahiromz/mzimeja)

## サポート

問題や質問がある場合：
1. [GitHubイシュー](https://github.com/katahiromz/mzimeja/issues)を作成
2. ビルドログとエラーメッセージを添付
3. 環境情報（OS、コンパイラ、Rustバージョン）を記載
