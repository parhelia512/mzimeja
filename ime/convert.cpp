﻿// convert.cpp --- mzimeja kana kanji conversion
// (Japanese, UTF-8)
// かな漢字変換。
//////////////////////////////////////////////////////////////////////////////
// 参考文献1：『自然言語処理の基礎』2010年、コロナ社。
// 参考文献2：『新編常用国語便覧』1995年、浜島書店。

#include "mzimeja.h"
#include "resource.h"
#include <algorithm>        // for std::sort

const DWORD c_dwMilliseconds = 8000;

// 辞書。
Dict g_basic_dict;
Dict g_name_dict;

// ひらがな表。品詞の活用で使用される。
static const WCHAR s_hiragana_table[][5] =
{
    // DAN_A, DAN_I, DAN_U, DAN_E, DAN_O
    {L'あ', L'い', L'う', L'え', L'お'}, // GYOU_A
    {L'か', L'き', L'く', L'け', L'こ'}, // GYOU_KA
    {L'が', L'ぎ', L'ぐ', L'げ', L'ご'}, // GYOU_GA
    {L'さ', L'し', L'す', L'せ', L'そ'}, // GYOU_SA
    {L'ざ', L'じ', L'ず', L'ぜ', L'ぞ'}, // GYOU_ZA
    {L'た', L'ち', L'つ', L'て', L'と'}, // GYOU_TA
    {L'だ', L'ぢ', L'づ', L'で', L'ど'}, // GYOU_DA
    {L'な', L'に', L'ぬ', L'ね', L'の'}, // GYOU_NA
    {L'は', L'ひ', L'ふ', L'へ', L'ほ'}, // GYOU_HA
    {L'ば', L'び', L'ぶ', L'べ', L'ぼ'}, // GYOU_BA
    {L'ぱ', L'ぴ', L'ぷ', L'ぺ', L'ぽ'}, // GYOU_PA
    {L'ま', L'み', L'む', L'め', L'も'}, // GYOU_MA
    {L'や',     0, L'ゆ',     0, L'よ'}, // GYOU_YA
    {L'ら', L'り', L'る', L'れ', L'ろ'}, // GYOU_RA
    {L'わ',     0,     0,     0, L'を'}, // GYOU_WA
    {L'ん',     0,     0,     0,     0}, // GYOU_NN
}; // ※ s_hiragana_table[GYOU_DA][DAN_U] のように使用する。

std::map<WCHAR,Dan>   g_hiragana_to_dan;  // 母音写像。
std::map<WCHAR,Gyou>  g_hiragana_to_gyou; // 子音写像。

// 子音の写像と母音の写像を作成する。
void mz_make_literal_maps()
{
    if (g_hiragana_to_gyou.size())
        return;
    g_hiragana_to_gyou.clear();
    g_hiragana_to_dan.clear();
    const size_t count = _countof(s_hiragana_table);
    for (size_t i = 0; i < count; ++i) {
        for (size_t k = 0; k < 5; ++k) {
            g_hiragana_to_gyou[ARRAY_AT_AT(s_hiragana_table, i, k)] = (Gyou)i;
        }
        for (size_t k = 0; k < 5; ++k) {
            g_hiragana_to_dan[ARRAY_AT_AT(s_hiragana_table, i, k)] = (Dan)k;
        }
    }
} // mz_make_literal_maps

// 品詞分類から文字列を取得する関数。
LPCTSTR mz_hinshi_to_string(HinshiBunrui hinshi)
{
    if (HB_MEISHI <= hinshi && hinshi <= HB_MAX)
        return TheIME.LoadSTR(IDS_HINSHI_00 + (hinshi - HB_MEISHI));
    return TEXT("");
}

// 文字列から品詞分類を取得する関数。
HinshiBunrui mz_string_to_hinshi(LPCTSTR str)
{
    for (INT hinshi = HB_MEISHI; hinshi <= HB_MAX; ++hinshi) {
        LPCTSTR psz = mz_hinshi_to_string((HinshiBunrui)hinshi);
        if (lstrcmpW(psz, str) == 0)
            return (HinshiBunrui)hinshi;
    }
    return HB_UNKNOWN;
}

// 品詞分類を文字列に変換する（デバッグ用）。
LPCWSTR mz_bunrui_to_string(HinshiBunrui bunrui)
{
    int index = int(bunrui) - int(HB_HEAD);
    static const WCHAR *s_array[] = {
        L"HB_HEAD",
        L"HB_TAIL",
        L"HB_UNKNOWN",
        L"HB_MEISHI",
        L"HB_IKEIYOUSHI",
        L"HB_NAKEIYOUSHI",
        L"HB_RENTAISHI",
        L"HB_FUKUSHI",
        L"HB_SETSUZOKUSHI",
        L"HB_KANDOUSHI",
        L"HB_KAKU_JOSHI",
        L"HB_SETSUZOKU_JOSHI",
        L"HB_FUKU_JOSHI",
        L"HB_SHUU_JOSHI",
        L"HB_JODOUSHI",
        L"HB_MIZEN_JODOUSHI",
        L"HB_RENYOU_JODOUSHI",
        L"HB_SHUUSHI_JODOUSHI",
        L"HB_RENTAI_JODOUSHI",
        L"HB_KATEI_JODOUSHI",
        L"HB_MEIREI_JODOUSHI",
        L"HB_GODAN_DOUSHI",
        L"HB_ICHIDAN_DOUSHI",
        L"HB_KAHEN_DOUSHI",
        L"HB_SAHEN_DOUSHI",
        L"HB_KANGO",
        L"HB_SETTOUJI",
        L"HB_SETSUBIJI",
        L"HB_PERIOD",
        L"HB_COMMA",
        L"HB_SYMBOL"
    };
    ASSERT(index <= HB_MAX);
    return s_array[index];
} // mz_bunrui_to_string

// 品詞の連結可能性を計算する関数。
BOOL LatticeNode::CanConnectTo(const LatticeNode& other) const
{
    HinshiBunrui h0 = bunrui, h1 = other.bunrui;

    if (h0 == HB_HEAD && (h1 == HB_SETSUBIJI || h1 == HB_SHUU_JOSHI))
        return FALSE;
    if (h0 == HB_SETTOUJI && h1 == HB_TAIL)
        return FALSE;
    if (h0 == HB_HEAD || h1 == HB_TAIL)
        return TRUE;
    if (h1 == HB_PERIOD || h1 == HB_COMMA)
        return TRUE;
    if (h0 == HB_SYMBOL || h1 == HB_SYMBOL)
        return TRUE;
    if (h0 == HB_UNKNOWN || h1 == HB_UNKNOWN)
        return TRUE;

    switch (h0) {
    case HB_MEISHI: // 名詞
        switch (h1) {
        case HB_SETTOUJI:
            return FALSE;
        default:
            return TRUE;
        }
        break;
    case HB_IKEIYOUSHI: case HB_NAKEIYOUSHI: // い形容詞、な形容詞
        switch (katsuyou) {
        case MIZEN_KEI:
            if (h1 == HB_JODOUSHI) {
                if (other.HasTag(L"[未然形に連結]")) {
                    if (other.pre[0] == L'な' || other.pre == L"う") {
                        return TRUE;
                    }
                }
            }
            return FALSE;
        case RENYOU_KEI:
            switch (h1) {
            case HB_JODOUSHI:
                if (other.HasTag(L"[連用形に連結]")) {
                    return TRUE;
                }
                return FALSE;
            case HB_MEISHI: case HB_SETTOUJI:
            case HB_IKEIYOUSHI: case HB_NAKEIYOUSHI:
            case HB_GODAN_DOUSHI: case HB_ICHIDAN_DOUSHI:
            case HB_KAHEN_DOUSHI: case HB_SAHEN_DOUSHI:
                return TRUE;
            default:
                return FALSE;
            }
            break;
        case SHUUSHI_KEI:
            if (h1 == HB_JODOUSHI) {
                if (other.HasTag(L"[終止形に連結]")) {
                    return TRUE;
                }
                if (other.HasTag(L"[種々の語]")) {
                    return TRUE;
                }
            }
            if (h1 == HB_SHUU_JOSHI) {
                return TRUE;
            }
            return FALSE;
        case RENTAI_KEI:
            switch (h1) {
            case HB_KANDOUSHI: case HB_JODOUSHI:
                return FALSE;
            default:
                return TRUE;
            }
        case KATEI_KEI:
            switch (h1) {
            case HB_SETSUZOKU_JOSHI:
                if (other.pre == L"ば" || other.pre == L"ども" || other.pre == L"ど") {
                    return TRUE;
                }
            default:
                break;
            }
            return FALSE;
        case MEIREI_KEI:
            switch (h1) {
            case HB_SHUU_JOSHI: case HB_MEISHI: case HB_SETTOUJI:
                return TRUE;
            default:
                break;
            }
            return FALSE;
        }
        break;
    case HB_RENTAISHI: // 連体詞
        switch (h1) {
        case HB_KANDOUSHI: case HB_JODOUSHI: case HB_SETSUBIJI:
            return FALSE;
        default:
            return TRUE;
        }
        break;
    case HB_FUKUSHI: // 副詞
        switch (h1) {
        case HB_KAKU_JOSHI: case HB_SETSUZOKU_JOSHI: case HB_FUKU_JOSHI:
        case HB_SETSUBIJI:
            return FALSE;
        default:
            return TRUE;
        }
        break;
    case HB_SETSUZOKUSHI: // 接続詞
        switch (h1) {
        case HB_KAKU_JOSHI: case HB_SETSUZOKU_JOSHI:
        case HB_FUKU_JOSHI: case HB_SETSUBIJI:
            return FALSE;
        default:
            return TRUE;
        }
        break;
    case HB_KANDOUSHI: // 感動詞
        switch (h1) {
        case HB_KAKU_JOSHI: case HB_SETSUZOKU_JOSHI:
        case HB_FUKU_JOSHI: case HB_SETSUBIJI: case HB_JODOUSHI:
            return FALSE;
        default:
            return TRUE;
        }
        break;
    case HB_KAKU_JOSHI: case HB_SETSUZOKU_JOSHI: case HB_FUKU_JOSHI:
        // 終助詞以外の助詞
        switch (h1) {
        case HB_SETSUBIJI:
        case HB_JODOUSHI:
        case HB_SHUU_JOSHI:
            return FALSE;
        default:
            return TRUE;
        }
        break;
    case HB_SHUU_JOSHI: // 終助詞
        switch (h1) {
        case HB_MEISHI: case HB_SETTOUJI: case HB_SHUU_JOSHI:
            return TRUE;
        default:
            return FALSE;
        }
        break;
    case HB_JODOUSHI: // 助動詞
        switch (katsuyou) {
        case MIZEN_KEI:
            if (h1 == HB_JODOUSHI) {
                if (other.HasTag(L"[未然形に連結]")) {
                    return TRUE;
                }
            }
            return FALSE;
        case RENYOU_KEI:
            switch (h1) {
            case HB_JODOUSHI:
                if (other.HasTag(L"[連用形に連結]")) {
                    return TRUE;
                }
                return FALSE;
            case HB_MEISHI: case HB_SETTOUJI:
            case HB_IKEIYOUSHI: case HB_NAKEIYOUSHI:
            case HB_GODAN_DOUSHI: case HB_ICHIDAN_DOUSHI:
            case HB_KAHEN_DOUSHI: case HB_SAHEN_DOUSHI:
                return TRUE;
            default:
                return FALSE;
            }
            break;
        case SHUUSHI_KEI:
            if (h1 == HB_JODOUSHI) {
                if (other.HasTag(L"[終止形に連結]")) {
                    return TRUE;
                }
                if (other.HasTag(L"[種々の語]")) {
                    return TRUE;
                }
            }
            if (h1 == HB_SHUU_JOSHI) {
                return TRUE;
            }
            return FALSE;
        case RENTAI_KEI:
            switch (h1) {
            case HB_KANDOUSHI: case HB_JODOUSHI:
                return FALSE;
            default:
                return TRUE;
            }
        case KATEI_KEI:
            switch (h1) {
            case HB_SETSUZOKU_JOSHI:
                if (other.pre == L"ば" || other.pre == L"ども" || other.pre == L"ど") {
                    return TRUE;
                }
                break;
            default:
                break;
            }
            return FALSE;
        case MEIREI_KEI:
            switch (h1) {
            case HB_SHUU_JOSHI: case HB_MEISHI: case HB_SETTOUJI:
                return TRUE;
            default:
                return FALSE;
            }
        }
        break;
    case HB_MIZEN_JODOUSHI: case HB_RENYOU_JODOUSHI:
    case HB_SHUUSHI_JODOUSHI: case HB_RENTAI_JODOUSHI:
    case HB_KATEI_JODOUSHI: case HB_MEIREI_JODOUSHI:
        ASSERT(0);
        break;
    case HB_GODAN_DOUSHI: case HB_ICHIDAN_DOUSHI:
    case HB_KAHEN_DOUSHI: case HB_SAHEN_DOUSHI:
        // 動詞
        switch (katsuyou) {
        case MIZEN_KEI:
            if (h1 == HB_JODOUSHI) {
                if (other.HasTag(L"[未然形に連結]")) {
                    return TRUE;
                }
            }
            return FALSE;
        case RENYOU_KEI:
            switch (h1) {
            case HB_JODOUSHI:
                if (other.HasTag(L"[連用形に連結]")) {
                    return TRUE;
                }
                return FALSE;
            case HB_MEISHI: case HB_SETTOUJI:
            case HB_IKEIYOUSHI: case HB_NAKEIYOUSHI:
            case HB_GODAN_DOUSHI: case HB_ICHIDAN_DOUSHI:
            case HB_KAHEN_DOUSHI: case HB_SAHEN_DOUSHI:
                return TRUE;
            default:
                return FALSE;
            }
            break;
        case SHUUSHI_KEI:
            if (h1 == HB_JODOUSHI) {
                if (other.HasTag(L"[終止形に連結]")) {
                    return TRUE;
                }
                if (other.HasTag(L"[種々の語]")) {
                    return TRUE;
                }
            }
            if (h1 == HB_SHUU_JOSHI) {
                return TRUE;
            }
            return FALSE;
        case RENTAI_KEI:
            switch (h1) {
            case HB_KANDOUSHI: case HB_JODOUSHI:
                return FALSE;
            default:
                return TRUE;
            }
            break;
        case KATEI_KEI:
            switch (h1) {
            case HB_SETSUZOKU_JOSHI:
                if (other.pre == L"ば" || other.pre == L"ども" || other.pre == L"ど") {
                    return TRUE;
                }
            default:
                break;
            }
            return FALSE;
        case MEIREI_KEI:
            switch (h1) {
            case HB_SHUU_JOSHI: case HB_MEISHI: case HB_SETTOUJI:
                return TRUE;
            default:
                return FALSE;
            }
            break;
        }
        break;
    case HB_SETTOUJI: // 接頭辞
        switch (h1) {
        case HB_MEISHI:
            return TRUE;
        default:
            return FALSE;
        }
        break;
    case HB_SETSUBIJI: // 接尾辞
        switch (h1) {
        case HB_SETTOUJI:
            return FALSE;
        default:
            break;
        }
        break;
    case HB_COMMA: case HB_PERIOD: // 、。
        if (other.IsJoshi())
            return FALSE;
        break;
    default:
        break;
    }
    return TRUE;
} // LatticeNode::CanConnectTo

// 単語コストの計算。
INT LatticeNode::WordCost() const
{
    INT ret = 20;

    HinshiBunrui h = bunrui;
    if (h == HB_MEISHI)
        ret += 30;
    else if (IsJodoushi())
        ret += 10;
    else if (IsDoushi())
        ret += 20;
    else if (IsJoshi())
        ret += 10;
    else if (h == HB_SETSUZOKUSHI)
        ret += 10;
    else
        ret += 30;

    if (h == HB_KANGO)
        ret += 300;
    if (h == HB_SYMBOL)
        ret += 120;
    if (h == HB_GODAN_DOUSHI && katsuyou == RENYOU_KEI)
        ret += 30;
    if (h == HB_SETTOUJI)
        ret += 200;

    if (HasTag(L"[数単位]"))
        ret += 10;
    if (HasTag(L"[非標準]"))
        ret += 100;
    if (HasTag(L"[不謹慎]"))
        ret += 50;
    if (HasTag(L"[人名]"))
        ret += 30;
    else if (HasTag(L"[駅名]"))
        ret += 30;
    else if (HasTag(L"[地名]"))
        ret += 30;
    if (HasTag(L"[ユーザ辞書]"))
        ret -= 20;
    if (HasTag(L"[優先++]"))
        ret -= 90;
    if (HasTag(L"[優先+]"))
        ret -= 30;
    if (HasTag(L"[優先--]"))
        ret += 90;
    if (HasTag(L"[優先-]"))
        ret += 30;

    ret += deltaCost;
    return ret;
} // LatticeNode::WordCost

// 連結コストの計算。
INT LatticeNode::ConnectCost(const LatticeNode& other) const
{
    HinshiBunrui h0 = bunrui, h1 = other.bunrui;
    if (h0 == HB_HEAD || h1 == HB_TAIL)
        return 0;
    if (h1 == HB_PERIOD || h1 == HB_COMMA)
        return 0;
    if (h0 == HB_SYMBOL || h1 == HB_SYMBOL)
        return 0;
    if (h0 == HB_UNKNOWN || h1 == HB_UNKNOWN)
        return 0;

    INT ret = 10;
    if (h0 == HB_MEISHI) {
        if (h1 == HB_MEISHI)
            ret += 10;
        if (h1 == HB_SETTOUJI)
            ret += 200;
        if (other.IsDoushi())
            ret += 60;
        if (other.IsKeiyoushi())
            ret += 20;
        if (other.IsJodoushi())
            ret += 50;
        if (h1 == HB_SAHEN_DOUSHI && other.pre.size() <= 2)
            ret -= 50;
    }
    if (IsKeiyoushi()) {
        if (other.IsKeiyoushi())
            ret += 10;
        if (other.IsDoushi())
            ret += 50;
    }
    if (IsDoushi()) {
        if (other.IsJoshi())
            ret -= 5;
        if (other.IsDoushi())
            ret += 20;
        if (other.IsJodoushi())
            ret -= 10;
    }
    if (h0 == HB_SETSUZOKUSHI && other.IsJoshi())
        ret += 5;
    if (h0 == HB_SETSUZOKUSHI && h1 == HB_MEISHI)
        ret += 5;
    if (h0 == HB_KANDOUSHI && h1 == HB_SHUU_JOSHI)
        ret += 300;
    if (post == L"し" && other.post == L"ます")
        ret -= 100;
    if ((post == L"でき" || post == L"出来") && other.post == L"ます")
        ret -= 100;

    return ret;
} // LatticeNode::ConnectCost

// マーキングを最適化する。
BOOL Lattice::OptimizeMarking(LatticeNode *ptr0)
{
    ASSERT(ptr0);

    if (!ptr0->marked)
        return FALSE;

    BOOL reach = (ptr0->bunrui == HB_TAIL);
    INT min_cost = MAXLONG;
    LatticeNode *min_node = NULL;
    branches_t::iterator it, end = ptr0->branches.end();
    for (it = ptr0->branches.begin(); it != end; ++it) {
        LatticeNodePtr ptr1 = *it;
        if (OptimizeMarking(ptr1.get())) {
            reach = TRUE;
            if (ptr1->subtotal_cost < min_cost) {
                min_cost = ptr1->subtotal_cost;
                min_node = ptr1.get();
            }
        }
    }

    {
        branches_t::iterator it, end = ptr0->branches.end();
        for (it = ptr0->branches.begin(); it != end; ++it) {
            LatticeNodePtr& ptr1 = *it;
            if (ptr1.get() != min_node) {
                ptr1->marked = 0;
            }
        }
    }

    if (!reach) {
        ptr0->marked = 0;
        return FALSE;
    }

    return TRUE;
} // Lattice::OptimizeMarking

