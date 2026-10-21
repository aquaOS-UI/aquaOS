#pragma once
#include "Common.h"
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <filesystem>

namespace Aqua {

    enum class CatalogItemType {
        Application,
        SystemTool,
        Setting,
        Command,
        Calculation,
        FileOrFolder,
        WebSearch,
        WebUrl
    };

    struct CatalogItem {
        std::wstring name;
        std::wstring description;
        std::wstring execPath;
        std::wstring execArgs;
        std::wstring iconPath;
        CatalogItemType type = CatalogItemType::Application;
        D2D1_COLOR_F accentColor = Colors::AccentBlue;
        int matchScore = 0;
    };

    class AppCatalogService {
    public:
        static AppCatalogService& Instance();

        void Initialize();
        void Shutdown();
        void RefreshAsync();
        void RefreshSync();

        // Search catalog with ranking, math computation, URL & command resolution
        std::vector<CatalogItem> Search(const std::wstring& query, int maxResults = 8);

        // Retrieve all indexed applications for Launchpad
        std::vector<CatalogItem> GetAllApplications() const;

        bool IsReady() const { return m_ready.load(); }

    private:
        AppCatalogService();
        ~AppCatalogService();

        void ScanShortcutsInDirectory(const std::filesystem::path& dir, std::vector<CatalogItem>& items);
        void ScanSystemTools(std::vector<CatalogItem>& items);
        void ScanKnownSettings(std::vector<CatalogItem>& items);
        void ScanPathExecutables(std::vector<CatalogItem>& items);

        bool EvaluateMathExpression(const std::wstring& expr, double& outResult);

        mutable std::mutex m_mutex;
        std::atomic<bool> m_ready{ false };
        std::atomic<bool> m_scanning{ false };
        std::atomic<bool> m_shutdown{ false };
        std::vector<CatalogItem> m_catalog;
    };

}

