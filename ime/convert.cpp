// convert.cpp --- mzimeja kana kanji conversion
// かな漢字変換。
//////////////////////////////////////////////////////////////////////////////
// (Japanese, UTF-8)

#include "mzimeja.h"
#include "resource.h"
#include <algorithm>        // for std::sort

const DWORD c_dwMilliseconds = 8000;

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

std::unordered_map<WCHAR,Dan>   g_hiragana_to_dan;  // 母音写像。
std::unordered_map<WCHAR,Gyou>  g_hiragana_to_gyou; // 子音写像。

// 子音の写像と母音の写像を作成する。
void MakeLiteralMaps()
{
    if (g_hiragana_to_gyou.size())
        return;
    g_hiragana_to_gyou.clear();
    g_hiragana_to_dan.clear();
    const size_t count = _countof(s_hiragana_table);
    for (size_t i = 0; i < count; ++i) {
        for (size_t k = 0; k < 5; ++k) {
            g_hiragana_to_gyou[s_hiragana_table[i][k]] = (Gyou)i;
        }
        for (size_t k = 0; k < 5; ++k) {
            g_hiragana_to_dan[s_hiragana_table[i][k]] = (Dan)k;
        }
    }
} // MakeLiteralMaps

// 品詞分類から文字列を取得する関数。
LPCTSTR HinshiToString(HinshiBunrui hinshi)
{
    if (HB_MEISHI <= hinshi && hinshi <= HB_MAX)
        return TheIME.LoadSTR(IDS_HINSHI_00 + (hinshi - HB_MEISHI));
    return NULL;
}

// 文字列から品詞分類を取得する関数。
HinshiBunrui StringToHinshi(LPCTSTR str)
{
    for (INT hinshi = HB_MEISHI; hinshi <= HB_MAX; ++hinshi) {
        LPCTSTR psz = HinshiToString((HinshiBunrui)hinshi);
        if (lstrcmpW(psz, str) == 0)
            return (HinshiBunrui)hinshi;
    }
    return HB_UNKNOWN;
}

// 品詞分類を文字列に変換する（デバッグ用）。
LPCWSTR BunruiToString(HinshiBunrui bunrui)
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
} // BunruiToString

// 品詞の連結コスト。
static int
CandConnectCost(HinshiBunrui bunrui1, HinshiBunrui bunrui2)
{
    if (bunrui2 == HB_PERIOD || bunrui2 == HB_COMMA) return 0;
    if (bunrui2 == HB_TAIL) return 0;
    if (bunrui1 == HB_SYMBOL || bunrui2 == HB_SYMBOL) return 0;
    if (bunrui1 == HB_UNKNOWN || bunrui2 == HB_UNKNOWN) return 0;
    switch (bunrui1) {
    case HB_HEAD:
        switch (bunrui2) {
        case HB_JODOUSHI: case HB_SHUU_JOSHI:
            return 5;
        default:
            break;
        }
        break;
    case HB_MEISHI: // 名詞
        switch (bunrui2) {
        case HB_MEISHI:
            return 10;
        case HB_SETTOUJI:
            return 10;
        case HB_IKEIYOUSHI: case HB_NAKEIYOUSHI:
            return 5;
        default:
            break;
        }
        break;
    case HB_IKEIYOUSHI: case HB_NAKEIYOUSHI: // い形容詞、な形容詞
        switch (bunrui1) {
        case HB_IKEIYOUSHI: case HB_NAKEIYOUSHI:
            return 10;
        case HB_GODAN_DOUSHI: case HB_ICHIDAN_DOUSHI:
        case HB_KAHEN_DOUSHI: case HB_SAHEN_DOUSHI:
            return 3;
        default:
            break;
        }
        break;
    case HB_MIZEN_JODOUSHI: case HB_RENYOU_JODOUSHI:
    case HB_SHUUSHI_JODOUSHI: case HB_RENTAI_JODOUSHI:
    case HB_KATEI_JODOUSHI: case HB_MEIREI_JODOUSHI:
        ASSERT(0);
        break;
    case HB_GODAN_DOUSHI: case HB_ICHIDAN_DOUSHI: case HB_KAHEN_DOUSHI:
    case HB_SAHEN_DOUSHI: case HB_SETTOUJI:
        switch (bunrui2) {
        case HB_GODAN_DOUSHI: case HB_ICHIDAN_DOUSHI: case HB_KAHEN_DOUSHI:
        case HB_SAHEN_DOUSHI: case HB_SETTOUJI:
            return 5;
        default:
            break;
        }
        break;
    case HB_RENTAISHI: case HB_FUKUSHI: case HB_SETSUZOKUSHI:
    case HB_KANDOUSHI: case HB_KAKU_JOSHI: case HB_SETSUZOKU_JOSHI:
    case HB_FUKU_JOSHI: case HB_SHUU_JOSHI: case HB_JODOUSHI:
    case HB_SETSUBIJI: case HB_COMMA: case HB_PERIOD:
    default:
        break;
    }
    return 1;
} // CandConnectCost

