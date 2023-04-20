// tests.cpp --- mzimeja のテスト。
//////////////////////////////////////////////////////////////////////////////

#include "mzimeja.h"
#include <shlobj.h>
#include <strsafe.h>
#include <clocale>
#include "resource.h"

//////////////////////////////////////////////////////////////////////////////

// ローカルファイルを検索する。
LPCTSTR findLocalFile(LPCTSTR name)
{
    TCHAR szDir[MAX_PATH];
    ::GetModuleFileName(NULL, szDir, _countof(szDir));
    ::PathRemoveFileSpec(szDir);

    static TCHAR s_szPath[MAX_PATH];
    StringCchCopy(s_szPath, _countof(s_szPath), szDir);
    ::PathAppend(s_szPath, name);
    if (::PathFileExists(s_szPath))
        return s_szPath;

    StringCchCopy(s_szPath, _countof(s_szPath), szDir);
    ::PathAppend(s_szPath, TEXT(".."));
    ::PathAppend(s_szPath, name);
    if (::PathFileExists(s_szPath))
        return s_szPath;

    StringCchCopy(s_szPath, _countof(s_szPath), szDir);
    ::PathAppend(s_szPath, TEXT(".."));
    ::PathAppend(s_szPath, TEXT(".."));
    ::PathAppend(s_szPath, name);
    if (::PathFileExists(s_szPath))
        return s_szPath;

    ASSERT(0);
    return NULL;
}

// テストエントリーを処理する。
void DoEntry(const std::wstring& pre, LPCWSTR post = NULL)
{
    MzConvResult result;
    TheIME.ConvertMultiClause(pre, result);
    auto got = result.get_str();
    printf("%ls\n\n", got.c_str());
    if (post)
    {
        if (got != post)
        {
            printf("%ls\n\n", result.get_str(true).c_str());
            ASSERT(0);
        }
    }
    else
    {
        printf("%ls\n\n", result.get_str(true).c_str());
    }
}

// 動詞のテスト。
void DoDoushi(void)
{
    DoEntry(L"よせる。よせない。よせるとき。よせれば。よせろよ。よせてよ。");

    DoEntry(L"たべる。たべない。たべます。たべた。たべるとき。たべれば。たべろ。たべよう。",
            L"食べる|。|食べ|ない|。|食べ|ます|。|食べ|た|。|食べる|とき|。|食べれ|ば|。|食べろ|。|食べよう|。");

    DoEntry(L"かきます。かいて。かかない。かく。かいた。かける。かこう。",
            L"書き|ます|。|書いて|。|書か|ない|。|書く|。|書いた|。|書ける|。|書こう|。");
    DoEntry(L"かけ。かくな。かけば。かかれる。かかせる。かかせられる。",
            L"書け|。|書くな|。|書け|ば|。|書か|れる|。|書か|せる|。|書かせ|られる|。");

    DoEntry(L"みます。みて。みない。みる。みた。みられる。みよう。",
            L"見|ます|。|見て|。|見ない|。|見る|。|見た|。|見られる|。|見よう|。");
    DoEntry(L"みろ。みるな。みれば。みられる。みさせる。みさせられる。",
            L"見ろ|。|見るな|。|見れ|ば|。|見られる|。|見させる|。|見させ|られる|。");

    DoEntry(L"やってみます。やってみて。やってみない。やってみる。やってみた。やってみられる。やってみよう。");
    DoEntry(L"やってみろ。やってみるな。やってみれば。やってみられる。やってみさせる。やってみさせられる。");

    DoEntry(L"きます。きて。こない。くる。きた。こられる。こよう。");
    DoEntry(L"こい。くるな。くれば。こられる。こさせる。こさせられる。",
            L"来い|。|来るな|。|来れ|ば|。|来られる|。|来させる|。|来させ|られる|。");

    DoEntry(L"やってきます。やってきて。やってこない。やってくる。やってきた。やってこられる。やってこよう。");
    DoEntry(L"やってこい。やってくるな。やってくれば。やってこられる。やってこさせる。やってこさせられる。");

    DoEntry(L"かいてんします。かいてんして。かいてんしない。");
    DoEntry(L"かいてんする。かいてんした。かいてんできる。かいてんしよう。");
    DoEntry(L"かいてんしろ。かいてんするな。かいてんすれば。");
    DoEntry(L"かいてんされる。かいてんさせる。かいてんさせられる。",
            L"回転|される|。|回転|させる|。|回転|させ|られる|。");
}

// 形容詞のテスト。
void DoKeiyoushi(void)
{
    DoEntry(L"すくない。すくなかろう。すくなかった。すくなく。すくなければ。",
            L"少ない|。|少なかろう|。|少なかった|。|少なく|。|少なければ|。");
    DoEntry(L"ただしい。ただしかろう。ただしかった。ただしく。ただしければ。",
            L"正しい|。|正しかろう|。|正しかった|。|正しく|。|正しければ|。");

    DoEntry(L"ゆたかだ。ゆたかだろう。ゆたかだった。ゆたかで。ゆたかに。ゆたかなこと。ゆたかならば。",
            L"豊かだ|。|豊かだろう|。|豊かだった|。|豊かで|。|豊かに|。|豊かな|こと|。|豊かならば|。");
}

// フレーズのテスト。
void DoPhrases(void)
{
    DoEntry(L"かのじょはにほんごがおじょうずですね。",
            L"彼女|は|日本語|が|お上手|ですね|。");
    DoEntry(L"わたしはしゅうきょうじょうのりゆうでおにくがたべられません。",
            L"私|は|宗教|上|の|理由|で|お肉|が|食べ|られ|ません|。");
    DoEntry(L"そこではなしはおわりになった", L"そこで|話|は|終わり|に|なった");
    DoEntry(L"わたしがわたしたわたをわたがしみたいにたべないでくださいませんか");
    DoEntry(L"あんた、そこにあいはあるんかいな");
}

// mzimejaのテスト。
void IME_Test1(void)
{
    DoEntry(L"てすとです", L"テスト|です");

    DoDoushi();
    DoKeiyoushi();
    DoPhrases();
}

BOOL OnOK(HWND hwnd)
{
    WCHAR szText[1024];
    GetDlgItemTextW(hwnd, edt1, szText, _countof(szText));
    StrTrimW(szText, L" \t\r\n");
    if (szText[0] == 0) {
        MessageBoxW(hwnd, L"空ではない文字列を入力して下さい", NULL, 0);
        return FALSE;
    }
    DoEntry(szText);
    return TRUE;
}

static INT_PTR CALLBACK
InputDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            if (OnOK(hwnd)) {
                EndDialog(hwnd, IDOK);
            }
            break;
        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            break;
        }
    }
    return 0;
}

void IME_Test2(void)
{
    while (::DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_INPUTBOX),
                       NULL, InputDialogProc) == IDOK)
    {
        ;
    }
}

// Unicode版のmain関数。
int wmain(int argc, wchar_t **argv)
{
    // Unicode出力を可能に。
    std::setlocale(LC_CTYPE, "");

    LPCTSTR pathname = findLocalFile(L"res\\mzimeja.dic");
    //LPCTSTR pathname = findLocalFile(L"res\\testdata.dic");
    if (!g_basic_dict.Load(pathname, L"BasicDictObject")) {
        ASSERT(0);
        return 1;
    }

    // テスト1。
    IME_Test1();

    // テスト2。
    IME_Test2();

    g_basic_dict.Unload();

    return 0;
}

// 古いコンパイラのサポートのため。
int main(void)
{
    int argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    int ret = wmain(argc, argv);
    LocalFree(argv);
    return ret;
}
