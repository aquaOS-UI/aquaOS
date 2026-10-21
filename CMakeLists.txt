cmake_minimum_required(VERSION 3.20)
project(AquaShell LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

set(SOURCES
    src/main.cpp
    src/DCompRenderer.cpp
    src/TopBar.cpp
    src/Dock.cpp
    src/Spotlight.cpp
    src/ControlCenter.cpp
    src/TaskbarController.cpp
    src/IconManager.cpp
    src/MissionControl.cpp
    src/Launchpad.cpp
    src/NotificationCenter.cpp
    src/StageManager.cpp
    src/WindowTiler.cpp
    src/QuickLook.cpp
    src/ThemeManager.cpp
    src/LockScreen.cpp
)

set(HEADERS
    include/Common.h
    include/DCompRenderer.h
    include/SystemInfo.h
    include/TopBar.h
    include/Dock.h
    include/Spotlight.h
    include/ControlCenter.h
    include/TaskbarController.h
    include/IconManager.h
    include/MissionControl.h
    include/Launchpad.h
    include/NotificationCenter.h
    include/StageManager.h
    include/WindowTiler.h
    include/QuickLook.h
    include/ThemeManager.h
    include/LockScreen.h
)

add_executable(AquaShell WIN32 ${SOURCES} ${HEADERS})

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
)

if (MSVC)
    target_compile_options(AquaShell PRIVATE /W3 /permissive- /utf-8)
endif()

