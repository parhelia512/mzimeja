// ime_setup.cpp --- MZ-IME setup program
//////////////////////////////////////////////////////////////////////////////

#define _CRT_SECURE_NO_WARNINGS   // use fopen
#include "../targetver.h"    // target Windows version

#define NOMINMAX
#include <windows.h>
#include <shlwapi.h>
#include <dlgs.h>
#include <cstdlib>    // for __argc, __wargv
#include <cstring>    // for wcsrchr
#include <algorithm>  // for std::max
#include "Wow64.h"
#include "resource.h"

HINSTANCE g_hInstance;

//////////////////////////////////////////////////////////////////////////////

LPWSTR FindLocalFile(LPWSTR pszPath, LPCWSTR pszFileName)
{
    ::GetModuleFileNameW(NULL, pszPath, MAX_PATH);
    PathRemoveFileSpecW(pszPath);

    for (INT i = 0; i < 5; ++i)
    {
        size_t ich = wcslen(pszPath);
        {
            PathAppendW(pszPath, pszFileName);
            if (PathFileExistsW(pszPath))
                return pszPath;
        }
        pszPath[ich] = 0;
        {
            PathAppendW(pszPath, L"mzimeja");
            PathAppendW(pszPath, pszFileName);
            if (PathFileExistsW(pszPath))
                return pszPath;
        }
        pszPath[ich] = 0;
        PathRemoveFileSpecW(pszPath);
    }
    return NULL;
}

LPWSTR GetSetupPathName64(LPWSTR pszPath) {
    return FindLocalFile(pszPath, L"ime_setup64.exe");
}

LPWSTR GetSrcImePathName32(LPWSTR pszPath) {
    return FindLocalFile(pszPath, L"x86\\mzimeja.ime");
}
LPWSTR GetSrcImePathName64(LPWSTR pszPath) {
    return FindLocalFile(pszPath, L"x64\\mzimeja.ime");
}

LPWSTR GetSystemImePathName(LPWSTR pszPath) {
    GetSystemDirectory(pszPath, MAX_PATH);
    wcscat(pszPath, L"\\mzimeja.ime");
    return pszPath;
}
LPWSTR GetSystemImePathNameWow64(LPWSTR pszPath) {
    GetWindowsDirectory(pszPath, MAX_PATH);
    wcscat(pszPath, L"\\SysWow64\\mzimeja.ime");
    return pszPath;
}

LPWSTR GetBasicDictPathName(LPWSTR pszPath) {
    return FindLocalFile(pszPath, L"basic.dic");
}

LPWSTR GetNameDictPathName(LPWSTR pszPath) {
    return FindLocalFile(pszPath, L"name.dic");
}

LPWSTR GetPostalDictPathName(LPWSTR pszPath) {
    return FindLocalFile(pszPath, L"postal.dat");
}

LPWSTR GetKanjiDataPathName(LPWSTR pszPath) {
    return FindLocalFile(pszPath, L"kanji.dat");
}

LPWSTR GetRadicalDataPathName(LPWSTR pszPath) {
    return FindLocalFile(pszPath, L"radical.dat");
}

LPWSTR GetImePadPathName(LPWSTR pszPath) {
    return FindLocalFile(pszPath, L"imepad.exe");
}

LPWSTR GetVerInfoPathName(LPWSTR pszPath) {
    return FindLocalFile(pszPath, L"verinfo.exe");
}

LPWSTR GetReadMePathName(LPWSTR pszPath) {
    LPWSTR ret = FindLocalFile(pszPath, L"mzimeja\\READMEJP.txt");
    if (ret)
        return ret;
    return FindLocalFile(pszPath, L"READMEJP.txt");
}

//////////////////////////////////////////////////////////////////////////////
// registry

LONG OpenRegKey(HKEY hKey, LPCWSTR pszSubKey, BOOL bWrite, HKEY *phSubKey) {
    LONG result;
    REGSAM sam = (bWrite ? KEY_ALL_ACCESS : KEY_READ);
    result = ::RegOpenKeyExW(hKey, pszSubKey, 0, sam | KEY_WOW64_64KEY, phSubKey);
    if (result != ERROR_SUCCESS) {
        result = ::RegOpenKeyExW(hKey, pszSubKey, 0, sam, phSubKey);
    }
    return result;
} // OpenRegKey

