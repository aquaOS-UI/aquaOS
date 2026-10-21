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

### 1. Unified macOS Top Menu Bar (`TopBar`)
- **Active Application Tracking:** Dynamically updates every time you switch windows to show the active application name in **Bold** (e.g. **Visual Studio Code**, **Notepad**, **Google Chrome**, **Microsoft Edge**, **Terminal**, **Finder**).
- **Dynamic Application Menus:**
  - **Native Win32 Menus:** For classic apps (Notepad, Paint, VLC, Task Manager), extracts their actual native Win32 `HMENU` items and routes clicks directly to the application.
  - **Contextual macOS App Menus:** For modern apps (VS Code, Browsers, Explorer, Terminal), dynamically generates authentic macOS menus:
    - *VS Code:* `File`, `Edit`, `Selection`, `View`, `Go`, `Terminal`, `Window`, `Help`
    - *Browsers:* `File`, `Edit`, `View`, `History`, `Bookmarks`, `Window`, `Help`
    - *Finder:* `File`, `Edit`, `View`, `Go`, `Window`, `Help`
    - *Terminal:* `Shell`, `Edit`, `View`, `Window`, `Help`
  - **Real Action Execution:** Selecting menu items triggers real keyboard commands (`Ctrl+N`, `Ctrl+T`, `Ctrl+S`, `Ctrl+Z`, `Ctrl+W`, `F11`, `F5`) sent directly to the active application.
- **Application Menu (Click Active App Name):**
  - About [Active App]
  - Settings / Preferences (`Ctrl+,`)
  - Hide [Active App] (`Win+Down`)
  - Quit [Active App] (`Alt+F4`)
- **System Logo Menu:** Apple logo system popup (About AquaOS, System Settings, Task Manager, Dynamic Wallpaper Style, Toggle Dark/Light Theme, Stage Manager, Mission Control, Launchpad, Lock Screen, Sleep, Restart, Shutdown, Show/Hide Windows Taskbar, Exit AquaOS).
- **Interactive Live Status Widgets:**
  - Real-time Wi-Fi status indicator (Click to open Windows Wi-Fi & Network Settings).
  - Live battery percentage & charging detection (`GetSystemPowerStatus`) (Click to open Windows Battery & Power Settings).
  - Search / Spotlight launcher button (Click to toggle Spotlight).
  - Control Center flyout toggle button (Click to toggle Control Center).
  - Live formatted clock (`Wed Oct 21  4:45 PM`) (Click to open Notification Center & Widgets).

### 2. Floating Dock with Spring Physics & Real App Icons (`Dock`)
- **Intelligent Taskbar Window Controller (Minimize / Restore Toggle):**
  - **Left-Click (Active Foreground App):** Minimizes the active window directly into the dock (`ShowWindow(hwnd, SW_MINIMIZE)`).
  - **Left-Click (Background / Minimized App):** Restores and brings the application window to the foreground seamlessly (`ShowWindow(hwnd, SW_RESTORE)` + `SwitchToThisWindow`).
  - **Left-Click (App Not Running):** Launches a new instance via `ShellExecuteW`.
  - **Shift + Left-Click:** Intentionally opens a **New Window / Instance**.
  - **Middle-Click:** Standard Windows taskbar shortcut to open a **New Window**.
  - **No Focus Stealing:** Built with `WS_EX_NOACTIVATE` and `MA_NOACTIVATE` so clicking the dock preserves application focus until the target action executes.
- **Real Windows App Icons:** Uses Windows Shell APIs (`SHGetImageList` Jumbo/ExtraLarge + `ExtractIconExW`) to extract genuine high-res 256x256 / 48x48 32-bit ARGB icons directly from installed `.exe` and `.lnk` files.
- **Parabolic Magnification:** Uses cosine distance falloff $f(d) = \cos(\frac{\pi d}{2R})$ to dynamically scale icons from 46px to 68px with hardware bicubic filtering as the cursor glides across the dock.
- **Custom App Pinning & Context Menu:**
  - **Right-Click Any Dock Icon** to open the context menu:
    - **Bring [App] to Front**
    - **Minimize [App]**
    - **New Window ([App])**
    - **Close [App]**
    - **Show in File Explorer**
    - **Remove from Dock**
    - **Pin Application to Dock...** (Pick any `.exe` or `.lnk` from your computer to pin it with its genuine icon)
