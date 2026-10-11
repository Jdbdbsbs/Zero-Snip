#include <windows.h>
#include <gdiplus.h>
#include "SnipperOverlay.h"
#include "PreviewWindow.h"
#include "ClipboardHelper.h"
#include "HotKeyManager.h"

// Set High-DPI Awareness
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

static const WCHAR MAIN_CLASS_NAME[] = L"ZeroSnip_MainReceiverClass";
static HotKeyManager* g_pHotKeyManager = nullptr;
static SnipperOverlay* g_pSnipper = nullptr;
static PreviewWindow* g_pPreview = nullptr;
static HWND g_hMainWnd = NULL;

static void TriggerCapture() {
    if (!g_pSnipper || !g_pPreview) return;

    // Close preview if already open
    g_pPreview->Close();

    g_pSnipper->StartCapture(
        [](Gdiplus::Bitmap* pCapturedBitmap) {
            // 1. Immediately copy to clipboard
            ClipboardHelper::CopyToClipboard(g_hMainWnd, pCapturedBitmap);

            // 2. Do NOT save to disk!

            // 3. Show preview GUI (gives user option to click "Save this")
            g_pPreview->ShowPreview(pCapturedBitmap, []() {
                TriggerCapture();
            });
        },
        []() {
            // Cancelled
        }
    );
}

static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (g_pHotKeyManager && g_pHotKeyManager->HandleMessage(uMsg, wParam, lParam)) {
        return 0;
    }
    if (uMsg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

#include "PrivilegeHelper.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd) {
    bool bSilent = (lpCmdLine && (wcsstr(lpCmdLine, L"--silent") || wcsstr(lpCmdLine, L"-s") || wcsstr(lpCmdLine, L"--autostart")));

    // Prevent multiple instances
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"Global\\ZeroSnip_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (!bSilent) {
            MessageBoxW(NULL, 
                L"Zero Snip is already running in the background.\n\n"
                L"Press F14 or Ctrl+Shift+S (or PrintScreen) to capture anytime.", 
                L"Zero Snip", MB_OK | MB_ICONINFORMATION);
        }
        return 0;
    }

    EnableDpiAwareness();
    PrivilegeHelper::EnableDebugPrivilege();

    // Initialize GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Register Main Message Receiver Window
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    wc.lpszClassName = MAIN_CLASS_NAME;
    RegisterClassExW(&wc);

    g_hMainWnd = CreateWindowExW(
        0, MAIN_CLASS_NAME, L"Zero Snip Background Service",
        WS_POPUP, 0, 0, 0, 0,
        NULL, NULL, hInstance, NULL
    );
    // Explicitly consume initial STARTUPINFO show command so it never overrides subsequent ShowWindow calls
    ShowWindow(g_hMainWnd, SW_HIDE);

    SnipperOverlay snipper;
    PreviewWindow preview;
    HotKeyManager hotkeyMgr;

    g_pSnipper = &snipper;
    g_pPreview = &preview;
    g_pHotKeyManager = &hotkeyMgr;

    hotkeyMgr.Initialize(g_hMainWnd, 
        []() {
            TriggerCapture();
        },
        []() {
            PostQuitMessage(0);
        }
    );

    // If launched with "--capture" or "-c" flag, trigger immediately
    if (lpCmdLine && (wcsstr(lpCmdLine, L"--capture") || wcsstr(lpCmdLine, L"-c"))) {
        TriggerCapture();
    }

    // Main Message Loop - Intercept WM_HOTKEY directly to guarantee instant response
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (msg.message == WM_HOTKEY) {
            if (g_pHotKeyManager && g_pHotKeyManager->HandleMessage(msg.message, msg.wParam, msg.lParam)) {
                continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    hotkeyMgr.Cleanup();
    Gdiplus::GdiplusShutdown(gdiplusToken);

    if (hMutex) {
        CloseHandle(hMutex);
    }

    return (int)msg.wParam;
}
