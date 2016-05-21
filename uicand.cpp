// uicand.cpp --- mzimeja candidate window UI
//////////////////////////////////////////////////////////////////////////////

#include "mzimeja.h"

extern "C" {

//////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK CandWnd_WindowProc(HWND hWnd, UINT message, WPARAM wParam,
                                    LPARAM lParam) {
  FOOTMARK();
  HWND hUIWnd;

  switch (message) {
    case WM_PAINT:
      CandWnd_Paint(hWnd);
      break;

    case WM_SETCURSOR:
    case WM_MOUSEMOVE:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
      DragUI(hWnd, message, wParam, lParam);
      if ((message == WM_SETCURSOR) && (HIWORD(lParam) != WM_LBUTTONDOWN) &&
          (HIWORD(lParam) != WM_RBUTTONDOWN))
        return ::DefWindowProc(hWnd, message, wParam, lParam);
      if ((message == WM_LBUTTONUP) || (message == WM_RBUTTONUP))
        ::SetWindowLong(hWnd, FIGWL_MOUSE, 0L);
      break;

    case WM_MOVE:
      hUIWnd = (HWND)GetWindowLongPtr(hWnd, FIGWLP_SERVERWND);
      if (::IsWindow(hUIWnd)) ::SendMessage(hUIWnd, WM_UI_CANDMOVE, wParam, lParam);
      break;

    default:
      if (!IsImeMessage(message))
        return ::DefWindowProc(hWnd, message, wParam, lParam);
      break;
  }
  return 0;
} // CandWnd_WindowProc

BOOL GetCandPosFromCompWnd(InputContext *lpIMC, LPUIEXTRA lpUIExtra, LPPOINT lppt) {
  FOOTMARK();

  BOOL ret = FALSE;

  DWORD iClause = 0;
  CandInfo *lpCandInfo = lpIMC->LockCandInfo();
  if (lpCandInfo) {
    if (lpCandInfo->dwCount > 0) ret = TRUE;
    CANDINFOEXTRA *extra = lpCandInfo->GetExtra();
    if (extra) iClause = extra->iClause;
    lpIMC->UnlockCandInfo();

    if (ret) {
      ret = FALSE;
      HWND hCompWnd = ClauseToCompWnd(lpUIExtra, lpIMC, iClause);
      if (GetCandPosHintFromComp(lpUIExtra, lpIMC, iClause, lppt)) {
        ret = ::IsWindowVisible(hCompWnd);
      }
    }
  }

  return ret;
}

BOOL GetCandPosFromCompForm(InputContext *lpIMC, LPUIEXTRA lpUIExtra,
                            LPPOINT lppt) {
  FOOTMARK();
  if (GetCandPosFromCompWnd(lpIMC, lpUIExtra, lppt)) {
    ::ScreenToClient(lpIMC->hWnd, lppt);
    return TRUE;
  }
  return FALSE;
} // GetCandPosFromCompForm

void CandWnd_Create(HWND hUIWnd, LPUIEXTRA lpUIExtra, InputContext *lpIMC) {
  FOOTMARK();
  POINT pt;

  if (GetCandPosFromCompWnd(lpIMC, lpUIExtra, &pt)) {
    lpUIExtra->uiCand.pt.x = pt.x;
    lpUIExtra->uiCand.pt.y = pt.y;
  }

  if (!::IsWindow(lpUIExtra->uiCand.hWnd)) {
    lpUIExtra->uiCand.hWnd =
        ::CreateWindowEx(WS_EX_WINDOWEDGE, szCandClassName, NULL,
                         WS_COMPDEFAULT | WS_DLGFRAME,
                         lpUIExtra->uiCand.pt.x, lpUIExtra->uiCand.pt.y,
                         1, 1, hUIWnd, NULL, TheIME.m_hInst, NULL);
  }

  ::SetWindowLongPtr(lpUIExtra->uiCand.hWnd, FIGWLP_SERVERWND, (LONG_PTR)hUIWnd);
  ::ShowWindow(lpUIExtra->uiCand.hWnd, SW_HIDE);
  lpUIExtra->uiCand.bShow = FALSE;
} // CandWnd_Create

