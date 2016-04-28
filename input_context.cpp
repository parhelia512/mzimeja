// input_context.cpp --- input context
//////////////////////////////////////////////////////////////////////////////

#include "mzimeja.h"

//////////////////////////////////////////////////////////////////////////////

void InputContext::Initialize() {
  FOOTMARK();
  if (!HasLogFont()) {
    lfFont.W.lfCharSet = SHIFTJIS_CHARSET;
    lfFont.W.lfFaceName[0] = L'\0';
    fdwInit |= INIT_LOGFONT;
  }

  if (!HasConversion()) {
    fdwConversion = IME_CMODE_ROMAN | IME_CMODE_FULLSHAPE | IME_CMODE_JAPANESE;
    fdwInit |= INIT_CONVERSION;
  }

  hCompStr = CompStr::ReCreate(hCompStr);
  hCandInfo = CandInfo::ReCreate(hCandInfo);
}

BOOL InputContext::HasCandInfo() {
  FOOTMARK();
  BOOL fRet = FALSE;

  if (ImmGetIMCCSize(hCandInfo) < sizeof(CANDIDATEINFO)) return FALSE;

  CandInfo *lpCandInfo = LockCandInfo();
  if (lpCandInfo) {
    fRet = (lpCandInfo->dwCount > 0);
    UnlockCandInfo();
  }
  return fRet;
}

BOOL InputContext::HasCompStr() {
  FOOTMARK();
  if (ImmGetIMCCSize(hCompStr) <= sizeof(COMPOSITIONSTRING)) return FALSE;

  CompStr *pCompStr = LockCompStr();
  BOOL ret = (pCompStr->dwCompStrLen > 0);
  UnlockCompStr();

  return ret;
}

CandInfo *InputContext::LockCandInfo() {
  DebugPrint(TEXT("InputContext::LockCandInfo: locking %p\n"), hCandInfo);
  CandInfo *info = (CandInfo *)ImmLockIMCC(hCandInfo);
  DebugPrint(TEXT("InputContext::LockCandInfo: locked %p\n"), hCandInfo);
  return info;
}

void InputContext::UnlockCandInfo() {
  DebugPrint(TEXT("InputContext::UnlockCandInfo: unlocking %p\n"), hCandInfo);
  ImmUnlockIMCC(hCandInfo);
  DebugPrint(TEXT("InputContext::UnlockCandInfo: unlocked %p\n"), hCandInfo);
}

CompStr *InputContext::LockCompStr() {
  DebugPrint(TEXT("InputContext::LockCompStr: locking %p\n"), hCompStr);
  CompStr *comp_str = (CompStr *)ImmLockIMCC(hCompStr);
  DebugPrint(TEXT("InputContext::LockCompStr: locked %p\n"), hCompStr);
  return comp_str;
}

void InputContext::UnlockCompStr() {
  DebugPrint(TEXT("InputContext::UnlockCompStr: unlocking %p\n"), hCompStr);
  ImmUnlockIMCC(hCompStr);
  DebugPrint(TEXT("InputContext::UnlockCompStr: unlocked %p\n"), hCompStr);
}

LPTRANSMSG InputContext::LockMsgBuf() {
  DebugPrint(TEXT("InputContext::LockMsgBuf: locking %p\n"), hMsgBuf);
  LPTRANSMSG lpTransMsg = (LPTRANSMSG)ImmLockIMCC(hMsgBuf);
  DebugPrint(TEXT("InputContext::LockMsgBuf: locked %p\n"), hMsgBuf);
  return lpTransMsg;
}

void InputContext::UnlockMsgBuf() {
  DebugPrint(TEXT("InputContext::UnlockMsgBuf: unlocking %p\n"), hMsgBuf);
  ImmUnlockIMCC(hMsgBuf);
  DebugPrint(TEXT("InputContext::UnlockMsgBuf: unlocked %p\n"), hMsgBuf);
}

DWORD& InputContext::NumMsgBuf() {
  return dwNumMsgBuf;
}

const DWORD& InputContext::NumMsgBuf() const {
  return dwNumMsgBuf;
}

