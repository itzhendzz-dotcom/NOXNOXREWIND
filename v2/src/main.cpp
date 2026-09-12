#include <mod/amlmod.h>
#include <mod/logger.h>
#include <mod/config.h>

#include <aml-psdk/game_sa/Events.h>

#include "RewindAudio.hpp"
#include "RewindCore.hpp"
#include "RewindUI.hpp"
#include "TemporalFX.hpp"

#include <algorithm>
#include <string>

MYMODCFG(net.noxxa.rewind, NOXXA REWIND, 2.1.2, henn)
NEEDGAME(com.rockstargames.gtasa)

BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.3.0)
END_DEPLIST()

namespace {

noxxa::RewindAudio g_audio;
noxxa::RewindCore g_core;
noxxa::RewindUI g_ui;
noxxa::TemporalFX g_fx;

std::string JoinPath(const char* root, const char* suffix) {
    std::string out = root ? root : "";
    if (!out.empty() && out.back() != '/') out.push_back('/');
    out += suffix;
    return out;
}

} // namespace

ON_MOD_LOAD()
{
    logger->SetTag("NOXXA REWIND v2.1.2");

#ifndef AML32
    logger->Error("v2.1.2 currently targets GTA SA v2.00 / armeabi-v7a.");
    aml->ShowToast(true, "NOXXA REWIND v2.1.2: 32-bit GTA SA v2.00 required");
    return;
#endif

    const std::string assetDir = JoinPath(aml->GetAndroidDataRootPath(), "mods/NoxxaRewind");
    const std::string rewindWav = JoinPath(assetDir.c_str(), "rewind_user.wav");

    const bool audioReady = g_audio.Init(rewindWav);
    if (!audioReady) {
        logger->Error("User rewind audio failed to load: %s", rewindWav.c_str());
    }

    const float clipDuration = audioReady ? g_audio.DurationSeconds() : 13.003f;

    noxxa::RewindSettings settings{};
    settings.maxRewindSeconds = cfg->GetFloat("MaxRewindSeconds", clipDuration, "Rewind");
    settings.historySeconds = cfg->GetFloat("HistorySeconds", std::max(13.25f, settings.maxRewindSeconds + 0.25f), "Rewind");
    settings.snapshotHz = cfg->GetInt("SnapshotHz", 24, "Rewind");
    settings.rewindSpeed = cfg->GetFloat("RewindSpeed", 1.0f, "Rewind");
    settings.quickSeconds = cfg->GetFloat("QuickRewindSeconds", 3.0f, "Rewind");
    settings.radius = cfg->GetFloat("Radius", 38.0f, "World");
    settings.maxEntities = cfg->GetInt("MaxEntities", 32, "World");
    settings.rewindTimeScale = cfg->GetFloat("TimeScale", 0.12f, "World");
    settings.restoreWorldHealth = cfg->GetBool("RestoreHealth", false, "World");
    settings.haptics = cfg->GetBool("Haptics", false, "Effects");
    settings.safeStart = true; // lock safe-start on for this isolation build
    settings.poseAnim = cfg->GetString("PoseAnim", "IDLE_TAXI", "Anchor");
    settings.poseIfp = cfg->GetString("PoseIFP", "PED", "Anchor");

    const float buttonX = cfg->GetFloat("ButtonX", 0.88f, "UI");
    const float buttonY = cfg->GetFloat("ButtonY", 0.62f, "UI");
    const float buttonScale = cfg->GetFloat("ButtonScale", 1.0f, "UI");

    g_ui.Init(buttonX, buttonY, buttonScale);
    g_core.Init(settings, audioReady ? &g_audio : nullptr);

    Events::gameProcessEvent.before += []() { g_core.BeforeGameProcess(); };
    Events::gameProcessEvent.after += []() {
        const noxxa::RewindInput input = g_ui.PollInput();
        g_core.Tick(input.held, input.released, input.doubleTapped);
    };
    Events::touchScreenEvent.after += [](int actionType, int finger, int x, int y) {
        g_ui.OnTouch(actionType, finger, x, y);
    };
    Events::drawAfterFadeEvent.after += []() { g_fx.Draw(g_core); };
    Events::drawHudEvent.after += []() { g_ui.DrawButton(g_core); };

    logger->Info("Loaded v2.1.2 audio-only isolation: clip %.3fs, history %.2fs @ %dHz, rewind %.2fx",
                 clipDuration, settings.historySeconds, settings.snapshotHz, settings.rewindSpeed);
    aml->ShowToast(false, "NOXXA REWIND v2.1.2 loaded - 13s SFX sync");
}
