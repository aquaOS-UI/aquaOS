#include <windows.h>
#include <iostream>

int main() {
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    if (CreateProcessW(L"D:\\Projects\\workspace\\operating-system\\build\\Release\\AquaShell.exe", nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        std::cout << "Waiting for process..." << std::endl;
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD code = 0;
        GetExitCodeProcess(pi.hProcess, &code);
        std::cout << "Exit code: 0x" << std::hex << code << std::dec << " (" << code << ")" << std::endl;
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        std::cout << "CreateProcess failed: " << GetLastError() << std::endl;
    }
    return 0;
}
