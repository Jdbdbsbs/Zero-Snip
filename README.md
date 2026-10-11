<p align="center">
  <img src="assets/logo.png" alt="Zero Snip Logo" width="130" height="130" style="border-radius: 24px;">
</p>

<h1 align="center">Zero Snip</h1>

<p align="center">
  <strong>The Ultra-Lightweight, Zero-Storage Snipping Tool for Windows</strong>
</p>

<p align="center">
  <em>Instant drag & capture. Direct to clipboard. Zero disk pollution.</em>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011-0078D6?style=flat-square&logo=windows" alt="Platform">
  <img src="https://img.shields.io/badge/Architecture-x64-black?style=flat-square" alt="Architecture">
  <img src="https://img.shields.io/badge/Language-Pure%20C%2B%2B20-00599C?style=flat-square&logo=cplusplus" alt="Language">
  <img src="https://img.shields.io/badge/Binary%20Size-1.1%20MB-brightgreen?style=flat-square" alt="Binary Size">
  <img src="https://img.shields.io/badge/RAM%20Usage-~7.5%20MB-blueviolet?style=flat-square" alt="RAM Usage">
  <img src="https://img.shields.io/badge/Disk%20Auto--Save-0%20Bytes-success?style=flat-square" alt="Zero Auto-Save">
  <img src="https://img.shields.io/badge/License-MIT-yellow?style=flat-square" alt="License">
</p>

---

## ⚡ What is Zero Snip?

Most people take dozens—or even hundreds—of screenshots every single day to share a quick snippet in Discord, Slack, WhatsApp, web forms, or work chats. 

Standard screenshot tools fail power users in two major ways:
1. **They hoard gigabytes of junk:** They silently save every single snapshot into your `Pictures\Screenshots` folder, cluttering your hard drive with thousands of unwanted files you only needed for three seconds.
2. **They are sluggish and bloated:** They take over a second to trigger, consume hundreds of megabytes of RAM, fail over elevated administrator windows, or rely on heavy frameworks like Electron, .NET, or Qt.

**Zero Snip** is built with a single philosophy: **Drag, capture, and paste immediately.** Captures live strictly in memory and your clipboard. Nothing is ever written to your drive unless you explicitly click **"Save this"**.

---

## ⚔️ Why Zero Snip is Better than Microsoft Snipping Tool

| Feature / Metric | Microsoft Snipping Tool | ShareX | Lightshot | 🎯 **Zero Snip** |
| :--- | :---: | :---: | :---: | :---: |
| **Disk Clutter (Auto-Save)** | ❌ Dumps all snips to disk | ⚠️ Saves to disk by default | ⚠️ Temporary local cache | ✅ **0 Bytes** (Memory only) |
| **Trigger Latency** | ❌ ~500ms – 1500ms (UWP lag) | ⚠️ ~200ms – 400ms | ⚠️ ~150ms | ✅ **< 10ms (Instant GDI)** |
| **Captures Elevated/Admin Apps** | ❌ Blocked by UIPI | ⚠️ Requires manual elevation | ❌ Blocked by UIPI | ✅ **Universal (`SeDebugPrivilege`)** |
| **Binary Size** | ~80 MB (UWP bundle) | ~140 MB (.NET runtime) | ~12 MB | ✅ **~1.1 MB (Pure C++)** |
| **RAM Footprint** | ~90 MB – 180 MB | ~120 MB – 250 MB | ~30 MB | ✅ **~7.5 MB** |
| **Background Noise** | UWP Background Agent | Heavy Tray Suite | Periodic Updaters | ✅ **Silent Native Daemon** |
| **Windows Startup** | Native UWP | Registry Run | Registry Run | ✅ **Task Scheduler (No UAC)** |
| **UI Aesthetic** | Fluent / Ad-hoc | Complex Nested Menus | Dated Win32 | ✅ **Obsidian Dark & Rounded** |

---

