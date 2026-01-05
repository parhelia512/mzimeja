#!/bin/bash
# convert_mecab_dict.sh --- Convert MeCab dictionary to Vibrato format
# MeCab辞書をVibrato形式に変換するスクリプト
#############################################################################

set -e

MECAB_DICT_URL="https://github.com/taku910/mecab/releases/download/mecab-0.996/mecab-ipadic-2.7.0-20070801.tar.gz"
MECAB_DICT_DIR="mecab-ipadic-2.7.0-20070801"
OUTPUT_DIR="dicts/vibrato"

echo "=== MeCab辞書をVibrato形式に変換 ==="

# 1. Vibratoツールのインストール確認
if ! command -v vibrato &> /dev/null; then
    echo "Vibratoがインストールされていません。インストール中..."
    cargo install vibrato --features=train
fi

# 2. MeCab辞書のダウンロード
if [ ! -d "$MECAB_DICT_DIR" ]; then
    echo "MeCab IPA辞書をダウンロード中..."
    if ! wget -q --show-progress "$MECAB_DICT_URL"; then
        echo "✗ ダウンロードに失敗しました"
        exit 1
    fi
    
    # ダウンロードファイルの検証
    DOWNLOAD_FILE="$(basename $MECAB_DICT_URL)"
    if [ ! -f "$DOWNLOAD_FILE" ]; then
        echo "✗ ダウンロードしたファイルが見つかりません"
        exit 1
    fi
    
    echo "ダウンロード完了。展開中..."
    if ! tar xzf "$DOWNLOAD_FILE"; then
        echo "✗ 展開に失敗しました"
        exit 1
    fi
    
    echo "✓ MeCab辞書を展開しました"
fi

# 3. 出力ディレクトリの作成
mkdir -p "$OUTPUT_DIR"

# 4. Vibrato形式に変換
echo "変換中..."
vibrato compile \
    --lexicon "$MECAB_DICT_DIR/Noun.csv" \
    --lexicon "$MECAB_DICT_DIR/Verb.csv" \
    --lexicon "$MECAB_DICT_DIR/Adj.csv" \
    --lexicon "$MECAB_DICT_DIR/Adverb.csv" \
    --lexicon "$MECAB_DICT_DIR/Auxil.csv" \
    --lexicon "$MECAB_DICT_DIR/Conjunction.csv" \
    --lexicon "$MECAB_DICT_DIR/Interjection.csv" \
    --lexicon "$MECAB_DICT_DIR/Prefix.csv" \
    --lexicon "$MECAB_DICT_DIR/Suffix.csv" \
    --lexicon "$MECAB_DICT_DIR/Symbol.csv" \
    --output "$OUTPUT_DIR/ipadic.vibrato"

echo "変換完了: $OUTPUT_DIR/ipadic.vibrato"
echo "サイズ: $(du -h $OUTPUT_DIR/ipadic.vibrato | cut -f1)"

# 5. 変換結果を検証
if [ -f "$OUTPUT_DIR/ipadic.vibrato" ]; then
    echo "✓ 辞書ファイルが正常に作成されました"
else
    echo "✗ 辞書ファイルの作成に失敗しました"
    exit 1
fi
