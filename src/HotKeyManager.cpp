#include "HotKeyManager.h"
#include <cwchar>

HotKeyManager::HotKeyManager() {
}

HotKeyManager::~HotKeyManager() {
    Cleanup();
}

static HICON CreateCustomTrayIcon() {
    // Dynamically create a clean, crisp 16x16 snip icon (crosshair / crop mark)
    int cx = 16;
    int cy = 16;
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbmColor = CreateCompatibleBitmap(hdcScreen, cx, cy);
    HBITMAP hbmMask = CreateBitmap(cx, cy, 1, 1, NULL);

    HBITMAP hOldColor = (HBITMAP)SelectObject(hdcMem, hbmColor);

    // Dark background #1e1e24
    HBRUSH hBg = CreateSolidBrush(RGB(30, 30, 36));
    RECT rc = { 0, 0, cx, cy };
    FillRect(hdcMem, &rc, hBg);
    DeleteObject(hBg);

    // Cyan crosshair/crop lines
    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 150, 255));
    HPEN hOldPen = (HPEN)SelectObject(hdcMem, hPen);

    // Crop box corners
    MoveToEx(hdcMem, 3, 7, NULL); LineTo(hdcMem, 3, 3); LineTo(hdcMem, 7, 3);
    MoveToEx(hdcMem, 9, 3, NULL); LineTo(hdcMem, 13, 3); LineTo(hdcMem, 13, 7);
    MoveToEx(hdcMem, 13, 9, NULL); LineTo(hdcMem, 13, 13); LineTo(hdcMem, 9, 13);
    MoveToEx(hdcMem, 7, 13, NULL); LineTo(hdcMem, 3, 13); LineTo(hdcMem, 3, 9);

    SelectObject(hdcMem, hOldPen);
    DeleteObject(hPen);
    SelectObject(hdcMem, hOldColor);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    ICONINFO ii = { 0 };
    ii.fIcon = TRUE;
    ii.hbmColor = hbmColor;
    ii.hbmMask = hbmMask;
    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hbmColor);
    DeleteObject(hbmMask);
    return hIcon;
}

bool HotKeyManager::Initialize(HWND hWndMessageReceiver, 
                               OnTriggerCallback onTrigger, 
                               OnExitCallback onExit) {
    m_hWnd = hWndMessageReceiver;
    m_onTrigger = onTrigger;
    m_onExit = onExit;

    auto safeRegister = [this](int id, UINT mod, UINT vk) {
        for (int i = 0; i < 5; ++i) {
            if (RegisterHotKey(m_hWnd, id, mod, vk)) return true;
            Sleep(80);
        }
        return false;
    };

    // Register F14 (0x7D)
    safeRegister(HK_F14, 0, 0x7D);

    // Register Ctrl + Shift + S
    safeRegister(HK_CTRL_SHIFT_S, MOD_CONTROL | MOD_SHIFT, 'S');

    // Register PrintScreen (VK_SNAPSHOT)
    safeRegister(HK_PRINTSCREEN, 0, VK_SNAPSHOT);

    // Initialize System Tray Icon with embedded Zero Snip logo
    ZeroMemory(&m_nid, sizeof(m_nid));
    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd = m_hWnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    if (!m_nid.hIcon) {
        m_nid.hIcon = CreateCustomTrayIcon();
    }
    wcscpy_s(m_nid.szTip, L"Zero Snip \u2014 Press F14 or Ctrl+Shift+S to capture");

    m_bTrayAdded = Shell_NotifyIconW(NIM_ADD, &m_nid);
    return true;
}

void HotKeyManager::Cleanup() {
    if (m_hWnd) {
        UnregisterHotKey(m_hWnd, HK_F14);
        UnregisterHotKey(m_hWnd, HK_CTRL_SHIFT_S);
        UnregisterHotKey(m_hWnd, HK_PRINTSCREEN);
    }
    if (m_bTrayAdded) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_bTrayAdded = false;
    }
    if (m_nid.hIcon) {
        DestroyIcon(m_nid.hIcon);
        m_nid.hIcon = NULL;
    }
}