// 基本辞書データをスキャンする。
static size_t ScanBasicDict(WStrings& records, const WCHAR *dict_data, WCHAR ch)
{
    DPRINTW(L"%c\n", ch);

    ASSERT(dict_data);

    if (ch == 0)
        return 0;

    // レコード区切りと文字chの組み合わせを検索する。
    // これで文字chで始まる単語を検索できる。
    // レコード群はソートされていると仮定。
    WCHAR sz[] = { RECORD_SEP, ch, 0 };
    const WCHAR *pch1 = wcsstr(dict_data, sz);
    if (pch1 == NULL)
        return FALSE;

    const WCHAR *pch2 = pch1; // 現在の位置。
    const WCHAR *pch3;
    for (;;) {
        // 現在の位置の次のレコード区切りと文字chの組み合わせを検索する。
        pch3 = wcsstr(pch2 + 1, sz);
        if (pch3 == NULL)
            break; // なければループ終わり。
        pch2 = pch3; // 現在の位置を更新。
    }
    pch3 = wcschr(pch2 + 1, RECORD_SEP); // 現在の位置の次のレコード区切りを検索する。
    if (pch3 == NULL)
        return FALSE;

    // 最初に発見したレコード区切りから最後のレコード区切りまでの文字列を取得する。
    std::wstring str;
    str.assign(pch1 + 1, pch3);

    // レコード区切りで分割してレコードを取得する。
    sz[0] = RECORD_SEP;
    sz[1] = 0;
    str_split(records, str, sz);
    ASSERT(records.size());
    return records.size();
} // ScanBasicDict

static WStrings s_UserDictRecords;

static INT CALLBACK UserDictProc(LPCTSTR lpRead, DWORD dwStyle, LPCTSTR lpStr, LPVOID lpData)
{
    ASSERT(lpStr && lpStr[0]);
    ASSERT(lpRead && lpRead[0]);
    Lattice *pThis = (Lattice *)lpData;
    ASSERT(pThis != NULL);

    // データの初期化。
    std::wstring pre = lpRead;
    std::wstring post = lpStr;
    Gyou gyou = GYOU_A;
    HinshiBunrui bunrui = StyleToHinshi(dwStyle);

    if (pre.size() <= 1)
        return 0;

    // データを辞書形式に変換する。
    WCHAR ch;
    size_t i;
    switch (bunrui) {
    case HB_NAKEIYOUSHI: // な形容詞
        // 終端の「な」を削る。
        i = pre.size() - 1;
        if (pre[i] == L'な')
            pre.resize(i);
        i = post.size() - 1;
        if (post[i] == L'な')
            post.resize(i);
        break;
    case HB_IKEIYOUSHI: // い形容詞
        // 終端の「い」を削る。
        i = pre.size() - 1;
        if (pre[i] == L'い')
            pre.resize(i);
        i = post.size() - 1;
        if (post[i] == L'い')
            post.resize(i);
        break;
    case HB_ICHIDAN_DOUSHI: // 一段動詞
        // 終端の「る」を削る。
        if (pre[pre.size() - 1] == L'る')
            pre.resize(pre.size() - 1);
        if (post[post.size() - 1] == L'る')
            post.resize(post.size() - 1);
        break;
    case HB_KAHEN_DOUSHI: // カ変動詞
        // 読みが３文字以上で「来る」「くる」で終わるとき、「来る」を削る。
        if (pre.size() >= 3) {
            if (pre.substr(pre.size() - 2, 2) == L"くる" &&
                post.substr(post.size() - 2, 2) == L"来る")
            {
                pre = pre.substr(0, pre.size() - 2);
                post = post.substr(0, post.size() - 2);
            }
        }
        break;
    case HB_SAHEN_DOUSHI: // サ変動詞
        gyou = GYOU_SA;
        if (pre.size() >= 2 && post.size() >= 2) { // 三文字以上のとき。
            // 「する」「ずる」ならば「する」「ずる」を削る。
            if (pre.substr(pre.size() - 2) == L"する" &&
                post.substr(post.size() - 2) == L"する")
            {
                gyou = GYOU_ZA;
                pre.resize(pre.size() - 2);
                post.resize(post.size() - 2);
            }
            else if (pre.substr(pre.size() - 2) == L"ずる" &&
                     post.substr(post.size() - 2) == L"ずる")
            {
                gyou = GYOU_SA;
                pre.resize(pre.size() - 2);
                post.resize(post.size() - 2);
            }
        }
        break;
    case HB_GODAN_DOUSHI: // 五段動詞
        // 写像を準備する。
        mz_make_literal_maps();
        // 終端がウ段の文字でなければ失敗。
        if (pre.empty())
            return TRUE;
        ch = pre[pre.size() - 1];
        if (g_hiragana_to_dan[ch] != DAN_U)
            return TRUE;
        if (ch != post[post.size() - 1])
            return TRUE;
        // 終端の文字を削る。
        pre.resize(pre.size() - 1);
        post.resize(post.size() - 1);
        // 終端の文字だったものの行を取得する。
        gyou = g_hiragana_to_gyou[ch];
        break;
    default:
        break;
    }

    WStrings fields(NUM_FIELDS);
    fields[I_FIELD_PRE] = pre;
    fields[I_FIELD_HINSHI].resize(1);
    fields[I_FIELD_HINSHI][0] = MAKEWORD(bunrui, gyou);
    fields[I_FIELD_POST] = post;
    fields[I_FIELD_TAGS] = L"[ユーザ辞書]";

    std::wstring record;
    std::wstring sep;
    sep.resize(1);
    sep[0] = FIELD_SEP;
    if (bunrui == HB_SAHEN_DOUSHI) {
        if (gyou == GYOU_ZA) {
            fields[I_FIELD_PRE] = pre + L"ざ";
            fields[I_FIELD_POST] = post + L"ざ";
            record = str_join(fields, sep);
            s_UserDictRecords.push_back(record);

            fields[I_FIELD_PRE] = pre + L"じ";
            fields[I_FIELD_POST] = post + L"じ";
            record = str_join(fields, sep);
            s_UserDictRecords.push_back(record);

            fields[I_FIELD_PRE] = pre + L"ぜ";
            fields[I_FIELD_POST] = post + L"ぜ";
            record = str_join(fields, sep);
            s_UserDictRecords.push_back(record);

            fields[I_FIELD_PRE] = pre + L"ずる";
            fields[I_FIELD_POST] = post + L"ずる";
            record = str_join(fields, sep);
            s_UserDictRecords.push_back(record);

            fields[I_FIELD_PRE] = pre + L"ずれ";
            fields[I_FIELD_POST] = post + L"ずれ";
            record = str_join(fields, sep);
            s_UserDictRecords.push_back(record);

            fields[I_FIELD_PRE] = pre + L"じろ";
            fields[I_FIELD_POST] = post + L"じろ";
            record = str_join(fields, sep);
            s_UserDictRecords.push_back(record);

            fields[I_FIELD_PRE] = pre + L"ぜよ";
            fields[I_FIELD_POST] = post + L"ぜよ";
            record = str_join(fields, sep);
            s_UserDictRecords.push_back(record);

            fields[I_FIELD_PRE] = pre + L"じよう";
            fields[I_FIELD_POST] = post + L"じよう";
            record = str_join(fields, sep);
            s_UserDictRecords.push_back(record);
        } else {
            fields[I_FIELD_PRE] = pre + L"さ";
            fields[I_FIELD_POST] = post + L"さ";
            record = str_join(fields, sep);
            s_UserDictRecords.push_back(record);

            fields[I_FIELD_PRE] = pre + L"し";
            fields[I_FIELD_POST] = post + L"し";
            record = str_join(fields, sep);
            s_UserDictRecords.push_back(record);

            fields[I_FIELD_PRE] = pre + L"せ";
            fields[I_FIELD_POST] = post + L"せ";
            record = str_join(fields, sep);
            s_UserDictRecords.push_back(record);

            fields[I_FIELD_PRE] = pre + L"する";
            fields[I_FIELD_POST] = post + L"する";
            record = str_join(fields, sep);
            s_UserDictRecords.push_back(record);

            fields[I_FIELD_PRE] = pre + L"すれ";
            fields[I_FIELD_POST] = post + L"すれ";
            record = str_join(fields, sep);
            s_UserDictRecords.push_back(record);

            fields[I_FIELD_PRE] = pre + L"しろ";
            fields[I_FIELD_POST] = post + L"しろ";
            record = str_join(fields, sep);
            s_UserDictRecords.push_back(record);

            fields[I_FIELD_PRE] = pre + L"せよ";
            fields[I_FIELD_POST] = post + L"せよ";
            record = str_join(fields, sep);
            s_UserDictRecords.push_back(record);

            fields[I_FIELD_PRE] = pre + L"しよう";
            fields[I_FIELD_POST] = post + L"しよう";
            record = str_join(fields, sep);
            s_UserDictRecords.push_back(record);
        }
    } else {
        record = str_join(fields, sep);
        s_UserDictRecords.push_back(record);
    }

    return TRUE;
}

// ユーザー辞書データをスキャンする。
static size_t ScanUserDict(WStrings& records, WCHAR ch, Lattice *pThis)
{
    DPRINTW(L"%c\n", ch);
    s_UserDictRecords.clear();
    ImeEnumRegisterWord(UserDictProc, NULL, 0, NULL, pThis);

    records.insert(records.end(), s_UserDictRecords.begin(), s_UserDictRecords.end());
    s_UserDictRecords.clear();
 
    return records.size();
}

//////////////////////////////////////////////////////////////////////////////
// Dict (dictionary) - 辞書データ。

// 辞書データのコンストラクタ。
Dict::Dict()
{
    m_hMutex = NULL;
    m_hFileMapping = NULL;
}

// 辞書データのデストラクタ。
Dict::~Dict()
{
    Unload();
}

// 辞書データファイルのサイズを取得する。
DWORD Dict::GetSize() const
{
    WIN32_FIND_DATAW find;
    HANDLE hFind = ::FindFirstFileW(m_strFileName.c_str(), &find);
    if (hFind != INVALID_HANDLE_VALUE) {
        ::FindClose(hFind);
        return find.nFileSizeLow;
    }
    return 0;
}

// 辞書を読み込む。
BOOL Dict::Load(const WCHAR *file_name, const WCHAR *object_name)
{
    if (IsLoaded())
        return TRUE; // すでに読み込み済み。

    m_strFileName = file_name;
    m_strObjectName = object_name;

    if (file_name == NULL)
        return FALSE;

    SECURITY_ATTRIBUTES *psa = CreateSecurityAttributes(); // セキュリティ属性を作成。
    ASSERT(psa);

    // ミューテックス (排他制御を行うオブジェクト) を作成。
    if (m_hMutex == NULL) {
        m_hMutex = ::CreateMutexW(psa, FALSE, m_strObjectName.c_str());
    }
    if (m_hMutex == NULL) {
        // free sa
        FreeSecurityAttributes(psa);
        return FALSE;
    }

    // ファイルサイズを取得。
    DWORD cbSize = GetSize();
    if (cbSize == 0)
        return FALSE;

    BOOL ret = FALSE;
    DWORD wait = ::WaitForSingleObject(m_hMutex, c_dwMilliseconds); // 排他制御を待つ。
    if (wait == WAIT_OBJECT_0) {
        // ファイルマッピングを作成する。
        m_hFileMapping = ::CreateFileMappingW(
                INVALID_HANDLE_VALUE, psa, PAGE_READWRITE,
                0, cbSize, (m_strObjectName + L"FileMapping").c_str());
        if (m_hFileMapping) {
            // ファイルマッピングが作成された。
            if (::GetLastError() == ERROR_ALREADY_EXISTS) {
                // ファイルマッピングがすでに存在する。
                ret = TRUE;
            } else {
                // 新しく作成された。ファイルを読み込む。
                FILE *fp = _wfopen(m_strFileName.c_str(), L"rb");
                if (fp) {
                    WCHAR *pch = Lock();
                    if (pch) {
                        ret = (BOOL)fread(pch, cbSize, 1, fp);
                        Unlock(pch);
                    }
                    fclose(fp);
                }
            }
        }
        // 排他制御を解放。
        ::ReleaseMutex(m_hMutex);
    }

    // free sa
    FreeSecurityAttributes(psa);

    return ret;
} // Dict::Load

// 辞書をアンロードする。
void Dict::Unload()
{
    if (m_hMutex) {
        if (m_hFileMapping) {
            DWORD wait = ::WaitForSingleObject(m_hMutex, c_dwMilliseconds); // 排他制御を待つ。
            if (wait == WAIT_OBJECT_0) {
                // ファイルマッピングを閉じる。
                if (m_hFileMapping) {
                    ::CloseHandle(m_hFileMapping);
                    m_hFileMapping = NULL;
                }
                // 排他制御を解放。
                ::ReleaseMutex(m_hMutex);
            }
        }
        // ミューテックスを閉じる。
        ::CloseHandle(m_hMutex);
        m_hMutex = NULL;
    }
}

// 辞書をロックして情報の取得を開始。
WCHAR *Dict::Lock()
{
    if (m_hFileMapping == NULL)
        return NULL;
    DWORD cbSize = GetSize();
    void *pv = ::MapViewOfFile(m_hFileMapping, FILE_MAP_ALL_ACCESS, 0, 0, cbSize);
    return reinterpret_cast<WCHAR *>(pv);
}

// 辞書のロックを解除して、情報の取得を終了。
void Dict::Unlock(WCHAR *data)
{
    ::UnmapViewOfFile(data);
}

// 辞書は読み込まれたか？
BOOL Dict::IsLoaded() const
{
    return (m_hMutex != NULL && m_hFileMapping != NULL);
}

//////////////////////////////////////////////////////////////////////////////
// MzConvResult, MzConvClause etc.

// 文節にノードを追加する。
void MzConvClause::add(const LatticeNode *node)
{
    candidates_t::iterator it, end = candidates.end();
    for (it = candidates.begin(); it != end; ++it) {
        MzConvCandidate& cand = *it;
        if (cand.post == node->post) {
            if (node->subtotal_cost < cand.cost) {
                cand.cost = node->subtotal_cost;
                cand.bunrui = node->bunrui;
                cand.katsuyou = node->katsuyou;
            }
            if (node->WordCost() < cand.word_cost) {
                cand.word_cost = node->WordCost();
            }
            cand.bunruis.insert(node->bunrui);
            cand.tags += node->tags;
            return;
        }
    }
    MzConvCandidate cand;
    cand.pre = node->pre;
    cand.post = mz_translate_string(node->post);
    cand.cost = node->subtotal_cost;
    cand.word_cost = node->WordCost();
    cand.bunruis.insert(node->bunrui);
    cand.bunrui = node->bunrui;
    cand.katsuyou = node->katsuyou;
    cand.tags = node->tags;
    candidates.push_back(cand);
}

static inline bool 
compare_candidate(const MzConvCandidate& cand1, const MzConvCandidate& cand2){
    if (cand1.cost < cand2.cost)
        return true;
    if (cand1.cost > cand2.cost)
        return false;
    if (cand1.post < cand2.post)
        return true;
    if (cand1.post > cand2.post)
        return false;
    return false;
}

static inline bool 
compare_candidate_by_post(const MzConvCandidate& cand1, const MzConvCandidate& cand2) {
    return cand1.post == cand2.post;
}

// コストで候補をソートする。
void MzConvClause::sort()
{
    std::sort(candidates.begin(), candidates.end(), compare_candidate);
    candidates.erase(std::unique(candidates.begin(), candidates.end(), compare_candidate_by_post),
        candidates.end()
    );
}

// コストで結果をソートする。
void MzConvResult::sort()
{
    clauses_t::iterator it, end = clauses.end();
    for (it = clauses.begin(); it != end; ++it) {
        MzConvClause& clause = *it;
        clause.sort();
    }
}

//////////////////////////////////////////////////////////////////////////////
// LatticeNode - ラティス（lattice）のノード。

// 動詞か？
bool LatticeNode::IsDoushi() const
{
    switch (bunrui) {
    case HB_GODAN_DOUSHI: case HB_ICHIDAN_DOUSHI:
    case HB_KAHEN_DOUSHI: case HB_SAHEN_DOUSHI:
        return true;
    default:
        return false;
    }
}

// 助動詞か？
bool LatticeNode::IsJodoushi() const
{
    switch (bunrui) {
    case HB_JODOUSHI:
    case HB_MIZEN_JODOUSHI: case HB_RENYOU_JODOUSHI:
    case HB_SHUUSHI_JODOUSHI: case HB_RENTAI_JODOUSHI:
    case HB_KATEI_JODOUSHI: case HB_MEIREI_JODOUSHI:
        return true;
    default:
        return false;
    }
}

// 助詞か？
bool LatticeNode::IsJoshi() const
{
    switch (bunrui) {
    case HB_KAKU_JOSHI:
    case HB_SETSUZOKU_JOSHI:
    case HB_FUKU_JOSHI:
    case HB_SHUU_JOSHI:
        return true;
    default:
        return false;
    }
}

// 形容詞か？
bool LatticeNode::IsKeiyoushi() const
{
    return bunrui == HB_IKEIYOUSHI || bunrui == HB_NAKEIYOUSHI;
}

//////////////////////////////////////////////////////////////////////////////
// Lattice - ラティス

