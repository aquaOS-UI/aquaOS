# AquaShell — macOS Desktop UI Layer for Windows

**AquaShell** is an ultra-fast, hardware-accelerated desktop UI layer for Windows built in pure **C++20** with **DirectComposition**, **Direct2D**, and **DirectWrite**. 

It replicates the fluid macOS desktop environment (Top Menu Bar, Parabolic Magnifying Dock, Spotlight Search, and Control Center) without relying on heavy web runtimes like Electron or Chromium.

---

## ⚡ Performance Highlights
- **Binary Footprint:** ~104 KB standalone executable.
- **Memory Usage:** ~15–30 MB RAM (vs 400+ MB for Electron-based shells).
- **CPU Overhead:** 0.0% at idle; 120 FPS hardware-accelerated spring animations.
- **Rendering:** Pre-multiplied alpha DirectComposition swapchains linked directly to Windows Desktop Window Manager (DWM).
- **Display Support:** Full Per-Monitor DPI Awareness v2 (pixel-perfect scaling on 100%, 125%, 150%, and 4K displays).

---

## 🖥️ Core Components

### 1. Top Menu Bar (`TopBar`)
- **System Logo Menu:** Apple-style system popup menu (About, System Settings, Task Manager, Lock, Sleep, Restart, Shutdown, Quit).
- **Active Application Title:** Live foreground process tracking.
- **Application Menus:** Interactive dropdowns (`File`, `Edit`, `View`, `Window`, `Help`).
- **Live Status Widgets:**
  - Real-time Wi-Fi status indicator.
  - Live battery percentage & charging detection (`GetSystemPowerStatus`).
  - Search / Spotlight quick launcher button.
  - Control Center flyout toggle button.
  - Formatted live clock updating every second (`Wed Oct 21  1:55 PM`).

### 2. Floating Dock with Spring Physics & Real App Icons (`Dock`)
- **Real Windows App Icons:** Uses Windows Shell APIs (`SHGetImageList` Jumbo/ExtraLarge + `ExtractIconExW`) to extract genuine high-res 256x256 / 48x48 32-bit ARGB icons directly from installed `.exe` and `.lnk` files.
- **Parabolic Magnification:** Uses cosine distance falloff $f(d) = \cos(\frac{\pi d}{2R})$ to dynamically scale icons from 46px to 68px with hardware bicubic filtering as the cursor glides across the dock.
- **Custom App Pinning & Context Menu:**
  - **Right-Click Any Dock Icon** to open the context menu:
    - **Open [App]**
    - **Show in File Explorer**
    - **Remove from Dock**
    - **Pin Application to Dock...** (Pick any `.exe` or `.lnk` from your computer to pin it with its genuine icon)
- **Persistent Dock Config:** Automatically saves your customized dock layout to `dock_items.cfg`.
- **Spring Bounce Animation:** Clicking any app triggers a physical vertical spring bounce.
- **Running Indicators:** Glowing dots underneath active and launched applications.
- **Interactive Tooltip:** Floating frosted pill displaying the application title above the hovered item.
- **Bundled Shortcuts:**
  - Finder (File Explorer)
  - Launchpad (Spotlight Grid)
  - Safari (Web Browser)
  - Terminal (Windows Terminal / PowerShell)
  - VS Code
  - Files
  - Calculator
  - Activity Monitor (Task Manager)
  - System Settings
  - Trash (Recycle Bin)

### 3. Spotlight Search Overlay (`Spotlight`)
- **Global Hotkey:** `Alt + Space` (works anywhere in Windows).
- **Instant Search:** Real-time fuzzy filtering of applications and system tools.
- **Live Calculator:** Enter arithmetic expressions (e.g. `25 * 4`, `500 / 2`, `80 + 120`) to get instant answers.
- **Web Search Integration:** Fallback to Google Search in the default browser.
- **Keyboard Navigation:** `Up` / `Down` arrows to navigate results; `Enter` to launch; `Escape` to close.

### 4. macOS Control Center (`ControlCenter`)
- **Quick Toggles:** Wi-Fi, Bluetooth, Dark Mode, and Focus / Do Not Disturb.
- **Direct Sound Volume Control:** Reads and updates Windows master volume in real-time via the Windows CoreAudio API (`IAudioEndpointVolume`).
- **Display Brightness Slider:** Smooth interactive brightness control.

### 5. macOS Mission Control / Window Grid (`MissionControl`)
- **Global Hotkey:** Press **`F3`** (or click Launchpad in Dock or View -> Mission Control).
- **Hardware-Accelerated DWM Live Thumbnails:** Uses Windows Desktop Window Manager (`DwmRegisterThumbnail` + `DwmUpdateThumbnailProperties`) to stream live, 60+ FPS composited thumbnails of all running application windows.
- **Smart Adaptive Grid Layout:** Dynamically arranges open windows into optimal macOS-style columns and rows based on window aspect ratio.
- **Spaces / Desktops Bar:** Displays top macOS Sonoma/Sequoia style "Desktop 1" spaces pill.
- **Card Controls:**
  - Hover over any window preview to see its glowing accent blue border and title.
  - Hover over the top-right of a card to reveal the macOS red close button (`X`) to terminate a window directly from Mission Control.
  - Click any card to raise, restore, and focus that application window instantly.
  - Press `Escape` or `F3` or click the frosted backdrop to dismiss.

### 6. Taskbar Suppression & Work Area Takeover (`TaskbarController`)
- **Shell Takeover:** Automatically suppresses the Windows 10/11 taskbar (`Shell_TrayWnd`, secondary tray windows, and Start button).
- **Work Area Reflow:** Adjusts the Windows desktop work area (`SPI_SETWORKAREA`) so maximized application windows seamlessly snap below the 32px TopBar.
- **Toggle Anytime:** Press **`Ctrl + Alt + T`** or select **"Show/Hide Windows Taskbar"** from the Apple logo menu to toggle the Windows taskbar on or off on demand.
- **Fail-Safe Restoration:** Restores the default Windows taskbar and desktop work area cleanly on exit.

---

## ⌨️ Global Shortcuts

| Shortcut | Action |
| :--- | :--- |
| **`F3`** | Toggle macOS Mission Control (Live Window Grid) |
| **`Alt + Space`** | Toggle macOS Spotlight Search & Calculator |
| **`Ctrl + Alt + T`** | Toggle Windows Taskbar visibility |
| **`Escape`** | Close Spotlight / Mission Control / Control Center |
| **`Win + L`** | Lock Screen |

---

## 🛠️ Building & Running

### Requirements
- Windows 10 (Build 19041+) or Windows 11.
- Visual Studio 2022 (MSVC v143 toolset) or VS Build Tools.
- Windows 10/11 SDK.
- CMake 3.20+.

### Build
Run `build.bat` from PowerShell or Command Prompt:
```cmd
build.bat
```
The compiled binary will be placed at `build\Release\AquaShell.exe`.

### Run
Launch `run.bat` or run the executable directly:
```cmd
run.bat
```

