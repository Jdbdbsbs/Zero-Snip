#pragma once
#include <windows.h>
#include <shellapi.h>
#include <functional>

#define WM_TRAYICON (WM_USER + 101)

class HotKeyManager {
public:
    using OnTriggerCallback = std::function<void()>;
    using OnExitCallback = std::function<void()>;

    HotKeyManager();
    ~HotKeyManager();

    bool Initialize(HWND hWndMessageReceiver, 
                    OnTriggerCallback onTrigger, 
                    OnExitCallback onExit);
    void Cleanup();

    bool HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    HWND m_hWnd = NULL;
    NOTIFYICONDATAW m_nid = { 0 };
    bool m_bTrayAdded = false;

    OnTriggerCallback m_onTrigger = nullptr;
    OnExitCallback m_onExit = nullptr;

    enum HotKeyId {
        HK_F14 = 1001,
        HK_CTRL_SHIFT_S = 1002,
        HK_PRINTSCREEN = 1003
    };

    enum MenuItemId {
        IDM_CAPTURE = 2001,
        IDM_ABOUT = 2002,
        IDM_RESTART_ADMIN = 2004,
        IDM_TOGGLE_AUTOSTART = 2005,
        IDM_EXIT = 2003
    };

    void ShowTrayMenu();
};