// 品詞の連結可能性。
static BOOL
IsNodeConnectable(const LatticeNode& node1, const LatticeNode& node2)
{
    if (node2.bunrui == HB_PERIOD || node2.bunrui == HB_COMMA) return TRUE;
    if (node2.bunrui == HB_TAIL) return TRUE;
    if (node1.bunrui == HB_SYMBOL || node2.bunrui == HB_SYMBOL) return TRUE;
    if (node1.bunrui == HB_UNKNOWN || node2.bunrui == HB_UNKNOWN) return TRUE;

    switch (node1.bunrui) {
    case HB_HEAD:
        switch (node2.bunrui) {
        case HB_SHUU_JOSHI:
            return FALSE;
        default:
            break;
        }
        return TRUE;
    case HB_MEISHI: // 名詞
        switch (node2.bunrui) {
        case HB_SETTOUJI:
            return FALSE;
        default:
            return TRUE;
        }
        break;
    case HB_IKEIYOUSHI: case HB_NAKEIYOUSHI: // い形容詞、な形容詞
        switch (node1.katsuyou) {
        case MIZEN_KEI:
            if (node2.bunrui == HB_JODOUSHI) {
                if (node2.HasTag(L"[未然形に連結]")) {
                    if (node2.pre[0] == L'な' || node2.pre == L"う") {
                        return TRUE;
                    }
                }
            }
            return FALSE;
        case RENYOU_KEI:
            switch (node2.bunrui) {
            case HB_JODOUSHI:
                if (node2.HasTag(L"[連用形に連結]")) {
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
            if (node2.bunrui == HB_JODOUSHI) {
                if (node2.HasTag(L"[終止形に連結]")) {
                    return TRUE;
                }
                if (node2.HasTag(L"[種々の語]")) {
                    return TRUE;
                }
            }
            if (node2.bunrui == HB_SHUU_JOSHI) {
                return TRUE;
            }
            return FALSE;
        case RENTAI_KEI:
            switch (node2.bunrui) {
            case HB_KANDOUSHI: case HB_JODOUSHI:
                return FALSE;
            default:
                return TRUE;
            }
        case KATEI_KEI:
            switch (node2.bunrui) {
            case HB_SETSUZOKU_JOSHI:
                if (node2.pre == L"ば" || node2.pre == L"ども" || node2.pre == L"ど") {
                    return TRUE;
                }
            default:
                break;
            }
            return FALSE;
        case MEIREI_KEI:
            switch (node2.bunrui) {
            case HB_SHUU_JOSHI: case HB_MEISHI: case HB_SETTOUJI:
                return TRUE;
            default:
                break;
            }
            return FALSE;
        }
        break;
    case HB_RENTAISHI: // 連体詞
        switch (node2.bunrui) {
        case HB_KANDOUSHI: case HB_JODOUSHI: case HB_SETSUBIJI:
            return FALSE;
        default:
            return TRUE;
        }
        break;
    case HB_FUKUSHI: // 副詞
        switch (node2.bunrui) {
        case HB_KAKU_JOSHI: case HB_SETSUZOKU_JOSHI: case HB_FUKU_JOSHI:
        case HB_SETSUBIJI:
            return FALSE;
        default:
            return TRUE;
        }
        break;
    case HB_SETSUZOKUSHI: // 接続詞
        switch (node2.bunrui) {
        case HB_KAKU_JOSHI: case HB_SETSUZOKU_JOSHI:
        case HB_FUKU_JOSHI: case HB_SETSUBIJI:
            return FALSE;
        default:
            return TRUE;
        }
        break;
    case HB_KANDOUSHI: // 感動詞
        switch (node2.bunrui) {
        case HB_KAKU_JOSHI: case HB_SETSUZOKU_JOSHI:
        case HB_FUKU_JOSHI: case HB_SETSUBIJI: case HB_JODOUSHI:
            return FALSE;
        default:
            return TRUE;
        }
        break;
    case HB_KAKU_JOSHI: case HB_SETSUZOKU_JOSHI: case HB_FUKU_JOSHI:
        // 終助詞以外の助詞
        switch (node2.bunrui) {
        case HB_SETSUBIJI:
            return FALSE;
        default:
            return TRUE;
        }
        break;
    case HB_SHUU_JOSHI: // 終助詞
        switch (node2.bunrui) {
        case HB_MEISHI: case HB_SETTOUJI: case HB_SHUU_JOSHI:
            return TRUE;
        default:
            return FALSE;
        }
        break;
    case HB_JODOUSHI: // 助動詞
        switch (node1.katsuyou) {
        case MIZEN_KEI:
            if (node2.bunrui == HB_JODOUSHI) {
                if (node2.HasTag(L"[未然形に連結]")) {
                    return TRUE;
                }
            }
            return FALSE;
        case RENYOU_KEI:
            switch (node2.bunrui) {
            case HB_JODOUSHI:
                if (node2.HasTag(L"[連用形に連結]")) {
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
            if (node2.bunrui == HB_JODOUSHI) {
                if (node2.HasTag(L"[終止形に連結]")) {
                    return TRUE;
                }
                if (node2.HasTag(L"[種々の語]")) {
                    return TRUE;
                }
            }
            if (node2.bunrui == HB_SHUU_JOSHI) {
                return TRUE;
            }
            return FALSE;
        case RENTAI_KEI:
            switch (node2.bunrui) {
            case HB_KANDOUSHI: case HB_JODOUSHI:
                return FALSE;
            default:
                return TRUE;
            }
        case KATEI_KEI:
            switch (node2.bunrui) {
            case HB_SETSUZOKU_JOSHI:
                if (node2.pre == L"ば" || node2.pre == L"ども" || node2.pre == L"ど") {
                    return TRUE;
                }
                break;
            default:
                break;
            }
            return FALSE;
        case MEIREI_KEI:
            switch (node2.bunrui) {
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
        switch (node1.katsuyou) {
        case MIZEN_KEI:
            if (node2.bunrui == HB_JODOUSHI) {
                if (node2.HasTag(L"[未然形に連結]")) {
                    return TRUE;
                }
            }
            return FALSE;
        case RENYOU_KEI:
            switch (node2.bunrui) {
            case HB_JODOUSHI:
                if (node2.HasTag(L"[連用形に連結]")) {
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
            if (node2.bunrui == HB_JODOUSHI) {
                if (node2.HasTag(L"[終止形に連結]")) {
                    return TRUE;
                }
                if (node2.HasTag(L"[種々の語]")) {
                    return TRUE;
                }
            }
            if (node2.bunrui == HB_SHUU_JOSHI) {
                return TRUE;
            }
            return FALSE;
        case RENTAI_KEI:
            switch (node2.bunrui) {
            case HB_KANDOUSHI: case HB_JODOUSHI:
                return FALSE;
            default:
                return TRUE;
            }
            break;
        case KATEI_KEI:
            switch (node2.bunrui) {
            case HB_SETSUZOKU_JOSHI:
                if (node2.pre == L"ば" || node2.pre == L"ども" || node2.pre == L"ど") {
                    return TRUE;
                }
            default:
                break;
            }
            return FALSE;
        case MEIREI_KEI:
            switch (node2.bunrui) {
            case HB_SHUU_JOSHI: case HB_MEISHI: case HB_SETTOUJI:
                return TRUE;
            default:
                return FALSE;
            }
            break;
        }
        break;
    case HB_SETTOUJI: // 接頭辞
        switch (node2.bunrui) {
        case HB_MEISHI:
            return TRUE;
        default:
            return FALSE;
        }
        break;
    case HB_SETSUBIJI: // 接尾辞
        switch (node2.bunrui) {
        case HB_SETTOUJI:
            return FALSE;
        default:
            break;
        }
        break;
    case HB_COMMA: case HB_PERIOD: // 、。
        switch (node2.bunrui) {
        case HB_KAKU_JOSHI: case HB_SETSUZOKU_JOSHI: case HB_FUKU_JOSHI:
        case HB_SHUU_JOSHI: case HB_SETSUBIJI: case HB_JODOUSHI:
            return FALSE;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return TRUE;
} // IsNodeConnectable

// 基本辞書データをスキャンする。
static size_t ScanBasicDict(WStrings& records, const WCHAR *dict_data, WCHAR ch)
{
    DPRINTW(L"%c\n", ch);

    ASSERT(dict_data);

    if (ch == 0)
        return 0;

    // レコード区切りと文字chの組み合わせを検索する。
    // これで文字chで始まる単語を検索できる。
    WCHAR sz[3] = {RECORD_SEP, ch, 0};
    const WCHAR *pch1 = wcsstr(dict_data, sz);
    if (pch1 == NULL)
        return FALSE;

    const WCHAR *pch2 = pch1; // 現在の位置。
    const WCHAR *pch3;
    for (;;) {
        // 現在の位置の次のレコード区切りと文字chの組み合わせを検索する。
        pch3 = wcsstr(pch2 + 1, sz);
        if (pch3 == NULL) break; // なければループ終わり。
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

static INT CALLBACK UserDictProc(LPCTSTR lpRead, DWORD dw, LPCTSTR lpStr, LPVOID lpData)
{
    ASSERT(lpStr && lpStr[0]);
    ASSERT(lpRead && lpRead[0]);
    Lattice *pThis = (Lattice *)lpData;
    ASSERT(pThis != NULL);

    // データの初期化。
    std::wstring pre = lpRead;
    std::wstring post = lpStr;
    Gyou gyou = GYOU_A;
    HinshiBunrui bunrui = StyleToHinshi(dw);

    if (pre.size() <= 1)
        return 0;

    // データを辞書形式に変換する。
    std::wstring substr;
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
        // 「する」「ずる」そのものは登録しない。
        if (pre == L"する" || pre == L"ずる")
            return TRUE;
        //  「する」または「ずる」で終わらなければ失敗。
        substr = pre.substr(pre.size() - 2, 2);
        if (substr == L"する" && post.substr(post.size() - 2, 2) == L"する")
            gyou = GYOU_SA;
        else if (substr == L"ずる" && post.substr(post.size() - 2, 2) == L"ずる")
            gyou = GYOU_ZA;
        else
            return TRUE;
        pre = pre.substr(0, pre.size() - 2);
        post = post.substr(0, post.size() - 2);
        break;
    case HB_GODAN_DOUSHI: // 五段動詞
        // 写像を準備する。
        MakeLiteralMaps();
        // 終端がウ段の文字でなければ失敗。
        if (pre.empty())
            return TRUE;
        ch = pre[pre.size() - 1];
        if (g_hiragana_to_dan[ch] != DAN_U)
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
    fields[I_FIELD_HINSHI] = { MAKEWORD(bunrui, gyou) };
    fields[I_FIELD_POST] = post;
    fields[I_FIELD_TAGS] = L"[ユーザ辞書]";

    std::wstring sep;
    sep += FIELD_SEP;
    std::wstring record = str_join(fields, sep);
    s_UserDictRecords.push_back(record);

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
        ::CloseHandle(hFind);
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
    bool matched = false;
    for (auto& cand : candidates) {
        if (cand.converted == node->post) {
            if (cand.cost > node->cost)  {
                cand.cost = node->cost;
                cand.bunruis.insert(node->bunrui);
                cand.tags += node->tags;
            }
            matched = true;
            break;
        }
    }

    if (!matched) {
        MzConvCandidate cand;
        cand.hiragana = node->pre;
        cand.converted = node->post;
        cand.cost = node->cost;
        cand.bunruis.insert(node->bunrui);
        cand.tags = node->tags;
        candidates.push_back(cand);
    }
}

static inline bool
CandidateCompare(const MzConvCandidate& cand1, const MzConvCandidate& cand2)
{
    return cand1.cost < cand2.cost;
}

// コストで候補をソートする。
void MzConvClause::sort()
{
    std::sort(candidates.begin(), candidates.end(), CandidateCompare);
}

// コストで結果をソートする。
void MzConvResult::sort()
{
    // 文節の境界について。
    for (size_t i = 1; i < clauses.size(); ++i) {
        // 隣り合う文節の候補について。
        for (auto& cand1 : clauses[i - 1].candidates) {
            for (auto& cand2 : clauses[i].candidates) {
                // 該当する品詞分類の最小コストを計算する。
                INT min_cost = 0x7FFF;
                for (auto& bunrui1 : cand1.bunruis) {
                    for (auto& bunrui2 : cand2.bunruis) {
                        INT cost = CandConnectCost(bunrui1, bunrui2);
                        if (cost < min_cost) {
                            min_cost = cost;
                        }
                    }
                }
                // 最小コストを加算する。
                cand2.cost += min_cost;
            }
        }
    }

    for (auto& clause : clauses) {
        clause.sort();
    }
}

//////////////////////////////////////////////////////////////////////////////
// LatticeNode - ラティス（lattice）のノード。

// コストを計算。
int LatticeNode::CalcCost() const
{
    int ret = 0;
    if (bunrui == HB_KANGO) ret += 200;
    if (bunrui == HB_SYMBOL) ret += 120;
    if (tags.size() != 0) {
        if (HasTag(L"[非標準]")) ret += 100;
        if (HasTag(L"[不謹慎]")) ret += 50;
        if (HasTag(L"[人名]")) ret += 30;
        else if (HasTag(L"[駅名]")) ret += 30;
        else if (HasTag(L"[地名]")) ret += 30;

        // ユーザー辞書単語はコストマイナス30。
        if (HasTag(L"[ユーザ辞書]")) ret -= 30;
    }
    return ret;
}

// 動詞か？
bool LatticeNode::IsDoushi() const
{
    switch (bunrui) {
    case HB_GODAN_DOUSHI: case HB_ICHIDAN_DOUSHI:
    case HB_KAHEN_DOUSHI: case HB_SAHEN_DOUSHI:
        return true;
    default:
        break;
    }
    return false;
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
        break;
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////////
// Lattice - ラティス

// 追加情報。
void Lattice::AddExtra()
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
        fields[I_FIELD_HINSHI] = { MAKEWORD(HB_MEISHI, 0) };

        StringCchPrintfW(sz, _countof(sz), L"%u年%u月%u日", st.wYear, st.wMonth, st.wDay);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        fields[I_FIELD_POST] = convert_to_kansuuji_brief(sz);
        DoFields(0, fields, +10);

        fields[I_FIELD_POST] = convert_to_kansuuji_brief_formal(sz);
        DoFields(0, fields, +10);

        StringCchPrintfW(sz, _countof(sz), L"%u年%02u月%02u日", st.wYear, st.wMonth, st.wDay);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        fields[I_FIELD_POST] = convert_to_kansuuji_brief(sz);
        DoFields(0, fields, +10);

        fields[I_FIELD_POST] = convert_to_kansuuji_brief_formal(sz);
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
        fields[I_FIELD_HINSHI] = { MAKEWORD(HB_MEISHI, 0) };

        StringCchPrintfW(sz, _countof(sz), L"%u年", st.wYear);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        fields[I_FIELD_POST] = convert_to_kansuuji_brief(sz);
        DoFields(0, fields, +10);

        fields[I_FIELD_POST] = convert_to_kansuuji_brief_formal(sz);
        DoFields(0, fields, +10);

        return;
    }

    // 今月（this month）
    if (m_pre == L"こんげつ") {
        WStrings fields(NUM_FIELDS);
        fields[I_FIELD_PRE] = m_pre;
        fields[I_FIELD_HINSHI] = { MAKEWORD(HB_MEISHI, 0) };

        StringCchPrintfW(sz, _countof(sz), L"%u年%u月", st.wYear, st.wMonth);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        fields[I_FIELD_POST] = convert_to_kansuuji_brief(sz);
        DoFields(0, fields, +10);

        fields[I_FIELD_POST] = convert_to_kansuuji_brief_formal(sz);
        DoFields(0, fields, +10);

        StringCchPrintfW(sz, _countof(sz), L"%u年%02u月", st.wYear, st.wMonth);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        fields[I_FIELD_POST] = convert_to_kansuuji_brief(sz);
        DoFields(0, fields, +10);

        fields[I_FIELD_POST] = convert_to_kansuuji_brief_formal(sz);
        DoFields(0, fields, +10);

        return;
    }

    // 現在の時刻（current time）
    if (m_pre == L"じこく" || m_pre == L"ただいま") {
        WStrings fields(NUM_FIELDS);
        fields[I_FIELD_PRE] = m_pre;
        fields[I_FIELD_HINSHI] = { MAKEWORD(HB_MEISHI, 0) };

        if (m_pre == L"ただいま") {
            fields[I_FIELD_POST] = L"ただ今";
            DoFields(0, fields);
            fields[I_FIELD_POST] = L"只今";
            DoFields(0, fields);
        }

        StringCchPrintfW(sz, _countof(sz), L"%u時%u分%u秒", st.wHour, st.wMinute, st.wSecond);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        fields[I_FIELD_POST] = convert_to_kansuuji_brief(sz);
        DoFields(0, fields, +10);

        fields[I_FIELD_POST] = convert_to_kansuuji_brief_formal(sz);
        DoFields(0, fields, +10);

        StringCchPrintfW(sz, _countof(sz), L"%02u時%02u分%02u秒", st.wHour, st.wMinute, st.wSecond);
        fields[I_FIELD_POST] = sz;
        DoFields(0, fields);

        fields[I_FIELD_POST] = convert_to_kansuuji_brief(sz);
        DoFields(0, fields, +10);

        fields[I_FIELD_POST] = convert_to_kansuuji_brief_formal(sz);
        DoFields(0, fields, +10);

        if (st.wHour >= 12) {
            StringCchPrintfW(sz, _countof(sz), L"午後%u時%u分%u秒", st.wHour - 12, st.wMinute, st.wSecond);
            fields[I_FIELD_POST] = sz;
            DoFields(0, fields);

            fields[I_FIELD_POST] = convert_to_kansuuji_brief(sz);
            DoFields(0, fields, +10);

            fields[I_FIELD_POST] = convert_to_kansuuji_brief_formal(sz);
            DoFields(0, fields, +10);

            StringCchPrintfW(sz, _countof(sz), L"午後%02u時%02u分%02u秒", st.wHour - 12, st.wMinute, st.wSecond);
            fields[I_FIELD_POST] = sz;
            DoFields(0, fields);

            fields[I_FIELD_POST] = convert_to_kansuuji_brief(sz);
            DoFields(0, fields, +10);

            fields[I_FIELD_POST] = convert_to_kansuuji_brief_formal(sz);
            DoFields(0, fields, +10);
        } else {
            StringCchPrintfW(sz, _countof(sz), L"午前%u時%u分%u秒", st.wHour, st.wMinute, st.wSecond);
            fields[I_FIELD_POST] = sz;
            DoFields(0, fields);

            fields[I_FIELD_POST] = convert_to_kansuuji_brief(sz);
            DoFields(0, fields, +10);

            fields[I_FIELD_POST] = convert_to_kansuuji_brief_formal(sz);
            DoFields(0, fields, +10);

            StringCchPrintfW(sz, _countof(sz), L"午前%02u時%02u分%02u秒", st.wHour, st.wMinute, st.wSecond);
            fields[I_FIELD_POST] = sz;
            DoFields(0, fields);

            fields[I_FIELD_POST] = convert_to_kansuuji_brief(sz);
            DoFields(0, fields, +10);

            fields[I_FIELD_POST] = convert_to_kansuuji_brief_formal(sz);
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
        fields[I_FIELD_HINSHI] = { MAKEWORD(HB_MEISHI, 0) };

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
            fields[I_FIELD_HINSHI] = { MAKEWORD(HB_MEISHI, 0) };
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
        fields[I_FIELD_HINSHI] = { MAKEWORD(HB_SYMBOL, 0) };
        for (auto& item : items) {
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
            fields[I_FIELD_HINSHI] = { MAKEWORD(HB_SYMBOL, 0) };
            int cost = 0;
            while (*pch) {
                fields[I_FIELD_POST].assign(1, *pch++);
                DoFields(0, fields, cost);
                ++cost;
            }
            return;
        }
    }
} // Lattice::AddExtra

// 辞書からノードを追加する。
BOOL Lattice::AddNodes(size_t index, const WCHAR *dict_data)
{
    FOOTMARK();
    const size_t length = m_pre.size();
    ASSERT(length);

    // フィールド区切り（separator）。
    std::wstring sep = { FIELD_SEP };

    WStrings fields, records;
    for (; index < length; ++index) {
        if (m_refs[index] == 0)
            continue;

        // periods (。。。)
        if (is_period(m_pre[index])) {
            size_t saved = index;
            do {
                ++index;
            } while (is_period(m_pre[index]));

            fields.resize(NUM_FIELDS);
            fields[I_FIELD_PRE] = m_pre.substr(saved, index - saved);
            fields[I_FIELD_HINSHI] = { MAKEWORD(HB_PERIOD, 0) };
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
            fields[I_FIELD_HINSHI] = { MAKEWORD(HB_SYMBOL, 0) };
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
        if (is_comma(m_pre[index])) {
            size_t saved = index;
            do {
                ++index;
            } while (is_comma(m_pre[index]));

            fields.resize(NUM_FIELDS);
            fields[I_FIELD_PRE] = m_pre.substr(saved, index - saved);
            fields[I_FIELD_HINSHI] = { MAKEWORD(HB_COMMA, 0) };
            fields[I_FIELD_POST] = fields[I_FIELD_PRE];
            DoFields(saved, fields);
            --index;
            continue;
        }

        // arrow right (→)
        if (is_hyphen(m_pre[index]) && (m_pre[index + 1] == L'>' || m_pre[index + 1] == L'＞'))
        {
            fields.resize(NUM_FIELDS);
            fields[I_FIELD_PRE] = m_pre.substr(index, 2);
            fields[I_FIELD_HINSHI] = { MAKEWORD(HB_SYMBOL, 0) };
            fields[I_FIELD_POST] = { L'→' };
            DoFields(index, fields);
            ++index;
            continue;
        }

        // arrow left (←)
        if ((m_pre[index] == L'<' || m_pre[index] == L'＜') && is_hyphen(m_pre[index + 1]))
        {
            fields.resize(NUM_FIELDS);
            fields[I_FIELD_PRE] = m_pre.substr(index, 2);
            fields[I_FIELD_HINSHI] = { MAKEWORD(HB_SYMBOL, 0) };
            fields[I_FIELD_POST] = { L'←' };
            DoFields(index, fields);
            ++index;
            continue;
        }

        // arrows (zh, zj, zk, zl) and z. etc.
        WCHAR ch0 = translateChar(m_pre[index], FALSE, TRUE);
        if (ch0 == L'z' || ch0 == L'Z') {
            WCHAR ch1 = translateChar(m_pre[index + 1], FALSE, TRUE);
            WCHAR ch2 = 0;
            if (ch1 == L'h' || ch1 == L'H') ch2 = L'←'; // zh
            else if (ch1 == L'j' || ch1 == L'J') ch2 = L'↓'; // zj
            else if (ch1 == L'k' || ch1 == L'K') ch2 = L'↑'; // zk
            else if (ch1 == L'l' || ch1 == L'L') ch2 = L'→'; // zl
            else if (is_hyphen(ch1)) ch2 = L'～'; // z-
            else if (is_period(ch1)) ch2 = L'…'; // z.
            else if (is_comma(ch1)) ch2 = L'‥'; // z,
            else if (ch1 == L'[' || ch1 == L'［' || ch1 == L'「') ch2 = L'『'; // z[
            else if (ch1 == L']' || ch1 == L'］' || ch1 == L'」') ch2 = L'』'; // z]
            else if (ch1 == L'/' || ch1 == L'／') ch2 = L'・'; // z/
            if (ch2) {
                fields.resize(NUM_FIELDS);
                fields[I_FIELD_PRE] = m_pre.substr(index, 2);
                fields[I_FIELD_HINSHI] = { MAKEWORD(HB_SYMBOL, 0) };
                fields[I_FIELD_POST] = { ch2 };
                DoFields(index, fields, -100);
                ++index;
                continue;
            }
        }

        if (!is_hiragana(m_pre[index])) { // ひらがなではない？
            size_t saved = index;
            do {
                ++index;
            } while ((!is_hiragana(m_pre[index]) || is_hyphen(m_pre[index])) && m_pre[index]);

            fields.resize(NUM_FIELDS);
            fields[I_FIELD_PRE] = m_pre.substr(saved, index - saved);
            fields[I_FIELD_HINSHI] = { MAKEWORD(HB_MEISHI, 0) };
            fields[I_FIELD_POST] = fields[I_FIELD_PRE];
            DoMeishi(saved, fields);

            // 全部が数字なら特殊な変換を行う。
            if (are_all_chars_numeric(fields[I_FIELD_PRE])) {
                fields[I_FIELD_POST] = convert_to_kansuuji(fields[I_FIELD_PRE]);
                DoMeishi(saved, fields);
                fields[I_FIELD_POST] = convert_to_kansuuji_brief(fields[I_FIELD_PRE]);
                DoMeishi(saved, fields);
                fields[I_FIELD_POST] = convert_to_kansuuji_formal(fields[I_FIELD_PRE]);
                DoMeishi(saved, fields);
                fields[I_FIELD_POST] = convert_to_kansuuji_brief_formal(fields[I_FIELD_PRE]);
                DoMeishi(saved, fields);
            }

            // 郵便番号変換。
            std::wstring postal = normalize_postal_code(fields[I_FIELD_PRE]);
            if (postal.size()) {
                std::wstring addr = convert_postal_code(postal);
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
        for (auto& record : records) {
            str_split(fields, record, sep);
            DoFields(index, fields);
        }

        // special cases
        switch (m_pre[index]) {
        case L'さ': case L'し': case L'せ': case L'す': // SURU
            // サ変動詞。
            fields.resize(NUM_FIELDS);
            fields[I_FIELD_HINSHI] = { MAKEWORD(HB_SAHEN_DOUSHI, GYOU_SA) };
            DoSahenDoushi(index, fields);
            break;
        default:
            break;
        }
    }

    return TRUE;
} // Lattice::AddNodes

// 単一文節変換用のノード群を追加する。
BOOL Lattice::AddNodesForSingle(const WCHAR *dict_data)
{
    // 区切りを準備。
    std::wstring sep = { FIELD_SEP };

    // 基本辞書をスキャンする。
    WStrings fields, records;
    size_t count = ScanBasicDict(records, dict_data, m_pre[0]);
    DPRINTW(L"ScanBasicDict(%c) count: %d\n", m_pre[0], count);

    // ユーザー辞書をスキャンする。
    count = ScanUserDict(records, m_pre[0], this);
    DPRINTW(L"ScanUserDict(%c) count: %d\n", m_pre[0], count);

    // 各レコードをフィールドに分割して処理。
    for (auto& record : records) {
        str_split(fields, record, sep);
        DoFields(0, fields);
    }

    // 異なるサイズのノードを削除する。
    for (size_t i = 0; i < m_chunks[0].size(); ++i) {
        auto it = std::remove_if(m_chunks[0].begin(), m_chunks[0].end(), [this](const LatticeNodePtr& n){
            return n->pre.size() != m_pre.size();
        });
        m_chunks[0].erase(it, m_chunks[0].end());
    }

    return !m_chunks[0].empty();
}

// 参照を更新する。
void Lattice::UpdateRefs()
{
    // 参照数を初期化する。
    m_refs.assign(m_pre.size() + 1, 0);
    m_refs[0] = 1;

    // 参照数を更新する。
    for (size_t index = 0; index < m_pre.size(); ++index) {
        if (m_refs[index] == 0)
            continue;
        LatticeChunk& chunk1 = m_chunks[index];
        for (auto& ptr1 : chunk1) {
            m_refs[index + ptr1->pre.size()]++;
        }
    }
} // Lattice::UpdateRefs

// リンクを更新する。
void Lattice::UpdateLinks()
{
    ASSERT(m_pre.size());
    ASSERT(m_pre.size() + 1 == m_chunks.size());
    ASSERT(m_pre.size() + 1 == m_refs.size());

    UnlinkAllNodes(); // すべてのノードのリンクを解除する。

    // ヘッド（頭）を追加する。リンク数は１。
    {
        LatticeNode node;
        node.bunrui = HB_HEAD;
        node.linked = 1;
        LatticeChunk& chunk1 = m_chunks[0];
        for (auto& ptr1 : chunk1) {
            ptr1->linked = 1;
            node.branches.push_back(ptr1);
        }
        m_head = std::make_shared<LatticeNode>(node);
    }

    // 尻尾（テイル）を追加する。
    {
        LatticeNode node;
        node.bunrui = HB_TAIL;
        m_chunks[m_pre.size()].clear();
        m_chunks[m_pre.size()].push_back(std::make_shared<LatticeNode>(node));
    }

    // リンクとブランチを追加する。
    size_t num_links = 0;
    for (size_t index = 0; index < m_pre.size(); ++index) {
        LatticeChunk& chunk1 = m_chunks[index];
        for (auto& ptr1 : chunk1) {
            if (!ptr1->linked) continue;
            const auto& pre = ptr1->pre;
            auto& chunk2 = m_chunks[index + pre.size()];
            for (auto& ptr2 : chunk2) {
                if (IsNodeConnectable(*ptr1.get(), *ptr2.get())) {
                    ptr1->branches.push_back(ptr2);
                    ptr2->linked++;
                    num_links++;
                }
            }
        }
    }
} // Lattice::UpdateLinks

void Lattice::UnlinkAllNodes()
{
    // ブランチリンクとリンク数をクリアする。
    for (size_t index = 0; index < m_pre.size(); ++index) {
        LatticeChunk& chunk1 = m_chunks[index];
        for (auto& ptr1 : chunk1) {
            ptr1->linked = 0;
            ptr1->branches.clear();
        }
    }
} // Lattice::UnlinkAllNodes

// 変換失敗時に未定義の単語を追加する。
void Lattice::AddComplement(size_t index, size_t min_size, size_t max_size)
{
    WStrings fields(NUM_FIELDS);
    fields[I_FIELD_HINSHI] = { MAKEWORD(HB_MEISHI, 0) };
    for (size_t count = min_size; count <= max_size; ++count) {
        if (m_pre.size() < index + count)
            continue;
        fields[I_FIELD_PRE] = m_pre.substr(index, count);
        fields[I_FIELD_POST] = fields[I_FIELD_PRE];
        DoFields(index, fields);
    }
} // Lattice::AddComplement

// リンクされていないノードを削除。
void Lattice::CutUnlinkedNodes()
{
    for (size_t index = 0; index < m_pre.size(); ++index) {
        auto& chunk1 = m_chunks[index];
        auto it = std::remove_if(chunk1.begin(), chunk1.end(), [](const LatticeNodePtr& node) {
            return node->linked == 0;
        });
        chunk1.erase(it, chunk1.end());
    }
} // Lattice::CutUnlinkedNodes

// 最後にリンクされたインデックスを取得する。
size_t Lattice::GetLastLinkedIndex() const
{
    // 最後にリンクされたノードがあるか？
    if (m_chunks[m_pre.size()][0]->linked) {
        return m_pre.size(); // 最後のインデックスを返す。
    }

    // チャンクを逆順でスキャンする。
    for (size_t index = m_pre.size(); index > 0; ) {
        --index;
        for (auto& ptr : m_chunks[index]) {
            if (ptr->linked) {
                return index; // リンクされたノードが見つかった。
            }
        }
    }
    return 0; // not found
} // Lattice::GetLastLinkedIndex

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
    node.cost = node.CalcCost() + deltaCost;

    // い形容詞の未然形。
    // 「痛い」→「痛かろ(う)」
    do {
        if (tail.empty() || tail.substr(0, 2) != L"かろ") break;
        node.katsuyou = MIZEN_KEI;
        node.pre = fields[I_FIELD_PRE] + L"かろ";
        node.post = fields[I_FIELD_POST] + L"かろ";
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    MakeLiteralMaps();

    // い形容詞の連用形。
    // 「痛い」→「痛かっ(た)」
    do {
        if (tail.empty() || tail.substr(0, 2) != L"かっ") break;
        node.pre = fields[I_FIELD_PRE] + L"かっ";
        node.post = fields[I_FIELD_POST] + L"かっ";
        node.katsuyou = RENYOU_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        if (tail.size() < 3 || tail[2] != L'た') break;
        node.pre += L'た';
        node.post += L'た';
        node.katsuyou = SHUUSHI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);
    // 「痛い」→「痛く(て)」、「広い」→「広く(て)」
    do {
        if (tail.empty() || tail[0] != L'く') break;
        node.pre = fields[I_FIELD_PRE] + L'く';
        node.post = fields[I_FIELD_POST] + L'く';
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        if (tail.size() < 2 || tail[1] != L'て') break;
        node.pre += tail[1];
        node.post += tail[1];
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);
    // 「広い」→「広う(て)」
    do {
        if (tail.empty() || tail[0] != L'う') break;
        node.pre = fields[I_FIELD_PRE] + L'う';
        node.post = fields[I_FIELD_POST] + L'う';
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);
    // 「美しい」→「美しゅう(て)」
    do {
        if (tail.empty() || tail[0] != L'ゅ' || tail[1] != L'う') break;
        node.pre = fields[I_FIELD_PRE] + L"ゅう";
        node.post = fields[I_FIELD_POST] + L"ゅう";
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);
    // TODO: 「危ない」→「危のう(て)」
    // TODO: 「暖かい」→「暖こう(て)」

    // い形容詞の終止形。「かわいい」「かわいいよ」「かわいいね」「かわいいぞ」
    node.katsuyou = SHUUSHI_KEI;
    do {
        if (tail.empty() || tail[0] != L'い') break;
        node.pre = fields[I_FIELD_PRE] + L'い';
        node.post = fields[I_FIELD_POST] + L'い';
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        if (tail.size() < 1 ||
            (tail[1] != L'よ' && tail[1] != L'ね' && tail[1] != L'な' && tail[1] != L'ぞ'))
                break;
        node.pre += tail[1];
        node.post += tail[1];
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    // い形容詞の連体形。
    // 「痛い」→「痛い(とき)」
    node.katsuyou = RENTAI_KEI;
    do {
        if (tail.empty() || tail[0] != L'い') break;
        node.pre = fields[I_FIELD_PRE] + L'い';
        node.post = fields[I_FIELD_POST] + L'い';
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);
    // 「痛い」→「痛き(とき)」
    do {
        if (tail.empty() || tail[0] != L'き') break;
        node.pre = fields[I_FIELD_PRE] + L'き';
        node.post = fields[I_FIELD_POST] + L'き';
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    // い形容詞の仮定形。
    // 「痛い」→「痛けれ(ば)」
    do {
        if (tail.empty() || tail.substr(0, 2) != L"けれ") break;
        node.katsuyou = KATEI_KEI;
        node.pre = fields[I_FIELD_PRE] + L"けれ";
        node.post = fields[I_FIELD_POST] + L"けれ";
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
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
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);
    do {
        if (tail.empty() || tail[0] != L'み') break;
        node.pre = fields[I_FIELD_PRE] + L'み';
        node.post = fields[I_FIELD_POST] + L'み';
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);
    do {
        if (tail.empty() || tail[0] != L'め') break;
        node.pre = fields[I_FIELD_PRE] + L'め';
        node.post = fields[I_FIELD_POST] + L'め';
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        node.post = fields[I_FIELD_POST] + L'目';
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
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
    node.cost = node.CalcCost() + deltaCost;

    // な形容詞の未然形。
    // 「巨大な」→「巨大だろ(う)」
    do {
        if (tail.empty() || tail.substr(0, 2) != L"だろ") break;
        node.katsuyou = MIZEN_KEI;
        node.pre = fields[I_FIELD_PRE] + L"だろ";
        node.post = fields[I_FIELD_POST] + L"だろ";
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    // な形容詞の連用形。
    // 「巨大な」→「巨大だっ(た)」
    do {
        if (tail.empty() || tail.substr(0, 2) != L"だっ") break;
        node.pre = fields[I_FIELD_PRE] + L"だっ";
        node.post = fields[I_FIELD_POST] + L"だっ";
        node.katsuyou = RENYOU_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        if (tail.size() < 3 || tail[2] != L'た') break;
        node.pre += L'た';
        node.post += L'た';
        node.katsuyou = SHUUSHI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);
    node.katsuyou = RENYOU_KEI;
    do {
        if (tail.empty() || tail[0] != L'で') break;
        node.pre = fields[I_FIELD_PRE] + L'で';
        node.post = fields[I_FIELD_POST] + L'で';
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);
    do {
        if (tail.empty() || tail[0] != L'に') break;
        node.pre = fields[I_FIELD_PRE] + L'に';
        node.post = fields[I_FIELD_POST] + L'に';
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    // な形容詞の終止形。
    // 「巨大な」→「巨大だ」「巨大だね」「巨大だぞ」
    do {
        if (tail.empty() || tail[0] != L'だ') break;
        node.katsuyou = SHUUSHI_KEI;
        node.pre = fields[I_FIELD_PRE] + L'だ';
        node.post = fields[I_FIELD_POST] + L'だ';
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        if (tail.size() < 1 ||
            (tail[1] != L'よ' && tail[1] != L'ね' && tail[1] != L'な' && tail[1] != L'ぞ'))
            break;
        node.pre += tail[1];
        node.post += tail[1];
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    // な形容詞の連体形。
    // 「巨大な」→「巨大な(とき)」
    do {
        if (tail.empty() || tail[0] != L'な') break;
        node.katsuyou = RENTAI_KEI;
        node.pre = fields[I_FIELD_PRE] + L'な';
        node.post = fields[I_FIELD_POST] + L'な';
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    // な形容詞の仮定形。
    // 「巨大な」→「巨大なら」
    do {
        if (tail.empty() || tail.substr(0, 2) != L"なら") break;
        node.katsuyou = KATEI_KEI;
        node.pre = fields[I_FIELD_PRE] + L"なら";
        node.post = fields[I_FIELD_POST] + L"なら";
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    // な形容詞の名詞形。
    // 「きれいな(な形容詞)」→「きれいさ(名詞)」、
    // 「巨大な」→「巨大さ」。
    node.bunrui = HB_MEISHI;
    do {
        if (tail.empty() || tail[0] != L'さ') break;
        node.pre = fields[I_FIELD_PRE] + L'さ';
        node.post = fields[I_FIELD_POST] + L'さ';
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
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
    node.cost = node.CalcCost() + deltaCost;
    node.gyou = (Gyou)HIBYTE(fields[I_FIELD_HINSHI][0]);

    // 五段動詞の未然形。
    // 「咲く(五段)」→「咲か(ない)」、「食う(五段)」→「食わ(ない)」
    do {
        node.katsuyou = MIZEN_KEI;
        if (node.gyou == GYOU_A) {
            if (tail.empty() || tail[0] != L'わ')
                break;
            node.pre = fields[I_FIELD_PRE] + L'わ';
            node.post = fields[I_FIELD_POST] + L'わ';
        } else {
            WCHAR ch = s_hiragana_table[node.gyou][DAN_A];
            if (tail.empty() || tail[0] != ch)
                break;
            node.pre = fields[I_FIELD_PRE] + ch;
            node.post = fields[I_FIELD_POST] + ch;
        }
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    // 五段動詞の連用形。
    // 「咲く(五段)」→「咲き(ます)」、「食う(五段)」→「食い(ます)」
    node.katsuyou = RENYOU_KEI;
    WCHAR ch = s_hiragana_table[node.gyou][DAN_I];
    if (tail.size() >= 1 && tail[0] == ch) {
        node.pre = fields[I_FIELD_PRE] + ch;
        node.post = fields[I_FIELD_POST] + ch;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    }

    // 五段動詞の連用形の音便処理。
    // 「咲き(て/た/たり/ても)」→「咲い(て/た/たり/ても)」
    // 「食い(て/た/たり/ても)」→「食っ(て/た/たり/ても)」
    // 「泣き(て/た/たり/ても)」→「泣い(て/た/たり/ても)」
    // 「持ち(て/た/たり/ても)」→「持っ(て/た/たり/ても)」
    // 「呼び(て/た/たり/ても)」→「呼ん(で/だ/だり/でも)」
    // 「書き(て/た/たり/ても)」→「書い(て/た/たり/ても)」
    Gyou gyou = g_hiragana_to_gyou[ch];
    WCHAR ch2 = 0;
    switch (gyou) {
    case GYOU_KA: case GYOU_GA:
        ch2 = L'い';
        break;

    case GYOU_NA: case GYOU_BA: case GYOU_MA:
        ch2 = L'ん';
        break;

    case GYOU_A: case GYOU_TA: case GYOU_RA: case GYOU_WA:
        ch2 = L'っ';
        break;

    case GYOU_SA: case GYOU_ZA: case GYOU_DA: case GYOU_HA: case GYOU_PA:
    case GYOU_YA: case GYOU_NN:
        ch2 = 0;
        break;
    }
    if (ch2 != 0 && tail.size() >= 1 && tail[0] == ch2) {
        node.pre = fields[I_FIELD_PRE] + ch2;
        node.post = fields[I_FIELD_POST] + ch2;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        if (tail.size() >= 3 && (tail[1] == L'て' || tail[1] == L'で') && tail[2] == L'も') {
            // 連用形「ても」「でも」
            node.katsuyou = RENYOU_KEI;
            node.pre = fields[I_FIELD_PRE] + ch2 + tail[1] + tail[2];
            node.post = fields[I_FIELD_POST] + ch2 + tail[1] + tail[2];
            m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
            m_refs[index + node.pre.size()]++;
        } else if (tail.size() >= 2 && (tail[1] == L'て' || tail[1] == L'で')) {
            // 連用形「て」「で」
            node.katsuyou = RENYOU_KEI;
            node.pre = fields[I_FIELD_PRE] + ch2 + tail[1];
            node.post = fields[I_FIELD_POST] + ch2 + tail[1];
            m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
            m_refs[index + node.pre.size()]++;
        } else if (tail.size() >= 3 && (tail[1] == L'た' || tail[1] == L'だ') && tail[2] == L'り') {
            // 連用形「たり」「だり」
            node.katsuyou = RENYOU_KEI;
            node.pre = fields[I_FIELD_PRE] + ch2 + tail[1] + tail[2];
            node.post = fields[I_FIELD_POST] + ch2 + tail[1] + tail[2];
            m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
            m_refs[index + node.pre.size()]++;
        } else if (tail.size() >= 3 && tail[1] == L'た') {
            // 終止形「た」
            node.katsuyou = SHUUSHI_KEI;
            node.pre = fields[I_FIELD_PRE] + ch2 + tail[1];
            node.post = fields[I_FIELD_POST] + ch2 + tail[1];
            m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
            m_refs[index + node.pre.size()]++;
        }
    }

    // 五段動詞の終止形。「動く」「聞き取る」
    // 五段動詞の連体形。「動く(とき)」「聞き取る(とき)」
    do {
        WCHAR ch = s_hiragana_table[node.gyou][DAN_U];
        if (tail.size() <= 0 || tail[0] != ch)
            break;

        node.katsuyou = SHUUSHI_KEI;
        node.pre = fields[I_FIELD_PRE] + ch;
        node.post = fields[I_FIELD_POST] + ch;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;

        node.katsuyou = RENTAI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;

        // 「動くよ」「動くね」「動くな」「動くぞ」
        if (tail.size() < 2 ||
            (tail[1] != L'よ' && tail[1] != L'ね' && tail[1] != L'な' && tail[1] != L'ぞ'))
                break;
        node.pre += tail[1];
        node.post += tail[1];
        node.katsuyou = SHUUSHI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    // 五段動詞の仮定形。「動く」→「動け(ば)」、「聞き取る」→「聞き取れ(ば)」
    // 五段動詞の命令形。「動く」→「動け」「動けよ」、「聞き取る」→「聞き取れ」「聞き取れよ」
    do {
        WCHAR ch = s_hiragana_table[node.gyou][DAN_E];
        if (tail.empty() || tail[0] != ch)
            break;
        node.katsuyou = KATEI_KEI;
        node.pre = fields[I_FIELD_PRE] + ch;
        node.post = fields[I_FIELD_POST] + ch;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;

        node.katsuyou = MEIREI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;

        if (tail.size() < 2 || (tail[1] != L'よ' && tail[1] != L'や'))
            break;
        node.pre += tail[1];
        node.post += tail[1];
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    // 五段動詞の命令形。「動く」→「動こう」「動こうよ」、「聞き取る」→「聞き取ろう」「聞き取ろうよ」
    do {
        WCHAR ch = s_hiragana_table[node.gyou][DAN_O];
        if (tail.size() < 2 || tail[0] != ch || tail[1] != L'う')
            break;
        node.katsuyou = MEIREI_KEI;
        node.pre = fields[I_FIELD_PRE] + ch + L'う';
        node.post = fields[I_FIELD_POST] + ch + L'う';
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;

        if (tail.size() < 3 ||
            (tail[2] != L'よ' && tail[2] != L'や' && tail[2] != L'な' && tail[2] != L'ね'))
            break;
        node.pre += tail[2];
        node.post += tail[2];
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    // 五段動詞の名詞形。
    // 「動く(五段)」→「動き(名詞)」「動き方(名詞)」、
    // 「聞き取る(五段)」→「聞き取り(名詞)」「聞き取り方(名詞)」など。
    node.bunrui = HB_MEISHI;
    do {
        WCHAR ch = s_hiragana_table[node.gyou][DAN_I];
        if (tail.empty() || tail[0] != ch)
            break;
        node.pre = fields[I_FIELD_PRE] + ch;
        node.post = fields[I_FIELD_POST] + ch;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        if (tail[1] != L'か' || tail[2] != L'た')
            break;
        node.pre += L"かた";
        node.post += L"方";
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    // 「動く(五段)」→「動ける(一段)」、
    // 「聞く(五段)」→「聞ける(一段)」など。
    do {
        WCHAR ch = s_hiragana_table[node.gyou][DAN_E];
        if (tail.empty() || tail[0] != ch)
            break;
        WStrings new_fields = fields;
        new_fields[I_FIELD_PRE] += ch;
        new_fields[I_FIELD_POST] += ch;
        DoIchidanDoushi(index, new_fields, deltaCost);
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
    node.cost = node.CalcCost() + deltaCost;

    // 一段動詞の未然形。「寄せる」→「寄せ(ない/よう)」、「見る」→「見(ない/よう)」
    // 一段動詞の連用形。「寄せる」→「寄せ(ます/た/て)」、「見る」→「見(ます/た/て)」
    do {
        node.pre = fields[I_FIELD_PRE];
        node.post = fields[I_FIELD_POST];
        node.katsuyou = MIZEN_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        node.katsuyou = RENYOU_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        // 「見て」
        if (tail.size() >= 1 && tail[0] == L'て') {
            node.pre += tail[0];
            node.post += tail[0];
            m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
            m_refs[index + node.pre.size()]++;
            // 「見ていた」
            if (tail.size() >= 3 && tail[1] == L'い' && tail[2] == L'た') {
                node.pre += L"いた";
                node.post += L"いた";
                node.katsuyou = SHUUSHI_KEI;
                m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
                m_refs[index + node.pre.size()]++;
                // 「見ていたよ」「見ていたな」「見ていたね」
                if (tail.size() >= 4 && (tail[3] == L'よ' || tail[3] == L'な' || tail[3] == L'ね')) {
                    node.pre += tail[3];
                    node.post += tail[3];
                    node.katsuyou = SHUUSHI_KEI;
                    m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
                    m_refs[index + node.pre.size()]++;
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
                        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
                        m_refs[index + node.pre.size()]++;
                    }
                }
            }
            // 「見てた」
            if (tail.size() >= 2 && tail[1] == L'た') {
                node.pre = fields[I_FIELD_PRE] + L"てた";
                node.post = fields[I_FIELD_POST] + L"てた";
                node.katsuyou = SHUUSHI_KEI;
                m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
                m_refs[index + node.pre.size()]++;
                // 「見てたよ」「見てたな」「見てたね」
                if (tail.size() >= 3 && (tail[2] == L'よ' || tail[2] == L'な' || tail[2] == L'ね')) {
                    node.pre += tail[2];
                    node.post += tail[2];
                    node.katsuyou = SHUUSHI_KEI;
                    m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
                    m_refs[index + node.pre.size()]++;
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
                        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
                        m_refs[index + node.pre.size()]++;
                    }
                }
            }
        }
        // 終止形「見よう」「見ようね」「見ようや」「見ような」「見ようぞ」
        if (tail.size() >= 2 && tail[0] == L'よ' && tail[1] == L'う') {
            node.katsuyou = SHUUSHI_KEI;
            node.pre = fields[I_FIELD_PRE] + L"よう";
            node.post = fields[I_FIELD_POST] + L"よう";
            m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
            m_refs[index + node.pre.size()]++;
            if (tail.size() >= 3 &&
                (tail[2] == L'ね' || tail[2] == L'や' || tail[2] == L'な' || tail[2] == L'ぞ'))
            {
                node.pre += tail[2];
                node.post += tail[2];
                m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
                m_refs[index + node.pre.size()]++;
            }
        }
    } while (0);

    // 一段動詞の終止形。「寄せる」「見る」
    // 一段動詞の連体形。「寄せる(とき)」「見る(とき)」
    do {
        if (tail.empty() || tail[0] != L'る') break;
        node.pre = fields[I_FIELD_PRE] + L'る';
        node.post = fields[I_FIELD_POST] + L'る';
        node.katsuyou = SHUUSHI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        node.katsuyou = RENTAI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        // 「見るよ」「見るね」「見るな」「見るぞ」
        if (tail.size() < 2 ||
            (tail[1] != L'よ' && tail[1] != L'ね' && tail[1] != L'な' && tail[1] != L'ぞ'))
                break;
        node.pre += tail[1];
        node.post += tail[1];
        node.katsuyou = SHUUSHI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    // 一段動詞の仮定形。「寄せる」→「寄せれ(ば)」、「見る」→「見れ(ば)」
    do {
        if (tail.empty() || tail[0] != L'れ') break;
        node.katsuyou = KATEI_KEI;
        node.pre = fields[I_FIELD_PRE] + L'れ';
        node.post = fields[I_FIELD_POST] + L'れ';
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    // 一段動詞の命令形。
    // 「寄せる」→「寄せろ」「寄せろよ」、「見る」→「見ろ」「見ろよ」
    node.katsuyou = MEIREI_KEI;
    do {
        if (tail.empty() || tail[0] != L'ろ') break;
        node.pre = fields[I_FIELD_PRE] + L'ろ';
        node.post = fields[I_FIELD_POST] + L'ろ';
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        if (tail.size() < 2 || (tail[1] != L'よ' && tail[1] != L'や')) break;
        node.pre += tail[1];
        node.post += tail[1];
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);
    // 「寄せる」→「寄せよ」、「見る」→「見よ」
    do {
        if (tail.empty() || tail[0] != L'よ') break;
        node.pre = fields[I_FIELD_PRE] + L'よ';
        node.post = fields[I_FIELD_POST] + L'よ';
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    // 一段動詞の名詞形。
    // 「寄せる」→「寄せ」「寄せ方」、「見る」→「見」「見方」
    node.bunrui = HB_MEISHI;
    do {
        node.pre = fields[I_FIELD_PRE];
        node.post = fields[I_FIELD_POST];
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        if (tail.empty() || tail[0] != L'か' || tail[1] != L'た') break;
        node.pre += L"かた";
        node.post += L"方";
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
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
    node.cost = node.CalcCost() + deltaCost;

    // 「くる」「こ(ない)」「き(ます)」などと、語幹が一致しないので、
    // 実際の辞書では「来い」を登録するなど回避策を施している。

    // 終止形と連用形「～来る」
    do {
        if (tail.size() < 2 || tail[0] != L'く' || tail[1] != L'る') break;
        node.pre = fields[I_FIELD_PRE] + L"くる";
        node.post = fields[I_FIELD_POST] + L"来る";
        node.katsuyou = SHUUSHI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        node.katsuyou = RENTAI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        // 「来るよ」「来るね」「来るな」「来るぞ」
        if (tail.size() < 3 ||
            (tail[2] != L'よ' && tail[2] != L'ね' && tail[2] != L'な' && tail[2] != L'ぞ'))
                break;
        node.pre += tail[2];
        node.post += tail[2];
        node.katsuyou = SHUUSHI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);
    do {
        if (fields[I_FIELD_PRE] != L"くる") break;
        node.pre = fields[I_FIELD_PRE];
        node.post = fields[I_FIELD_POST];
        node.katsuyou = SHUUSHI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        node.katsuyou = RENTAI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        // 「来るよ」「来るね」「来るな」「来るぞ」
        if (tail.size() < 1 ||
            (tail[0] != L'よ' && tail[0] != L'ね' && tail[0] != L'な' && tail[0] != L'ぞ'))
                break;
        node.pre += tail[0];
        node.post += tail[0];
        node.katsuyou = SHUUSHI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    // 命令形「～こい」「～こいよ」「～こいや」
    do {
        if (tail.size() < 2 || tail[0] != L'こ' || tail[1] != L'い') break;
        node.pre = fields[I_FIELD_PRE] + L"こい";
        node.post = fields[I_FIELD_POST] + L"来い";
        node.katsuyou = MEIREI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        if (tail.size() < 3 || (tail[2] != L'よ' && tail[2] != L'や')) break;
        node.pre += tail[2];
        node.post += tail[2];
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);
    do {
        if (fields[I_FIELD_PRE] != L"こい") break;
        node.pre = fields[I_FIELD_PRE];
        node.post = fields[I_FIELD_POST];
        node.katsuyou = MEIREI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        if (tail.size() < 1 || (tail[0] != L'よ' && tail[0] != L'や')) break;
        node.pre += tail[0];
        node.post += tail[0];
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    // 仮定形「～来れ」
    do {
        if (tail.size() < 2 || tail[0] != L'く' || tail[1] != L'れ') break;
        node.pre = fields[I_FIELD_PRE] + L"くれ";
        node.post = fields[I_FIELD_POST] + L"来れ";
        node.katsuyou = KATEI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);
    do {
        if (fields[I_FIELD_PRE] != L"くれ") break;
        node.pre = fields[I_FIELD_PRE];
        node.post = fields[I_FIELD_POST];
        node.katsuyou = KATEI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    // 未然形「～来」（こ）
    do {
        if (tail.size() < 1 || tail[0] != L'こ') break;
        node.pre = fields[I_FIELD_PRE] + L"こ";
        node.post = fields[I_FIELD_POST] + L"来";
        node.katsuyou = MIZEN_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);
    do {
        if (fields[I_FIELD_PRE] != L"こ") break;
        node.pre = fields[I_FIELD_PRE];
        node.post = fields[I_FIELD_POST];
        node.katsuyou = MIZEN_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    // 連用形「～来」（き）
    do {
        if (tail.size() < 1 || tail[0] != L'き') break;
        node.pre = fields[I_FIELD_PRE] + L"き";
        node.post = fields[I_FIELD_POST] + L"来";
        node.katsuyou = RENYOU_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);
    do {
        if (fields[I_FIELD_PRE] != L"き") break;
        node.pre = fields[I_FIELD_PRE];
        node.post = fields[I_FIELD_POST];
        node.katsuyou = RENYOU_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
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
    // 語幹の後の部分文字列。
    std::wstring tail = m_pre.substr(index + length);

    // ラティスノードの準備。
    LatticeNode node;
    node.bunrui = HB_SAHEN_DOUSHI;
    node.tags = fields[I_FIELD_TAGS];
    node.cost = node.CalcCost() + deltaCost;
    node.gyou = (Gyou)HIBYTE(fields[I_FIELD_HINSHI][0]);

    // 未然形
    node.katsuyou = MIZEN_KEI;
    do {
        if (node.gyou == GYOU_ZA) {
            if (tail.empty() || tail[0] != L'ざ') break;
            node.pre = fields[I_FIELD_PRE] + L'ざ';
            node.post = fields[I_FIELD_POST] + L'ざ';
        } else {
            if (tail.empty() || tail[0] != L'さ') break;
            node.pre = fields[I_FIELD_PRE] + L'さ';
            node.post = fields[I_FIELD_POST] + L'さ';
        }
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);
    do {
        if (node.gyou == GYOU_ZA) {
            if (tail.empty() || tail[0] != L'じ') break;
            node.pre = fields[I_FIELD_PRE] + L'じ';
            node.post = fields[I_FIELD_POST] + L'じ';
        } else {
            if (tail.empty() || tail[0] != L'し') break;
            node.pre = fields[I_FIELD_PRE] + L'し';
            node.post = fields[I_FIELD_POST] + L'し';
        }
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);
    do {
        if (node.gyou == GYOU_ZA) {
            if (tail.empty() || tail[0] != L'ぜ') break;
            node.pre = fields[I_FIELD_PRE] + L'ぜ';
            node.post = fields[I_FIELD_POST] + L'ぜ';
        } else {
            if (tail.empty() || tail[0] != L'せ') break;
            node.pre = fields[I_FIELD_PRE] + L'せ';
            node.post = fields[I_FIELD_POST] + L'せ';
        }
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    // 連用形
    node.katsuyou = RENYOU_KEI;
    do {
        if (node.gyou == GYOU_ZA) {
            if (tail.empty() || tail[0] != L'じ') break;
            node.pre = fields[I_FIELD_PRE] + L'じ';
            node.post = fields[I_FIELD_POST] + L'じ';
        } else {
            if (tail.empty() || tail[0] != L'し') break;
            node.pre = fields[I_FIELD_PRE] + L'し';
            node.post = fields[I_FIELD_POST] + L'し';
        }
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    // 終止形
    // 連用形
    do {
        if (node.gyou == GYOU_ZA) {
            if (tail.empty() || tail.substr(0, 2) != L"ずる") break;
            node.pre = fields[I_FIELD_PRE] + L"ずる";
            node.post = fields[I_FIELD_POST] + L"ずる";
        } else {
            if (tail.empty() || tail.substr(0, 2) != L"する") break;
            node.pre = fields[I_FIELD_PRE] + L"する";
            node.post = fields[I_FIELD_POST] + L"する";
        }
        node.katsuyou = SHUUSHI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;

        node.katsuyou = RENYOU_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);
    do {
        if (node.gyou == GYOU_ZA) {
            if (tail.empty() || tail[0] != L'ず') break;
            node.pre = fields[I_FIELD_PRE] + L'ず';
            node.post = fields[I_FIELD_POST] + L'ず';
        } else {
            if (tail.empty() || tail[0] != L'す') break;
            node.pre = fields[I_FIELD_PRE] + L'す';
            node.post = fields[I_FIELD_POST] + L'す';
        }
        node.katsuyou = SHUUSHI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    // 仮定形
    do {
        if (node.gyou == GYOU_ZA) {
            if (tail.empty() || tail.substr(0, 2) != L"ずれ") break;
            node.pre = fields[I_FIELD_PRE] + L"ずれ";
            node.post = fields[I_FIELD_POST] + L"ずれ";
        } else {
            if (tail.empty() || tail.substr(0, 2) != L"すれ") break;
            node.pre = fields[I_FIELD_PRE] + L"すれ";
            node.post = fields[I_FIELD_POST] + L"すれ";
        }
        node.katsuyou = KATEI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    // 命令形
    node.katsuyou = MEIREI_KEI;
    do {
        if (node.gyou == GYOU_ZA) {
            if (tail.empty() || tail.substr(0, 2) != L"じろ") break;
            node.pre = fields[I_FIELD_PRE] + L"じろ";
            node.post = fields[I_FIELD_POST] + L"じろ";
        } else {
            if (tail.empty() || tail.substr(0, 2) != L"しろ") break;
            node.pre = fields[I_FIELD_PRE] + L"しろ";
            node.post = fields[I_FIELD_POST] + L"しろ";
        }
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);
    do {
        if (node.gyou == GYOU_ZA) {
            if (tail.empty() || tail.substr(0, 2) != L"ぜよ") break;
            node.pre = fields[I_FIELD_PRE] + L"ぜよ";
            node.post = fields[I_FIELD_POST] + L"ぜよ";
        } else {
            if (tail.empty() || tail.substr(0, 2) != L"せよ") break;
            node.pre = fields[I_FIELD_PRE] + L"せよ";
            node.post = fields[I_FIELD_POST] + L"せよ";
        }
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);
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
    node.cost = node.CalcCost() + deltaCost;

    // 名詞は活用なし。
    if (node.HasTag(L"[動植物]")) {
        // 動植物名は、カタカナでもよい。
        node.pre = fields[I_FIELD_PRE];
        node.post = lcmap(fields[I_FIELD_PRE], LCMAP_KATAKANA | LCMAP_FULLWIDTH);
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + length]++;

        node.cost += 30;
        node.pre = fields[I_FIELD_PRE];
        node.post = fields[I_FIELD_POST];
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + length]++;
    } else {
        node.pre = fields[I_FIELD_PRE];
        node.post = fields[I_FIELD_POST];
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + length]++;
    }

    // 名詞＋「っぽい」でい形容詞に。
    if (tail.size() >= 2 && tail[0] == L'っ' && tail[1] == L'ぽ') {
        WStrings new_fields = fields;
        new_fields[I_FIELD_PRE] += L"っぽ";
        new_fields[I_FIELD_POST] += L"っぽ";
        DoIkeiyoushi(index, new_fields, deltaCost);
    }

    // 名詞＋「する」「すれ」でサ変動詞に。
    if (tail.size() >= 2 && tail[0] == L'す' && (tail[1] == L'る' || tail[1] == L'れ')) {
        DoSahenDoushi(index, fields, deltaCost - 10);
    }

    // 名詞＋「し」でサ変動詞に
    if (tail.size() >= 1 && tail[0] == L'し') {
        DoSahenDoushi(index, fields, deltaCost - 10);
    }

    // 名詞＋「せよ」でサ変動詞に。
    if (tail.size() >= 2 && tail[0] == L'せ' && tail[1] == L'よ') {
        DoSahenDoushi(index, fields, deltaCost - 10);
    }

    // 名詞＋「な」でな形容詞に。
    if (tail.size() >= 1 && tail[0] == L'な') {
        DoNakeiyoushi(index, fields, deltaCost);
    }

    // 名詞＋「たる」「たれ」で五段動詞に。
    if (tail.size() >= 2 && tail[0] == L'た' && (tail[1] == L'る' || tail[1] == L'れ')) {
        WStrings new_fields = fields;
        new_fields[I_FIELD_PRE] += L'た';
        new_fields[I_FIELD_POST] += L'た';
        new_fields[I_FIELD_HINSHI] = { MAKEWORD(HB_GODAN_DOUSHI, GYOU_RA) };
        DoGodanDoushi(index, new_fields, deltaCost);
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
    node.cost = node.CalcCost() + deltaCost;

    // 副詞。活用はない。
    node.pre = fields[I_FIELD_PRE];
    node.post = fields[I_FIELD_POST];
    m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
    m_refs[index + node.pre.size()]++;

    // 副詞なら最後に「と」「に」を付けてもいい。
    do {
        if (tail.size() < 1 || (tail[0] != L'と' && tail[0] != L'に')) break;
        node.pre += tail[0];
        node.post += tail[0];
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
    } while (0);

    // 副詞なら最後に「っと」「って」を付けてもいい。
    do {
        if (tail.size() < 2 || tail[0] != L'っ' || (tail[1] != L'と' && tail[1] != L'て')) break;
        node.pre = fields[I_FIELD_PRE] + tail[0] + tail[1];
        node.post = fields[I_FIELD_POST] + tail[0] + tail[1];
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
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
    node.cost = node.CalcCost() + deltaCost;

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
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
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
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        break;
    case HB_RENYOU_JODOUSHI: // 連用助動詞。
        node.bunrui = HB_JODOUSHI;
        node.katsuyou = RENYOU_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        break;
    case HB_SHUUSHI_JODOUSHI: // 終止助動詞。
        node.bunrui = HB_JODOUSHI;
        node.katsuyou = SHUUSHI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        break;
    case HB_RENTAI_JODOUSHI: // 連体助動詞。
        node.bunrui = HB_JODOUSHI;
        node.katsuyou = RENTAI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        break;
    case HB_KATEI_JODOUSHI: // 仮定助動詞。
        node.bunrui = HB_JODOUSHI;
        node.katsuyou = KATEI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
        break;
    case HB_MEIREI_JODOUSHI: // 命令助動詞。
        node.bunrui = HB_JODOUSHI;
        node.katsuyou = MEIREI_KEI;
        m_chunks[index].push_back(std::make_shared<LatticeNode>(node));
        m_refs[index + node.pre.size()]++;
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
        for (size_t k = 0; k < m_chunks[i].size(); ++k) {
            DPRINTW(L" %s(%s)", m_chunks[i][k]->post.c_str(),
                        BunruiToString(m_chunks[i][k]->bunrui));
        }
        DPRINTW(L"\n");
    }
} // Lattice::Dump

//////////////////////////////////////////////////////////////////////////////

BOOL Lattice::MakeLatticeInternal(size_t length, const WCHAR* dict_data)
{
    // link and cut not linked
    UpdateLinks();
    CutUnlinkedNodes();

    // does it reach the last?
    size_t index = GetLastLinkedIndex();
    if (index == length)
        return TRUE;

    // add complement
    UpdateRefs();
    AddComplement(index, 1, 5);
    AddNodes(index + 1, dict_data);

    return FALSE;
}

// 複数文節変換において、ラティスを作成する。
BOOL MzIme::MakeLatticeForMulti(Lattice& lattice, const std::wstring& pre)
{
    DPRINTW(L"%s\n", pre.c_str());

    ASSERT(pre.size() != 0);

    // ラティスを初期化。
    lattice.m_pre = pre; // 変換前の文字列。
    lattice.m_chunks.resize(pre.size() + 1);
    lattice.m_refs.assign(pre.size() + 1, 0);
    lattice.m_refs[0] = 1;

    size_t count = 0;
    const DWORD c_retry_count = 64; // 再試行の最大回数。

    WCHAR *dict_data1 = m_basic_dict.Lock(); // 基本辞書をロック。
    if (dict_data1) {
        // ノードを追加。
        lattice.AddNodes(0, dict_data1);

        // 最後までリンクを繰り返す。
        while (!lattice.MakeLatticeInternal(pre.size(), dict_data1)) {
            ++count;
            if (count >= c_retry_count)
                break;
        }

        m_basic_dict.Unlock(dict_data1); // 基本辞書のロックを解除。
    }

    WCHAR *dict_data2 = m_name_dict.Lock(); // 人名・地名辞書をロック。
    if (dict_data2) {
        // ノードを追加。
        lattice.AddNodes(0, dict_data2);

        // 最後までリンクを繰り返す。
        while (!lattice.MakeLatticeInternal(pre.size(), dict_data2)) {
            ++count;
            if (count >= c_retry_count)
                break;
        }

        m_name_dict.Unlock(dict_data2); // 人名・地名辞書のロックを解除。
    }

    if (count < c_retry_count)
        return TRUE; // 成功。

    // ダンプ。
    lattice.Dump(4);
    return FALSE; // 失敗。
} // MzIme::MakeLatticeForMulti

// 単一文節変換において、ラティスを作成する。
BOOL MzIme::MakeLatticeForSingle(Lattice& lattice, const std::wstring& pre)
{
    DPRINTW(L"%s\n", pre.c_str());

    ASSERT(pre.size() != 0);
    const size_t length = pre.size();

    // ラティスを初期化。
    lattice.m_pre = pre;
    lattice.m_chunks.resize(length + 1);
    lattice.m_refs.assign(length + 1, 0);
    lattice.m_refs[0] = 1;

    BOOL bOK = TRUE;

    WCHAR *dict_data1 = m_basic_dict.Lock(); // 基本辞書をロックする。
    if (dict_data1) {
        // ノード群を追加。
        if (!lattice.AddNodesForSingle(dict_data1)) {
            lattice.AddComplement(0, pre.size(), pre.size());
        }

        m_basic_dict.Unlock(dict_data1); // 基本辞書のロックを解除。
    } else {
        bOK = FALSE;
    }

    WCHAR *dict_data2 = m_name_dict.Lock(); // 人名・地名辞書をロックする。
    if (dict_data2) {
        // ノード群を追加。
        if (!lattice.AddNodesForSingle(dict_data2)) {
            lattice.AddComplement(0, pre.size(), pre.size());
        }

        m_name_dict.Unlock(dict_data2); // 人名・地名辞書のロックを解除。
    } else {
        bOK = FALSE;
    }

    if (bOK)
        return TRUE;

    // ダンプ。
    lattice.Dump(4);
    return FALSE; // 失敗。
} // MzIme::MakeLatticeForSingle

// 単一文節変換において、変換結果を生成する。
void MzIme::MakeResultForMulti(MzConvResult& result, Lattice& lattice)
{
    DPRINTW(L"%s\n", lattice.m_pre.c_str());
    result.clear(); // 結果をクリア。

    // 2文節最長一致法・改。
    const size_t length = lattice.m_pre.size();
    LatticeNodePtr node1 = lattice.m_head;
    LatticeNodePtr tail = ARRAY_AT(ARRAY_AT(lattice.m_chunks, length), 0);
    while (node1 != tail) {
        size_t kb1 = 0, max_len = 0, max_len1 = 0;
        for (size_t ib1 = 0; ib1 < node1->branches.size(); ++ib1) {
            LatticeNodePtr& node2 = ARRAY_AT(node1->branches, ib1);
            if (node2->branches.empty()) {
                size_t len = node2->pre.size();
                // (doushi or jodoushi) + jodoushi
                if ((node1->IsDoushi() || node1->IsJodoushi()) && node2->IsJodoushi()) {
                    ++len;
                    node2->cost -= 80;
                }
                // jodoushi + shuu joshi
                if (node1->IsJodoushi() && node2->bunrui == HB_SHUU_JOSHI) {
                    ++len;
                    node2->cost -= 30;
                }
                // suushi + (suushi or number unit)
                if (node1->HasTag(L"[数詞]")) {
                    if (node2->HasTag(L"[数詞]") || node2->HasTag(L"[数単位]")) {
                        ++len;
                        node2->cost -= 100;
                    }
                }
                if (max_len < len) {
                    max_len1 = node2->pre.size();
                    max_len = len;
                    kb1 = ib1;
                }
            } else {
                for (size_t ib2 = 0; ib2 < node2->branches.size(); ++ib2) {
                    LatticeNodePtr& node3 = ARRAY_AT(node2->branches, ib2);
                    size_t len = node2->pre.size() + node3->pre.size();
                    // (doushi or jodoushi) + jodoushi
                    if ((node1->IsDoushi() || node1->IsJodoushi()) && node2->IsJodoushi()) {
                        ++len;
                        node2->cost -= 80;
                    } else {
                        if ((node2->IsDoushi() || node2->IsJodoushi()) && node3->IsJodoushi()) {
                            ++len;
                            node2->cost -= 80;
                        }
                    }
                    // jodoushu + shuu joshi
                    if (node1->IsJodoushi() && node2->bunrui == HB_SHUU_JOSHI) {
                        ++len;
                        node2->cost -= 30;
                    } else {
                        if (node2->IsJodoushi() && node3->bunrui == HB_SHUU_JOSHI) {
                            ++len;
                            node2->cost -= 30;
                        }
                    }
                    // suushi + (suushi or number unit)
                    if (node1->HasTag(L"[数詞]")) {
                        if (node2->HasTag(L"[数詞]") || node2->HasTag(L"[数単位]")) {
                            ++len;
                            node2->cost -= 100;
                        }
                    } else {
                        if (node2->HasTag(L"[数詞]")) {
                            if (node3->HasTag(L"[数詞]") || node3->HasTag(L"[数単位]")) {
                                ++len;
                                node2->cost -= 100;
                            }
                        }
                    }
                    if (max_len < len) {
                        max_len1 = node2->pre.size();
                        max_len = len;
                        kb1 = ib1;
                    } else if (max_len == len) {
                        if (max_len1 < node2->pre.size()) {
                            max_len1 = node2->pre.size();
                            max_len = len;
                            kb1 = ib1;
                        }
                    }
                }
            }
        }

        // 到達できないときは打ち切る。TODO: もっとエレガントに
        if (kb1 >= node1->branches.size()) {
            break;
        }

        // add clause
        if (ARRAY_AT(node1->branches, kb1)->pre.size()) {
            MzConvClause clause;
            clause.add(ARRAY_AT(node1->branches, kb1).get());
            result.clauses.push_back(clause);
        }

        // go next
        node1 = ARRAY_AT(node1->branches, kb1);
    }

    // add other candidates
    size_t index = 0, iClause = 0;
    while (index < length && iClause < result.clauses.size()) {
        const LatticeChunk& chunk = ARRAY_AT(lattice.m_chunks, index);
        MzConvClause& clause = ARRAY_AT(result.clauses, iClause);

        std::wstring hiragana = ARRAY_AT(clause.candidates, 0).hiragana;
        const size_t size = hiragana.size();
        for (size_t i = 0; i < chunk.size(); ++i) {
            if (ARRAY_AT(chunk, i)->pre.size() == size) {
                // add a candidate of same size
                clause.add(ARRAY_AT(chunk, i).get());
            }
        }

        LatticeNode node;
        node.bunrui = HB_UNKNOWN;
        node.cost = 40; // コストは人名・地名よりも高くする。
        node.pre = lcmap(hiragana, LCMAP_HIRAGANA | LCMAP_FULLWIDTH);

        // add hiragana
        node.post = lcmap(hiragana, LCMAP_HIRAGANA | LCMAP_FULLWIDTH);
        clause.add(&node);

        // add katakana
        node.post = lcmap(hiragana, LCMAP_KATAKANA | LCMAP_FULLWIDTH);
        clause.add(&node);

        // add halfwidth katakana
        node.post = lcmap(hiragana, LCMAP_KATAKANA | LCMAP_HALFWIDTH);
        clause.add(&node);

        // add the lowercase and fullwidth
        node.post = lcmap(hiragana, LCMAP_LOWERCASE | LCMAP_FULLWIDTH);
        clause.add(&node);

        // add the uppercase and fullwidth
        node.post = lcmap(hiragana, LCMAP_UPPERCASE | LCMAP_FULLWIDTH);
        clause.add(&node);

        // add the capital and fullwidth
        node.post = node.post[0] + lcmap(hiragana.substr(1), LCMAP_LOWERCASE | LCMAP_FULLWIDTH);
        clause.add(&node);

        // add the lowercase and halfwidth
        node.post = lcmap(hiragana, LCMAP_LOWERCASE | LCMAP_HALFWIDTH);
        clause.add(&node);

        // add the uppercase and halfwidth
        node.post = lcmap(hiragana, LCMAP_UPPERCASE | LCMAP_HALFWIDTH);
        clause.add(&node);

        // add the capital and halfwidth
        node.post = node.post[0] + lcmap(hiragana.substr(1), LCMAP_LOWERCASE | LCMAP_HALFWIDTH);
        clause.add(&node);

        // go to the next clause
        index += size;
        ++iClause;
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
    node.cost = 40; // コストは人名・地名よりも高くする。
    node.bunrui = HB_MEISHI; // 名詞。

    // 文節に無変換文字列を追加。
    node.post = pre; // 変換後の文字列。
    clause.add(&node);

    // 文節にひらがなを追加。
    node.post = lcmap(pre, LCMAP_HIRAGANA); // 変換後の文字列。
    clause.add(&node);

    // 文節にカタカナを追加。
    node.post = lcmap(pre, LCMAP_KATAKANA); // 変換後の文字列。
    clause.add(&node);

    // 文節に全角を追加。
    node.post = lcmap(pre, LCMAP_FULLWIDTH); // 変換後の文字列。
    clause.add(&node);

    // 文節に半角を追加。
    node.post = lcmap(pre, LCMAP_HALFWIDTH | LCMAP_KATAKANA); // 変換後の文字列。
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
    node.cost = 40; // コストは人名・地名よりも高くする。

    // 文節に無変換文字列を追加。
    node.post = pre; // 変換後の文字列。
    clause.add(&node);

    // 文節にひらがなを追加。
    node.post = lcmap(pre, LCMAP_HIRAGANA); // 変換後の文字列。
    clause.add(&node);

    // 文節にカタカナを追加。
    node.post = lcmap(pre, LCMAP_KATAKANA); // 変換後の文字列。
    clause.add(&node);

    // 文節に全角を追加。
    node.post = lcmap(pre, LCMAP_FULLWIDTH); // 変換後の文字列。
    clause.add(&node);

    // 文節に半角を追加。
    node.post = lcmap(pre, LCMAP_HALFWIDTH | LCMAP_KATAKANA); // 変換後の文字列。
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
    std::wstring strHiragana = ARRAY_AT(comp.extra.hiragana_clauses, comp.extra.iClause);
    if (!ConvertMultiClause(strHiragana, result)) {
        return FALSE;
    }
    return StoreResult(result, comp, cand);
} // MzIme::ConvertMultiClause

// 複数文節を変換する。
BOOL MzIme::ConvertMultiClause(const std::wstring& strHiragana, MzConvResult& result)
{
    DPRINTW(L"%s\n", strHiragana.c_str());
#if 1
    // make lattice and make result
    Lattice lattice;
    std::wstring pre = lcmap(strHiragana, LCMAP_FULLWIDTH | LCMAP_HIRAGANA);
    if (MakeLatticeForMulti(lattice, pre)) {
        lattice.AddExtra();
        MakeResultForMulti(result, lattice);
    } else {
        lattice.AddExtra();
        MakeResultOnFailure(result, pre);
    }
#else
    // dummy sample
    WCHAR sz[64];
    result.clauses.clear();
    for (DWORD iClause = 0; iClause < 5; ++iClause) {
        MzConvClause clause;
        for (DWORD iCand = 0; iCand < 18; ++iCand) {
            MzConvCandidate cand;
            ::StringCchPrintfW(sz, _countof(sz), L"こうほ%u-%u", iClause, iCand);
            cand.hiragana = sz;
            ::StringCchPrintfW(sz, _countof(sz), L"候補%u-%u", iClause, iCand);
            cand.converted = sz;
            clause.candidates.push_back(cand);
        }
        result.clauses.push_back(clause);
    }
    result.clauses[0].candidates[0].hiragana = L"ひらりー";
    result.clauses[0].candidates[0].converted = L"ヒラリー";
    result.clauses[1].candidates[0].hiragana = L"とらんぷ";
    result.clauses[1].candidates[0].converted = L"トランプ";
    result.clauses[2].candidates[0].hiragana = L"さんだーす";
    result.clauses[2].candidates[0].converted = L"サンダース";
    result.clauses[3].candidates[0].hiragana = L"かたやま";
    result.clauses[3].candidates[0].converted = L"片山";
    result.clauses[4].candidates[0].hiragana = L"うちゅうじん";
    result.clauses[4].candidates[0].converted = L"宇宙人";
#endif

    return TRUE;
} // MzIme::ConvertMultiClause

// 単一文節を変換する。
BOOL MzIme::ConvertSingleClause(LogCompStr& comp, LogCandInfo& cand, BOOL bRoman)
{
    DWORD iClause = comp.extra.iClause;

    // convert
    MzConvResult result;
    std::wstring strHiragana = ARRAY_AT(comp.extra.hiragana_clauses, iClause);
    if (!ConvertSingleClause(strHiragana, result)) {
        return FALSE;
    }

    // setting composition
    result.clauses.resize(1);
    MzConvClause& clause = ARRAY_AT(result.clauses, 0);
    comp.SetClauseCompString(iClause, ARRAY_AT(clause.candidates, 0).converted);
    comp.SetClauseCompHiragana(iClause, ARRAY_AT(clause.candidates, 0).hiragana, bRoman);

    // setting cand
    LogCandList cand_list;
    for (size_t i = 0; i < clause.candidates.size(); ++i) {
        MzConvCandidate& cand = ARRAY_AT(clause.candidates, i);
        cand_list.cand_strs.push_back(cand.converted);
    }
    ARRAY_AT(cand.cand_lists, iClause) = cand_list;
    cand.iClause = iClause;

    comp.extra.iClause = iClause;

    return TRUE;
} // MzIme::ConvertSingleClause

// 単一文節を変換する。
BOOL MzIme::ConvertSingleClause(const std::wstring& strHiragana, MzConvResult& result)
{
    DPRINTW(L"%s\n", strHiragana.c_str());
    result.clear();

#if 1
    // make lattice and make result
    Lattice lattice;
    std::wstring pre = lcmap(strHiragana, LCMAP_FULLWIDTH | LCMAP_HIRAGANA);
    MakeLatticeForSingle(lattice, pre);
    lattice.AddExtra();
    MakeResultForSingle(result, lattice);
#else
    // dummy sample
    MzConvCandidate cand;
    cand.hiragana = L"たんいつぶんせつへんかん";
    cand.converted = L"単一文節変換1";
    result.candidates.push_back(cand);
    cand.hiragana = L"たんいつぶんせつへんかん";
    cand.converted = L"単一文節変換2";
    result.candidates.push_back(cand);
    cand.hiragana = L"たんいつぶんせつへんかん";
    cand.converted = L"単一文節変換3";
    result.candidates.push_back(cand);
#endif
    return TRUE;
} // MzIme::ConvertSingleClause

// 文節を左に伸縮する。
BOOL MzIme::StretchClauseLeft(LogCompStr& comp, LogCandInfo& cand, BOOL bRoman)
{
    DWORD iClause = comp.extra.iClause;

    // get the clause string
    std::wstring str1 = comp.extra.hiragana_clauses[iClause];
    if (str1.size() <= 1) return FALSE;

    // get the last character of this clause
    WCHAR ch = str1[str1.size() - 1];
    // shrink
    str1.resize(str1.size() - 1);

    // get the next clause string and add the character
    std::wstring str2;
    BOOL bSplitted = FALSE;
    if (iClause + 1 < comp.GetClauseCount()) {
        str2 = ch + comp.extra.hiragana_clauses[iClause + 1];
    } else {
        str2 += ch;
        bSplitted = TRUE;
    }

    // convert two clauses
    MzConvResult result1, result2;
    if (!ConvertSingleClause(str1, result1)) {
        return FALSE;
    }
    if (!ConvertSingleClause(str2, result2)) {
        return FALSE;
    }

    // add one clause if the clause was splitted
    if (bSplitted) {
        std::wstring str;
        comp.extra.hiragana_clauses.insert(
                comp.extra.hiragana_clauses.begin() + iClause + 1, str);
        comp.extra.comp_str_clauses.insert(
                comp.extra.comp_str_clauses.begin() + iClause + 1, str);
    }

    // seting composition
    MzConvClause& clause1 = result1.clauses[0];
    MzConvClause& clause2 = result2.clauses[0];
    comp.extra.hiragana_clauses[iClause] = str1;
    comp.extra.comp_str_clauses[iClause] = clause1.candidates[0].converted;
    comp.extra.hiragana_clauses[iClause + 1] = str2;
    comp.extra.comp_str_clauses[iClause + 1] = clause2.candidates[0].converted;
    comp.UpdateFromExtra(bRoman);

    // seting the candidate list
    {
        LogCandList cand_list;
        for (size_t i = 0; i < clause1.candidates.size(); ++i) {
            MzConvCandidate& cand = clause1.candidates[i];
            cand_list.cand_strs.push_back(cand.converted);
        }
        cand.cand_lists[iClause] = cand_list;
    }
    {
        LogCandList cand_list;
        for (size_t i = 0; i < clause2.candidates.size(); ++i) {
            MzConvCandidate& cand = clause2.candidates[i];
            cand_list.cand_strs.push_back(cand.converted);
        }
        if (bSplitted) {
            cand.cand_lists.push_back(cand_list);
        } else {
            cand.cand_lists[iClause + 1] = cand_list;
        }
    }

    // set the current clause
    cand.iClause = iClause;
    comp.extra.iClause = iClause;
    // set the clause attributes
    comp.SetClauseAttr(iClause, ATTR_TARGET_CONVERTED);

    return TRUE;
} // MzIme::StretchClauseLeft

// 文節を右に伸縮する。
BOOL MzIme::StretchClauseRight(LogCompStr& comp, LogCandInfo& cand, BOOL bRoman)
{
    DWORD iClause = comp.extra.iClause;

    // get the current clause
    std::wstring str1 = comp.extra.hiragana_clauses[iClause];
    // we cannot stretch right if the clause was the right end
    if (iClause == comp.GetClauseCount() - 1) return FALSE;

    // get the next clause
    std::wstring str2 = comp.extra.hiragana_clauses[iClause + 1];
    if (str2.empty()) return FALSE;

    // get the first character of the second clause
    WCHAR ch = str2[0];
    // add the character to the first clause
    str1 += ch;
    if (str2.size() == 1) {
        str2.clear();
    } else {
        str2 = str2.substr(1);
    }

    // convert
    MzConvResult result1, result2;
    if (!ConvertSingleClause(str1, result1)) {
        return FALSE;
    }
    if (str2.size() && !ConvertSingleClause(str2, result2)) {
        return FALSE;
    }

    MzConvClause& clause1 = result1.clauses[0];

    // if the second clause was joined?
    if (str2.empty()) {
        // delete the joined clause
        comp.extra.hiragana_clauses.erase(
                comp.extra.hiragana_clauses.begin() + iClause + 1);
        comp.extra.comp_str_clauses.erase(
                comp.extra.comp_str_clauses.begin() + iClause + 1);
        comp.extra.hiragana_clauses[iClause] = str1;
        comp.extra.comp_str_clauses[iClause] = clause1.candidates[0].converted;
    } else {
        // seting two clauses
        MzConvClause& clause2 = result2.clauses[0];
        comp.extra.hiragana_clauses[iClause] = str1;
        comp.extra.comp_str_clauses[iClause] = clause1.candidates[0].converted;
        comp.extra.hiragana_clauses[iClause + 1] = str2;
        comp.extra.comp_str_clauses[iClause + 1] = clause2.candidates[0].converted;
    }
    // update composition by extra
    comp.UpdateFromExtra(bRoman);

    // seting the candidate list
    {
        LogCandList cand_list;
        for (size_t i = 0; i < clause1.candidates.size(); ++i) {
            MzConvCandidate& cand = clause1.candidates[i];
            cand_list.cand_strs.push_back(cand.converted);
        }
        cand.cand_lists[iClause] = cand_list;
    }
    if (str2.size()) {
        MzConvClause& clause2 = result2.clauses[0];
        LogCandList cand_list;
        for (size_t i = 0; i < clause2.candidates.size(); ++i) {
            MzConvCandidate& cand = clause2.candidates[i];
            cand_list.cand_strs.push_back(cand.converted);
        }
        cand.cand_lists[iClause + 1] = cand_list;
    }

    // set the current clause
    cand.iClause = iClause;
    comp.extra.iClause = iClause;
    // set the clause attributes
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
    result.clauses.clear();
    MzConvClause clause;

    // ノードを初期化。
    LatticeNode node;
    node.pre = strTyping;
    node.bunrui = HB_UNKNOWN;
    node.cost = 0;

    // 16進を読み込み。
    ULONG hex_code = wcstoul(strTyping.c_str(), NULL, 16);

    // Unicodeのノードを文節に追加。
    WCHAR szUnicode[2];
    szUnicode[0] = WCHAR(hex_code);
    szUnicode[1] = 0;
    node.post = szUnicode; // 変換後の文字列。
    clause.add(&node);
    node.cost++; // コストを１つ加算。

    // Shift_JISコードのノードを文節に追加。
    CHAR szSJIS[8];
    WORD wSJIS = WORD(hex_code);
    if (is_sjis_code(wSJIS)) {
        szSJIS[0] = HIBYTE(wSJIS);
        szSJIS[1] = LOBYTE(wSJIS);
        szSJIS[2] = 0;
        ::MultiByteToWideChar(932, 0, szSJIS, -1, szUnicode, 2);
        node.post = szUnicode; // 変換後の文字列。
        node.cost++; // コストを１つ加算。
        clause.add(&node);
    }

    // JISコードのノードを文節に追加。
    if (is_jis_code(WORD(hex_code))) {
        wSJIS = jis2sjis(WORD(hex_code));
        if (is_sjis_code(wSJIS)) {
            szSJIS[0] = HIBYTE(wSJIS);
            szSJIS[1] = LOBYTE(wSJIS);
            szSJIS[2] = 0;
            ::MultiByteToWideChar(932, 0, szSJIS, -1, szUnicode, 2);
            node.post = szUnicode; // 変換後の文字列。
            node.cost++; // コストを１つ加算。
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
            ::MultiByteToWideChar(932, 0, szSJIS, -1, szUnicode, 2);
            node.post = szUnicode; // 変換後の文字列。
            node.cost++; // コストを１つ加算。
            clause.add(&node);
        }
    }

    // 元の入力文字列のノードを文節に追加。
    node.post = strTyping; // 変換後の文字列。
    node.cost++; // コストを１つ加算。
    clause.add(&node);

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
    comp.comp_str.clear();
    comp.extra.clear();

    // setting composition
    comp.comp_clause.resize(result.clauses.size() + 1);
    for (size_t k = 0; k < result.clauses.size(); ++k) {
        const MzConvClause& clause = result.clauses[k];
        for (size_t i = 0; i < clause.candidates.size(); ++i) {
            const MzConvCandidate& cand = clause.candidates[i];
            comp.comp_clause[k] = (DWORD)comp.comp_str.size();
            comp.extra.hiragana_clauses.push_back(cand.hiragana);
            std::wstring typing;
            typing = hiragana_to_typing(cand.hiragana);
            comp.extra.typing_clauses.push_back(typing);
            comp.comp_str += cand.converted;
            break;
        }
    }
    comp.comp_clause[result.clauses.size()] = (DWORD)comp.comp_str.size();
    comp.comp_attr.assign(comp.comp_str.size(), ATTR_CONVERTED);
    comp.extra.iClause = 0;
    comp.SetClauseAttr(comp.extra.iClause, ATTR_TARGET_CONVERTED);
    comp.dwCursorPos = (DWORD)comp.comp_str.size();
    comp.dwDeltaStart = 0;

    // setting cand
    cand.clear();
    for (size_t k = 0; k < result.clauses.size(); ++k) {
        const MzConvClause& clause = result.clauses[k];
        LogCandList cand_list;
        for (size_t i = 0; i < clause.candidates.size(); ++i) {
            const MzConvCandidate& cand = clause.candidates[i];
            cand_list.cand_strs.push_back(cand.converted);
        }
        cand.cand_lists.push_back(cand_list);
    }
    cand.iClause = 0;
    return TRUE;
} // MzIme::StoreResult

//////////////////////////////////////////////////////////////////////////////
