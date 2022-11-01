// mzimeja.cpp --- MZ-IME Japanese Input (mzimeja)
//////////////////////////////////////////////////////////////////////////////
// (Japanese, Shift_JIS)

#include "mzimeja.h"
#include <shlobj.h>
#include <strsafe.h>
#include "resource.h"

//////////////////////////////////////////////////////////////////////////////

// the window classes for mzimeja UI windows
const WCHAR szUIServerClassName[] = L"MZIMEUI";
const WCHAR szCompStrClassName[]  = L"MZIMECompStr";
const WCHAR szCandClassName[]     = L"MZIMECand";
const WCHAR szStatusClassName[]   = L"MZIMEStatus";
const WCHAR szGuideClassName[]    = L"MZIMEGuide";

const MZGUIDELINE glTable[] = {
    {GL_LEVEL_ERROR, GL_ID_NODICTIONARY, IDS_GL_NODICTIONARY, 0},
    {GL_LEVEL_WARNING, GL_ID_TYPINGERROR, IDS_GL_TYPINGERROR, 0},
    {GL_LEVEL_WARNING, GL_ID_PRIVATE_FIRST, IDS_GL_TESTGUIDELINESTR,
     IDS_GL_TESTGUIDELINEPRIVATE}
};

// filename of the IME
const WCHAR szImeFileName[] = L"mzimeja.ime";

//////////////////////////////////////////////////////////////////////////////

HFONT CheckNativeCharset(HDC hDC) {
    HFONT hOldFont = (HFONT)GetCurrentObject(hDC, OBJ_FONT);

    LOGFONT lfFont;
    GetObject(hOldFont, sizeof(LOGFONT), &lfFont);

    if (lfFont.lfCharSet != SHIFTJIS_CHARSET) {
        lfFont.lfWeight = FW_NORMAL;
        lfFont.lfCharSet = SHIFTJIS_CHARSET;
        lfFont.lfFaceName[0] = 0;
        SelectObject(hDC, CreateFontIndirect(&lfFont));
    } else {
        hOldFont = NULL;
    }
    return hOldFont;
} // CheckNativeCharset

