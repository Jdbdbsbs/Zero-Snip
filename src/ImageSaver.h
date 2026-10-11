#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <string>

class ImageSaver {
public:
    // Prompts the user with GetSaveFileNameW and saves the bitmap.
    // Returns true if successfully saved, false if cancelled or error.
    // If outSavedPath is provided, receives the chosen file path.
    static bool PromptAndSave(HWND hwndParent, Gdiplus::Bitmap* pBitmap, std::wstring* outSavedPath = nullptr);

    // Direct save helper with specific encoder
    static bool SaveToFile(Gdiplus::Bitmap* pBitmap, const std::wstring& filePath);

private:
    static int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);
};
