#pragma once
#include <string>

namespace StartupHelper {
    std::wstring GetExecutablePath();
    bool IsStartupEnabled();
    bool EnableStartup(const std::wstring& explicitPath = L"");
    bool DisableStartup();
}