// adjust window position
void RepositionWindow(HWND hWnd) {
    FOOTMARK();
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

//////////////////////////////////////////////////////////////////////////////

MzIme TheIME;

MzIme::MzIme() {
    m_hInst = NULL;
    m_hMyKL = NULL;
    m_bWinLogOn = FALSE;

    m_lpCurTransKey = NULL;
    m_uNumTransKey = 0;

    m_fOverflowKey = FALSE;

    m_hIMC = NULL;
    m_lpIMC = NULL;
}

BOOL MzIme::LoadDict() {
    BOOL ret = TRUE;
    DWORD dw;

    std::wstring basic;
    if (!GetUserDword(L"BasicDictDisabled", &dw) || !dw) {
        if (GetUserString(L"BasicDictPathName", basic) ||
            GetComputerString(L"BasicDictPathName", basic))
        {
            if (!m_basic_dict.Load(basic.c_str(), L"BasicDictObject")) {
                ret = FALSE;
            }
        }
    } else {
        m_basic_dict.Unload();
    }

    std::wstring name;
    if (!GetUserDword(L"NameDictDisabled", &dw) || !dw) {
        if (GetUserString(L"NameDictPathName", name) ||
            GetComputerString(L"NameDictPathName", name))
        {
            if (!m_name_dict.Load(name.c_str(), L"NameDictObject")) {
                ret = FALSE;
            }
        }
    } else {
        m_name_dict.Unload();
    }

    return ret;
}

void MzIme::UnloadDict() {
    m_basic_dict.Unload();
    m_name_dict.Unload();
}

BOOL MzIme::Init(HINSTANCE hInstance) {
    FOOTMARK();
    m_hInst = hInstance;
    //::InitCommonControls();

    MakeLiteralMaps();

    // load dict
    LoadDict();

    // register window classes for IME
    return RegisterClasses(m_hInst);
} // MzIme::Init

VOID MzIme::Uninit(VOID) {
    FOOTMARK();
    UnregisterClasses();
    UnloadDict();
}

//////////////////////////////////////////////////////////////////////////////
// registry

LONG MzIme::OpenRegKey(
        HKEY hKey, LPCWSTR pszSubKey, BOOL bWrite, HKEY *phSubKey) const
{
    LONG result;
    REGSAM sam = (bWrite ? KEY_WRITE : KEY_READ);
    result = ::RegOpenKeyExW(hKey, pszSubKey, 0, sam | KEY_WOW64_64KEY, phSubKey);
    if (result != ERROR_SUCCESS) {
        result = ::RegOpenKeyExW(hKey, pszSubKey, 0, sam, phSubKey);
    }
    return result;
} // MzIme::OpenRegKey

LONG MzIme::CreateRegKey(HKEY hKey, LPCWSTR pszSubKey, HKEY *phSubKey) {
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
} // MzIme::CreateRegKey

//////////////////////////////////////////////////////////////////////////////
// settings

static const WCHAR s_szRegKey[] =
        L"SOFTWARE\\Katayama Hirofumi MZ\\mzimeja";

LONG MzIme::OpenComputerSettingKey(BOOL bWrite, HKEY *phKey) {
    LONG result;
    if (bWrite) {
        result = OpenRegKey(HKEY_LOCAL_MACHINE, s_szRegKey, TRUE, phKey);
    } else {
        result = OpenRegKey(HKEY_LOCAL_MACHINE, s_szRegKey, FALSE, phKey);
    }
    return result;
}

LONG MzIme::OpenUserSettingKey(BOOL bWrite, HKEY *phKey) {
    LONG result;
    if (bWrite) {
        HKEY hSoftware;
        result = OpenRegKey(HKEY_CURRENT_USER, L"Software", TRUE, &hSoftware);
        if (result == ERROR_SUCCESS) {
            HKEY hCompany;
            result = CreateRegKey(hSoftware, L"Katayama Hirofumi MZ", &hCompany);
            if (result == ERROR_SUCCESS) {
                HKEY hMZIMEJA;
                result = CreateRegKey(hCompany, L"mzimeja", &hMZIMEJA);
                if (result == ERROR_SUCCESS) {
                    ::RegCloseKey(hMZIMEJA);
                }
                ::RegCloseKey(hCompany);
            }
            ::RegCloseKey(hSoftware);
        }
        result = OpenRegKey(HKEY_CURRENT_USER, s_szRegKey, TRUE, phKey);
    } else {
        result = OpenRegKey(HKEY_CURRENT_USER, s_szRegKey, FALSE, phKey);
    }
    return result;
}

BOOL MzIme::GetComputerString(LPCWSTR pszSettingName, std::wstring& value) {
    HKEY hKey;
    WCHAR szValue[MAX_PATH * 2];
    LONG result = OpenComputerSettingKey(FALSE, &hKey);
    if (result == ERROR_SUCCESS && hKey) {
        DWORD cbData = sizeof(szValue);
        result = ::RegQueryValueExW(hKey, pszSettingName, NULL, NULL,
                                    reinterpret_cast<LPBYTE>(szValue), &cbData);
        ::RegCloseKey(hKey);
        if (result == ERROR_SUCCESS) {
            value = szValue;
            return TRUE;
        }
    }
    return FALSE;
} // MzIme::GetComputerString

BOOL MzIme::SetComputerString(LPCWSTR pszSettingName, LPCWSTR pszValue) {
    HKEY hKey;
    LONG result = OpenComputerSettingKey(TRUE, &hKey);
    if (result == ERROR_SUCCESS && hKey) {
        DWORD cbData = (::lstrlenW(pszValue) + 1) * sizeof(WCHAR);
        result = ::RegSetValueExW(hKey, pszSettingName, 0, REG_SZ,
                                  reinterpret_cast<const BYTE *>(pszValue), cbData);
        ::RegCloseKey(hKey);
        if (result == ERROR_SUCCESS) {
            return TRUE;
        }
    }
    return FALSE;
} // MzIme::SetComputerString

BOOL
MzIme::GetComputerData(LPCWSTR pszSettingName, void *ptr, DWORD size) {
    HKEY hKey;
    LONG result = OpenComputerSettingKey(FALSE, &hKey);
    if (result == ERROR_SUCCESS && hKey) {
        DWORD cbData = size;
        result = ::RegQueryValueExW(hKey, pszSettingName, NULL, NULL,
                                    reinterpret_cast<LPBYTE>(ptr), &cbData);
        ::RegCloseKey(hKey);
        if (result == ERROR_SUCCESS) {
            return TRUE;
        }
    }
    return FALSE;
} // MzIme::GetComputerData

BOOL
MzIme::SetComputerData(LPCWSTR pszSettingName, const void *ptr, DWORD size) {
    HKEY hKey;
    LONG result = OpenComputerSettingKey(TRUE, &hKey);
    if (result == ERROR_SUCCESS && hKey) {
        result = ::RegSetValueExW(hKey, pszSettingName, 0, REG_BINARY,
                                  reinterpret_cast<const BYTE *>(ptr), size);
        ::RegCloseKey(hKey);
        if (result == ERROR_SUCCESS) {
            return TRUE;
        }
    }
    return FALSE;
} // MzIme::SetComputerData

BOOL MzIme::GetComputerDword(LPCWSTR pszSettingName, DWORD *ptr) {
    HKEY hKey;
    LONG result = OpenComputerSettingKey(FALSE, &hKey);
    if (result == ERROR_SUCCESS && hKey) {
        DWORD cbData = sizeof(DWORD);
        result = ::RegQueryValueExW(hKey, pszSettingName, NULL, NULL,
                                    reinterpret_cast<LPBYTE>(ptr), &cbData);
        ::RegCloseKey(hKey);
        if (result == ERROR_SUCCESS) {
            return TRUE;
        }
    }
    return FALSE;
} // MzIme::GetComputerData

BOOL MzIme::SetComputerDword(LPCWSTR pszSettingName, DWORD data) {
    HKEY hKey;
    DWORD dwData = data;
    LONG result = OpenComputerSettingKey(TRUE, &hKey);
    if (result == ERROR_SUCCESS && hKey) {
        DWORD size = sizeof(DWORD);
        result = ::RegSetValueExW(hKey, pszSettingName, 0, REG_BINARY,
                                  reinterpret_cast<const BYTE *>(&dwData), size);
        ::RegCloseKey(hKey);
        if (result == ERROR_SUCCESS) {
            return TRUE;
        }
    }
    return FALSE;
} // MzIme::SetComputerData

BOOL MzIme::GetUserString(LPCWSTR pszSettingName, std::wstring& value) {
    HKEY hKey;
    WCHAR szValue[MAX_PATH * 2];
    LONG result = OpenUserSettingKey(FALSE, &hKey);
    if (result == ERROR_SUCCESS && hKey) {
        DWORD cbData = sizeof(szValue);
        result = ::RegQueryValueExW(hKey, pszSettingName, NULL, NULL,
                                    reinterpret_cast<LPBYTE>(szValue), &cbData);
        ::RegCloseKey(hKey);
        if (result == ERROR_SUCCESS) {
            value = szValue;
            return TRUE;
        }
    }
    return FALSE;
} // MzIme::GetUserString

BOOL MzIme::SetUserString(LPCWSTR pszSettingName, LPCWSTR pszValue) {
    HKEY hKey;
    LONG result = OpenUserSettingKey(TRUE, &hKey);
    assert(result == ERROR_SUCCESS);
    if (result == ERROR_SUCCESS && hKey) {
        DWORD cbData = (::lstrlenW(pszValue) + 1) * sizeof(WCHAR);
        result = ::RegSetValueExW(hKey, pszSettingName, 0, REG_SZ,
                                  reinterpret_cast<const BYTE *>(pszValue), cbData);
        ::RegCloseKey(hKey);
        assert(result == ERROR_SUCCESS);
        if (result == ERROR_SUCCESS) {
            return TRUE;
        }
    }
    return FALSE;
} // MzIme::SetUserString

BOOL MzIme::GetUserData(LPCWSTR pszSettingName, void *ptr, DWORD size) {
    HKEY hKey;
    LONG result = OpenUserSettingKey(FALSE, &hKey);
    if (result == ERROR_SUCCESS && hKey) {
        DWORD cbData = size;
        result = ::RegQueryValueExW(hKey, pszSettingName, NULL, NULL,
                                    reinterpret_cast<LPBYTE>(ptr), &cbData);
        ::RegCloseKey(hKey);
        if (result == ERROR_SUCCESS) {
            return TRUE;
        }
    }
    return FALSE;
} // MzIme::GetUserData

BOOL MzIme::SetUserData(LPCWSTR pszSettingName, const void *ptr, DWORD size) {
    HKEY hKey;
    LONG result = OpenUserSettingKey(TRUE, &hKey);
    assert(result == ERROR_SUCCESS);
    if (result == ERROR_SUCCESS && hKey) {
        result = ::RegSetValueExW(hKey, pszSettingName, 0, REG_BINARY,
                                  reinterpret_cast<const BYTE *>(ptr), size);
        ::RegCloseKey(hKey);
        assert(result == ERROR_SUCCESS);
        if (result == ERROR_SUCCESS) {
            return TRUE;
        }
    }
    return FALSE;
} // MzIme::SetUserData

BOOL MzIme::GetUserDword(LPCWSTR pszSettingName, DWORD *ptr) {
    HKEY hKey;
    LONG result = OpenUserSettingKey(FALSE, &hKey);
    if (result == ERROR_SUCCESS && hKey) {
        DWORD cbData = sizeof(DWORD);
        result = ::RegQueryValueExW(hKey, pszSettingName, NULL, NULL,
                                    reinterpret_cast<LPBYTE>(ptr), &cbData);
        ::RegCloseKey(hKey);
        if (result == ERROR_SUCCESS) {
            return TRUE;
        }
    }
    return FALSE;
} // MzIme::GetUserData

BOOL MzIme::SetUserDword(LPCWSTR pszSettingName, DWORD data) {
    HKEY hKey;
    DWORD dwData = data;
    LONG result = OpenUserSettingKey(TRUE, &hKey);
    assert(result == ERROR_SUCCESS);
    if (result == ERROR_SUCCESS && hKey) {
        DWORD size = sizeof(DWORD);
        result = ::RegSetValueExW(hKey, pszSettingName, 0, REG_DWORD,
                                  reinterpret_cast<const BYTE *>(&dwData), size);
        ::RegCloseKey(hKey);
        assert(result == ERROR_SUCCESS);
        if (result == ERROR_SUCCESS) {
            return TRUE;
        }
    }
    return FALSE;
} // MzIme::SetUserData

//////////////////////////////////////////////////////////////////////////////

BOOL MzIme::RegisterClasses(HINSTANCE hInstance) {
#define CS_MZIME (CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS | CS_IME)
    WNDCLASSEX wcx;
    FOOTMARK();

    // register class of UI server window.
    wcx.cbSize = sizeof(WNDCLASSEX);
    wcx.style = CS_MZIME;
    wcx.lpfnWndProc = MZIMEWndProc;
    wcx.cbClsExtra = 0;
    wcx.cbWndExtra = 2 * sizeof(LONG_PTR);
    wcx.hInstance = hInstance;
    wcx.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcx.hIcon = NULL;
    wcx.lpszMenuName = NULL;
    wcx.lpszClassName = szUIServerClassName;
    wcx.hbrBackground = NULL;
    wcx.hIconSm = NULL;
    if (!RegisterClassEx(&wcx)) FOOTMARK_RETURN_INT(FALSE);

    // register class of composition window.
    wcx.cbSize = sizeof(WNDCLASSEX);
    wcx.style = CS_MZIME;
    wcx.lpfnWndProc = CompWnd_WindowProc;
    wcx.cbClsExtra = 0;
    wcx.cbWndExtra = UIEXTRASIZE;
    wcx.hInstance = hInstance;
    wcx.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcx.hIcon = NULL;
    wcx.lpszMenuName = NULL;
    wcx.lpszClassName = szCompStrClassName;
    wcx.hbrBackground = NULL;
    wcx.hIconSm = NULL;
    if (!RegisterClassEx(&wcx)) FOOTMARK_RETURN_INT(FALSE);

    // register class of candidate window.
    wcx.cbSize = sizeof(WNDCLASSEX);
    wcx.style = CS_MZIME;
    wcx.lpfnWndProc = CandWnd_WindowProc;
    wcx.cbClsExtra = 0;
    wcx.cbWndExtra = UIEXTRASIZE;
    wcx.hInstance = hInstance;
    wcx.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcx.hIcon = NULL;
    wcx.lpszMenuName = NULL;
    wcx.lpszClassName = szCandClassName;
    wcx.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
    wcx.hIconSm = NULL;
    if (!RegisterClassEx(&wcx)) FOOTMARK_RETURN_INT(FALSE);

    // register class of status window.
    wcx.cbSize = sizeof(WNDCLASSEX);
    wcx.style = CS_MZIME;
    wcx.lpfnWndProc = StatusWnd_WindowProc;
    wcx.cbClsExtra = 0;
    wcx.cbWndExtra = UIEXTRASIZE;
    wcx.hInstance = hInstance;
    wcx.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcx.hIcon = NULL;
    wcx.lpszMenuName = NULL;
    wcx.lpszClassName = szStatusClassName;
    wcx.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
    wcx.hIconSm = NULL;
    if (!RegisterClassEx(&wcx)) FOOTMARK_RETURN_INT(FALSE);

    // register class of guideline window.
    wcx.cbSize = sizeof(WNDCLASSEX);
    wcx.style = CS_MZIME;
    wcx.lpfnWndProc = GuideWnd_WindowProc;
    wcx.cbClsExtra = 0;
    wcx.cbWndExtra = UIEXTRASIZE;
    wcx.hInstance = hInstance;
    wcx.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcx.hIcon = NULL;
    wcx.lpszMenuName = NULL;
    wcx.lpszClassName = szGuideClassName;
    wcx.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
    wcx.hIconSm = NULL;
    if (!RegisterClassEx(&wcx)) FOOTMARK_RETURN_INT(FALSE);

    FOOTMARK_RETURN_INT(TRUE);
#undef CS_MZIME
} // MzIme::RegisterClasses

HKL MzIme::GetHKL(VOID) {
    FOOTMARK();
    HKL hKL = NULL;

    // get list size and allocate buffer for list
    DWORD dwSize = ::GetKeyboardLayoutList(0, NULL);
    HKL *lphkl = (HKL *)::GlobalAlloc(GPTR, dwSize * sizeof(DWORD));
    if (lphkl == NULL) FOOTMARK_RETURN_PTR(HKL, NULL);

    // get the list of keyboard layouts
    ::GetKeyboardLayoutList(dwSize, lphkl);

    // find hKL from the list
    TCHAR szFile[32];
    for (DWORD dwi = 0; dwi < dwSize; dwi++) {
        HKL hKLTemp = *(lphkl + dwi);
        ::ImmGetIMEFileName(hKLTemp, szFile, _countof(szFile));

        if (::lstrcmp(szFile, szImeFileName) == 0) {
            hKL = hKLTemp;
            break;
        }
    }

    // free the list
    ::GlobalFree(lphkl);
    FOOTMARK_RETURN_PTR(HKL, hKL);
}

// Update the transrate key buffer
BOOL MzIme::GenerateMessage(LPTRANSMSG lpGeneMsg) {
    BOOL ret = FALSE;
    FOOTMARK_FORMAT("(%u,%d,%d)\n",
                    lpGeneMsg->message, lpGeneMsg->wParam, lpGeneMsg->lParam);

    if (m_lpCurTransKey)
        FOOTMARK_RETURN_INT(GenerateMessageToTransKey(lpGeneMsg));

    if (m_lpIMC && ::IsWindow(m_lpIMC->hWnd)) {
        DWORD dwNewSize = sizeof(TRANSMSG) * (m_lpIMC->NumMsgBuf() + 1);
        m_lpIMC->hMsgBuf = ImmReSizeIMCC(m_lpIMC->hMsgBuf, dwNewSize);
        if (m_lpIMC->hMsgBuf) {
            LPTRANSMSG lpTransMsg = m_lpIMC->LockMsgBuf();
            if (lpTransMsg) {
                lpTransMsg[m_lpIMC->NumMsgBuf()++] = *lpGeneMsg;
                m_lpIMC->UnlockMsgBuf();
                ret = ImmGenerateMessage(m_hIMC);
            }
        }
    }
    FOOTMARK_RETURN_INT(ret);
}

BOOL MzIme::GenerateMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    FOOTMARK_FORMAT("(%u, 0x%08lX, 0x%08lX)\n", message, wParam, lParam);
    TRANSMSG genmsg;
    genmsg.message = message;
    genmsg.wParam = wParam;
    genmsg.lParam = lParam;
    FOOTMARK_RETURN_INT(GenerateMessage(&genmsg));
}

