// imepad.cpp --- mzimeja IME Pad UI
//////////////////////////////////////////////////////////////////////////////

#define _CRT_SECURE_NO_WARNINGS   // use fopen
#include "../targetver.h"   // target Windows version

#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <tchar.h>          // for Windows generic text

#include <string>           // for std::string, std::wstring, ...
#include <vector>           // for std::vector
#include <set>              // for std::set
#include <map>              // for std::map
#include <algorithm>        // for std::sort

#include <cstdlib>          // for C standard library
#include <cstdio>           // for C standard I/O
#include <cctype>           // for C character types
#include <cassert>          // for assert
#include <cstring>          // for C string

#include <strsafe.h>        // StringC...

#include "Wow64.h"
#include "../str.hpp"       // for str_*

#include "resource.h"

//////////////////////////////////////////////////////////////////////////////

#define INITIAL_WIDTH 380
#define INITIAL_HEIGHT 250
#define INTERVAL_MILLISECONDS   300

#define SMALL_FONT_SIZE 12
#define LARGE_FONT_SIZE 26
#define STROKES_WIDTH 90
#define STROKES_HEIGHT (LARGE_FONT_SIZE + 4)
#define RADICAL_WIDTH 140
#define RADICAL_SIZE 24
#define CHAR_BOX_SIZE (LARGE_FONT_SIZE + 6)
#define IDW_LISTBOX1 2
#define IDW_LISTBOX2 3

struct KANJI_ENTRY {
    WORD kanji_id;
    WCHAR kanji_char;
    WORD radical_id2;
    WORD strokes;
    std::wstring readings;
    KANJI_ENTRY()
    {
        kanji_id = 0;
        kanji_char = 0;
        radical_id2 = 0;
        strokes = 0;
    }
};

struct RADICAL_ENTRY {
    WORD radical_id;
    WORD radical_id2;
    WORD strokes;
    std::wstring readings;
    RADICAL_ENTRY()
    {
        radical_id = 0;
        radical_id2 = 0;
        strokes = 0;
    }
};

const WCHAR szImePadClassName[] = L"MZIMEPad";

//////////////////////////////////////////////////////////////////////////////
// IME Pad

class ImePad {
public:
    ImePad();
    ~ImePad();

    BOOL PrepareForKanji();
    static BOOL Create(HWND hwndParent);
    static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);

protected:
    HWND m_hWnd;

    // data
    std::vector<KANJI_ENTRY>            m_kanji_table;
    std::map<WORD, std::vector<WORD> >  m_kanji_stroke_map;
    std::vector<RADICAL_ENTRY>          m_radical_table;
    std::map<WORD, std::vector<WORD> >  m_radical_stroke_map;
    std::map<WORD, WORD>                m_radical_id_map;
    std::map<WORD, std::vector<WORD> >  m_radical2_to_kanji_map;
    BOOL LoadKanjiData();
    BOOL LoadRadicalData();
    BOOL LoadKanjiAndRadical();

    // UI
    HWND m_hTabCtrl;
    HWND m_hListView;
    HWND m_hListBox1;
    HWND m_hListBox2;
    HWND m_hwndLastActive;
    BOOL m_bInSizing;
    WNDPROC m_fnListBox1OldWndProc;
    WNDPROC m_fnListBox2OldWndProc;
    static LRESULT CALLBACK ListBox1WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK ListBox2WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void MySendInput(WCHAR ch);

    // images
    HIMAGELIST m_himlKanji;
    HIMAGELIST m_himlRadical;
    HBITMAP m_hbmRadical;
    BOOL LoadRadicalImage();
    BOOL CreateKanjiImageList();
    BOOL CreateRadicalImageList();
    void DeleteAllImages();

    // fonts
    HFONT m_hSmallFont;
    HFONT m_hLargeFont;
    BOOL CreateAllFonts();
    void DeleteAllFonts();

    BOOL OnCreate(HWND hWnd);
    void OnSize(HWND hWnd);
    void OnCommand(HWND hWnd, WPARAM wParam, LPARAM lParam);
    void OnDestroy(HWND hWnd);
    void OnNotify(HWND hWnd, WPARAM wParam, LPARAM lParam);
    void OnDrawItemListBox1(HWND hWnd, LPDRAWITEMSTRUCT lpDraw);
    void OnDrawItemListBox2(HWND hWnd, LPDRAWITEMSTRUCT lpDraw);
    void OnLB1StrokesChanged(HWND hWnd);
    void OnLB2StrokesChanged(HWND hWnd);
    void OnTimer(HWND hWnd);
    void OnGetMinMaxInfo(LPMINMAXINFO pmmi);
}; // class ImePad

//////////////////////////////////////////////////////////////////////////////

HINSTANCE g_hInst;

//////////////////////////////////////////////////////////////////////////////

LPWSTR LoadStringDx(INT nStringID) {
    static WCHAR s_sz[MAX_PATH * 2];
    s_sz[0] = L'\0';
    ::LoadStringW(g_hInst, nStringID, s_sz, MAX_PATH * 2);
    return s_sz;
}

HBITMAP LoadBitmapDx(LPCWSTR pszName) {
    assert(g_hInst);
    return ::LoadBitmap(g_hInst, pszName);
}

HBITMAP LoadBitmapDx(INT nBitmapID) {
    return LoadBitmapDx(MAKEINTRESOURCEW(nBitmapID));
}

static const WCHAR s_szRegKey[] =
        L"SOFTWARE\\Katayama Hirofumi MZ\\mzimeja";