void InputContext::MakeGuideLine(DWORD dwID) {
  FOOTMARK();
  DWORD dwSize =
      sizeof(GUIDELINE) + (MAXGLCHAR + sizeof(TCHAR)) * 2 * sizeof(TCHAR);
  LPTSTR lpStr;

  hGuideLine = ImmReSizeIMCC(hGuideLine, dwSize);
  LPGUIDELINE lpGuideLine = LockGuideLine();

  lpGuideLine->dwSize = dwSize;
  lpGuideLine->dwLevel = glTable[dwID].dwLevel;
  lpGuideLine->dwIndex = glTable[dwID].dwIndex;
  lpGuideLine->dwStrOffset = sizeof(GUIDELINE);
  lpStr = (LPTSTR)((LPBYTE)lpGuideLine + lpGuideLine->dwStrOffset);
  LoadString(TheIME.m_hInst, glTable[dwID].dwStrID, lpStr, MAXGLCHAR);
  lpGuideLine->dwStrLen = lstrlen(lpStr);

  if (glTable[dwID].dwPrivateID) {
    lpGuideLine->dwPrivateOffset =
        sizeof(GUIDELINE) + (MAXGLCHAR + 1) * sizeof(TCHAR);
    lpStr = (LPTSTR)((LPBYTE)lpGuideLine + lpGuideLine->dwPrivateOffset);
    LoadString(TheIME.m_hInst, glTable[dwID].dwStrID, lpStr, MAXGLCHAR);
    lpGuideLine->dwPrivateSize = lstrlen(lpStr) * sizeof(TCHAR);
  } else {
    lpGuideLine->dwPrivateOffset = 0L;
    lpGuideLine->dwPrivateSize = 0L;
  }

  TheIME.GenerateMessage(WM_IME_NOTIFY, IMN_GUIDELINE, 0);

  UnlockGuideLine();
}

LPGUIDELINE InputContext::LockGuideLine() {
  DebugPrint(TEXT("InputContext::LockGuideLine: locking %p\n"), hGuideLine);
  LPGUIDELINE guideline = (LPGUIDELINE)ImmLockIMCC(hGuideLine);
  DebugPrint(TEXT("InputContext::LockGuideLine: locked %p\n"), hGuideLine);
  return guideline;
}

void InputContext::UnlockGuideLine() {
  DebugPrint(TEXT("InputContext::UnlockGuideLine: unlocking %p\n"), hGuideLine);
  ImmUnlockIMCC(hGuideLine);
  DebugPrint(TEXT("InputContext::UnlockGuideLine: unlocked %p\n"), hGuideLine);
}

void InputContext::AddChar(WCHAR ch) {
  FOOTMARK();

  // get logical data
  LogCompStr log;
  CompStr *lpCompStr = LockCompStr();
  if (lpCompStr) {
    lpCompStr->GetLog(log);
    UnlockCompStr();
  }

  if (log.comp_str.empty()) {
    // start composition
    TheIME.GenerateMessage(WM_IME_STARTCOMPOSITION);
  }

  // fix cursor pos
  std::wstring& comp_str = log.comp_str;
  if ((DWORD)comp_str.size() < log.dwCursorPos) {
    log.dwCursorPos = (DWORD)comp_str.size();
  }

  // enter a char depending on the conversion mode
  if (Conversion() & IME_CMODE_FULLSHAPE) {
    if (Conversion() & IME_CMODE_ROMAN) {
      add_romaji_char(comp_str, ch, log.dwCursorPos);
    } else {
      if (Conversion() & IME_CMODE_KATAKANA) {
        add_katakana_char(comp_str, ch, log.dwCursorPos);
      } else {
        add_hiragana_char(comp_str, ch, log.dwCursorPos);
      }
    }
  } else {
    add_ascii_char(comp_str, ch, log.dwCursorPos);
  }

  // update info
  log.comp_read_str = comp_str;
  log.comp_read_attr.resize(comp_str.size(), ATTR_INPUT);
  log.comp_attr.resize(comp_str.size(), ATTR_INPUT);
  log.comp_read_clause.resize(2);
  log.comp_read_clause[0] = 0;
  log.comp_read_clause[1] = (DWORD)log.comp_read_str.size();

  log.comp_clause.resize(2);
  log.comp_clause[0] = 0;
  log.comp_clause[1] = (DWORD)comp_str.size();

  // recreate
  DumpCompStr();
  hCompStr = CompStr::ReCreate(hCompStr, &log);
  DumpCompStr();

  LPARAM lParam = GCS_COMPALL | GCS_CURSORPOS | GCS_DELTASTART;
  TheIME.GenerateMessage(WM_IME_COMPOSITION, 0, lParam);
} // InputContext::AddChar