// 追加情報。
void Lattice::AddExtraNodes()
{
    FOOTMARK();
    static const LPCWSTR s_weekdays[] = {
        L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat"
    };
    static const LPCWSTR s_months[] = {
        L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun",
        L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec"
    };

    WCHAR sz[128];

    // 現在の日時を取得する。
    SYSTEMTIME st;
    ::GetLocalTime(&st);

    // 今日（today）
    if (m_pre == L"きょう") {
        WStrings fields(NUM_FIELDS);
        fields[I_FIELD_PRE] = m_pre;
        fields[I_FIELD_HINSHI].resize(1);
        fields[I_FIELD_HINSHI][0] = MAKEWORD(HB_MEISHI, 0);

        StringCchPrintfW(sz, _countof(sz), L"%u年%u月%u日", st.wYear, st.wMonth, st.wDay);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        fields[I_FIELD_POST] = mz_convert_to_kansuuji_brief(sz);
        DoFields(0, fields, +10);

        fields[I_FIELD_POST] = mz_convert_to_kansuuji_brief_formal(sz);
        DoFields(0, fields, +10);

        StringCchPrintfW(sz, _countof(sz), L"%u年%02u月%02u日", st.wYear, st.wMonth, st.wDay);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        fields[I_FIELD_POST] = mz_convert_to_kansuuji_brief(sz);
        DoFields(0, fields, +10);

        fields[I_FIELD_POST] = mz_convert_to_kansuuji_brief_formal(sz);
        DoFields(0, fields, +10);

        StringCchPrintfW(sz, _countof(sz), L"%04u-%02u-%02u", st.wYear, st.wMonth, st.wDay);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        StringCchPrintfW(sz, _countof(sz), L"%04u/%02u/%02u", st.wYear, st.wMonth, st.wDay);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        StringCchPrintfW(sz, _countof(sz), L"%04u/%u/%u", st.wYear, st.wMonth, st.wDay);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        StringCchPrintfW(sz, _countof(sz), L"%04u.%02u.%02u", st.wYear, st.wMonth, st.wDay);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        StringCchPrintfW(sz, _countof(sz), L"%04u.%u.%u", st.wYear, st.wMonth, st.wDay);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        StringCchPrintfW(sz, _countof(sz), L"%02u/%02u/%04u", st.wMonth, st.wDay, st.wYear);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        StringCchPrintfW(sz, _countof(sz), L"%u/%u/%04u", st.wMonth, st.wDay, st.wYear);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        StringCchPrintfW(sz, _countof(sz), L"%s %s %02u %04u",
                         s_weekdays[st.wDayOfWeek], s_months[st.wMonth - 1],
                         st.wDay, st.wYear);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        return;
    }

    // 今年（this year）
    if (m_pre == L"ことし") {
        WStrings fields(NUM_FIELDS);
        fields[I_FIELD_PRE] = m_pre;
        fields[I_FIELD_HINSHI].resize(1);
        fields[I_FIELD_HINSHI][0] = MAKEWORD(HB_MEISHI, 0);

        StringCchPrintfW(sz, _countof(sz), L"%u年", st.wYear);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        fields[I_FIELD_POST] = mz_convert_to_kansuuji_brief(sz);
        DoFields(0, fields, +10);

        fields[I_FIELD_POST] = mz_convert_to_kansuuji_brief_formal(sz);
        DoFields(0, fields, +10);

        return;
    }

    // 今月（this month）
    if (m_pre == L"こんげつ") {
        WStrings fields(NUM_FIELDS);
        fields[I_FIELD_PRE] = m_pre;
        fields[I_FIELD_HINSHI].resize(1);
        fields[I_FIELD_HINSHI][0] = MAKEWORD(HB_MEISHI, 0);

        StringCchPrintfW(sz, _countof(sz), L"%u年%u月", st.wYear, st.wMonth);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        fields[I_FIELD_POST] = mz_convert_to_kansuuji_brief(sz);
        DoFields(0, fields, +10);

        fields[I_FIELD_POST] = mz_convert_to_kansuuji_brief_formal(sz);
        DoFields(0, fields, +10);

        StringCchPrintfW(sz, _countof(sz), L"%u年%02u月", st.wYear, st.wMonth);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        fields[I_FIELD_POST] = mz_convert_to_kansuuji_brief(sz);
        DoFields(0, fields, +10);

        fields[I_FIELD_POST] = mz_convert_to_kansuuji_brief_formal(sz);
        DoFields(0, fields, +10);

        return;
    }

    // 現在の時刻（current time）
    if (m_pre == L"じこく" || m_pre == L"ただいま") {
        WStrings fields(NUM_FIELDS);
        fields[I_FIELD_PRE] = m_pre;
        fields[I_FIELD_HINSHI].resize(1);
        fields[I_FIELD_HINSHI][0] = MAKEWORD(HB_MEISHI, 0);

        if (m_pre == L"ただいま") {
            fields[I_FIELD_POST] = L"ただ今";
            DoFields(0, fields);
            fields[I_FIELD_POST] = L"只今";
            DoFields(0, fields);
        }

        StringCchPrintfW(sz, _countof(sz), L"%u時%u分%u秒", st.wHour, st.wMinute, st.wSecond);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        fields[I_FIELD_POST] = mz_convert_to_kansuuji_brief(sz);
        DoFields(0, fields, +10);

        fields[I_FIELD_POST] = mz_convert_to_kansuuji_brief_formal(sz);
        DoFields(0, fields, +10);

        StringCchPrintfW(sz, _countof(sz), L"%02u時%02u分%02u秒", st.wHour, st.wMinute, st.wSecond);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        fields[I_FIELD_POST] = mz_convert_to_kansuuji_brief(sz);
        DoFields(0, fields, +10);

        fields[I_FIELD_POST] = mz_convert_to_kansuuji_brief_formal(sz);
        DoFields(0, fields, +10);

        if (st.wHour >= 12) {
            StringCchPrintfW(sz, _countof(sz), L"午後%u時%u分%u秒", st.wHour - 12, st.wMinute, st.wSecond);
            fields[I_FIELD_POST] = sz;
            DoFields(0, fields);

            fields[I_FIELD_POST] = mz_convert_to_kansuuji_brief(sz);
            DoFields(0, fields, +10);

            fields[I_FIELD_POST] = mz_convert_to_kansuuji_brief_formal(sz);
            DoFields(0, fields, +10);

            StringCchPrintfW(sz, _countof(sz), L"午後%02u時%02u分%02u秒", st.wHour - 12, st.wMinute, st.wSecond);
            fields[I_FIELD_POST] = sz;
            DoFields(0, fields);

            fields[I_FIELD_POST] = mz_convert_to_kansuuji_brief(sz);
            DoFields(0, fields, +10);

            fields[I_FIELD_POST] = mz_convert_to_kansuuji_brief_formal(sz);
            DoFields(0, fields, +10);
        } else {
            StringCchPrintfW(sz, _countof(sz), L"午前%u時%u分%u秒", st.wHour, st.wMinute, st.wSecond);
            fields[I_FIELD_POST] = sz;
            DoFields(0, fields);

            fields[I_FIELD_POST] = mz_convert_to_kansuuji_brief(sz);
            DoFields(0, fields, +10);

            fields[I_FIELD_POST] = mz_convert_to_kansuuji_brief_formal(sz);
            DoFields(0, fields, +10);

            StringCchPrintfW(sz, _countof(sz), L"午前%02u時%02u分%02u秒", st.wHour, st.wMinute, st.wSecond);
            fields[I_FIELD_POST] = sz;
            DoFields(0, fields);

            fields[I_FIELD_POST] = mz_convert_to_kansuuji_brief(sz);
            DoFields(0, fields, +10);

            fields[I_FIELD_POST] = mz_convert_to_kansuuji_brief_formal(sz);
            DoFields(0, fields, +10);
        }

        StringCchPrintfW(sz, _countof(sz), L"%02u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);
        return;
    }

    if (m_pre == L"にちじ") { // date and time
        WStrings fields(NUM_FIELDS);
        fields[I_FIELD_PRE] = m_pre;
        fields[I_FIELD_HINSHI].resize(1);
        fields[I_FIELD_HINSHI][0] = MAKEWORD(HB_MEISHI, 0);

        StringCchPrintfW(sz, _countof(sz), L"%u年%u月%u日%u時%u分%u秒",
                         st.wYear, st.wMonth, st.wDay,
                         st.wHour, st.wMinute, st.wSecond);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        StringCchPrintfW(sz, _countof(sz), L"%04u年%02u月%02u日%02u時%02u分%02u秒",
                         st.wYear, st.wMonth, st.wDay,
                         st.wHour, st.wMinute, st.wSecond);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        StringCchPrintfW(sz, _countof(sz), L"%04u-%02u-%02u %02u:%02u:%02u",
                         st.wYear, st.wMonth, st.wDay,
                         st.wHour, st.wMinute, st.wSecond);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        StringCchPrintfW(sz, _countof(sz), L"%s %s %02u %04u %02u:%02u:%02u",
                         s_weekdays[st.wDayOfWeek], s_months[st.wMonth - 1],
                         st.wDay, st.wYear,
                         st.wHour, st.wMinute, st.wSecond);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);
    }

    if (m_pre == L"じぶん") { // myself
        DWORD dwSize = _countof(sz);
        if (::GetUserNameW(sz, &dwSize)) {
            WStrings fields(NUM_FIELDS);
            fields[I_FIELD_PRE] = m_pre;
            fields[I_FIELD_HINSHI].resize(1);
            fields[I_FIELD_HINSHI][0] = MAKEWORD(HB_MEISHI, 0);
            fields[I_FIELD_POST] = sz;
            DoFields(0, fields);
        }
        return;
    }

    // カッコ (parens, brackets, braces, ...)
    if (m_pre == L"かっこ") {
        WStrings items;
        str_split(items, TheIME.LoadSTR(IDS_PAREN), std::wstring(L"\t"));

        WStrings fields(NUM_FIELDS);
        fields[I_FIELD_PRE] = m_pre;
        fields[I_FIELD_HINSHI].resize(1);
        fields[I_FIELD_HINSHI][0] = MAKEWORD(HB_SYMBOL, 0);
        for (size_t i = 0; i < items.size(); ++i) {
            std::wstring& item = items[i];
            fields[I_FIELD_POST] = item;
            DoFields(0, fields);
        }
        return;
    }

    // 記号（symbols）
    static const WCHAR *s_words[] = {
        L"きごう",      // IDS_SYMBOLS
        L"けいせん",    // IDS_KEISEN
        L"けいさん",    // IDS_MATH
        L"さんかく",    // IDS_SANKAKU
        L"しかく",      // IDS_SHIKAKU
        L"ずけい",      // IDS_ZUKEI
        L"まる",        // IDS_MARU
        L"ほし",        // IDS_STARS
        L"ひし",        // IDS_HISHI
        L"てん",        // IDS_POINTS
        L"たんい",      // IDS_UNITS
        L"ふとうごう",  // IDS_FUTOUGOU
        L"たて",        // IDS_TATE
        L"たてひだり",  // IDS_TATE_HIDARI
        L"たてみぎ",    // IDS_TATE_MIGI
        L"ひだりうえ",  // IDS_HIDARI_UE
        L"ひだりした",  // IDS_HIDARI_SHITA
        L"ふとわく",    // IDS_FUTO_WAKU
        L"ほそわく",    // IDS_HOSO_WAKU
        L"まんなか",    // IDS_MANNAKA
        L"みぎうえ",    // IDS_MIGI_UE
        L"みぎした",    // IDS_MIGI_SHITA
        L"よこ",        // IDS_YOKO
        L"よこうえ",    // IDS_YOKO_UE
        L"よこした",    // IDS_YOKO_SHITA
        L"おなじ",      // IDS_SAME
        L"やじるし",    // IDS_ARROWS
        L"ぎりしゃ",    // IDS_GREEK
        L"うえ",        // IDS_UP
        L"した",        // IDS_DOWN
        L"ひだり",      // IDS_LEFT
        L"みぎ",        // IDS_RIGHT
    };
    for (size_t i = 0; i < _countof(s_words); ++i) {
        if (m_pre == s_words[i]) {
            WStrings items;
            WCHAR *pch = TheIME.LoadSTR(IDS_SYMBOLS + INT(i));
            WStrings fields(NUM_FIELDS);
            fields[I_FIELD_PRE] = m_pre;
            fields[I_FIELD_HINSHI].resize(1);
            fields[I_FIELD_HINSHI][0] = MAKEWORD(HB_SYMBOL, 0);
            int cost = 0;
            while (*pch) {
                fields[I_FIELD_POST].assign(1, *pch++);
                DoFields(0, fields, cost);
                ++cost;
            }
            return;
        }
    }
} // Lattice::AddExtraNodes

// 辞書からノード群を追加する。
BOOL Lattice::AddNodesFromDict(size_t index, const WCHAR *dict_data)
{
    FOOTMARK();
    const size_t length = m_pre.size();
    ASSERT(length);

    // フィールド区切り（separator）。
    std::wstring sep;
    sep.resize(1);
    sep[0] = FIELD_SEP;

    WStrings fields, records;
    for (; index < length; ++index) {
        // periods (。。。)
        if (mz_is_period(m_pre[index])) {
            size_t saved = index;
            do {
                ++index;
            } while (mz_is_period(m_pre[index]));

            fields.resize(NUM_FIELDS);
            fields[I_FIELD_PRE] = m_pre.substr(saved, index - saved);
            fields[I_FIELD_HINSHI].resize(1);
            fields[I_FIELD_HINSHI][0] = MAKEWORD(HB_PERIOD, 0);
            switch (index - saved) {
            case 2:
                fields[I_FIELD_POST] += L'‥';
                break;
            case 3:
                fields[I_FIELD_POST] += L'…';
                break;
            default:
                fields[I_FIELD_POST] = fields[I_FIELD_PRE];
                break;
            }
            DoFields(saved, fields);
            --index;
            continue;
        }

        // center dots (・・・)
        if (m_pre[index] == L'・') {
            size_t saved = index;
            do {
                ++index;
            } while (m_pre[index] == L'・');

            fields.resize(NUM_FIELDS);
            fields[I_FIELD_PRE] = m_pre.substr(saved, index - saved);
            fields[I_FIELD_HINSHI].resize(1);
            fields[I_FIELD_HINSHI][0] = MAKEWORD(HB_SYMBOL, 0);
            switch (index - saved) {
            case 2:
                fields[I_FIELD_POST] += L'‥';
                break;
            case 3:
                fields[I_FIELD_POST] += L'…';
                break;
            default:
                fields[I_FIELD_POST] = fields[I_FIELD_PRE];
                break;
            }
            DoFields(saved, fields);
            --index;
            continue;
        }

        // commas (、、、)
        if (mz_is_comma(m_pre[index])) {
            size_t saved = index;
            do {
                ++index;
            } while (mz_is_comma(m_pre[index]));

            fields.resize(NUM_FIELDS);
            fields[I_FIELD_PRE] = m_pre.substr(saved, index - saved);
            fields[I_FIELD_HINSHI].resize(1);
            fields[I_FIELD_HINSHI][0] = MAKEWORD(HB_COMMA, 0);
            fields[I_FIELD_POST] = fields[I_FIELD_PRE];
            DoFields(saved, fields);
            --index;
            continue;
        }

        // arrow right (→)
        if (mz_is_hyphen(m_pre[index]) && (m_pre[index + 1] == L'>' || m_pre[index + 1] == L'＞'))
        {
            fields.resize(NUM_FIELDS);
            fields[I_FIELD_PRE] = m_pre.substr(index, 2);
            fields[I_FIELD_HINSHI].resize(1);
            fields[I_FIELD_HINSHI][0] = MAKEWORD(HB_SYMBOL, 0);
            fields[I_FIELD_POST].resize(1);
            fields[I_FIELD_POST][0] = L'→';
            DoFields(index, fields);
            ++index;
            continue;
        }

        // arrow left (←)
        if ((m_pre[index] == L'<' || m_pre[index] == L'＜') && mz_is_hyphen(m_pre[index + 1]))
        {
            fields.resize(NUM_FIELDS);
            fields[I_FIELD_PRE] = m_pre.substr(index, 2);
            fields[I_FIELD_HINSHI].resize(1);
            fields[I_FIELD_HINSHI][0] = MAKEWORD(HB_SYMBOL, 0);
            fields[I_FIELD_POST].resize(1);
            fields[I_FIELD_POST][0] = L'←';
            DoFields(index, fields);
            ++index;
            continue;
        }

        // arrows (zh, zj, zk, zl) and z. etc.
        WCHAR ch0 = mz_translate_char(m_pre[index], FALSE, TRUE);
        if (ch0 == L'z' || ch0 == L'Z') {
            WCHAR ch1 = mz_translate_char(m_pre[index + 1], FALSE, TRUE);
            WCHAR ch2 = 0;
            if (ch1 == L'h' || ch1 == L'H') ch2 = L'←'; // zh
            else if (ch1 == L'j' || ch1 == L'J') ch2 = L'↓'; // zj
            else if (ch1 == L'k' || ch1 == L'K') ch2 = L'↑'; // zk
            else if (ch1 == L'l' || ch1 == L'L') ch2 = L'→'; // zl
            else if (mz_is_hyphen(ch1)) ch2 = L'～'; // z-
            else if (mz_is_period(ch1)) ch2 = L'…'; // z.
            else if (mz_is_comma(ch1)) ch2 = L'‥'; // z,
            else if (ch1 == L'[' || ch1 == L'［' || ch1 == L'「') ch2 = L'『'; // z[
            else if (ch1 == L']' || ch1 == L'］' || ch1 == L'」') ch2 = L'』'; // z]
            else if (ch1 == L'/' || ch1 == L'／') ch2 = L'・'; // z/
            if (ch2) {
                fields.resize(NUM_FIELDS);
                fields[I_FIELD_PRE] = m_pre.substr(index, 2);
                fields[I_FIELD_HINSHI].resize(1);
                fields[I_FIELD_HINSHI][0] = MAKEWORD(HB_SYMBOL, 0);
                fields[I_FIELD_POST].resize(1);
                fields[I_FIELD_POST][0] = ch2;
                DoFields(index, fields, -100);
                ++index;
                continue;
            }
        }

        if (!mz_is_hiragana(m_pre[index])) { // ひらがなではない？
            size_t saved = index;
            do {
                ++index;
            } while ((!mz_is_hiragana(m_pre[index]) || mz_is_hyphen(m_pre[index])) && m_pre[index]);

            fields.resize(NUM_FIELDS);
            fields[I_FIELD_PRE] = m_pre.substr(saved, index - saved);
            fields[I_FIELD_HINSHI].resize(1);
            fields[I_FIELD_HINSHI][0] = MAKEWORD(HB_MEISHI, 0);
            fields[I_FIELD_POST] = fields[I_FIELD_PRE];
            DoMeishi(saved, fields);

            // 全部が数字なら特殊な変換を行う。
            if (mz_are_all_chars_numeric(fields[I_FIELD_PRE])) {
                fields[I_FIELD_POST] = mz_convert_to_kansuuji(fields[I_FIELD_PRE]);
                DoMeishi(saved, fields);
                fields[I_FIELD_POST] = mz_convert_to_kansuuji_brief(fields[I_FIELD_PRE]);
                DoMeishi(saved, fields);
                fields[I_FIELD_POST] = mz_convert_to_kansuuji_formal(fields[I_FIELD_PRE]);
                DoMeishi(saved, fields);
                fields[I_FIELD_POST] = mz_convert_to_kansuuji_brief_formal(fields[I_FIELD_PRE]);
                DoMeishi(saved, fields);
            }

            // 郵便番号変換。
            std::wstring postal = mz_normalize_postal_code(fields[I_FIELD_PRE]);
            if (postal.size()) {
                std::wstring addr = mz_convert_postal_code(postal);
                if (addr.size()) {
                    fields[I_FIELD_POST] = addr;
                    DoMeishi(saved, fields, -10);
                }
            }

            --index;
            continue;
        }

        // 基本辞書をスキャンする。
        size_t count = ScanBasicDict(records, dict_data, m_pre[index]);
        DPRINTW(L"ScanBasicDict(%c) count: %d\n", m_pre[index], count);

        // ユーザー辞書をスキャンする。
        count = ScanUserDict(records, m_pre[index], this);
        DPRINTW(L"ScanUserDict(%c) count: %d\n", m_pre[index], count);

        // 各レコードをフィールドに分割し、処理する。
        for (size_t i = 0; i < records.size(); ++i) {
            std::wstring& record = records[i];
            str_split(fields, record, sep);
            DoFields(index, fields);
        }
    }

    return TRUE;
} // Lattice::AddNodesFromDict

struct lattice_compare
{
    std::wstring m_pre;
    lattice_compare(std::wstring pre) : m_pre(pre)
    {
    }

    bool operator()(const LatticeNodePtr& n) const {
        return n->pre.size() != m_pre.size();
    }
};

// 単一文節変換用のノード群を追加する。
BOOL Lattice::AddNodesFromDict(const WCHAR *dict_data)
{
    // 区切りを準備。
    std::wstring sep;
    sep.resize(1);
    sep[0] = FIELD_SEP;

    // 基本辞書をスキャンする。
    WStrings fields, records;
    size_t count = ScanBasicDict(records, dict_data, m_pre[0]);
    DPRINTW(L"ScanBasicDict(%c) count: %d\n", m_pre[0], count);

    // ユーザー辞書をスキャンする。
    count = ScanUserDict(records, m_pre[0], this);
    DPRINTW(L"ScanUserDict(%c) count: %d\n", m_pre[0], count);

    // 各レコードをフィールドに分割して処理。
    for (size_t i = 0; i < records.size(); ++i) {
        std::wstring& record = records[i];
        str_split(fields, record, sep);
        DoFields(0, fields);
    }

    lattice_compare compare(m_pre);

    // 異なるサイズのノードを削除する。
    for (size_t i = 0; i < ARRAY_AT(m_chunks, 0).size(); ++i) {
        LatticeChunk::iterator it;
        it = std::remove_if(ARRAY_AT(m_chunks, 0).begin(), ARRAY_AT(m_chunks, 0).end(), compare);
        ARRAY_AT(m_chunks, 0).erase(it, ARRAY_AT(m_chunks, 0).end());
    }

    return !ARRAY_AT(m_chunks, 0).empty();
}