static std::wstring GetSettingString(LPCWSTR pszSettingName) {
    HKEY hKey;
    LONG result;
    WCHAR szValue[MAX_PATH * 2];
    result = ::RegOpenKeyExW(HKEY_CURRENT_USER, s_szRegKey,
                             0, KEY_READ | KEY_WOW64_64KEY, &hKey);
    if (result != ERROR_SUCCESS) {
        result = ::RegOpenKeyExW(HKEY_CURRENT_USER, s_szRegKey,
                                 0, KEY_READ, &hKey);
    }
    if (result == ERROR_SUCCESS && hKey) {
        DWORD cbData = sizeof(szValue);
        result = ::RegQueryValueExW(hKey, pszSettingName, NULL, NULL,
                                    reinterpret_cast<LPBYTE>(szValue), &cbData);
        ::RegCloseKey(hKey);
        if (result == ERROR_SUCCESS) {
            return std::wstring(szValue);
        }
    }
    return L"";
} // GetSettingString

// adjust window position
static void RepositionWindow(HWND hWnd) {
    RECT rc, rcWorkArea;
    ::GetWindowRect(hWnd, &rc);
    ::SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWorkArea, FALSE);
    SIZE siz;
    siz.cx = rc.right - rc.left;
    siz.cy = rc.bottom - rc.top;
    if (rc.right > rcWorkArea.right) {
        rc.right = rcWorkArea.right;
        rc.left = rcWorkArea.right - siz.cx;
    }
    if (rc.left < rcWorkArea.left) {
        rc.left = rcWorkArea.left;
        rc.right = rc.left + siz.cx;
    }
    if (rc.bottom > rcWorkArea.bottom) {
        rc.bottom = rcWorkArea.bottom;
        rc.top = rcWorkArea.bottom - siz.cy;
    }
    if (rc.top < rcWorkArea.top) {
        rc.top = rcWorkArea.top;
        rc.bottom = rc.top + siz.cy;
    }
    ::MoveWindow(hWnd, rc.left, rc.top, siz.cx, siz.cy, TRUE);
}

static HBITMAP Create24BppBitmap(HDC hDC, INT cx, INT cy) {
    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = cx;
    bi.bmiHeader.biHeight = cy;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 24;

    LPVOID pvBits;
    return ::CreateDIBSection(hDC, &bi, DIB_RGB_COLORS, &pvBits, NULL, 0);
}

inline bool
radical_compare(const RADICAL_ENTRY& entry1, const RADICAL_ENTRY& entry2) {
    return entry1.strokes < entry2.strokes;
}