void InputContext::GetCands(LogCandInfo& log_cand_info, std::wstring& str) {
  DWORD dwCount = (DWORD)log_cand_info.cand_strs.size();
  if (dwCount > 0) {
    log_cand_info.dwSelection++;
    if (log_cand_info.dwSelection >= dwCount) {
      log_cand_info.dwSelection = 0;
    }
  } else {
    log_cand_info.dwSelection = 0;
    log_cand_info.cand_strs.push_back(L"これは");
    log_cand_info.cand_strs.push_back(L"テスト");
    log_cand_info.cand_strs.push_back(L"です。");
  }
  str = log_cand_info.cand_strs[log_cand_info.dwSelection];
}

BOOL InputContext::DoConvert() {
  FOOTMARK();

  if ((GetFileAttributes(TheIME.m_szDicFileName) == 0xFFFFFFFF) ||
      (GetFileAttributes(TheIME.m_szDicFileName) & FILE_ATTRIBUTE_DIRECTORY)) {
    MakeGuideLine(MYGL_NODICTIONARY);
    return FALSE;
  }

  BOOL bHasCompStr = FALSE, bIsBeingConverted = FALSE;
  LogCompStr log_comp_str;
  CompStr *lpCompStr = LockCompStr();
  if (lpCompStr) {
    lpCompStr->GetLog(log_comp_str);
    bIsBeingConverted = lpCompStr->IsBeingConverted();
    UnlockCompStr();
    bHasCompStr = (log_comp_str.comp_str.size() > 0);
  }

  if (!bHasCompStr) {
    return FALSE;
  }

  if (bIsBeingConverted) {
    // the composition string is being converted.
    if (!HasCandInfo()) {
      // there is no candidate.
      // open candidate
      TheIME.GenerateMessage(WM_IME_NOTIFY, IMN_OPENCANDIDATE, 1);
    }
  }

  // get logical data of candidate info
  LogCandInfo log_cand_info;
  CandInfo *lpCandInfo = LockCandInfo();
  if (lpCandInfo) {
    lpCandInfo->GetLog(log_cand_info);
    UnlockCandInfo();
  }

  // get candidates
  std::wstring str = log_comp_str.comp_str;
  GetCands(log_cand_info, str);
  log_comp_str.comp_str = str;

  // recreate candidate
  hCandInfo = CandInfo::ReCreate(hCandInfo, &log_cand_info);
  // generate message to change candidate
  TheIME.GenerateMessage(WM_IME_NOTIFY, IMN_CHANGECANDIDATE, 1);

  // set composition string
  log_comp_str.comp_str = str;
  log_comp_str.comp_attr.assign(str.size(), ATTR_TARGET_CONVERTED);
  log_comp_str.comp_clause.resize(2);
  log_comp_str.comp_clause[0] = 0;
  log_comp_str.comp_clause[1] = str.size();
  // recreate composition
  DumpCompStr();
  hCompStr = CompStr::ReCreate(hCompStr, &log_comp_str);
  DumpCompStr();

  // generate message to change composition
  LPARAM lParam = GCS_COMPALL | GCS_CURSORPOS | GCS_DELTASTART;
  TheIME.GenerateMessage(WM_IME_COMPOSITION, 0, lParam);

  return TRUE;
} // InputContext::DoConvert

