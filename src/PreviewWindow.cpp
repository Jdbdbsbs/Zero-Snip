#include "PreviewWindow.h"
#include "ImageSaver.h"
#include "ClipboardHelper.h"
#include <windowsx.h>
#include <dwmapi.h>
#include <algorithm>
#include <cwchar>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1
#define DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 19
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif

static void ApplyDarkTitleBar(HWND hWnd) {
    if (!hWnd) return;
    BOOL value = TRUE;
    // Windows 10 build 19041+ (including 19045) & Windows 11
    DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
    // Fallback for earlier Windows 10 builds
    DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1, &value, sizeof(value));

    // Windows 11 custom dark caption color (#0d0d11 matching our app)
    COLORREF captionColor = RGB(13, 13, 17);
    DwmSetWindowAttribute(hWnd, DWMWA_CAPTION_COLOR, &captionColor, sizeof(captionColor));

    COLORREF textColor = RGB(245, 245, 245);
    DwmSetWindowAttribute(hWnd, DWMWA_TEXT_COLOR, &textColor, sizeof(textColor));
}

static const WCHAR PREVIEW_CLASS_NAME[] = L"ZeroSnip_PreviewClass";

// Helper: Add rounded rectangle to GraphicsPath
static void AddRoundedRect(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& rect, float radius) {
    float diameter = radius * 2.0f;
    if (diameter > rect.Width) diameter = rect.Width;
    if (diameter > rect.Height) diameter = rect.Height;

    float r = rect.X + rect.Width;
    float b = rect.Y + rect.Height;

    path.AddArc(rect.X, rect.Y, diameter, diameter, 180.0f, 90.0f);
    path.AddArc(r - diameter, rect.Y, diameter, diameter, 270.0f, 90.0f);
    path.AddArc(r - diameter, b - diameter, diameter, diameter, 0.0f, 90.0f);
    path.AddArc(rect.X, b - diameter, diameter, diameter, 90.0f, 90.0f);
    path.CloseFigure();
}

// Helper: Draw soft multi-layer drop shadow behind a rounded rectangle
static void DrawRoundedDropShadow(Gdiplus::Graphics& g, const Gdiplus::RectF& rect, float radius, int passes = 4, int maxAlpha = 28) {
    for (int i = passes; i >= 1; --i) {
        Gdiplus::RectF sRect = rect;
        sRect.Offset(0.0f, (float)i * 1.0f); // slight downward offset
        sRect.Inflate((float)i * 0.45f, (float)i * 0.45f);

        int alpha = (int)((float)maxAlpha * ((float)(passes - i + 1) / (float)passes));
        if (alpha > 255) alpha = 255;

        Gdiplus::GraphicsPath shadowPath;
        AddRoundedRect(shadowPath, sRect, radius + (float)i * 0.4f);
        Gdiplus::SolidBrush shadowBrush(Gdiplus::Color((BYTE)alpha, 0, 0, 0));
        g.FillPath(&shadowBrush, &shadowPath);
    }
}

PreviewWindow::PreviewWindow() {
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = PreviewWindow::WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = PREVIEW_CLASS_NAME;
    RegisterClassExW(&wc);
}

PreviewWindow::~PreviewWindow() {
    if (m_pBitmap) {
        delete m_pBitmap;
        m_pBitmap = nullptr;
    }
    if (m_hWnd && IsWindow(m_hWnd)) {
        DestroyWindow(m_hWnd);
        m_hWnd = NULL;
    }
}

void PreviewWindow::Close() {
    if (m_hWnd && IsWindow(m_hWnd)) {
        ShowWindow(m_hWnd, SW_HIDE);
    }
}

