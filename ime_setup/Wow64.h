
#pragma once

typedef BOOL (WINAPI *FN_IsWow64Process)(HANDLE hProcess, PBOOL Wow64Process);
typedef BOOL (WINAPI *FN_Wow64DisableWow64FsRedirection)(PVOID *OldValue);
typedef BOOL (WINAPI *FN_Wow64RevertWow64FsRedirection)(PVOID OldValue);

static inline BOOL IsWow64(void)
{
    HINSTANCE hKernel32 = GetModuleHandleA("kernel32");
    FN_IsWow64Process fn = (FN_IsWow64Process)GetProcAddress(hKernel32, "IsWow64Process");

    BOOL bIsWow64;
    return fn && fn(GetCurrentProcess(), &bIsWow64) && bIsWow64;
}

static inline BOOL DisableWow64FsRedirection(PVOID *OldValue)
{
    HINSTANCE hKernel32 = GetModuleHandleA("kernel32");
    FN_Wow64DisableWow64FsRedirection pWow64DisableWow64FsRedirection;
    pWow64DisableWow64FsRedirection =
        (FN_Wow64DisableWow64FsRedirection)GetProcAddress(hKernel32, "Wow64DisableWow64FsRedirection");
    if (pWow64DisableWow64FsRedirection)
        return (*pWow64DisableWow64FsRedirection)(OldValue);
    return FALSE;
}

static inline BOOL RevertWow64FsRedirection(PVOID OldValue)
{
    HINSTANCE hKernel32 = GetModuleHandleA("kernel32");
    FN_Wow64RevertWow64FsRedirection pWow64RevertWow64FsRedirection;
    pWow64RevertWow64FsRedirection =
        (FN_Wow64RevertWow64FsRedirection)GetProcAddress(hKernel32, "Wow64RevertWow64FsRedirection");
    if (pWow64RevertWow64FsRedirection)
        return (*pWow64RevertWow64FsRedirection)(OldValue);
    return FALSE;
}