// Update the transrate key buffer
BOOL MzIme::GenerateMessageToTransKey(LPTRANSMSG lpGeneMsg) {
    FOOTMARK();

    // increment the number
    ++m_uNumTransKey;

    // check overflow
    if (m_uNumTransKey >= m_lpCurTransKey->uMsgCount) {
        m_fOverflowKey = TRUE;
        FOOTMARK_RETURN_INT(FALSE);
    }

    // put one message to TRANSMSG buffer
    LPTRANSMSG lpgmT0 = m_lpCurTransKey->TransMsg + (m_uNumTransKey - 1);
    *lpgmT0 = *lpGeneMsg;

    FOOTMARK_RETURN_INT(TRUE);
}

BOOL MzIme::DoCommand(HIMC hIMC, DWORD dwCommand) {
    FOOTMARK();
    switch (dwCommand) {
    case IDM_RECONVERT:
        break;
    case IDM_ABOUT:
        GenerateMessage(WM_IME_NOTIFY, IMN_PRIVATE, MAKELPARAM(0, 0xDEAD));
        break;
    case IDM_HIRAGANA:
        SetInputMode(hIMC, IMODE_FULL_HIRAGANA);
        break;
    case IDM_FULL_KATAKANA:
        SetInputMode(hIMC, IMODE_FULL_KATAKANA);
        break;
    case IDM_FULL_ASCII:
        SetInputMode(hIMC, IMODE_FULL_ASCII);
        break;
    case IDM_HALF_KATAKANA:
        SetInputMode(hIMC, IMODE_HALF_KANA);
        break;
    case IDM_HALF_ASCII:
        SetInputMode(hIMC, IMODE_HALF_ASCII);
        break;
    case IDM_CANCEL:
        break;
    case IDM_ROMAN_INPUT:
        SetRomanMode(hIMC, TRUE);
        break;
    case IDM_KANA_INPUT:
        SetRomanMode(hIMC, FALSE);
        break;
    case IDM_HIDE:
        break;
    case IDM_PROPERTY:
        break;
    case IDM_ADD_WORD:
        break;
    case IDM_IME_PAD:
        GenerateMessage(WM_IME_NOTIFY, IMN_PRIVATE, MAKELPARAM(0, 0xFACE));
        break;
    default:
        FOOTMARK_RETURN_INT(FALSE);
    }
    FOOTMARK_RETURN_INT(TRUE);
} // MzIme::DoCommand