// 部分最小コストを計算する。
INT Lattice::CalcSubTotalCosts(LatticeNode *ptr1)
{
    ASSERT(ptr1);

    if (ptr1->subtotal_cost != MAXLONG)
        return ptr1->subtotal_cost;

    INT min_cost = MAXLONG;
    LatticeNode *min_node = NULL;
    if (ptr1->reverse_branches.empty())
        min_cost = 0;

    reverse_branches_t::iterator it, end = ptr1->reverse_branches.end();
    for (it = ptr1->reverse_branches.begin(); it != end; ++it) {
        LatticeNode *ptr0 = *it;
        INT word_cost = ptr1->WordCost();
        INT connect_cost = ptr0->ConnectCost(*ptr1);
        INT cost = CalcSubTotalCosts(ptr0);
        cost += word_cost;
        cost += connect_cost;
        if (cost < min_cost) {
            min_cost = cost;
            min_node = ptr0;
        }
    }

    if (min_node) {
        min_node->marked = 1;
    }

    ptr1->subtotal_cost = min_cost;
    return min_cost;
} // Lattice::CalcSubTotalCosts

// リンクを更新する。
void Lattice::UpdateLinksAndBranches()
{
    ASSERT(m_pre.size());
    ASSERT(m_pre.size() + 1 == m_chunks.size());

    // リンク数とブランチ群をリセットする。
    ResetLatticeInfo();

    // ヘッド（頭）を追加する。リンク数は１。
    {
        LatticeNode node;
        node.bunrui = HB_HEAD;
        node.linked = 1;
        // 現在位置のノードを先頭ブランチに追加する。
        LatticeChunk& chunk1 = ARRAY_AT(m_chunks, 0);
        LatticeChunk::iterator it, end = chunk1.end();
        for (it = chunk1.begin(); it != end; ++it) {
            LatticeNodePtr& ptr1 = *it;
            if (node.CanConnectTo(*ptr1)) {
                ptr1->linked = 1;
                node.branches.push_back(ptr1);
            }
        }
        m_head = unboost::make_shared<LatticeNode>(node);
    }

    // 尻尾（テイル）を追加する。
    {
        LatticeNode node;
        node.bunrui = HB_TAIL;
        m_tail = unboost::make_shared<LatticeNode>(node);
        ARRAY_AT(m_chunks, m_pre.size()).clear();
        ARRAY_AT(m_chunks, m_pre.size()).push_back(m_tail);
    }

    // 各インデックス位置について。
    for (size_t index = 0; index < m_pre.size(); ++index) {
        // インデックス位置のノード集合を取得。
        LatticeChunk& chunk1 = ARRAY_AT(m_chunks, index);
        // 各ノードについて。
        LatticeChunk::iterator it, end = chunk1.end();
        for (it = chunk1.begin(); it != end; ++it) {
            LatticeNodePtr& ptr1 = *it;
            // リンク数がゼロならば無視。
            if (!ptr1->linked)
                continue;
            // 区間チェック。
            ASSERT(index + ptr1->pre.size() <= m_pre.size());
            if (!(index + ptr1->pre.size() <= m_pre.size()))
                continue;
            // 連結可能であれば、リンク先をブランチに追加し、リンク先のリンク数を増やす。
            LatticeChunk& chunk2 = ARRAY_AT(m_chunks, index + ptr1->pre.size());
            {
                LatticeChunk::iterator it, end = chunk2.end();
                for (it = chunk2.begin(); it != end; ++it) {
                    LatticeNodePtr& ptr2 = *it;
                    if (ptr1->CanConnectTo(*ptr2.get())) {
                        ptr1->branches.push_back(ptr2);
                        ptr2->linked++;
                    }
                }
            }
        }
    }
} // Lattice::UpdateLinksAndBranches

// リンク数とブランチ群をリセットする。
void Lattice::ResetLatticeInfo()
{
    for (size_t index = 0; index < m_pre.size(); ++index) {
        LatticeChunk& chunk1 = ARRAY_AT(m_chunks, index);
        LatticeChunk::iterator it, end = chunk1.end();
        for (it = chunk1.begin(); it != end; ++it) {
            LatticeNodePtr& ptr1 = *it;
            ptr1->linked = 0;
            ptr1->branches.clear();
            ptr1->reverse_branches.clear();
        }
    }
} // Lattice::ResetLatticeInfo

// 変換失敗時に未定義の単語を追加する。
void Lattice::AddComplement()
{
    size_t lastIndex = GetLastLinkedIndex();
    if (lastIndex == m_pre.size())
        return;

    lastIndex += ARRAY_AT(m_chunks, lastIndex)[0]->pre.size();

    LatticeNode node;
    node.bunrui = HB_UNKNOWN;
    node.deltaCost = 0;
    node.pre = node.post = m_pre.substr(lastIndex);
    ARRAY_AT(m_chunks, lastIndex).push_back(unboost::make_shared<LatticeNode>(node));
    UpdateLinksAndBranches();
} // Lattice::AddComplement

// 変換失敗時に未定義の単語を追加する。
void Lattice::AddComplement(size_t index, size_t min_size, size_t max_size)
{
    WStrings fields(NUM_FIELDS);
    fields[I_FIELD_HINSHI].resize(1);
    fields[I_FIELD_HINSHI][0] = MAKEWORD(HB_MEISHI, 0);
    for (size_t count = min_size; count <= max_size; ++count) {
        if (m_pre.size() < index + count)
            continue;
        fields[I_FIELD_PRE] = m_pre.substr(index, count);
        fields[I_FIELD_POST] = fields[I_FIELD_PRE];
        DoFields(index, fields);
    }
} // Lattice::AddComplement

static inline bool latice_compare_by_linked(const LatticeNodePtr& node) {
    return node->linked == 0;
}

// リンクされていないノードを削除。
void Lattice::CutUnlinkedNodes()
{
    for (size_t index = 0; index < m_pre.size(); ++index) {
        LatticeChunk& chunk1 = ARRAY_AT(m_chunks, index);
        LatticeChunk::iterator it;
        it = std::remove_if(chunk1.begin(), chunk1.end(), latice_compare_by_linked);
        chunk1.erase(it, chunk1.end());
    }
} // Lattice::CutUnlinkedNodes

// 最後にリンクされたインデックスを取得する。
size_t Lattice::GetLastLinkedIndex() const
{
    // 最後にリンクされたノードがあるか？
    if (ARRAY_AT(m_chunks, m_pre.size())[0]->linked) {
        return m_pre.size(); // 最後のインデックスを返す。
    }

    // チャンクを逆順でスキャンする。
    for (size_t index = m_pre.size(); index > 0; ) {
        --index;
        const LatticeChunk& chunk = ARRAY_AT(m_chunks, index);
        LatticeChunk::const_iterator it, end = chunk.end();
        for (it = chunk.begin(); it != end; ++it) {
            unboost::shared_ptr<LatticeNode> ptr = *it;
            if (ptr->linked) {
                return index; // リンクされたノードが見つかった。
            }
        }
    }
    return 0; // not found
} // Lattice::GetLastLinkedIndex

// ノードを一つ追加する。
void Lattice::AddNode(size_t index, const LatticeNode& node)
{
    // ノードを追加するとき、必ずこの関数を通る。
    // ここで条件付きでブレークさせて、呼び出し履歴を取得すれば、
    // どのようにノードが追加されているのかが観測できる。
    ASSERT(index + node.pre.size() <= m_pre.size());
    ARRAY_AT(m_chunks, index).push_back(unboost::make_shared<LatticeNode>(node));
}

// イ形容詞を変換する。
void Lattice::DoIkeiyoushi(size_t index, const WStrings& fields, INT deltaCost)
{
    FOOTMARK();
    ASSERT(fields.size() == NUM_FIELDS);
    ASSERT(fields[I_FIELD_PRE].size());
    size_t length = fields[I_FIELD_PRE].size();

    // 区間チェック。
    if (index + length > m_pre.size()) {
        return;
    }
    // 対象のテキストが語幹と一致するか確かめる。
    if (m_pre.substr(index, length) != fields[I_FIELD_PRE]) {
        return;
    }
    // 語幹の後の部分文字列。
    std::wstring tail = m_pre.substr(index + length);

    // ラティスノードの準備。
    LatticeNode node;
    node.bunrui = HB_IKEIYOUSHI;
    node.tags = fields[I_FIELD_TAGS];
    node.deltaCost = deltaCost;

    // い形容詞の未然形。
    // 「痛い」→「痛かろ(う)」
    do {
        if (tail.empty() || tail.substr(0, 3) != L"かろう") break;
        node.katsuyou = MIZEN_KEI;
        node.pre = fields[I_FIELD_PRE] + L"かろう";
        node.post = fields[I_FIELD_POST] + L"かろう";
        AddNode(index, node);
    } while (0);

    mz_make_literal_maps();

    // い形容詞の連用形。
    // 「痛い」→「痛かっ(た)」
    do {
        if (tail.empty() || tail.substr(0, 2) != L"かっ") break;
        node.pre = fields[I_FIELD_PRE] + L"かっ";
        node.post = fields[I_FIELD_POST] + L"かっ";
        node.katsuyou = RENYOU_KEI;
        AddNode(index, node);

        if (tail.size() < 3 || tail[2] != L'た') break;
        node.pre += L'た';
        node.post += L'た';
        node.katsuyou = SHUUSHI_KEI;
        AddNode(index, node);
    } while (0);
    // 「痛い」→「痛く(て)」、「広い」→「広く(て)」
    do {
        if (tail.empty() || tail[0] != L'く') break;
        node.pre = fields[I_FIELD_PRE] + L'く';
        node.post = fields[I_FIELD_POST] + L'く';
        AddNode(index, node);

        if (tail.size() < 2 || tail[1] != L'て') break;
        node.pre += tail[1];
        node.post += tail[1];
        AddNode(index, node);
    } while (0);
    // 「広い」→「広う(て)」
    do {
        if (tail.empty() || tail[0] != L'う') break;
        node.pre = fields[I_FIELD_PRE] + L'う';
        node.post = fields[I_FIELD_POST] + L'う';
        AddNode(index, node);
    } while (0);
    // 「美しい」→「美しゅう(て)」
    do {
        if (tail.empty() || tail[0] != L'ゅ' || tail[1] != L'う') break;
        node.pre = fields[I_FIELD_PRE] + L"ゅう";
        node.post = fields[I_FIELD_POST] + L"ゅう";
        AddNode(index, node);
    } while (0);
    // TODO: 「危ない」→「危のう(て)」
    // TODO: 「暖かい」→「暖こう(て)」

    // い形容詞の終止形。「かわいい」「かわいいよ」「かわいいね」「かわいいぞ」
    node.katsuyou = SHUUSHI_KEI;
    do {
        if (tail.empty() || tail[0] != L'い') break;
        node.pre = fields[I_FIELD_PRE] + L'い';
        node.post = fields[I_FIELD_POST] + L'い';
        AddNode(index, node);
        if (tail.size() < 1 ||
            (tail[1] != L'よ' && tail[1] != L'ね' && tail[1] != L'な' && tail[1] != L'ぞ'))
                break;
        node.pre += tail[1];
        node.post += tail[1];
        AddNode(index, node);
    } while (0);

    // い形容詞の連体形。
    // 「痛い」→「痛い(とき)」
    node.katsuyou = RENTAI_KEI;
    do {
        if (tail.empty() || tail[0] != L'い') break;
        node.pre = fields[I_FIELD_PRE] + L'い';
        node.post = fields[I_FIELD_POST] + L'い';
        AddNode(index, node);
    } while (0);
    // 「痛い」→「痛き(とき)」
    do {
        if (tail.empty() || tail[0] != L'き') break;
        node.pre = fields[I_FIELD_PRE] + L'き';
        node.post = fields[I_FIELD_POST] + L'き';
        AddNode(index, node);
    } while (0);

    // い形容詞の仮定形。
    // 「痛い」→「痛けれ(ば)」
    do {
        if (tail.empty() || tail.substr(0, 2) != L"けれ") break;
        node.katsuyou = KATEI_KEI;
        node.pre = fields[I_FIELD_PRE] + L"けれ";
        node.post = fields[I_FIELD_POST] + L"けれ";
        AddNode(index, node);
        if (tail.empty() || tail.substr(0, 3) != L"ければ") break;
        node.katsuyou = KATEI_KEI;
        node.pre = fields[I_FIELD_PRE] + L"ければ";
        node.post = fields[I_FIELD_POST] + L"ければ";
        AddNode(index, node);
    } while (0);

    // い形容詞の名詞形。
    // 「痛い(い形容詞)」→「痛さ(名詞)」、
    // 「痛い(い形容詞)」→「痛み(名詞)」、
    // 「痛い(い形容詞)」→「痛め(名詞)」「痛目(名詞)」など。
    node.bunrui = HB_MEISHI;
    do {
        if (tail.empty() || tail[0] != L'さ') break;
        node.pre = fields[I_FIELD_PRE] + L'さ';
        node.post = fields[I_FIELD_POST] + L'さ';
        AddNode(index, node);
    } while (0);
    do {
        if (tail.empty() || tail[0] != L'み') break;
        node.pre = fields[I_FIELD_PRE] + L'み';
        node.post = fields[I_FIELD_POST] + L'み';
        AddNode(index, node);
    } while (0);
    do {
        if (tail.empty() || tail[0] != L'め') break;
        node.pre = fields[I_FIELD_PRE] + L'め';
        node.post = fields[I_FIELD_POST] + L'め';
        AddNode(index, node);

        node.post = fields[I_FIELD_POST] + L'目';
        AddNode(index, node);
    } while (0);

    // 「痛い(い形容詞)」→「痛そうな(な形容詞)」など。
    if (tail.size() >= 2 && tail[0] == L'そ' && tail[1] == L'う') {
        WStrings new_fields = fields;
        new_fields[I_FIELD_PRE] = fields[I_FIELD_PRE] + L"そう";
        new_fields[I_FIELD_POST] = fields[I_FIELD_POST] + L"そう";
        DoNakeiyoushi(index, new_fields, deltaCost);
    }

    // 「痛い(い形容詞)」→「痛すぎる(一段動詞)」
    if (tail.size() >= 2 && tail[0] == L'す' && tail[1] == L'ぎ') {
        WStrings new_fields = fields;
        new_fields[I_FIELD_PRE] = fields[I_FIELD_PRE] + L"すぎ";
        new_fields[I_FIELD_POST] = fields[I_FIELD_POST] + L"すぎ";
        DoIchidanDoushi(index, new_fields, deltaCost);
        new_fields[I_FIELD_POST] = fields[I_FIELD_POST] + L"過ぎ";
        DoIchidanDoushi(index, new_fields, deltaCost);
    }

    // 「痛。」「寒。」など
    if (tail.empty()) {
        DoMeishi(index, fields, deltaCost);
    } else {
        switch (tail[0]) {
        case L'。': case L'、': case L'，': case L'．': case L'.': case L',':
            DoMeishi(index, fields, deltaCost);
            break;
        }
    }
} // Lattice::DoIkeiyoushi

// ナ形容詞を変換する。
void Lattice::DoNakeiyoushi(size_t index, const WStrings& fields, INT deltaCost)
{
    FOOTMARK();
    ASSERT(fields.size() == NUM_FIELDS);
    ASSERT(fields[I_FIELD_PRE].size());
    size_t length = fields[I_FIELD_PRE].size();
    // 区間チェック。
    if (index + length > m_pre.size()) {
        return;
    }
    // 対象のテキストが語幹と一致するか確かめる。
    if (m_pre.substr(index, length) != fields[I_FIELD_PRE]) {
        return;
    }
    // 語幹の後の部分文字列。
    std::wstring tail = m_pre.substr(index + length);

    // ラティスノードの準備。
    LatticeNode node;
    node.bunrui = HB_NAKEIYOUSHI;
    node.tags = fields[I_FIELD_TAGS];
    node.deltaCost = deltaCost;

    // な形容詞の未然形。
    // 「巨大な」→「巨大だろ(う)」
    do {
        if (tail.empty() || tail.substr(0, 3) != L"だろう") break;
        node.katsuyou = MIZEN_KEI;
        node.pre = fields[I_FIELD_PRE] + L"だろう";
        node.post = fields[I_FIELD_POST] + L"だろう";
        AddNode(index, node);
        if (tail.empty() || tail.substr(0, 4) != L"だろうに") break;
        node.pre = fields[I_FIELD_PRE] + L"だろうに";
        node.post = fields[I_FIELD_POST] + L"だろうに";
        node.bunrui = HB_FUKUSHI;
        AddNode(index, node);
        node.bunrui = HB_NAKEIYOUSHI;
    } while (0);

    // な形容詞の連用形。
    // 「巨大な」→「巨大だっ(た)」
    do {
        if (tail.empty() || tail.substr(0, 2) != L"だっ") break;
        node.pre = fields[I_FIELD_PRE] + L"だっ";
        node.post = fields[I_FIELD_POST] + L"だっ";
        node.katsuyou = RENYOU_KEI;
        AddNode(index, node);

        if (tail.size() < 3 || tail[2] != L'た') break;
        node.pre += L'た';
        node.post += L'た';
        node.katsuyou = SHUUSHI_KEI;
        AddNode(index, node);
    } while (0);
    node.katsuyou = RENYOU_KEI;
    do {
        if (tail.empty() || tail[0] != L'で') break;
        node.pre = fields[I_FIELD_PRE] + L'で';
        node.post = fields[I_FIELD_POST] + L'で';
        AddNode(index, node);
    } while (0);
    do {
        if (tail.empty() || tail[0] != L'に') break;
        node.pre = fields[I_FIELD_PRE] + L'に';
        node.post = fields[I_FIELD_POST] + L'に';
        AddNode(index, node);
    } while (0);

    // な形容詞の終止形。
    // 「巨大な」→「巨大だ」「巨大だね」「巨大だぞ」
    do {
        if (tail.empty() || tail[0] != L'だ') break;
        node.katsuyou = SHUUSHI_KEI;
        node.pre = fields[I_FIELD_PRE] + L'だ';
        node.post = fields[I_FIELD_POST] + L'だ';
        AddNode(index, node);

        if (tail.size() < 1 ||
            (tail[1] != L'よ' && tail[1] != L'ね' && tail[1] != L'な' && tail[1] != L'ぞ'))
            break;
        node.pre += tail[1];
        node.post += tail[1];
        AddNode(index, node);
    } while (0);

    // な形容詞の連体形。
    // 「巨大な」→「巨大な(とき)」
    do {
        if (tail.empty() || tail[0] != L'な') break;
        node.katsuyou = RENTAI_KEI;
        node.pre = fields[I_FIELD_PRE] + L'な';
        node.post = fields[I_FIELD_POST] + L'な';
        AddNode(index, node);
    } while (0);

    // な形容詞の仮定形。
    // 「巨大な」→「巨大なら」
    do {
        if (tail.empty() || tail.substr(0, 2) != L"なら") break;
        node.katsuyou = KATEI_KEI;
        node.pre = fields[I_FIELD_PRE] + L"なら";
        node.post = fields[I_FIELD_POST] + L"なら";
        AddNode(index, node);
        if (tail.empty() || tail.substr(0, 3) != L"ならば") break;
        node.katsuyou = KATEI_KEI;
        node.pre = fields[I_FIELD_PRE] + L"ならば";
        node.post = fields[I_FIELD_POST] + L"ならば";
        AddNode(index, node);
    } while (0);

    // な形容詞の名詞形。
    // 「きれいな(な形容詞)」→「きれいさ(名詞)」、
    // 「巨大な」→「巨大さ」。
    node.bunrui = HB_MEISHI;
    do {
        if (tail.empty() || tail[0] != L'さ') break;
        node.pre = fields[I_FIELD_PRE] + L'さ';
        node.post = fields[I_FIELD_POST] + L'さ';
        AddNode(index, node);
    } while (0);

    // 「きれい。」「静か。」「巨大。」など
    if (tail.empty()) {
        DoMeishi(index, fields, deltaCost);
    } else {
        switch (tail[0]) {
        case L'。': case L'、': case L'，': case L'．': case L',': case L'.':
            DoMeishi(index, fields, deltaCost);
            break;
        }
    }
} // Lattice::DoNakeiyoushi

