@echo off
setlocal
echo ===================================================
echo  Building Zero Snip Setup (Standalone Installer)
echo ===================================================

set CXX=g++
set CXXFLAGS=-O2 -municode -mwindows -static -static-libgcc -static-libstdc++
set LIBS=-lgdiplus -lgdi32 -luser32 -lshell32 -lole32 -luuid -ldwmapi
set OUT=zerosnip-setup.exe

echo Compiling installer resources ...
windres.exe installer.rc -O coff -o installer-res.o
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to compile installer resources.
    goto :end
)

echo Compiling %OUT% ...
%CXX% %CXXFLAGS% installer_main.cpp installer-res.o -o %OUT% %LIBS%

if %ERRORLEVEL% EQU 0 (
    echo Stripping symbols for minimum size ...
    strip -s %OUT%
    echo Successfully built %OUT%!
    copy /Y %OUT% ..\%OUT% >nul 2>nul
    copy /Y %OUT% ..\..\%OUT% >nul 2>nul
    echo Copied %OUT% to workspace root.
) else (
    echo [ERROR] Build failed with error code %ERRORLEVEL%
)

:end
endlocal
