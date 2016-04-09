// uicand.cpp
//////////////////////////////////////////////////////////////////////////////

#include "mzimeja.h"

extern "C" {

//////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK CandWndProc(HWND hWnd, UINT message, WPARAM wParam,
                             LPARAM lParam) {
  HWND hUIWnd;

  switch (message) {
    case WM_PAINT:
      PaintCandWindow(hWnd);
      break;

    case WM_SETCURSOR:
    case WM_MOUSEMOVE:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
      DragUI(hWnd, message, wParam, lParam);
      if ((message == WM_SETCURSOR) && (HIWORD(lParam) != WM_LBUTTONDOWN) &&
          (HIWORD(lParam) != WM_RBUTTONDOWN))
        return DefWindowProc(hWnd, message, wParam, lParam);
      if ((message == WM_LBUTTONUP) || (message == WM_RBUTTONUP))
        SetWindowLong(hWnd, FIGWL_MOUSE, 0L);
      break;

    case WM_MOVE:
      hUIWnd = (HWND)GetWindowLongPtr(hWnd, FIGWL_SVRWND);
      if (IsWindow(hUIWnd)) SendMessage(hUIWnd, WM_UI_CANDMOVE, wParam, lParam);
      break;

    default:
      if (!MyIsIMEMessage(message))
        return DefWindowProc(hWnd, message, wParam, lParam);
      break;
  }
  return 0L;
}

BOOL PASCAL GetCandPosFromCompWnd(LPUIEXTRA lpUIExtra, LPPOINT lppt) {
  RECT rc;

  if (lpUIExtra->dwCompStyle) {
    if (lpUIExtra->uiComp[0].bShow) {
      GetWindowRect(lpUIExtra->uiComp[0].hWnd, &rc);
      lppt->x = rc.left;
      lppt->y = rc.bottom + 1;
      return TRUE;
    }
  } else {
    if (lpUIExtra->uiDefComp.bShow) {
      GetWindowRect(lpUIExtra->uiDefComp.hWnd, &rc);
      lppt->x = rc.left;
      lppt->y = rc.bottom + 1;
      return TRUE;
    }
  }
  return FALSE;
}

BOOL PASCAL GetCandPosFromCompForm(InputContext *lpIMC, LPUIEXTRA lpUIExtra,
                                   LPPOINT lppt) {
  if (lpUIExtra->dwCompStyle) {
    if (lpIMC && lpIMC->HasCompForm()) {
      int height = GetCompFontHeight(lpUIExtra);;
      if (!lpUIExtra->bVertical) {
        lppt->x = lpIMC->cfCompForm.ptCurrentPos.x;
        lppt->y = lpIMC->cfCompForm.ptCurrentPos.y + height;
      } else {
        lppt->x = lpIMC->cfCompForm.ptCurrentPos.x - height;
        lppt->y = lpIMC->cfCompForm.ptCurrentPos.y;
      }
      return TRUE;
    }
  } else {
    if (GetCandPosFromCompWnd(lpUIExtra, lppt)) {
      ScreenToClient(lpIMC->hWnd, lppt);
      return TRUE;
    }
  }
  return FALSE;
}

void PASCAL CreateCandWindow(HWND hUIWnd, LPUIEXTRA lpUIExtra,
                             InputContext *lpIMC) {
  POINT pt;

  if (GetCandPosFromCompWnd(lpUIExtra, &pt)) {
    lpUIExtra->uiCand.pt.x = pt.x;
    lpUIExtra->uiCand.pt.y = pt.y;
  }

  if (!IsWindow(lpUIExtra->uiCand.hWnd)) {
    lpUIExtra->uiCand.hWnd =
        CreateWindowEx(WS_EX_WINDOWEDGE, szCandClassName, NULL,
                       WS_COMPDEFAULT | WS_DLGFRAME, lpUIExtra->uiCand.pt.x,
                       lpUIExtra->uiCand.pt.y, 1, 1, hUIWnd, NULL, hInst, NULL);
  }

  SetWindowLongPtr(lpUIExtra->uiCand.hWnd, FIGWL_SVRWND, (LONG_PTR)hUIWnd);
  ShowWindow(lpUIExtra->uiCand.hWnd, SW_HIDE);
  lpUIExtra->uiCand.bShow = FALSE;

  return;
}

