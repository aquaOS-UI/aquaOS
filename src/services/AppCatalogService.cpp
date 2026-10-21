#define NOMINMAX
#include "AppCatalogService.h"
#include "IconManager.h"
#include <shlobj.h>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <thread>
#include <cwctype>

namespace fs = std::filesystem;

namespace Aqua {

    // Simple Recursive-Descent Expression Parser for Spotlight Math
    class MathParser {
        std::wstring m_src;
        size_t m_pos = 0;

        void SkipWhitespace() {
            while (m_pos < m_src.length() && iswspace(m_src[m_pos])) m_pos++;
        }

        double ParsePrimary() {
            SkipWhitespace();
            if (m_pos >= m_src.length()) return 0.0;

            if (m_src[m_pos] == L'-') {
                m_pos++;
                return -ParsePrimary();
            }
            if (m_src[m_pos] == L'+') {
                m_pos++;
                return ParsePrimary();
            }

            if (m_src[m_pos] == L'(') {
                m_pos++;
                double val = ParseExpression();
                SkipWhitespace();
                if (m_pos < m_src.length() && m_src[m_pos] == L')') m_pos++;
                return val;
            }

            if (iswalpha(m_src[m_pos])) {
                size_t start = m_pos;
                while (m_pos < m_src.length() && iswalpha(m_src[m_pos])) m_pos++;
                std::wstring fn = m_src.substr(start, m_pos - start);
                std::transform(fn.begin(), fn.end(), fn.begin(), ::towlower);

                if (fn == L"pi") return 3.14159265358979323846;
                if (fn == L"e") return 2.71828182845904523536;

                SkipWhitespace();
                if (m_pos < m_src.length() && m_src[m_pos] == L'(') {
                    m_pos++;
                    double arg = ParseExpression();
                    SkipWhitespace();
                    if (m_pos < m_src.length() && m_src[m_pos] == L')') m_pos++;

                    if (fn == L"sqrt" && arg >= 0.0) return std::sqrt(arg);
                    if (fn == L"sin") return std::sin(arg);
                    if (fn == L"cos") return std::cos(arg);
                    if (fn == L"tan") return std::tan(arg);
                    if (fn == L"abs") return std::abs(arg);
                    if (fn == L"log" && arg > 0.0) return std::log10(arg);
                    if (fn == L"ln" && arg > 0.0) return std::log(arg);
                }
                return 0.0;
            }

            size_t start = m_pos;
            while (m_pos < m_src.length() && (iswdigit(m_src[m_pos]) || m_src[m_pos] == L'.')) m_pos++;
            if (start == m_pos) return 0.0;
            return std::wcstod(m_src.c_str() + start, nullptr);
        }

        double ParseExponent() {
            double left = ParsePrimary();
            SkipWhitespace();
            while (m_pos < m_src.length() && m_src[m_pos] == L'^') {
                m_pos++;
                double right = ParseExponent();
                left = std::pow(left, right);
                SkipWhitespace();
            }
            return left;
        }

        double ParseTerm() {
            double left = ParseExponent();
            SkipWhitespace();
            while (m_pos < m_src.length() && (m_src[m_pos] == L'*' || m_src[m_pos] == L'/' || m_src[m_pos] == L'%')) {
                wchar_t op = m_src[m_pos++];
                double right = ParseExponent();
                if (op == L'*') left *= right;
                else if (op == L'/') left = (right != 0.0) ? (left / right) : 0.0;
                else if (op == L'%') left = std::fmod(left, (right != 0.0) ? right : 1.0);
                SkipWhitespace();
            }
            return left;
        }

    public:
        double ParseExpression() {
            double left = ParseTerm();
            SkipWhitespace();
            while (m_pos < m_src.length() && (m_src[m_pos] == L'+' || m_src[m_pos] == L'-')) {
                wchar_t op = m_src[m_pos++];
                double right = ParseTerm();
                if (op == L'+') left += right;
                else if (op == L'-') left -= right;
                SkipWhitespace();
            }
            return left;
        }