LONG CreateRegKey(HKEY hKey, LPCWSTR pszSubKey, HKEY *phSubKey) {
    LONG result;
    DWORD dwDisposition;
    const REGSAM sam = KEY_WRITE;
    result = ::RegCreateKeyExW(hKey, pszSubKey, 0, NULL, 0, sam |
                               KEY_WOW64_64KEY, NULL, phSubKey, &dwDisposition);
    if (result != ERROR_SUCCESS) {
        result = ::RegCreateKeyExW(hKey, pszSubKey, 0, NULL, 0, sam, NULL,
                                   phSubKey, &dwDisposition);
    }
    return result;
} // CreateRegKey

//////////////////////////////////////////////////////////////////////////////

LPCTSTR DoLoadString(INT nID) {
    static WCHAR s_szBuf[1024];
    s_szBuf[0] = 0;
    LoadStringW(g_hInstance, nID, s_szBuf, 1024);
    return s_szBuf[0] ? s_szBuf : L"Internal Error";
}

INT DoCopyFiles(VOID) {
    WCHAR szPathSrc[MAX_PATH], szPathDest[MAX_PATH];

    //////////////////////////////////////////////////////////
    // {app}\mzimeja.ime --> C:\Windows\system32\mzimeja.ime

#ifdef _WIN64
    GetSrcImePathName64(szPathSrc);
#else
    GetSrcImePathName32(szPathSrc);
#endif
    GetSystemImePathName(szPathDest);
    BOOL b1 = CopyFile(szPathSrc, szPathDest, FALSE);
    if (!b1) {
        return 2;
    }

#ifdef _WIN64
    GetSrcImePathName32(szPathSrc);
    GetSystemImePathNameWow64(szPathDest);
    BOOL b2 = CopyFile(szPathSrc, szPathDest, FALSE);
    if (!b2) {
        return 3;
    }
#endif

    return 0;
} // DoCopyFiles

INT DoDeleteFiles(VOID) {
    WCHAR szPath[MAX_PATH];
    if (!DeleteFileW(GetSystemImePathName(szPath))) {
        return 1;
    }
    if (!DeleteFileW(GetSystemImePathNameWow64(szPath))) {
        return 1;
    }
    return 0;
}

BOOL DoSetRegSz(HKEY hKey, const WCHAR *pszName, const WCHAR *pszValue) {
    DWORD cbData = (lstrlenW(pszValue) + 1) * sizeof(WCHAR);
    LONG result;
    result = RegSetValueExW(hKey, pszName, 0, REG_SZ, (BYTE *)pszValue, cbData);
    return result == ERROR_SUCCESS;
}

static const WCHAR s_szKeyboardLayouts[] = L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts";

HKL GetImeHKL(VOID)
{
    LSTATUS error;
    HKEY hKey;
    error = OpenRegKey(HKEY_CURRENT_USER, L"SOFTWARE\\Katayama Hirofumi MZ\\mzimeja", FALSE, &hKey);
    if (error != ERROR_SUCCESS)
        return NULL;

    WCHAR szHKL[16];
    DWORD cbHKL = sizeof(szHKL);
    HKL hKL = NULL;
    error = RegQueryValueExW(hKey, L"szHKL", NULL, NULL, (PBYTE)szHKL, &cbHKL);
    szHKL[_countof(szHKL) - 1] = UNICODE_NULL;
    if (!error)
    {
        hKL = (HKL)UlongToHandle(wcstoul(szHKL, NULL, 16));
    }
    RegCloseKey(hKey);
    return hKL;
}

BOOL SetImeHKL(HKL hKL)
{
    LSTATUS error;
    HKEY hKey;
    error = CreateRegKey(HKEY_CURRENT_USER, L"SOFTWARE\\Katayama Hirofumi MZ\\mzimeja", &hKey);
    if (error != ERROR_SUCCESS)
        return NULL;

    WCHAR szHKL[16];
    wsprintfW(szHKL, L"%08X", HandleToUlong(hKL));
    DWORD cbHKL = lstrlenW(szHKL) * sizeof(WCHAR);
    error = RegSetValueExW(hKey, L"szHKL", 0, REG_SZ, (PBYTE)szHKL, cbHKL);
    RegCloseKey(hKey);
    return !error;
}