void PASCAL PaintCandWindow(HWND hCandWnd) {
  RECT rc;
  GetClientRect(hCandWnd, &rc);

  PAINTSTRUCT ps;
  HDC hDC = BeginPaint(hCandWnd, &ps);
  SetBkMode(hDC, TRANSPARENT);
  HWND hSvrWnd = (HWND)GetWindowLongPtr(hCandWnd, FIGWL_SVRWND);

  HBRUSH hbrHightLight = CreateSolidBrush(GetSysColor(COLOR_HIGHLIGHT));
  HIMC hIMC = (HIMC)GetWindowLongPtr(hSvrWnd, IMMGWLP_IMC);
  if (hIMC) {
    InputContext *lpIMC = (InputContext *)ImmLockIMC(hIMC);
    HFONT hOldFont = CheckNativeCharset(hDC);
    CandInfo *lpCandInfo = lpIMC->LockCandInfo();
    if (lpCandInfo) {
      int height = GetSystemMetrics(SM_CYEDGE);
      CandList *lpCandList = lpCandInfo->GetList();
      for (DWORD i = lpCandList->dwPageStart;
           i < lpCandList->GetPageEnd(); i++) {
        SIZE sz;
        HBRUSH hbr;
        LPTSTR lpstr = lpCandList->GetCandString(i);
        GetTextExtentPoint(hDC, lpstr, lstrlen(lpstr), &sz);
        if (lpCandInfo->cl.dwSelection == i) {
          hbr = (HBRUSH)SelectObject(hDC, hbrHightLight);
          PatBlt(hDC, 0, height, rc.right, sz.cy, PATCOPY);
          SelectObject(hDC, hbr);
          SetTextColor(hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
        } else {
          HBRUSH hbrLGR = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
          hbr = (HBRUSH)SelectObject(hDC, hbrLGR);
          PatBlt(hDC, 0, height, rc.right, sz.cy, PATCOPY);
          SelectObject(hDC, hbr);
          SetTextColor(hDC, RGB(0, 0, 0));
        }
        TextOut(hDC, GetSystemMetrics(SM_CXEDGE), height, lpstr,
                lstrlen(lpstr));
        height += sz.cy;
      }
      lpIMC->UnlockCandInfo();
    }
    if (hOldFont) {
      DeleteObject(SelectObject(hDC, hOldFont));
    }
    ImmUnlockIMC(hIMC);
  }
  EndPaint(hCandWnd, &ps);

  DeleteObject(hbrHightLight);
}

void PASCAL ResizeCandWindow(LPUIEXTRA lpUIExtra, InputContext *lpIMC) {
  if (IsWindow(lpUIExtra->uiCand.hWnd)) {
    int width = 0, height = 0;
    HDC hDC = GetDC(lpUIExtra->uiCand.hWnd);
    HFONT hOldFont = CheckNativeCharset(hDC);
    CandInfo *lpCandInfo = lpIMC->LockCandInfo();
    if (lpCandInfo) {
      CandList *lpCandList = lpCandInfo->GetList();
      for (DWORD i = lpCandList->dwPageStart;
           i < lpCandList->GetPageEnd(); i++) {
        LPTSTR lpstr = lpCandList->GetCandString(i);
        SIZE sz;
        GetTextExtentPoint(hDC, lpstr, lstrlen(lpstr), &sz);
        if (width < sz.cx) width = sz.cx;
        height += sz.cy;
      }
      lpIMC->UnlockCandInfo();
    }
    if (hOldFont) {
      DeleteObject(SelectObject(hDC, hOldFont));
    }
    ReleaseDC(lpUIExtra->uiCand.hWnd, hDC);

    RECT rc;
    GetWindowRect(lpUIExtra->uiCand.hWnd, &rc);
    MoveWindow(lpUIExtra->uiCand.hWnd, rc.left, rc.top,
               width + 4 * GetSystemMetrics(SM_CXEDGE),
               height + 4 * GetSystemMetrics(SM_CYEDGE), TRUE);
  }
}

void PASCAL HideCandWindow(LPUIEXTRA lpUIExtra) {
  RECT rc;

  if (IsWindow(lpUIExtra->uiCand.hWnd)) {
    GetWindowRect(lpUIExtra->uiCand.hWnd, (LPRECT)&rc);
    lpUIExtra->uiCand.pt.x = rc.left;
    lpUIExtra->uiCand.pt.y = rc.top;
    MoveWindow(lpUIExtra->uiCand.hWnd, -1, -1, 0, 0, TRUE);
    ShowWindow(lpUIExtra->uiCand.hWnd, SW_HIDE);
    lpUIExtra->uiCand.bShow = FALSE;
  }
}

void PASCAL MoveCandWindow(HWND hUIWnd, InputContext *lpIMC,
                           LPUIEXTRA lpUIExtra, BOOL fForceComp) {
  RECT rc;
  POINT pt;
  CANDIDATEFORM caf;

  if (fForceComp) {
    if (GetCandPosFromCompForm(lpIMC, lpUIExtra, &pt)) {
      caf.dwIndex = 0;
      caf.dwStyle = CFS_CANDIDATEPOS;
      caf.ptCurrentPos.x = pt.x;
      caf.ptCurrentPos.y = pt.y;
      ImmSetCandidateWindow(lpUIExtra->hIMC, &caf);
    }
    return;
  }

  // Not initialized !!
  if (lpIMC->cfCandForm[0].dwIndex == (DWORD)-1) {
    if (GetCandPosFromCompWnd(lpUIExtra, &pt)) {
      lpUIExtra->uiCand.pt.x = pt.x;
      lpUIExtra->uiCand.pt.y = pt.y;
      GetWindowRect(lpUIExtra->uiCand.hWnd, &rc);
      MoveWindow(lpUIExtra->uiCand.hWnd, pt.x, pt.y, rc.right - rc.left,
                 rc.bottom - rc.top, TRUE);
      ShowWindow(lpUIExtra->uiCand.hWnd, SW_SHOWNOACTIVATE);
      lpUIExtra->uiCand.bShow = TRUE;
      InvalidateRect(lpUIExtra->uiCand.hWnd, NULL, FALSE);
      SendMessage(hUIWnd, WM_UI_CANDMOVE, 0, MAKELONG(pt.x, pt.y));
    }
    return;
  }

  if (!lpIMC->IsCandidate()) return;

  if (lpIMC->cfCandForm[0].dwStyle == CFS_EXCLUDE) {
    RECT rcWork;
    RECT rcAppWnd;

    SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWork, FALSE);
    GetClientRect(lpUIExtra->uiCand.hWnd, &rc);
    GetWindowRect(lpIMC->hWnd, &rcAppWnd);

    if (!lpUIExtra->bVertical) {
      pt.x = lpIMC->cfCandForm[0].ptCurrentPos.x;
      pt.y = lpIMC->cfCandForm[0].rcArea.bottom;
      ClientToScreen(lpIMC->hWnd, &pt);

      if (pt.y + rc.bottom > rcWork.bottom)
        pt.y = rcAppWnd.top + lpIMC->cfCandForm[0].rcArea.top - rc.bottom;
    } else {
      pt.x = lpIMC->cfCandForm[0].rcArea.left - rc.right;
      pt.y = lpIMC->cfCandForm[0].ptCurrentPos.y;
      ClientToScreen(lpIMC->hWnd, &pt);

      if (pt.x < 0) pt.x = rcAppWnd.left + lpIMC->cfCandForm[0].rcArea.right;
    }

    if (IsWindow(lpUIExtra->uiCand.hWnd)) {
      GetWindowRect(lpUIExtra->uiCand.hWnd, &rc);
      MoveWindow(lpUIExtra->uiCand.hWnd, pt.x, pt.y, rc.right - rc.left,
                 rc.bottom - rc.top, TRUE);
      ShowWindow(lpUIExtra->uiCand.hWnd, SW_SHOWNOACTIVATE);
      lpUIExtra->uiCand.bShow = TRUE;
      InvalidateRect(lpUIExtra->uiCand.hWnd, NULL, FALSE);
    }
    SendMessage(hUIWnd, WM_UI_CANDMOVE, 0, MAKELONG(pt.x, pt.y));
  } else if (lpIMC->cfCandForm[0].dwStyle == CFS_CANDIDATEPOS) {
    pt.x = lpIMC->cfCandForm[0].ptCurrentPos.x;
    pt.y = lpIMC->cfCandForm[0].ptCurrentPos.y;
    ClientToScreen(lpIMC->hWnd, &pt);

    if (IsWindow(lpUIExtra->uiCand.hWnd)) {
      GetWindowRect(lpUIExtra->uiCand.hWnd, &rc);
      MoveWindow(lpUIExtra->uiCand.hWnd, pt.x, pt.y, rc.right - rc.left,
                 rc.bottom - rc.top, TRUE);
      ShowWindow(lpUIExtra->uiCand.hWnd, SW_SHOWNOACTIVATE);
      lpUIExtra->uiCand.bShow = TRUE;
      InvalidateRect(lpUIExtra->uiCand.hWnd, NULL, FALSE);
    }
    SendMessage(hUIWnd, WM_UI_CANDMOVE, 0, MAKELONG(pt.x, pt.y));
  }
}

//////////////////////////////////////////////////////////////////////////////

}  // extern "C"

//////////////////////////////////////////////////////////////////////////////