#include "RewindAudio.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

namespace noxxa {
namespace {

uint16_t ReadU16(std::ifstream& f) {
    uint8_t b[2]{};
    f.read(reinterpret_cast<char*>(b), 2);
    return static_cast<uint16_t>(b[0] | (b[1] << 8));
}

uint32_t ReadU32(std::ifstream& f) {
    uint8_t b[4]{};
    f.read(reinterpret_cast<char*>(b), 4);
    return static_cast<uint32_t>(b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24));
}

} // namespace

RewindAudio::~RewindAudio() {
    Destroy();
}

bool RewindAudio::LoadPcm16Wav(const std::string& path, PcmClip& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    char riff[4]{}, wave[4]{};
    f.read(riff, 4);
    (void)ReadU32(f);
    f.read(wave, 4);
    if (std::memcmp(riff, "RIFF", 4) || std::memcmp(wave, "WAVE", 4)) return false;

    uint16_t format = 0, channels = 0, bits = 0;
    uint32_t sampleRate = 0;
    std::vector<uint8_t> pcmBytes;

    while (f && (!format || pcmBytes.empty())) {
        char id[4]{};
        f.read(id, 4);
        if (!f) break;
        const uint32_t size = ReadU32(f);

        if (!std::memcmp(id, "fmt ", 4)) {
            format = ReadU16(f);
            channels = ReadU16(f);
            sampleRate = ReadU32(f);
            (void)ReadU32(f);
            (void)ReadU16(f);
            bits = ReadU16(f);
            if (size > 16) f.seekg(size - 16, std::ios::cur);
        } else if (!std::memcmp(id, "data", 4)) {
            pcmBytes.resize(size);
            f.read(reinterpret_cast<char*>(pcmBytes.data()), size);
        } else {
            f.seekg(size, std::ios::cur);
        }
        if (size & 1) f.seekg(1, std::ios::cur);
    }

    if (format != 1 || bits != 16 || (channels != 1 && channels != 2) || sampleRate == 0 || pcmBytes.empty()) {
        return false;
    }

    out.sampleRate = sampleRate;
    out.channels = channels;
    out.samples.resize(pcmBytes.size() / sizeof(int16_t));
    std::memcpy(out.samples.data(), pcmBytes.data(), pcmBytes.size());
    return true;
}