HKL FindImeHKL(VOID)
{
    HKEY hKey;
    LSTATUS error = OpenRegKey(HKEY_LOCAL_MACHINE, s_szKeyboardLayouts, FALSE, &hKey);
    if (error)
        return NULL;

    WCHAR szSubKey[MAX_PATH];
    DWORD cchSubKey;
    HKEY hSubKey;
    HKL hKL = NULL;
    for (DWORD dwIndex = 0; ; ++dwIndex)
    {
        cchSubKey = _countof(szSubKey);
        error = RegEnumKeyExW(hKey, dwIndex, szSubKey, &cchSubKey, NULL, NULL, NULL, NULL);
        if (error)
            break;

        error = OpenRegKey(hKey, szSubKey, FALSE, &hSubKey);
        if (error)
            break;

        WCHAR szImeFile[32];
        DWORD cbImeFile = sizeof(szImeFile);
        error = RegQueryValueExW(hSubKey, L"IME File", NULL, NULL, (PBYTE)szImeFile, &cbImeFile);
        szImeFile[_countof(szImeFile) - 1] = UNICODE_NULL;
        RegCloseKey(hSubKey);

        if (error)
            continue;

        if (lstrcmpiW(szImeFile, L"mzimeja.ime") == 0)
        {
            hKL = (HKL)UlongToHandle(wcstoul(szSubKey, NULL, 16));
            break;
        }
    }

    RegCloseKey(hKey);
    return hKL;
}

INT DoSetRegistry1(VOID) {
    // IMEのHKLを探す。
    HKL hKL = FindImeHKL();
    if (!hKL)
        return -1;

    // IMEのHKLを覚えておく。
    SetImeHKL(hKL);

    BOOL ret = FALSE;
    HKEY hKey;
    LONG error = OpenRegKey(HKEY_LOCAL_MACHINE, s_szKeyboardLayouts, TRUE, &hKey);
    if (!error)
    {
        WCHAR szSubKey[32];
        wsprintfW(szSubKey, L"%08X", HandleToUlong(hKL));

        HKEY hkLayouts;
        error = CreateRegKey(hKey, szSubKey, &hkLayouts);
        if (!error)
        {
            if (DoSetRegSz(hkLayouts, L"Layout Text", DoLoadString(IDS_IMELAYOUTTEXT)))
            {
                LPCWSTR pszValue = L"@%SystemRoot%\\system32\\mzimeja.ime,-1024";
                DWORD cbValue = (lstrlenW(pszValue) + 1) * sizeof(WCHAR);
                error = RegSetValueExW(hkLayouts, L"Layout Display Name", 0, REG_EXPAND_SZ, (PBYTE)pszValue, cbValue);
                if (!error)
                    ret = TRUE;
            }
            RegCloseKey(hkLayouts);
        }
        RegCloseKey(hKey);
    }
    return (ret ? 0 : -1);
} // DoSetRegistry1

INT DoSetRegistry2(HKEY hBaseKey) {
    BOOL ret = FALSE;
    HKEY hkSoftware;
    LONG result = CreateRegKey(HKEY_CURRENT_USER, L"SOFTWARE\\Katayama Hirofumi MZ\\mzimeja", &hkSoftware);
    if (result == ERROR_SUCCESS && hkSoftware) {
        TCHAR szBasicDictPath[MAX_PATH];
        TCHAR szNameDictPath[MAX_PATH];
        TCHAR szPostalDictPath[MAX_PATH];
        TCHAR szKanjiPath[MAX_PATH];
        TCHAR szRadicalPath[MAX_PATH];
        TCHAR szImePadPath[MAX_PATH];
        TCHAR szVerInfoPath[MAX_PATH];
        TCHAR szReadMePath[MAX_PATH];

        GetBasicDictPathName(szBasicDictPath);
        GetNameDictPathName(szNameDictPath);
        GetPostalDictPathName(szPostalDictPath);
        GetKanjiDataPathName(szKanjiPath);
        GetRadicalDataPathName(szRadicalPath);
        GetImePadPathName(szImePadPath);
        GetVerInfoPathName(szVerInfoPath);
        GetReadMePathName(szReadMePath);

        if (DoSetRegSz(hkSoftware, L"BasicDictPathName", szBasicDictPath) &&
            DoSetRegSz(hkSoftware, L"NameDictPathName", szNameDictPath) &&
            DoSetRegSz(hkSoftware, L"PostalDictPathName", szPostalDictPath) &&
            DoSetRegSz(hkSoftware, L"KanjiDataFile", szKanjiPath) &&
            DoSetRegSz(hkSoftware, L"RadicalDataFile", szRadicalPath) &&
            DoSetRegSz(hkSoftware, L"ImePadFile", szImePadPath) &&
            DoSetRegSz(hkSoftware, L"VerInfoFile", szVerInfoPath) &&
            DoSetRegSz(hkSoftware, L"ReadMeFile", szReadMePath))
        {
            ret = TRUE;
        }
        RegCloseKey(hkSoftware);
    }
    return (ret ? 0 : -1);
} // DoSetRegistry2