LRESULT CALLBACK ImePad::ListBox1WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    ImePad* pThis = (ImePad*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (!pThis)
        return DefWindowProc(hWnd, uMsg, wParam, lParam);

    if (uMsg == WM_ERASEBKGND)
        return TRUE;

    return CallWindowProc(pThis->m_fnListBox1OldWndProc, hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK ImePad::ListBox2WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    ImePad* pThis = (ImePad*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (!pThis)
        return DefWindowProc(hWnd, uMsg, wParam, lParam);

    if (uMsg == WM_ERASEBKGND)
        return TRUE;

    return CallWindowProc(pThis->m_fnListBox2OldWndProc, hWnd, uMsg, wParam, lParam);
}

void ImePad::OnDrawItemListBox1(HWND hWnd, LPDRAWITEMSTRUCT lpDraw) {
    // Double buffering
    HDC hdcMem = CreateCompatibleDC(lpDraw->hDC);
    RECT rc = lpDraw->rcItem;
    OffsetRect(&rc, -rc.left, -rc.top);
    HBITMAP hbmMem = CreateCompatibleBitmap(lpDraw->hDC, rc.right - rc.left, rc.bottom - rc.top);
    HGDIOBJ hbmOld = SelectObject(hdcMem, hbmMem);
    HGDIOBJ hFontOld = SelectObject(hdcMem, m_hLargeFont);

    INT id = lpDraw->itemID;
	WCHAR text[128] = L"(No data)";
	SendMessage(lpDraw->hwndItem, LB_GETTEXT, id, (LPARAM)text);
    if (lpDraw->itemState & ODS_SELECTED) {
        ::FillRect(hdcMem, &rc, (HBRUSH)(COLOR_HIGHLIGHT + 1));
        ::SetBkMode(hdcMem, TRANSPARENT);
        ::SetTextColor(hdcMem, GetSysColor(COLOR_HIGHLIGHTTEXT));
        ::DrawText(hdcMem, text, -1,
                   &rc, DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_NOCLIP | DT_NOPREFIX | DT_END_ELLIPSIS);
    } else {
        ::FillRect(hdcMem, &rc, (HBRUSH)(COLOR_WINDOW + 1));
        ::SetBkMode(hdcMem, TRANSPARENT);
        ::SetTextColor(hdcMem, GetSysColor(COLOR_WINDOWTEXT));
        ::DrawText(hdcMem, text, -1,
                   &rc, DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_NOCLIP | DT_NOPREFIX | DT_END_ELLIPSIS);
    }

    rc = lpDraw->rcItem;
    BitBlt(lpDraw->hDC, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, hdcMem, 0, 0, SRCCOPY);

    SelectObject(hdcMem, hFontOld);
    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
}

void ImePad::OnDrawItemListBox2(HWND hWnd, LPDRAWITEMSTRUCT lpDraw) {
    // Double buffering
    HDC hdcMem = CreateCompatibleDC(lpDraw->hDC);
    RECT rc = lpDraw->rcItem;
    OffsetRect(&rc, -rc.left, -rc.top);
    HBITMAP hbmMem = CreateCompatibleBitmap(lpDraw->hDC, rc.right - rc.left, rc.bottom - rc.top);
    HGDIOBJ hbmOld = SelectObject(hdcMem, hbmMem);
    HGDIOBJ hFontOld = SelectObject(hdcMem, m_hSmallFont);

    INT id = lpDraw->itemID;
    const RADICAL_ENTRY& entry = m_radical_table[id];
    if (lpDraw->itemState & ODS_SELECTED) {
        ::FillRect(hdcMem, &rc, (HBRUSH)(COLOR_HIGHLIGHT + 1));
        ImageList_Draw(m_himlRadical, entry.radical_id - 1, hdcMem, 0, 0, ILD_NORMAL);
        ::SelectObject(hdcMem, ::GetStockObject(BLACK_PEN));
        ::SelectObject(hdcMem, ::GetStockObject(NULL_BRUSH));
        ::Rectangle(hdcMem, 0, 0, RADICAL_SIZE, RADICAL_SIZE);
        rc.left += RADICAL_SIZE + 2;
        ::SetBkMode(hdcMem, TRANSPARENT);
        ::SetTextColor(hdcMem, GetSysColor(COLOR_HIGHLIGHTTEXT));
        ::DrawText(hdcMem, entry.readings.c_str(), -1,
                   &rc, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOCLIP | DT_NOPREFIX | DT_END_ELLIPSIS);
    } else {
        ::FillRect(hdcMem, &rc, (HBRUSH)(COLOR_WINDOW + 1));
        ImageList_Draw(m_himlRadical, entry.radical_id - 1, hdcMem, 0, 0, ILD_NORMAL);
        ::SelectObject(hdcMem, ::GetStockObject(BLACK_PEN));
        ::SelectObject(hdcMem, ::GetStockObject(NULL_BRUSH));
        ::Rectangle(hdcMem, 0, 0, RADICAL_SIZE, RADICAL_SIZE);
        rc.left += RADICAL_SIZE + 2;
        ::SetBkMode(hdcMem, TRANSPARENT);
        ::SetTextColor(hdcMem, GetSysColor(COLOR_WINDOWTEXT));
        ::DrawText(hdcMem, entry.readings.c_str(), -1,
                   &rc, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOCLIP | DT_NOPREFIX | DT_END_ELLIPSIS);
    }

    rc = lpDraw->rcItem;
    BitBlt(lpDraw->hDC, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, hdcMem, 0, 0, SRCCOPY);

    SelectObject(hdcMem, hFontOld);
    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
}

//////////////////////////////////////////////////////////////////////////////
// create/destroy

/*static*/ BOOL ImePad::Create(HWND hwndParent) {
    RECT rc;
    ::SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);

    rc.left = rc.right - INITIAL_WIDTH - 32;
    rc.top = rc.bottom - INITIAL_HEIGHT - 32;

    DWORD style = WS_CAPTION | WS_SYSMENU | WS_SIZEBOX | WS_ACTIVECAPTION | WS_CLIPCHILDREN;
    DWORD exstyle =
        WS_EX_WINDOWEDGE |
        WS_EX_TOPMOST |
        WS_EX_NOACTIVATE;

    HWND hImePad = ::CreateWindowEx(
            exstyle, szImePadClassName, LoadStringDx(IDM_IME_PAD),
            style, rc.left, rc.top, INITIAL_WIDTH, INITIAL_HEIGHT,
            hwndParent, NULL, g_hInst, NULL);
    assert(hImePad);

    RepositionWindow(hImePad);
    ::ShowWindow(hImePad, SW_SHOWNA);

    return hImePad != NULL;
} // ImePad_Create

ImePad::ImePad() {
    m_hWnd = NULL;
    m_himlKanji = NULL;
    m_himlRadical = NULL;
    m_hSmallFont = NULL;
    m_hLargeFont = NULL;
    m_hbmRadical = NULL;
    m_hwndLastActive = NULL;
    m_bInSizing = FALSE;
    m_fnListBox1OldWndProc = NULL;
    m_fnListBox2OldWndProc = NULL;
    m_hTabCtrl = NULL;
    m_hListView = NULL;
}

ImePad::~ImePad() {
    DeleteAllImages();
    DeleteAllFonts();
}

//////////////////////////////////////////////////////////////////////////////
// loading res/kanji.dat and res/radical.dat

BOOL FindLocalFile(std::wstring& path, LPCWSTR filename) {
    WCHAR szPath[MAX_PATH];
    ::GetModuleFileNameW(NULL, szPath, MAX_PATH);
    PathRemoveFileSpecW(szPath);

    for (INT i = 0; i < 5; ++i) {
        size_t ich = wcslen(szPath);
        {
            PathAppendW(szPath, filename);
            if (PathFileExistsW(szPath)) {
                path = szPath;
                return TRUE;
            }
        }
        szPath[ich] = 0;
        {
            PathAppendW(szPath, L"mzimeja");
            PathAppendW(szPath, filename);
            if (PathFileExistsW(szPath)) {
                path = szPath;
                return TRUE;
            }
        }
        szPath[ich] = 0;
        PathRemoveFileSpecW(szPath);
    }

    path.clear();
    return NULL;
}

LPWSTR GetKanjiDataPathName(LPWSTR pszPath) {
    std::wstring path;
    if (FindLocalFile(path, L"kanji.dat") || FindLocalFile(path, L"res\\kanji.dat"))
        return lstrcpynW(pszPath, path.c_str(), MAX_PATH);
    return NULL;
}

LPWSTR GetRadicalDataPathName(LPWSTR pszPath) {
    std::wstring path;
    if (FindLocalFile(path, L"radical.dat") || FindLocalFile(path, L"res\\radical.dat"))
        return lstrcpynW(pszPath, path.c_str(), MAX_PATH);
    return NULL;
}

BOOL ImePad::LoadKanjiData() {
    if (m_kanji_table.size()) {
        return TRUE;
    }
    char buf[256];
    wchar_t wbuf[256];
    WCHAR szPath[MAX_PATH];
    GetKanjiDataPathName(szPath);
    FILE *fp = _wfopen(szPath, L"rb");
    if (!fp) {
        std::wstring kanji_file = GetSettingString(L"KanjiDataFile");
        fp = _wfopen(kanji_file.c_str(), L"rb");
    }
    if (fp) {
        KANJI_ENTRY entry;
        while (fgets(buf, 256, fp) != NULL) {
            if (buf[0] == ';') continue;
            ::MultiByteToWideChar(CP_UTF8, 0, buf, -1, wbuf, 256);
            std::wstring str = wbuf;
            str_trim_right(str, L"\r\n");
            std::vector<std::wstring> vec;
            str_split(vec, str, L"\t");
            entry.kanji_id = _wtoi(vec[0].c_str());
            entry.kanji_char = vec[1][0];
            entry.radical_id2 = _wtoi(vec[2].c_str());
            entry.strokes = _wtoi(vec[3].c_str());
            entry.readings = vec[4];
            m_kanji_table.push_back(entry);
            m_kanji_stroke_map[entry.strokes].push_back(entry.kanji_id);
        }
        fclose(fp);
        return TRUE;
    }
    assert(0);
    return FALSE;
} // ImePad::LoadKanjiData

BOOL ImePad::LoadRadicalData() {
    if (m_radical_table.size()) {
        return TRUE;
    }
    char buf[256];
    wchar_t wbuf[256];
    WCHAR szPath[MAX_PATH];
    GetRadicalDataPathName(szPath);
    FILE *fp = _wfopen(szPath, L"rb");
    if (!fp) {
        std::wstring radical_file = GetSettingString(L"RadicalDataFile");
        fp = _wfopen(radical_file.c_str(), L"rb");
    }
    if (fp) {
        RADICAL_ENTRY entry;
        while (fgets(buf, 256, fp) != NULL) {
            if (buf[0] == ';') continue;
            ::MultiByteToWideChar(CP_UTF8, 0, buf, -1, wbuf, 256);
            std::wstring str = wbuf;
            str_trim_right(str, L"\r\n");
            std::vector<std::wstring> vec;
            str_split(vec, str, L"\t");
            entry.radical_id = _wtoi(vec[0].c_str());
            entry.radical_id2 = _wtoi(vec[1].c_str());
            entry.strokes = _wtoi(vec[3].c_str());
            entry.readings = vec[4];
            m_radical_table.push_back(entry);
            m_radical_stroke_map[entry.strokes].push_back(entry.radical_id);
            m_radical_id_map[entry.radical_id] = entry.radical_id2;
        }
        fclose(fp);
        return TRUE;
    }
    assert(0);
    return FALSE;
} // ImePad::LoadRadicalData

BOOL ImePad::LoadKanjiAndRadical() {
    if (LoadKanjiData() && LoadRadicalData()) {
        if (m_radical2_to_kanji_map.size()) {
            return TRUE;
        }

        for (size_t i = 0; i < m_kanji_table.size(); ++i) {
            const KANJI_ENTRY& kanji = m_kanji_table[i];
            m_radical2_to_kanji_map[kanji.radical_id2].push_back(kanji.kanji_id);
        }
        std::sort(m_radical_table.begin(), m_radical_table.end(),
                  radical_compare);
        return TRUE;
    }
    assert(0);
    return FALSE;
}

void ImePad::DeleteAllImages() {
    if (m_hbmRadical) {
        ::DeleteObject(m_hbmRadical);
        m_hbmRadical = NULL;
    }
    if (m_himlKanji) {
        ImageList_Destroy(m_himlKanji);
        m_himlKanji = NULL;
    }
    if (m_himlRadical) {
        ImageList_Destroy(m_himlRadical);
        m_himlRadical = NULL;
    }
}

void ImePad::DeleteAllFonts() {
    if (m_hSmallFont) {
        ::DeleteObject(m_hSmallFont);
        m_hSmallFont = NULL;
    }
    if (m_hLargeFont) {
        ::DeleteObject(m_hLargeFont);
        m_hLargeFont = NULL;
    }
}

BOOL ImePad::CreateAllFonts() {
    if (m_hSmallFont) {
        DeleteAllFonts();
    }

    LOGFONT lf;
    ZeroMemory(&lf, sizeof(lf));
    lf.lfCharSet = SHIFTJIS_CHARSET;
    lf.lfQuality = PROOF_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;

    lf.lfHeight = SMALL_FONT_SIZE;
    m_hSmallFont = ::CreateFontIndirect(&lf);
    lf.lfHeight = LARGE_FONT_SIZE;
    m_hLargeFont = ::CreateFontIndirect(&lf);
    if (m_hSmallFont && m_hLargeFont) {
        return TRUE;
    }
    assert(0);
    return FALSE;
} // ImePad::CreateAllFonts

BOOL ImePad::LoadRadicalImage() {
    if (m_hbmRadical == NULL) {
        m_hbmRadical = LoadBitmapDx(1);
    }
    assert(m_hbmRadical);
    return m_hbmRadical != NULL;
}

BOOL ImePad::CreateKanjiImageList() {
    if (!LoadKanjiAndRadical()) {
        return FALSE;
    }

    if (m_himlKanji) {
        ImageList_Destroy(m_himlKanji);
        m_himlKanji = NULL;
    }

    m_himlKanji = ImageList_Create(CHAR_BOX_SIZE, CHAR_BOX_SIZE, ILC_COLOR,
                                   (INT)m_kanji_table.size(), 1);
    if (m_himlKanji == NULL) {
        return FALSE;
    }

    HDC hDC = ::CreateCompatibleDC(NULL);
    ::SelectObject(hDC, GetStockObject(WHITE_BRUSH));
    ::SelectObject(hDC, GetStockObject(BLACK_PEN));
    HGDIOBJ hFontOld = ::SelectObject(hDC, m_hLargeFont);
    for (size_t i = 0; i < m_kanji_table.size(); ++i) {
        HBITMAP hbm = Create24BppBitmap(hDC, CHAR_BOX_SIZE, CHAR_BOX_SIZE);
        HGDIOBJ hbmOld = ::SelectObject(hDC, hbm);
        {
            ::Rectangle(hDC, 0, 0, CHAR_BOX_SIZE, CHAR_BOX_SIZE);
            RECT rc;
            ::SetRect(&rc, 0, 0, CHAR_BOX_SIZE, CHAR_BOX_SIZE);
            ::DrawTextW(hDC, &m_kanji_table[i].kanji_char, 1, &rc,
                        DT_CENTER | DT_NOCLIP | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);
        }
        ::SelectObject(hDC, hbmOld);
        ImageList_Add(m_himlKanji, hbm, NULL);
        ::DeleteObject(hbm);
    }
    ::SelectObject(hDC, hFontOld);

    return TRUE;
} // ImePad::CreateKanjiImageList

BOOL ImePad::CreateRadicalImageList() {
    if (!LoadKanjiAndRadical() || !LoadRadicalImage()) {
        return FALSE;
    }

    if (m_himlRadical) {
        ImageList_Destroy(m_himlRadical);
    }
    m_himlRadical = ImageList_Create(RADICAL_SIZE, RADICAL_SIZE, ILC_COLOR,
                                     (INT)m_radical_table.size(), 1);
    if (m_himlRadical == NULL) {
        return FALSE;
    }

    assert(m_hbmRadical);
    HDC hDC = ::CreateCompatibleDC(NULL);
    HDC hDC2 = ::CreateCompatibleDC(NULL);
    HGDIOBJ hbm2Old = ::SelectObject(hDC2, m_hbmRadical);
    for (size_t i = 0; i < m_radical_table.size(); ++i) {
        HBITMAP hbm = Create24BppBitmap(hDC, RADICAL_SIZE, RADICAL_SIZE);
        HGDIOBJ hbmOld = ::SelectObject(hDC, hbm);
        {
            ::BitBlt(hDC, 0, 0, RADICAL_SIZE, RADICAL_SIZE, hDC2, INT(i * RADICAL_SIZE), 0, SRCCOPY);
        }
        ::SelectObject(hDC, hbmOld);
        ImageList_Add(m_himlRadical, hbm, NULL);
        ::DeleteObject(hbm);
    }
    ::SelectObject(hDC2, hbm2Old);
    ::DeleteDC(hDC2);
    ::DeleteDC(hDC);

    return TRUE;
} // ImePad::CreateRadicalImageList

BOOL ImePad::PrepareForKanji() {
    if (CreateAllFonts() &&
        CreateKanjiImageList() &&
        CreateRadicalImageList())
    {
        return TRUE;
    }
    return FALSE;
}

BOOL ImePad::OnCreate(HWND hWnd) {
    if (!PrepareForKanji()) {
        return FALSE;
    }

    ::SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) this);

    RECT rc;
    ::GetClientRect(hWnd, &rc);

    DWORD style, exstyle;

    // create tab control
    style = WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | TCS_FOCUSNEVER | WS_CLIPCHILDREN;
    exstyle = WS_EX_NOACTIVATE;
    m_hTabCtrl = ::CreateWindowEx(exstyle, WC_TABCONTROL, NULL, style,
                                  0, 0, rc.right - rc.left, rc.bottom - rc.top,
                                  hWnd, (HMENU)1, g_hInst, NULL);

    ::SetWindowLongPtr(m_hTabCtrl, GWLP_USERDATA, (LONG_PTR) this);

    // initialize tab control
    TC_ITEM item;
    item.mask = TCIF_TEXT;
    item.pszText = LoadStringDx(IDM_STROKES);
    TabCtrl_InsertItem(m_hTabCtrl, 0, &item);
    item.pszText = LoadStringDx(IDM_RADICALS);
    TabCtrl_InsertItem(m_hTabCtrl, 1, &item);

    // get inner area
    TabCtrl_AdjustRect(m_hTabCtrl, FALSE, &rc);

    // create list box (strokes)
    style = WS_CHILD | WS_VSCROLL | LBS_OWNERDRAWFIXED | LBS_NOTIFY | WS_CLIPSIBLINGS | LBS_HASSTRINGS;
    exstyle = WS_EX_NOACTIVATE | WS_EX_CLIENTEDGE;
    m_hListBox1 = ::CreateWindowEx(exstyle, TEXT("LISTBOX"), NULL, style,
                                   rc.left, rc.top, STROKES_WIDTH, rc.bottom - rc.top,
                                   hWnd, (HMENU)IDW_LISTBOX1, g_hInst, NULL);
    SendMessage(m_hListBox1, LB_SETITEMHEIGHT, 0, STROKES_HEIGHT);

    // create list box (radicals)
    style = WS_CHILD | WS_VSCROLL | LBS_OWNERDRAWFIXED | LBS_NOTIFY | WS_CLIPSIBLINGS;
    exstyle = WS_EX_NOACTIVATE | WS_EX_CLIENTEDGE;
    m_hListBox2 = ::CreateWindowEx(exstyle, TEXT("LISTBOX"), NULL, style,
                                   rc.left, rc.top, RADICAL_WIDTH, rc.bottom - rc.top,
                                   hWnd, (HMENU)IDW_LISTBOX2, g_hInst, NULL);
    SendMessage(m_hListBox2, LB_SETITEMHEIGHT, 0, RADICAL_SIZE);

    // create list view
    style = WS_CHILD | LVS_ICON | WS_CLIPSIBLINGS;
    exstyle = WS_EX_NOACTIVATE | WS_EX_CLIENTEDGE;
    m_hListView = ::CreateWindowEx(exstyle, WC_LISTVIEW, NULL, style,
                                   rc.left, rc.top, 120, rc.bottom - rc.top,
                                   hWnd, (HMENU)4, g_hInst, NULL);

    // Less flickering