void PreviewWindow::ShowPreview(Gdiplus::Bitmap* pCapturedBitmap, OnNewCaptureRequest onNewCapture) {
    if (m_pBitmap) {
        delete m_pBitmap;
    }
    m_pBitmap = pCapturedBitmap;
    m_onNewCapture = onNewCapture;
    m_statusText = L"\u2713 Copied to clipboard";
    m_statusIsSuccess = true;

    if (!m_pBitmap) return;

    int imgW = (int)m_pBitmap->GetWidth();
    int imgH = (int)m_pBitmap->GetHeight();

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    int chromeH = 74 + 36; // Header (74) + Footer (36)
    int targetW = imgW + 64;
    int targetH = imgH + chromeH + 64;

    int minW = 680;
    int minH = 440;
    int maxW = (int)(screenW * 0.85);
    int maxH = (int)(screenH * 0.85);

    int winW = std::max(minW, std::min(targetW, maxW));
    int winH = std::max(minH, std::min(targetH, maxH));

    int posX = (screenW - winW) / 2;
    int posY = (screenH - winH) / 2;

    if (!m_hWnd) {
        m_hWnd = CreateWindowExW(
            WS_EX_APPWINDOW,
            PREVIEW_CLASS_NAME,
            L"Zero Snip \u2014 Preview",
            WS_OVERLAPPEDWINDOW,
            posX, posY, winW, winH,
            NULL, NULL, GetModuleHandle(NULL), this
        );
        SendMessageW(m_hWnd, WM_SETICON, ICON_BIG, (LPARAM)LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1)));
        SendMessageW(m_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
        ApplyDarkTitleBar(m_hWnd);
    } else {
        SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG_PTR)this);
        SetWindowPos(m_hWnd, HWND_TOP, posX, posY, winW, winH, SWP_SHOWWINDOW);
        ApplyDarkTitleBar(m_hWnd);
    }

    if (m_hWnd) {
        // Compute layout IMMEDIATELY so buttons are positioned on initial frame
        RECT clientRc = { 0 };
        GetClientRect(m_hWnd, &clientRc);
        UpdateLayout(clientRc.right - clientRc.left, clientRc.bottom - clientRc.top);

        SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        SetForegroundWindow(m_hWnd);
        SetFocus(m_hWnd);
        SetWindowPos(m_hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        ShowWindow(m_hWnd, SW_SHOW);
        UpdateWindow(m_hWnd);
    }
}

void PreviewWindow::UpdateLayout(int width, int height) {
    if (width <= 0 || height <= 0) return;

    int btnH = 34;
    int btnY = 22; // centered vertically in the 70px header
    int margin = 18;

    int curX = width - margin;

    // Close button
    int wClose = 72;
    m_rcClose = { curX - wClose, btnY, curX, btnY + btnH };
    curX -= (wClose + 8);

    // New Snip button
    int wNew = 90;
    m_rcNew = { curX - wNew, btnY, curX, btnY + btnH };
    curX -= (wNew + 8);

    // Copy button
    int wCopy = 74;
    m_rcCopy = { curX - wCopy, btnY, curX, btnY + btnH };
    curX -= (wCopy + 8);

    // Save This button (Primary Accent)
    int wSave = 100;
    m_rcSave = { curX - wSave, btnY, curX, btnY + btnH };
}

void PreviewWindow::DrawCustomButton(Gdiplus::Graphics& g, const RECT& rc, const WCHAR* label,
                                     bool isPrimary, bool isHovered, bool isPressed) {
    if (rc.right <= rc.left || rc.bottom <= rc.top) return;

    float radius = 8.0f; // Rounded rectangle radius
    Gdiplus::RectF btnRect((Gdiplus::REAL)rc.left, (Gdiplus::REAL)rc.top, 
                          (Gdiplus::REAL)(rc.right - rc.left), (Gdiplus::REAL)(rc.bottom - rc.top));

    // 1. Draw Drop Shadow behind button
    if (!isPressed) {
        DrawRoundedDropShadow(g, btnRect, radius, 4, isPrimary ? 36 : 24);
    } else {
        btnRect.Offset(0.0f, 1.0f); // subtle push down on press
    }

    // 2. Button colors matching reference image palette
    Gdiplus::Color bgColor;
    Gdiplus::Color borderColor;
    Gdiplus::Color textColor(255, 255, 255, 255);

    if (isPrimary) {
        if (isPressed) {
            bgColor = Gdiplus::Color(255, 29, 78, 216);    // #1d4ed8
            borderColor = Gdiplus::Color(255, 30, 64, 175);
        } else if (isHovered) {
            bgColor = Gdiplus::Color(255, 59, 130, 246);   // #3b82f6
            borderColor = Gdiplus::Color(255, 96, 165, 250);
        } else {
            bgColor = Gdiplus::Color(255, 37, 99, 235);    // #2563eb
            borderColor = Gdiplus::Color(255, 59, 130, 246);
        }
    } else {
        if (isPressed) {
            bgColor = Gdiplus::Color(255, 24, 24, 29);     // #18181d
            borderColor = Gdiplus::Color(255, 50, 50, 60);
        } else if (isHovered) {
            bgColor = Gdiplus::Color(255, 42, 42, 52);     // #2a2a34
            borderColor = Gdiplus::Color(255, 75, 75, 90);
        } else {
            bgColor = Gdiplus::Color(255, 32, 32, 40);     // #202028 (dark card)
            borderColor = Gdiplus::Color(255, 54, 54, 66); // subtle border
        }
    }

    // 3. Fill Rounded Rectangle Button
    Gdiplus::GraphicsPath btnPath;
    AddRoundedRect(btnPath, btnRect, radius);

    Gdiplus::SolidBrush bgBrush(bgColor);
    g.FillPath(&bgBrush, &btnPath);

    Gdiplus::Pen borderPen(borderColor, 1.0f);
    g.DrawPath(&borderPen, &btnPath);

    // 4. Draw Label
    Gdiplus::Font font(L"Segoe UI", 9.2f, isPrimary ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::StringFormat sf;
    sf.SetAlignment(Gdiplus::StringAlignmentCenter);
    sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);

    Gdiplus::SolidBrush textBrush(textColor);
    g.DrawString(label, -1, &font, btnRect, &sf, &textBrush);
}