        bool Evaluate(const std::wstring& expr, double& result) {
            bool hasDigit = false;
            bool hasOp = false;
            for (wchar_t c : expr) {
                if (iswdigit(c)) hasDigit = true;
                if (c == L'+' || c == L'-' || c == L'*' || c == L'/' || c == L'^' || c == L'%' || c == L'(') hasOp = true;
            }
            if (!hasDigit || !hasOp) return false;

            m_src = expr;
            m_pos = 0;
            try {
                result = ParseExpression();
                SkipWhitespace();
                return (m_pos >= m_src.length()) && std::isfinite(result);
            } catch (...) {
                return false;
            }
        }
    };

    AppCatalogService& AppCatalogService::Instance() {
        static AppCatalogService s_instance;
        return s_instance;
    }

    AppCatalogService::AppCatalogService() = default;

    AppCatalogService::~AppCatalogService() {
        Shutdown();
    }

    void AppCatalogService::Initialize() {
        RefreshAsync();
    }

    void AppCatalogService::Shutdown() {
        m_shutdown.store(true);
    }

    void AppCatalogService::RefreshAsync() {
        if (m_scanning.exchange(true)) return;

        std::thread([this]() {
            RefreshSync();
            m_scanning.store(false);
        }).detach();
    }

    void AppCatalogService::ScanShortcutsInDirectory(const fs::path& dir, std::vector<CatalogItem>& items) {
        std::error_code ec;
        for (const auto& entry : fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec)) {
            if (m_shutdown.load()) return;
            if (ec) continue;

            if (entry.is_regular_file()) {
                auto ext = entry.path().extension().wstring();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
                if (ext == L".lnk") {
                    std::wstring filename = entry.path().stem().wstring();
                    std::wstring lower = filename;
                    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

                    // Skip uninstallers, setup files, docs, and licenses
                    if (lower.find(L"uninstall") != std::wstring::npos ||
                        lower.find(L"remove") != std::wstring::npos ||
                        lower.find(L"setup") != std::wstring::npos ||
                        lower.find(L"readme") != std::wstring::npos ||
                        lower.find(L"help") != std::wstring::npos ||
                        lower.find(L"documentation") != std::wstring::npos ||
                        lower.find(L"manual") != std::wstring::npos ||
                        lower.find(L"license") != std::wstring::npos) {
                        continue;
                    }

                    // Deduplicate
                    bool exists = false;
                    for (const auto& existing : items) {
                        if (_wcsicmp(existing.name.c_str(), filename.c_str()) == 0) {
                            exists = true;
                            break;
                        }
                    }

                    if (!exists) {
                        CatalogItem item;
                        item.name = filename;
                        item.description = L"Installed Application";
                        item.execPath = entry.path().wstring();
                        item.iconPath = entry.path().wstring();
                        item.type = CatalogItemType::Application;
                        item.accentColor = Colors::AccentBlue;
                        items.push_back(item);
                    }
                }
            }
        }
    }

    void AppCatalogService::ScanSystemTools(std::vector<CatalogItem>& items) {
        struct SysToolDef {
            const wchar_t* name;
            const wchar_t* desc;
            const wchar_t* exec;
            const wchar_t* args;
            D2D1_COLOR_F color;
        };

        const SysToolDef systemTools[] = {
            { L"Terminal",              L"Windows Terminal & PowerShell",       L"wt.exe",              L"", Colors::AccentGreen },
            { L"Command Prompt",        L"Windows Command Processor",          L"cmd.exe",             L"", Colors::AccentGreen },
            { L"PowerShell",            L"Windows PowerShell Shell",            L"powershell.exe",      L"", Colors::AccentGreen },
            { L"File Explorer",         L"Files, Folders & Drives",             L"explorer.exe",        L"", Colors::AccentBlue },
            { L"Task Manager",          L"System Activity & Performance",       L"taskmgr.exe",         L"", Colors::AccentOrange },
            { L"System Settings",       L"Windows OS Preferences & Control",    L"ms-settings:",        L"", Colors::AccentPurple },
            { L"Control Panel",         L"Legacy Windows Control Panel",        L"control.exe",         L"", Colors::AccentBlue },
            { L"Calculator",            L"Arithmetic & Scientific Tool",        L"calc.exe",            L"", Colors::AccentOrange },
            { L"Notepad",               L"Simple Text & Code Editor",           L"notepad.exe",         L"", Colors::AccentBlue },
            { L"Paint",                 L"Image & Graphics Drawing",            L"mspaint.exe",         L"", Colors::AccentPurple },
            { L"Snipping Tool",         L"Screen Capture & Annotation",         L"snippingtool.exe",    L"", Colors::AccentBlue },
            { L"Registry Editor",       L"Windows System Registry",             L"regedit.exe",         L"", Colors::AccentRed },
            { L"Device Manager",        L"Hardware Devices & Drivers",          L"devmgmt.msc",         L"", Colors::AccentBlue },
            { L"Disk Management",       L"Storage Partitions & Drives",         L"diskmgmt.msc",        L"", Colors::AccentBlue },
            { L"Services",              L"Windows Background Services",         L"services.msc",        L"", Colors::AccentBlue },
            { L"Event Viewer",          L"System Diagnostics & Logs",           L"eventvwr.msc",        L"", Colors::AccentBlue },
            { L"Resource Monitor",      L"Detailed CPU, Memory & Network",      L"resmon.exe",          L"", Colors::AccentOrange },
            { L"DirectX Diagnostic",    L"DirectX & Graphics Hardware Tool",    L"dxdiag.exe",          L"", Colors::AccentPurple },
            { L"Remote Desktop",        L"Connect to Remote PC / Server",       L"mstsc.exe",           L"", Colors::AccentBlue },
            { L"Character Map",         L"Unicode Glyphs & Symbols",            L"charmap.exe",         L"", Colors::AccentBlue },
            { L"Disk Cleanup",          L"Free Up Storage Space",               L"cleanmgr.exe",        L"", Colors::AccentBlue },
            { L"System Information",    L"Detailed Hardware & OS Spec",         L"msinfo32.exe",        L"", Colors::AccentBlue }
        };

        for (const auto& tool : systemTools) {
            bool found = false;
            for (auto& existing : items) {
                if (_wcsicmp(existing.name.c_str(), tool.name) == 0) {
                    found = true;
                    existing.type = CatalogItemType::SystemTool;
                    existing.description = tool.desc;
                    existing.accentColor = tool.color;
                    break;
                }
            }
            if (!found) {
                CatalogItem item;
                item.name = tool.name;
                item.description = tool.desc;
                item.execPath = tool.exec;
                item.execArgs = tool.args;
                item.iconPath = tool.exec;
                item.type = CatalogItemType::SystemTool;
                item.accentColor = tool.color;
                items.push_back(item);
            }
        }
    }

    void AppCatalogService::ScanKnownSettings(std::vector<CatalogItem>& items) {
        struct SettingDef {
            const wchar_t* name;
            const wchar_t* uri;
            const wchar_t* desc;
        };

        const SettingDef settings[] = {
            { L"Display Settings",           L"ms-settings:display",           L"Resolution, Scale & Refresh Rate" },
            { L"Sound Settings",             L"ms-settings:sound",             L"Output Devices & Volume Levels" },
            { L"Bluetooth & Devices",        L"ms-settings:bluetooth",         L"Manage Connected Peripherals" },
            { L"Wi-Fi Settings",             L"ms-settings:network-wifi",      L"Wireless Network Connections" },
            { L"Ethernet Settings",          L"ms-settings:network-ethernet",  L"Wired Network Adapter" },
            { L"Personalization Settings",   L"ms-settings:personalization",   L"Wallpaper, Colors & Themes" },
            { L"Installed Apps",             L"ms-settings:appsfeatures",      L"Manage Installed Software" },
            { L"Default Apps",               L"ms-settings:defaultapps",       L"Configure Default Handlers" },
            { L"Power & Battery Settings",   L"ms-settings:powersleep",        L"Power Modes & Sleep Timers" },
            { L"Windows Update",             L"ms-settings:windowsupdate",     L"OS Updates & Patches" }
        };

        for (const auto& s : settings) {
            CatalogItem item;
            item.name = s.name;
            item.description = s.desc;
            item.execPath = s.uri;
            item.iconPath = L"C:\\Windows\\ImmersiveControlPanel\\SystemSettings.exe";
            item.type = CatalogItemType::Setting;
            item.accentColor = Colors::AccentPurple;
            items.push_back(item);
        }
    }

    void AppCatalogService::ScanPathExecutables(std::vector<CatalogItem>& items) {
        struct PathToolDef {
            const wchar_t* name;
            const wchar_t* exec;
            const wchar_t* desc;
        };

        const PathToolDef pathTools[] = {
            { L"Visual Studio Code", L"code.exe",        L"Code & Script Editor" },
            { L"Visual Studio",      L"devenv.exe",      L"Enterprise IDE" },
            { L"Google Chrome",      L"chrome.exe",      L"Web Browser" },
            { L"Microsoft Edge",     L"msedge.exe",      L"Web Browser" },
            { L"Mozilla Firefox",    L"firefox.exe",     L"Web Browser" },
            { L"Spotify",            L"spotify.exe",     L"Music & Podcasts" },
            { L"Discord",            L"Discord.exe",     L"Voice & Text Chat" },
            { L"Steam",              L"steam.exe",       L"Gaming Client & Store" },
            { L"Git Bash",           L"git-bash.exe",    L"Git Terminal Shell" }
        };

        for (const auto& pt : pathTools) {
            wchar_t searched[MAX_PATH] = {};
            if (SearchPathW(nullptr, pt.exec, nullptr, MAX_PATH, searched, nullptr) > 0) {
                bool found = false;
                for (const auto& existing : items) {
                    if (_wcsicmp(existing.name.c_str(), pt.name) == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    CatalogItem item;
                    item.name = pt.name;
                    item.description = pt.desc;
                    item.execPath = searched;
                    item.iconPath = searched;
                    item.type = CatalogItemType::Application;
                    item.accentColor = Colors::AccentBlue;
                    items.push_back(item);
                }
            }
        }
    }

    void AppCatalogService::RefreshSync() {
        std::vector<CatalogItem> newItems;

        // 1. Common Programs Start Menu (ProgramData)
        wchar_t commonPath[MAX_PATH] = {};
        if (SHGetFolderPathW(nullptr, CSIDL_COMMON_PROGRAMS, nullptr, 0, commonPath) == S_OK) {
            ScanShortcutsInDirectory(commonPath, newItems);
        }

        // 2. User Programs Start Menu (AppData)
        wchar_t userPath[MAX_PATH] = {};
        if (SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, userPath) == S_OK) {
            ScanShortcutsInDirectory(userPath, newItems);
        }

        // 3. System Management Tools & Utilities
        ScanSystemTools(newItems);

        // 4. Windows Settings URIs
        ScanKnownSettings(newItems);

        // 5. Popular Executables located in System PATH
        ScanPathExecutables(newItems);

        // Sort alphabetically
        std::sort(newItems.begin(), newItems.end(), [](const CatalogItem& a, const CatalogItem& b) {
            return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
        });

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_catalog = std::move(newItems);
            m_ready.store(true);
        }
    }

    std::vector<CatalogItem> AppCatalogService::GetAllApplications() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<CatalogItem> apps;
        for (const auto& item : m_catalog) {
            if (item.type == CatalogItemType::Application || item.type == CatalogItemType::SystemTool) {
                apps.push_back(item);
            }
        }
        return apps;
    }

    static int ScoreMatch(const std::wstring& name, const std::wstring& desc, const std::wstring& query) {
        if (query.empty()) return 0;

        std::wstring nLower = name;
        std::wstring qLower = query;
        std::transform(nLower.begin(), nLower.end(), nLower.begin(), ::towlower);
        std::transform(qLower.begin(), qLower.end(), qLower.begin(), ::towlower);

        if (nLower == qLower) return 1000;
        if (nLower.rfind(qLower, 0) == 0) return 500;

        // Word boundary match
        size_t wb = nLower.find(L" " + qLower);
        if (wb != std::wstring::npos) return 400;

        // Substring in name
        size_t sub = nLower.find(qLower);
        if (sub != std::wstring::npos) return 250 - static_cast<int>(std::min<size_t>(sub * 2, 50));

        // Substring in description
        std::wstring dLower = desc;
        std::transform(dLower.begin(), dLower.end(), dLower.begin(), ::towlower);
        if (dLower.find(qLower) != std::wstring::npos) return 100;

        // Acronym / Subsequence match
        size_t qi = 0;
        for (size_t ni = 0; ni < nLower.length() && qi < qLower.length(); ++ni) {
            if (nLower[ni] == qLower[qi]) qi++;
        }
        if (qi == qLower.length()) return 60;

        return 0;
    }

    std::vector<CatalogItem> AppCatalogService::Search(const std::wstring& query, int maxResults) {
        std::vector<CatalogItem> results;

        std::wstring trimmed = query;
        // Trim leading and trailing spaces
        size_t first = trimmed.find_first_not_of(L" \t");
        if (first != std::wstring::npos) {
            size_t last = trimmed.find_last_not_of(L" \t");
            trimmed = trimmed.substr(first, (last - first + 1));
        } else {
            trimmed.clear();
        }

        // 1. Math Calculation Check
        if (!trimmed.empty()) {
            double mathVal = 0.0;
            MathParser parser;
            if (parser.Evaluate(trimmed, mathVal)) {
                wchar_t resBuf[64];
                swprintf_s(resBuf, L"= %.8g", mathVal);

                CatalogItem calcItem;
                calcItem.name = resBuf;
                calcItem.description = L"Calculation Result (Press Enter to Copy)";
                calcItem.execPath = L""; // Handled in Spotlight to copy to clipboard
                calcItem.type = CatalogItemType::Calculation;
                calcItem.accentColor = Colors::AccentOrange;
                calcItem.matchScore = 2000;
                results.push_back(calcItem);
            }
        }

        // 2. Direct Shell Command Check (e.g. "> ipconfig" or specific commands)
        if (!trimmed.empty()) {
            if (trimmed.rfind(L">", 0) == 0) {
                std::wstring cmd = trimmed.substr(1);
                while (!cmd.empty() && cmd[0] == L' ') cmd.erase(0, 1);
                if (!cmd.empty()) {
                    CatalogItem cmdItem;
                    cmdItem.name = cmd;
                    cmdItem.description = L"Execute Command in Terminal";
                    cmdItem.execPath = L"cmd.exe";
                    cmdItem.execArgs = L"/k " + cmd;
                    cmdItem.type = CatalogItemType::Command;
                    cmdItem.accentColor = Colors::AccentGreen;
                    cmdItem.matchScore = 1800;
                    results.push_back(cmdItem);
                }
            }
        }

        // 3. Web URL Detection
        if (!trimmed.empty()) {
            bool isUrl = (trimmed.rfind(L"http://", 0) == 0 ||
                          trimmed.rfind(L"https://", 0) == 0 ||
                          trimmed.rfind(L"www.", 0) == 0 ||
                          (trimmed.find(L".com") != std::wstring::npos && trimmed.find(L" ") == std::wstring::npos) ||
                          (trimmed.find(L".org") != std::wstring::npos && trimmed.find(L" ") == std::wstring::npos) ||
                          (trimmed.find(L".net") != std::wstring::npos && trimmed.find(L" ") == std::wstring::npos) ||
                          (trimmed.find(L".io") != std::wstring::npos && trimmed.find(L" ") == std::wstring::npos));

            if (isUrl) {
                std::wstring targetUrl = trimmed;
                if (targetUrl.rfind(L"http://", 0) != 0 && targetUrl.rfind(L"https://", 0) != 0) {
                    targetUrl = L"https://" + targetUrl;
                }
                CatalogItem urlItem;
                urlItem.name = targetUrl;
                urlItem.description = L"Open Web URL in Default Browser";
                urlItem.execPath = targetUrl;
                urlItem.type = CatalogItemType::WebUrl;
                urlItem.accentColor = Colors::AccentBlue;
                urlItem.matchScore = 1500;
                results.push_back(urlItem);
            }
        }

        // 4. File / Directory Path Detection
        if (!trimmed.empty()) {
            if ((trimmed.length() >= 2 && trimmed[1] == L':') ||
                trimmed.rfind(L"\\\\", 0) == 0 ||
                trimmed.rfind(L"%", 0) == 0) {
                std::wstring resolved = IconManager::Instance().ResolvePath(trimmed);
                std::error_code ec;
                if (fs::exists(resolved, ec)) {
                    CatalogItem fileItem;
                    fileItem.name = fs::path(resolved).filename().wstring();
                    if (fileItem.name.empty()) fileItem.name = resolved;
                    fileItem.description = resolved;
                    fileItem.execPath = resolved;
                    fileItem.iconPath = resolved;
                    fileItem.type = CatalogItemType::FileOrFolder;
                    fileItem.accentColor = Colors::AccentOrange;
                    fileItem.matchScore = 1400;
                    results.push_back(fileItem);
                }
            }
        }

        // 5. Query App Catalog
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (trimmed.empty()) {
                // Default Suggestions when query is empty: popular tools
                for (const auto& item : m_catalog) {
                    if (item.name == L"Terminal" ||
                        item.name == L"File Explorer" ||
                        item.name == L"System Settings" ||
                        item.name == L"Task Manager" ||
                        item.name == L"Calculator") {
                        results.push_back(item);
                        if (static_cast<int>(results.size()) >= maxResults) break;
                    }
                }
            } else {
                std::vector<CatalogItem> scoredMatches;
                for (auto item : m_catalog) {
                    int score = ScoreMatch(item.name, item.description, trimmed);
                    if (score > 0) {
                        item.matchScore = score;
                        scoredMatches.push_back(item);
                    }
                }

                std::sort(scoredMatches.begin(), scoredMatches.end(), [](const CatalogItem& a, const CatalogItem& b) {
                    return a.matchScore > b.matchScore;
                });

                for (const auto& m : scoredMatches) {
                    results.push_back(m);
                    if (static_cast<int>(results.size()) >= maxResults) break;
                }
            }
        }

        // 6. Web Search Fallback
        if (!trimmed.empty() && (results.empty() || results[0].type != CatalogItemType::Calculation)) {
            CatalogItem webItem;
            webItem.name = L"Search Web for \"" + trimmed + L"\"";
            webItem.description = L"Google Search in default browser";
            webItem.execPath = L"https://www.google.com/search?q=" + trimmed;
            webItem.type = CatalogItemType::WebSearch;
            webItem.accentColor = Colors::AccentBlue;
            webItem.matchScore = 10;
            results.push_back(webItem);
        }

        if (static_cast<int>(results.size()) > maxResults) {
            results.resize(maxResults);
        }

        return results;
    }

}
