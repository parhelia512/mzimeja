// vibrato_engine.cpp --- Vibrato morphological analysis engine implementation
//////////////////////////////////////////////////////////////////////////////
// (Japanese, UTF-8)
// Vibrato形態素解析エンジンの実装

#include "vibrato_engine.h"
#include "../str.hpp"
#include <vector>

//////////////////////////////////////////////////////////////////////////////
// Constructor / Destructor

VibratoEngine::VibratoEngine()
    : initialized_(FALSE)
    , tokenizer_(nullptr)
{
}

VibratoEngine::~VibratoEngine()
{
#ifdef HAVE_VIBRATO
    if (tokenizer_) {
        vibrato_tokenizer_free(tokenizer_);
        tokenizer_ = nullptr;
    }
#endif
    initialized_ = FALSE;
}

//////////////////////////////////////////////////////////////////////////////
// Initialization

BOOL VibratoEngine::Initialize(const std::wstring& dict_path)
{
    DPRINTW(L"VibratoEngine::Initialize: %s\n", dict_path.c_str());
    
#ifdef HAVE_VIBRATO
    // 辞書パスの検証
    if (dict_path.empty()) {
        DPRINTW(L"VibratoEngine: Dictionary path is empty\n");
        return FALSE;
    }
    
    // UTF-8に変換
    std::string dict_path_utf8 = WideToUTF8(dict_path);
    
    // トークナイザーの初期化
    tokenizer_ = vibrato_tokenizer_load(dict_path_utf8.c_str());
    if (!tokenizer_) {
        DPRINTW(L"VibratoEngine: Failed to load tokenizer\n");
        return FALSE;
    }
    
    initialized_ = TRUE;
    DPRINTW(L"VibratoEngine: Initialized successfully\n");
    return TRUE;
    
#else
    // Vibratoが無効な場合
    DPRINTW(L"VibratoEngine: Vibrato support not compiled in\n");
    return FALSE;
#endif
}

//////////////////////////////////////////////////////////////////////////////
// Conversion methods

BOOL VibratoEngine::AnalyzeToLattice(const std::wstring& text, Lattice& lattice)
{
    if (!initialized_) {
        DPRINTW(L"VibratoEngine::AnalyzeToLattice: Not initialized\n");
        return FALSE;
    }
    
#ifdef HAVE_VIBRATO
    // テキストをUTF-8に変換
    std::string text_utf8 = WideToUTF8(text);
    
    // Vibratoで形態素解析を実行
    VibratoToken* tokens = nullptr;
    size_t num_tokens = 0;
    
    int result = vibrato_tokenize(tokenizer_, text_utf8.c_str(), &tokens, &num_tokens);
    if (result != 0 || !tokens) {
        DPRINTW(L"VibratoEngine::AnalyzeToLattice: Tokenization failed\n");
        return FALSE;
    }
    
    // ラティスの初期化
    lattice.m_pre = text;
    lattice.m_chunks.clear();
    lattice.m_chunks.resize(text.size() + 1);
    
    // 先頭ノードと末端ノードを作成
    lattice.m_head = LatticeNodePtr(new LatticeNode());
    lattice.m_head->bunrui = HB_HEAD;
    lattice.m_head->deltaCost = 0;
    
    lattice.m_tail = LatticeNodePtr(new LatticeNode());
    lattice.m_tail->bunrui = HB_TAIL;
    lattice.m_tail->deltaCost = 0;
    
    // トークンをラティスノードに変換
    for (size_t i = 0; i < num_tokens; i++) {
        LatticeNode node;
        std::string surface(tokens[i].surface);
        std::string feature(tokens[i].feature);
        
        VibratoTokenToLatticeNode(surface, feature, tokens[i].start, tokens[i].end, node);
        
        // ノードをラティスに追加
        size_t start_pos = tokens[i].start;
        if (start_pos <= text.size()) {
            lattice.AddNode(start_pos, node);
        }
    }
    
    // トークンのメモリを解放
    vibrato_tokens_free(tokens, num_tokens);
    
    // ラティスの構造を更新
    lattice.Fix(text);
    
    DPRINTW(L"VibratoEngine::AnalyzeToLattice: Success (%zu tokens)\n", num_tokens);
    return TRUE;
#else
    return FALSE;
#endif
}

BOOL VibratoEngine::ConvertMultiClause(const std::wstring& text, MzConvResult& result)
{
    if (!initialized_) {
        DPRINTW(L"VibratoEngine::ConvertMultiClause: Not initialized\n");
        return FALSE;
    }
    
#ifdef HAVE_VIBRATO
    // ラティス構造を作成
    Lattice lattice;
    if (!AnalyzeToLattice(text, lattice)) {
        DPRINTW(L"VibratoEngine::ConvertMultiClause: AnalyzeToLattice failed\n");
        return FALSE;
    }
    
    // ラティスから変換結果を生成
    extern MzIme TheIME;
    TheIME.MakeResultForMulti(result, lattice);
    
    return TRUE;
#else
    return FALSE;
#endif
}

BOOL VibratoEngine::ConvertSingleClause(const std::wstring& text, MzConvResult& result)
{
    if (!initialized_) {
        DPRINTW(L"VibratoEngine::ConvertSingleClause: Not initialized\n");
        return FALSE;
    }
    
#ifdef HAVE_VIBRATO
    // ラティス構造を作成
    Lattice lattice;
    if (!AnalyzeToLattice(text, lattice)) {
        DPRINTW(L"VibratoEngine::ConvertSingleClause: AnalyzeToLattice failed\n");
        return FALSE;
    }
    
    // ラティスから変換結果を生成
    extern MzIme TheIME;
    TheIME.MakeResultForSingle(result, lattice);
    
    return TRUE;
#else
    return FALSE;
#endif
}