#ifndef LVS_EX_DOUBLEBUFFER
    #define LVS_EX_DOUBLEBUFFER 0x10000
#endif
    ListView_SetExtendedListViewStyleEx(m_hListView, LVS_EX_DOUBLEBUFFER, LVS_EX_DOUBLEBUFFER);

    // insert items to for strokes
    std::set<WORD> strokes;
    for (size_t i = 0; i < m_kanji_table.size(); ++i) {
        strokes.insert(m_kanji_table[i].strokes);
    }
    std::set<WORD>::iterator it, end = strokes.end();
    TCHAR sz[128];
    for (it = strokes.begin(); it != end; ++it) {
        StringCchPrintfW(sz, _countof(sz), LoadStringDx(IDM_KAKUSUU), *it);
        ::SendMessage(m_hListBox1, LB_ADDSTRING, 0, (LPARAM)sz);
    }

    // fill radical list box
    for (size_t i = 0; i < m_radical_table.size(); ++i) {
        const RADICAL_ENTRY& entry = m_radical_table[i];
        LPCTSTR psz = entry.readings.c_str();
        ::SendMessage(m_hListBox2, LB_ADDSTRING, 0, (LPARAM)psz);
        ::SendMessage(m_hListBox2, LB_SETITEMDATA, i, (LPARAM)entry.radical_id);
    }

    // set image list to list view
    ListView_SetImageList(m_hListView, m_himlKanji, LVSIL_NORMAL);

    // set font
    ::SendMessage(m_hListBox2, WM_SETFONT, (WPARAM)m_hSmallFont, FALSE);
    ::SendMessage(m_hListView, WM_SETFONT, (WPARAM)m_hSmallFont, FALSE);
    ::SendMessage(m_hListBox1, WM_SETFONT, (WPARAM)m_hLargeFont, FALSE);
    ::SendMessage(m_hListBox1, LB_SETITEMHEIGHT, 0, LARGE_FONT_SIZE + 4);

    // show child windows
    ::ShowWindow(m_hListBox1, SW_SHOWNOACTIVATE);
    ::ShowWindow(m_hListView, SW_SHOWNOACTIVATE);

    // highlight
    TabCtrl_HighlightItem(m_hTabCtrl, 0, TRUE);

    SetWindowPos(m_hTabCtrl, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    SetWindowLongPtr(m_hListBox1, GWLP_USERDATA, (LONG_PTR)this);
    SetWindowLongPtr(m_hListBox2, GWLP_USERDATA, (LONG_PTR)this);
    m_fnListBox1OldWndProc = (WNDPROC)SetWindowLongPtr(m_hListBox1, GWLP_WNDPROC, (LONG_PTR)ListBox1WndProc);
    m_fnListBox2OldWndProc = (WNDPROC)SetWindowLongPtr(m_hListBox2, GWLP_WNDPROC, (LONG_PTR)ListBox2WndProc);

    // Select 1st item
    PostMessage(m_hListBox1, LB_SETCURSEL, 0, 0);
    PostMessage(m_hListBox2, LB_SETCURSEL, 0, 0);
    PostMessage(hWnd, WM_COMMAND, MAKELONG(GetDlgCtrlID(m_hListBox1), LBN_SELCHANGE), (LPARAM)m_hListBox1);
    PostMessage(hWnd, WM_COMMAND, MAKELONG(GetDlgCtrlID(m_hListBox2), LBN_SELCHANGE), (LPARAM)m_hListBox2);

    // set timer
    ::SetTimer(hWnd, 666, INTERVAL_MILLISECONDS, NULL);

    return TRUE;
} // ImePad::OnCreate