void MzIme::UpdateIndicIcon(HIMC hIMC) {
    FOOTMARK();
    if (m_hMyKL == NULL) {
        m_hMyKL = GetHKL();
        if (m_hMyKL == NULL) return;
    }

    // TODO: enable pen icon update
    HWND hwndIndicate = ::FindWindow(INDICATOR_CLASS, NULL);
    if (::IsWindow(hwndIndicate)) {
        BOOL fOpen = FALSE;
        if (hIMC) {
            fOpen = ImmGetOpenStatus(hIMC);
        }

        ATOM atomTip = ::GlobalAddAtom(TEXT("MZ-IME Open"));
        LPARAM lParam = (LPARAM)m_hMyKL;
        ::PostMessage(hwndIndicate, INDICM_SETIMEICON, fOpen ? 1 : (-1), lParam);
        ::PostMessage(hwndIndicate, INDICM_SETIMETOOLTIPS, (fOpen ? atomTip : (-1)),
                      lParam);
        ::PostMessage(hwndIndicate, INDICM_REMOVEDEFAULTMENUITEMS,
                      (fOpen ? (RDMI_LEFT) : 0), lParam);
    }
}

void MzIme::UnregisterClasses() {
    ::UnregisterClass(szUIServerClassName, m_hInst);
    ::UnregisterClass(szCompStrClassName, m_hInst);
    ::UnregisterClass(szCandClassName, m_hInst);
    ::UnregisterClass(szStatusClassName, m_hInst);
}

