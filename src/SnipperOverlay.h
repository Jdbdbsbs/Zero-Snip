#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <functional>

class SnipperOverlay {
public:
    using OnCaptureCallback = std::function<void(Gdiplus::Bitmap* pCapturedBitmap)>;
    using OnCancelCallback = std::function<void()>;

    SnipperOverlay();
    ~SnipperOverlay();

    // Starts the snipping process: freezes screen, shows overlay, calls callbacks on completion
    bool StartCapture(OnCaptureCallback onCapture, OnCancelCallback onCancel = nullptr);

    // Cancel current capture if active
    void Cancel();

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    HWND m_hWnd = NULL;
    HBITMAP m_hbmScreen = NULL;
    HDC m_hdcScreenMem = NULL;
    int m_vx = 0;
    int m_vy = 0;
    int m_vw = 0;
    int m_vh = 0;

    bool m_bDragging = false;
    POINT m_ptStart = { 0, 0 };
    POINT m_ptCurrent = { 0, 0 };

    OnCaptureCallback m_onCapture = nullptr;
    OnCancelCallback m_onCancel = nullptr;

    void FreezeScreen();
    void CleanupScreen();
    void FinishCapture();
};