#include "PrivilegeHelper.h"
#include "StartupHelper.h"

void HotKeyManager::ShowTrayMenu() {
    HMENU hMenu = CreatePopupMenu();
    InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, IDM_CAPTURE, L"&Capture Area (F14 / Ctrl+Shift+S)");
    InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

    if (PrivilegeHelper::IsProcessElevated()) {
        InsertMenuW(hMenu, 2, MF_BYPOSITION | MF_STRING | MF_GRAYED, 0, L"[\u2713] Administrator Mode (Captures All Windows)");
    } else {
        InsertMenuW(hMenu, 2, MF_BYPOSITION | MF_STRING, IDM_RESTART_ADMIN, L"[\U0001F6E1] Restart as Administrator (Capture Protected Apps)");
    }

    bool bStartup = StartupHelper::IsStartupEnabled();
    std::wstring startupText = bStartup ? L"[\u2713] Start with Windows (Autostart)" : L"[   ] Start with Windows (Autostart)";
    InsertMenuW(hMenu, 3, MF_BYPOSITION | MF_STRING, IDM_TOGGLE_AUTOSTART, startupText.c_str());

    InsertMenuW(hMenu, 4, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    InsertMenuW(hMenu, 5, MF_BYPOSITION | MF_STRING, IDM_ABOUT, L"&About Zero Snip");
    InsertMenuW(hMenu, 6, MF_BYPOSITION | MF_STRING, IDM_EXIT, L"E&xit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(m_hWnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, m_hWnd, NULL);
    PostMessage(m_hWnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

bool HotKeyManager::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_HOTKEY) {
        if (wParam == HK_F14 || wParam == HK_CTRL_SHIFT_S || wParam == HK_PRINTSCREEN) {
            if (m_onTrigger) {
                m_onTrigger();
            }
            return true;
        }
    } else if (uMsg == WM_TRAYICON) {
        if (lParam == WM_RBUTTONUP) {
            ShowTrayMenu();
            return true;
        } else if (lParam == WM_LBUTTONUP) {
            if (m_onTrigger) {
                m_onTrigger();
            }
            return true;
        }
    } else if (uMsg == WM_COMMAND) {
        WORD cmd = LOWORD(wParam);
        if (cmd == IDM_CAPTURE) {
            if (m_onTrigger) {
                m_onTrigger();
            }
            return true;
        } else if (cmd == IDM_ABOUT) {
            MessageBoxW(m_hWnd,
                L"Zero Snip v1.0.0\n\n"
                L"Ultra-lightweight drag & capture tool.\n\n"
                L"\u2022 Press F14 or Ctrl+Shift+S (or PrintScreen) to capture.\n"
                L"\u2022 Drag to select, release to capture.\n"
                L"\u2022 Automatically copied to clipboard for immediate paste.\n"
                L"\u2022 Zero disk clutter: Never saved unless you click 'Save this'.\n"
                L"\u2022 Seamless capture over administrator and protected windows.",
                L"About Zero Snip", MB_OK | MB_ICONINFORMATION);
            return true;
        } else if (cmd == IDM_TOGGLE_AUTOSTART) {
            bool isEnabled = StartupHelper::IsStartupEnabled();
            if (isEnabled) {
                StartupHelper::DisableStartup();
            } else {
                StartupHelper::EnableStartup();
            }
            return true;
        } else if (cmd == IDM_RESTART_ADMIN) {
            if (PrivilegeHelper::RelaunchElevated()) {
                PostQuitMessage(0);
            }
            return true;
        } else if (cmd == IDM_EXIT) {
            if (m_onExit) {
                m_onExit();
            } else {
                PostQuitMessage(0);
            }
            return true;
        }
    }
    return false;
}