// 五段動詞を変換する。
void Lattice::DoGodanDoushi(size_t index, const WStrings& fields, INT deltaCost)
{
    FOOTMARK();
    ASSERT(fields.size() == NUM_FIELDS);
    ASSERT(fields[I_FIELD_PRE].size());
    size_t length = fields[I_FIELD_PRE].size();
    // 区間チェック。
    if (index + length > m_pre.size()) {
        return;
    }
    // 対象のテキストが語幹と一致するか確かめる。
    if (m_pre.substr(index, length) != fields[I_FIELD_PRE]) {
        return;
    }
    // 語幹の後の部分文字列。
    std::wstring tail = m_pre.substr(index + length);
    DPRINTW(L"DoGodanDoushi: %s, %s\n", fields[I_FIELD_PRE].c_str(), tail.c_str());

    // ラティスノードの準備。
    LatticeNode node;
    node.bunrui = HB_GODAN_DOUSHI;
    node.tags = fields[I_FIELD_TAGS];
    node.deltaCost = deltaCost;
    node.gyou = (Gyou)HIBYTE(fields[I_FIELD_HINSHI][0]);

    // 五段動詞の未然形。
    // 「咲く(五段)」→「咲か(ない)」、「食う(五段)」→「食わ(ない)」
    do {
        node.katsuyou = MIZEN_KEI;
        WCHAR ch;
        if (node.gyou == GYOU_A) {
            ch = L'わ';
        } else {
            ch = ARRAY_AT_AT(s_hiragana_table, node.gyou, DAN_A);
        }
        if (tail.empty() || tail[0] != ch)
            break;
        node.pre = fields[I_FIELD_PRE] + ch;
        node.post = fields[I_FIELD_POST] + ch;
        AddNode(index, node);

        // 「咲かせ」「食わせ」～られる
        if (tail.size() >= 2 && tail[1] == L'せ') {
            node.pre = fields[I_FIELD_PRE] + ch + tail[1];
            node.post = fields[I_FIELD_POST] + ch + tail[1];
            AddNode(index, node);
        }
    } while (0);
    // 「とまら(ない)」→「とまん(ない)」
    // 「さわら(ない)」→「さわん(ない)」
    do {
        WCHAR ch = L'ん';
        if (tail.empty() || tail[0] != ch || node.gyou != GYOU_RA) {
            break;
        }
        node.katsuyou = MIZEN_KEI;
        node.pre = fields[I_FIELD_PRE] + ch;
        node.post = fields[I_FIELD_POST] + ch;
        AddNode(index, node);
    } while (0);

    // 五段動詞の連用形。
    // 「咲く(五段)」→「咲き(ます)」、「食う(五段)」→「食い(ます)」
    node.katsuyou = RENYOU_KEI;
    WCHAR ch = ARRAY_AT_AT(s_hiragana_table, node.gyou, DAN_I);
    if (tail.size() >= 1 && tail[0] == ch) {
        node.pre = fields[I_FIELD_PRE] + ch;
        node.post = fields[I_FIELD_POST] + ch;
        AddNode(index, node);
    }

    // 五段動詞の連用形の音便の分類。
    WCHAR ch2 = 0, ch3 = 0, ch4 = 0;
    switch (node.gyou) {
    case GYOU_NA:
    case GYOU_MA:
    case GYOU_BA:
        // あそんだ、あんだ、うんだ、かんだ、くんだ、こんだ、しんだ、すんだ、
        // つんだ、とんだ、のんだ、ふんだ、もんだ、やんだ、よんだ
        ch2 = L'ん';
        ch3 = L'だ';
        ch4 = L'で';
        break;

    case GYOU_A:
    case GYOU_TA:
    case GYOU_RA:
        // あった、うった、うつった、おった、かった、きった、くった、
        // けった、さった、しった、すった、そった、たった、ちった、
        // つった、とった、なった、ぬった、のった、はった、ふった、
        // へった、ほった、まった、もった、やった、ゆった、よった、
        // わった、わらった
        ch2 = L'っ';
        ch3 = L'た';
        ch4 = L'て';
        break;

    case GYOU_KA:
        // ういた、えがいた、かいた、きいた、さいた、しいた、たいた、
        // だいた、ついた、といた、どいた、ないた、ぬいた、のいた、
        // はいた、ひいた、ふいた、ふぶいた、やいた、わいた
        ch2 = L'い';
        ch3 = L'た';
        ch4 = L'て';
        break;

    case GYOU_GA:
        // かいだ、さわいだ、そいだ、みついだ
        ch2 = L'い';
        ch3 = L'だ';
        ch4 = L'で';
        break;

    default:
        // よくわからない
        ch2 = ARRAY_AT_AT(s_hiragana_table, node.gyou, DAN_I);
        ch3 = L'た';
        ch4 = L'て';
        break;
    }

    // 上記の分類に基づいて、五段動詞の連用形の音便を処理する。
    if (ch2 != 0 && tail.size() >= 1 && tail[0] == ch2) {
        if (tail.size() >= 3 && tail[1] == ch4 && tail[2] == L'も') {
            // 連用形「ても」「でも」
            node.katsuyou = RENYOU_KEI;
            node.pre = fields[I_FIELD_PRE] + ch2 + tail[1] + tail[2];
            node.post = fields[I_FIELD_POST] + ch2 + tail[1] + tail[2];
            AddNode(index, node);
        } else if (tail.size() >= 2 && tail[1] == ch4) {
            // 連用形「て」「で」
            node.katsuyou = RENYOU_KEI;
            node.pre = fields[I_FIELD_PRE] + ch2 + tail[1];
            node.post = fields[I_FIELD_POST] + ch2 + tail[1];
            AddNode(index, node);
        } else if (tail.size() >= 3 && tail[1] == ch3 && tail[2] == L'り') {
            // 連用形「たり」「だり」
            node.katsuyou = RENYOU_KEI;
            node.pre = fields[I_FIELD_PRE] + ch2 + tail[1] + tail[2];
            node.post = fields[I_FIELD_POST] + ch2 + tail[1] + tail[2];
            AddNode(index, node);
        } else if (tail.size() >= 2 && tail[1] == ch3) {
            // 終止形「た」「だ」
            node.katsuyou = SHUUSHI_KEI;
            node.pre = fields[I_FIELD_PRE] + ch2 + tail[1];
            node.post = fields[I_FIELD_POST] + ch2 + tail[1];
            AddNode(index, node);
            // 連用形「た」「だ」
            node.katsuyou = RENYOU_KEI;
            node.pre = fields[I_FIELD_PRE] + ch2 + tail[1];
            node.post = fields[I_FIELD_POST] + ch2 + tail[1];
            AddNode(index, node);
        }
    }

    // 五段動詞の終止形。「動く」「聞き取る」
    // 五段動詞の連体形。「動く(とき)」「聞き取る(とき)」
    do {
        WCHAR ch = ARRAY_AT_AT(s_hiragana_table, node.gyou, DAN_U);
        if (tail.size() <= 0 || tail[0] != ch)
            break;

        node.katsuyou = SHUUSHI_KEI;
        node.pre = fields[I_FIELD_PRE] + ch;
        node.post = fields[I_FIELD_POST] + ch;
        AddNode(index, node);

        node.katsuyou = RENTAI_KEI;
        AddNode(index, node);

        // 「動くよ」「動くね」「動くな」「動くぞ」
        if (tail.size() < 2 ||
            (tail[1] != L'よ' && tail[1] != L'ね' && tail[1] != L'な' && tail[1] != L'ぞ'))
                break;
        node.pre += tail[1];
        node.post += tail[1];
        node.katsuyou = SHUUSHI_KEI;
        AddNode(index, node);
    } while (0);

    // 五段動詞の仮定形。「動く」→「動け(ば)」、「聞き取る」→「聞き取れ(ば)」
    // 五段動詞の命令形。
    // 「動く」→「動け」「動けよ」、「聞き取る」→「聞き取れ」「聞き取れよ」
    do {
        WCHAR ch = ARRAY_AT_AT(s_hiragana_table, node.gyou, DAN_E);
        if (tail.size() >= 1) {
            if (tail[0] == ch) {
                node.katsuyou = KATEI_KEI;
                node.pre = fields[I_FIELD_PRE] + ch;
                node.post = fields[I_FIELD_POST] + ch;
                AddNode(index, node);

                node.katsuyou = MEIREI_KEI;
                AddNode(index, node);

                if (tail.size() >= 2 && (tail[1] == L'よ' || tail[1] == L'や')) {
                    node.pre += tail[1];
                    node.post += tail[1];
                    AddNode(index, node);
                }
            }
            // 「くだされ」→「ください」
            // 「なされ」→「なさい」
            std::wstring pre = fields[I_FIELD_PRE];
            if (pre[pre.size() - 1] == L'さ' && ch == L'れ') {
                ch = L'い';
                if (tail[0] == ch) {
                    node.katsuyou = MEIREI_KEI;
                    node.pre = fields[I_FIELD_PRE] + ch;
                    node.post = fields[I_FIELD_POST] + ch;
                    AddNode(index, node);
                }
            }
        }
    } while (0);

    // 五段動詞の命令形。「動く」→「動こう」「動こうよ」、「聞き取る」→「聞き取ろう」「聞き取ろうよ」
    do {
        WCHAR ch = ARRAY_AT_AT(s_hiragana_table, node.gyou, DAN_O);
        if (tail.size() < 2 || tail[0] != ch || tail[1] != L'う')
            break;
        node.katsuyou = MEIREI_KEI;
        node.pre = fields[I_FIELD_PRE] + ch + L'う';
        node.post = fields[I_FIELD_POST] + ch + L'う';
        AddNode(index, node);

        if (tail.size() < 3 ||
            (tail[2] != L'よ' && tail[2] != L'や' && tail[2] != L'な' && tail[2] != L'ね'))
            break;
        node.pre += tail[2];
        node.post += tail[2];
        AddNode(index, node);
    } while (0);

    // 五段動詞の名詞形。
    // 「動く(五段)」→「動き(名詞)」「動き方(名詞)」、
    // 「聞き取る(五段)」→「聞き取り(名詞)」「聞き取り方(名詞)」など。
    node.bunrui = HB_MEISHI;
    WCHAR ch1 = ARRAY_AT_AT(s_hiragana_table, node.gyou, DAN_I);
    do {
        if (tail.empty() || tail[0] != ch1)
            break;
        node.pre = fields[I_FIELD_PRE] + ch1;
        node.post = fields[I_FIELD_POST] + ch1;
        node.deltaCost = deltaCost + 40;
        AddNode(index, node);

        if (tail[1] != L'か' || tail[2] != L'た')
            break;
        node.pre += L"かた";
        node.post += L"方";
        node.deltaCost = deltaCost;
        AddNode(index, node);
    } while (0);

    do {
        if (tail.empty() || tail[0] != ch1)
            break;
        // 「動きやすい」「聞き取りやすい」
        if (tail.size() >= 3 && tail.substr(1, 2) == L"やす") {
            WStrings new_fields = fields;
            new_fields[I_FIELD_PRE] += ch;
            new_fields[I_FIELD_POST] += ch;
            new_fields[I_FIELD_PRE] += L"やす";
            new_fields[I_FIELD_POST] += L"やす";
            DoIkeiyoushi(index, new_fields, deltaCost);

            new_fields = fields;
            new_fields[I_FIELD_PRE] += ch;
            new_fields[I_FIELD_POST] += ch;
            new_fields[I_FIELD_PRE] += L"やす";
            new_fields[I_FIELD_POST] += L"易い";
            DoIkeiyoushi(index, new_fields, deltaCost + 10);
        }

        // 「動きにくい」「聞き取りにくい」
        if (tail.size() >= 3 && tail.substr(1, 2) == L"にく") {
            WStrings new_fields = fields;
            new_fields[I_FIELD_PRE] += ch;
            new_fields[I_FIELD_POST] += ch;
            new_fields[I_FIELD_PRE] += L"にく";
            new_fields[I_FIELD_POST] += L"にく";
            DoIkeiyoushi(index, new_fields, deltaCost);

            new_fields = fields;
            new_fields[I_FIELD_PRE] += ch;
            new_fields[I_FIELD_POST] += ch;
            new_fields[I_FIELD_PRE] += L"にく";
            new_fields[I_FIELD_POST] += L"難";
            DoIkeiyoushi(index, new_fields, deltaCost + 10);
        }

        // 「動きづらい」「聞き取りづらい」
        // 「動きにくい」「聞き取りにくい」
        if (tail.size() >= 3 && tail.substr(1, 2) == L"づら") {
            WStrings new_fields = fields;
            new_fields[I_FIELD_PRE] += ch;
            new_fields[I_FIELD_POST] += ch;
            new_fields[I_FIELD_PRE] += L"づら";
            new_fields[I_FIELD_POST] += L"づら";
            DoIkeiyoushi(index, new_fields, deltaCost);

            new_fields = fields;
            new_fields[I_FIELD_PRE] += ch;
            new_fields[I_FIELD_POST] += ch;
            new_fields[I_FIELD_PRE] += L"づら";
            new_fields[I_FIELD_POST] += L"辛";
            DoIkeiyoushi(index, new_fields, deltaCost + 10);
        }
    } while (0);

    // 「動く(五段)」→「動ける(一段)」、
    // 「聞く(五段)」→「聞ける(一段)」など。
    do {
        WCHAR ch = ARRAY_AT_AT(s_hiragana_table, node.gyou, DAN_E);
        if (tail.empty() || tail[0] != ch)
            break;
        WStrings new_fields = fields;
        new_fields[I_FIELD_PRE] += ch;
        new_fields[I_FIELD_POST] += ch;
        DoIchidanDoushi(index, new_fields, deltaCost + 30);
    } while (0);
} // Lattice::DoGodanDoushi

// 一段動詞を変換する。
void Lattice::DoIchidanDoushi(size_t index, const WStrings& fields, INT deltaCost)
{
    FOOTMARK();
    ASSERT(fields.size() == NUM_FIELDS);
    ASSERT(fields[I_FIELD_PRE].size());
    size_t length = fields[I_FIELD_PRE].size();
    // 区間チェック。
    if (index + length > m_pre.size()) {
        return;
    }
    // 対象のテキストが語幹と一致するか確かめる。
    if (m_pre.substr(index, length) != fields[I_FIELD_PRE]) {
        return;
    }
    // 語幹の後の部分文字列。
    std::wstring tail = m_pre.substr(index + length);

    // ラティスノードの準備。
    LatticeNode node;
    node.bunrui = HB_ICHIDAN_DOUSHI;
    node.tags = fields[I_FIELD_TAGS];
    node.deltaCost = deltaCost;

    // 一段動詞の未然形。「寄せる」→「寄せ(ない/よう)」、「見る」→「見(ない/よう)」
    // 一段動詞の連用形。「寄せる」→「寄せ(ます/た/て)」、「見る」→「見(ます/た/て)」
    do {
        node.pre = fields[I_FIELD_PRE];
        node.post = fields[I_FIELD_POST];
        node.katsuyou = MIZEN_KEI;
        AddNode(index, node);

        node.katsuyou = RENYOU_KEI;
        AddNode(index, node);

        // 「見て」
        if (tail.size() >= 1 && tail[0] == L'て') {
            node.pre += tail[0];
            node.post += tail[0];
            AddNode(index, node);
            // 「見ていた」
            if (tail.size() >= 3 && tail[1] == L'い' && tail[2] == L'た') {
                node.pre += L"いた";
                node.post += L"いた";
                node.katsuyou = SHUUSHI_KEI;
                AddNode(index, node);
                // 「見ていたよ」「見ていたな」「見ていたね」
                if (tail.size() >= 4 && (tail[3] == L'よ' || tail[3] == L'な' || tail[3] == L'ね')) {
                    node.pre += tail[3];
                    node.post += tail[3];
                    node.katsuyou = SHUUSHI_KEI;
                    AddNode(index, node);
                    // 「見ていたよぉ」「見ていたなー」「見ていたねえ」
                    if (tail.size() >= 5 &&
                        (tail[4] == L'ー' ||
                         tail[4] == L'あ' || tail[4] == L'ぁ' ||
                         tail[4] == L'え' || tail[4] == L'ぇ' ||
                         tail[4] == L'お' || tail[4] == L'ぉ'))
                    {
                        node.pre += tail[4];
                        node.post += tail[4];
                        node.katsuyou = SHUUSHI_KEI;
                        AddNode(index, node);
                    }
                }
            }
            // 「見てた」
            if (tail.size() >= 2 && tail[1] == L'た') {
                node.pre = fields[I_FIELD_PRE] + L"てた";
                node.post = fields[I_FIELD_POST] + L"てた";
                node.katsuyou = SHUUSHI_KEI;
                AddNode(index, node);
                // 「見てたよ」「見てたな」「見てたね」
                if (tail.size() >= 3 && (tail[2] == L'よ' || tail[2] == L'な' || tail[2] == L'ね')) {
                    node.pre += tail[2];
                    node.post += tail[2];
                    node.katsuyou = SHUUSHI_KEI;
                    AddNode(index, node);
                    // 「見てたよぉ」「見てたなー」「見てたねえ」
                    if (tail.size() >= 4 &&
                        (tail[3] == L'ー' ||
                         tail[3] == L'あ' || tail[3] == L'ぁ' ||
                         tail[3] == L'え' || tail[3] == L'ぇ' ||
                         tail[3] == L'お' || tail[3] == L'ぉ'))
                    {
                        node.pre += tail[3];
                        node.post += tail[3];
                        node.katsuyou = SHUUSHI_KEI;
                        AddNode(index, node);
                    }
                }
            }
        }
        // 終止形「見ない」
        if (tail.size() >= 2 && tail.substr(0, 2) == L"ない") {
            node.pre = fields[I_FIELD_PRE] + L"ない";
            node.post = fields[I_FIELD_POST] + L"ない";
            node.katsuyou = SHUUSHI_KEI;
            AddNode(index, node);
        }
        // 終止形「見よう」「見ようね」「見ようや」「見ような」「見ようぞ」
        if (tail.size() >= 2 && tail[0] == L'よ' && tail[1] == L'う') {
            node.katsuyou = SHUUSHI_KEI;
            node.pre = fields[I_FIELD_PRE] + L"よう";
            node.post = fields[I_FIELD_POST] + L"よう";
            AddNode(index, node);

            if (tail.size() >= 3 &&
                (tail[2] == L'ね' || tail[2] == L'や' || tail[2] == L'な' || tail[2] == L'ぞ'))
            {
                node.pre += tail[2];
                node.post += tail[2];
                AddNode(index, node);
            }
        }
    } while (0);

    // 「～やすい」「～にくい」「～づらい」で、い形容詞の形。
    if (tail.size() >= 2) {
        // 「寄せやすい」
        if (tail.substr(0, 2) == L"やす") {
            WStrings new_fields = fields;
            new_fields[I_FIELD_PRE] += L"やす";
            new_fields[I_FIELD_POST] += L"やす";
            DoIkeiyoushi(index, new_fields, deltaCost);

            new_fields = fields;
            new_fields[I_FIELD_PRE] += L"やす";
            new_fields[I_FIELD_POST] += L"易";
            DoIkeiyoushi(index, new_fields, deltaCost + 10);
        }
        // 「寄せにくい」
        if (tail.substr(0, 2) == L"にく") {
            WStrings new_fields = fields;
            new_fields[I_FIELD_PRE] += L"にく";
            new_fields[I_FIELD_POST] += L"にく";
            DoIkeiyoushi(index, new_fields, deltaCost);

            new_fields = fields;
            new_fields[I_FIELD_PRE] += L"にく";
            new_fields[I_FIELD_POST] += L"難";
            DoIkeiyoushi(index, new_fields, deltaCost + 10);
        }
        // 「寄せづらい」
        if (tail.substr(0, 2) == L"づら") {
            WStrings new_fields = fields;
            new_fields[I_FIELD_PRE] += L"づら";
            new_fields[I_FIELD_POST] += L"づら";
            DoIkeiyoushi(index, new_fields, deltaCost);

            new_fields = fields;
            new_fields[I_FIELD_PRE] += L"づら";
            new_fields[I_FIELD_POST] += L"辛い";
            DoIkeiyoushi(index, new_fields, deltaCost + 10);
        }
    }

    // 一段動詞の終止形。「寄せる」「見る」
    // 一段動詞の連体形。「寄せる(とき)」「見る(とき)」
    do {
        if (tail.empty() || tail[0] != L'る') break;
        node.pre = fields[I_FIELD_PRE] + L'る';
        node.post = fields[I_FIELD_POST] + L'る';
        node.katsuyou = SHUUSHI_KEI;
        AddNode(index, node);

        node.katsuyou = RENTAI_KEI;
        AddNode(index, node);

        // 「見るよ」「見るね」「見るな」「見るぞ」
        if (tail.size() < 2 ||
            (tail[1] != L'よ' && tail[1] != L'ね' && tail[1] != L'な' && tail[1] != L'ぞ'))
                break;
        node.pre += tail[1];
        node.post += tail[1];
        node.katsuyou = SHUUSHI_KEI;
        AddNode(index, node);
    } while (0);

    // 一段動詞の仮定形。「寄せる」→「寄せれ(ば)」、「見る」→「見れ(ば)」
    do {
        if (tail.empty() || tail[0] != L'れ') break;
        node.katsuyou = KATEI_KEI;
        node.pre = fields[I_FIELD_PRE] + L'れ';
        node.post = fields[I_FIELD_POST] + L'れ';
        AddNode(index, node);
    } while (0);

    // 一段動詞の命令形。
    // 「寄せる」→「寄せろ」「寄せろよ」、「見る」→「見ろ」「見ろよ」
    node.katsuyou = MEIREI_KEI;
    do {
        if (tail.empty() || tail[0] != L'ろ') break;
        node.pre = fields[I_FIELD_PRE] + L'ろ';
        node.post = fields[I_FIELD_POST] + L'ろ';
        AddNode(index, node);

        if (tail.size() < 2 || (tail[1] != L'よ' && tail[1] != L'や')) break;
        node.pre += tail[1];
        node.post += tail[1];
        AddNode(index, node);
    } while (0);
    // 「寄せる」→「寄せよ」、「見る」→「見よ」
    do {
        if (tail.empty() || tail[0] != L'よ') break;
        node.pre = fields[I_FIELD_PRE] + L'よ';
        node.post = fields[I_FIELD_POST] + L'よ';
        AddNode(index, node);
    } while (0);

    // 一段動詞の名詞形。
    // 「寄せる」→「寄せ」「寄せ方」、「見る」→「見」「見方」
    node.bunrui = HB_MEISHI;
    do {
        node.pre = fields[I_FIELD_PRE];
        node.post = fields[I_FIELD_POST];
        AddNode(index, node);

        if (tail.empty() || tail[0] != L'か' || tail[1] != L'た') break;
        node.pre += L"かた";
        node.post += L"方";
        AddNode(index, node);
    } while (0);
} // Lattice::DoIchidanDoushi