- **Persistent Dock Config:** Automatically saves your customized dock layout to `dock_items.cfg`.
- **Spring Bounce Animation:** Clicking any app triggers a physical vertical spring bounce.
- **Real-Time Running Indicators:** Glowing dots underneath running applications, dynamically updated as apps open, minimize, and close.
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

### 6. macOS Fullscreen Launchpad (`Launchpad`)
- **Global Hotkey:** Press **`F4`** (or click the Launchpad rocket icon in the Dock or View -> Launchpad).
- **Auto-Discovers Installed Apps:** Automatically crawls Windows Start Menu programs (`CSIDL_COMMON_PROGRAMS` + `CSIDL_PROGRAMS`) and Windows built-in tools (100+ applications found automatically).
- **macOS 7x5 App Grid:** Large 64px application icons with readable typography and subtle hover zooms.
- **Instant Search Bar:** Centered macOS frosted search pill at top; start typing to instantly filter apps in real time.
- **Multi-page Pagination Dots:** Interactive pagination dots at the bottom; navigate between pages with arrow keys, mouse wheel, or clicking page dots.
- **One-Click Launch:** Click any app to launch it and dismiss Launchpad.

### 7. macOS Notification Center & Live Widgets (`NotificationCenter`)
- **Global Hotkey:** Press **`Ctrl + Alt + N`** (or click the Clock in the TopBar or View -> Notification Center).
- **Slide-Out Frosted Glass Panel:** Smooth 60 FPS cubic-ease slide-in / slide-out animation anchored to the right display border.
- **Hardware-Sampled Real-Time Performance Gauges:**
  - Dual Circular Progress Arcs for CPU & RAM metrics.
  - Instantaneous low-overhead kernel polling via `GetSystemTimes` (delta calculation) and `GlobalMemoryStatusEx` (< 0.1ms compute cost, 0% CPU impact).
  - Dynamic macOS alert colors: cyan/green for normal load, amber for medium load, orange/red for heavy load.
- **macOS Monthly Calendar Widget:** Displays full month name, year, and a complete 7-day day-of-week calendar grid with current day highlighted in vibrant accent blue.
- **World Clock Widget:** Live synchronized digital clocks displaying time across key timezones (Local, New York, London, Tokyo).
- **Quick Scratchpad Notes Widget:** Embedded macOS Notes widget with clean San Francisco typography for instant note-taking during active tasks.
- **Notification Stream:** Interactive macOS notification cards with application icons, timestamps, message bodies, and a "Clear All" action button.

### 8. macOS Stage Manager (`StageManager`)
- **Global Hotkey:** Press **`Ctrl + Alt + S`** (or select View -> Stage Manager).
- **Left-Edge Live Thumbnail Strip:** Displays inactive running applications organized into vertical stage cards along the left display border.
- **Hardware-Accelerated Live Feeds:** Direct DWM thumbnail composition (`DwmRegisterThumbnail` & `DwmUpdateThumbnailProperties`) streaming window contents in real-time 60 FPS without stealing focus (`WS_EX_NOACTIVATE`).
- **Multi-Window App Stacks:** When an app has multiple open windows, renders an offset stacked card silhouette behind the primary thumbnail with a rounded count pill badge (e.g. `[ 2 ]`, `[ 3 ]`).
- **Interactive 60 FPS Hover Slide:** Hovering over any stage smoothly slides the card 14px to the right with an accent blue glow outline and an app title pill.
- **Fluid Center Stage Switching:** Click any stage card to restore and bring that application to Center Stage while smoothly parking the previous foreground window into the side strip.
- **Desktop Peek:** Click empty background space in the strip to minimize the active window and reveal the clean desktop.