void InputContext::MakeResult() {
  FOOTMARK();

  // close candidate
  if (HasCandInfo()) {
    hCandInfo = CandInfo::ReCreate(hCandInfo);
    TheIME.GenerateMessage(WM_IME_NOTIFY, IMN_CLOSECANDIDATE, 1);
  }

  // get logical data
  LogCompStr log;
  CompStr *lpCompStr = LockCompStr();
  if (lpCompStr) {
    lpCompStr->GetLog(log);
    UnlockCompStr();
  }

  // set result
  log.result_read_clause = log.comp_read_clause;
  log.result_read_str = log.comp_read_str;
  log.result_clause = log.comp_clause;
  log.result_str = log.comp_str;

  // clear compostion
  log.dwCursorPos = 0;
  log.dwDeltaStart = 0;
  log.comp_read_attr.clear();
  log.comp_read_clause.clear();
  log.comp_read_str.clear();
  log.comp_attr.clear();
  log.comp_clause.clear();
  log.comp_str.clear();

  // recreate
  DumpCompStr();
  hCompStr = CompStr::ReCreate(hCompStr, &log);
  DumpCompStr();

  // generate messages to end composition
  TheIME.GenerateMessage(WM_IME_COMPOSITION, 0, GCS_COMPALL | GCS_RESULTALL);
  TheIME.GenerateMessage(WM_IME_ENDCOMPOSITION);
} // InputContext::MakeResult

void InputContext::MakeHiragana() {
  FOOTMARK();

  // close candidate
  if (HasCandInfo()) {
    hCandInfo = CandInfo::ReCreate(hCandInfo);
    TheIME.GenerateMessage(WM_IME_NOTIFY, IMN_CLOSECANDIDATE, 1);
  }

  // get logical data
  LogCompStr log;
  CompStr *lpCompStr = LockCompStr();
  if (lpCompStr) {
    lpCompStr->GetLog(log);
    UnlockCompStr();
  }

  std::wstring str;
  str = romaji_to_hiragana(log.comp_read_str);
  str = zenkaku_katakana_to_hiragana(str);

  // update composition
  log.comp_str = str;
  DWORD len = (DWORD)log.comp_str.size();
  log.dwCursorPos = len;
  log.dwDeltaStart = 0;
  log.comp_attr.assign(len, ATTR_INPUT);
  log.comp_clause.resize(2);
  log.comp_clause[0] = 0;
  log.comp_clause[1] = len;

  // recreate
  DumpCompStr();
  hCompStr = CompStr::ReCreate(hCompStr, &log);
  DumpCompStr();

  // generate messages to update composition
  TheIME.GenerateMessage(WM_IME_COMPOSITION, 0, GCS_COMPALL | GCS_RESULTALL);
}

void InputContext::MakeKatakana() {
  FOOTMARK();

  // close candidate
  if (HasCandInfo()) {
    hCandInfo = CandInfo::ReCreate(hCandInfo);
    TheIME.GenerateMessage(WM_IME_NOTIFY, IMN_CLOSECANDIDATE, 1);
  }

  // get logical data
  LogCompStr log;
  CompStr *lpCompStr = LockCompStr();
  if (lpCompStr) {
    lpCompStr->GetLog(log);
    UnlockCompStr();
  }

  std::wstring str;
  str = romaji_to_hiragana(log.comp_read_str);
  str = zenkaku_hiragana_to_katakana(str);

  // update composition
  log.comp_str = str;
  DWORD len = (DWORD)log.comp_str.size();
  log.dwCursorPos = len;
  log.dwDeltaStart = 0;
  log.comp_attr.assign(len, ATTR_INPUT);
  log.comp_clause.resize(2);
  log.comp_clause[0] = 0;
  log.comp_clause[1] = len;

  // recreate
  DumpCompStr();
  hCompStr = CompStr::ReCreate(hCompStr, &log);
  DumpCompStr();

  // generate messages to update composition
  TheIME.GenerateMessage(WM_IME_COMPOSITION, 0, GCS_COMPALL | GCS_RESULTALL);
}