// カ変動詞を変換する。
void Lattice::DoKahenDoushi(size_t index, const WStrings& fields, INT deltaCost)
{
    FOOTMARK();
    ASSERT(fields.size() == NUM_FIELDS);
    ASSERT(fields[I_FIELD_PRE].size());
    size_t length = fields[I_FIELD_PRE].size();
    // 区間チェック。
    if (index + length > m_pre.size()) {
        return;
    }
    // 対象のテキストが語幹と一致するか確かめる。
    if (m_pre.substr(index, length) != fields[I_FIELD_PRE]) {
        return;
    }
    // 語幹の後の部分文字列。
    std::wstring tail = m_pre.substr(index + length);

    // ラティスノードの準備。
    LatticeNode node;
    node.bunrui = HB_KAHEN_DOUSHI;
    node.tags = fields[I_FIELD_TAGS];
    node.deltaCost = deltaCost;

    // 「くる」「こ(ない)」「き(ます)」などと、語幹が一致しないので、
    // 実際の辞書では「来い」を登録するなど回避策を施している。

    // 終止形と連用形「～来る」
    do {
        if (tail.size() < 2 || tail[0] != L'く' || tail[1] != L'る') break;
        node.pre = fields[I_FIELD_PRE] + L"くる";
        node.post = fields[I_FIELD_POST] + L"来る";
        node.katsuyou = SHUUSHI_KEI;
        AddNode(index, node);

        node.katsuyou = RENTAI_KEI;
        AddNode(index, node);

        // 「来るよ」「来るね」「来るな」「来るぞ」
        if (tail.size() < 3 ||
            (tail[2] != L'よ' && tail[2] != L'ね' && tail[2] != L'な' && tail[2] != L'ぞ'))
                break;
        node.pre += tail[2];
        node.post += tail[2];
        node.katsuyou = SHUUSHI_KEI;
        AddNode(index, node);
    } while (0);
    do {
        if (fields[I_FIELD_PRE] != L"くる") break;
        node.pre = fields[I_FIELD_PRE];
        node.post = fields[I_FIELD_POST];
        node.katsuyou = SHUUSHI_KEI;
        AddNode(index, node);

        node.katsuyou = RENTAI_KEI;
        AddNode(index, node);

        // 「来るよ」「来るね」「来るな」「来るぞ」
        if (tail.size() < 1 ||
            (tail[0] != L'よ' && tail[0] != L'ね' && tail[0] != L'な' && tail[0] != L'ぞ'))
                break;
        node.pre += tail[0];
        node.post += tail[0];
        node.katsuyou = SHUUSHI_KEI;
        AddNode(index, node);
    } while (0);

    // 命令形「～こい」「～こいよ」「～こいや」
    do {
        if (tail.size() < 2 || tail[0] != L'こ' || tail[1] != L'い') break;
        node.pre = fields[I_FIELD_PRE] + L"こい";
        node.post = fields[I_FIELD_POST] + L"来い";
        node.katsuyou = MEIREI_KEI;
        AddNode(index, node);

        if (tail.size() < 3 || (tail[2] != L'よ' && tail[2] != L'や')) break;
        node.pre += tail[2];
        node.post += tail[2];
        AddNode(index, node);
    } while (0);
    do {
        if (fields[I_FIELD_PRE] != L"こい") break;
        node.pre = fields[I_FIELD_PRE];
        node.post = fields[I_FIELD_POST];
        node.katsuyou = MEIREI_KEI;
        AddNode(index, node);

        if (tail.size() < 1 || (tail[0] != L'よ' && tail[0] != L'や')) break;
        node.pre += tail[0];
        node.post += tail[0];
        AddNode(index, node);
    } while (0);

    // 仮定形「～来れ」
    do {
        if (tail.size() < 2 || tail[0] != L'く' || tail[1] != L'れ') break;
        node.pre = fields[I_FIELD_PRE] + L"くれ";
        node.post = fields[I_FIELD_POST] + L"来れ";
        node.katsuyou = KATEI_KEI;
        AddNode(index, node);
    } while (0);
    do {
        if (fields[I_FIELD_PRE] != L"くれ") break;
        node.pre = fields[I_FIELD_PRE];
        node.post = fields[I_FIELD_POST];
        node.katsuyou = KATEI_KEI;
        AddNode(index, node);
    } while (0);

    // 未然形「～来」（こ）
    do {
        if (tail.size() < 1 || tail[0] != L'こ') break;
        node.pre = fields[I_FIELD_PRE] + L"こ";
        node.post = fields[I_FIELD_POST] + L"来";
        node.katsuyou = MIZEN_KEI;
        AddNode(index, node);
        if (tail.substr(0, 3) != L"こさせ") break;
        node.pre = fields[I_FIELD_PRE] + L"こさせ";
        node.post = fields[I_FIELD_POST] + L"来させ";
        AddNode(index, node);
    } while (0);
    do {
        if (fields[I_FIELD_PRE] != L"こ") break;
        node.pre = fields[I_FIELD_PRE];
        node.post = fields[I_FIELD_POST];
        node.katsuyou = MIZEN_KEI;
        AddNode(index, node);
        if (tail.substr(0, 3) != L"させ") break;
        node.pre = fields[I_FIELD_PRE] + L"させ";
        node.post = fields[I_FIELD_POST] + L"させ";
        AddNode(index, node);
    } while (0);

    // 連用形「～来」（き）
    do {
        if (tail.size() < 1 || tail[0] != L'き') break;
        node.pre = fields[I_FIELD_PRE] + L"き";
        node.post = fields[I_FIELD_POST] + L"来";
        node.katsuyou = RENYOU_KEI;
        AddNode(index, node);
    } while (0);
    do {
        if (fields[I_FIELD_PRE] != L"き") break;
        node.pre = fields[I_FIELD_PRE];
        node.post = fields[I_FIELD_POST];
        node.katsuyou = RENYOU_KEI;
        AddNode(index, node);
    } while (0);
} // Lattice::DoKahenDoushi

// サ変動詞を変換する。
void Lattice::DoSahenDoushi(size_t index, const WStrings& fields, INT deltaCost)
{
    FOOTMARK();
    ASSERT(fields.size() == NUM_FIELDS);
    ASSERT(fields[I_FIELD_PRE].size());
    size_t length = fields[I_FIELD_PRE].size();
    // 区間チェック。
    if (index + length > m_pre.size()) {
        return;
    }
    // 対象のテキストが語幹と一致するか確かめる。
    if (m_pre.substr(index, length) != fields[I_FIELD_PRE]) {
        return;
    }
    // 変換前の語幹。
    std::wstring pre = fields[I_FIELD_PRE];
    // 変換後の語幹。
    std::wstring post = fields[I_FIELD_POST];
    // 語幹の後の部分文字列。
    std::wstring tail = m_pre.substr(index + length);

    // ラティスノードの準備。
    LatticeNode node;
    node.bunrui = HB_SAHEN_DOUSHI;
    node.tags = fields[I_FIELD_TAGS];
    node.pre = pre;
    node.post = post;
    node.deltaCost = deltaCost;

    // カ変動詞と同様に、サ変動詞は語幹が変化するので、「回避策」が必要になる。
    // 回避策として、辞書にそれぞれ異なる語幹を登録している。

    WCHAR lastChar = pre[pre.size() - 1];

    // 未然形「～さ」「～ざ」
    // 未然形「～し」「～じ」
    // 未然形「～せ」「～ぜ」
    node.katsuyou = MIZEN_KEI;
    if (tail.size() >= 1) {
        node.deltaCost += 50;
        if (node.gyou == GYOU_ZA) {
            if (lastChar == L'ざ' || lastChar == L'じ' || lastChar == L'ぜ')
                AddNode(index, node);
        } else {
            if (lastChar == L'さ' || lastChar == L'し' || lastChar == L'せ')
                AddNode(index, node);
        }
        node.deltaCost -= 50;
    }

    // 連用形「～し」「～じ」
    node.katsuyou = RENYOU_KEI;
    if (tail.size() >= 1) {
        node.deltaCost += 50;
        if (node.gyou == GYOU_ZA) {
            if (lastChar == L'じ')
                AddNode(index, node);
        } else {
            if (lastChar == L'し')
                AddNode(index, node);
        }
        node.deltaCost -= 50;
    }

    if (pre.size() >= 2) {
        std::wstring lastTwoChars = pre.substr(pre.size() - 2);
        // 終止形「～する」「～ずる」
        // 連用形「～する(とき)」「～ずる(とき)」
        if (node.gyou == GYOU_ZA) {
            if (lastTwoChars == L"ずる") {
                node.katsuyou = SHUUSHI_KEI;
                AddNode(index, node);

                node.katsuyou = RENYOU_KEI;
                AddNode(index, node);

                // 「ずるな」「ずるよ」
                if (tail.size() && (tail[0] == L'な' || tail[0] == L'よ')) {
                    node.pre = pre + tail[0];
                    node.post = post + tail[0];
                    AddNode(index, node);
                }
                // 「ずるなよ」
                if (tail.size() >= 2 && tail.substr(0, 2) == L"なよ") {
                    node.pre = pre + L"なよ";
                    node.post = post + L"なよ";
                    AddNode(index, node);
                }
            }
        } else {
            if (lastTwoChars == L"する") {
                node.katsuyou = SHUUSHI_KEI;
                AddNode(index, node);

                node.katsuyou = RENYOU_KEI;
                AddNode(index, node);

                // 「するな」「するよ」
                if (tail.size() && (tail[0] == L'な' || tail[0] == L'よ')) {
                    node.pre = pre + tail[0];
                    node.post = post + tail[0];
                    AddNode(index, node);
                }
                // 「するなよ」
                if (tail.size() >= 2 && tail.substr(0, 2) == L"なよ") {
                    node.pre = pre + L"なよ";
                    node.post = post + L"なよ";
                    AddNode(index, node);
                }
            }
        }

        // 仮定形「～すれ(ば)」「～ずれ(ば)」
        node.katsuyou = KATEI_KEI;
        if (node.gyou == GYOU_ZA) {
            if (lastTwoChars == L"ずれ") {
                node.pre = pre;
                node.post = post;
                AddNode(index, node);
            }
        } else {
            if (lastTwoChars == L"すれ") {
                node.pre = pre;
                node.post = post;
                AddNode(index, node);
            }
        }

        // 命令形「～しろ」「～じろ」「せよ」「ぜよ」「せい」「ぜい」
        node.katsuyou = MEIREI_KEI;
        if (node.gyou == GYOU_ZA) {
            if (lastTwoChars == L"じろ" || lastTwoChars == L"ぜよ" || lastTwoChars == L"ぜい") {
                node.pre = pre;
                node.post = post;
                AddNode(index, node);
            }
        } else {
            if (lastTwoChars == L"しろ" || lastTwoChars == L"せよ" || lastTwoChars == L"せい") {
                node.pre = pre;
                node.post = post;
                AddNode(index, node);
            }
        }
    }

    if (pre.size() >= 3) {
        std::wstring lastTriChars = pre.substr(pre.size() - 3);
        if (node.gyou == GYOU_ZA) {
            if (lastTriChars == L"じよう") {
                node.pre = pre;
                node.post = post;
                AddNode(index, node);
            }
        } else {
            if (lastTriChars == L"しよう") {
                node.pre = pre;
                node.post = post;
                AddNode(index, node);
            }
        }
    }
} // Lattice::DoSahenDoushi

void Lattice::DoMeishi(size_t index, const WStrings& fields, INT deltaCost)
{
    FOOTMARK();
    ASSERT(fields.size() == NUM_FIELDS);
    ASSERT(fields[I_FIELD_PRE].size());

    size_t length = fields[I_FIELD_PRE].size();
    // 区間チェック。
    if (index + length > m_pre.size()) {
        return;
    }
    // 対象のテキストが語幹と一致するか確かめる。
    if (m_pre.substr(index, length) != fields[I_FIELD_PRE]) {
        return;
    }
    // 語幹の後の部分文字列。
    std::wstring tail = m_pre.substr(index + length);

    // ラティスノードの準備。
    LatticeNode node;
    node.bunrui = HB_MEISHI;
    node.tags = fields[I_FIELD_TAGS];
    node.deltaCost = deltaCost;

    // 名詞は活用なし。
    if (node.HasTag(L"[動植物]")) {
        // 動植物名は、カタカナでもよい。
        node.pre = fields[I_FIELD_PRE];
        node.post = mz_lcmap(fields[I_FIELD_PRE], LCMAP_KATAKANA | LCMAP_FULLWIDTH);
        AddNode(index, node);

        node.deltaCost += 30;
        node.pre = fields[I_FIELD_PRE];
        node.post = fields[I_FIELD_POST];
        AddNode(index, node);
    } else {
        node.pre = fields[I_FIELD_PRE];
        node.post = fields[I_FIELD_POST];
        AddNode(index, node);
    }

    // 名詞＋「っぽい」でい形容詞に。
    if (tail.size() >= 2 && tail[0] == L'っ' && tail[1] == L'ぽ') {
        WStrings new_fields = fields;
        new_fields[I_FIELD_PRE] += L"っぽ";
        new_fields[I_FIELD_POST] += L"っぽ";
        DoIkeiyoushi(index, new_fields, deltaCost);
    }

    // 名詞＋「みたいな」でな形容詞に。
    if (tail.size() >= 3 && tail[0] == L'み' && tail[1] == L'た' && tail[2] == L'い') {
        WStrings new_fields = fields;
        new_fields[I_FIELD_PRE] += L"みたい";
        new_fields[I_FIELD_POST] += L"みたい";
        DoNakeiyoushi(index, new_fields, deltaCost);
    }
    // 名詞＋「みたい」でい形容詞に。
    if (tail.size() >= 2 && tail[0] == L'み' && tail[1] == L'た') {
        WStrings new_fields = fields;
        new_fields[I_FIELD_PRE] += L"みた";
        new_fields[I_FIELD_POST] += L"みた";
        DoIkeiyoushi(index, new_fields, deltaCost);
    }

    // 名詞＋「さ」、名詞＋「し」、名詞＋「せ」でサ変動詞に
    if (tail.size() >= 1 && (tail[0] == L'さ' || tail[0] == L'し' || tail[0] == L'せ')) {
        WStrings new_fields = fields;
        new_fields[I_FIELD_PRE] += tail[0];
        new_fields[I_FIELD_POST] += tail[0];
        DoSahenDoushi(index, new_fields, deltaCost - 10);
    }

    // 名詞＋「する」、名詞＋「すれ」、名詞＋「せよ」、名詞＋「しろ」、名詞＋「せい」でサ変動詞に。
    if (tail.size() >= 2 &&
        (tail.substr(tail.size() - 2) == L"する" || tail.substr(tail.size() - 2) == L"すれ" ||
         tail.substr(tail.size() - 2) == L"せよ" || tail.substr(tail.size() - 2) == L"しろ" ||
         tail.substr(tail.size() - 2) == L"せい"))
    {
        WStrings new_fields = fields;
        new_fields[I_FIELD_PRE] += tail.substr(tail.size() - 2);
        new_fields[I_FIELD_POST] += tail.substr(tail.size() - 2);
        DoSahenDoushi(index, new_fields, deltaCost - 60);
        // 「するよ」「するな」「せいよ」「せいな」
        if (tail.substr(tail.size() - 2) == L"する" || tail.substr(tail.size() - 2) == L"せい") {
            new_fields[I_FIELD_PRE] += L'よ';
            new_fields[I_FIELD_POST] += L'よ';
            DoSahenDoushi(index, new_fields, deltaCost - 60);
            new_fields = fields;
            new_fields[I_FIELD_PRE] += tail.substr(tail.size() - 2) + L'な';
            new_fields[I_FIELD_POST] += tail.substr(tail.size() - 2) + L'な';
            DoSahenDoushi(index, new_fields, deltaCost - 60);
        }
    }

    // 名詞＋「な」でな形容詞に。
    if (tail.size() >= 1 && tail[0] == L'な') {
        DoNakeiyoushi(index, fields, deltaCost + 80);
    }

    // 名詞＋「たる」「たれ」で五段動詞に。
    if (tail.size() >= 2 && tail[0] == L'た' && (tail[1] == L'る' || tail[1] == L'れ')) {
        WStrings new_fields = fields;
        new_fields[I_FIELD_PRE] += L'た';
        new_fields[I_FIELD_POST] += L'た';
        new_fields[I_FIELD_HINSHI].resize(1);
        new_fields[I_FIELD_HINSHI][0] = MAKEWORD(HB_GODAN_DOUSHI, GYOU_RA);
        DoGodanDoushi(index, new_fields, deltaCost);
    }

    // 名詞＋「でき(る)」でな形容詞に。
    if (tail.size() >= 2 && tail.substr(0, 2) == L"でき") {
        WStrings new_fields = fields;
        new_fields[I_FIELD_PRE] += L"でき";
        new_fields[I_FIELD_POST] += L"でき";
        new_fields[I_FIELD_HINSHI].resize(1);
        new_fields[I_FIELD_HINSHI][0] = MAKEWORD(HB_ICHIDAN_DOUSHI, GYOU_RA);
        DoIchidanDoushi(index, fields, deltaCost - 10);
    }
} // Lattice::DoMeishi