void CandWnd_Paint(HWND hCandWnd) {
  FOOTMARK();
  RECT rc;
  ::GetClientRect(hCandWnd, &rc);

  PAINTSTRUCT ps;
  HDC hDC = ::BeginPaint(hCandWnd, &ps);
  ::SetBkMode(hDC, TRANSPARENT);
  HWND hSvrWnd = (HWND)GetWindowLongPtr(hCandWnd, FIGWLP_SERVERWND);

  HBRUSH hbrHightLight = CreateSolidBrush(GetSysColor(COLOR_HIGHLIGHT));
  HIMC hIMC = (HIMC)GetWindowLongPtr(hSvrWnd, IMMGWLP_IMC);
  if (hIMC) {
    InputContext *lpIMC = TheIME.LockIMC(hIMC);
    HFONT hOldFont = CheckNativeCharset(hDC);
    CandInfo *lpCandInfo = lpIMC->LockCandInfo();
    if (lpCandInfo) {
      int height = ::GetSystemMetrics(SM_CYEDGE);
      CANDINFOEXTRA *extra = lpCandInfo->GetExtra();
      DWORD iList = 0;
      if (extra) iList = extra->iClause;
      CandList *lpCandList = lpCandInfo->GetList(iList);
      DWORD i, end = lpCandList->GetPageEnd();
      for (i = lpCandList->dwPageStart; i < end; i++) {
        SIZE sz;
        HBRUSH hbr;
        WCHAR *psz = lpCandList->GetCandString(i);
        ::GetTextExtentPoint32W(hDC, psz, lstrlenW(psz), &sz);
        if (lpCandList->dwSelection == i) {
          hbr = (HBRUSH)::SelectObject(hDC, hbrHightLight);
          ::PatBlt(hDC, 0, height, rc.right, sz.cy, PATCOPY);
          ::SelectObject(hDC, hbr);
          ::SetTextColor(hDC, ::GetSysColor(COLOR_HIGHLIGHTTEXT));
        } else {
          HBRUSH hbrLGR = (HBRUSH)::GetStockObject(LTGRAY_BRUSH);
          hbr = (HBRUSH)::SelectObject(hDC, hbrLGR);
          ::PatBlt(hDC, 0, height, rc.right, sz.cy, PATCOPY);
          ::SelectObject(hDC, hbr);
          ::SetTextColor(hDC, RGB(0, 0, 0));
        }
        ::TextOutW(hDC, ::GetSystemMetrics(SM_CXEDGE), height, psz,
                   ::lstrlenW(psz));
        height += sz.cy;
      }
      lpIMC->UnlockCandInfo();
    }
    if (hOldFont) {
      ::DeleteObject(SelectObject(hDC, hOldFont));
    }
    TheIME.UnlockIMC(hIMC);
  }
  ::EndPaint(hCandWnd, &ps);

  ::DeleteObject(hbrHightLight);
} // CandWnd_Paint

SIZE CandWnd_CalcSize(LPUIEXTRA lpUIExtra, InputContext *lpIMC) {
  FOOTMARK();
  int width = 0, height = 0;
  HDC hDC = ::CreateCompatibleDC(NULL);
  HFONT hOldFont = CheckNativeCharset(hDC);
  CandInfo *lpCandInfo = lpIMC->LockCandInfo();
  if (lpCandInfo) {
    if (lpCandInfo->dwCount > 0) {
      CANDINFOEXTRA *extra = lpCandInfo->GetExtra();
      DWORD iList = 0;
      if (extra) iList = extra->iClause;
      CandList *lpCandList = lpCandInfo->GetList(iList);
      DWORD i, end = lpCandList->GetPageEnd();
      for (i = lpCandList->dwPageStart; i < end; i++) {
        WCHAR *psz = lpCandList->GetCandString(i);
        SIZE siz;
        ::GetTextExtentPoint32W(hDC, psz, ::lstrlenW(psz), &siz);
        if (width < siz.cx) width = siz.cx;
        height += siz.cy;
      }
    } else {
      FOOTMARK_POINT();
      lpCandInfo->Dump();
      assert(0);
    }
    lpIMC->UnlockCandInfo();
  }
  if (hOldFont) {
    ::DeleteObject(::SelectObject(hDC, hOldFont));
  }
  ::DeleteDC(hDC);
  SIZE ret;
  ret.cx = width;
  ret.cy = height;
  return ret;
} // CandWnd_CalcSize

void CandWnd_Resize(LPUIEXTRA lpUIExtra, InputContext *lpIMC) {
  FOOTMARK();
  if (::IsWindow(lpUIExtra->uiCand.hWnd)) {
    SIZE siz = CandWnd_CalcSize(lpUIExtra, lpIMC);
    siz.cx += 4 * GetSystemMetrics(SM_CXEDGE);
    siz.cy += 4 * GetSystemMetrics(SM_CYEDGE);

    RECT rc;
    ::GetWindowRect(lpUIExtra->uiCand.hWnd, &rc);
    ::MoveWindow(lpUIExtra->uiCand.hWnd, rc.left, rc.top,
                 siz.cx, siz.cy, TRUE);
  }
} // CandWnd_Resize

void CandWnd_Hide(LPUIEXTRA lpUIExtra) {
  FOOTMARK();
  RECT rc;

  if (::IsWindow(lpUIExtra->uiCand.hWnd)) {
    ::GetWindowRect(lpUIExtra->uiCand.hWnd, (LPRECT)&rc);
    lpUIExtra->uiCand.pt.x = rc.left;
    lpUIExtra->uiCand.pt.y = rc.top;
    ::MoveWindow(lpUIExtra->uiCand.hWnd, -1, -1, 0, 0, TRUE);
    ::ShowWindow(lpUIExtra->uiCand.hWnd, SW_HIDE);
    lpUIExtra->uiCand.bShow = FALSE;
  }
} // CandWnd_Hide

