# Contributing to AquaOS

Thank you for your interest in contributing to **AquaOS**! We welcome contributions from developers passionate about native systems programming, modern UI design, and high-performance Windows desktop enhancement.

This guide outlines our architecture principles, coding standards, development setup, and pull request workflow.

---

## 🏛️ Core Architectural Principles

When contributing to AquaOS, please keep our core tenets in mind:

1. **Pure Native Performance (Zero Web Runtimes):**
   - AquaOS is built strictly in **C++20** using **DirectComposition**, **Direct2D**, and **DirectWrite**.
   - Under no circumstances do we introduce Electron, Chromium, WebView2, or heavy JavaScript runtimes. Our baseline idle CPU is 0.0% and memory footprint is < 65 MB.

2. **DWM Integration & Pre-Multiplied Alpha:**
   - All visual elements render via DirectComposition surfaces with pre-multiplied alpha (`DXGI_FORMAT_B8G8R8A8_UNORM`).
   - Surfaces are composited directly by the Windows Desktop Window Manager (DWM) for tear-free 120+ FPS animations.

3. **High-DPI First (PerMonitorV2):**
   - Every window, dimension, font size, padding, and threshold must be scaled dynamically via `GetDpiScale(hwnd)` or `m_dpiScale`. Hardcoded physical pixel values are prohibited.

4. **Robust Resource Management (RAII):**
   - All COM interfaces must use `Microsoft::WRL::ComPtr<T>` or WinRT projection smart types.
   - OS resources (HWND, HHOOK, GDI objects, handles) must be managed using deterministic RAII wrappers.

5. **Fail-Safe Shell Operations:**
   - Because AquaOS manages taskbar suppression and desktop work areas, all operations must fail safely. `RestoreTaskbar.exe` and `scripts/restore.bat` must remain functional under all failure conditions.

---

## 🛠️ Development Setup

### Prerequisites
- **Operating System:** Windows 10 (Build 19041+) or Windows 11 (64-bit).
- **Compiler:** Microsoft Visual C++ (MSVC) v143 toolset (included with Visual Studio 2022 or VS Build Tools).
- **Windows SDK:** Windows 10/11 SDK (version `10.0.22000.0` or later).
- **Build System:** CMake 3.20 or newer.
- **Optional Tools:** Inno Setup 6 (for building standalone setup installer).

### Getting Started

1. **Clone the Repository:**
   ```bash
   git clone https://github.com/aquaOS-UI/aquaOS.git
   cd operating-system
   ```

2. **Build Release Configuration:**
   ```cmd
   .\scripts\build.bat Release
   ```
   The executable is output to `build\Release\AquaShell.exe`.

3. **Run AquaOS:**
   ```cmd
   .\scripts\run.bat
   ```

4. **Emergency Taskbar Restoration:**
   If you ever exit during development and need to restore the Windows taskbar immediately:
   ```cmd
   .\scripts\restore.bat
   ```

5. **Package Distribution:**
   To test package generation and the standalone installer:
   ```cmd
   .\scripts\package.bat
   ```

---

## 📁 Project Organization

```
operating-system/
├── CMakeLists.txt                # Root CMake definition
├── .clang-format                 # Code formatting specification
├── include/                      # Public headers
│   ├── Common.h                  # Types, macros, DPI utilities
│   ├── core/                     # DirectComposition engine & ThemeManager
│   ├── platform/                 # System, hardware, and taskbar controllers
│   ├── ui/                       # TopBar, Dock, Spotlight, MissionControl, etc.
│   └── services/                 # WindowTiler, StageManager, QuickLook, Notifications
├── src/                          # Implementations matching include structure
├── scripts/                      # Build, packaging, and restore automation
├── installer/                    # Inno Setup installer script (AquaOS.iss)
└── assets/                       # Manifests and Windows resource files
```

---

## 📏 Coding Standards

### Formatting
- We enforce consistent formatting via [`.clang-format`](file:///.clang-format).
- Run `clang-format` on any modified `.cpp` or `.h` files before submitting.

### Style Conventions
- **Classes and Structs:** `PascalCase` (e.g., `NotificationBanner`, `ThemeManager`).
- **Methods and Functions:** `PascalCase` (e.g., `Initialize()`, `Render()`, `HandleMessage()`).
- **Member Variables:** `m_` prefix with camelCase (e.g., `m_hwnd`, `m_dpiScale`, `m_visible`).
- **Constants:** `UPPER_SNAKE_CASE` or `constexpr auto` (e.g., `WM_PROCESS_QUEUE`).
- **Namespaces:** All core code resides within the `Aqua` namespace.

### Threading & WinRT Guidelines
- UI operations must execute on the thread that created the corresponding `HWND`.
- Background worker threads (e.g., in `NotificationService`, `HardwareController`, `MediaControllerService`) must initialize C++/WinRT apartments safely:
  ```cpp
  try {
      winrt::init_apartment();
      // WinRT operations...
  } catch (...) {
      // Gracefully handle unavailable WinRT components
  }
  ```

---

## 🔀 Git Workflow & Commit Guidelines

### Branching Strategy
- `main` is our production and stable release branch.
- Create feature branches with descriptive names:
  - `feat/feature-name`
  - `fix/bug-description`
  - `perf/optimization`
  - `refactor/subsystem-name`

### Commit Message Conventions
We follow the **Conventional Commits** specification:
- `feat(scope): add feature description`
- `fix(scope): resolve bug description`
- `docs(scope): update documentation`
- `refactor(scope): internal refactoring`
- `perf(scope): performance optimization`
- `ci(scope): updates to build scripts or GitHub Actions`

*Example:*
```git
feat(tiler): add magnetic window preview overlay for Sequoia snapping
fix(dock): resolve focus stealing during minimize toggle
```

---

## 📬 Submitting a Pull Request

1. **Verify Local Build:** Ensure `.\scripts\build.bat Release` completes with 0 errors and 0 warnings.
2. **Test Runtime:** Verify normal launch (`run.bat`), shortcuts (`F3`, `Alt+Space`, `Ctrl+Alt+S`, etc.), and clean shutdown (`restore.bat`).
3. **Open PR:** Submit a Pull Request targeting the `main` branch with:
   - A clear explanation of what changed and why.
   - Steps to reproduce or test the change.
   - Any relevant screenshots or GIFs for UI modifications.

Thank you for helping make AquaOS the fastest and most elegant macOS desktop layer for Windows!