void PreviewWindow::OnSaveThisClicked() {
    if (!m_pBitmap) return;

    std::wstring savedPath;
    if (ImageSaver::PromptAndSave(m_hWnd, m_pBitmap, &savedPath)) {
        size_t slash = savedPath.find_last_of(L"\\/");
        std::wstring fileName = (slash != std::wstring::npos) ? savedPath.substr(slash + 1) : savedPath;

        m_statusText = L"\u2713 Saved: " + fileName;
        m_statusIsSuccess = true;
    }
    InvalidateRect(m_hWnd, NULL, FALSE);
}

void PreviewWindow::OnCopyClicked() {
    if (!m_pBitmap) return;
    if (ClipboardHelper::CopyToClipboard(m_hWnd, m_pBitmap)) {
        m_statusText = L"\u2713 Re-copied to clipboard!";
        m_statusIsSuccess = true;
    }
    InvalidateRect(m_hWnd, NULL, FALSE);
}

void PreviewWindow::OnNewSnipClicked() {
    Close();
    if (m_onNewCapture) {
        m_onNewCapture();
    }
}

LRESULT CALLBACK PreviewWindow::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    PreviewWindow* pThis = nullptr;
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCTW* pCS = (CREATESTRUCTW*)lParam;
        pThis = (PreviewWindow*)pCS->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (PreviewWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->HandleMessage(hWnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

LRESULT PreviewWindow::HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_SIZE: {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        UpdateLayout(w, h);
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;
    }

    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ButtonId oldHover = m_hoveredBtn;
        m_hoveredBtn = BTN_NONE;

        if (PtInRect(&m_rcSave, pt)) m_hoveredBtn = BTN_SAVE_THIS;
        else if (PtInRect(&m_rcCopy, pt)) m_hoveredBtn = BTN_COPY;
        else if (PtInRect(&m_rcNew, pt)) m_hoveredBtn = BTN_NEW_SNIP;
        else if (PtInRect(&m_rcClose, pt)) m_hoveredBtn = BTN_CLOSE;

        if (oldHover != m_hoveredBtn) {
            InvalidateRect(hWnd, NULL, FALSE);
        }

        TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE: {
        if (m_hoveredBtn != BTN_NONE) {
            m_hoveredBtn = BTN_NONE;
            InvalidateRect(hWnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (PtInRect(&m_rcSave, pt)) m_pressedBtn = BTN_SAVE_THIS;
        else if (PtInRect(&m_rcCopy, pt)) m_pressedBtn = BTN_COPY;
        else if (PtInRect(&m_rcNew, pt)) m_pressedBtn = BTN_NEW_SNIP;
        else if (PtInRect(&m_rcClose, pt)) m_pressedBtn = BTN_CLOSE;
        else m_pressedBtn = BTN_NONE;

        if (m_pressedBtn != BTN_NONE) {
            SetCapture(hWnd);
            InvalidateRect(hWnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (m_pressedBtn != BTN_NONE) {
            ReleaseCapture();
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ButtonId clicked = BTN_NONE;
            if (PtInRect(&m_rcSave, pt) && m_pressedBtn == BTN_SAVE_THIS) clicked = BTN_SAVE_THIS;
            else if (PtInRect(&m_rcCopy, pt) && m_pressedBtn == BTN_COPY) clicked = BTN_COPY;
            else if (PtInRect(&m_rcNew, pt) && m_pressedBtn == BTN_NEW_SNIP) clicked = BTN_NEW_SNIP;
            else if (PtInRect(&m_rcClose, pt) && m_pressedBtn == BTN_CLOSE) clicked = BTN_CLOSE;

            m_pressedBtn = BTN_NONE;
            InvalidateRect(hWnd, NULL, FALSE);

            if (clicked == BTN_SAVE_THIS) OnSaveThisClicked();
            else if (clicked == BTN_COPY) OnCopyClicked();
            else if (clicked == BTN_NEW_SNIP) OnNewSnipClicked();
            else if (clicked == BTN_CLOSE) Close();
        }
        return 0;
    }

    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) {
            Close();
            return 0;
        }
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            if (wParam == 'S' || wParam == 's') {
                OnSaveThisClicked();
                return 0;
            }
            if (wParam == 'C' || wParam == 'c') {
                OnCopyClicked();
                return 0;
            }
            if (wParam == 'N' || wParam == 'n') {
                OnNewSnipClicked();
                return 0;
            }
        }
        break;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT clientRc;
        GetClientRect(hWnd, &clientRc);
        int clientW = clientRc.right - clientRc.left;
        int clientH = clientRc.bottom - clientRc.top;

        // GUARANTEE: update layout before any paint occurs
        UpdateLayout(clientW, clientH);

        // Double buffer
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, clientW, clientH);
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

        {
            Gdiplus::Graphics g(hdcMem);
            g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
            g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

            // 1. Overall Deep Matte Black Canvas (#0d0d11, matching reference image)
            Gdiplus::SolidBrush bgBrush(Gdiplus::Color(255, 13, 13, 17));
            g.FillRectangle(&bgBrush, 0, 0, clientW, clientH);

            int headerH = 74;
            int footerH = 36;

            // 2. Top Header Bar Area
            // Subtitle tag: ZERO SNIP // CAPTURE
            {
                Gdiplus::Font tagFont(L"Segoe UI", 7.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
                Gdiplus::SolidBrush tagBrush(Gdiplus::Color(255, 120, 120, 132));
                Gdiplus::PointF tagPt(22.0f, 15.0f);
                g.DrawString(L"ZERO SNIP  //  INSTANT CAPTURE", -1, &tagFont, tagPt, &tagBrush);
            }

            // Status Pill (Rounded rectangle badge with dynamic sizing and strict anti-collision)
            {
                Gdiplus::Font statusFont(L"Segoe UI", 8.8f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);

                // Dynamically measure text width
                Gdiplus::RectF measuredText;
                g.MeasureString(m_statusText.c_str(), -1, &statusFont, Gdiplus::PointF(0, 0), &measuredText);
                float pillWidth = measuredText.Width + 24.0f; // 12px padding per side

                // Strictly ensure pill NEVER overlaps the "Save this" button
                float maxPillRight = (float)m_rcSave.left - 20.0f;
                float pillStartX = 20.0f;
                if (pillStartX + pillWidth > maxPillRight) {
                    pillWidth = std::max(60.0f, maxPillRight - pillStartX);
                }

                Gdiplus::RectF pillRect(pillStartX, 32.0f, pillWidth, 26.0f);

                Gdiplus::GraphicsPath pillPath;
                AddRoundedRect(pillPath, pillRect, 6.0f);

                Gdiplus::Color pillBg = m_statusIsSuccess ? Gdiplus::Color(255, 18, 42, 28) : Gdiplus::Color(255, 36, 36, 44);
                Gdiplus::Color pillBorder = m_statusIsSuccess ? Gdiplus::Color(255, 34, 197, 94) : Gdiplus::Color(255, 70, 70, 80);
                Gdiplus::Color pillText = m_statusIsSuccess ? Gdiplus::Color(255, 74, 222, 128) : Gdiplus::Color(255, 230, 230, 230);

                Gdiplus::SolidBrush pillBgBrush(pillBg);
                g.FillPath(&pillBgBrush, &pillPath);

                Gdiplus::Pen pillBorderPen(pillBorder, 1.0f);
                g.DrawPath(&pillBorderPen, &pillPath);

                Gdiplus::SolidBrush textBrush(pillText);
                Gdiplus::StringFormat sf;
                sf.SetAlignment(Gdiplus::StringAlignmentCenter);
                sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
                g.DrawString(m_statusText.c_str(), -1, &statusFont, pillRect, &sf, &textBrush);
            }

            // Draw Toolbar Buttons (Rounded Rectangles with Drop Shadows)
            DrawCustomButton(g, m_rcSave, L"Save this", true, 
                             m_hoveredBtn == BTN_SAVE_THIS, m_pressedBtn == BTN_SAVE_THIS);
            DrawCustomButton(g, m_rcCopy, L"Copy", false, 
                             m_hoveredBtn == BTN_COPY, m_pressedBtn == BTN_COPY);
            DrawCustomButton(g, m_rcNew, L"New Snip", false, 
                             m_hoveredBtn == BTN_NEW_SNIP, m_pressedBtn == BTN_NEW_SNIP);
            DrawCustomButton(g, m_rcClose, L"Close", false, 
                             m_hoveredBtn == BTN_CLOSE, m_pressedBtn == BTN_CLOSE);

            // Subtle divider line under header
            Gdiplus::Pen dividerPen(Gdiplus::Color(255, 30, 30, 38), 1.0f);
            g.DrawLine(&dividerPen, 20, headerH, clientW - 20, headerH);

            // 3. Central Image Viewport (Framed in a Rounded Card Container)
            int cardMargin = 20;
            int cardX = cardMargin;
            int cardY = headerH + 12;
            int cardW = clientW - (cardMargin * 2);
            int cardH = clientH - headerH - footerH - 24;

            if (cardW > 40 && cardH > 40 && m_pBitmap) {
                Gdiplus::RectF cardRect((Gdiplus::REAL)cardX, (Gdiplus::REAL)cardY, 
                                       (Gdiplus::REAL)cardW, (Gdiplus::REAL)cardH);
                float cardRadius = 10.0f;

                // Drop shadow behind card container
                DrawRoundedDropShadow(g, cardRect, cardRadius, 4, 30);

                // Card background and border (#16161b, border #282832)
                Gdiplus::GraphicsPath cardPath;
                AddRoundedRect(cardPath, cardRect, cardRadius);

                Gdiplus::SolidBrush cardBgBrush(Gdiplus::Color(255, 22, 22, 27));
                g.FillPath(&cardBgBrush, &cardPath);

                Gdiplus::Pen cardBorderPen(Gdiplus::Color(255, 40, 40, 50), 1.0f);
                g.DrawPath(&cardBorderPen, &cardPath);

                // Image inside card with padding
                int pad = 12;
                int availW = cardW - (pad * 2);
                int availH = cardH - (pad * 2);

                int imgW = (int)m_pBitmap->GetWidth();
                int imgH = (int)m_pBitmap->GetHeight();

                double scale = 1.0;
                if (imgW > availW || imgH > availH) {
                    double scaleW = (double)availW / (double)imgW;
                    double scaleH = (double)availH / (double)imgH;
                    scale = std::min(scaleW, scaleH);
                }

                int drawW = (int)(imgW * scale);
                int drawH = (int)(imgH * scale);
                int drawX = cardX + (cardW - drawW) / 2;
                int drawY = cardY + (cardH - drawH) / 2;

                Gdiplus::Rect imgRect(drawX, drawY, drawW, drawH);
                g.DrawImage(m_pBitmap, imgRect);

                // Thin crisp inner outline around the image
                Gdiplus::Pen imgBorder(Gdiplus::Color(255, 52, 52, 64), 1.0f);
                g.DrawRectangle(&imgBorder, drawX - 1, drawY - 1, drawW + 1, drawH + 1);
            }

            // 4. Bottom Footer Metadata Bar
            int footerY = clientH - footerH;
            g.DrawLine(&dividerPen, 20, footerY, clientW - 20, footerY);

            if (m_pBitmap) {
                WCHAR infoBuf[160];
                swprintf_s(infoBuf, L"RESOLUTION: %u \u00D7 %u PX   \u2022   STORAGE: CLIPBOARD (NEVER AUTO-SAVED)   \u2022   HOTKEYS: F14 / CTRL+SHIFT+S",
                          m_pBitmap->GetWidth(), m_pBitmap->GetHeight());

                Gdiplus::Font footFont(L"Segoe UI", 7.8f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
                Gdiplus::SolidBrush footBrush(Gdiplus::Color(255, 120, 120, 134));
                Gdiplus::PointF footPt(22.0f, (Gdiplus::REAL)(footerY + 10));
                g.DrawString(infoBuf, -1, &footFont, footPt, &footBrush);
            }
        }

        BitBlt(hdc, 0, 0, clientW, clientH, hdcMem, 0, 0, SRCCOPY);

        SelectObject(hdcMem, hbmOld);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_CLOSE:
        Close();
        return 0;
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}