void Lattice::DoFukushi(size_t index, const WStrings& fields, INT deltaCost)
{
    FOOTMARK();
    ASSERT(fields.size() == NUM_FIELDS);
    ASSERT(fields[I_FIELD_PRE].size());

    size_t length = fields[I_FIELD_PRE].size();
    // 区間チェック。
    if (index + length > m_pre.size()) {
        return;
    }
    // 対象のテキストが語幹と一致するか確かめる。
    if (m_pre.substr(index, length) != fields[I_FIELD_PRE]) {
        return;
    }
    // 語幹の後の部分文字列。
    std::wstring tail = m_pre.substr(index + length);

    // ラティスノードの準備。
    LatticeNode node;
    node.bunrui = HB_FUKUSHI;
    node.tags = fields[I_FIELD_TAGS];
    node.deltaCost = deltaCost;

    // 副詞。活用はない。
    node.pre = fields[I_FIELD_PRE];
    node.post = fields[I_FIELD_POST];
    AddNode(index, node);

    // 副詞なら最後に「と」「に」を付けてもいい。
    do {
        if (tail.size() < 1 || (tail[0] != L'と' && tail[0] != L'に')) break;
        node.pre += tail[0];
        node.post += tail[0];
        AddNode(index, node);
    } while (0);

    // 副詞なら最後に「っと」「って」を付けてもいい。
    do {
        if (tail.size() < 2 || tail[0] != L'っ' || (tail[1] != L'と' && tail[1] != L'て')) break;
        node.pre = fields[I_FIELD_PRE] + tail[0] + tail[1];
        node.post = fields[I_FIELD_POST] + tail[0] + tail[1];
        AddNode(index, node);
    } while (0);
}

void Lattice::DoFields(size_t index, const WStrings& fields, INT deltaCost)
{
    ASSERT(fields.size() == NUM_FIELDS);
    if (fields.size() != NUM_FIELDS) {
        DPRINTW(L"%s, %s\n", fields[I_FIELD_PRE].c_str(), fields[I_FIELD_POST].c_str());
        return;
    }
    const size_t length = fields[I_FIELD_PRE].size();
    // 区間チェック。
    if (index + length > m_pre.size()) {
        return;
    }
    // 対象のテキストが語幹と一致するか確かめる。
    if (m_pre.substr(index, length) != fields[I_FIELD_PRE]) {
        return;
    }
    DPRINTW(L"DoFields: %s\n", fields[I_FIELD_PRE].c_str());

    // ラティスノードの準備。
    LatticeNode node;
    node.pre = fields[I_FIELD_PRE];
    node.post = fields[I_FIELD_POST];
    WORD w = fields[I_FIELD_HINSHI][0];
    node.bunrui = (HinshiBunrui)LOBYTE(w);
    node.gyou = (Gyou)HIBYTE(w);
    node.tags = fields[I_FIELD_TAGS];
    node.deltaCost = deltaCost;

    // 品詞分類で場合分けする。
    switch (node.bunrui) {
    case HB_MEISHI:
        DoMeishi(index, fields, deltaCost);
        break;
    case HB_PERIOD: case HB_COMMA: case HB_SYMBOL:
    case HB_RENTAISHI: 
    case HB_SETSUZOKUSHI: case HB_KANDOUSHI:
    case HB_KAKU_JOSHI: case HB_SETSUZOKU_JOSHI:
    case HB_FUKU_JOSHI: case HB_SHUU_JOSHI:
    case HB_KANGO: case HB_SETTOUJI: case HB_SETSUBIJI:
        AddNode(index, node);
        break;
    case HB_FUKUSHI:
        DoFukushi(index, fields, deltaCost);
        break;
    case HB_IKEIYOUSHI: // い形容詞。
        DoIkeiyoushi(index, fields, deltaCost);
        break;
    case HB_NAKEIYOUSHI: // な形容詞。
        DoNakeiyoushi(index, fields, deltaCost);
        break;
    case HB_MIZEN_JODOUSHI: // 未然助動詞。
        node.bunrui = HB_JODOUSHI;
        node.katsuyou = MIZEN_KEI;
        AddNode(index, node);
        break;
    case HB_RENYOU_JODOUSHI: // 連用助動詞。
        node.bunrui = HB_JODOUSHI;
        node.katsuyou = RENYOU_KEI;
        AddNode(index, node);
        break;
    case HB_SHUUSHI_JODOUSHI: // 終止助動詞。
        node.bunrui = HB_JODOUSHI;
        node.katsuyou = SHUUSHI_KEI;
        AddNode(index, node);
        break;
    case HB_RENTAI_JODOUSHI: // 連体助動詞。
        node.bunrui = HB_JODOUSHI;
        node.katsuyou = RENTAI_KEI;
        AddNode(index, node);
        break;
    case HB_KATEI_JODOUSHI: // 仮定助動詞。
        node.bunrui = HB_JODOUSHI;
        node.katsuyou = KATEI_KEI;
        AddNode(index, node);
        break;
    case HB_MEIREI_JODOUSHI: // 命令助動詞。
        node.bunrui = HB_JODOUSHI;
        node.katsuyou = MEIREI_KEI;
        AddNode(index, node);
        break;
    case HB_GODAN_DOUSHI: // 五段動詞。
        DoGodanDoushi(index, fields, deltaCost);
        break;
    case HB_ICHIDAN_DOUSHI: // 一段動詞。
        DoIchidanDoushi(index, fields, deltaCost);
        break;
    case HB_KAHEN_DOUSHI: // カ変動詞。
        DoKahenDoushi(index, fields, deltaCost);
        break;
    case HB_SAHEN_DOUSHI: // サ変動詞。
        DoSahenDoushi(index, fields, deltaCost);
        break;
    default:
        break;
    }
} // Lattice::DoFields

// ラティスをダンプする。
void Lattice::Dump(int num)
{
    const size_t length = m_pre.size();
    DPRINTW(L"### Lattice::Dump(%d) ###\n", num);
    DPRINTW(L"Lattice length: %d\n", int(length));
    for (size_t i = 0; i < length; ++i) {
        DPRINTW(L"Lattice chunk #%d:", int(i));
        for (size_t k = 0; k < ARRAY_AT(m_chunks, i).size(); ++k) {
            DPRINTW(L" %s(%s)", ARRAY_AT_AT(m_chunks, i, k)->post.c_str(),
                        mz_bunrui_to_string(ARRAY_AT_AT(m_chunks, i, k)->bunrui));
        }
        DPRINTW(L"\n");
    }
} // Lattice::Dump

//////////////////////////////////////////////////////////////////////////////

// 逆向きブランチ群を追加する。
void Lattice::MakeReverseBranches(LatticeNode *ptr0)
{
    ASSERT(ptr0);

    if (!ptr0->linked || ptr0->bunrui == HB_TAIL)
        return;

    size_t i = 0;
    branches_t::iterator it, end = ptr0->branches.end();
    for (it = ptr0->branches.begin(); it != end; ++it) {
        LatticeNodePtr& ptr1 = *it;
        if (ptr1->reverse_branches.count(ptr0) == 0) {
            ptr1->reverse_branches.insert(ptr0);
            MakeReverseBranches(ptr1.get());
        }
        ++i;
    }
} // Lattice::MakeReverseBranches

// 複数文節変換において、ラティスを作成する。
BOOL Lattice::AddNodesForMulti(const std::wstring& pre)
{
    DPRINTW(L"%s\n", pre.c_str());

    // ラティスを初期化。
    ASSERT(pre.size() != 0);
    m_pre = pre; // 変換前の文字列。
    m_chunks.resize(pre.size() + 1);

    WCHAR *dict_data1 = g_basic_dict.Lock(); // 基本辞書をロック。
    if (dict_data1) {
        // ノード群を追加。
        AddNodesFromDict(0, dict_data1);

        g_basic_dict.Unlock(dict_data1); // 基本辞書のロックを解除。
    }

    WCHAR *dict_data2 = g_name_dict.Lock(); // 人名・地名辞書をロック。
    if (dict_data2) {
        // ノード群を追加。
        AddNodesFromDict(0, dict_data2);

        g_name_dict.Unlock(dict_data2); // 人名・地名辞書のロックを解除。
    }

    return TRUE;
} // Lattice::AddNodesForMulti

// 単一文節変換において、ラティスを作成する。
BOOL Lattice::AddNodesForSingle(const std::wstring& pre)
{
    DPRINTW(L"%s\n", pre.c_str());

    // ラティスを初期化。
    ASSERT(pre.size() != 0);
    m_pre = pre;
    m_chunks.resize(pre.size() + 1);

    BOOL bOK = TRUE;

    WCHAR *dict_data1 = g_basic_dict.Lock(); // 基本辞書をロックする。
    if (dict_data1) {
        // ノード群を追加。
        if (!AddNodesFromDict(dict_data1)) {
            AddComplement(0, pre.size(), pre.size());
        }

        g_basic_dict.Unlock(dict_data1); // 基本辞書のロックを解除。
    } else {
        bOK = FALSE;
    }

    WCHAR *dict_data2 = g_name_dict.Lock(); // 人名・地名辞書をロックする。
    if (dict_data2) {
        // ノード群を追加。
        if (!AddNodesFromDict(dict_data2)) {
            AddComplement(0, pre.size(), pre.size());
        }

        g_name_dict.Unlock(dict_data2); // 人名・地名辞書のロックを解除。
    } else {
        bOK = FALSE;
    }

    if (bOK)
        return TRUE;

    // ダンプ。
    Dump(4);
    return FALSE; // 失敗。
} // Lattice::AddNodesForSingle

// 複数文節変換において、変換結果を生成する。
void MzIme::MakeResultForMulti(MzConvResult& result, Lattice& lattice)
{
    DPRINTW(L"%s\n", lattice.m_pre.c_str());
    result.clear(); // 結果をクリア。

    LatticeNode* ptr0 = lattice.m_head.get();
    while (ptr0 && ptr0 != lattice.m_tail.get()) {
        LatticeNode* target = NULL;
        {
            branches_t::iterator it, end = ptr0->branches.end();
            for (it = ptr0->branches.begin(); it != end; ++it) {
                LatticeNodePtr& ptr1 = *it;
                if (lattice.OptimizeMarking(ptr1.get())) {
                    target = ptr1.get();
                    break;
                }
            }
        }

        if (!target || target->bunrui == HB_TAIL)
            break;

        {
            branches_t::iterator it, end = ptr0->branches.end();
            for (it = ptr0->branches.begin(); it != end; ++it) {
                LatticeNodePtr& ptr1 = *it;
                if (ptr1.get() != target) {
                    ptr1->marked = FALSE;
                }
            }
        }

        MzConvClause clause;
        clause.add(target);

        {
            branches_t::iterator it, end = ptr0->branches.end();
            for (it = ptr0->branches.begin(); it != end; ++it) {
                LatticeNodePtr& ptr1 = *it;
                if (target->pre.size() == ptr1->pre.size()) {
                    if (target != ptr1.get()) {
                        clause.add(ptr1.get());
                    }
                }
            }
        }

        LatticeNode node;
        node.bunrui = HB_UNKNOWN;
        node.deltaCost = 3000;
        node.pre = mz_lcmap(target->pre, LCMAP_HIRAGANA | LCMAP_FULLWIDTH);

        node.post = mz_lcmap(node.pre, LCMAP_HIRAGANA | LCMAP_FULLWIDTH);
        clause.add(&node);

        node.post = mz_lcmap(node.pre, LCMAP_KATAKANA | LCMAP_FULLWIDTH);
        clause.add(&node);

        node.post = mz_lcmap(node.pre, LCMAP_KATAKANA | LCMAP_HALFWIDTH);
        clause.add(&node);

        node.post = mz_lcmap(node.pre, LCMAP_LOWERCASE | LCMAP_FULLWIDTH);
        clause.add(&node);

        node.post = mz_lcmap(node.pre, LCMAP_UPPERCASE | LCMAP_FULLWIDTH);
        clause.add(&node);

        node.post = node.post[0] + mz_lcmap(node.pre.substr(1), LCMAP_LOWERCASE | LCMAP_FULLWIDTH);
        clause.add(&node);

        node.post = mz_lcmap(node.pre, LCMAP_LOWERCASE | LCMAP_HALFWIDTH);
        clause.add(&node);

        node.post = mz_lcmap(node.pre, LCMAP_UPPERCASE | LCMAP_HALFWIDTH);
        clause.add(&node);

        node.post = node.post[0] + mz_lcmap(node.pre.substr(1), LCMAP_LOWERCASE | LCMAP_HALFWIDTH);
        clause.add(&node);

        result.clauses.push_back(clause);
        ptr0 = target;
    }

    // コストによりソートする。
    result.sort();
} // MzIme::MakeResultForMulti

// 変換に失敗したときの結果を作成する。
void MzIme::MakeResultOnFailure(MzConvResult& result, const std::wstring& pre)
{
    DPRINTW(L"%s\n", pre.c_str());
    MzConvClause clause; // 文節。
    result.clear(); // 結果をクリア。

    // ノードを初期化。
    LatticeNode node;
    node.pre = pre; // 変換前の文字列。
    node.deltaCost = 40; // コストは人名・地名よりも高くする。
    node.bunrui = HB_MEISHI; // 名詞。

    // 文節に無変換文字列を追加。
    node.post = pre; // 変換後の文字列。
    clause.add(&node);

    // 文節にひらがなを追加。
    node.post = mz_lcmap(pre, LCMAP_HIRAGANA); // 変換後の文字列。
    clause.add(&node);

    // 文節にカタカナを追加。
    node.post = mz_lcmap(pre, LCMAP_KATAKANA); // 変換後の文字列。
    clause.add(&node);

    // 文節に全角を追加。
    node.post = mz_lcmap(pre, LCMAP_FULLWIDTH); // 変換後の文字列。
    clause.add(&node);

    // 文節に半角を追加。
    node.post = mz_lcmap(pre, LCMAP_HALFWIDTH | LCMAP_KATAKANA); // 変換後の文字列。
    clause.add(&node);

    // 結果に文節を追加。
    result.clauses.push_back(clause);
} // MzIme::MakeResultOnFailure

// 単一文節変換の結果を作成する。
void MzIme::MakeResultForSingle(MzConvResult& result, Lattice& lattice)
{
    DPRINTW(L"%s\n", lattice.m_pre.c_str());
    result.clear(); // 結果をクリア。
    const size_t length = lattice.m_pre.size();

    // add other candidates
    MzConvClause clause;
    ASSERT(lattice.m_chunks.size());
    const LatticeChunk& chunk = ARRAY_AT(lattice.m_chunks, 0);
    for (size_t i = 0; i < chunk.size(); ++i) {
        if (ARRAY_AT(chunk, i)->pre.size() == length) {
            // add a candidate of same size
            clause.add(ARRAY_AT(chunk, i).get());
        }
    }

    // ノードを初期化する。
    std::wstring pre = lattice.m_pre; // 変換前の文字列。
    LatticeNode node;
    node.pre = pre;
    node.bunrui = HB_UNKNOWN;
    node.deltaCost = 40; // コストは人名・地名よりも高くする。

    // 文節に無変換文字列を追加。
    node.post = pre; // 変換後の文字列。
    clause.add(&node);

    // 文節にひらがなを追加。
    node.post = mz_lcmap(pre, LCMAP_HIRAGANA); // 変換後の文字列。
    clause.add(&node);

    // 文節にカタカナを追加。
    node.post = mz_lcmap(pre, LCMAP_KATAKANA); // 変換後の文字列。
    clause.add(&node);

    // 文節に全角を追加。
    node.post = mz_lcmap(pre, LCMAP_FULLWIDTH); // 変換後の文字列。
    clause.add(&node);

    // 文節に半角を追加。
    node.post = mz_lcmap(pre, LCMAP_HALFWIDTH | LCMAP_KATAKANA); // 変換後の文字列。
    clause.add(&node);

    // 結果に文節を追加。
    result.clauses.push_back(clause);
    ASSERT(ARRAY_AT(result.clauses, 0).candidates.size());

    // コストによりソートする。
    result.sort();
    ASSERT(ARRAY_AT(result.clauses, 0).candidates.size());
} // MzIme::MakeResultForSingle

// 複数文節を変換する。
BOOL MzIme::ConvertMultiClause(LogCompStr& comp, LogCandInfo& cand, BOOL bRoman)
{
    MzConvResult result;
    std::wstring str = ARRAY_AT(comp.extra.hiragana_clauses, comp.extra.iClause);
    if (!ConvertMultiClause(str, result)) {
        return FALSE;
    }
    return StoreResult(result, comp, cand);
} // MzIme::ConvertMultiClause

// Graphvizの候補テキストを取得する。
std::string GetGraphvizCandText(const MzConvCandidate* cand, BOOL bFirst)
{
    if (!cand) {
        if (bFirst)
            return "HEAD";
        else
            return "TAIL";
    }
    static CHAR sz[MAX_PATH];
    ::WideCharToMultiByte(CP_UTF8, 0, cand->post.c_str(), -1, sz, _countof(sz), NULL, NULL);
    sz[_countof(sz) - 1] = 0;
    return sz;
}

// Graphvizのエッジを出力する。
void OutputGraphvizEdge(FILE* fout, const MzConvCandidate *cand0, const MzConvCandidate *cand1)
{
    std::string str1 = GetGraphvizCandText(cand0, TRUE);
    std::string str2 = GetGraphvizCandText(cand1, FALSE);
    if (!cand0)
        cand0 = (MzConvCandidate*)0xDEAD;
    if (!cand1)
        cand1 = (MzConvCandidate*)0xFACE;
    fprintf(fout, "L%p [label=\"%s\"];\n", cand0, str1.c_str());
    fprintf(fout, "L%p [label=\"%s\"];\n", cand1, str2.c_str());
    fprintf(fout, "L%p -> L%p;\n", cand0, cand1);
}

// Graphvizでグラフ構造を表示する。
void ShowGraphviz(const MzConvResult& result)
{
    if (!mz_find_graphviz()[0])
        return;

    TCHAR path0[MAX_PATH], path1[MAX_PATH];
    ExpandEnvironmentStrings(TEXT("%TEMP%\\graph.dot"), path0, _countof(path0));
    ExpandEnvironmentStrings(TEXT("%TEMP%\\graph.png"), path1, _countof(path1));
    ::DeleteFile(path1);

    if (FILE *fout = _tfopen(path0, _T("wb"))) {
        fprintf(fout, "digraph test {\n");
        fprintf(fout, "  graph [fontname=\"MS UI Gothic\"];\n");
        fprintf(fout, "  node [fontname=\"MS UI Gothic\"];\n");
        fprintf(fout, "  edge [fontname=\"MS UI Gothic\"];\n");

        size_t i = 0;
        {
            clauses_t::const_iterator it0, end0 = result.clauses.end();
            for (it0 = result.clauses.begin(); it0 != end0; ++it0) {
                const MzConvClause& clause = *it0;

                candidates_t::const_iterator it1, end1 = clause.candidates.end();
                for (it1 = clause.candidates.begin(); it1 != end1; ++it1) {
                    const MzConvCandidate& cand1 = *it1;
                    if (i == 0) {
                        OutputGraphvizEdge(fout, NULL, &cand1);
                    } else {
                        candidates_t::const_iterator it2, end2 = result.clauses[i - 1].candidates.end();
                        for (it2 = result.clauses[i - 1].candidates.begin(); it2 != end2; ++it2) {
                            const MzConvCandidate& cand0 = *it2;
                            OutputGraphvizEdge(fout, &cand0, &cand1);
                        }
                    }
                }
                ++i;
            }
        }

        {
            candidates_t::const_iterator it, end = result.clauses[i - 1].candidates.end();
            for (it = result.clauses[i - 1].candidates.begin(); it != end; ++it) {
                const MzConvCandidate& cand = *it;
                OutputGraphvizEdge(fout, &cand, NULL);
            }
        }

        fprintf(fout, "}\n");
        fclose(fout);

        TCHAR cmdline[MAX_PATH * 2];
        StringCchPrintf(cmdline, _countof(cmdline), TEXT("-Tpng \"%s\" -o \"%s\""), path0, path1);
        ::ShellExecute(NULL, NULL, mz_find_graphviz(), cmdline, NULL, SW_SHOWNORMAL);
        for (INT i = 0; i < 20; ++i) {
            ::Sleep(100);
            if (PathFileExists(path1))
                break;
        }
        ::Sleep(100);
        ::ShellExecute(NULL, NULL, path1, NULL, NULL, SW_SHOWNORMAL);
    }

    ::DeleteFile(path0);
}

