#pragma once
#include "Common.h"
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <memory>

namespace Aqua {

    struct MediaTrackInfo {
        std::wstring title = L"No Media Playing";
        std::wstring artist = L"";
        std::wstring album = L"";
        std::wstring sourceApp = L"";
        bool isPlaying = false;
        bool hasMedia = false;
        std::vector<uint8_t> thumbnailBytes;
        uint64_t trackId = 0; // Incremented on track change
    };

    class MediaControllerService {
    public:
        static MediaControllerService& Instance();

        void Initialize();
        void Shutdown();

        MediaTrackInfo GetCurrentTrack() const;
        ComPtr<ID2D1Bitmap> GetThumbnailBitmap(ID2D1DeviceContext* ctx);

        void TogglePlayPause();
        void NextTrack();
        void PreviousTrack();

    private:
        MediaControllerService();
        ~MediaControllerService();

        void WorkerThreadLoop();
        void UpdateMediaSession();

        mutable std::mutex m_mutex;
        std::atomic<bool> m_running{ false };
        MediaTrackInfo m_currentTrack;

        // Cached D2D Bitmap for album artwork
        uint64_t m_cachedTrackId = 0;
        ComPtr<ID2D1Bitmap> m_cachedBitmap;
    };

}