bool RewindAudio::Init(const std::string& enterWav,
                       const std::string& loopWav,
                       const std::string& releaseWav) {
    Destroy();
    if (!LoadPcm16Wav(enterWav, m_enter) ||
        !LoadPcm16Wav(loopWav, m_loop) ||
        !LoadPcm16Wav(releaseWav, m_release)) {
        return false;
    }

    if (m_enter.sampleRate != m_loop.sampleRate ||
        m_enter.sampleRate != m_release.sampleRate ||
        m_enter.channels != m_loop.channels ||
        m_enter.channels != m_release.channels) {
        return false;
    }

    if (slCreateEngine(&m_engineObject, 0, nullptr, 0, nullptr, nullptr) != SL_RESULT_SUCCESS) return false;
    if ((*m_engineObject)->Realize(m_engineObject, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS) return false;
    if ((*m_engineObject)->GetInterface(m_engineObject, SL_IID_ENGINE, &m_engine) != SL_RESULT_SUCCESS) return false;

    if ((*m_engine)->CreateOutputMix(m_engine, &m_outputMixObject, 0, nullptr, nullptr) != SL_RESULT_SUCCESS) return false;
    if ((*m_outputMixObject)->Realize(m_outputMixObject, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS) return false;

    SLDataLocator_AndroidSimpleBufferQueue locQueue{SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 3};
    const SLuint32 speakerMask = m_loop.channels == 2
        ? (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT)
        : SL_SPEAKER_FRONT_CENTER;
    SLDataFormat_PCM pcm{
        SL_DATAFORMAT_PCM,
        m_loop.channels,
        m_loop.sampleRate * 1000,
        SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_PCMSAMPLEFORMAT_FIXED_16,
        speakerMask,
        SL_BYTEORDER_LITTLEENDIAN
    };
    SLDataSource source{&locQueue, &pcm};
    SLDataLocator_OutputMix locOut{SL_DATALOCATOR_OUTPUTMIX, m_outputMixObject};
    SLDataSink sink{&locOut, nullptr};

    const SLInterfaceID ids[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_VOLUME};
    const SLboolean required[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_FALSE};
    if ((*m_engine)->CreateAudioPlayer(m_engine, &m_playerObject, &source, &sink, 2, ids, required) != SL_RESULT_SUCCESS) return false;
    if ((*m_playerObject)->Realize(m_playerObject, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS) return false;
    if ((*m_playerObject)->GetInterface(m_playerObject, SL_IID_PLAY, &m_player) != SL_RESULT_SUCCESS) return false;
    if ((*m_playerObject)->GetInterface(m_playerObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &m_queue) != SL_RESULT_SUCCESS) return false;
    (void)(*m_playerObject)->GetInterface(m_playerObject, SL_IID_VOLUME, &m_volume);
    if ((*m_queue)->RegisterCallback(m_queue, BufferCallback, this) != SL_RESULT_SUCCESS) return false;

    m_ready = true;
    SetIntensity(0.0f);
    return true;
}

void RewindAudio::BufferCallback(SLAndroidSimpleBufferQueueItf, void* context) {
    static_cast<RewindAudio*>(context)->OnBufferFinished();
}

void RewindAudio::OnBufferFinished() {
    if (m_mode.load(std::memory_order_relaxed) == Mode::Rewinding) {
        m_callbacksSinceStart.fetch_add(1, std::memory_order_relaxed);
        Enqueue(m_loop);
    }
}

bool RewindAudio::Enqueue(const PcmClip& clip) {
    if (!m_queue || clip.samples.empty()) return false;
    return (*m_queue)->Enqueue(m_queue,
                               clip.samples.data(),
                               clip.samples.size() * sizeof(int16_t)) == SL_RESULT_SUCCESS;
}

void RewindAudio::StartRewind() {
    if (!m_ready) return;
    m_mode.store(Mode::Stopped, std::memory_order_relaxed);
    (*m_queue)->Clear(m_queue);
    m_callbacksSinceStart.store(0, std::memory_order_relaxed);
    Enqueue(m_enter);
    Enqueue(m_loop);
    m_mode.store(Mode::Rewinding, std::memory_order_relaxed);
    (*m_player)->SetPlayState(m_player, SL_PLAYSTATE_PLAYING);
}

void RewindAudio::StopWithRelease() {
    if (!m_ready) return;
    m_mode.store(Mode::Release, std::memory_order_relaxed);
    (*m_queue)->Clear(m_queue);
    Enqueue(m_release);
    (*m_player)->SetPlayState(m_player, SL_PLAYSTATE_PLAYING);
}

void RewindAudio::Stop() {
    if (!m_ready) return;
    m_mode.store(Mode::Stopped, std::memory_order_relaxed);
    (*m_queue)->Clear(m_queue);
    (*m_player)->SetPlayState(m_player, SL_PLAYSTATE_STOPPED);
}

void RewindAudio::SetIntensity(float intensity01) {
    if (!m_ready || !m_volume) return;
    intensity01 = std::clamp(intensity01, 0.0f, 1.0f);
    const SLmillibel level = static_cast<SLmillibel>(-650.0f + intensity01 * 500.0f);
    (*m_volume)->SetVolumeLevel(m_volume, level);
}

void RewindAudio::Destroy() {
    m_ready = false;
    m_mode.store(Mode::Stopped, std::memory_order_relaxed);
    if (m_playerObject) {
        (*m_playerObject)->Destroy(m_playerObject);
        m_playerObject = nullptr;
    }
    if (m_outputMixObject) {
        (*m_outputMixObject)->Destroy(m_outputMixObject);
        m_outputMixObject = nullptr;
    }
    if (m_engineObject) {
        (*m_engineObject)->Destroy(m_engineObject);
        m_engineObject = nullptr;
    }
    m_player = nullptr;
    m_volume = nullptr;
    m_queue = nullptr;
    m_engine = nullptr;
}

} // namespace noxxa
