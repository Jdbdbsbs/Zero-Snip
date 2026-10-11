#include "ClipboardHelper.h"
#include <iostream>

bool ClipboardHelper::CopyToClipboard(HWND hwndOwner, HBITMAP hBitmap) {
    if (!hBitmap) return false;

    BITMAP bm;
    if (!GetObject(hBitmap, sizeof(BITMAP), &bm)) {
        return false;
    }

    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) return false;

    DWORD dwBytesPerRow = ((bm.bmWidth * 32 + 31) / 32) * 4;
    DWORD dwSizeImage = dwBytesPerRow * bm.bmHeight;
    DWORD dwTotalSize = sizeof(BITMAPINFOHEADER) + dwSizeImage;

    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, dwTotalSize);
    if (!hGlobal) {
        ReleaseDC(NULL, hdcScreen);
        return false;
    }

    BYTE* pBuffer = (BYTE*)GlobalLock(hGlobal);
    if (!pBuffer) {
        GlobalFree(hGlobal);
        ReleaseDC(NULL, hdcScreen);
        return false;
    }

    BITMAPINFOHEADER* pHeader = (BITMAPINFOHEADER*)pBuffer;
    ZeroMemory(pHeader, sizeof(BITMAPINFOHEADER));
    pHeader->biSize = sizeof(BITMAPINFOHEADER);
    pHeader->biWidth = bm.bmWidth;
    pHeader->biHeight = bm.bmHeight; // bottom-up
    pHeader->biPlanes = 1;
    pHeader->biBitCount = 32;
    pHeader->biCompression = BI_RGB;
    pHeader->biSizeImage = dwSizeImage;

    BITMAPINFO bi;
    bi.bmiHeader = *pHeader;

    int scanlines = GetDIBits(hdcScreen, hBitmap, 0, bm.bmHeight, 
                              pBuffer + sizeof(BITMAPINFOHEADER), 
                              &bi, DIB_RGB_COLORS);

    GlobalUnlock(hGlobal);
    ReleaseDC(NULL, hdcScreen);

    if (scanlines == 0) {
        GlobalFree(hGlobal);
        return false;
    }

    if (!OpenClipboard(hwndOwner)) {
        GlobalFree(hGlobal);
        return false;
    }

    EmptyClipboard();

    // Set CF_DIB
    SetClipboardData(CF_DIB, hGlobal);

    // Also set CF_BITMAP for legacy consumers
    HBITMAP hbmCopy = (HBITMAP)CopyImage(hBitmap, IMAGE_BITMAP, 0, 0, LR_COPYRETURNORG);
    if (hbmCopy) {
        SetClipboardData(CF_BITMAP, hbmCopy);
    }

    CloseClipboard();
    return true;
}

bool ClipboardHelper::CopyToClipboard(HWND hwndOwner, Gdiplus::Bitmap* pBitmap) {
    if (!pBitmap) return false;
    HBITMAP hBmp = NULL;
    Gdiplus::Color bg(0, 0, 0, 0);
    if (pBitmap->GetHBITMAP(bg, &hBmp) != Gdiplus::Ok || !hBmp) {
        return false;
    }
    bool result = CopyToClipboard(hwndOwner, hBmp);
    DeleteObject(hBmp);
    return result;
}