void CandWnd_Move(HWND hUIWnd, InputContext *lpIMC, LPUIEXTRA lpUIExtra,
                  BOOL fForceComp) {
  FOOTMARK();
  RECT rc;
  POINT pt;
  CANDIDATEFORM caf;

  // not initialized yet?
  if (lpIMC->cfCandForm[0].dwIndex == (DWORD)-1) {
    FOOTMARK_POINT();
    lpIMC->DumpCandInfo();
    if (GetCandPosFromCompWnd(lpIMC, lpUIExtra, &pt)) {
      lpUIExtra->uiCand.pt.x = pt.x;
      lpUIExtra->uiCand.pt.y = pt.y;
      HWND hwndCand = lpUIExtra->uiCand.hWnd;
      ::GetWindowRect(hwndCand, &rc);
      int cx, cy;
      cx = rc.right - rc.left;
      cy = rc.bottom - rc.top;
      ::MoveWindow(hwndCand, pt.x, pt.y, cx, cy, TRUE);
      ::ShowWindow(hwndCand, SW_SHOWNOACTIVATE);
      lpUIExtra->uiCand.bShow = TRUE;
      ::InvalidateRect(hwndCand, NULL, FALSE);
      ::SendMessage(hUIWnd, WM_UI_CANDMOVE, 0, MAKELONG(pt.x, pt.y));
    }
    return;
  }

  // has candidates?
  if (lpIMC->HasCandInfo()) {
    DWORD dwStyle = lpIMC->cfCandForm[0].dwStyle;
    if (dwStyle == CFS_EXCLUDE) {
      FOOTMARK_POINT();
      lpIMC->DumpCandInfo();
      // get work area and app window rect
      RECT rcWork, rcAppWnd;
      ::SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWork, FALSE);
      ::GetWindowRect(lpIMC->hWnd, &rcAppWnd);

      // get the specified position in screen coordinates
      ::GetClientRect(lpUIExtra->uiCand.hWnd, &rc);
      if (!lpUIExtra->bVertical) {
        pt.x = lpIMC->cfCandForm[0].ptCurrentPos.x;
        pt.y = lpIMC->cfCandForm[0].rcArea.bottom;
        ::ClientToScreen(lpIMC->hWnd, &pt);
        if (pt.y + rc.bottom > rcWork.bottom) {
          pt.y = rcAppWnd.top + lpIMC->cfCandForm[0].rcArea.top - rc.bottom;
        }
      } else {
        pt.x = lpIMC->cfCandForm[0].rcArea.left - rc.right;
        pt.y = lpIMC->cfCandForm[0].ptCurrentPos.y;
        ::ClientToScreen(lpIMC->hWnd, &pt);
        if (pt.x < 0) {
          pt.x = rcAppWnd.left + lpIMC->cfCandForm[0].rcArea.right;
        }
      }

      // move and show candidate window
      HWND hwndCand = lpUIExtra->uiCand.hWnd;
      if (::IsWindow(hwndCand)) {
        ::GetWindowRect(hwndCand, &rc);
        int cx, cy;
        cx = rc.right - rc.left;
        cy = rc.bottom - rc.top;
        ::MoveWindow(hwndCand, pt.x, pt.y, cx, cy, TRUE);
        ::ShowWindow(hwndCand, SW_SHOWNOACTIVATE);
        lpUIExtra->uiCand.bShow = TRUE;
        ::InvalidateRect(hwndCand, NULL, FALSE);
      }
      ::SendMessage(hUIWnd, WM_UI_CANDMOVE, 0, MAKELPARAM(pt.x, pt.y));
    } else if (dwStyle == CFS_CANDIDATEPOS) {
      FOOTMARK_POINT();
      lpIMC->DumpCandInfo();
      // get the specified position in screen coordinates
      pt.x = lpIMC->cfCandForm[0].ptCurrentPos.x;
      pt.y = lpIMC->cfCandForm[0].ptCurrentPos.y;
      ::ClientToScreen(lpIMC->hWnd, &pt);

      // move and show candidate window
      HWND hwndCand = lpUIExtra->uiCand.hWnd;
      if (::IsWindow(hwndCand)) {
        ::GetWindowRect(hwndCand, &rc);
        int cx, cy;
        cx = rc.right - rc.left;
        cy = rc.bottom - rc.top;
        ::MoveWindow(hwndCand, pt.x, pt.y, cx, cy, TRUE);
        ::ShowWindow(hwndCand, SW_SHOWNOACTIVATE);
        lpUIExtra->uiCand.bShow = TRUE;
        ::InvalidateRect(hwndCand, NULL, FALSE);
      }
      ::SendMessage(hUIWnd, WM_UI_CANDMOVE, 0, MAKELPARAM(pt.x, pt.y));
    }
  }
} // CandWnd_Move

//////////////////////////////////////////////////////////////////////////////

}  // extern "C"

//////////////////////////////////////////////////////////////////////////////