### 9. macOS Sequoia Window Tiling & Snap Zones (`WindowTiler`)
- **Interactive Drag Snapping:** Drag any application window to screen edges or corners to snap into macOS-style tiled layouts.
- **Translucent Accent Blue Silhouette:** Displays a smooth, hardware-accelerated rounded rectangle preview overlay (`rgba(0, 122, 255, 0.22)`) indicating the snap zone destination with spring scale-in easing.
- **Event-Driven & Zero CPU Overhead:** Uses Win32 `EVENT_SYSTEM_MOVESIZESTART` and `EVENT_SYSTEM_MOVESIZEEND` hooks, consuming 0% CPU when not dragging windows.
- **Snap Zones Supported:**
  - **Left Half / Right Half:** 50% split with 6px margin and below TopBar.
  - **Top Half / Bottom Half:** Horizontal split.
  - **Four Corner Quarters:** Top-Left, Top-Right, Bottom-Left, Bottom-Right quarters.
  - **Maximize / Fill:** Snaps to full screen cleanly below TopBar without obscuring the menubar.
  - **Center Window:** Centers window at 72% width and 76% height.
- **macOS Sequoia Keyboard Shortcuts:**
  - `Ctrl + Alt + Left` / `Right`: Snap to Left or Right Half.
  - `Ctrl + Alt + Up`: Maximize / Fill.
  - `Ctrl + Alt + Down`: Center / Restore.
  - `Ctrl + Alt + 1 / 2 / 3 / 4`: Snap to Four Quarters (Top-Left, Top-Right, Bottom-Left, Bottom-Right).
- **Menubar Integration:** Select **Window ➔ Move & Resize (macOS Tiling)** from the TopBar to trigger any snap layout on demand.

### 10. macOS Quick Look (`QuickLook`)
- **Instant `Spacebar` File Previews:** Select any file in Windows File Explorer or on the Desktop and press **`Spacebar`** to open an instant, hardware-accelerated preview modal.
- **Deep Windows Shell Integration:** Directly queries the active Explorer or Desktop view via Windows Shell COM interfaces (`CLSID_ShellWindows`, `IFolderView`) to detect selected items.
- **Caret & Rename Safety:** Monitors the active thread caret (`GUITHREADINFO`); if you are actively typing or renaming a file in Explorer, `Spacebar` types normally.
- **High-Performance Image Previews (WIC):**
  - Instant hardware-accelerated decoding of PNG, JPG, JPEG, BMP, ICO, GIF, WebP, and TIFF.
  - Dynamically computes window dimensions matching the image aspect ratio.
  - Displays pixel resolution (e.g. `2560 × 1440`).
- **Code & Text Previews with Syntax Highlighting:**
  - High-speed preview for C++, Python, JavaScript, TypeScript, JSON, Markdown, HTML, CSS, Batch, PowerShell, INI, SQL, XML, etc.
  - Gutter with line numbers, monospace typography (Cascadia Code / Consolas).
  - Syntax coloring for keywords, strings, comments, and numbers.
- **Interactive Navigation & Actions:**
  - **Live Arrow Key Browsing:** Use the arrow keys while Quick Look is open to seamlessly cycle through items in File Explorer with instant live preview updates.
  - **Open:** Press `Enter` or click **"Open"** to launch the file in its default Windows app.
  - **Dismiss:** Tap `Spacebar` or `Escape` or click the close button (`×`).

### 11. macOS Dynamic Wallpapers & Theme Engine (`ThemeManager`)
- **4 Dynamic macOS Wallpapers:**
  - **Sonoma Horizon:** Sky azure and luminous sunlight transitioning to dusk magenta.
  - **Sequoia Wilderness:** Deep emerald lake and spruce green blending to misty twilight forest.
  - **Ventura Dunes:** Warm desert dawn and rose dunes melting into midnight starlight.
  - **Monterey Ocean:** Vibrant ocean blue ribbons and neon violet twilight.
