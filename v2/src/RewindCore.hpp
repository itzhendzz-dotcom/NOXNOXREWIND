#pragma once

#include "TimelineBuffer.hpp"
#include "WorldState.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace noxxa {

class RewindAudio;

struct RewindSettings {
    float historySeconds{13.25f};
    int snapshotHz{24};
    float rewindSpeed{1.0f};
    float quickSeconds{3.0f};
    float maxRewindSeconds{13.003f};
    float radius{38.0f};
    int maxEntities{32};
    float rewindTimeScale{0.12f};
    bool restoreWorldHealth{false};
    bool haptics{true};
    bool safeStart{true};
    std::string poseAnim{"IDLE_TAXI"};
    std::string poseIfp{"PED"};
};

class RewindCore {
public:
    void Init(const RewindSettings& settings, RewindAudio* audio);
    void BeforeGameProcess();
    void Tick(bool holdDown, bool released, bool doubleTapped);

    bool IsRewinding() const { return m_rewinding; }
    bool IsQuickMode() const { return m_quickMode; }
    bool PlayerCanRewind() const { return m_world.PlayerCanAnchor(); }
    bool PlayerInVehicle() const { return m_world.IsPlayerInVehicle(); }

    float RewoundSeconds() const;
    float AvailableSeconds() const;
    float Progress01() const;
    float VisualIntensity() const { return m_visualIntensity; }
    std::size_t FrameCount() const { return m_timeline.Size(); }
    std::size_t CurrentEntityCount() const;

private:
    using Clock = std::chrono::steady_clock;

    void Record(double dt);
    bool BeginRewind(bool quickMode);
    void ProcessRewind(double dt, bool holdDown, bool released);
    void EndRewind(bool commit = true);
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
    bool m_rewinding{false};
    bool m_quickMode{false};
    bool m_justBegan{false};
    bool m_poseActive{false};
    bool m_audioActive{false};
};

} // namespace noxxa