HBITMAP MzIme::LoadBMP(LPCTSTR pszName) {
    return ::LoadBitmap(m_hInst, pszName);
}

WCHAR *MzIme::LoadSTR(INT nID) {
    static WCHAR sz[512];
    sz[0] = 0;
    ::LoadStringW(m_hInst, nID, sz, 512);
    return sz;
}

InputContext *MzIme::LockIMC(HIMC hIMC) {
    FOOTMARK();
    DebugPrintA("MzIme::LockIMC: locking: %p\n", hIMC);
    InputContext *context = (InputContext *)::ImmLockIMC(hIMC);
    if (context) {
        m_hIMC = hIMC;
        m_lpIMC = context;
        DebugPrintA("MzIme::LockIMC: locked: %p\n", hIMC);
    } else {
        DebugPrintA("MzIme::LockIMC: cannot lock: %p\n", hIMC);
    }
    FOOTMARK_RETURN_PTR(InputContext *, context);
}

VOID MzIme::UnlockIMC(HIMC hIMC) {
    FOOTMARK();
    DebugPrintA("MzIme::UnlockIMC: unlocking: %p\n", hIMC);
    ::ImmUnlockIMC(hIMC);
    DebugPrintA("MzIme::UnlockIMC: unlocked: %p\n", hIMC);
    if (::ImmGetIMCLockCount(hIMC) == 0) {
        m_hIMC = NULL;
        m_lpIMC = NULL;
    }
}

