#pragma once

#include "TimelineBuffer.hpp"
#include "WorldState.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace noxxa {
class RewindAudio;

enum class RewindPhase : uint8_t { Idle, Arming, Rewinding, Recovering };

struct RewindSettings {
    float historySeconds{13.25f};
    int snapshotHz{24};
    float rewindSpeed{1.0f};
    float quickSeconds{3.0f};
    float maxRewindSeconds{13.0f};
    float radius{35.0f};
    int maxEntities{28};
    // V3.1 causal playback freezes GTA simulation. Our rewind cursor still
    // moves from steady_clock real time, so zero here does not freeze rewind.
    float rewindTimeScale{0.0f};
    bool restoreWorldHealth{true};
    bool haptics{false};
    bool poseEnabled{false};
    int armFrames{2};
    int recoveryFrames{2};
    std::string poseAnim{"IDLE_TAXI"};
    std::string poseIfp{"PED"};
};

class RewindCore {
public:
    void Init(const RewindSettings& settings, RewindAudio* audio);
    void BeforeGameProcess();
    void Tick(bool holdDown, bool released, bool doubleTapped);

    RewindPhase Phase() const { return m_phase; }
    bool IsRewinding() const { return m_phase == RewindPhase::Arming || m_phase == RewindPhase::Rewinding; }
    bool IsWorldRewinding() const { return m_phase == RewindPhase::Rewinding; }
    bool IsQuickMode() const { return m_quickMode; }
    bool PlayerCanRewind() const { return m_world.PlayerCanAnchor(); }
    float RewoundSeconds() const;
    float AvailableSeconds() const;
    float MaxRewindSeconds() const { return m_settings.maxRewindSeconds; }
    float Progress01() const;
    float Charge01() const;
    float VisualIntensity() const { return m_visualIntensity; }
    std::size_t CurrentEntityCount() const;

    const WorldFrame* FxCurrentFrame() const {
        return m_phase == RewindPhase::Rewinding ? m_timeline.Current() : nullptr;
    }
    const WorldFrame* FxOlderFrame() const {
        return m_phase == RewindPhase::Rewinding ? m_timeline.PeekStepBack() : nullptr;
    }
    float FxInterpolation01() const {
        if (m_phase != RewindPhase::Rewinding) return 0.0f;
        if (m_rewindPhase <= 0.0) return 0.0f;
        if (m_rewindPhase >= 1.0) return 1.0f;
        return static_cast<float>(m_rewindPhase);
    }

private:
    using Clock = std::chrono::steady_clock;
    void Record(double dt);
    bool BeginRewind(bool quickMode);
    void ActivateRewind();
    void ProcessRewind(double dt, bool holdDown, bool released);
    void FinishRewind(bool commit);
    void CancelArming();
    void ApplyInterpolatedCurrent();
    void UpdateVisualIntensity(double dt);

    RewindSettings m_settings{};
    TimelineBuffer<WorldFrame> m_timeline{322};
    WorldStateAdapter m_world{};
    RewindAudio* m_audio{nullptr};
    PlayerAnchor m_anchor{};
    Clock::time_point m_lastTick{};
    double m_recordAccumulator{0.0};
    double m_rewindPhase{0.0};
    double m_activeRewindSeconds{0.0};
    uint64_t m_sequence{0};
    std::size_t m_rewindStartCursor{0};
    double m_quickRemainingSeconds{0.0};
    float m_savedTimeScale{1.0f};
    float m_visualIntensity{0.0f};
    RewindPhase m_phase{RewindPhase::Idle};
    int m_phaseFrames{0};
    bool m_quickMode{false};
    bool m_audioActive{false};
    bool m_poseActive{false};
    bool m_waitForRelease{false};
};

} // namespace noxxa
