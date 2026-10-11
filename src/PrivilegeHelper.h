#pragma once
#include <windows.h>

class PrivilegeHelper {
public:
    // Check if the current process is running with Administrator / High Integrity privileges
    static bool IsProcessElevated();

    // Enable SeDebugPrivilege to allow capturing over all processes and system windows
    static bool EnableDebugPrivilege();

    // Attempt to relaunch the current process with elevated administrator permissions
    static bool RelaunchElevated(const WCHAR* lpParameters = nullptr);
};
