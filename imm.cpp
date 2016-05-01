// imm.cpp --- IME/IMM related
//////////////////////////////////////////////////////////////////////////////

#include "mzimeja.h"
#include "resource.h"

extern "C" {

//////////////////////////////////////////////////////////////////////////////

BOOL WINAPI ImeInquire(LPIMEINFO lpIMEInfo, LPTSTR lpszClassName,
                       DWORD dwSystemInfoFlags) {
  FOOTMARK();

  lpIMEInfo->dwPrivateDataSize = sizeof(UIEXTRA);
  lpIMEInfo->fdwProperty = IME_PROP_KBD_CHAR_FIRST |
                           IME_PROP_UNICODE | IME_PROP_AT_CARET;
  lpIMEInfo->fdwConversionCaps = IME_CMODE_LANGUAGE | IME_CMODE_FULLSHAPE |
                                 IME_CMODE_ROMAN | IME_CMODE_CHARCODE;
  lpIMEInfo->fdwSentenceCaps = 0L;
  lpIMEInfo->fdwUICaps = UI_CAP_2700;

  lpIMEInfo->fdwSCSCaps = 0;
  //lpIMEInfo->fdwSCSCaps = SCS_CAP_COMPSTR | SCS_CAP_MAKEREAD |
  //                        SCS_CAP_SETRECONVERTSTRING;

  lpIMEInfo->fdwSelectCaps = SELECT_CAP_CONVERSION;

  lstrcpy(lpszClassName, szUIServerClassName);

  if (dwSystemInfoFlags & IME_SYSINFO_WINLOGON) {
    TheIME.m_bWinLogOn = TRUE;
  }

  return TRUE;
}

DWORD WINAPI ImeConversionList(HIMC hIMC, LPCTSTR lpSource,
                               LPCANDIDATELIST lpCandList, DWORD dwBufLen,
                               UINT uFlags) {
  FOOTMARK();

  return 0;
}

BOOL WINAPI ImeDestroy(UINT uForce) {
  FOOTMARK();

  return TRUE;
}

LRESULT WINAPI ImeEscape(HIMC hIMC, UINT uSubFunc, LPVOID lpData) {
  LRESULT ret = FALSE;
  FOOTMARK();

  switch (uSubFunc) {
  case IME_ESC_QUERY_SUPPORT:
    switch (*(LPUINT)lpData) {
      case IME_ESC_QUERY_SUPPORT:
      case IME_ESC_GETHELPFILENAME:
        ret = TRUE;
        break;

      default:
        ret = FALSE;
        break;
    }
    break;

  case IME_ESC_GETHELPFILENAME:
    lstrcpy((LPTSTR)lpData, TEXT("mzimeja.hlp"));
    ret = TRUE;
    break;

  default:
    ret = FALSE;
    break;
  }

  return ret;
}

BOOL WINAPI ImeSetActiveContext(HIMC hIMC, BOOL fFlag) {
  FOOTMARK();

  TheIME.UpdateIndicIcon(hIMC);

  return TRUE;
}

BOOL WINAPI NotifyIME(HIMC hIMC, DWORD dwAction, DWORD dwIndex, DWORD dwValue) {
  InputContext *lpIMC;
  BOOL ret = FALSE;
  CandInfo *lpCandInfo;
  CandList *lpCandList;
  //LPDWORD lpdw;

  FOOTMARK();

  switch (dwAction) {
  case NI_CONTEXTUPDATED:
    DebugPrint(TEXT("NI_CONTEXTUPDATED\n"));
    switch (dwValue) {
    case IMC_SETOPENSTATUS:
      if (dwIndex == 0) {
        lpIMC = TheIME.LockIMC(hIMC);
        if (lpIMC) {
          lpIMC->CancelText();
          TheIME.UnlockIMC(hIMC);
        }
      }
      TheIME.UpdateIndicIcon(hIMC);
      ret = TRUE;
      break;

    case IMC_SETCONVERSIONMODE:
      break;

    case IMC_SETCOMPOSITIONWINDOW:
      break;

    default:
      break;
    }
    break;

  case NI_COMPOSITIONSTR:
    DebugPrint(TEXT("NI_COMPOSITIONSTR\n"));
    switch (dwIndex) {
    case CPS_COMPLETE:  // make result
      lpIMC = TheIME.LockIMC(hIMC);
      if (lpIMC) {
        lpIMC->MakeResult();
        TheIME.UnlockIMC(hIMC);
      }
      ret = TRUE;
      break;

    case CPS_CONVERT:   // do convert
      lpIMC = TheIME.LockIMC(hIMC);
      if (lpIMC) {
        lpIMC->DoConvert();
        ret = TRUE;
        TheIME.UnlockIMC(hIMC);
      }
      break;

    case CPS_REVERT:    // do revert
      lpIMC = TheIME.LockIMC(hIMC);
      if (lpIMC) {
        lpIMC->RevertText();
        TheIME.UnlockIMC(hIMC);
      }
      ret = TRUE;
      break;

    case CPS_CANCEL:    // do cancel
      lpIMC = TheIME.LockIMC(hIMC);
      if (lpIMC) {
        lpIMC->CancelText();
        TheIME.UnlockIMC(hIMC);
      }
      ret = TRUE;
      break;

    default:
      break;
    }
    break;

  case NI_OPENCANDIDATE:  // open candidate
    DebugPrint(TEXT("NI_OPENCANDIDATE\n"));
    lpIMC = TheIME.LockIMC(hIMC);
    if (lpIMC) {
      lpIMC->OpenCandidate();
      TheIME.UnlockIMC(hIMC);
    }
    break;

  case NI_CLOSECANDIDATE: // close candidate
    DebugPrint(TEXT("NI_CLOSECANDIDATE\n"));
    lpIMC = TheIME.LockIMC(hIMC);
    if (lpIMC) {
      ret = lpIMC->CloseCandidate();
      TheIME.UnlockIMC(hIMC);
    }
    break;

  case NI_SELECTCANDIDATESTR:
    DebugPrint(TEXT("NI_SELECTCANDIDATESTR\n"));
    lpIMC = TheIME.LockIMC(hIMC);
    if (lpIMC) {
      if (dwIndex == 1 && lpIMC->HasCandInfo()) {
        lpCandInfo = lpIMC->LockCandInfo();
        if (lpCandInfo) {
          lpCandList = lpCandInfo->GetList();
          if (lpCandList->dwCount > dwValue) {
            lpCandList->dwSelection = dwValue;
            TheIME.GenerateMessage(WM_IME_NOTIFY, IMN_CHANGECANDIDATE, 1);
            ret = TRUE;
          }
          lpIMC->UnlockCandInfo();
        }
      }
      TheIME.UnlockIMC(hIMC);
    }
    break;

  case NI_CHANGECANDIDATELIST:
    DebugPrint(TEXT("NI_CHANGECANDIDATELIST\n"));
    lpIMC = TheIME.LockIMC(hIMC);
    if (lpIMC) {
      if (dwIndex == 1 && lpIMC->HasCandInfo()) ret = TRUE;
      TheIME.UnlockIMC(hIMC);
    }
    break;

  case NI_SETCANDIDATE_PAGESIZE:
    DebugPrint(TEXT("NI_SETCANDIDATE_PAGESIZE\n"));
    lpIMC = TheIME.LockIMC(hIMC);
    if (lpIMC) {
      if (dwIndex == 1 && lpIMC->HasCandInfo()) {
        lpCandInfo = lpIMC->LockCandInfo();
        if (lpCandInfo && lpCandInfo->dwCount > 0) {
          CandList *lpCandList = lpCandInfo->GetList();
          lpCandList->dwPageSize = dwValue;
          lpIMC->UnlockCandInfo();
          TheIME.GenerateMessage(WM_IME_NOTIFY, IMN_CHANGECANDIDATE, 1);
          ret = TRUE;
        }
      }
      TheIME.UnlockIMC(hIMC);
    }
    break;

  case NI_SETCANDIDATE_PAGESTART:
    DebugPrint(TEXT("NI_SETCANDIDATE_PAGESTART\n"));
    lpIMC = TheIME.LockIMC(hIMC);
    if (lpIMC) {
      if (dwIndex == 1 && lpIMC->HasCandInfo()) {
        lpCandInfo = lpIMC->LockCandInfo();
        if (lpCandInfo) {
          lpCandList = lpCandInfo->GetList();
          if (dwValue < lpCandList->dwCount) {
            lpCandList->dwPageStart = dwValue;
            TheIME.GenerateMessage(WM_IME_NOTIFY, IMN_CHANGECANDIDATE, 1);
            ret = TRUE;
          }
          lpIMC->UnlockCandInfo();
        }
      }
      TheIME.UnlockIMC(hIMC);
    }
    break;

  case NI_IMEMENUSELECTED:
    DebugPrint(TEXT("NI_IMEMENUSELECTED\n"));
    TheIME.DoCommand(hIMC, dwIndex);
    break;

  default:
    DebugPrint(TEXT("NI_(unknown)\n"));
    break;
  }

  return ret;
}

BOOL WINAPI ImeSelect(HIMC hIMC, BOOL fSelect) {
  FOOTMARK();

  if (fSelect) TheIME.UpdateIndicIcon(hIMC);
  if (NULL != hIMC) {
    InputContext *lpIMC = TheIME.LockIMC(hIMC);
    if (lpIMC) {
      if (fSelect) {
        lpIMC->Initialize();
      }
      TheIME.UnlockIMC(hIMC);
    }
  }
  return TRUE;
}

#ifdef _DEBUG
void DumpRS(LPRECONVERTSTRING lpRS) {
  LPTSTR lpDump = ((LPTSTR)lpRS) + lpRS->dwStrOffset;
  *(LPTSTR)(lpDump + lpRS->dwStrLen) = 0;

  DebugPrint(TEXT("DumpRS\n"));
  DebugPrint(TEXT("dwSize            %x\n"), lpRS->dwSize);
  DebugPrint(TEXT("dwStrLen          %x\n"), lpRS->dwStrLen);
  DebugPrint(TEXT("dwStrOffset       %x\n"), lpRS->dwStrOffset);
  DebugPrint(TEXT("dwCompStrLen      %x\n"), lpRS->dwCompStrLen);
  DebugPrint(TEXT("dwCompStrOffset   %x\n"), lpRS->dwCompStrOffset);
  DebugPrint(TEXT("dwTargetStrLen    %x\n"), lpRS->dwTargetStrLen);
  DebugPrint(TEXT("dwTargetStrOffset %x\n"), lpRS->dwTargetStrOffset);
  DebugPrint(TEXT("%s\n"), lpDump);
}
#endif  // ndef _DEBUG

BOOL WINAPI ImeSetCompositionString(HIMC hIMC, DWORD dwIndex, LPVOID lpComp,
                                    DWORD dwComp, LPVOID lpRead, DWORD dwRead) {
  FOOTMARK();

  switch (dwIndex) {
    case SCS_QUERYRECONVERTSTRING:
      DebugPrint(TEXT("SCS_QUERYRECONVERTSTRING\n"));
      break;

    case SCS_SETRECONVERTSTRING:
      DebugPrint(TEXT("SCS_SETRECONVERTSTRING\n"));
      break;

    default:
      DebugPrint(TEXT("SCS_(unknown)\n"));
      break;
  }

  return FALSE;
}

struct MYMENUITEM {
  INT nCommandID;
  INT nStringID;
};
static const MYMENUITEM top_menu_items[] = {
  {IDM_HIRAGANA, IDM_HIRAGANA},
  {IDM_ZEN_KATAKANA, IDM_ZEN_KATAKANA},
  {IDM_ZEN_ALNUM, IDM_ZEN_ALNUM},
  {IDM_HAN_KATAKANA, IDM_HAN_KATAKANA},
  {IDM_ALNUM, IDM_ALNUM},
  {-1, -1},
  {IDM_ROMAN_INPUT, IDM_ROMAN_INPUT},
  {IDM_KANA_INPUT, IDM_KANA_INPUT},
  {-1, -1},
  {IDM_ADD_WORD, IDM_ADD_WORD},
  {IDM_RECONVERT, IDM_RECONVERT},
  {-1, -1},
  {IDM_PROPERTY, IDM_PROPERTY},
  {IDM_ABOUT, IDM_ABOUT},
};

DWORD WINAPI ImeGetImeMenuItems(HIMC hIMC, DWORD dwFlags, DWORD dwType,
                                LPIMEMENUITEMINFO lpImeParentMenu,
                                LPIMEMENUITEMINFO lpImeMenu, DWORD dwSize) {
  FOOTMARK();
  INT ret = 0;

  if (lpImeMenu == NULL) {
    if (lpImeParentMenu == NULL) {
      if (dwFlags & IGIMIF_RIGHTMENU)
        ret = _countof(top_menu_items);
    }
    return ret;
  }

  if (lpImeParentMenu == NULL) {
    if (dwFlags & IGIMIF_RIGHTMENU) {
      BOOL bOpen;
      bOpen = ImmGetOpenStatus(hIMC);
      DWORD dwConversion, dwSentence;
      ImmGetConversionStatus(hIMC, &dwConversion, &dwSentence);
      INPUT_MODE imode;
      imode = InputModeFromConversionMode(bOpen, dwConversion);

      for (size_t i = 0; i < _countof(top_menu_items); ++i) {
        const MYMENUITEM& item = top_menu_items[i];
        lpImeMenu[i].cbSize = sizeof(IMEMENUITEMINFO);
        lpImeMenu[i].fState = 0;
        switch (item.nCommandID) {
        case -1:
          lpImeMenu[i].fType = IMFT_SEPARATOR;
          break;
        case IDM_HIRAGANA:
          lpImeMenu[i].fType = IMFT_RADIOCHECK;
          if (imode == IMODE_ZEN_HIRAGANA) {
            lpImeMenu[i].fState = IMFS_CHECKED;
          }
          break;
        case IDM_ZEN_KATAKANA:
          lpImeMenu[i].fType = IMFT_RADIOCHECK;
          if (imode == IMODE_ZEN_KATAKANA) {
            lpImeMenu[i].fState = IMFS_CHECKED;
          }
          break;
        case IDM_ZEN_ALNUM:
          lpImeMenu[i].fType = IMFT_RADIOCHECK;
          if (imode == IMODE_ZEN_EISUU) {
            lpImeMenu[i].fState = IMFS_CHECKED;
          }
          break;
        case IDM_HAN_KATAKANA:
          lpImeMenu[i].fType = IMFT_RADIOCHECK;
          if (imode == IMODE_HAN_KANA) {
            lpImeMenu[i].fState = IMFS_CHECKED;
          }
          break;
        case IDM_ALNUM:
          lpImeMenu[i].fType = IMFT_RADIOCHECK;
          if (imode == IMODE_HAN_EISUU) {
            lpImeMenu[i].fState = IMFS_CHECKED;
          }
          break;
        case IDM_ROMAN_INPUT:
        case IDM_KANA_INPUT:
          lpImeMenu[i].fType = IMFT_RADIOCHECK;
          if (dwConversion & IME_CMODE_ROMAN) {
            if (item.nCommandID == IDM_ROMAN_INPUT) {
              lpImeMenu[i].fState = IMFS_CHECKED;
            }
          } else {
            if (item.nCommandID == IDM_KANA_INPUT) {
              lpImeMenu[i].fState = IMFS_CHECKED;
            }
          }
          break;
        default:
          lpImeMenu[i].fType = 0;
          break;
        }
        lpImeMenu[i].wID = item.nCommandID;
        lpImeMenu[i].hbmpChecked = 0;
        lpImeMenu[i].hbmpUnchecked = 0;
        if (item.nStringID != -1) {
          lstrcpy(lpImeMenu[i].szString, TheIME.LoadSTR(item.nStringID));
        } else {
          lpImeMenu[i].szString[0] = TEXT('\0');
        }
        lpImeMenu[i].hbmpItem = 0;
      }
      ret = _countof(top_menu_items);
    }
  }

  return ret;
} // ImeGetImeMenuItems

//////////////////////////////////////////////////////////////////////////////

}  // extern "C"

//////////////////////////////////////////////////////////////////////////////
