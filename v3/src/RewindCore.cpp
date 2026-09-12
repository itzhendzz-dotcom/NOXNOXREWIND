#include "RewindCore.hpp"
#include "RewindAudio.hpp"

#include <aml-psdk/game_sa/base/Timer.h>
#include <mod/amlmod.h>
#include <mod/logger.h>

#include <algorithm>
#include <cmath>

namespace noxxa {

void RewindCore::Init(const RewindSettings& s, RewindAudio* audio) {
    m_settings = s;
    m_settings.historySeconds = std::clamp(m_settings.historySeconds, 2.0f, 20.0f);
    m_settings.snapshotHz = std::clamp(m_settings.snapshotHz, 15, 45);
    m_settings.rewindSpeed = std::clamp(m_settings.rewindSpeed, 0.5f, 2.0f);
    m_settings.quickSeconds = std::clamp(m_settings.quickSeconds, 0.5f, m_settings.historySeconds);
    m_settings.maxRewindSeconds = std::clamp(m_settings.maxRewindSeconds, 0.5f, m_settings.historySeconds);
    m_settings.radius = std::clamp(m_settings.radius, 10.0f, 65.0f);
    m_settings.maxEntities = std::clamp(m_settings.maxEntities, 4, static_cast<int>(kHardMaxWorldEntities));
    // Causal mode intentionally permits zero. Timeline movement is based on
    // steady_clock, not GTA's scaled timer.
    m_settings.rewindTimeScale = std::clamp(m_settings.rewindTimeScale, 0.0f, 0.05f);
    m_settings.armFrames = std::clamp(m_settings.armFrames, 1, 5);
    m_settings.recoveryFrames = std::clamp(m_settings.recoveryFrames, 1, 6);

    m_timeline.SetCapacity(static_cast<std::size_t>(std::ceil(m_settings.historySeconds * m_settings.snapshotHz)) + 2);

    WorldCaptureSettings ws{};
    ws.radius = m_settings.radius;
    ws.maxEntities = static_cast<std::size_t>(m_settings.maxEntities);
    ws.restoreHealth = m_settings.restoreWorldHealth;
    m_world.Configure(ws);

    m_audio = audio;
    m_lastTick = Clock::now();
    logger->Info("V3.1 causal init: %.2fs @ %dHz, %.1fm, %d entities, simScale=%.3f, health=%d",
                 m_settings.maxRewindSeconds,
                 m_settings.snapshotHz,
                 m_settings.radius,
                 m_settings.maxEntities,
                 m_settings.rewindTimeScale,
                 m_settings.restoreWorldHealth ? 1 : 0);
}

void RewindCore::BeforeGameProcess() {
    if (m_phase != RewindPhase::Rewinding) return;

    // The key V3.1 semantic change: do not let GTA's present-time AI/physics
    // advance while historical playback is happening. The manual rewind cursor
    // still advances from real time in Tick().
    CTimer::ms_fTimeScale = m_settings.rewindTimeScale;
    m_world.QuiescePlayer();
    m_world.QuiesceWorld(m_timeline.Current());
}

void RewindCore::Tick(bool held, bool released, bool doubleTapped) {
    const auto now = Clock::now();
    double dt = std::chrono::duration<double>(now - m_lastTick).count();
    m_lastTick = now;
    dt = std::clamp(dt, 0.0, 0.100);

    if (!held && !m_quickMode) m_waitForRelease = false;

    switch (m_phase) {
        case RewindPhase::Idle:
            Record(dt);
            if (!m_waitForRelease) {
                if (doubleTapped) BeginRewind(true);
                else if (held) BeginRewind(false);
            }
            break;

        case RewindPhase::Arming:
            if (!m_quickMode && (released || !held)) {
                CancelArming();
                break;
            }
            if (++m_phaseFrames >= m_settings.armFrames) ActivateRewind();
            break;

        case RewindPhase::Rewinding:
            if (!m_anchor.valid || m_world.IsPlayerInVehicle()) {
                FinishRewind(false);
                break;
            }
            m_world.ApplyAnchor(m_anchor);
            ProcessRewind(dt, held, released);
            break;

        case RewindPhase::Recovering:
            if (++m_phaseFrames >= m_settings.recoveryFrames) {
                m_phase = RewindPhase::Idle;
                m_phaseFrames = 0;
                m_lastTick = Clock::now();
            }
            break;
    }

    UpdateVisualIntensity(dt);
    if (m_audio && m_audioActive) m_audio->SetIntensity(m_visualIntensity);
}

void RewindCore::Record(double dt) {
    const double period = 1.0 / static_cast<double>(m_settings.snapshotHz);
    m_recordAccumulator += dt;

    int captures = 0;
    while (m_recordAccumulator >= period && captures < 2) {
        WorldFrame f{};
        if (m_world.CaptureWorld(f, ++m_sequence)) m_timeline.Push(f);
        m_recordAccumulator -= period;
        ++captures;
    }
    if (captures == 2 && m_recordAccumulator > period * 2.0) m_recordAccumulator = 0.0;
}

bool RewindCore::BeginRewind(bool quick) {
    if (!m_world.PlayerCanAnchor() || m_timeline.Size() < 2) return false;

    WorldFrame present{};
    if (m_world.CaptureWorld(present, ++m_sequence)) m_timeline.Push(present);
    if (!m_timeline.BeginRewind()) return false;

    if (!m_world.CaptureAnchor(m_anchor)) {
        m_timeline.CancelRewind();
        return false;
    }

    m_savedTimeScale = CTimer::ms_fTimeScale;
    if (m_savedTimeScale <= 0.0f) m_savedTimeScale = 1.0f;

    m_phase = RewindPhase::Arming;
    m_phaseFrames = 0;
    m_quickMode = quick;
    m_rewindPhase = 0.0;
    m_activeRewindSeconds = 0.0;
    m_rewindStartCursor = m_timeline.Cursor();
    m_quickRemainingSeconds = quick ? m_settings.quickSeconds : 0.0;
    m_audioActive = false;
    m_poseActive = false;

    logger->Info("V3.1 causal rewind armed: quick=%d", quick ? 1 : 0);
    return true;
}

void RewindCore::ActivateRewind() {
    m_phase = RewindPhase::Rewinding;
    m_phaseFrames = 0;

    // Freeze normal simulation before audio or historical world mutation.
    CTimer::ms_fTimeScale = m_settings.rewindTimeScale;

    if (m_audio && m_audio->Ready()) {
        m_audio->StartRewind();
        m_audioActive = true;
    }
    if (m_settings.poseEnabled) {
        m_world.BeginAnchorPose(m_settings.poseAnim.c_str(), m_settings.poseIfp.c_str());
        m_poseActive = true;
    }
    if (m_settings.haptics && aml) aml->DoVibro(10);

    logger->Info("V3.1 historical playback active; GTA simulation scale=%.3f", m_settings.rewindTimeScale);
}

void RewindCore::CancelArming() {
    m_timeline.CancelRewind();
    m_anchor = {};
    m_phase = RewindPhase::Recovering;
    m_phaseFrames = 0;
    m_quickMode = false;
    m_rewindPhase = 0.0;
    m_activeRewindSeconds = 0.0;
    m_recordAccumulator = 0.0;
}

void RewindCore::ApplyInterpolatedCurrent() {
    const WorldFrame* newer = m_timeline.Current();
    const WorldFrame* older = m_timeline.PeekStepBack();
    if (!newer) return;

    if (older) {
        m_world.ApplyWorldInterpolated(*newer, *older, static_cast<float>(m_rewindPhase));
    }
    m_world.ApplyAnchor(m_anchor);
}

void RewindCore::ProcessRewind(double dt, bool held, bool released) {
    if (!m_quickMode && (released || !held)) {
        FinishRewind(true);
        return;
    }

    const double scaledDt = dt * static_cast<double>(m_settings.rewindSpeed);
    m_activeRewindSeconds += scaledDt;

    if (!m_quickMode && m_activeRewindSeconds >= m_settings.maxRewindSeconds) {
        FinishRewind(true);
        return;
    }

    if (m_quickMode) {
        m_quickRemainingSeconds -= scaledDt;
        if (m_quickRemainingSeconds <= 0.0) {
            FinishRewind(true);
            return;
        }
    }

    m_rewindPhase += scaledDt * static_cast<double>(m_settings.snapshotHz);
    int boundaries = 0;
    while (m_rewindPhase >= 1.0 && boundaries < 6) {
        if (!m_timeline.CanStepBack()) {
            m_rewindPhase = 0.0;
            FinishRewind(true);
            return;
        }
        m_timeline.StepBack();
        m_rewindPhase -= 1.0;
        ++boundaries;
    }

    if (!m_timeline.PeekStepBack()) {
        if (const WorldFrame* oldest = m_timeline.Current()) {
            m_world.ApplyWorldInterpolated(*oldest, *oldest, 1.0f);
        }
        m_world.ApplyAnchor(m_anchor);
        FinishRewind(true);
        return;
    }

    ApplyInterpolatedCurrent();
}

void RewindCore::FinishRewind(bool commit) {
    if (m_phase != RewindPhase::Rewinding && m_phase != RewindPhase::Arming) return;

    WorldFrame selectedPast{};
    bool haveSelectedPast = false;

    if (m_phase == RewindPhase::Rewinding) {
        // Materialise the exact partial/interpolated historical state first.
        ApplyInterpolatedCurrent();
        if (commit) {
            haveSelectedPast = m_world.CaptureWorld(selectedPast, ++m_sequence);
        }
    }

    if (commit) m_timeline.CommitRewind();
    else m_timeline.CancelRewind();

    // The selected past now becomes the new present. Resume GTA's simulation
    // only after the future event-response state has been reconciled.
    CTimer::ms_fTimeScale = m_savedTimeScale;

    int clearedFutureResponses = 0;
    if (commit && haveSelectedPast) {
        clearedFutureResponses = m_world.CommitCausalState(selectedPast);
    }

    if (m_anchor.valid) m_world.ApplyAnchor(m_anchor);

    if (m_poseActive) {
        m_world.EndAnchorPose();
        m_poseActive = false;
    }
    if (m_audio && m_audioActive) {
        m_audio->StopWithRelease();
        m_audioActive = false;
    }

    if (commit) {
        // Branch from the reconciled past, not from the discarded future.
        WorldFrame branch{};
        if (m_world.CaptureWorld(branch, ++m_sequence)) m_timeline.Push(branch);
    }

    if (m_settings.haptics && aml) aml->DoVibro(6);

    m_waitForRelease = !m_quickMode;
    m_phase = RewindPhase::Recovering;
    m_phaseFrames = 0;
    m_quickMode = false;
    m_rewindPhase = 0.0;
    m_activeRewindSeconds = 0.0;
    m_quickRemainingSeconds = 0.0;
    m_recordAccumulator = 0.0;
    m_anchor = {};

    logger->Info("V3.1 rewind finished: commit=%d, futureEventResponsesCleared=%d",
                 commit ? 1 : 0, clearedFutureResponses);
}

void RewindCore::UpdateVisualIntensity(double dt) {
    const float target = (m_phase == RewindPhase::Arming || m_phase == RewindPhase::Rewinding) ? 1.0f : 0.0f;
    const float speed = target > m_visualIntensity ? 7.5f : 5.0f;
    const float step = static_cast<float>(dt) * speed;

    if (m_visualIntensity < target) m_visualIntensity = std::min(target, m_visualIntensity + step);
    else if (m_visualIntensity > target) m_visualIntensity = std::max(target, m_visualIntensity - step);
}

float RewindCore::RewoundSeconds() const {
    if (m_phase != RewindPhase::Rewinding || m_rewindStartCursor < m_timeline.Cursor()) return 0.0f;
    return static_cast<float>(
        (static_cast<double>(m_rewindStartCursor - m_timeline.Cursor()) + m_rewindPhase) /
        static_cast<double>(m_settings.snapshotHz));
}

float RewindCore::AvailableSeconds() const {
    return m_timeline.Size() < 2
        ? 0.0f
        : static_cast<float>(m_timeline.Size() - 1) / static_cast<float>(m_settings.snapshotHz);
}

float RewindCore::Progress01() const {
    return std::clamp(RewoundSeconds() / std::max(0.001f, m_settings.maxRewindSeconds), 0.0f, 1.0f);
}

float RewindCore::Charge01() const {
    return std::clamp(AvailableSeconds() / std::max(0.001f, m_settings.maxRewindSeconds), 0.0f, 1.0f);
}

std::size_t RewindCore::CurrentEntityCount() const {
    if (m_phase == RewindPhase::Rewinding) {
        if (const WorldFrame* f = m_timeline.Current()) return f->count;
    }
    return m_timeline.Empty() ? 0 : m_timeline.Back().count;
}

} // namespace noxxa
