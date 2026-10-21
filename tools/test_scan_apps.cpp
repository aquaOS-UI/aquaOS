#include <windows.h>
#include <shlobj.h>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

struct AppItem {
    std::wstring name;
    std::wstring path;
};

void ScanDirectory(const fs::path& dir, std::vector<AppItem>& apps) {
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) continue;
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().wstring();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
            if (ext == L".lnk") {
                std::wstring filename = entry.path().stem().wstring();
                std::wstring lower = filename;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

                // Filter out uninstallers and documentation
                if (lower.find(L"uninstall") != std::wstring::npos ||
                    lower.find(L"remove") != std::wstring::npos ||
                    lower.find(L"setup") != std::wstring::npos ||
                    lower.find(L"readme") != std::wstring::npos ||
                    lower.find(L"help") != std::wstring::npos ||
                    lower.find(L"documentation") != std::wstring::npos) {
                    continue;
                }

                // Avoid duplicates
                bool exists = false;
                for (const auto& app : apps) {
                    if (_wcsicmp(app.name.c_str(), filename.c_str()) == 0) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    apps.push_back({ filename, entry.path().wstring() });
                }
            }
        }
    }
}

int main() {
    std::vector<AppItem> apps;

    wchar_t commonPath[MAX_PATH] = {};
    if (SHGetFolderPathW(nullptr, CSIDL_COMMON_PROGRAMS, nullptr, 0, commonPath) == S_OK) {
        ScanDirectory(commonPath, apps);
    }

    wchar_t userPath[MAX_PATH] = {};
    if (SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, userPath) == S_OK) {
        ScanDirectory(userPath, apps);
    }

    std::sort(apps.begin(), apps.end(), [](const AppItem& a, const AppItem& b) {
        return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
    });

    std::cout << "Found " << apps.size() << " apps:" << std::endl;
    for (size_t i = 0; i < std::min((size_t)15, apps.size()); ++i) {
        std::wcout << L"  - " << apps[i].name << L" (" << apps[i].path << L")" << std::endl;
    }
    return 0;
}
