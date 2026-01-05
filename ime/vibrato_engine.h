// vibrato_engine.h --- Vibrato morphological analysis engine integration
//////////////////////////////////////////////////////////////////////////////
// (Japanese, UTF-8)
// Vibrato形態素解析エンジンの統合

#pragma once

#include "mzimeja.h"

#ifdef HAVE_VIBRATO
// Vibrato C API
extern "C" {
    #include <vibrato.h>
}
#endif

// Vibrato engine class for morphological analysis
// 形態素解析用のVibratoエンジンクラス
class VibratoEngine {
public:
    VibratoEngine();
    ~VibratoEngine();
    
    // Initialize the Vibrato engine
    // Vibratoエンジンを初期化
    BOOL Initialize(const std::wstring& dict_path = L"");
    
    // Check if the engine is initialized
    // エンジンが初期化されているかチェック
    BOOL IsInitialized() const { return initialized_; }
    
    // Analyze text and convert to Lattice structure
    // テキストを解析してLattice構造に変換
    BOOL AnalyzeToLattice(const std::wstring& text, Lattice& lattice);
    
    // Convert text using multi-clause conversion
    // 文節変換を使用してテキストを変換
    BOOL ConvertMultiClause(const std::wstring& text, MzConvResult& result);
    
    // Convert text using single-clause conversion
    // 単文節変換を使用してテキストを変換
    BOOL ConvertSingleClause(const std::wstring& text, MzConvResult& result);
    
private:
    BOOL initialized_;
#ifdef HAVE_VIBRATO
    VibratoTokenizer* tokenizer_;
#else
    void* tokenizer_;  // Placeholder when Vibrato is not available
#endif
    
    // Convert Vibrato token to Lattice node
    // Vibratoトークンをラティスノードに変換
    void VibratoTokenToLatticeNode(const std::string& surface, const std::string& feature,
                                    size_t start_pos, size_t end_pos, LatticeNode& node);
    
    // Convert MeCab/Vibrato part-of-speech to MZ-IMEja HinshiBunrui
    // MeCab/Vibratoの品詞をMZ-IMEjaの品詞分類に変換
    HinshiBunrui ConvertPartOfSpeech(const std::string& feature);
    
    // UTF-8/UTF-16 conversion utilities
    // UTF-8/UTF-16変換ユーティリティ
    std::string WideToUTF8(const std::wstring& wstr);
    std::wstring UTF8ToWide(const std::string& str);
};

// Global Vibrato engine instance (defined in convert.cpp)
// グローバルなVibratoエンジンインスタンス（convert.cppで定義）
#ifdef HAVE_VIBRATO
extern VibratoEngine g_vibrato_engine;
#endif
