#pragma once

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace noxxa {

class RewindAudio {
public:
    ~RewindAudio();

    bool Init(const std::string& enterWav,
              const std::string& loopWav,
              const std::string& releaseWav);
    void StartRewind();
    void StopWithRelease();
    void Stop();
    void SetIntensity(float intensity01);
    bool Ready() const { return m_ready; }

private:
    struct PcmClip {
        uint32_t sampleRate{0};
        uint16_t channels{0};
        std::vector<int16_t> samples;
    };

    enum class Mode : uint8_t {
        Stopped,
        Rewinding,
        Release,
    };

    static bool LoadPcm16Wav(const std::string& path, PcmClip& out);
    static void BufferCallback(SLAndroidSimpleBufferQueueItf queue, void* context);
    void OnBufferFinished();
    bool Enqueue(const PcmClip& clip);
    void Destroy();

    PcmClip m_enter;
    PcmClip m_loop;
    PcmClip m_release;
    std::atomic<Mode> m_mode{Mode::Stopped};
    std::atomic<int> m_callbacksSinceStart{0};
    bool m_ready{false};

    SLObjectItf m_engineObject{nullptr};
    SLEngineItf m_engine{nullptr};
    SLObjectItf m_outputMixObject{nullptr};
    SLObjectItf m_playerObject{nullptr};
    SLPlayItf m_player{nullptr};
    SLVolumeItf m_volume{nullptr};
    SLAndroidSimpleBufferQueueItf m_queue{nullptr};
};

} // namespace noxxa
