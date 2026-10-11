#include <windows.h>
#include <windowsx.h>
#include <initguid.h>
#include <gdiplus.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <dwmapi.h>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "dwmapi.lib")

#define IDR_PAYLOAD_APP       101
#define IDR_PAYLOAD_UNINSTALL 102
#define IDR_LOGO_PNG          103

#define WM_INSTALL_PROGRESS (WM_USER + 201)
#define WM_INSTALL_COMPLETE (WM_USER + 202)

enum SetupState {
    STATE_CONFIG = 0,
    STATE_INSTALLING = 1,
    STATE_COMPLETED = 2
};

struct InstallerApp {
    HWND hWnd = NULL;
    SetupState state = STATE_CONFIG;
    Gdiplus::Bitmap* pLogo = nullptr;

    std::wstring installDir = L"C:\\Program Files\\Zero Snip";
    bool optStartup = true;
    bool optDesktopShortcut = true;
    bool optStartMenuShortcut = true;

    // Progress
    int progressPercent = 0;
    std::wstring currentStatus = L"Ready to install...";

    // UI Hit Rects
    RECT rcBrowseBtn = { 0 };
    RECT rcStartupCheck = { 0 };
    RECT rcDesktopCheck = { 0 };
    RECT rcStartMenuCheck = { 0 };
    RECT rcInstallBtn = { 0 };
    RECT rcCancelBtn = { 0 };
    RECT rcFinishBtn = { 0 };

    bool hoverBrowse = false;
    bool hoverStartup = false;
    bool hoverDesktop = false;
    bool hoverStartMenu = false;
    bool hoverInstall = false;
    bool hoverCancel = false;
    bool hoverFinish = false;
};

static InstallerApp g_app;

static void EnableDpiAwareness() {
    typedef BOOL(WINAPI* PFN_SetProcessDpiAwarenessContext)(DPI_AWARENESS_CONTEXT);
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        PFN_SetProcessDpiAwarenessContext pfn = 
            (PFN_SetProcessDpiAwarenessContext)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        if (pfn) {
            pfn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            return;
        }
    }
    SetProcessDPIAware();
}

static void ApplyImmersiveDarkMode(HWND hWnd) {
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hWnd, 20, &darkMode, sizeof(darkMode)); // DWMWA_USE_IMMERSIVE_DARK_MODE
    DwmSetWindowAttribute(hWnd, 19, &darkMode, sizeof(darkMode));
    COLORREF captionColor = RGB(13, 13, 18);
    DwmSetWindowAttribute(hWnd, 35, &captionColor, sizeof(captionColor)); // DWMWA_CAPTION_COLOR
}

static Gdiplus::Bitmap* LoadBitmapFromResource(int resId) {
    HRSRC hRes = FindResourceW(NULL, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!hRes) return nullptr;
    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData) return nullptr;
    DWORD size = SizeofResource(NULL, hRes);
    void* pData = LockResource(hData);
    if (!pData || size == 0) return nullptr;

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!hMem) return nullptr;
    void* pMem = GlobalLock(hMem);
    memcpy(pMem, pData, size);
    GlobalUnlock(hMem);

    IStream* pStream = nullptr;
    if (CreateStreamOnHGlobal(hMem, TRUE, &pStream) == S_OK) {
        Gdiplus::Bitmap* pBmp = Gdiplus::Bitmap::FromStream(pStream);
        pStream->Release();
        return pBmp;
    }
    GlobalFree(hMem);
    return nullptr;
}

static bool ExtractResourceToFile(int resId, const std::wstring& outPath) {
    HRSRC hRes = FindResourceW(NULL, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!hRes) return false;
    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData) return false;
    DWORD size = SizeofResource(NULL, hRes);
    void* pData = LockResource(hData);
    if (!pData || size == 0) return false;

    HANDLE hFile = CreateFileW(outPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    BOOL ok = WriteFile(hFile, pData, size, &written, NULL);
    CloseHandle(hFile);
    return (ok && written == size);
}

static bool CreateLnkShortcut(const std::wstring& targetPath, const std::wstring& linkPath, const std::wstring& desc, const std::wstring& args = L"") {
    IShellLinkW* psl = NULL;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID*)&psl);
    if (SUCCEEDED(hr)) {
        psl->SetPath(targetPath.c_str());
        if (!args.empty()) psl->SetArguments(args.c_str());
        if (!desc.empty()) psl->SetDescription(desc.c_str());
        psl->SetIconLocation(targetPath.c_str(), 0);

        IPersistFile* ppf = NULL;
        hr = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);
        if (SUCCEEDED(hr)) {
            hr = ppf->Save(linkPath.c_str(), TRUE);
            ppf->Release();
        }
        psl->Release();
    }
    return SUCCEEDED(hr);
}

