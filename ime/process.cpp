// process.cpp --- mzimeja input process
// 入力処理。
//////////////////////////////////////////////////////////////////////////////

#include "mzimeja.h"
#include "vksub.h"

extern "C" {

// 仮想キー判定関数群
inline BOOL isAlphaKey(UINT vKey) {
    return ('A' <= vKey && vKey <= 'Z');
}
inline BOOL isNumericKey(UINT vKey) {
    return ('0' <= vKey && vKey <= '9');
}
inline BOOL isAlphaNumericKey(UINT vKey) {
    return isAlphaKey(vKey) || isNumericKey(vKey);
}
inline BOOL isNumPadKey(UINT vKey) {
    return VK_NUMPAD0 <= vKey && vKey <= VK_DIVIDE;
}
inline BOOL isDbeKey(UINT vKey) {
    return vKey == VK_KANJI || vKey == VK_CONVERT ||
           (VK_DBE_ALPHANUMERIC <= vKey && vKey <= VK_DBE_ENTERDLGCONVERSIONMODE);
}

//////////////////////////////////////////////////////////////////////////////

BOOL
DoProcessKey(
    HIMC hIMC,
    InputContext *lpIMC,
    WPARAM wParam,
    CONST LPBYTE lpbKeyState,
    BOOL bKeyUp,
    BOOL bDoAction)
{
    FOOTMARK_FORMAT("(%p, %p, %p)\n", lpIMC, wParam, lpbKeyState);
    BYTE vk = (BYTE)wParam; // 仮想キー

    if (vk == VK_SHIFT || vk == VK_CONTROL)
        FOOTMARK_RETURN_INT(FALSE); // 処理しない

    BOOL bOpen = lpIMC->IsOpen();
    if (!bDoAction && bOpen && (isAlphaNumericKey(vk) || isNumPadKey(vk)))
        FOOTMARK_RETURN_INT(TRUE); // 処理すべき

    BOOL bCompStr = lpIMC->HasCompStr();
    BOOL bAlt = !!(lpbKeyState[VK_MENU] & 0x80);
    BOOL bShift = !!(lpbKeyState[VK_SHIFT] & 0x80);
    BOOL bCtrl = !!(lpbKeyState[VK_CONTROL] & 0x80);
    BOOL bCapsLock = !!(lpbKeyState[VK_CAPITAL] & 0x80);
    BOOL bRoman = IsRomanMode(hIMC);

    if (bKeyUp) {
        FOOTMARK_RETURN_INT(FALSE);
    }

    switch (vk) {
    case VK_KANJI: case VK_DBE_DBCSCHAR: case VK_DBE_SBCSCHAR: // [半角／全角]キー
        if (bDoAction) {
            ImmSetOpenStatus(hIMC, !bOpen);
        }
        break;
    case VK_SPACE: // スペース キー
        if (bDoAction) {
            if (bCtrl) {
                if (bOpen && bCompStr)
                    lpIMC->AddChar(' ', UNICODE_NULL); // Ctrl+Spaceは、半角スペース
                else
                    FOOTMARK_RETURN_INT(FALSE);
            } else {
                if (bCompStr) { // 変換
                    lpIMC->Convert(bShift);
                } else { // 全角スペース (U+3000: '　')
                    TheIME.GenerateMessage(WM_IME_CHAR, 0x3000, 1);
                }
            }
        }
        break;
    case VK_DBE_ALPHANUMERIC: // 英数キー
        if (!bShift && !bCtrl) {
            if (bDoAction) {
                INPUT_MODE imode = GetInputMode(hIMC);
                ImmSetOpenStatus(hIMC, TRUE);
                if (bOpen && imode == IMODE_FULL_HIRAGANA)
                    SetInputMode(hIMC, IMODE_FULL_ASCII);
                else
                    SetInputMode(hIMC, IMODE_FULL_HIRAGANA);
            }
        }
        break;
    case VK_OEM_3: // '~'
        // 英語キーボードなどの Alt+~ を処理する
        if (bAlt && !bShift && !bCtrl) {
            if (bDoAction) ImmSetOpenStatus(hIMC, !bOpen);
        } else {
            FOOTMARK_RETURN_INT(FALSE);
        }
        break;
    case VK_DBE_ROMAN: // ローマ字
        if (bDoAction) {
            if (bOpen) {
                SetRomanMode(hIMC, !bRoman);
            }
        }
        break;
    case VK_DBE_HIRAGANA: case VK_DBE_KATAKANA: // [カナ/かな]キー
        if (bDoAction) {
            // 入力モードの状態に応じて入力モードを切り替える
            INPUT_MODE imode = GetInputMode(hIMC);
            switch (imode) {
            case IMODE_FULL_HIRAGANA:
                if (bShift) SetInputMode(hIMC, IMODE_FULL_KATAKANA);
                break;
            case IMODE_FULL_KATAKANA:
                if (!bShift) SetInputMode(hIMC, IMODE_FULL_HIRAGANA);
                break;
            case IMODE_FULL_ASCII: case IMODE_HALF_ASCII:
                if (bShift)
                    SetInputMode(hIMC, IMODE_FULL_KATAKANA);
                else
                    SetInputMode(hIMC, IMODE_FULL_HIRAGANA);
                break;
            case IMODE_HALF_KANA:
                if (!bShift)
                    SetInputMode(hIMC, IMODE_FULL_HIRAGANA);
                break;
            default:
                break;
            }
        }
        break;
    case VK_ESCAPE: // Escキー
        if (bDoAction) {
            if (bCompStr) {
                lpIMC->Escape();
            } else {
                TheIME.GenerateMessage(WM_IME_KEYDOWN, VK_ESCAPE, 1);
                TheIME.GenerateMessage(WM_IME_KEYUP, VK_ESCAPE, 0xC0000001);
            }
        }
        break;
    case VK_DELETE: case VK_BACK: // DelキーまたはBkSpキー
        if (bDoAction) {
            if (bCompStr) {
                lpIMC->DeleteChar(vk == VK_BACK);
            } else {
                if (vk == VK_BACK) {
                    TheIME.GenerateMessage(WM_IME_KEYDOWN, VK_BACK);
                    TheIME.GenerateMessage(WM_IME_KEYUP, VK_BACK, 0x80000000);
                } else {
                    TheIME.GenerateMessage(WM_IME_KEYDOWN, VK_DELETE);
                    TheIME.GenerateMessage(WM_IME_KEYUP, VK_DELETE, 0x80000000);
                }
            }
        }
        break;
    case VK_CONVERT: // [変換]キー
        if (bDoAction) {
            lpIMC->Convert(bShift);
        }
        break;
    case VK_NONCONVERT: // [無変換]キー
        if (bDoAction) {
            if (bCompStr) {
                lpIMC->MakeHiragana();
            } else {
                TheIME.GenerateMessage(WM_IME_KEYDOWN, VK_NONCONVERT, 1);
                TheIME.GenerateMessage(WM_IME_KEYUP, VK_NONCONVERT, 0xC0000001);
            }
        }
        break;
    case VK_F5: // F5はコード変換
        if (bDoAction) {
            lpIMC->ConvertCode();
        }
        break;
    case VK_F6: // F6はひらがな変換
        if (bDoAction) {
            if (bCompStr) {
                lpIMC->MakeHiragana();
            } else {
                TheIME.GenerateMessage(WM_IME_KEYDOWN, VK_F6, 1);
                TheIME.GenerateMessage(WM_IME_KEYUP, VK_F6, 0xC0000001);
            }
        }
        break;
    case VK_F7: // F7はカタカナ変換
        if (bDoAction) {
            if (bCompStr) {
                lpIMC->MakeKatakana();
            } else {
                TheIME.GenerateMessage(WM_IME_KEYDOWN, VK_F7, 1);
                TheIME.GenerateMessage(WM_IME_KEYUP, VK_F7, 0xC0000001);
            }
        }
        break;
    case VK_F8: // F8は半角変換
        if (bDoAction) {
            if (bCompStr) {
                lpIMC->MakeHankaku();
            } else {
                TheIME.GenerateMessage(WM_IME_KEYDOWN, VK_F8, 1);
                TheIME.GenerateMessage(WM_IME_KEYUP, VK_F8, 0xC0000001);
            }
        }
        break;
    case VK_F9: // F9は全角英数変換
        if (bDoAction) {
            if (bCompStr) {
                lpIMC->MakeZenEisuu();
            } else {
                TheIME.GenerateMessage(WM_IME_KEYDOWN, VK_F9, 1);
                TheIME.GenerateMessage(WM_IME_KEYUP, VK_F9, 0xC0000001);
            }
        }
        break;
    case VK_F10: // F10は半角英数変換
        if (bDoAction) {
            if (bCompStr) {
                lpIMC->MakeHanEisuu();
            } else {
                TheIME.GenerateMessage(WM_IME_KEYDOWN, VK_F10, 1);
                TheIME.GenerateMessage(WM_IME_KEYUP, VK_F10, 0xC0000001);
            }
        }
        break;
    case VK_RETURN: // Enterキー
        if (bDoAction) {
            if (lpIMC->Conversion() & IME_CMODE_CHARCODE) {
                // code input
                lpIMC->CancelText();
            } else {
                if (bCompStr) {
                    lpIMC->MakeResult();
                } else {
                    // add new line
                    TheIME.GenerateMessage(WM_IME_KEYDOWN, VK_RETURN, 1);
                    TheIME.GenerateMessage(WM_IME_KEYUP, VK_RETURN, 0xC0000001);
                }
            }
        }
        break;
    case VK_LEFT: // [←]
        if (bDoAction) {
            if (bCompStr) {
                lpIMC->MoveLeft(bShift);
            } else {
                TheIME.GenerateMessage(WM_IME_KEYDOWN, VK_LEFT, 1);
                TheIME.GenerateMessage(WM_IME_KEYUP, VK_LEFT, 0xC0000001);
            }
        }
        break;
    case VK_RIGHT: // [→]
        if (bDoAction) {
            if (bCompStr) {
                lpIMC->MoveRight(bShift);
            } else {
                TheIME.GenerateMessage(WM_IME_KEYDOWN, VK_RIGHT, 1);
                TheIME.GenerateMessage(WM_IME_KEYUP, VK_RIGHT, 0xC0000001);
            }
        }
        break;
    case VK_UP: // [↑]
        if (bDoAction) {
            if (bCompStr) {
                lpIMC->MoveUp();
            } else {
                TheIME.GenerateMessage(WM_IME_KEYDOWN, VK_UP, 1);
                TheIME.GenerateMessage(WM_IME_KEYUP, VK_UP, 0xC0000001);
            }
        }
        break;
    case VK_DOWN: // [↓]
        if (bDoAction) {
            if (bCompStr) {
                lpIMC->MoveDown();
            } else {
                TheIME.GenerateMessage(WM_IME_KEYDOWN, VK_DOWN, 1);
                TheIME.GenerateMessage(WM_IME_KEYUP, VK_DOWN, 0xC0000001);
            }
        }
        break;
    case VK_PRIOR: // Page Up
        if (bDoAction) {
            if (bCompStr) {
                lpIMC->PageUp();
            } else {
                TheIME.GenerateMessage(WM_IME_KEYDOWN, VK_PRIOR, 1);
                TheIME.GenerateMessage(WM_IME_KEYUP, VK_PRIOR, 0xC0000001);
            }
        }
        break;
    case VK_NEXT: // Page Down
        if (bDoAction) {
            if (bCompStr) {
                lpIMC->PageDown();
            } else {
                TheIME.GenerateMessage(WM_IME_KEYDOWN, VK_NEXT, 1);
                TheIME.GenerateMessage(WM_IME_KEYUP, VK_NEXT, 0xC0000001);
            }
        }
        break;
    case VK_HOME: // [Home]キー
        if (bDoAction) {
            if (bCompStr) {
                lpIMC->MoveHome();
            } else {
                TheIME.GenerateMessage(WM_IME_KEYDOWN, VK_HOME, 1);
                TheIME.GenerateMessage(WM_IME_KEYUP, VK_HOME, 0xC0000001);
            }
        }
        break;
    case VK_END:
        if (bDoAction) {
            if (bCompStr) {
                lpIMC->MoveEnd();
            } else {
                TheIME.GenerateMessage(WM_IME_KEYDOWN, VK_END, 1);
                TheIME.GenerateMessage(WM_IME_KEYUP, VK_END, 0xC0000001);
            }
        }
        break;
    default:
        {
            if (!bOpen || bCtrl || bAlt)
                FOOTMARK_RETURN_INT(FALSE); // 処理しない

            // 可能ならキーをひらがなにする。
            WCHAR chTranslated = 0;
            if (!bRoman) {
                chTranslated = mz_vkey_to_hiragana(vk, bShift);
            }

            // 可能ならキーを文字にする。
            WCHAR chTyped;
            if (vk == VK_PACKET) {
                chTyped = chTranslated = HIWORD(wParam);
            } else {
                chTyped = mz_typing_key_to_char(vk, bShift, bCapsLock);
            }

            if (chTranslated || chTyped) { // キーを変換できたら
                if (bDoAction) {
                    if (lpIMC->HasCandInfo() && L'1' <= chTyped && chTyped <= L'9') {
                        // 候補情報があり、候補の選択であれば、候補を選択。
                        lpIMC->SelectCand(chTyped - L'1');
                    } else {
                        // さもなければ文字を追加。
                        lpIMC->AddChar(chTyped, chTranslated);
                    }
                }
                FOOTMARK_RETURN_INT(TRUE); // 処理すべき／処理した
            }
        }
        FOOTMARK_RETURN_INT(FALSE); // 処理しない
    }

    FOOTMARK_RETURN_INT(TRUE); // 処理すべき／処理した
}

BOOL WINAPI
ImeProcessKey(
    HIMC hIMC,
    UINT vKey,
    LPARAM lKeyData,
    CONST LPBYTE lpbKeyState)
{
    // キー判定
    BOOL ret = FALSE;
    InputContext *lpIMC = NULL;
    if (hIMC && (lpIMC = TheIME.LockIMC(hIMC))) {
        BOOL bKeyUp = !!(lKeyData & 0x80000000);
        ret = DoProcessKey(hIMC, lpIMC, vKey, lpbKeyState, bKeyUp, FALSE);
        TheIME.UnlockIMC(hIMC);
    }

    return ret;
}

UINT WINAPI
ImeToAsciiEx(
    UINT uVKey,
    UINT uScanCode,
    CONST LPBYTE lpbKeyState,
    LPTRANSMSGLIST lpTransBuf,
    UINT fuState,
    HIMC hIMC)
{
    FOOTMARK_FORMAT("(%u (0x%X), 0x%X, %p, %p, 0x%X, %p)\n",
                    uVKey, uVKey, uScanCode, lpbKeyState, lpTransBuf, fuState, hIMC);

    TheIME.m_lpCurTransKey = lpTransBuf;
    TheIME.m_uNumTransKey = 0;

    InputContext *lpIMC;
    if (hIMC && (lpIMC = TheIME.LockIMC(hIMC))) {
        BOOL bKeyUp = !!(uScanCode & 0x8000);
        DoProcessKey(hIMC, lpIMC, uVKey, lpbKeyState, bKeyUp, TRUE);

        TheIME.m_lpCurTransKey = NULL;
        TheIME.UnlockIMC(hIMC);
    }

    if (TheIME.m_fOverflowKey) {
        DPRINTA("***************************************\n");
        DPRINTA("*   TransKey OVER FLOW Messages!!!    *\n");
        DPRINTA("*                by MZIMEJA.IME       *\n");
        DPRINTA("***************************************\n");
    }

    FOOTMARK_RETURN_INT(TheIME.m_uNumTransKey);
} // ImeToAsciiEx

//////////////////////////////////////////////////////////////////////////////

}  // extern "C"

//////////////////////////////////////////////////////////////////////////////
