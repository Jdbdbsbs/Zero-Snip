#include <windows.h>
#include <shlobj.h>
#include <string>
#include <vector>

static const WCHAR TASK_NAME[] = L"ZeroSnip";
static const WCHAR REG_RUN[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const WCHAR REG_UNINSTALL[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ZeroSnip";

static void RunCmdHidden(const std::wstring& cmd) {
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = { 0 };
    std::vector<WCHAR> buf(cmd.begin(), cmd.end());
    buf.push_back(L'\0');
    if (CreateProcessW(NULL, buf.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 3000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

static std::wstring GetModuleDir() {
    WCHAR path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring p(path);
    size_t pos = p.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        return p.substr(0, pos);
    }
    return p;
}

static void DeleteShortcut(int csidl, const std::wstring& subPath) {
    WCHAR path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, csidl, NULL, 0, path))) {
        std::wstring fullPath = std::wstring(path) + L"\\" + subPath;
        DeleteFileW(fullPath.c_str());
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd) {
    int choice = MessageBoxW(NULL, 
        L"Are you sure you want to completely uninstall Zero Snip and remove all its components from this computer?",
        L"Zero Snip Uninstall",
        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);

    if (choice != IDYES) {
        return 0;
    }

    // 1. Terminate running Zero Snip processes
    RunCmdHidden(L"taskkill.exe /f /im zerosnip.exe");

    // 2. Remove Task Scheduler Startup task
    RunCmdHidden(L"schtasks.exe /delete /tn \"" + std::wstring(TASK_NAME) + L"\" /f");

    // 3. Remove Registry Run Keys
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_RUN, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, L"ZeroSnip");
        RegCloseKey(hKey);
    }
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, REG_RUN, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, L"ZeroSnip");
        RegCloseKey(hKey);
    }

    // 4. Remove Add/Remove Programs Registry Key
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, REG_UNINSTALL);

    // 5. Remove Start Menu & Desktop Shortcuts
    DeleteShortcut(CSIDL_COMMON_PROGRAMS, L"Zero Snip.lnk");
    DeleteShortcut(CSIDL_PROGRAMS, L"Zero Snip.lnk");
    DeleteShortcut(CSIDL_COMMON_DESKTOPDIRECTORY, L"Zero Snip.lnk");
    DeleteShortcut(CSIDL_DESKTOPDIRECTORY, L"Zero Snip.lnk");

    // 6. Delete application files
    std::wstring installDir = GetModuleDir();
    std::wstring appExe = installDir + L"\\zerosnip.exe";
    DeleteFileW(appExe.c_str());

    // 7. Schedule self-deletion of uninstall.exe and removal of install folder
    WCHAR selfPath[MAX_PATH];
    GetModuleFileNameW(NULL, selfPath, MAX_PATH);

    std::wstring cleanupCmd = L"cmd.exe /c timeout /t 1 /nobreak >nul & del /f /q \"" +
                              std::wstring(selfPath) + L"\" & rmdir /s /q \"" + installDir + L"\"";
    
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = { 0 };
    std::vector<WCHAR> cmdBuf(cleanupCmd.begin(), cleanupCmd.end());
    cmdBuf.push_back(L'\0');
    CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (pi.hProcess) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    MessageBoxW(NULL,
        L"Zero Snip was successfully removed from your computer.",
        L"Uninstall Complete",
        MB_OK | MB_ICONINFORMATION);

    return 0;
}