void InputContext::MakeHankaku() {
  FOOTMARK();

  // close candidate
  if (HasCandInfo()) {
    hCandInfo = CandInfo::ReCreate(hCandInfo);
    TheIME.GenerateMessage(WM_IME_NOTIFY, IMN_CLOSECANDIDATE, 1);
  }

  // get logical data
  LogCompStr log;
  CompStr *lpCompStr = LockCompStr();
  if (lpCompStr) {
    lpCompStr->GetLog(log);
    UnlockCompStr();
  }

  std::wstring str;
  str = romaji_to_hiragana(log.comp_read_str);
  str = zenkaku_to_hankaku(str);

  // update composition
  log.comp_str = str;
  DWORD len = (DWORD)log.comp_str.size();
  log.dwCursorPos = len;
  log.dwDeltaStart = 0;
  log.comp_attr.assign(len, ATTR_INPUT);
  log.comp_clause.resize(2);
  log.comp_clause[0] = 0;
  log.comp_clause[1] = len;

  // recreate
  DumpCompStr();
  hCompStr = CompStr::ReCreate(hCompStr, &log);
  DumpCompStr();

  // generate messages to update composition
  TheIME.GenerateMessage(WM_IME_COMPOSITION, 0, GCS_COMPALL | GCS_RESULTALL);
}

void InputContext::MakeZenEisuu() {
  FOOTMARK();

  // close candidate
  if (HasCandInfo()) {
    hCandInfo = CandInfo::ReCreate(hCandInfo);
    TheIME.GenerateMessage(WM_IME_NOTIFY, IMN_CLOSECANDIDATE, 1);
  }

  // get logical data
  LogCompStr log;
  CompStr *lpCompStr = LockCompStr();
  if (lpCompStr) {
    lpCompStr->GetLog(log);
    UnlockCompStr();
  }

  std::wstring str;
  str = hiragana_to_romaji(log.comp_read_str);
  str = hankaku_to_zenkaku(str);

  // update composition
  log.comp_str = str;
  DWORD len = (DWORD)log.comp_str.size();
  log.dwCursorPos = len;
  log.dwDeltaStart = 0;
  log.comp_attr.assign(len, ATTR_INPUT);
  log.comp_clause.resize(2);
  log.comp_clause[0] = 0;
  log.comp_clause[1] = len;

  // recreate
  DumpCompStr();
  hCompStr = CompStr::ReCreate(hCompStr, &log);
  DumpCompStr();

  // generate messages to update composition
  TheIME.GenerateMessage(WM_IME_COMPOSITION, 0, GCS_COMPALL | GCS_RESULTALL);
}

void InputContext::MakeHanEisuu() {
  FOOTMARK();

  // close candidate
  if (HasCandInfo()) {
    hCandInfo = CandInfo::ReCreate(hCandInfo);
    TheIME.GenerateMessage(WM_IME_NOTIFY, IMN_CLOSECANDIDATE, 1);
  }

  // get logical data
  LogCompStr log;
  CompStr *lpCompStr = LockCompStr();
  if (lpCompStr) {
    lpCompStr->GetLog(log);
    UnlockCompStr();
  }

  std::wstring str;
  str = hiragana_to_romaji(log.comp_read_str);
  str = zenkaku_to_hankaku(str);

  // update composition
  log.comp_str = str;
  DWORD len = (DWORD)log.comp_str.size();
  log.dwCursorPos = len;
  log.dwDeltaStart = 0;
  log.comp_attr.assign(len, ATTR_INPUT);
  log.comp_clause.resize(2);
  log.comp_clause[0] = 0;
  log.comp_clause[1] = len;

  // recreate
  DumpCompStr();
  hCompStr = CompStr::ReCreate(hCompStr, &log);
  DumpCompStr();

  // generate messages to update composition
  TheIME.GenerateMessage(WM_IME_COMPOSITION, 0, GCS_COMPALL | GCS_RESULTALL);
}

void InputContext::CancelText() {
  FOOTMARK();

  // close candidate
  if (HasCandInfo()) {
    hCandInfo = CandInfo::ReCreate(hCandInfo);
    TheIME.GenerateMessage(WM_IME_NOTIFY, IMN_CLOSECANDIDATE, 1);
  }

  DumpCompStr();
  hCompStr = CompStr::ReCreate(hCompStr);
  DumpCompStr();

  // generate messages to end composition
  TheIME.GenerateMessage(WM_IME_COMPOSITION, 0, GCS_COMPALL);
  TheIME.GenerateMessage(WM_IME_ENDCOMPOSITION);
} // InputContext::CancelText

