#include "SnipperOverlay.h"
#include <windowsx.h>
#include <algorithm>
#include <cwchar>

static const WCHAR OVERLAY_CLASS_NAME[] = L"KsnipLite_OverlayClass";

SnipperOverlay::SnipperOverlay() {
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = SnipperOverlay::WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_CROSS);
    wc.lpszClassName = OVERLAY_CLASS_NAME;
    RegisterClassExW(&wc);
}

SnipperOverlay::~SnipperOverlay() {
    CleanupScreen();
    if (m_hWnd && IsWindow(m_hWnd)) {
        DestroyWindow(m_hWnd);
        m_hWnd = NULL;
    }
}

struct MonitorEnumCtx {
    HDC hdcMem;
    int vx;
    int vy;
    int count;
};

static BOOL CALLBACK CaptureMonitorCallback(HMONITOR hMon, HDC hdcMon, LPRECT lprcMon, LPARAM lParam) {
    MonitorEnumCtx* pCtx = (MonitorEnumCtx*)lParam;
    int monW = lprcMon->right - lprcMon->left;
    int monH = lprcMon->bottom - lprcMon->top;
    int destX = lprcMon->left - pCtx->vx;
    int destY = lprcMon->top - pCtx->vy;

    MONITORINFOEXW mi;
    ZeroMemory(&mi, sizeof(mi));
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(hMon, &mi)) {
        HDC hdcDisplay = CreateDCW(NULL, mi.szDevice, NULL, NULL);
        if (hdcDisplay) {
            BitBlt(pCtx->hdcMem, destX, destY, monW, monH, hdcDisplay, 0, 0, SRCCOPY | CAPTUREBLT);
            DeleteDC(hdcDisplay);
            pCtx->count++;
            return TRUE;
        }
    }

    HDC hdcScreen = GetDC(NULL);
    BitBlt(pCtx->hdcMem, destX, destY, monW, monH, hdcScreen, lprcMon->left, lprcMon->top, SRCCOPY | CAPTUREBLT);
    ReleaseDC(NULL, hdcScreen);
    pCtx->count++;
    return TRUE;
}

void SnipperOverlay::FreezeScreen() {
    CleanupScreen();

    m_vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    m_vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    m_vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    m_vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HDC hdcScreen = GetDC(NULL);
    m_hdcScreenMem = CreateCompatibleDC(hdcScreen);
    m_hbmScreen = CreateCompatibleBitmap(hdcScreen, m_vw, m_vh);
    SelectObject(m_hdcScreenMem, m_hbmScreen);

    // Capture each monitor device directly to handle hardware acceleration and protected desktop surfaces
    MonitorEnumCtx ctx = { m_hdcScreenMem, m_vx, m_vy, 0 };
    EnumDisplayMonitors(NULL, NULL, CaptureMonitorCallback, (LPARAM)&ctx);

    if (ctx.count == 0) {
        // Fallback to virtual screen capture with CAPTUREBLT
        BitBlt(m_hdcScreenMem, 0, 0, m_vw, m_vh, hdcScreen, m_vx, m_vy, SRCCOPY | CAPTUREBLT);
    }
    ReleaseDC(NULL, hdcScreen);
}

void SnipperOverlay::CleanupScreen() {
    if (m_hdcScreenMem) {
        DeleteDC(m_hdcScreenMem);
        m_hdcScreenMem = NULL;
    }
    if (m_hbmScreen) {
        DeleteObject(m_hbmScreen);
        m_hbmScreen = NULL;
    }
}

bool SnipperOverlay::StartCapture(OnCaptureCallback onCapture, OnCancelCallback onCancel) {
    m_onCapture = onCapture;
    m_onCancel = onCancel;
    m_bDragging = false;

    // Small delay to ensure any existing popup closes before screen capture
    Sleep(120);
    FreezeScreen();

    if (!m_hWnd) {
        m_hWnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            OVERLAY_CLASS_NAME,
            L"Zero Snip Overlay",
            WS_POPUP,
            m_vx, m_vy, m_vw, m_vh,
            NULL, NULL, GetModuleHandle(NULL), this
        );
    } else {
        SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG_PTR)this);
        SetWindowPos(m_hWnd, HWND_TOPMOST, m_vx, m_vy, m_vw, m_vh, SWP_SHOWWINDOW);
    }

    if (!m_hWnd) {
        CleanupScreen();
        return false;
    }

    SetWindowPos(m_hWnd, HWND_TOPMOST, m_vx, m_vy, m_vw, m_vh, SWP_SHOWWINDOW);
    ShowWindow(m_hWnd, SW_SHOW);
    BringWindowToTop(m_hWnd);
    SetForegroundWindow(m_hWnd);
    SetFocus(m_hWnd);
    UpdateWindow(m_hWnd);
    return true;
}