void ImePad::OnSize(HWND hWnd) {
    RECT rc;
    ::GetClientRect(m_hWnd, &rc);

    HDWP hDWP = ::BeginDeferWindowPos(3);
    UINT uFlags = SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER;

    hDWP = ::DeferWindowPos(hDWP, m_hTabCtrl, NULL, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, uFlags);

    TabCtrl_AdjustRect(m_hTabCtrl, FALSE, &rc);

    const INT cx1 = STROKES_WIDTH, cx2 = RADICAL_WIDTH;
    if (::IsWindowVisible(m_hListBox1)) {
        hDWP = ::DeferWindowPos(hDWP, m_hListBox1, NULL, rc.left, rc.top,
                                cx1, rc.bottom - rc.top, uFlags);
        hDWP = ::DeferWindowPos(hDWP, m_hListView, NULL, rc.left + cx1, rc.top,
                                rc.right - rc.left - cx1, rc.bottom - rc.top, uFlags);
    } else {
        hDWP = ::DeferWindowPos(hDWP, m_hListBox2, NULL, rc.left, rc.top,
                                cx2, rc.bottom - rc.top, uFlags);
        hDWP = ::DeferWindowPos(hDWP, m_hListView, NULL, rc.left + cx2, rc.top,
                                rc.right - rc.left - cx2, rc.bottom - rc.top, uFlags);
    }

    ::EndDeferWindowPos(hDWP);

    ListView_Arrange(m_hListView, LVA_DEFAULT);
}