void InputContext::RevertText() {
  FOOTMARK();

  // close candidate
  if (HasCandInfo()) {
    hCandInfo = CandInfo::ReCreate(hCandInfo);
    TheIME.GenerateMessage(WM_IME_NOTIFY, IMN_CLOSECANDIDATE, 1);
  }

  // return if no composition string
  if (!HasCompStr()) {
    return;
  }

  // get logical data
  LogCompStr log;
  CompStr *lpCompStr = LockCompStr();
  if (lpCompStr) {
    lpCompStr->GetLog(log);
    UnlockCompStr();
  }

  // reset composition
  log.comp_str = log.comp_read_str;
  log.comp_clause.resize(2);
  log.comp_clause[0] = 0;
  log.comp_clause[1] = (DWORD)log.comp_str.size();
  log.comp_attr.assign(log.comp_read_str.size(), ATTR_INPUT);
  log.dwCursorPos = (DWORD)log.comp_str.size();
  log.dwDeltaStart = 0;

  // recreate
  DumpCompStr();
  hCompStr = CompStr::ReCreate(hCompStr, &log);
  DumpCompStr();

  LPARAM lParam = GCS_COMPALL | GCS_CURSORPOS | GCS_DELTASTART;
  TheIME.GenerateMessage(WM_IME_COMPOSITION, 0, lParam);
} // InputContext::RevertText

void InputContext::DeleteChar(BOOL bBackSpace) {
  FOOTMARK();

  // get logical data
  LogCompStr log;
  CompStr *lpCompStr = LockCompStr();
  if (lpCompStr) {
    lpCompStr->GetLog(log);
    UnlockCompStr();
  }

  // delete char
  if (bBackSpace) {
    if (log.dwCursorPos == 0) {
      DebugPrint(TEXT("log.dwCursorPos == 0\n"));
      return;
    } else if (log.dwCursorPos <= log.comp_str.size()) {
      --log.dwCursorPos;
      log.comp_str.erase(log.dwCursorPos);
      log.dwDeltaStart = log.dwCursorPos;
    } else {
      log.dwCursorPos = (DWORD)log.comp_str.size();
      log.dwDeltaStart = log.dwCursorPos;
    }
  } else {
    if (log.dwCursorPos >= log.comp_str.size()) {
      DebugPrint(TEXT("log.dwCursorPos >= log.comp_str.size()\n"));
      return;
    } else {
      log.comp_str.erase(log.dwCursorPos);
      log.dwCursorPos = log.dwDeltaStart;
    }
  }

  // update info
  log.comp_read_str = log.comp_str;
  log.comp_read_attr.resize(log.comp_str.size(), ATTR_INPUT);
  log.comp_attr.resize(log.comp_str.size(), ATTR_INPUT);
  log.comp_read_clause.resize(2);
  log.comp_read_clause[0] = 0;
  log.comp_read_clause[1] = (DWORD)log.comp_read_str.size();
  log.comp_clause.resize(2);
  log.comp_clause[0] = 0;
  log.comp_clause[1] = (DWORD)log.comp_str.size();

  // recreate
  DumpCompStr();
  hCompStr = CompStr::ReCreate(hCompStr, &log);
  DumpCompStr();

  if (log.comp_str.empty()) {
    // close candidate if any
    if (HasCandInfo()) {
      hCandInfo = CandInfo::ReCreate(hCandInfo);
    }

    // generate messages to end composition
    TheIME.GenerateMessage(WM_IME_COMPOSITION);
    TheIME.GenerateMessage(WM_IME_ENDCOMPOSITION);
  } else {
    // update composition
    LPARAM lParam = GCS_COMPALL | GCS_CURSORPOS | GCS_DELTASTART;
    TheIME.GenerateMessage(WM_IME_COMPOSITION, 0, lParam);
  }
} // InputContext::DeleteChar

