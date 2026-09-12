#include "RewindCore.hpp"
#include "RewindAudio.hpp"

#include <aml-psdk/game_sa/base/Timer.h>
#include <aml-psdk/game_sa/other/Pools.h>
#include <mod/amlmod.h>
#include <mod/logger.h>

#include <algorithm>
#include <cmath>

namespace noxxa {

void RewindCore::Init(const RewindSettings& settings, RewindAudio* audio) {
    m_settings = settings;
    m_settings.historySeconds = std::clamp(m_settings.historySeconds, 2.0f, 12.0f);
    m_settings.snapshotHz = std::clamp(m_settings.snapshotHz, 15, 45);
    m_settings.rewindSpeed = std::clamp(m_settings.rewindSpeed, 0.5f, 2.5f);
    m_settings.quickSeconds = std::clamp(m_settings.quickSeconds, 0.5f, m_settings.historySeconds);
    m_settings.radius = std::clamp(m_settings.radius, 10.0f, 75.0f);
    m_settings.maxEntities = std::clamp(m_settings.maxEntities, 4, static_cast<int>(kHardMaxWorldEntities));
    m_settings.rewindTimeScale = std::clamp(m_settings.rewindTimeScale, 0.08f, 0.35f);

    const auto capacity = static_cast<std::size_t>(std::ceil(m_settings.historySeconds * m_settings.snapshotHz)) + 2;
    m_timeline.SetCapacity(capacity);

    WorldCaptureSettings worldSettings{};
    worldSettings.radius = m_settings.radius;
    worldSettings.maxEntities = static_cast<std::size_t>(m_settings.maxEntities);
    worldSettings.restoreHealth = m_settings.restoreWorldHealth;
    m_world.Configure(worldSettings);

    m_audio = audio;
    m_lastTick = Clock::now();
    logger->Info("SAFE START=%d", m_settings.safeStart ? 1 : 0);
}

void RewindCore::BeforeGameProcess() {
    if (!m_rewinding || m_justBegan) return;
    CTimer::ms_fTimeScale = m_settings.rewindTimeScale;
    m_world.QuiescePlayer();
    m_world.QuiesceWorld(m_timeline.Current());
}

void RewindCore::Tick(bool holdDown, bool released, bool doubleTapped) {
    const auto now = Clock::now();
    double dt = std::chrono::duration<double>(now - m_lastTick).count();
    m_lastTick = now;
    dt = std::clamp(dt, 0.0, 0.100);

    if (!m_rewinding) {
        Record(dt);
        if (doubleTapped) BeginRewind(true);
        else if (holdDown) BeginRewind(false);
    }

    if (m_rewinding) {
        if (m_justBegan) {
            // Critical safety barrier: do not mutate the ped, world, audio queue,
            // animation task tree, or time scale in the same frame as touch input.
            m_justBegan = false;
            logger->Info("BEGIN stage 6: armed; first apply deferred to next frame");
        } else if (!m_anchor.valid || m_world.IsPlayerInVehicle()) {
            EndRewind(false);
        } else {
            m_world.ApplyAnchor(m_anchor);
            ProcessRewind(dt, holdDown, released);
        }
    }

    UpdateVisualIntensity(dt);
    if (m_audio && m_audioActive) m_audio->SetIntensity(m_visualIntensity);
}

void RewindCore::Record(double dt) {
    const double period = 1.0 / static_cast<double>(m_settings.snapshotHz);
    m_recordAccumulator += dt;

    int captures = 0;
    while (m_recordAccumulator >= period && captures < 2) {
        WorldFrame frame{};
        if (m_world.CaptureWorld(frame, ++m_sequence)) m_timeline.Push(frame);
        m_recordAccumulator -= period;
        ++captures;
    }
    if (captures == 2 && m_recordAccumulator > period * 2.0) m_recordAccumulator = 0.0;
}

bool RewindCore::BeginRewind(bool quickMode) {
    logger->Info("BEGIN stage 1: request");
    if (!m_world.PlayerCanAnchor()) {
        logger->Info("BEGIN cancelled: player cannot anchor");
        return false;
    }

    logger->Info("BEGIN stage 2: capture present");
    WorldFrame present{};
    if (m_world.CaptureWorld(present, ++m_sequence)) m_timeline.Push(present);

    logger->Info("BEGIN stage 3: open timeline cursor");
    if (!m_timeline.BeginRewind()) {
        logger->Info("BEGIN cancelled: not enough history");
        return false;
    }

    logger->Info("BEGIN stage 4: capture player anchor");
    if (!m_world.CaptureAnchor(m_anchor)) {
        m_timeline.CancelRewind();
        logger->Info("BEGIN cancelled: anchor capture failed");
        return false;
    }

    m_savedTimeScale = CTimer::ms_fTimeScale;
    if (m_savedTimeScale <= 0.0f) m_savedTimeScale = 1.0f;

    m_rewinding = true;
    m_quickMode = quickMode;
    m_rewindPhase = 0.0;
    m_rewindStartCursor = m_timeline.Cursor();
    m_quickRemainingSeconds = quickMode ? m_settings.quickSeconds : 0.0;
    m_justBegan = true;
    m_poseActive = false;
    m_audioActive = false;

    logger->Info("BEGIN stage 5: core state armed");

    if (!m_settings.safeStart) {
        // Experimental/full path only. SafeStart deliberately avoids all APIs
        // that may free task/audio internals on the touch frame.
        m_world.BeginAnchorPose(m_settings.poseAnim.c_str(), m_settings.poseIfp.c_str());
        m_poseActive = true;
        if (m_audio) {
            m_audio->StartRewind();
            m_audioActive = true;
        }
        if (m_settings.haptics && aml) aml->DoVibro(14);
    }

    return true;
}

void RewindCore::ApplyInterpolatedCurrent() {
    const WorldFrame* newer = m_timeline.Current();
    const WorldFrame* older = m_timeline.PeekStepBack();
    if (!newer) return;
    if (older) m_world.ApplyWorldInterpolated(*newer, *older, static_cast<float>(m_rewindPhase));
    m_world.ApplyAnchor(m_anchor);
}

void RewindCore::ProcessRewind(double dt, bool holdDown, bool released) {
    if (!m_quickMode && (released || !holdDown)) {
        EndRewind(true);
        return;
    }

    if (m_quickMode) {
        m_quickRemainingSeconds -= dt * static_cast<double>(m_settings.rewindSpeed);
        if (m_quickRemainingSeconds <= 0.0) {
            EndRewind(true);
            return;
        }
    }

    const double fps = static_cast<double>(m_settings.snapshotHz) * static_cast<double>(m_settings.rewindSpeed);
    m_rewindPhase += dt * fps;

    int boundaries = 0;
    while (m_rewindPhase >= 1.0 && boundaries < 6) {
        if (!m_timeline.CanStepBack()) {
            m_rewindPhase = 0.0;
            EndRewind(true);
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
        EndRewind(true);
        return;
    }

    ApplyInterpolatedCurrent();
}

void RewindCore::EndRewind(bool commit) {
    if (!m_rewinding) return;
    logger->Info("END stage 1: commit=%d", commit ? 1 : 0);

    if (!m_justBegan) ApplyInterpolatedCurrent();
    if (commit) m_timeline.CommitRewind();
    else m_timeline.CancelRewind();

    if (commit) {
        WorldFrame branch{};
        if (m_world.CaptureWorld(branch, ++m_sequence)) m_timeline.Push(branch);
    }

    CTimer::ms_fTimeScale = m_savedTimeScale;
    if (m_anchor.valid) {
        m_world.ApplyAnchor(m_anchor);
        if (CPed* ped = CPools::GetPed(m_anchor.pedRef)) ped->bUsesCollision = m_anchor.collisionEnabled;
    }

    if (m_poseActive) {
        m_world.EndAnchorPose();
        m_poseActive = false;
    }

    m_rewinding = false;
    m_quickMode = false;
    m_justBegan = false;
    m_rewindPhase = 0.0;
    m_quickRemainingSeconds = 0.0;
    m_recordAccumulator = 0.0;
    m_anchor = {};
    m_lastTick = Clock::now();

    if (m_audio && m_audioActive) {
        m_audio->StopWithRelease();
        m_audioActive = false;
    }
    if (!m_settings.safeStart && m_settings.haptics && aml) aml->DoVibro(8);
    logger->Info("END stage 2: complete");
}

void RewindCore::UpdateVisualIntensity(double dt) {
    const float target = m_rewinding ? 1.0f : 0.0f;
    const float speed = m_rewinding ? 6.0f : 5.5f;
    const float step = static_cast<float>(dt) * speed;
    if (m_visualIntensity < target) m_visualIntensity = std::min(target, m_visualIntensity + step);
    else if (m_visualIntensity > target) m_visualIntensity = std::max(target, m_visualIntensity - step);
}

float RewindCore::RewoundSeconds() const {
    if (!m_rewinding || m_rewindStartCursor < m_timeline.Cursor()) return 0.0f;
    const double frames = static_cast<double>(m_rewindStartCursor - m_timeline.Cursor()) + m_rewindPhase;
    return static_cast<float>(frames / static_cast<double>(m_settings.snapshotHz));
}

float RewindCore::AvailableSeconds() const {
    if (m_timeline.Size() < 2) return 0.0f;
    return static_cast<float>(m_timeline.Size() - 1) / static_cast<float>(m_settings.snapshotHz);
}

float RewindCore::Progress01() const {
    const float available = AvailableSeconds();
    if (available <= 0.001f) return 0.0f;
    return std::clamp(RewoundSeconds() / available, 0.0f, 1.0f);
}

std::size_t RewindCore::CurrentEntityCount() const {
    if (m_rewinding) {
        if (const WorldFrame* f = m_timeline.Current()) return f->count;
        return 0;
    }
    return m_timeline.Empty() ? 0 : m_timeline.Back().count;
}

} // namespace noxxa
