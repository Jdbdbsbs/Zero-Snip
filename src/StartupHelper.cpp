#include "StartupHelper.h"
#include <windows.h>
#include <vector>

namespace StartupHelper {

static const WCHAR RUN_KEY_PATH[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const WCHAR APP_KEY_NAME[] = L"ZeroSnip";
static const WCHAR TASK_NAME[] = L"ZeroSnip";

std::wstring GetExecutablePath() {
    std::vector<WCHAR> buffer(MAX_PATH);
    DWORD len = GetModuleFileNameW(NULL, buffer.data(), (DWORD)buffer.size());
    while (len == buffer.size()) {
        buffer.resize(buffer.size() * 2);
        len = GetModuleFileNameW(NULL, buffer.data(), (DWORD)buffer.size());
    }
    return std::wstring(buffer.data(), len);
}

static bool RunHiddenProcess(const std::wstring& cmd) {
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };
    std::vector<WCHAR> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    if (CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 5000);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return (exitCode == 0);
    }
    return false;
}

bool IsStartupEnabled() {
    // 1. Check Registry Run Key
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY_PATH, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0;
        DWORD size = 0;
        LONG res = RegQueryValueExW(hKey, APP_KEY_NAME, NULL, &type, NULL, &size);
        RegCloseKey(hKey);
        if (res == ERROR_SUCCESS) {
            return true;
        }
    }

    // 2. Check Task Scheduler
    std::wstring checkCmd = L"schtasks.exe /query /tn \"" + std::wstring(TASK_NAME) + L"\"";
    if (RunHiddenProcess(checkCmd)) {
        return true;
    }

    return false;
}

bool EnableStartup(const std::wstring& explicitPath) {
    std::wstring exe = explicitPath.empty() ? GetExecutablePath() : explicitPath;
    if (exe.empty()) return false;

    // 1. Configure Task Scheduler (runs elevated on logon without UAC prompt)
    std::wstring createCmd = L"schtasks.exe /create /tn \"" + std::wstring(TASK_NAME) +
                             L"\" /tr \"\\\"" + exe + L"\\\" --silent\" /sc onlogon /rl highest /f";
    RunHiddenProcess(createCmd);

    // 2. Configure HKCU Run Key as standard backup
    HKEY hKey = NULL;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, RUN_KEY_PATH, 0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        std::wstring regVal = L"\"" + exe + L"\" --silent";
        RegSetValueExW(hKey, APP_KEY_NAME, 0, REG_SZ,
                       (const BYTE*)regVal.c_str(), (DWORD)((regVal.length() + 1) * sizeof(WCHAR)));
        RegCloseKey(hKey);
    }

    return true;
}

bool DisableStartup() {
    // 1. Remove Task Scheduler task
    std::wstring delCmd = L"schtasks.exe /delete /tn \"" + std::wstring(TASK_NAME) + L"\" /f";
    RunHiddenProcess(delCmd);

    // 2. Remove Registry Run Key (Current User)
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY_PATH, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, APP_KEY_NAME);
        RegCloseKey(hKey);
    }

    // 3. Remove Registry Run Key (Local Machine if present)
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, RUN_KEY_PATH, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, APP_KEY_NAME);
        RegCloseKey(hKey);
    }

    return true;
}

} // namespace StartupHelper