void InputContext::MoveLeft() {
  FOOTMARK();

  // get logical data
  LogCompStr log;
  BOOL bIsBeingConverted = FALSE;
  CompStr *lpCompStr = LockCompStr();
  if (lpCompStr) {
    bIsBeingConverted = lpCompStr->IsBeingConverted();
    lpCompStr->GetLog(log);
    UnlockCompStr();
  }

  DWORD dwCursorPos = log.dwCursorPos;
  if (bIsBeingConverted) {
    size_t i, siz = log.comp_clause.size();
    if (siz > 1) {
      for (i = 0; i < siz; ++i) {
        if (dwCursorPos <= log.comp_clause[i]) {
          if (i == 0) {
            i = siz - 2;
          } else {
            --i;
          }
          break;
        }
      }
      dwCursorPos = log.comp_clause[i];
    }
  } else {
    if (log.dwCursorPos > 0) {
      --dwCursorPos;
    } else {
      return;
    }
  }
  log.dwCursorPos = dwCursorPos;

  // recreate
  DumpCompStr();
  hCompStr = CompStr::ReCreate(hCompStr, &log);
  DumpCompStr();

  // update composition
  TheIME.GenerateMessage(WM_IME_COMPOSITION, 0, GCS_CURSORPOS);
} // InputContext::MoveLeft

void InputContext::MoveRight() {
  FOOTMARK();

  // get logical data
  LogCompStr log;
  BOOL bIsBeingConverted = FALSE;
  CompStr *lpCompStr = LockCompStr();
  if (lpCompStr) {
    bIsBeingConverted = lpCompStr->IsBeingConverted();
    lpCompStr->GetLog(log);
    UnlockCompStr();
  }

  DWORD dwCursorPos = log.dwCursorPos;
  if (bIsBeingConverted) {
    size_t i, k, siz = log.comp_clause.size();
    if (siz > 1) {
      for (i = k = 0; i < siz; ++i) {
        if (log.comp_clause[i] <= dwCursorPos) {
          if (siz <= i + 1) {
            k = 0;
          } else {
            k = i + 1;
          }
        }
      }
      dwCursorPos = log.comp_clause[k];
    }
  } else {
    if (log.dwCursorPos < (DWORD)log.comp_str.size()) {
      ++dwCursorPos;
    } else {
      return;
    }
  }
  log.dwCursorPos = dwCursorPos;

  // recreate
  DumpCompStr();
  hCompStr = CompStr::ReCreate(hCompStr, &log);
  DumpCompStr();

  // update composition
  TheIME.GenerateMessage(WM_IME_COMPOSITION, 0, GCS_CURSORPOS);
} // InputContext::MoveRight

void InputContext::MoveToBeginning() {
  FOOTMARK();

  // get logical data
  LogCompStr log;
  CompStr *lpCompStr = LockCompStr();
  if (lpCompStr) {
    lpCompStr->GetLog(log);
    UnlockCompStr();
  }

  log.dwCursorPos = 0;

  // recreate
  DumpCompStr();
  hCompStr = CompStr::ReCreate(hCompStr, &log);
  DumpCompStr();

  // update composition
  TheIME.GenerateMessage(WM_IME_COMPOSITION, 0, GCS_CURSORPOS);
}

void InputContext::MoveToEnd() {
  FOOTMARK();

  // get logical data
  LogCompStr log;
  CompStr *lpCompStr = LockCompStr();
  if (lpCompStr) {
    lpCompStr->GetLog(log);
    UnlockCompStr();
  }

  log.dwCursorPos = (DWORD)log.comp_str.size();

  // recreate
  DumpCompStr();
  hCompStr = CompStr::ReCreate(hCompStr, &log);
  DumpCompStr();

  // update composition
  TheIME.GenerateMessage(WM_IME_COMPOSITION, 0, GCS_CURSORPOS);
}

void InputContext::DumpCompStr() {
  FOOTMARK();
#ifndef NDEBUG
  CompStr *pCompStr = LockCompStr();
  if (pCompStr) {
    pCompStr->Dump();
    UnlockCompStr();
  } else {
    DebugPrint(TEXT("(no comp str)\n"));
  }
#endif
} // InputContext::DumpCompStr

void InputContext::DumpCandInfo() {
  FOOTMARK();
#ifndef NDEBUG
  CandInfo *pCandInfo = LockCandInfo();
  if (pCandInfo) {
    pCandInfo->Dump();
    UnlockCandInfo();
  } else {
    DebugPrint(TEXT("(no cand info)\n"));
  }
#endif
} // InputContext::DumpCandInfo

//////////////////////////////////////////////////////////////////////////////