LONG MyDeleteRegKey(HKEY hKey, LPCTSTR pszSubKey)
{
    LONG ret;
    DWORD cchSubKeyMax, cchValueMax;
    DWORD cchMax, cch;
    TCHAR szNameBuf[MAX_PATH], *pszName = szNameBuf;
    HKEY hSubKey = hKey;

    if (pszSubKey != NULL)
    {
        ret = OpenRegKey(hKey, pszSubKey, FALSE, &hSubKey);
        if (ret) return ret;
    }

    ret = RegQueryInfoKey(hSubKey, NULL, NULL, NULL, NULL,
                          &cchSubKeyMax, NULL, NULL, &cchValueMax, NULL, NULL, NULL);
    if (ret) goto cleanup;

    cchSubKeyMax++;
    cchValueMax++;
    cchMax = std::max(cchSubKeyMax, cchValueMax);
    if (cchMax > sizeof(szNameBuf) / sizeof(TCHAR))
    {
        pszName = (LPTSTR)HeapAlloc(GetProcessHeap(), 0, cchMax *
                                    sizeof(TCHAR));
        if (pszName == NULL)
        {
            ret = ERROR_NOT_ENOUGH_MEMORY;
            goto cleanup;
        }
    }

    while(TRUE)
    {
        cch = cchMax;
        if (RegEnumKeyEx(hSubKey, 0, pszName, &cch, NULL,
                         NULL, NULL, NULL)) break;

        ret = MyDeleteRegKey(hSubKey, pszName);
        if (ret) goto cleanup;
    }

    if (pszSubKey != NULL)
        ret = RegDeleteKey(hKey, pszSubKey);
    else
        while(TRUE)
        {
            cch = cchMax;
            if (RegEnumValue(hKey, 0, pszName, &cch,
                             NULL, NULL, NULL, NULL)) break;

            ret = RegDeleteValue(hKey, pszName);
            if (ret) goto cleanup;
        }

cleanup:
    if (pszName != szNameBuf)
        HeapFree(GetProcessHeap(), 0, pszName);
    if (pszSubKey != NULL)
        RegCloseKey(hSubKey);
    return ret;
} // MyDeleteRegKey

