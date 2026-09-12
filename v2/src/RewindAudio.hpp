#pragma once

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <cstdint>
#include <string>
#include <vector>

namespace noxxa {

class RewindAudio {
public:
    ~RewindAudio();

    // v2.1.2 uses one user-supplied rewind clip. The clip starts when rewind
    // becomes active and is stopped immediately when rewind is released.
    bool Init(const std::string& rewindWav);
    void StartRewind();
    void StopWithRelease();
    void Stop();
    void SetIntensity(float intensity01);
    bool Ready() const { return m_ready; }
    float DurationSeconds() const;

private:
    struct PcmClip {
        uint32_t sampleRate{0};
        uint16_t channels{0};
        std::vector<int16_t> samples;
    };

    static bool LoadPcm16Wav(const std::string& path, PcmClip& out);
    bool Enqueue(const PcmClip& clip);
    void Destroy();

    PcmClip m_rewind;
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
