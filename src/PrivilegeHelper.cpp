#include "PrivilegeHelper.h"
#include <shellapi.h>

bool PrivilegeHelper::IsProcessElevated() {
    BOOL fIsElevated = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            fIsElevated = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return fIsElevated != FALSE;
}

bool PrivilegeHelper::EnableDebugPrivilege() {
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }

    LUID luid;
    if (!LookupPrivilegeValueW(NULL, SE_DEBUG_NAME, &luid)) {
        CloseHandle(hToken);
        return false;
    }

    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    BOOL result = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
    CloseHandle(hToken);
    return result && (GetLastError() != ERROR_NOT_ALL_ASSIGNED);
}

bool PrivilegeHelper::RelaunchElevated(const WCHAR* lpParameters) {
    WCHAR szPath[MAX_PATH];
    if (GetModuleFileNameW(NULL, szPath, MAX_PATH) == 0) {
        return false;
    }

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas"; // triggers UAC elevation prompt
    sei.lpFile = szPath;
    sei.lpParameters = lpParameters;
    sei.nShow = SW_NORMAL;

    if (!ShellExecuteExW(&sei)) {
        return false;
    }
    return true;
}