- **Time-Adaptive Day & Night Transitions:** Wallpapers automatically calculate atmospheric day/night transitions and shift dynamically via `SPI_SETDESKWALLPAPER`.
- **System-Wide Light / Dark Theme Switching:**
  - Centralized palette with frosted acrylic translucencies and high-contrast typography.
  - Instant one-click toggle via the **"Dark Mode"** tile in Control Center.
  - Global Hotkeys:
    - **`Ctrl + Alt + D`**: Toggle Light / Dark Mode system-wide.
    - **`Ctrl + Alt + W`**: Cycle through wallpaper styles instantly.
- **macOS Desktop Context Menu:** Right-click anywhere on the desktop to open the macOS context menu with *New Folder*, *Get Info*, *Change Wallpaper*, *Appearance*, *Stage Manager*, and *Show/Hide Taskbar*.

### 12. macOS Sonoma Lock Screen & Screensaver (`LockScreen`)
- **Authentic macOS Lock Experience:** Fullscreen Sonoma atmospheric aerial motion background with subtle glowing nebula drift.
- **San Francisco Large Typography Clock:** 76pt bold clock display with live formatted date banner (*e.g. Wednesday, October 21*).
- **User Profile & Avatar:** Circular profile plate displaying user initial and Windows account name.
- **Frosted Password Pill & Arrow Submit:** Pill-shaped frosted input field with masked character dots (`●●●`) and arrow button (`➔`).
- **Caps Lock Detection:** Real-time Caps Lock status banner (`⇪ Caps Lock is On`).
- **macOS Damped Spring Shake Animation:** Invalid password or error input triggers authentic horizontal spring oscillation.
- **Bottom System Actions:** One-click circular buttons for **Sleep** (`☾`), **Restart** (`↻`), **Shut Down** (`⏻`), and **Switch User** (`👤`).
- **Instant Triggers:** Press **`Ctrl + Alt + L`** or select **Apple Menu ➔ Lock Screen**.

### 13. Taskbar Suppression & Work Area Takeover (`TaskbarController`)
- **Shell Takeover:** Automatically suppresses the Windows 10/11 taskbar (`Shell_TrayWnd`, secondary tray windows, and Start button).
- **Work Area Reflow:** Adjusts the Windows desktop work area (`SPI_SETWORKAREA`) so maximized application windows seamlessly snap below the 32px TopBar.
- **Toggle Anytime:** Press **`Ctrl + Alt + T`** or select **"Show/Hide Windows Taskbar"** from the Apple logo menu to toggle the Windows taskbar on or off on demand.
- **Fail-Safe Restoration:** Restores the default Windows taskbar and desktop work area cleanly on exit.

---

## ⌨️ Global Shortcuts

| Shortcut | Action |
| :--- | :--- |
| **`Ctrl + Alt + L`** | **Lock Screen (macOS Sonoma Lock Screen & Screensaver)** |
| **`Ctrl + Alt + D`** | Toggle System-Wide macOS Light / Dark Mode |
| **`Ctrl + Alt + W`** | Cycle macOS Dynamic Wallpaper Style |
| **`Spacebar`** *(on Explorer / Desktop file)* | Toggle macOS Quick Look (Instant File & Code Previewer) |
| **`Ctrl + Alt + Left / Right`** | Tile Window to Left / Right Half (macOS Sequoia Tiling) |
| **`Ctrl + Alt + Up / Down`** | Maximize / Center Window |
| **`Ctrl + Alt + 1 / 2 / 3 / 4`** | Snap Window to Quarters (Top-L, Top-R, Bot-L, Bot-R) |
| **`Ctrl + Alt + S`** | Toggle macOS Stage Manager (Side Strip & App Grouping) |
| **`F3`** | Toggle macOS Mission Control (Live Window Grid) |
| **`F4`** | Toggle macOS Launchpad (Fullscreen App Drawer) |
| **`Ctrl + Alt + N`** | Toggle macOS Notification Center & Live Widgets |
| **`Alt + Space`** | Toggle macOS Spotlight Search & Calculator |
| **`Ctrl + Alt + T`** | Toggle Windows Taskbar visibility |
| **`Escape`** | Close active overlay / Unlock Lock Screen |

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