INT DoUnsetRegistry1(VOID) {
    HKL hKL = GetImeHKL();
    if (!hKL)
        return -1;

    HKEY hKey;
    LSTATUS error;

    // Preloadレジストリエントリを変更する。
    error = OpenRegKey(HKEY_CURRENT_USER, L"Keyboard Layout\\Preload", TRUE, &hKey);
    if (!error)
    {
        HKL ahKLs[128];
        ZeroMemory(ahKLs, sizeof(ahKLs));

        // まずは全部読み込む。
        for (DWORD i = 0; i < _countof(ahKLs); ++i)
        {
            WCHAR szName[64];
            wsprintfW(szName, L"%u", i + 1);

            WCHAR szKL[32];
            DWORD cbKL = sizeof(szKL);
            error = RegQueryValueExW(hKey, szName, NULL, NULL, (PBYTE)szKL, &cbKL);
            szKL[_countof(szKL) - 1] = 0;
            if (error)
                break;

            ahKLs[i] = (HKL)UlongToHandle(wcstoul(szKL, NULL, 16));
        }

        // エントリをすべて削除。
        for (DWORD i = 0; ahKLs[i]; ++i)
        {
            WCHAR szName[64];
            wsprintfW(szName, L"%u", i + 1);
            error = RegDeleteValueW(hKey, szName);
            if (error)
                break;
        }

        // hKLを探し、それを配列から除去する。
        INT k = 0;
        for (INT i = 0; ahKLs[i]; ++i)
        {
            ahKLs[k] = ahKLs[i];
            if (ahKLs[i] != hKL)
                ++k;
        }
        ahKLs[k] = NULL;

        // 配列に従ってPreloadエントリを書き込む。
        for (DWORD i = 0; ahKLs[i]; ++i)
        {
            WCHAR szName[64];
            wsprintfW(szName, L"%u", i + 1);

            WCHAR szKL[32];
            wsprintfW(szKL, L"%08X", ahKLs[i]);
            DWORD cbKL = (lstrlenW(szKL) + 1) * sizeof(WCHAR);

            error = RegSetValueExW(hKey, szName, 0, REG_SZ, (PBYTE)szKL, cbKL);
            if (error)
                break;
        }

        RegCloseKey(hKey);
    }

    // "Keyboard Layouts" からターゲットのIMEを削除する。
    BOOL ret = FALSE;
    error = OpenRegKey(HKEY_LOCAL_MACHINE, s_szKeyboardLayouts, TRUE, &hKey);
    if (!error)
    {
        WCHAR szSubKey[16];
        wsprintfW(szSubKey, L"%08X", HandleToUlong(hKL));

        error = MyDeleteRegKey(hKey, szSubKey);
        if (!error)
            ret = TRUE;

        RegCloseKey(hKey);
    }

    return (ret ? 0 : -1);
} // DoUnsetRegistry1

INT DoUnsetRegistry2(VOID) {
    BOOL ret = FALSE;
    HKEY hKey;
    LSTATUS error = OpenRegKey(HKEY_CURRENT_USER, L"SOFTWARE\\Katayama Hirofumi MZ", TRUE, &hKey);
    if (!error) {
        error = MyDeleteRegKey(hKey, L"mzimeja");
        if (!error) {
            ret = TRUE;
        }
        RegCloseKey(hKey);
    }
    return (ret ? 0 : -1);
} // DoUnsetRegistry2

//////////////////////////////////////////////////////////////////////////////

INT DoInstall(VOID) {
    if (0 != DoSetRegistry2(HKEY_LOCAL_MACHINE) ||
        0 != DoSetRegistry2(HKEY_CURRENT_USER))
    {
        // failure
        ::MessageBoxW(NULL, DoLoadString(IDS_REGSETUPFAIL), NULL, MB_ICONERROR);
        return 2;
    }

    if (0 != DoCopyFiles()) {
        // failure
        ::MessageBoxW(NULL, DoLoadString(IDS_FAILCOPYFILE), NULL, MB_ICONERROR);
        return 1;
    }

    WCHAR szPath[MAX_PATH];
    ::GetSystemDirectoryW(szPath, MAX_PATH);
    wcscat(szPath, L"\\mzimeja.ime");
    if (!ImmInstallIME(szPath, DoLoadString(IDS_IMELAYOUTTEXT))) {
        // failure
        WCHAR szMsg[128];
        DWORD dwError = ::GetLastError();
        ::wsprintfW(szMsg, DoLoadString(IDS_FAILDOCONTACT), dwError);
        ::MessageBoxW(NULL, szMsg, NULL, MB_ICONERROR);
        return 3;
    }

    if (0 != DoSetRegistry1()) {
        // failure
        ::MessageBoxW(NULL, DoLoadString(IDS_REGFAILED), NULL, MB_ICONERROR);
        return 2;
    }

    HINSTANCE hInputDll = LoadLibraryEx(TEXT("input.dll"), NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (hInputDll)
    {
        FreeLibrary(hInputDll);
        ShellExecuteW(NULL, NULL, L"control.exe", L"input.dll", NULL, SW_SHOWNORMAL);
    }
    else
    {
        ShellExecuteW(NULL, NULL, L"rundll32.exe", L"shell32.dll,Control_RunDLL intl.cpl,,5", NULL, SW_SHOWNORMAL);
    }

    return 0;
} // DoInstall

INT DoUninstall(VOID) {
    DoUnsetRegistry1();
    DoUnsetRegistry2();
    DoDeleteFiles();
    return 0;
} // DoUninstall

//////////////////////////////////////////////////////////////////////////////

extern "C"
INT_PTR CALLBACK
DialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_INITDIALOG:
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            if (::IsDlgButtonChecked(hWnd, rad1) == BST_CHECKED) {
                ::EndDialog(hWnd, rad1);
                break;
            }
            if (::IsDlgButtonChecked(hWnd, rad2) == BST_CHECKED) {
                ::EndDialog(hWnd, rad2);
                break;
            }
            break;
        case IDCANCEL:
            ::EndDialog(hWnd, IDCANCEL);
            break;
        }
    }
    return FALSE;
}
//////////////////////////////////////////////////////////////////////////////

