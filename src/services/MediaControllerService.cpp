#define NOMINMAX
#include "MediaControllerService.h"
#include <unknwn.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#include <wincodec.h>
#include <thread>
#include <chrono>
#include <algorithm>

#pragma comment(lib, "windowsapp.lib")

namespace Aqua {

    MediaControllerService& MediaControllerService::Instance() {
        static MediaControllerService s_instance;
        return s_instance;
    }

    MediaControllerService::MediaControllerService() = default;

    MediaControllerService::~MediaControllerService() {
        Shutdown();
    }

    void MediaControllerService::Initialize() {
        if (m_running.exchange(true)) return;

        std::thread([this]() {
            WorkerThreadLoop();
        }).detach();
    }

    void MediaControllerService::Shutdown() {
        m_running.store(false);
    }

    MediaTrackInfo MediaControllerService::GetCurrentTrack() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_currentTrack;
    }

    ComPtr<ID2D1Bitmap> MediaControllerService::GetThumbnailBitmap(ID2D1DeviceContext* ctx) {
        if (!ctx) return nullptr;

        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_currentTrack.trackId == m_cachedTrackId && m_cachedBitmap) {
            return m_cachedBitmap;
        }

        m_cachedBitmap = nullptr;
        m_cachedTrackId = m_currentTrack.trackId;

        if (m_currentTrack.thumbnailBytes.empty()) {
            return nullptr;
        }

        const auto& bytes = m_currentTrack.thumbnailBytes;
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes.size());
        if (!hMem) return nullptr;

        void* pMem = GlobalLock(hMem);
        if (!pMem) {
            GlobalFree(hMem);
            return nullptr;
        }

        memcpy(pMem, bytes.data(), bytes.size());
        GlobalUnlock(hMem);

        IStream* pStream = nullptr;
        if (FAILED(CreateStreamOnHGlobal(hMem, TRUE, &pStream)) || !pStream) {
            GlobalFree(hMem);
            return nullptr;
        }

        ComPtr<IWICImagingFactory> wicFactory;
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));
        if (SUCCEEDED(hr) && wicFactory) {
            ComPtr<IWICBitmapDecoder> decoder;
            hr = wicFactory->CreateDecoderFromStream(pStream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder);
            if (SUCCEEDED(hr) && decoder) {
                ComPtr<IWICBitmapFrameDecode> frame;
                hr = decoder->GetFrame(0, &frame);
                if (SUCCEEDED(hr) && frame) {
                    ComPtr<IWICFormatConverter> converter;
                    hr = wicFactory->CreateFormatConverter(&converter);
                    if (SUCCEEDED(hr) && converter) {
                        converter->Initialize(
                            frame.Get(),
                            GUID_WICPixelFormat32bppPBGRA,
                            WICBitmapDitherTypeNone,
                            nullptr,
                            0.0,
                            WICBitmapPaletteTypeCustom
                        );
                        ctx->CreateBitmapFromWicBitmap(converter.Get(), nullptr, &m_cachedBitmap);
                    }
                }
            }
        }

        pStream->Release();
        return m_cachedBitmap;
    }

    void MediaControllerService::TogglePlayPause() {
        keybd_event(VK_MEDIA_PLAY_PAUSE, 0, KEYEVENTF_EXTENDEDKEY, 0);
        keybd_event(VK_MEDIA_PLAY_PAUSE, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
    }

    void MediaControllerService::NextTrack() {
        keybd_event(VK_MEDIA_NEXT_TRACK, 0, KEYEVENTF_EXTENDEDKEY, 0);
        keybd_event(VK_MEDIA_NEXT_TRACK, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
    }

    void MediaControllerService::PreviousTrack() {
        keybd_event(VK_MEDIA_PREV_TRACK, 0, KEYEVENTF_EXTENDEDKEY, 0);
        keybd_event(VK_MEDIA_PREV_TRACK, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
    }

    void MediaControllerService::WorkerThreadLoop() {
        // Initialize WinRT on worker thread
        try {
            winrt::init_apartment();
        } catch (...) {}

        while (m_running.load()) {
            UpdateMediaSession();
            std::this_thread::sleep_for(std::chrono::milliseconds(600));
        }

        try {
            winrt::uninit_apartment();
        } catch (...) {}
    }

    void MediaControllerService::UpdateMediaSession() {
        try {
            using namespace winrt::Windows::Media::Control;

            auto asyncOp = GlobalSystemMediaTransportControlsSessionManager::RequestAsync();
            auto manager = asyncOp.get();
            if (!manager) return;

            auto session = manager.GetCurrentSession();
            if (session) {
                auto mediaProps = session.TryGetMediaPropertiesAsync().get();
                auto playbackInfo = session.GetPlaybackInfo();

                MediaTrackInfo info;
                info.title = mediaProps.Title().c_str();
                info.artist = mediaProps.Artist().c_str();
                info.album = mediaProps.AlbumTitle().c_str();

                std::wstring rawApp = session.SourceAppUserModelId().c_str();
                if (rawApp.find(L"Spotify") != std::wstring::npos) info.sourceApp = L"Spotify";
                else if (rawApp.find(L"Chrome") != std::wstring::npos) info.sourceApp = L"Google Chrome";
                else if (rawApp.find(L"Edge") != std::wstring::npos) info.sourceApp = L"Microsoft Edge";
                else if (rawApp.find(L"AppleMusic") != std::wstring::npos) info.sourceApp = L"Apple Music";
                else if (!rawApp.empty()) {
                    // Extract friendly app name if possible
                    size_t pos = rawApp.find_first_of(L"!_");
                    info.sourceApp = (pos != std::wstring::npos) ? rawApp.substr(0, pos) : rawApp;
                } else {
                    info.sourceApp = L"Media Player";
                }

                info.isPlaying = (playbackInfo && playbackInfo.PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing);
                info.hasMedia = !info.title.empty();

                // Thumbnail extraction
                auto thumbRef = mediaProps.Thumbnail();
                if (thumbRef) {
                    try {
                        auto stream = thumbRef.OpenReadAsync().get();
                        if (stream) {
                            uint64_t sz = stream.Size();
                            if (sz > 0 && sz < 8 * 1024 * 1024) {
                                winrt::Windows::Storage::Streams::Buffer buf(static_cast<uint32_t>(sz));
                                stream.ReadAsync(buf, static_cast<uint32_t>(sz), winrt::Windows::Storage::Streams::InputStreamOptions::None).get();
                                auto reader = winrt::Windows::Storage::Streams::DataReader::FromBuffer(buf);
                                info.thumbnailBytes.resize(buf.Length());
                                reader.ReadBytes(winrt::array_view<uint8_t>(info.thumbnailBytes));
                            }
                        }
                    } catch (...) {}
                }

                // Update thread-safe state
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    bool changed = (m_currentTrack.title != info.title ||
                                    m_currentTrack.artist != info.artist ||
                                    m_currentTrack.isPlaying != info.isPlaying ||
                                    m_currentTrack.hasMedia != info.hasMedia);

                    if (changed || m_currentTrack.trackId == 0) {
                        static uint64_t s_trackIdCounter = 1;
                        info.trackId = ++s_trackIdCounter;
                        m_currentTrack = std::move(info);
                    }
                }
            } else {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_currentTrack.hasMedia) {
                    m_currentTrack.hasMedia = false;
                    m_currentTrack.isPlaying = false;
                    m_currentTrack.title = L"No Media Playing";
                    m_currentTrack.artist = L"";
                    m_currentTrack.thumbnailBytes.clear();
                }
            }
        } catch (...) {
            // Gracefully ignore any WinRT transport exceptions
        }
    }

}
