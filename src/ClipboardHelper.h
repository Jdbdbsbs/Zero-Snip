#pragma once
#include <windows.h>
#include <gdiplus.h>

class ClipboardHelper {
public:
    // Copies an HBITMAP to clipboard as both CF_DIB and CF_BITMAP
    static bool CopyToClipboard(HWND hwndOwner, HBITMAP hBitmap);

    // Copies a Gdiplus::Bitmap to clipboard
    static bool CopyToClipboard(HWND hwndOwner, Gdiplus::Bitmap* pBitmap);
};