void ImePad::OnLB1StrokesChanged(HWND hWnd) {
    ::SendMessage(m_hListView, WM_HSCROLL, MAKEWPARAM(SB_LEFT, 0), 0);
    ::SendMessage(m_hListView, WM_VSCROLL, MAKEWPARAM(SB_TOP, 0), 0);
    ListView_DeleteAllItems(m_hListView);

    INT i = (INT)SendMessage(m_hListBox1, LB_GETCURSEL, 0, 0);
    if (i == LB_ERR) {
        OnSize(m_hWnd);
        return;
    }

    TCHAR sz[32];
    SendMessage(m_hListBox1, LB_GETTEXT, i, (LPARAM)sz);
    sz[_countof(sz) - 1] = 0;
    int strokes = _ttoi(sz);

    LV_ITEMW lv_item;
    ZeroMemory(&lv_item, sizeof(lv_item));
    lv_item.mask = LVIF_TEXT | LVIF_IMAGE;
    for (size_t i = 0; i < m_kanji_table.size(); ++i) {
        const KANJI_ENTRY& entry = m_kanji_table[i];
        if (entry.strokes != strokes) {
            continue;
        }
        lv_item.iItem = (INT)i;
        lv_item.iSubItem = 0;
        lv_item.pszText = const_cast<WCHAR *>(entry.readings.c_str());
        lv_item.iImage = (int)i;
        ListView_InsertItem(m_hListView, &lv_item);
    }
    OnSize(m_hWnd);
}