static void RunCmdHidden(const std::wstring& cmd) {
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = { 0 };
    std::vector<WCHAR> buf(cmd.begin(), cmd.end());
    buf.push_back(L'\0');
    if (CreateProcessW(NULL, buf.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 4000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

static void RegisterUninstallEntry(const std::wstring& installDir, const std::wstring& exePath, const std::wstring& uninstallerPath) {
    HKEY hKey = NULL;
    const WCHAR* subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ZeroSnip";
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, subKey, 0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        const WCHAR* name = L"Zero Snip";
        RegSetValueExW(hKey, L"DisplayName", 0, REG_SZ, (const BYTE*)name, (DWORD)((wcslen(name) + 1) * sizeof(WCHAR)));
        
        const WCHAR* ver = L"1.0.0";
        RegSetValueExW(hKey, L"DisplayVersion", 0, REG_SZ, (const BYTE*)ver, (DWORD)((wcslen(ver) + 1) * sizeof(WCHAR)));

        const WCHAR* pub = L"Zero Snip";
        RegSetValueExW(hKey, L"Publisher", 0, REG_SZ, (const BYTE*)pub, (DWORD)((wcslen(pub) + 1) * sizeof(WCHAR)));

        RegSetValueExW(hKey, L"InstallLocation", 0, REG_SZ, (const BYTE*)installDir.c_str(), (DWORD)((installDir.length() + 1) * sizeof(WCHAR)));

        std::wstring uninstCmd = L"\"" + uninstallerPath + L"\"";
        RegSetValueExW(hKey, L"UninstallString", 0, REG_SZ, (const BYTE*)uninstCmd.c_str(), (DWORD)((uninstCmd.length() + 1) * sizeof(WCHAR)));

        std::wstring iconVal = exePath + L",0";
        RegSetValueExW(hKey, L"DisplayIcon", 0, REG_SZ, (const BYTE*)iconVal.c_str(), (DWORD)((iconVal.length() + 1) * sizeof(WCHAR)));

        DWORD sizeKb = 2048;
        RegSetValueExW(hKey, L"EstimatedSize", 0, REG_DWORD, (const BYTE*)&sizeKb, sizeof(DWORD));

        DWORD noModify = 1;
        RegSetValueExW(hKey, L"NoModify", 0, REG_DWORD, (const BYTE*)&noModify, sizeof(DWORD));
        RegSetValueExW(hKey, L"NoRepair", 0, REG_DWORD, (const BYTE*)&noModify, sizeof(DWORD));

        RegCloseKey(hKey);
    }
}

static void DoInstallWorker() {
    auto update = [](int pct, const std::wstring& text) {
        g_app.progressPercent = pct;
        g_app.currentStatus = text;
        PostMessageW(g_app.hWnd, WM_INSTALL_PROGRESS, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(180));
    };

    update(10, L"Terminating existing instances...");
    RunCmdHidden(L"taskkill.exe /f /im zerosnip.exe");

    update(25, L"Creating installation folder: " + g_app.installDir);
    SHCreateDirectoryExW(NULL, g_app.installDir.c_str(), NULL);

    std::wstring appExe = g_app.installDir + L"\\zerosnip.exe";
    std::wstring uninstExe = g_app.installDir + L"\\uninstall.exe";

    update(45, L"Installing zerosnip.exe (~1.0 MB)...");
    ExtractResourceToFile(IDR_PAYLOAD_APP, appExe);

    update(60, L"Installing uninstaller (uninstall.exe)...");
    ExtractResourceToFile(IDR_PAYLOAD_UNINSTALL, uninstExe);

    update(70, L"Creating shortcuts...");
    WCHAR pathBuf[MAX_PATH];
    if (g_app.optStartMenuShortcut && SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_COMMON_PROGRAMS, NULL, 0, pathBuf))) {
        std::wstring lnk = std::wstring(pathBuf) + L"\\Zero Snip.lnk";
        CreateLnkShortcut(appExe, lnk, L"Ultra-lightweight snipping tool");
    }
    if (g_app.optDesktopShortcut && SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_COMMON_DESKTOPDIRECTORY, NULL, 0, pathBuf))) {
        std::wstring lnk = std::wstring(pathBuf) + L"\\Zero Snip.lnk";
        CreateLnkShortcut(appExe, lnk, L"Ultra-lightweight snipping tool");
    }

    if (g_app.optStartup) {
        update(80, L"Configuring Windows Startup Task (runs on boot elevated & silent)...");
        std::wstring schCmd = L"schtasks.exe /create /tn \"ZeroSnip\" /tr \"\\\"" + appExe + L"\\\" --silent\" /sc onlogon /rl highest /f";
        RunCmdHidden(schCmd);

        HKEY hRun = NULL;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, NULL, 0, KEY_SET_VALUE, NULL, &hRun, NULL) == ERROR_SUCCESS) {
            std::wstring regVal = L"\"" + appExe + L"\" --silent";
            RegSetValueExW(hRun, L"ZeroSnip", 0, REG_SZ, (const BYTE*)regVal.c_str(), (DWORD)((regVal.length() + 1) * sizeof(WCHAR)));
            RegCloseKey(hRun);
        }
    } else {
        update(80, L"Skipping startup configuration per user choice...");
    }

    update(90, L"Registering in Windows Programs and Features...");
    RegisterUninstallEntry(g_app.installDir, appExe, uninstExe);

    update(95, L"Starting Zero Snip silently in the background...");
    std::wstring launchCmd = L"\"" + appExe + L"\" --silent";
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    std::vector<WCHAR> launchBuf(launchCmd.begin(), launchCmd.end());
    launchBuf.push_back(L'\0');
    if (CreateProcessW(NULL, launchBuf.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    update(100, L"Installation completed successfully!");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    PostMessageW(g_app.hWnd, WM_INSTALL_COMPLETE, 0, 0);
}

// UI Drawing Helpers
static void DrawRoundedRect(Gdiplus::Graphics& g, Gdiplus::Pen* pen, Gdiplus::Brush* brush, const Gdiplus::RectF& rect, float radius) {
    Gdiplus::GraphicsPath path;
    float d = radius * 2.0f;
    if (d > rect.Width) d = rect.Width;
    if (d > rect.Height) d = rect.Height;
    path.AddArc(rect.X, rect.Y, d, d, 180, 90);
    path.AddArc(rect.X + rect.Width - d, rect.Y, d, d, 270, 90);
    path.AddArc(rect.X + rect.Width - d, rect.Y + rect.Height - d, d, d, 0, 90);
    path.AddArc(rect.X, rect.Y + rect.Height - d, d, d, 90, 90);
    path.CloseFigure();

    if (brush) g.FillPath(brush, &path);
    if (pen) g.DrawPath(pen, &path);
}

static void OnPaint(HWND hWnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);

    RECT clientRc;
    GetClientRect(hWnd, &clientRc);
    int width = clientRc.right - clientRc.left;
    int height = clientRc.bottom - clientRc.top;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBM = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBM = (HBITMAP)SelectObject(memDC, memBM);

    {
        Gdiplus::Graphics g(memDC);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        // Solid Background canvas #0d0d12
        Gdiplus::SolidBrush bgBrush(Gdiplus::Color(255, 13, 13, 18));
        g.FillRectangle(&bgBrush, 0, 0, width, height);

        Gdiplus::Font fontHeader(L"Segoe UI", 16.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::Font fontSub(L"Segoe UI", 11.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::Font fontCardTitle(L"Segoe UI", 12.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::Font fontBody(L"Segoe UI", 11.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::Font fontSmall(L"Segoe UI", 10.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::Font fontBtn(L"Segoe UI", 12.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);

        Gdiplus::SolidBrush whiteBrush(Gdiplus::Color(255, 255, 255, 255));
        Gdiplus::SolidBrush textDimBrush(Gdiplus::Color(255, 155, 155, 170));
        Gdiplus::SolidBrush cardBgBrush(Gdiplus::Color(255, 22, 22, 30));
        Gdiplus::Pen cardBorderPen(Gdiplus::Color(255, 42, 42, 56), 1.0f);

        // Header Section with App Logo
        {
            float logoX = 28.0f;
            float logoY = 16.0f;
            float logoSize = 46.0f;

            if (g_app.pLogo) {
                Gdiplus::RectF logoFrame(logoX, logoY, logoSize, logoSize);
                Gdiplus::SolidBrush logoFrameBg(Gdiplus::Color(255, 0, 0, 0));
                Gdiplus::Pen logoFramePen(Gdiplus::Color(255, 50, 50, 65), 1.0f);
                DrawRoundedRect(g, &logoFramePen, &logoFrameBg, logoFrame, 8.0f);
                g.DrawImage(g_app.pLogo, logoX + 3.0f, logoY + 3.0f, logoSize - 6.0f, logoSize - 6.0f);
            }

            float textLeft = g_app.pLogo ? (logoX + logoSize + 14.0f) : 28.0f;

            // App Title + Version Pill
            g.DrawString(L"Zero Snip Setup", -1, &fontHeader, Gdiplus::PointF(textLeft, 18.0f), &whiteBrush);

            Gdiplus::RectF pillRc(textLeft + 160.0f, 19.0f, 52.0f, 19.0f);
            Gdiplus::SolidBrush pillBg(Gdiplus::Color(255, 0, 122, 255));
            DrawRoundedRect(g, nullptr, &pillBg, pillRc, 4.0f);
            Gdiplus::Font pillFont(L"Segoe UI", 9.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
            g.DrawString(L"v1.0.0", -1, &pillFont, Gdiplus::PointF(textLeft + 167.0f, 22.0f), &whiteBrush);

            g.DrawString(L"Ultra-lightweight snipping tool \u2014 Instant clipboard copy, zero disk clutter.",
                         -1, &fontSub, Gdiplus::PointF(textLeft, 43.0f), &textDimBrush);

            Gdiplus::Pen sepPen(Gdiplus::Color(255, 34, 34, 46), 1.0f);
            g.DrawLine(&sepPen, 28, 72, width - 28, 72);
        }

        if (g_app.state == STATE_CONFIG) {
            // Card 1: Target Location
            Gdiplus::RectF c1(28.0f, 85.0f, (float)(width - 56), 66.0f);
            DrawRoundedRect(g, &cardBorderPen, &cardBgBrush, c1, 8.0f);

            g.DrawString(L"Installation Folder", -1, &fontCardTitle, Gdiplus::PointF(42.0f, 95.0f), &whiteBrush);

            // Path box
            Gdiplus::RectF pBox(42.0f, 117.0f, (float)(width - 170), 24.0f);
            Gdiplus::SolidBrush pBoxBg(Gdiplus::Color(255, 14, 14, 20));
            Gdiplus::Pen pBoxBorder(Gdiplus::Color(255, 55, 55, 70), 1.0f);
            DrawRoundedRect(g, &pBoxBorder, &pBoxBg, pBox, 4.0f);
            g.DrawString(g_app.installDir.c_str(), -1, &fontBody, Gdiplus::PointF(48.0f, 121.0f), &textDimBrush);

            // Browse button
            Gdiplus::RectF bBtn((float)(width - 118), 117.0f, 76.0f, 24.0f);
            g_app.rcBrowseBtn = { (LONG)bBtn.X, (LONG)bBtn.Y, (LONG)(bBtn.X + bBtn.Width), (LONG)(bBtn.Y + bBtn.Height) };
            Gdiplus::SolidBrush bBtnBg(g_app.hoverBrowse ? Gdiplus::Color(255, 48, 48, 65) : Gdiplus::Color(255, 35, 35, 48));
            Gdiplus::Pen bBtnBorder(Gdiplus::Color(255, 70, 70, 90), 1.0f);
            DrawRoundedRect(g, &bBtnBorder, &bBtnBg, bBtn, 4.0f);
            g.DrawString(L"Browse...", -1, &fontSmall, Gdiplus::PointF(bBtn.X + 15.0f, bBtn.Y + 4.5f), &whiteBrush);

            // Card 2: STARTUP PERMISSION & SOLID CARD (Clean solid aesthetic, NO green transparent slop)
            Gdiplus::RectF c2(28.0f, 162.0f, (float)(width - 56), 180.0f);
            DrawRoundedRect(g, &cardBorderPen, &cardBgBrush, c2, 8.0f);

            g.DrawString(L"Startup Permission & Behavior", -1, &fontCardTitle, Gdiplus::PointF(42.0f, 172.0f), &whiteBrush);

            // Startup Checkbox row
            Gdiplus::RectF chkBox(42.0f, 196.0f, 18.0f, 18.0f);
            g_app.rcStartupCheck = { 42, 194, width - 42, 218 };
            Gdiplus::SolidBrush chkBg(g_app.optStartup ? Gdiplus::Color(255, 0, 122, 255) : Gdiplus::Color(255, 35, 35, 48));
            Gdiplus::Pen chkBorder(g_app.hoverStartup ? Gdiplus::Color(255, 0, 150, 255) : Gdiplus::Color(255, 70, 70, 90), 1.5f);
            DrawRoundedRect(g, &chkBorder, &chkBg, chkBox, 4.0f);

            if (g_app.optStartup) {
                Gdiplus::Pen checkPen(Gdiplus::Color(255, 255, 255, 255), 2.0f);
                g.DrawLine(&checkPen, 46.0f, 205.0f, 49.5f, 209.0f);
                g.DrawLine(&checkPen, 49.5f, 209.0f, 56.0f, 200.0f);
            }

            g.DrawString(L"Start Zero Snip automatically on Windows startup (Recommended)",
                         -1, &fontCardTitle, Gdiplus::PointF(68.0f, 196.0f), &whiteBrush);

            // Solid informative card replacing previous green box
            if (g_app.optStartup) {
                // Enabled state: Solid dark surface with subtle border
                Gdiplus::RectF solidBox(42.0f, 226.0f, (float)(width - 84), 102.0f);
                Gdiplus::SolidBrush solidBg(Gdiplus::Color(255, 16, 16, 22)); // Solid dark, not transparent
                Gdiplus::Pen solidBorder(Gdiplus::Color(255, 38, 38, 52), 1.0f);
                DrawRoundedRect(g, &solidBorder, &solidBg, solidBox, 6.0f);

                // Section Badge Pill
                Gdiplus::RectF tagPill(54.0f, 235.0f, 215.0f, 18.0f);
                Gdiplus::SolidBrush tagBg(Gdiplus::Color(255, 28, 28, 40));
                DrawRoundedRect(g, nullptr, &tagBg, tagPill, 4.0f);
                Gdiplus::Font tagFont(L"Segoe UI", 9.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
                Gdiplus::SolidBrush tagText(Gdiplus::Color(255, 200, 200, 220));
                g.DrawString(L"AUTOSTART CONFIGURATION", -1, &tagFont, Gdiplus::PointF(62.0f, 238.0f), &tagText);

                g.DrawString(L"\u2022 Launches silently into system tray when Windows boots up.\n"
                             L"\u2022 F14, Ctrl+Shift+S, and PrintScreen hotkeys will be active immediately.\n"
                             L"\u2022 Uses Windows Task Scheduler to run with admin rights without UAC prompts.",
                             -1, &fontSmall, Gdiplus::PointF(54.0f, 260.0f), &textDimBrush);
            } else {
                // Disabled state: Solid dark warning card
                Gdiplus::RectF solidBox(42.0f, 226.0f, (float)(width - 84), 102.0f);
                Gdiplus::SolidBrush solidBg(Gdiplus::Color(255, 32, 18, 22)); // Solid crimson/dark red
                Gdiplus::Pen solidBorder(Gdiplus::Color(255, 75, 32, 40), 1.0f);
                DrawRoundedRect(g, &solidBorder, &solidBg, solidBox, 6.0f);

                Gdiplus::RectF tagPill(54.0f, 235.0f, 175.0f, 18.0f);
                Gdiplus::SolidBrush tagBg(Gdiplus::Color(255, 60, 24, 30));
                DrawRoundedRect(g, nullptr, &tagBg, tagPill, 4.0f);
                Gdiplus::Font tagFont(L"Segoe UI", 9.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
                Gdiplus::SolidBrush tagText(Gdiplus::Color(255, 255, 140, 140));
                g.DrawString(L"\u26A0  STARTUP DISABLED", -1, &tagFont, Gdiplus::PointF(62.0f, 238.0f), &tagText);

                Gdiplus::SolidBrush alertBodyText(Gdiplus::Color(255, 230, 210, 210));
                g.DrawString(L"\u2022 Zero Snip will NOT run when Windows boots up.\n"
                             L"\u2022 You will need to manually open Zero Snip every time you restart your PC.\n"
                             L"\u2022 Hotkeys (F14 / Ctrl+Shift+S) will NOT work until the app is opened.",
                             -1, &fontSmall, Gdiplus::PointF(54.0f, 260.0f), &alertBodyText);
            }

            // Card 3: Additional shortcuts
            Gdiplus::RectF c3(28.0f, 353.0f, (float)(width - 56), 66.0f);
            DrawRoundedRect(g, &cardBorderPen, &cardBgBrush, c3, 8.0f);

            // Desktop shortcut
            Gdiplus::RectF dBox(42.0f, 365.0f, 16.0f, 16.0f);
            g_app.rcDesktopCheck = { 42, 364, 250, 384 };
            Gdiplus::SolidBrush dBg(g_app.optDesktopShortcut ? Gdiplus::Color(255, 0, 122, 255) : Gdiplus::Color(255, 35, 35, 48));
            Gdiplus::Pen dBorder(g_app.hoverDesktop ? Gdiplus::Color(255, 0, 150, 255) : Gdiplus::Color(255, 70, 70, 90), 1.2f);
            DrawRoundedRect(g, &dBorder, &dBg, dBox, 3.0f);
            if (g_app.optDesktopShortcut) {
                Gdiplus::Pen checkPen(Gdiplus::Color(255, 255, 255, 255), 1.8f);
                g.DrawLine(&checkPen, 45.0f, 373.0f, 48.0f, 376.0f);
                g.DrawLine(&checkPen, 48.0f, 376.0f, 54.0f, 368.0f);
            }
            g.DrawString(L"Create Desktop shortcut", -1, &fontBody, Gdiplus::PointF(66.0f, 365.0f), &whiteBrush);

            // Start menu shortcut
            Gdiplus::RectF smBox(42.0f, 391.0f, 16.0f, 16.0f);
            g_app.rcStartMenuCheck = { 42, 390, 250, 410 };
            Gdiplus::SolidBrush smBg(g_app.optStartMenuShortcut ? Gdiplus::Color(255, 0, 122, 255) : Gdiplus::Color(255, 35, 35, 48));
            Gdiplus::Pen smBorder(g_app.hoverStartMenu ? Gdiplus::Color(255, 0, 150, 255) : Gdiplus::Color(255, 70, 70, 90), 1.2f);
            DrawRoundedRect(g, &smBorder, &smBg, smBox, 3.0f);
            if (g_app.optStartMenuShortcut) {
                Gdiplus::Pen checkPen(Gdiplus::Color(255, 255, 255, 255), 1.8f);
                g.DrawLine(&checkPen, 45.0f, 399.0f, 48.0f, 402.0f);
                g.DrawLine(&checkPen, 48.0f, 402.0f, 54.0f, 394.0f);
            }
            g.DrawString(L"Create Start Menu shortcut", -1, &fontBody, Gdiplus::PointF(66.0f, 391.0f), &whiteBrush);

            // Bottom Buttons
            Gdiplus::RectF cancelBtn((float)(width - 240), (float)(height - 56), 96.0f, 36.0f);
            g_app.rcCancelBtn = { (LONG)cancelBtn.X, (LONG)cancelBtn.Y, (LONG)(cancelBtn.X + cancelBtn.Width), (LONG)(cancelBtn.Y + cancelBtn.Height) };
            Gdiplus::SolidBrush cancelBg(g_app.hoverCancel ? Gdiplus::Color(255, 45, 45, 58) : Gdiplus::Color(255, 30, 30, 40));
            Gdiplus::Pen cancelBorder(Gdiplus::Color(255, 60, 60, 75), 1.0f);
            DrawRoundedRect(g, &cancelBorder, &cancelBg, cancelBtn, 6.0f);
            g.DrawString(L"Cancel", -1, &fontBtn, Gdiplus::PointF(cancelBtn.X + 24.0f, cancelBtn.Y + 9.0f), &whiteBrush);

            Gdiplus::RectF installBtn((float)(width - 132), (float)(height - 56), 104.0f, 36.0f);
            g_app.rcInstallBtn = { (LONG)installBtn.X, (LONG)installBtn.Y, (LONG)(installBtn.X + installBtn.Width), (LONG)(installBtn.Y + installBtn.Height) };
            Gdiplus::SolidBrush installBg(g_app.hoverInstall ? Gdiplus::Color(255, 0, 140, 255) : Gdiplus::Color(255, 0, 122, 255));
            DrawRoundedRect(g, nullptr, &installBg, installBtn, 6.0f);
            g.DrawString(L"Install Now", -1, &fontBtn, Gdiplus::PointF(installBtn.X + 16.0f, installBtn.Y + 9.0f), &whiteBrush);
        }
        else if (g_app.state == STATE_INSTALLING) {
            // Installing Progress Screen
            Gdiplus::RectF card(28.0f, 130.0f, (float)(width - 56), 200.0f);
            DrawRoundedRect(g, &cardBorderPen, &cardBgBrush, card, 8.0f);

            g.DrawString(L"Installing Zero Snip...", -1, &fontHeader, Gdiplus::PointF(48.0f, 155.0f), &whiteBrush);

            // Status message
            g.DrawString(g_app.currentStatus.c_str(), -1, &fontBody, Gdiplus::PointF(48.0f, 195.0f), &textDimBrush);

            // Progress bar track
            Gdiplus::RectF pTrack(48.0f, 225.0f, (float)(width - 152), 16.0f);
            Gdiplus::SolidBrush trackBg(Gdiplus::Color(255, 14, 14, 20));
            Gdiplus::Pen trackBorder(Gdiplus::Color(255, 50, 50, 65), 1.0f);
            DrawRoundedRect(g, &trackBorder, &trackBg, pTrack, 8.0f);

            // Progress bar fill
            float fillW = (float)(width - 152) * (g_app.progressPercent / 100.0f);
            if (fillW > 16.0f) {
                Gdiplus::RectF pFill(48.0f, 225.0f, fillW, 16.0f);
                Gdiplus::SolidBrush fillBrush(Gdiplus::Color(255, 0, 150, 255));
                DrawRoundedRect(g, nullptr, &fillBrush, pFill, 8.0f);
            }

            // Percentage
            WCHAR pctStr[16];
            swprintf_s(pctStr, L"%d%%", g_app.progressPercent);
            g.DrawString(pctStr, -1, &fontCardTitle, Gdiplus::PointF((float)(width - 92), 224.0f), &whiteBrush);
        }
        else if (g_app.state == STATE_COMPLETED) {
            // Completed Screen
            Gdiplus::RectF card(28.0f, 95.0f, (float)(width - 56), 310.0f);
            DrawRoundedRect(g, &cardBorderPen, &cardBgBrush, card, 8.0f);

            // Green success circle with checkmark
            Gdiplus::RectF circleRc(48.0f, 120.0f, 44.0f, 44.0f);
            Gdiplus::SolidBrush circleBg(Gdiplus::Color(255, 16, 124, 65));
            g.FillEllipse(&circleBg, circleRc);

            Gdiplus::Pen checkPen(Gdiplus::Color(255, 255, 255, 255), 3.0f);
            g.DrawLine(&checkPen, 58.0f, 142.0f, 66.0f, 150.0f);
            g.DrawLine(&checkPen, 66.0f, 150.0f, 82.0f, 134.0f);

            g.DrawString(L"Installation Complete!", -1, &fontHeader, Gdiplus::PointF(106.0f, 124.0f), &whiteBrush);
            g.DrawString(L"Zero Snip is now running silently in your system tray.", -1, &fontSub, Gdiplus::PointF(106.0f, 148.0f), &textDimBrush);

            // Features recap card
            Gdiplus::RectF infoCard(48.0f, 185.0f, (float)(width - 96), 180.0f);
            Gdiplus::SolidBrush infoBg(Gdiplus::Color(255, 15, 15, 22));
            Gdiplus::Pen infoBorder(Gdiplus::Color(255, 38, 38, 50), 1.0f);
            DrawRoundedRect(g, &infoBorder, &infoBg, infoCard, 6.0f);

            g.DrawString(L"Quick Start Guide:", -1, &fontCardTitle, Gdiplus::PointF(64.0f, 198.0f), &whiteBrush);

            std::wstring guideText = 
                L"\u2022 Press F14 or Ctrl+Shift+S (or PrintScreen) to crop and capture.\n"
                L"\u2022 Captures are immediately in your clipboard \u2014 paste anywhere (Ctrl+V).\n"
                L"\u2022 Zero disk clutter: Nothing is saved unless you click 'Save this' in the preview.\n"
                L"\u2022 Captures over all elevated and protected windows seamlessly.\n"
                L"\u2022 Right-click the system tray icon to toggle startup, take manual snips, or exit.";

            g.DrawString(guideText.c_str(), -1, &fontSmall, Gdiplus::PointF(64.0f, 226.0f), &textDimBrush);

            // Finish button
            Gdiplus::RectF finishBtn((float)(width - 150), (float)(height - 56), 122.0f, 36.0f);
            g_app.rcFinishBtn = { (LONG)finishBtn.X, (LONG)finishBtn.Y, (LONG)(finishBtn.X + finishBtn.Width), (LONG)(finishBtn.Y + finishBtn.Height) };
            Gdiplus::SolidBrush finishBg(g_app.hoverFinish ? Gdiplus::Color(255, 0, 140, 255) : Gdiplus::Color(255, 0, 122, 255));
            DrawRoundedRect(g, nullptr, &finishBg, finishBtn, 6.0f);
            g.DrawString(L"Finish & Enjoy", -1, &fontBtn, Gdiplus::PointF(finishBtn.X + 16.0f, finishBtn.Y + 9.0f), &whiteBrush);
        }
    }

    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBM);
    DeleteObject(memBM);
    DeleteDC(memDC);
    EndPaint(hWnd, &ps);
}

static bool PtInR(const RECT& rc, int x, int y) {
    return (x >= rc.left && x <= rc.right && y >= rc.top && y <= rc.bottom);
}

static void PromptStartupWarningAndInstall(HWND hWnd) {
    if (!g_app.optStartup) {
        int res = MessageBoxW(hWnd,
            L"\u26A0 WARNING: Startup Permission is Disabled!\n\n"
            L"Zero Snip will NOT run when Windows boots up.\n\n"
            L"You will need to manually open Zero Snip every single time you restart your computer, "
            L"otherwise your snipping hotkeys (F14 / Ctrl+Shift+S) will not be active.\n\n"
            L"Are you sure you want to proceed without adding to startup?\n"
            L"(Click 'No' to re-enable automatic startup)",
            L"Zero Snip Setup \u2014 Startup Permission Warning",
            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);

        if (res != IDYES) {
            g_app.optStartup = true;
            InvalidateRect(hWnd, NULL, FALSE);
            return;
        }
    }

    // Switch to Installing state
    g_app.state = STATE_INSTALLING;
    InvalidateRect(hWnd, NULL, FALSE);

    std::thread t(DoInstallWorker);
    t.detach();
}

static void OnBrowse(HWND hWnd) {
    BROWSEINFOW bi = { 0 };
    bi.hwndOwner = hWnd;
    bi.lpszTitle = L"Select Installation Directory for Zero Snip";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        WCHAR path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            g_app.installDir = std::wstring(path) + L"\\Zero Snip";
            InvalidateRect(hWnd, NULL, FALSE);
        }
        CoTaskMemFree(pidl);
    }
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_app.hWnd = hWnd;
        ApplyImmersiveDarkMode(hWnd);
        SendMessageW(hWnd, WM_SETICON, ICON_BIG, (LPARAM)LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1)));
        SendMessageW(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
        return 0;

    case WM_PAINT:
        OnPaint(hWnd);
        return 0;

    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        bool prevHover = g_app.hoverBrowse || g_app.hoverStartup || g_app.hoverDesktop || 
                         g_app.hoverStartMenu || g_app.hoverInstall || g_app.hoverCancel || g_app.hoverFinish;

        g_app.hoverBrowse = PtInR(g_app.rcBrowseBtn, x, y);
        g_app.hoverStartup = PtInR(g_app.rcStartupCheck, x, y);
        g_app.hoverDesktop = PtInR(g_app.rcDesktopCheck, x, y);
        g_app.hoverStartMenu = PtInR(g_app.rcStartMenuCheck, x, y);
        g_app.hoverInstall = PtInR(g_app.rcInstallBtn, x, y);
        g_app.hoverCancel = PtInR(g_app.rcCancelBtn, x, y);
        g_app.hoverFinish = PtInR(g_app.rcFinishBtn, x, y);

        bool newHover = g_app.hoverBrowse || g_app.hoverStartup || g_app.hoverDesktop || 
                        g_app.hoverStartMenu || g_app.hoverInstall || g_app.hoverCancel || g_app.hoverFinish;

        if (prevHover != newHover) {
            SetCursor(LoadCursor(NULL, newHover ? IDC_HAND : IDC_ARROW));
            InvalidateRect(hWnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        if (g_app.state == STATE_CONFIG) {
            if (PtInR(g_app.rcBrowseBtn, x, y)) {
                OnBrowse(hWnd);
            } else if (PtInR(g_app.rcStartupCheck, x, y)) {
                g_app.optStartup = !g_app.optStartup;
                InvalidateRect(hWnd, NULL, FALSE);
            } else if (PtInR(g_app.rcDesktopCheck, x, y)) {
                g_app.optDesktopShortcut = !g_app.optDesktopShortcut;
                InvalidateRect(hWnd, NULL, FALSE);
            } else if (PtInR(g_app.rcStartMenuCheck, x, y)) {
                g_app.optStartMenuShortcut = !g_app.optStartMenuShortcut;
                InvalidateRect(hWnd, NULL, FALSE);
            } else if (PtInR(g_app.rcInstallBtn, x, y)) {
                PromptStartupWarningAndInstall(hWnd);
            } else if (PtInR(g_app.rcCancelBtn, x, y)) {
                PostQuitMessage(0);
            }
        } else if (g_app.state == STATE_COMPLETED) {
            if (PtInR(g_app.rcFinishBtn, x, y)) {
                PostQuitMessage(0);
            }
        }
        return 0;
    }

    case WM_INSTALL_PROGRESS:
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;

    case WM_INSTALL_COMPLETE:
        g_app.state = STATE_COMPLETED;
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd) {
    EnableDpiAwareness();

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    CoInitialize(NULL);

    g_app.pLogo = LoadBitmapFromResource(IDR_LOGO_PNG);

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    wc.lpszClassName = L"ZeroSnip_Installer_Class";
    RegisterClassExW(&wc);

    int winWidth = 620;
    int winHeight = 520;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenW - winWidth) / 2;
    int posY = (screenH - winHeight) / 2;

    HWND hWnd = CreateWindowExW(
        0, wc.lpszClassName, L"Zero Snip Setup",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        posX, posY, winWidth, winHeight,
        NULL, NULL, hInstance, NULL
    );

    ShowWindow(hWnd, SW_SHOWNORMAL);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_app.pLogo) {
        delete g_app.pLogo;
        g_app.pLogo = nullptr;
    }

    CoUninitialize();
    Gdiplus::GdiplusShutdown(gdiplusToken);
    return (int)msg.wParam;
}
