# Changelog

All notable changes to the **AquaOS** project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.0.1] - 2026-10-22

### 🎉 Initial Release — AquaOS Foundation

We are thrilled to announce the initial release of **AquaOS v0.0.1**, an ultra-fast, hardware-accelerated macOS desktop environment layer for Windows 10 and 11. Built entirely in pure **C++20** with **DirectComposition**, **Direct2D**, and **DirectWrite**, AquaOS delivers the fluid, elegant macOS workflow without the heavy memory footprint or latency of web runtimes.

---

### ✨ Key Features & Capabilities

#### 🖥️ macOS Top Menu Bar (`TopBar`)
- **Active Application Tracking:** Real-time window focus detection displaying active app names in bold typography.
- **Dynamic Application Menus:** Routes native Win32 `HMENU` commands for legacy applications and provides contextual macOS menus for modern apps (VS Code, browsers, File Explorer, Terminal).
- **Interactive System Status Widgets:** Integrated Wi-Fi indicator, battery percentage & charging detection (`GetSystemPowerStatus`), Spotlight launcher, Control Center flyout toggle, and synchronized clock.
- **Apple System Menu:** Quick access to About AquaOS, System Settings, Task Manager, Wallpaper Cycling, Dark/Light Theme toggle, Lock Screen, and power management actions.

#### 🚀 Floating Parabolic Dock (`Dock`)
- **Physics-Based Parabolic Magnification:** Smooth cosine distance falloff expanding icons from 46px to 68px with hardware bicubic filtering.
- **Genuine High-Res Windows App Icons:** Direct extraction of 256x256 / 48x48 32-bit ARGB icons from installed applications via `SHGetImageList` Jumbo/ExtraLarge APIs.
- **Intelligent Window Minimization & Restoral:** Left-clicking active apps minimizes them smoothly into the dock without stealing focus (`WS_EX_NOACTIVATE`); left-clicking background apps restores them to the foreground.
- **Custom Application Pinning:** Persistent dock layout configuration stored in `dock_items.cfg` with right-click context menu options to pin any `.exe` or `.lnk`.
- **Spring Physics Animation:** Vertical spring bounce animation upon launching applications, complemented by glowing active running indicator dots.

#### 🔍 Spotlight Search & Quick Calculator (`Spotlight`)
- **Global Hotkey:** `Alt + Space`.
- **Fuzzy Application Search:** Instant indexing and launching of installed Windows applications and system tools.
- **Live Arithmetic Evaluation:** Instant math calculator supporting inline expressions (`*`, `/`, `+`, `-`).
- **Web Search Integration:** Fallback search directly into Google Search via the default browser.

#### 🎛️ macOS Control Center (`ControlCenter`)
- **Quick Control Tiles:** One-click toggles for Wi-Fi, Bluetooth, Dark Mode, and Do Not Disturb.
- **Native Audio Volume Slider:** Real-time master sound volume reading and adjustment via Windows CoreAudio `IAudioEndpointVolume`.
- **Display Brightness Control:** Smooth physical display backlight adjustments.

#### 🪟 Mission Control & Virtual Desktops Spaces Bar (`MissionControl`)
- **Global Hotkey:** `F3`.
- **DWM Live Video Thumbnails:** Hardware-accelerated live composited window feeds via `DwmRegisterThumbnail` with 60 FPS fluidity.
- **Top Spaces Bar:** Interactive virtual desktops shelf featuring thumbnail capsules ("Desktop 1", "Desktop 2", etc.), active space indicators, and seamless desktop switching via Windows Virtual Desktop shortcuts.
- **Desktop Isolation:** Integrated with `IVirtualDesktopManager` to display windows strictly belonging to the active virtual workspace.
- **Interactive Card Management:** Direct window termination via the macOS-style red close button (`×`) on hover.

#### 📱 Fullscreen Launchpad (`Launchpad`)
- **Global Hotkey:** `F4`.
- **Start Menu Crawler:** Auto-discovers 100+ installed applications across `CSIDL_COMMON_PROGRAMS` and `CSIDL_PROGRAMS`.
- **macOS 7x5 App Grid:** High-res 64px icons with smooth hover scaling and multi-page pagination dots.
- **Instant Search Pill:** Centered frosted search input filtering apps in real-time.

#### 📊 Notification Center & Live Widgets (`NotificationCenter`)
- **Global Hotkey:** `Ctrl + Alt + N`.
- **Frosted Glass Flyout:** Right-edge sliding panel with smooth 60 FPS cubic ease animation.
- **Real-Time Hardware Gauges:** Dual circular progress rings sampling CPU and RAM metrics with zero measurable overhead via `GetSystemTimes` and `GlobalMemoryStatusEx`.
- **Modular Widgets:** Integrated monthly calendar widget, multi-timezone world clock, and San Francisco typography scratchpad notes widget.