INT DoMain(HINSTANCE hInstance, INT argc, LPWSTR *wargv)
{
    TCHAR szText[128];

    g_hInstance = hInstance;

#ifndef _WIN64
    if (IsWow64())
    {
        WCHAR szPath[MAX_PATH];
        if (!GetSetupPathName64(szPath))
        {
            LoadString(hInstance, IDS_64BITNOTSUPPORTED, szText, _countof(szText));
            MessageBox(NULL, szText, TEXT("MZ-IME"), MB_ICONERROR);
            return -1;
        }

        SHELLEXECUTEINFOW sei = { sizeof(sei), SEE_MASK_NOCLOSEPROCESS };
        sei.lpFile = szPath;
        if (__argc == 2)
            sei.lpParameters = wargv[1];
        sei.nShow = SW_SHOWNORMAL;
        if (ShellExecuteExW(&sei))
        {
            WaitForSingleObject(sei.hProcess, INFINITE);
            DWORD dwExitCode;
            GetExitCodeProcess(sei.hProcess, &dwExitCode);
            CloseHandle(sei.hProcess);
            return (INT)dwExitCode;
        }
        return -2;
    }
#endif

    int ret;
    switch (argc) {
    case 2:
        if (lstrcmpiW(wargv[1], L"/i") == 0) {
            return DoInstall();
        }
        if (lstrcmpiW(wargv[1], L"/u") == 0) {
            return DoUninstall();
        }
        break;
    default:
        ret = (INT)::DialogBoxW(hInstance, MAKEINTRESOURCEW(1), NULL, DialogProc);
        switch (ret) {
        case rad1:
            if (DoInstall() == 0) {
                LoadString(hInstance, IDS_SETUPOK, szText, _countof(szText));
                MessageBox(NULL, szText, TEXT("MZ-IME"), MB_ICONINFORMATION);
            } else {
                LoadString(hInstance, IDS_SETUPFAIL, szText, _countof(szText));
                MessageBox(NULL, szText, TEXT("MZ-IME"), MB_ICONERROR);
            }
            break;
        case rad2:
            if (DoUninstall() == 0) {
                LoadString(hInstance, IDS_SETUPOK, szText, _countof(szText));
                MessageBox(NULL, szText, TEXT("MZ-IME"), MB_ICONINFORMATION);
            } else {
                LoadString(hInstance, IDS_SETUPFAIL, szText, _countof(szText));
                MessageBox(NULL, szText, TEXT("MZ-IME"), MB_ICONERROR);
            }
            break;
        default:
            break;
        }
        break;
    }

    return 0;
}

extern "C"
INT WINAPI
wWinMain(
        HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPWSTR lpCmdLine,
        INT nCmdShow)
{
    // Wow64リダイレクションを無効化する。
    PVOID OldValue;
    BOOL bRedirected = DisableWow64FsRedirection(&OldValue);

    // メイン処理。
    INT ret = DoMain(hInstance, __argc, __wargv);

    // Wow64リダイレクションを元に戻す。
    if (bRedirected) RevertWow64FsRedirection(OldValue);

    return ret;
} // wWinMain

//////////////////////////////////////////////////////////////////////////////
