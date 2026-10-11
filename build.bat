@echo off
setlocal enabledelayedexpansion
echo ===================================================
echo   Zero Snip - Complete Repository Build
echo ===================================================

set CXX=g++
set CXXFLAGS=-O2 -municode -mwindows -static -static-libgcc -static-libstdc++
set LIBS=-lgdiplus -lgdi32 -luser32 -lcomdlg32 -lshell32 -lole32 -ldwmapi

echo [1/3] Compiling Zero Snip application (zerosnip.exe) ...
cd src
windres.exe zerosnip.rc -O coff -o zerosnip-res.o
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to compile zerosnip.rc
    cd ..
    goto :error
)

set SRCS=main.cpp SnipperOverlay.cpp PreviewWindow.cpp ClipboardHelper.cpp ImageSaver.cpp HotKeyManager.cpp PrivilegeHelper.cpp StartupHelper.cpp
%CXX% %CXXFLAGS% %SRCS% zerosnip-res.o -o zerosnip.exe %LIBS%
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to compile zerosnip.exe
    cd ..
    goto :error
)
strip -s zerosnip.exe
copy /Y zerosnip.exe ..\installer\zerosnip.exe >nul

echo [2/3] Compiling standalone uninstaller (uninstall.exe) ...
%CXX% %CXXFLAGS% uninstall.cpp -o uninstall.exe -lshell32 -lole32
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to compile uninstall.exe
    cd ..
    goto :error
)
strip -s uninstall.exe
copy /Y uninstall.exe ..\installer\uninstall.exe >nul
cd ..

echo [3/3] Compiling Zero Snip Setup Installer (zerosnip-setup.exe) ...
cd installer
windres.exe installer.rc -O coff -o installer-res.o
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to compile installer.rc
    cd ..
    goto :error
)
%CXX% %CXXFLAGS% installer_main.cpp installer-res.o -o zerosnip-setup.exe -lgdiplus -lgdi32 -luser32 -lshell32 -lole32 -luuid -ldwmapi
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to compile zerosnip-setup.exe
    cd ..
    goto :error
)
strip -s zerosnip-setup.exe
copy /Y zerosnip-setup.exe ..\releases\zerosnip-setup.exe >nul
cd ..

echo.
echo ===================================================
echo   BUILD SUCCESSFUL!
echo   Installer placed in releases/:
echo     - zerosnip-setup.exe (GUI Installer, ~2.4 MB)
echo ===================================================
goto :end

:error
echo [ERROR] Build failed. Please check compiler errors above.
exit /b 1

:end
endlocal
