#include "ImageSaver.h"
#include <commdlg.h>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>

int ImageSaver::GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0;          // number of image encoders
    UINT size = 0;         // size of the image encoder array in bytes

    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;

    Gdiplus::ImageCodecInfo* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
    if (!pImageCodecInfo) return -1;

    Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }

    free(pImageCodecInfo);
    return -1;
}

bool ImageSaver::PromptAndSave(HWND hwndParent, Gdiplus::Bitmap* pBitmap, std::wstring* outSavedPath) {
    if (!pBitmap) return false;

    // Generate default filename with current timestamp
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::wstringstream ss;
    ss << L"Snip_" << std::put_time(&tm, L"%Y%m%d_%H%M%S") << L".png";
    std::wstring defaultFileName = ss.str();

    WCHAR szFile[MAX_PATH] = { 0 };
    wcsncpy_s(szFile, defaultFileName.c_str(), _TRUNCATE);

    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwndParent;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
    ofn.lpstrFilter = L"PNG Image (*.png)\0*.png\0JPEG Image (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0Bitmap Image (*.bmp)\0*.bmp\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrDefExt = L"png";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_ENABLESIZING;

    if (!GetSaveFileNameW(&ofn)) {
        return false; // User cancelled
    }

    std::wstring chosenPath = szFile;
    if (SaveToFile(pBitmap, chosenPath)) {
        if (outSavedPath) {
            *outSavedPath = chosenPath;
        }
        return true;
    }
    return false;
}

bool ImageSaver::SaveToFile(Gdiplus::Bitmap* pBitmap, const std::wstring& filePath) {
    if (!pBitmap || filePath.empty()) return false;

    // Detect format from extension
    std::wstring ext;
    size_t dotPos = filePath.find_last_of(L".");
    if (dotPos != std::wstring::npos) {
        ext = filePath.substr(dotPos + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
    }

    const WCHAR* mime = L"image/png";
    if (ext == L"jpg" || ext == L"jpeg") {
        mime = L"image/jpeg";
    } else if (ext == L"bmp") {
        mime = L"image/bmp";
    }

    CLSID encoderClsid;
    if (GetEncoderClsid(mime, &encoderClsid) < 0) {
        return false;
    }

    Gdiplus::Status status = pBitmap->Save(filePath.c_str(), &encoderClsid, NULL);
    return (status == Gdiplus::Ok);
}