void ImePad::OnLB2StrokesChanged(HWND hWnd) {
    ::SendMessage(m_hListView, WM_HSCROLL, MAKEWPARAM(SB_LEFT, 0), 0);
    ::SendMessage(m_hListView, WM_VSCROLL, MAKEWPARAM(SB_TOP, 0), 0);
    ListView_DeleteAllItems(m_hListView);

    INT i = (INT)SendMessage(m_hListBox2, LB_GETCURSEL, 0, 0);
    if (i == LB_ERR) {
        OnSize(m_hWnd);
        return;
    }

    WORD radical_id2 = m_radical_table[i].radical_id2;

    LV_ITEMW lv_item;
    ZeroMemory(&lv_item, sizeof(lv_item));
    lv_item.mask = LVIF_TEXT | LVIF_IMAGE;
    for (size_t i = 0; i < m_kanji_table.size(); ++i) {
        const KANJI_ENTRY& entry = m_kanji_table[i];
        if (entry.radical_id2 != radical_id2) {
            continue;
        }
        lv_item.iItem = (INT)i;
        lv_item.iSubItem = 0;
        lv_item.pszText = const_cast<WCHAR *>(entry.readings.c_str());
        lv_item.iImage = (int)i;
        ListView_InsertItem(m_hListView, &lv_item);
    }

    OnSize(m_hWnd);
}

void ImePad::OnTimer(HWND hWnd) {
    if (m_bInSizing)
        return;

    HWND hwndTarget = ::GetForegroundWindow();

    if (hwndTarget == NULL || hwndTarget == hWnd)
        return;

    if (m_hwndLastActive != hwndTarget) {
        WCHAR text[MAX_PATH], szCls[MAX_PATH];
        GetWindowTextW(hwndTarget, text, _countof(text));
        GetClassNameW(hwndTarget, szCls, _countof(szCls));

        WCHAR str[MAX_PATH];
        StringCchPrintfW(str, _countof(str), L"[%s] %s\n", szCls, text);
        OutputDebugStringW(str);

        m_hwndLastActive = hwndTarget;
    }
}

void ImePad::OnGetMinMaxInfo(LPMINMAXINFO pmmi) {
    pmmi->ptMinTrackSize.x = 200;
    pmmi->ptMinTrackSize.y = 200;
}

void ImePad::OnCommand(HWND hWnd, WPARAM wParam, LPARAM lParam) {
    if (HIWORD(wParam) == LBN_SELCHANGE) {
        switch (LOWORD(wParam)) {
        case IDW_LISTBOX1:
            OnLB1StrokesChanged(hWnd);
            break;
        case IDW_LISTBOX2:
            OnLB2StrokesChanged(hWnd);
            break;
        }
        return;
    }
}

void ImePad::OnDestroy(HWND hWnd) {
    KillTimer(hWnd, 666);

    ::SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
    ::SetWindowLongPtr(m_hTabCtrl, GWLP_USERDATA, 0);
    ::SetWindowLongPtr(m_hListView, GWLP_USERDATA, 0);
    ::SetWindowLongPtr(m_hListBox1, GWLP_USERDATA, 0);
    ::SetWindowLongPtr(m_hListBox2, GWLP_USERDATA, 0);
}

void ImePad::MySendInput(WCHAR ch) {
    if (m_hwndLastActive)
        ::SwitchToThisWindow(m_hwndLastActive, TRUE);

    INPUT input;
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = 0;
    input.ki.wScan = ch;
    input.ki.dwFlags = KEYEVENTF_UNICODE;
    input.ki.time = 0;
    input.ki.dwExtraInfo = 0;
    ::SendInput(1, &input, sizeof(INPUT));
}

void ImePad::OnNotify(HWND hWnd, WPARAM wParam, LPARAM lParam) {
    NMHDR *pnmhdr = (NMHDR *)lParam;
    switch (pnmhdr->code) {
    case TCN_SELCHANGE:
    {
        INT iCurSel = TabCtrl_GetCurSel(m_hTabCtrl);
        TabCtrl_HighlightItem(m_hTabCtrl, 0, FALSE);
        TabCtrl_HighlightItem(m_hTabCtrl, 1, FALSE);
        TabCtrl_HighlightItem(m_hTabCtrl, iCurSel, TRUE);
        switch (iCurSel) {
        case 0:
            ::ShowWindow(m_hListBox1, SW_SHOWNOACTIVATE);
            ::ShowWindow(m_hListBox2, SW_HIDE);
            OnLB1StrokesChanged(hWnd);
            break;
        case 1:
            ::ShowWindow(m_hListBox1, SW_HIDE);
            ::ShowWindow(m_hListBox2, SW_SHOWNOACTIVATE);
            OnLB2StrokesChanged(hWnd);
            break;
        default:
            break;
        }
        break;
    }
    case NM_DBLCLK:
        if (pnmhdr->hwndFrom == m_hListView) {
            INT iItem = ListView_GetNextItem(m_hListView, -1, LVNI_ALL | LVNI_SELECTED);
            if (iItem == -1) {
                break;
            }
            LV_ITEM item;
            ZeroMemory(&item, sizeof(item));
            item.mask = LVIF_IMAGE;
            item.iItem = iItem;
            item.iSubItem = 0;
            ListView_GetItem(m_hListView, &item);
            WCHAR ch = m_kanji_table[item.iImage].kanji_char;
            MySendInput(ch);
        }
        break;
    }
}