// 複数文節を変換する。
BOOL MzIme::ConvertMultiClause(const std::wstring& str, MzConvResult& result, BOOL show_graphviz)
{
    DPRINTW(L"%s\n", str.c_str());

    // 変換前文字列をひらがな全角で取得。
    std::wstring pre = mz_lcmap(str, LCMAP_FULLWIDTH | LCMAP_HIRAGANA);

    // ラティスを作成し、結果を作成する。
    Lattice lattice;
    lattice.AddNodesForMulti(pre);
    lattice.UpdateLinksAndBranches();
    lattice.CutUnlinkedNodes();
    lattice.AddComplement();
    lattice.MakeReverseBranches(lattice.m_head.get());

    lattice.m_tail->marked = 1;
    lattice.CalcSubTotalCosts(lattice.m_tail.get());

    lattice.m_head->marked = 1;
    lattice.OptimizeMarking(lattice.m_head.get());

    MakeResultForMulti(result, lattice);

    if (result.clauses.empty()) {
        MakeResultOnFailure(result, pre);
    }

    if (show_graphviz)
        ShowGraphviz(result);

    return TRUE;
} // MzIme::ConvertMultiClause

// 単一文節を変換する。
BOOL MzIme::ConvertSingleClause(LogCompStr& comp, LogCandInfo& cand, BOOL bRoman)
{
    DWORD iClause = comp.extra.iClause; // 現在の文節。

    // 変換する。
    MzConvResult result;
    std::wstring str = ARRAY_AT(comp.extra.hiragana_clauses, iClause);
    if (!ConvertSingleClause(str, result)) {
        return FALSE;
    }

    // 未確定文字列をセット。
    result.clauses.resize(1);
    MzConvClause& clause = ARRAY_AT(result.clauses, 0);
    comp.SetClauseCompString(iClause, ARRAY_AT(clause.candidates, 0).post);
    comp.SetClauseCompHiragana(iClause, ARRAY_AT(clause.candidates, 0).pre, bRoman);

    // 候補リストをセットする。
    LogCandList cand_list;
    {
        candidates_t::iterator it, end = clause.candidates.end();
        for (it = clause.candidates.begin(); it != end; ++it) {
            MzConvCandidate& cand2 = *it;
            cand_list.cand_strs.push_back(cand2.post);
        }
    }
    ARRAY_AT(cand.cand_lists, iClause) = cand_list;

    // 現在の文節をセットする。
    cand.iClause = iClause;
    comp.extra.iClause = iClause;

    return TRUE;
} // MzIme::ConvertSingleClause

// 単一文節を変換する。
BOOL MzIme::ConvertSingleClause(const std::wstring& str, MzConvResult& result)
{
    DPRINTW(L"%s\n", str.c_str());
    result.clear(); // 結果をクリア。

    // 変換前文字列をひらがな全角で取得。
    std::wstring pre = mz_lcmap(str, LCMAP_FULLWIDTH | LCMAP_HIRAGANA);

    // ラティスを作成する。
    Lattice lattice;
    lattice.AddNodesForSingle(pre);
    lattice.AddExtraNodes();

    // 結果を作成する。
    MakeResultForSingle(result, lattice);

    return TRUE;
} // MzIme::ConvertSingleClause

// 文節を左に伸縮する。
BOOL MzIme::StretchClauseLeft(LogCompStr& comp, LogCandInfo& cand, BOOL bRoman)
{
    DWORD iClause = comp.extra.iClause; // 現在の文節の位置。

    // 現在の文節を取得する。
    std::wstring str1 = comp.extra.hiragana_clauses[iClause];
    // 一文字以下の長さなら左に拡張できない。
    if (str1.size() <= 1)
        return FALSE;

    // この文節の最後の文字。
    WCHAR ch = str1[str1.size() - 1];
    // この文節を１文字縮小する。
    str1.resize(str1.size() - 1);

    // その文字を次の文節の先頭に追加する。
    std::wstring str2;
    BOOL bSplitted = FALSE; // 分離したか？
    if (iClause + 1 < comp.GetClauseCount()) {
        str2 = ch + comp.extra.hiragana_clauses[iClause + 1];
    } else {
        str2 += ch;
        bSplitted = TRUE; // 分離した。
    }

    // ２つの文節を単一文節変換する。
    MzConvResult result1, result2;
    if (!ConvertSingleClause(str1, result1)) {
        return FALSE;
    }
    if (!ConvertSingleClause(str2, result2)) {
        return FALSE;
    }

    // 文節が分離したら、新しい文節を挿入する。
    if (bSplitted) {
        std::wstring str;
        comp.extra.hiragana_clauses.insert(comp.extra.hiragana_clauses.begin() + iClause + 1, str);
        comp.extra.comp_str_clauses.insert(comp.extra.comp_str_clauses.begin() + iClause + 1, str);
    }

    // 未確定文字列をセット。
    MzConvClause& clause1 = result1.clauses[0];
    MzConvClause& clause2 = result2.clauses[0];
    comp.extra.hiragana_clauses[iClause] = str1;
    comp.extra.comp_str_clauses[iClause] = clause1.candidates[0].post;
    comp.extra.hiragana_clauses[iClause + 1] = str2;
    comp.extra.comp_str_clauses[iClause + 1] = clause2.candidates[0].post;

    // 余剰情報から未確定文字列を更新する。
    comp.UpdateFromExtra(bRoman);

    // 候補リストをセットする。
    {
        LogCandList cand_list;
        candidates_t::iterator it, end = clause1.candidates.end();
        for (it = clause1.candidates.begin(); it != end; ++it) {
            MzConvCandidate& cand1 = *it;
            cand_list.cand_strs.push_back(cand1.post);
        }
        cand.cand_lists[iClause] = cand_list;
    }
    {
        LogCandList cand_list;
        {
            candidates_t::iterator it, end = clause2.candidates.end();
            for (it = clause2.candidates.begin(); it != end; ++it) {
                MzConvCandidate& cand2 = *it;
                cand_list.cand_strs.push_back(cand2.post);
            }
        }
        if (bSplitted) {
            cand.cand_lists.push_back(cand_list);
        } else {
            cand.cand_lists[iClause + 1] = cand_list;
        }
    }

    // 現在の文節をセットする。
    cand.iClause = iClause;
    comp.extra.iClause = iClause;

    // 文節属性をセットする。
    comp.SetClauseAttr(iClause, ATTR_TARGET_CONVERTED);

    return TRUE;
} // MzIme::StretchClauseLeft

// 文節を右に伸縮する。
BOOL MzIme::StretchClauseRight(LogCompStr& comp, LogCandInfo& cand, BOOL bRoman)
{
    DWORD iClause = comp.extra.iClause; // 現在の文節の位置。

    // 現在の文節を取得する。
    std::wstring str1 = comp.extra.hiragana_clauses[iClause];

    // 右端であれば右には拡張できない。
    if (iClause == comp.GetClauseCount() - 1)
        return FALSE;

    // 次の文節を取得する。
    std::wstring str2 = comp.extra.hiragana_clauses[iClause + 1];
    // 次の文節が空ならば、右には拡張できない。
    if (str2.empty())
        return FALSE;

    // str2の最初の文字。
    WCHAR ch = str2[0];

    // それをstr1の末尾に引っ越しする。
    str1 += ch;
    if (str2.size() == 1) {
        str2.clear();
    } else {
        str2 = str2.substr(1);
    }

    // 関係する文節を単一文節変換。
    MzConvResult result1, result2;
    if (!ConvertSingleClause(str1, result1)) {
        return FALSE;
    }
    if (str2.size() && !ConvertSingleClause(str2, result2)) {
        return FALSE;
    }

    // 現在の文節。
    MzConvClause& clause1 = result1.clauses[0];

    if (str2.empty()) { // 次の文節が空になったか？
        // 次の文節を削除する。
        comp.extra.hiragana_clauses.erase(comp.extra.hiragana_clauses.begin() + iClause + 1);
        comp.extra.comp_str_clauses.erase(comp.extra.comp_str_clauses.begin() + iClause + 1);
        comp.extra.hiragana_clauses[iClause] = str1;
        comp.extra.comp_str_clauses[iClause] = clause1.candidates[0].post;
    } else {
        // ２つの文節情報をセットする。
        MzConvClause& clause2 = result2.clauses[0];
        comp.extra.hiragana_clauses[iClause] = str1;
        comp.extra.comp_str_clauses[iClause] = clause1.candidates[0].post;
        comp.extra.hiragana_clauses[iClause + 1] = str2;
        comp.extra.comp_str_clauses[iClause + 1] = clause2.candidates[0].post;
    }

    // 余剰情報から未確定文字列を更新する。
    comp.UpdateFromExtra(bRoman);

    // 候補リストをセットする。
    {
        LogCandList cand_list;
        candidates_t::iterator it, end = clause1.candidates.end();
        for (it = clause1.candidates.begin(); it != end; ++it) {
            MzConvCandidate& cand1 = *it;
            cand_list.cand_strs.push_back(cand1.post);
        }
        cand.cand_lists[iClause] = cand_list;
    }
    if (str2.size()) {
        MzConvClause& clause2 = result2.clauses[0];
        LogCandList cand_list;
        candidates_t::iterator it, end = clause2.candidates.end();
        for (it = clause2.candidates.begin(); it != end; ++it) {
            MzConvCandidate& cand2 = *it;
            cand_list.cand_strs.push_back(cand2.post);
        }
        cand.cand_lists[iClause + 1] = cand_list;
    }

    // 現在の文節をセットする。
    cand.iClause = iClause;
    comp.extra.iClause = iClause;

    // 文節属性をセットする。
    comp.SetClauseAttr(iClause, ATTR_TARGET_CONVERTED);

    return TRUE;
} // MzIme::StretchClauseRight

// Shift_JISのマルチバイト文字の1バイト目か？
inline bool is_sjis_lead(BYTE ch)
{
    return (((0x81 <= ch) && (ch <= 0x9F)) || ((0xE0 <= ch) && (ch <= 0xEF)));
}

// Shift_JISのマルチバイト文字の2バイト目か？
inline bool is_sjis_trail(BYTE ch)
{
    return (((0x40 <= ch) && (ch <= 0x7E)) || ((0x80 <= ch) && (ch <= 0xFC)));
}

// JISバイトか？
inline bool is_jis_byte(BYTE ch)
{
    return ((0x21 <= ch) && (ch <= 0x7E));
}

// JISコードか？
inline bool is_jis_code(WORD w)
{
    BYTE ch0 = BYTE(w >> 8);
    BYTE ch1 = BYTE(w);
    return (is_jis_byte(ch0) && is_jis_byte(ch1));
}

// JISコードをShift_JISに変換する。
inline WORD jis2sjis(BYTE c0, BYTE c1)
{
    if (c0 & 0x01) {
        c0 >>= 1;
        if (c0 < 0x2F) {
            c0 += 0x71;
        } else {
            c0 -= 0x4F;
        }
        if (c1 > 0x5F) {
            c1 += 0x20;
        } else {
            c1 += 0x1F;
        }
    } else {
        c0 >>= 1;
        if (c0 < 0x2F) {
            c0 += 0x70;
        } else {
            c0 -= 0x50;
        }
        c1 += 0x7E;
    }
    WORD sjis_code = WORD((c0 << 8) | c1);
    return sjis_code;
} // jis2sjis

// JISコードをShift_JISコードに変換する。
inline WORD jis2sjis(WORD jis_code)
{
    BYTE c0 = BYTE(jis_code >> 8);
    BYTE c1 = BYTE(jis_code);
    return jis2sjis(c0, c1);
}

// Shift_JISコードか？
inline bool is_sjis_code(WORD w)
{
    return is_sjis_lead(BYTE(w >> 8)) && is_sjis_trail(BYTE(w));
}

// 区点からJISコードに変換。
inline WORD kuten_to_jis(const std::wstring& str)
{
    if (str.size() != 5) return 0; // 五文字でなければ区点コードではない。
    std::wstring ku_bangou = str.substr(0, 3); // 区番号。
    std::wstring ten_bangou = str.substr(3, 2); // 点番号。
    WORD ku = WORD(wcstoul(ku_bangou.c_str(), NULL, 10)); // 区番号を10進数として解釈。
    WORD ten = WORD(wcstoul(ten_bangou.c_str(), NULL, 10)); // 点番号を10進数として解釈。
    WORD jis_code = (ku + 32) * 256 + ten + 32; // 区と点によりJISコードを計算。
    return jis_code;
}

// コード変換。
BOOL MzIme::ConvertCode(const std::wstring& strTyping, MzConvResult& result)
{
    // ノードを初期化。
    LatticeNode node;
    node.pre = strTyping;
    node.bunrui = HB_UNKNOWN;

    // 16進を読み込み。
    ULONG hex_code = wcstoul(strTyping.c_str(), NULL, 16);

    // 文節情報。
    MzConvClause clause;

    // Unicodeのノードを文節に追加。
    WCHAR szUnicode[2];
    szUnicode[0] = WCHAR(hex_code);
    szUnicode[1] = 0;
    node.post = szUnicode; // 変換後の文字列。
    clause.add(&node);
    node.deltaCost++; // コストを１つ加算。

    // Shift_JISコードのノードを文節に追加。
    CHAR szSJIS[8];
    WORD wSJIS = WORD(hex_code);
    if (is_sjis_code(wSJIS)) {
        szSJIS[0] = HIBYTE(wSJIS);
        szSJIS[1] = LOBYTE(wSJIS);
        szSJIS[2] = 0;
        ::MultiByteToWideChar(CODEPAGE_SJIS_932, 0, szSJIS, -1, szUnicode, 2);
        node.post = szUnicode; // 変換後の文字列。
        node.deltaCost++; // コストを１つ加算。
        clause.add(&node);
    }

    // JISコードのノードを文節に追加。
    if (is_jis_code(WORD(hex_code))) {
        wSJIS = jis2sjis(WORD(hex_code));
        if (is_sjis_code(wSJIS)) {
            szSJIS[0] = HIBYTE(wSJIS);
            szSJIS[1] = LOBYTE(wSJIS);
            szSJIS[2] = 0;
            ::MultiByteToWideChar(CODEPAGE_SJIS_932, 0, szSJIS, -1, szUnicode, 2);
            node.post = szUnicode; // 変換後の文字列。
            node.deltaCost++; // コストを１つ加算。
            clause.add(&node);
        }
    }

    // 区点コードのノードを文節に追加。
    WORD wJIS = kuten_to_jis(strTyping);
    if (is_jis_code(wJIS)) {
        wSJIS = jis2sjis(wJIS);
        if (is_sjis_code(wSJIS)) {
            szSJIS[0] = HIBYTE(wSJIS);
            szSJIS[1] = LOBYTE(wSJIS);
            szSJIS[2] = 0;
            ::MultiByteToWideChar(CODEPAGE_SJIS_932, 0, szSJIS, -1, szUnicode, 2);
            node.post = szUnicode; // 変換後の文字列。
            node.deltaCost++; // コストを１つ加算。
            clause.add(&node);
        }
    }

    // 元の入力文字列のノードを文節に追加。
    node.post = strTyping; // 変換後の文字列。
    node.deltaCost++; // コストを１つ加算。
    clause.add(&node);

    // 結果に文節情報をセット。
    result.clauses.clear();
    result.clauses.push_back(clause);

    return TRUE;
} // MzIme::ConvertCode

// コード変換。
BOOL MzIme::ConvertCode(LogCompStr& comp, LogCandInfo& cand)
{
    MzConvResult result;
    std::wstring strTyping = comp.extra.typing_clauses[comp.extra.iClause];
    if (!ConvertCode(strTyping, result)) {
        return FALSE;
    }
    return StoreResult(result, comp, cand);
} // MzIme::ConvertCode

// 結果を格納する。
BOOL MzIme::StoreResult(const MzConvResult& result, LogCompStr& comp, LogCandInfo& cand)
{
    // 未確定文字列をクリア。
    comp.comp_str.clear();
    comp.extra.clear();

    // 未確定文字列をセット。
    comp.comp_clause.resize(result.clauses.size() + 1);
    for (size_t iClause = 0; iClause < result.clauses.size(); ++iClause) {
        const MzConvClause& clause = result.clauses[iClause];
        candidates_t::const_iterator it, end = clause.candidates.end();
        for (it = clause.candidates.begin(); it != end; ++it) {
            const MzConvCandidate& cand2 = *it;
            comp.comp_clause[iClause] = (DWORD)comp.comp_str.size();
            comp.extra.hiragana_clauses.push_back(cand2.pre);
            std::wstring typing = mz_hiragana_to_typing(cand2.pre);
            comp.extra.typing_clauses.push_back(typing);
            comp.comp_str += cand2.post;
            break;
        }
    }
    comp.comp_clause[result.clauses.size()] = (DWORD)comp.comp_str.size();
    comp.comp_attr.assign(comp.comp_str.size(), ATTR_CONVERTED);
    comp.extra.iClause = 0;
    comp.SetClauseAttr(comp.extra.iClause, ATTR_TARGET_CONVERTED);
    comp.dwCursorPos = (DWORD)comp.comp_str.size();
    comp.dwDeltaStart = 0;

    // 候補情報をセット。
    cand.clear();
    clauses_t::const_iterator it0, end0 = result.clauses.end();
    for (it0 = result.clauses.begin(); it0 != end0; ++it0) {
        const MzConvClause& clause = *it0;
        LogCandList cand_list;
        candidates_t::const_iterator it1, end1 = clause.candidates.end();
        for (it1 = clause.candidates.begin(); it1 != end1; ++it1) {
            const MzConvCandidate& cand2 = *it1;
            cand_list.cand_strs.push_back(cand2.post);
        }
        cand.cand_lists.push_back(cand_list);
    }
    cand.iClause = 0;

    return TRUE;
} // MzIme::StoreResult

LPCTSTR KatsuyouToString(KatsuyouKei kk) {
    static const LPCWSTR s_array[] =
    {
        L"未然形", // MIZEN_KEI
        L"連用形", // RENYOU_KEI
        L"終止形", // SHUUSHI_KEI
        L"連体形", // RENTAI_KEI
        L"仮定形", // KATEI_KEI
        L"命令形", // MEIREI_KEI
    };
    ASSERT(kk < _countof(s_array));
    return s_array[kk];
}

std::wstring MzConvResult::get_str(bool detailed) const
{
    std::wstring ret;
    size_t iClause = 0;
    clauses_t::const_iterator it0, end0 = clauses.end();
    for (it0 = clauses.begin(); it0 != end0; ++it0) {
        const MzConvClause& clause = *it0;
        if (iClause)
            ret += L"|";
        if (clause.candidates.size() == 1 || !detailed) {
            ret += clause.candidates[0].post;
        } else {
            ret += L"(";
            size_t iCand = 0;
            candidates_t::const_iterator it1, end1 = clause.candidates.end();
            for (it1 = clause.candidates.begin(); it1 != end1; ++it1) {
                const MzConvCandidate& cand = *it1;
                if (iCand)
                    ret += L"|";
                ret += cand.post;
                ret += L":";
                if (cand.word_cost == MAXLONG)
                    ret += L"∞";
                else
                    ret += unboost::to_wstring(cand.word_cost);
                ret += L":";
                if (cand.cost == MAXLONG)
                    ret += L"∞";
                else
                    ret += unboost::to_wstring(cand.cost);
                ret += L":";
                ret += mz_hinshi_to_string(cand.bunrui);
                if (cand.bunrui == HB_GODAN_DOUSHI) {
                    ret += L":";
                    ret += KatsuyouToString(cand.katsuyou);
                }
                ++iCand;
            }
            ret += L")";
        }
        ++iClause;
    }
    return ret;
}

//////////////////////////////////////////////////////////////////////////////