//////////////////////////////////////////////////////////////////////////////

extern "C" {

//////////////////////////////////////////////////////////////////////////////
// UI extra related

HGLOBAL GetUIExtraFromServerWnd(HWND hwndServer) {
    FOOTMARK();
    FOOTMARK_RETURN_PTR(HGLOBAL,
                        (HGLOBAL)GetWindowLongPtr(hwndServer, IMMGWLP_PRIVATE));
}

void SetUIExtraToServerWnd(HWND hwndServer, HGLOBAL hUIExtra) {
    FOOTMARK();
    SetWindowLongPtr(hwndServer, IMMGWLP_PRIVATE, (LONG_PTR)hUIExtra);
}

UIEXTRA *LockUIExtra(HWND hwndServer) {
    FOOTMARK();
    HGLOBAL hUIExtra = GetUIExtraFromServerWnd(hwndServer);
    UIEXTRA *lpUIExtra = (UIEXTRA *)::GlobalLock(hUIExtra);
    assert(lpUIExtra);
    FOOTMARK_RETURN_PTR(UIEXTRA *, lpUIExtra);
}

void UnlockUIExtra(HWND hwndServer) {
    FOOTMARK();
    HGLOBAL hUIExtra = GetUIExtraFromServerWnd(hwndServer);
    ::GlobalUnlock(hUIExtra);
}

void FreeUIExtra(HWND hwndServer) {
    FOOTMARK();
    HGLOBAL hUIExtra = GetUIExtraFromServerWnd(hwndServer);
    ::GlobalFree(hUIExtra);
    SetWindowLongPtr(hwndServer, IMMGWLP_PRIVATE, (LONG_PTR)NULL);
}

//////////////////////////////////////////////////////////////////////////////
// for debugging

#ifdef MZIMEJA_DEBUG_OUTPUT
void DebugPrintA(const char *lpszFormat, ...) {
    char szMsgA[1024];

    va_list marker;
    va_start(marker, lpszFormat);
    StringCchVPrintfA(szMsgA, _countof(szMsgA), lpszFormat, marker);
    va_end(marker);

    INT cch = strlen(szMsgA);
    if (cch > 0) {
        if (szMsgA[cch - 1] == '\n') {
            szMsgA[cch - 1] = 0;
        }
        StringCchCatA(szMsgA, _countof(szMsgA), "\r\n");
    }

    WCHAR szMsgW[1024];
    szMsgW[0] = 0;
    ::MultiByteToWideChar(932, 0, szMsgA, -1, szMsgW, 1024);

    CHAR szLogFile[MAX_PATH];
    SHGetSpecialFolderPathA(NULL, szLogFile, CSIDL_DESKTOP, FALSE);
    StringCchCatA(szLogFile, _countof(szLogFile), "\\mzimeja.log");

    //OutputDebugString(szMsg);
    FILE *fp = fopen(szLogFile, "ab");
    if (fp) {
        INT len = lstrlenW(szMsgW);
        fwrite(szMsgW, len * sizeof(WCHAR), 1, fp);
        fclose(fp);
    }
}
void DebugPrintW(const WCHAR *lpszFormat, ...) {
    WCHAR szMsg[1024];

    va_list marker;
    va_start(marker, lpszFormat);
    StringCchVPrintfW(szMsg, _countof(szMsg), lpszFormat, marker);
    va_end(marker);

    INT cch = wcslen(szMsg);
    if (cch > 0) {
        if (szMsg[cch - 1] == L'\n') {
            szMsg[cch - 1] = 0;
        }
        StringCchCatW(szMsg, _countof(szMsg), L"\r\n");
    }

    CHAR szLogFile[MAX_PATH];
    SHGetSpecialFolderPathA(NULL, szLogFile, CSIDL_DESKTOP, FALSE);
    StringCchCatA(szLogFile, _countof(szLogFile), "\\mzimeja.log");

    //OutputDebugString(szMsg);
    FILE *fp = fopen(szLogFile, "ab");
    if (fp) {
        INT len = lstrlenW(szMsg);
        fwrite(szMsg, len * sizeof(WCHAR), 1, fp);
        fclose(fp);
    }
}
#endif  // def MZIMEJA_DEBUG_OUTPUT

//////////////////////////////////////////////////////////////////////////////
// DLL entry point

BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD dwFunction, LPVOID lpNot) {
    FOOTMARK_FORMAT("(%p, 0x%08lX, %p)\n", hInstDLL, dwFunction, lpNot);
    switch (dwFunction) {
    case DLL_PROCESS_ATTACH:
        ::DisableThreadLibraryCalls(hInstDLL);
        DebugPrintA("DLL_PROCESS_ATTACH: hInst is %p\n", hInstDLL);
        TheIME.Init(hInstDLL);
        break;

    case DLL_PROCESS_DETACH:
        DebugPrintA("DLL_PROCESS_DETACH: hInst is %p\n", TheIME.m_hInst);
        TheIME.Uninit();
        break;

    case DLL_THREAD_ATTACH:
        DebugPrintA("DLL_THREAD_ATTACH: hInst is %p\n", TheIME.m_hInst);
        break;

    case DLL_THREAD_DETACH:
        DebugPrintA("DLL_THREAD_DETACH: hInst is %p\n", TheIME.m_hInst);
        break;
    }

    FOOTMARK_RETURN_INT(TRUE);
} // DllMain

//////////////////////////////////////////////////////////////////////////////

}  // extern "C"

//////////////////////////////////////////////////////////////////////////////