/*static*/ LRESULT CALLBACK
ImePad::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    ImePad *pImePad;
    pImePad = (ImePad *)::GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (uMsg) {
    case WM_CREATE:
        pImePad = new ImePad;
        if (!pImePad->OnCreate(hWnd)) {
            delete pImePad;
            return -1;
        }
        pImePad->m_hWnd = hWnd;
        // Force active looking
        ::SendMessageW(hWnd, WM_NCACTIVATE, TRUE, 0);
        break;

    case WM_NCACTIVATE:
        // Force active looking
        return DefWindowProcW(hWnd, WM_NCACTIVATE, TRUE, -1);

    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;

    case WM_MOVING:
    case WM_SIZING:
        // There was a problem with WS_EX_NOACTIVATE, in display of moving/sizing the window.
        // https://stackoverflow.com/questions/25051552/create-a-window-using-the-ws-ex-noactivate-flag-but-it-cant-be-dragged-until-i
        {
            RECT* prc = (RECT*)lParam;
            SetWindowPos(hWnd, NULL, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top, 0);
        }
        break;

    case WM_SIZE:
        if (pImePad)
            pImePad->OnSize(hWnd);
        break;

    case WM_NOTIFY:
        pImePad->OnNotify(hWnd, wParam, lParam);
        break;

    case WM_COMMAND:
        pImePad->OnCommand(hWnd, wParam, lParam);
        break;

    case WM_DESTROY:
        pImePad->OnDestroy(hWnd);
        delete pImePad;
        PostQuitMessage(0);
        break;

    case WM_TIMER:
        if (wParam == 666) {
            pImePad->OnTimer(hWnd);
        }
        break;

    case WM_GETMINMAXINFO:
        pImePad->OnGetMinMaxInfo((LPMINMAXINFO)lParam);
        break;

    case WM_ENTERSIZEMOVE:
        pImePad->m_bInSizing = TRUE;
        break;

    case WM_EXITSIZEMOVE:
        pImePad->m_bInSizing = FALSE;
        break;

    case WM_MEASUREITEM:
        {
            LPMEASUREITEMSTRUCT lpMeasure = (LPMEASUREITEMSTRUCT)lParam;
            if (lpMeasure->CtlID == IDW_LISTBOX1) {
                lpMeasure->itemWidth = STROKES_WIDTH;
                lpMeasure->itemHeight = STROKES_HEIGHT;
                return TRUE;
            }
            if (lpMeasure->CtlID == IDW_LISTBOX2) {
                lpMeasure->itemWidth = RADICAL_WIDTH;
                lpMeasure->itemHeight = RADICAL_SIZE;
                return TRUE;
            }
        }
        break;

    case WM_DRAWITEM:
        {
            LPDRAWITEMSTRUCT lpDraw = (LPDRAWITEMSTRUCT)lParam;
            if (lpDraw->CtlID == IDW_LISTBOX1) {
                pImePad->OnDrawItemListBox1(hWnd, lpDraw);
                return TRUE;
            }
            if (lpDraw->CtlID == IDW_LISTBOX2) {
                pImePad->OnDrawItemListBox2(hWnd, lpDraw);
                return TRUE;
            }
        }
        break;

    default:
        return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    return 0;
} // ImePad::WindowProc

//////////////////////////////////////////////////////////////////////////////

INT AppMain(HINSTANCE hInstance, LPWSTR lpCmdLine, INT nCmdShow) {
    g_hInst = hInstance;
    ::InitCommonControls();

    // register class of IME Pad window.
    WNDCLASSEX wcx = { sizeof(wcx) };
    wcx.style = CS_DBLCLKS | CS_IME | CS_SAVEBITS;
    wcx.lpfnWndProc = ImePad::WindowProc;
    wcx.hInstance = hInstance;
    wcx.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1));
    wcx.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcx.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    wcx.lpszClassName = szImePadClassName;
    wcx.hIconSm = NULL;
    if (!::RegisterClassEx(&wcx)) {
        MessageBoxA(NULL, "RegisterClassEx", NULL, MB_ICONERROR);
        return 1;
    }

    if (!ImePad::Create(NULL)) {
        MessageBoxA(NULL, "CreateWindowEx", NULL, MB_ICONERROR);
        return 2;
    }

    MSG msg;
    while (::GetMessage(&msg, NULL, 0, 0)) {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }

    return INT(msg.wParam);
}

extern "C"
INT WINAPI
wWinMain(
        HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPWSTR lpCmdLine,
        INT nCmdShow)
{
    HWND hwndFound = ::FindWindowW(szImePadClassName, LoadStringDx(IDM_IME_PAD));
    if (hwndFound) {
        ::FlashWindow(hwndFound, TRUE);
        return 0;
    }

    PVOID OldValue;
    DisableWow64FsRedirection(&OldValue);

    INT ret = AppMain(hInstance, lpCmdLine, nCmdShow);

    RevertWow64FsRedirection(OldValue);
    return ret;
} // wWinMain

//////////////////////////////////////////////////////////////////////////////