### 1. 🗑️ True Ephemeral Pipeline (Zero Storage Waste)
Windows 11 Snipping Tool automatically dumps every capture into `C:\Users\<Name>\Pictures\Screenshots`. Within a few months, users unknowingly accumulate gigabytes of junk images.  
With **Zero Snip**, the captured bitmap is injected straight into Windows clipboard (`CF_DIB` and `CF_BITMAP`). You paste it with `Ctrl+V` anywhere and move on with your day. Only when you explicitly click **"Save this"** does a native Windows file dialog appear.

### 2. ⚡ Lightning Hotkey Response (<10ms)
Microsoft's Snipping Tool runs on modern UWP/XAML frameworks which frequently produce a noticeable stutter, cursor lag, or 1-second delay before the screen freezes.  
Zero Snip registers low-level Win32 global hotkeys (`F14`, `Ctrl+Shift+S`, `PrintScreen`) hooked directly into native GDI virtual desktop framebuffers. The freeze is instant and seamless.

### 3. 🛡️ Universal Capture Over Administrator & Protected Windows
Due to Windows UIPI (User Interface Privilege Isolation), default Snipping Tool fails, produces black screens, or ignores hotkeys when you are focused on Task Manager, Administrator PowerShell/Command Prompt, Registry Editor, anti-cheat games, or elevated software.  
Zero Snip includes native elevation manifests, automatically acquires `SeDebugPrivilege`, and uses per-monitor device context capture with `CAPTUREBLT` to guarantee 100% reliable capture across every pixel on your displays.

