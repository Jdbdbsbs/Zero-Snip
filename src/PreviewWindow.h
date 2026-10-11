#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <functional>

class PreviewWindow {
public:
    using OnNewCaptureRequest = std::function<void()>;

    PreviewWindow();
    ~PreviewWindow();

    // Shows the preview GUI for the newly captured bitmap
    void ShowPreview(Gdiplus::Bitmap* pCapturedBitmap, OnNewCaptureRequest onNewCapture = nullptr);

    // Closes preview window if open
    void Close();

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    HWND m_hWnd = NULL;
    Gdiplus::Bitmap* m_pBitmap = nullptr;
    OnNewCaptureRequest m_onNewCapture = nullptr;

    std::wstring m_statusText;
    bool m_statusIsSuccess = true;

    // Button identifiers and rects
    enum ButtonId {
        BTN_NONE = 0,
        BTN_SAVE_THIS,
        BTN_COPY,
        BTN_NEW_SNIP,
        BTN_CLOSE
    };

    ButtonId m_hoveredBtn = BTN_NONE;
    ButtonId m_pressedBtn = BTN_NONE;

    RECT m_rcSave = { 0 };
    RECT m_rcCopy = { 0 };
    RECT m_rcNew = { 0 };
    RECT m_rcClose = { 0 };

    void UpdateLayout(int width, int height);
    void DrawCustomButton(Gdiplus::Graphics& g, const RECT& rc, const WCHAR* label, 
                          bool isPrimary, bool isHovered, bool isPressed);
    void OnSaveThisClicked();
    void OnCopyClicked();
    void OnNewSnipClicked();
};