#### 📑 macOS Stage Manager (`StageManager`)
- **Global Hotkey:** `Ctrl + Alt + S`.
- **Left-Edge Live Thumbnail Strip:** Organizes inactive running apps into vertical stages composited live with DWM.
- **App Stacking:** Automatically clusters multiple windows of the same application into tiered stacks with count badges.
- **Hover Slide & Center Stage:** Hovering slides stage cards 14px to the right; clicking promotes a stage to Center Stage while parking the active window.

#### 🧲 macOS Sequoia Magnetic Window Tiling (`WindowTiler`)
- **Interactive Edge Snapping:** Drag any application window to screen edges or corners to trigger magnetic snapping.
- **Translucent Blue Snap Preview Overlay:** Hardware-accelerated frosted blue overlay (`rgba(0, 122, 255, 0.22)`) indicating the snap zone destination.
- **Keyboard Shortcuts:** Full suite of Sequoia tiling shortcuts:
  - `Ctrl + Alt + Left / Right`: Snap to Left or Right Half.
  - `Ctrl + Alt + Up / Down`: Maximize or Center window.
  - `Ctrl + Alt + 1 / 2 / 3 / 4`: Snap to Four Quarters.

#### 👁️ macOS Quick Look File Previewer (`QuickLook`)
- **Instant File Previews:** Select any file in Windows File Explorer or the Desktop and press **`Spacebar`**.
- **WIC Image Previews:** Hardware-accelerated decoding of PNG, JPG, GIF, WebP, BMP, ICO, and TIFF.
- **Code & Syntax Highlighting:** Monospace previews with line numbers and syntax coloring for C++, Python, JavaScript, TypeScript, Batch, PowerShell, JSON, HTML, and Markdown.
- **Live Navigation:** Use arrow keys while Quick Look is open to cycle through files with instant live preview updating.

#### 🌅 Dynamic Day/Night Wallpaper Engine (`ThemeManager`)
- **5 macOS Wallpaper Styles:** Sonoma Horizon, Sequoia Wilderness, Ventura Dunes, Monterey Ocean, and Big Sur Coast.
- **4 Solar Phases:** Real-time time-adaptive transitions across Dawn, Day, Sunset, and Night.
- **Direct-Memory BMP Generation:** Sub-millisecond in-memory bitmap generation avoiding external dependencies.
- **Theme Palette:** Unified system-wide Light and Dark mode switching (`Ctrl + Alt + D`).

#### 🔒 macOS Sonoma Lock Screen & Screensaver (`LockScreen`)
- **Atmospheric Visuals:** Fullscreen Sonoma aerial motion background with subtle nebula glow.
- **San Francisco Large Typography Clock:** 76pt bold clock with live formatted date banner.
- **Interactive Password Pill:** Frosted input field with masked character dots (`●●●`) and spring shake animation on error.
- **System Power Actions:** Sleep, Restart, Shut Down, and Switch User buttons.

#### 🔔 macOS Notification Banners (`NotificationBanner` & `NotificationService`)
- **Floating Banner Cards:** Top-right frosted acrylic cards (16px corner radius, specular top highlight) sliding in with 60 FPS spring physics.
- **Windows Toast Listener:** Background WinRT `UserNotificationListener` integration intercepting notifications from installed apps (Slack, Teams, Chrome, Outlook, System) and rendering them in authentic macOS styling.
- **Card Controls:** 5-second auto-dismiss, hover pause, and click-to-dismiss.

#### 🛡️ Taskbar Suppression & Restoration (`TaskbarController`)
- **Clean Shell Takeover:** Suppresses the Windows 10/11 taskbar and Start button while expanding the work area below the TopBar.
- **Interactive Toggle:** Press **`Ctrl + Alt + T`** to show or hide the Windows taskbar at any time.
- **Fail-Safe Recovery:** Multiple safety handlers (`atexit`, unhandled exception filters) and standalone `RestoreTaskbar.exe` ensuring the Windows taskbar is never lost.

#### 📦 CI/CD & Automated Installer
- **Inno Setup Script:** [`installer/AquaOS.iss`](installer/AquaOS.iss) generating standalone `AquaOS-Setup.exe`.
- **Packaging Automation:** [`scripts/package.bat`](scripts/package.bat) creating portable ZIP packages and compiling installers.
- **GitHub Actions Pipeline:** Multi-step CI/CD workflow building and publishing releases automatically.

---

### 📦 Artifacts & Downloads
- **Portable ZIP:** `AquaOS-v0.0.1-windows-x64.zip`
- **Standalone Installer:** `AquaOS-v0.0.1-Setup.exe`
- **Supported OS:** Windows 10 (Build 19041+) / Windows 11 (64-bit)

