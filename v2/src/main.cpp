#include <mod/amlmod.h>
#include <mod/logger.h>
#include <mod/config.h>

#include <aml-psdk/game_sa/Events.h>

#include "RewindAudio.hpp"
#include "RewindCore.hpp"
#include "RewindUI.hpp"
#include "TemporalFX.hpp"

#include <string>

MYMODCFG(net.noxxa.rewind, NOXXA REWIND, 2.1.1, henn)
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
    logger->SetTag("NOXXA REWIND v2.1.1");

#ifndef AML32
    logger->Error("v2.1.1 currently targets GTA SA v2.00 / armeabi-v7a.");
    aml->ShowToast(true, "NOXXA REWIND v2.1.1: 32-bit GTA SA v2.00 required");
    return;
#endif

    noxxa::RewindSettings settings{};
    settings.historySeconds = cfg->GetFloat("HistorySeconds", 8.0f, "Rewind");
    settings.snapshotHz = cfg->GetInt("SnapshotHz", 24, "Rewind");
    settings.rewindSpeed = cfg->GetFloat("RewindSpeed", 1.25f, "Rewind");
    settings.quickSeconds = cfg->GetFloat("QuickRewindSeconds", 3.0f, "Rewind");
    settings.radius = cfg->GetFloat("Radius", 38.0f, "World");
    settings.maxEntities = cfg->GetInt("MaxEntities", 32, "World");
    settings.rewindTimeScale = cfg->GetFloat("TimeScale", 0.12f, "World");
    settings.restoreWorldHealth = cfg->GetBool("RestoreHealth", false, "World");
    settings.haptics = cfg->GetBool("Haptics", true, "Effects");
    settings.safeStart = cfg->GetBool("SafeStart", true, "Debug");
    settings.poseAnim = cfg->GetString("PoseAnim", "IDLE_TAXI", "Anchor");
    settings.poseIfp = cfg->GetString("PoseIFP", "PED", "Anchor");

    const float buttonX = cfg->GetFloat("ButtonX", 0.88f, "UI");
    const float buttonY = cfg->GetFloat("ButtonY", 0.62f, "UI");
    const float buttonScale = cfg->GetFloat("ButtonScale", 1.0f, "UI");

    const std::string assetDir = JoinPath(aml->GetAndroidDataRootPath(), "mods/NoxxaRewind");
    const std::string enterWav = JoinPath(assetDir.c_str(), "rewind_enter.wav");
    const std::string bedWav = JoinPath(assetDir.c_str(), "rewind_loop.wav");
    const std::string releaseWav = JoinPath(assetDir.c_str(), "rewind_release.wav");

    if (!g_audio.Init(enterWav, bedWav, releaseWav)) {
        logger->Error("Temporal audio failed to load. Rewind core will continue without it.");
    }

    g_ui.Init(buttonX, buttonY, buttonScale);
    g_core.Init(settings, &g_audio);

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

    logger->Info("Loaded v2.1.1 safe-start build: %.1fs @ %dHz, radius %.1fm, max %d entities, safe=%d",
                 settings.historySeconds, settings.snapshotHz, settings.radius, settings.maxEntities,
                 settings.safeStart ? 1 : 0);
    aml->ShowToast(false, "NOXXA REWIND v2.1.1 loaded - safe start ON");
}
