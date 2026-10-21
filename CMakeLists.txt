cmake_minimum_required(VERSION 3.20)
project(AquaShell
    VERSION 1.0.0
    DESCRIPTION "Enterprise macOS Desktop Environment Layer for Windows"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Project Source Definitions
# ---------------------------------------------------------------------------

set(CORE_SOURCES
    src/core/DCompRenderer.cpp
    src/core/ThemeManager.cpp
)

set(CORE_HEADERS
    include/Common.h
    include/core/DCompRenderer.h
    include/core/ThemeManager.h
)

set(PLATFORM_SOURCES
    src/platform/IconManager.cpp
    src/platform/TaskbarController.cpp
)

set(PLATFORM_HEADERS
    include/platform/SystemInfo.h
    include/platform/IconManager.h
    include/platform/TaskbarController.h
)

set(UI_SOURCES
    src/ui/Dock.cpp
    src/ui/TopBar.cpp
    src/ui/ControlCenter.cpp
    src/ui/NotificationCenter.cpp
    src/ui/Launchpad.cpp
    src/ui/Spotlight.cpp
    src/ui/MissionControl.cpp
    src/ui/LockScreen.cpp
)

set(UI_HEADERS
    include/ui/Dock.h
    include/ui/TopBar.h
    include/ui/ControlCenter.h
    include/ui/NotificationCenter.h
    include/ui/Launchpad.h
    include/ui/Spotlight.h
    include/ui/MissionControl.h
    include/ui/LockScreen.h
)

set(SERVICES_SOURCES
    src/services/WindowTiler.cpp
    src/services/StageManager.cpp
    src/services/QuickLook.cpp
    src/services/AppCatalogService.cpp
    src/services/MediaControllerService.cpp
)

set(SERVICES_HEADERS
    include/services/WindowTiler.h
    include/services/StageManager.h
    include/services/QuickLook.h
    include/services/AppCatalogService.h
    include/services/MediaControllerService.h
)

set(APP_SOURCES
    src/main.cpp
)

set(ASSET_RESOURCES
    assets/resources.rc
    assets/app.manifest
)

# ---------------------------------------------------------------------------
# Target Configuration
# ---------------------------------------------------------------------------

add_executable(AquaShell WIN32
    ${APP_SOURCES}
    ${CORE_SOURCES}
    ${CORE_HEADERS}
    ${PLATFORM_SOURCES}
    ${PLATFORM_HEADERS}
    ${UI_SOURCES}
    ${UI_HEADERS}
    ${SERVICES_SOURCES}
    ${SERVICES_HEADERS}
    ${ASSET_RESOURCES}
)

# Organize virtual folders in IDEs (Visual Studio, CLion)
source_group("App" FILES ${APP_SOURCES})
source_group("Core" FILES ${CORE_SOURCES} ${CORE_HEADERS})
source_group("Platform" FILES ${PLATFORM_SOURCES} ${PLATFORM_HEADERS})
source_group("UI" FILES ${UI_SOURCES} ${UI_HEADERS})
source_group("Services" FILES ${SERVICES_SOURCES} ${SERVICES_HEADERS})
source_group("Assets" FILES ${ASSET_RESOURCES})

# Modular Include Directories
target_include_directories(AquaShell PRIVATE
    include
    include/core
    include/platform
    include/ui
    include/services
    assets
)

# System Libraries
target_link_libraries(AquaShell PRIVATE
    d3d11
    dxgi
    d2d1
    dwrite
    dcomp
    dwmapi
    user32
    gdi32
    shell32
    ole32
    oleaut32
    shlwapi
    windowscodecs
    uuid
    powrprof
    comctl32
    comdlg32
    version
    windowsapp
)

# Compiler & Performance Flags
if (MSVC)
    target_compile_options(AquaShell PRIVATE
        /W3
        /permissive-
        /utf-8
        /MP
    )
    target_compile_definitions(AquaShell PRIVATE
        UNICODE
        _UNICODE
    )
endif()

# ---------------------------------------------------------------------------
# Auxiliary Recovery Tool
# ---------------------------------------------------------------------------
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/RestoreTaskbar.cpp")
    add_executable(RestoreTaskbar tools/RestoreTaskbar.cpp)
    target_link_libraries(RestoreTaskbar PRIVATE user32 gdi32)
    if (MSVC)
        target_compile_options(RestoreTaskbar PRIVATE /W3 /utf-8 /MP)
        target_compile_definitions(RestoreTaskbar PRIVATE UNICODE _UNICODE)
    endif()
endif()