void SnipperOverlay::Cancel() {
    if (m_hWnd && IsWindow(m_hWnd)) {
        ShowWindow(m_hWnd, SW_HIDE);
    }
    CleanupScreen();
    if (m_onCancel) {
        m_onCancel();
    }
}

void SnipperOverlay::FinishCapture() {
    int cropX = std::min(m_ptStart.x, m_ptCurrent.x);
    int cropY = std::min(m_ptStart.y, m_ptCurrent.y);
    int cropW = std::abs(m_ptCurrent.x - m_ptStart.x);
    int cropH = std::abs(m_ptCurrent.y - m_ptStart.y);

    // Clamp inside virtual screen
    cropX = std::max(0, std::min(cropX, m_vw - 1));
    cropY = std::max(0, std::min(cropY, m_vh - 1));
    cropW = std::min(cropW, m_vw - cropX);
    cropH = std::min(cropH, m_vh - cropY);

    if (cropW < 5 || cropH < 5) {
        // Accidental click without drag - cancel
        Cancel();
        return;
    }

    // Hide overlay before invoking callbacks
    if (m_hWnd && IsWindow(m_hWnd)) {
        ShowWindow(m_hWnd, SW_HIDE);
    }

    // Crop from captured frozen screen
    Gdiplus::Bitmap* pCropped = new Gdiplus::Bitmap(cropW, cropH, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics gCrop(pCropped);
        HDC hdcCrop = gCrop.GetHDC();
        BitBlt(hdcCrop, 0, 0, cropW, cropH, m_hdcScreenMem, cropX, cropY, SRCCOPY);
        gCrop.ReleaseHDC(hdcCrop);
    }

    CleanupScreen();

    if (m_onCapture) {
        m_onCapture(pCropped);
    }
}

