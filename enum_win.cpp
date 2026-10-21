#include <windows.h>
#include <iostream>
#include <string>

BOOL CALLBACK EnumProc(HWND hwnd, LPARAM lParam) {
    wchar_t cls[256] = {};
    GetClassNameW(hwnd, cls, 256);
    wchar_t title[256] = {};
    GetWindowTextW(hwnd, title, 256);

    std::wstring sCls(cls);
    if (!sCls.empty() && (sCls.find(L"Tray") != std::wstring::npos || sCls.find(L"Shell") != std::wstring::npos || sCls.find(L"Progman") != std::wstring::npos)) {
        std::wcout << L"HWND=" << (void*)hwnd << L" Cls=" << cls << L" Title=" << title << L" Vis=" << IsWindowVisible(hwnd) << std::endl;
    }
    return TRUE;
}

int main() {
    HDESK hDesk = OpenDesktopA("Default", 0, FALSE, GENERIC_ALL);
    if (!hDesk) {
        std::cout << "Failed to open Default desktop, err: " << GetLastError() << std::endl;
        return 1;
    }
    std::cout << "Opened Default desktop: " << (void*)hDesk << std::endl;
    
    // Enum windows on Default desktop
    EnumDesktopWindows(hDesk, EnumProc, 0);

    CloseDesktop(hDesk);
    return 0;
}