### 4. 🪶 Featherweight Standalone Footprint (1.1 MB • 7.5 MB RAM)
Zero Snip is written from the ground up in modern native C++20 using Win32 and GDI+. There is zero Electron bloat, zero Qt framework overhead (down from ksnip's original 100MB+ Qt dependencies), and zero .NET runtime baggage. The entire application is a self-contained ~1.1 MB executable consuming only ~7.5 MB of RAM in the background.

### 5. 🔕 Silent Background Daemon & Painless Startup
Zero Snip can be added to Windows Startup via the included GUI installer or directly from the system tray menu. It creates an elevated Windows Task Scheduler task (`schtasks /sc onlogon /rl highest`) so it starts silently with Windows on boot with full permissions **without triggering annoying UAC prompts** on every restart.

### 6. 🎨 Obsidian Dark Aesthetic (Zero Transparent Slop)
Featuring a deep matte obsidian canvas (`#0d0d12`), DWM immersive dark mode title bars, smooth rounded rectangle cards, and floating drop shadows. No blurry transparent artifacts or jarring Windows borders.

---

## ⌨️ Default Shortcuts

| Shortcut | Action |
| :--- | :--- |
| **`F14`** | Primary drag & capture hotkey (Dedicated macro key) |
| **`Ctrl + Shift + S`** | Universal snipping hotkey (Familiar Snipping Tool muscle memory) |
| **`PrintScreen`** | Global screen capture trigger |
| **`Esc`** | Cancel active snip overlay |
| **`Enter`** (in preview) | Copy capture to clipboard |
| **`Ctrl + S`** (in preview) | Open "Save this" dialog (PNG, JPG, BMP) |

---

## 🚀 Key Features

- **Multi-Monitor Support:** Seamlessly spans virtual desktops, mixed-DPI displays, and multi-monitor setups without coordinate clipping or distortion.
- **Instant Clipboard Injection:** Encodes captures directly as 32-bit DIB and standard device-dependent bitmaps for immediate pasting into Discord, Slack, Chrome, Firefox, Photoshop, Word, etc.
- **On-Demand Preview GUI:** Displays the snip in a compact dark window with a live *"✓ Copied to clipboard"* status pill, **"Save this"**, **"Copy"**, **"New Snip"**, and **"Close"** buttons.
- **System Tray Controls:** Right-click the tray icon to take manual snips, check administrator status, toggle **"Start with Windows"** on or off, or exit.
- **Self-Contained Installer & Uninstaller:** Comes with a custom dark-mode installer (`zerosnip-setup.exe`) that configures Windows Startup with permission prompts, creates Start Menu / Desktop shortcuts, and registers with Windows **Add/Remove Programs** alongside a clean `uninstall.exe`.

---

## 📥 Installation

### Option A: Standalone GUI Installer (Recommended)
1. Download **`zerosnip-setup.exe`** from the [releases](releases/) folder.
2. Run the installer (requests Administrator privileges to configure startup and elevated capture).
3. Choose whether to enable **Start with Windows** on boot (with live status confirmation).
4. Click **Install Now**. Zero Snip installs to `C:\Program Files\Zero Snip` and starts running silently in your system tray immediately!

### Option B: Portable Mode
1. Download **`zerosnip.exe`** from [releases](releases/).
2. Place it in any directory of your choice and run it. It runs completely standalone with zero external DLLs required.

---

## 🛠️ Building From Source

### Prerequisites
- **OS:** Windows 10 or 11 (64-bit)
- **Compiler:** MinGW-w64 (`g++` supporting C++17/C++20)
- **Tools:** `windres` and `strip` (included in MinGW-w64 / WinLibs / MSYS2)

### 1-Click Master Build
Simply run the included build script from the repository root:

```cmd
build.bat
```

This single command will:
1. Compile `src/zerosnip.rc` with the embedded `app.ico` and manifest.
2. Compile and link `src/*.cpp` into `zerosnip.exe` (stripped to ~1.1 MB).
3. Compile `src/uninstall.cpp` into standalone `uninstall.exe`.
4. Compile `installer/installer_main.cpp` into `zerosnip-setup.exe` (embedding `zerosnip.exe`, `uninstall.exe`, and high-res logos).
5. Place both release-ready executables into `releases/`.

---

## 📁 Repository Structure

```text
git/
├── assets/
│   ├── app.ico                  # Multi-resolution Windows icon (16x16 to 256x256)
│   ├── logo.png                 # High-resolution application logo
│   └── logo.jpg                 # Original logo artwork
├── installer/
│   ├── installer_main.cpp       # Dark GUI installer with startup permission flow
│   ├── installer.manifest       # Administrator execution level manifest
│   ├── installer.rc             # Resource script embedding payload binaries & logo
│   └── build_installer.bat      # Installer-specific build script
├── releases/
│   ├── zerosnip.exe             # Standalone portable binary (~1.1 MB)
│   └── zerosnip-setup.exe       # Complete standalone GUI installer (~2.4 MB)
├── src/
│   ├── main.cpp                 # Entry point, single-instance mutex, DPI awareness
│   ├── SnipperOverlay.h/.cpp    # Virtual desktop freeze, drag-crop, dimensions
│   ├── PreviewWindow.h/.cpp     # Matte dark preview GUI, rounded buttons, drop shadows
│   ├── ClipboardHelper.h/.cpp   # Direct CF_DIB / CF_BITMAP clipboard injector
│   ├── ImageSaver.h/.cpp        # GetSaveFileNameW save handler (PNG, JPG, BMP)
│   ├── HotKeyManager.h/.cpp     # Global hotkeys (F14, Ctrl+Shift+S), tray icon
│   ├── PrivilegeHelper.h/.cpp   # SeDebugPrivilege and elevation helpers
│   ├── StartupHelper.h/.cpp     # Task Scheduler & Registry startup management
│   ├── zerosnip.manifest        # Application manifest (requireAdministrator, High-DPI)
│   ├── zerosnip.rc              # Windows resource script (icon, manifest)
│   └── uninstall.cpp            # Standalone uninstaller with clean registry cleanup
├── build.bat                    # Master build script (builds app + installer)
├── .gitignore                   # Clean ignore rules for build artifacts
├── LICENSE                      # MIT License
└── README.md                    # Comprehensive documentation & comparison
```

---

## 📄 License & Credits

- Distributed under the **MIT License**. See [`LICENSE`](LICENSE) for details.
- Derived from the open-source spirit of `ksnip`, stripped of Qt bloat and rewritten as an ultra-fast, native Win32/GDI+ snipping utility.