LRESULT CALLBACK SnipperOverlay::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    SnipperOverlay* pThis = nullptr;
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCTW* pCS = (CREATESTRUCTW*)lParam;
        pThis = (SnipperOverlay*)pCS->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (SnipperOverlay*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->HandleMessage(hWnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

LRESULT SnipperOverlay::HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_SETCURSOR:
        SetCursor(LoadCursor(NULL, IDC_CROSS));
        return TRUE;

    case WM_LBUTTONDOWN: {
        m_bDragging = true;
        m_ptStart.x = GET_X_LPARAM(lParam);
        m_ptStart.y = GET_Y_LPARAM(lParam);
        m_ptCurrent = m_ptStart;
        SetCapture(hWnd);
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (m_bDragging) {
            m_ptCurrent.x = GET_X_LPARAM(lParam);
            m_ptCurrent.y = GET_Y_LPARAM(lParam);
            InvalidateRect(hWnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (m_bDragging) {
            m_bDragging = false;
            ReleaseCapture();
            m_ptCurrent.x = GET_X_LPARAM(lParam);
            m_ptCurrent.y = GET_Y_LPARAM(lParam);
            FinishCapture();
        }
        return 0;
    }

    case WM_RBUTTONDOWN:
    case WM_KEYDOWN: {
        if (uMsg == WM_RBUTTONDOWN || wParam == VK_ESCAPE) {
            if (m_bDragging) {
                m_bDragging = false;
                ReleaseCapture();
            }
            Cancel();
            return 0;
        }
        break;
    }

    case WM_ERASEBKGND:
        return 1; // Handled in WM_PAINT

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdcWindow = BeginPaint(hWnd, &ps);

        // Double buffer
        HDC hdcBuffer = CreateCompatibleDC(hdcWindow);
        HBITMAP hbmBuffer = CreateCompatibleBitmap(hdcWindow, m_vw, m_vh);
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcBuffer, hbmBuffer);

        // Draw background snapshot
        if (m_hdcScreenMem) {
            BitBlt(hdcBuffer, 0, 0, m_vw, m_vh, m_hdcScreenMem, 0, 0, SRCCOPY);
        }

        // Apply dark overlay with cutout
        {
            Gdiplus::Graphics g(hdcBuffer);
            g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
            g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

            if (m_bDragging) {
                int selX = std::min(m_ptStart.x, m_ptCurrent.x);
                int selY = std::min(m_ptStart.y, m_ptCurrent.y);
                int selW = std::abs(m_ptCurrent.x - m_ptStart.x);
                int selH = std::abs(m_ptCurrent.y - m_ptStart.y);

                Gdiplus::Region dimRegion(Gdiplus::Rect(0, 0, m_vw, m_vh));
                if (selW > 0 && selH > 0) {
                    Gdiplus::Rect selRect(selX, selY, selW, selH);
                    dimRegion.Exclude(selRect);
                }
                g.SetClip(&dimRegion);
                Gdiplus::SolidBrush dimBrush(Gdiplus::Color(125, 0, 0, 0)); // Clean dark shade
                g.FillRectangle(&dimBrush, 0, 0, m_vw, m_vh);
                g.ResetClip();

                if (selW > 0 && selH > 0) {
                    // Crisp accent border (Snipping Tool / ksnip cyan-blue)
                    Gdiplus::Pen borderPen(Gdiplus::Color(255, 0, 120, 215), 2.0f);
                    g.DrawRectangle(&borderPen, selX, selY, selW, selH);

                    // Dimensions badge
                    WCHAR dimText[64];
                    swprintf_s(dimText, L"%d \u00D7 %d px", selW, selH);
                    Gdiplus::Font font(L"Segoe UI", 9.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
                    Gdiplus::PointF origin((Gdiplus::REAL)selX + 4, (Gdiplus::REAL)selY + selH + 6);
                    if (origin.Y + 24 > m_vh) {
                        origin.Y = (Gdiplus::REAL)selY - 26;
                    }
                    if (origin.X + 110 > m_vw) {
                        origin.X = (Gdiplus::REAL)m_vw - 114;
                    }

                    Gdiplus::RectF textBounds;
                    g.MeasureString(dimText, -1, &font, origin, &textBounds);
                    textBounds.Inflate(6.0f, 3.0f);

                    Gdiplus::SolidBrush badgeBg(Gdiplus::Color(230, 24, 24, 27));
                    g.FillRectangle(&badgeBg, textBounds);
                    Gdiplus::Pen badgeBorder(Gdiplus::Color(255, 70, 70, 75), 1.0f);
                    g.DrawRectangle(&badgeBorder, textBounds.X, textBounds.Y, textBounds.Width, textBounds.Height);

                    Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 255, 255, 255));
                    g.DrawString(dimText, -1, &font, origin, &textBrush);
                }
            } else {
                // Not dragging yet - dim entire screen
                Gdiplus::SolidBrush dimBrush(Gdiplus::Color(110, 0, 0, 0));
                g.FillRectangle(&dimBrush, 0, 0, m_vw, m_vh);

                // Top instruction banner
                const WCHAR* hint = L"Drag to capture area \u2022 ESC to cancel";
                Gdiplus::Font hintFont(L"Segoe UI", 11.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
                Gdiplus::PointF centerOrigin((Gdiplus::REAL)(m_vw / 2), 24.0f);
                Gdiplus::StringFormat sf;
                sf.SetAlignment(Gdiplus::StringAlignmentCenter);

                Gdiplus::RectF hintBounds;
                g.MeasureString(hint, -1, &hintFont, centerOrigin, &sf, &hintBounds);
                hintBounds.Inflate(16.0f, 6.0f);

                Gdiplus::SolidBrush bannerBg(Gdiplus::Color(220, 20, 20, 24));
                g.FillRectangle(&bannerBg, hintBounds);
                Gdiplus::Pen bannerBorder(Gdiplus::Color(255, 60, 60, 65), 1.0f);
                g.DrawRectangle(&bannerBorder, hintBounds.X, hintBounds.Y, hintBounds.Width, hintBounds.Height);

                Gdiplus::SolidBrush bannerText(Gdiplus::Color(255, 240, 240, 240));
                g.DrawString(hint, -1, &hintFont, centerOrigin, &sf, &bannerText);
            }
        }

        // Copy buffer to window
        BitBlt(hdcWindow, 0, 0, m_vw, m_vh, hdcBuffer, 0, 0, SRCCOPY);

        SelectObject(hdcBuffer, hbmOld);
        DeleteObject(hbmBuffer);
        DeleteDC(hdcBuffer);

        EndPaint(hWnd, &ps);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}