//////////////////////////////////////////////////////////////////////////////
// Private helper methods

void VibratoEngine::VibratoTokenToLatticeNode(const std::string& surface, const std::string& feature,
                                                size_t start_pos, size_t end_pos, LatticeNode& node)
{
#ifdef HAVE_VIBRATO
    // 表層形をUTF-16に変換
    node.pre = UTF8ToWide(surface);
    node.post = node.pre;  // 初期値として同じ文字列を設定
    
    // 品詞情報を解析
    node.bunrui = ConvertPartOfSpeech(feature);
    
    // コストの設定（デフォルト値）
    node.deltaCost = 0;
    
    // 活用形情報のデフォルト設定
    node.gyou = DAN_NO_GYOU;
    node.katsuyou = KATSUYOU_NONE;
    
    // 特徴文字列からタグを抽出（必要に応じて）
    node.tags.clear();
#endif
}

HinshiBunrui VibratoEngine::ConvertPartOfSpeech(const std::string& feature)
{
#ifdef HAVE_VIBRATO
    // MeCab IPA辞書の品詞体系を解析
    // feature は CSV形式: "品詞,品詞細分類1,品詞細分類2,品詞細分類3,活用型,活用形,原形,読み,発音"
    
    if (feature.empty()) return HB_UNKNOWN;
    
    // カンマで分割
    std::vector<std::string> fields;
    str_split(fields, feature, std::string(","));
    
    if (fields.empty()) return HB_UNKNOWN;
    
    const std::string& pos = fields[0];  // 品詞
    
    // 名詞
    if (pos == "名詞") {
        return HB_MEISHI;
    }
    // 動詞
    else if (pos == "動詞") {
        // 活用型で五段/一段/カ変/サ変を判定
        if (fields.size() >= 5) {
            const std::string& conj_type = fields[4];  // 活用型
            if (conj_type.find("五段") != std::string::npos) {
                return HB_GODAN_DOUSHI;
            }
            else if (conj_type.find("一段") != std::string::npos) {
                return HB_ICHIDAN_DOUSHI;
            }
            else if (conj_type.find("カ変") != std::string::npos || conj_type.find("カ行変格") != std::string::npos) {
                return HB_KAHEN_DOUSHI;
            }
            else if (conj_type.find("サ変") != std::string::npos || conj_type.find("サ行変格") != std::string::npos) {
                return HB_SAHEN_DOUSHI;
            }
        }
        return HB_GODAN_DOUSHI;  // デフォルト
    }
    // 形容詞
    else if (pos == "形容詞") {
        return HB_IKEIYOUSHI;
    }
    // 形容動詞（な形容詞）
    else if (pos == "形容動詞") {
        return HB_NAKEIYOUSHI;
    }
    // 連体詞
    else if (pos == "連体詞") {
        return HB_RENTAISHI;
    }
    // 副詞
    else if (pos == "副詞") {
        return HB_FUKUSHI;
    }
    // 接続詞
    else if (pos == "接続詞") {
        return HB_SETSUZOKUSHI;
    }
    // 感動詞
    else if (pos == "感動詞") {
        return HB_KANDOUSHI;
    }
    // 助詞
    else if (pos == "助詞") {
        if (fields.size() >= 2) {
            const std::string& pos_sub = fields[1];  // 品詞細分類1
            if (pos_sub == "格助詞") {
                return HB_KAKU_JOSHI;
            }
            else if (pos_sub == "接続助詞") {
                return HB_SETSUZOKU_JOSHI;
            }
            else if (pos_sub == "副助詞" || pos_sub == "副助詞／並立助詞／終助詞") {
                return HB_FUKU_JOSHI;
            }
            else if (pos_sub == "終助詞") {
                return HB_SHUU_JOSHI;
            }
        }
        return HB_KAKU_JOSHI;  // デフォルト
    }
    // 助動詞
    else if (pos == "助動詞") {
        return HB_JODOUSHI;
    }
    // 接頭詞
    else if (pos == "接頭詞") {
        return HB_SETTOUJI;
    }
    // 接尾詞
    else if (pos == "接尾") {
        return HB_SETSUBIJI;
    }
    // 記号
    else if (pos == "記号") {
        if (fields.size() >= 2) {
            const std::string& pos_sub = fields[1];
            if (pos_sub == "句点") {
                return HB_PERIOD;
            }
            else if (pos_sub == "読点") {
                return HB_COMMA;
            }
        }
        return HB_SYMBOL;
    }
    
    return HB_UNKNOWN;
#else
    return HB_UNKNOWN;
#endif
}

//////////////////////////////////////////////////////////////////////////////
// UTF-8/UTF-16 conversion utilities

std::string VibratoEngine::WideToUTF8(const std::wstring& wstr)
{
    if (wstr.empty()) return std::string();
    
    // WideCharToMultiByte を使用（Windows専用）
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), 
                                          (int)wstr.size(), NULL, 0, NULL, NULL);
    if (size_needed <= 0) return std::string();
    
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), 
                       &result[0], size_needed, NULL, NULL);
    return result;
}

std::wstring VibratoEngine::UTF8ToWide(const std::string& str)
{
    if (str.empty()) return std::wstring();
    
    // MultiByteToWideChar を使用（Windows専用）
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), 
                                          (int)str.size(), NULL, 0);
    if (size_needed <= 0) return std::wstring();
    
    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), 
                       &result[0], size_needed);
    return result;
}
