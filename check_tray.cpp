#include <windows.h>
#include <iostream>

int main() {
    HWND hTray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!hTray) {
        std::cout << "Shell_TrayWnd NOT FOUND!" << std::endl;
        return 1;
    }
    BOOL vis = IsWindowVisible(hTray);
    std::cout << "Shell_TrayWnd handle: " << (void*)hTray << ", IsVisible: " << (vis ? "TRUE (Visible)" : "FALSE (Hidden)") << std::endl;

    RECT rc = {};
    GetWindowRect(hTray, &rc);
    std::cout << "Tray Rect: [" << rc.left << ", " << rc.top << ", " << rc.right << ", " << rc.bottom << "]" << std::endl;

    RECT wa = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    std::cout << "WorkArea: [" << wa.left << ", " << wa.top << ", " << wa.right << ", " << wa.bottom << "]" << std::endl;

    return 0;
}
