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

MYMODCFG(net.noxxa.rewind, NOXXA REWIND, 3.0.0, henn)
NEEDGAME(com.rockstargames.gtasa)
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.3.0)
END_DEPLIST()

namespace {
noxxa::RewindAudio g_audio;
noxxa::RewindCore g_core;
noxxa::RewindUI g_ui;
noxxa::TemporalFX g_fx;
std::string JoinPath(const char* root,const char* suffix){std::string o=root?root:"";if(!o.empty()&&o.back()!='/')o.push_back('/');o+=suffix;return o;}
}

ON_MOD_LOAD() {
    logger->SetTag("NOXXA REWIND v3");
#ifndef AML32
    logger->Error("v3.0 currently targets GTA SA v2.00 / armeabi-v7a.");
    aml->ShowToast(true,"NOXXA REWIND v3: 32-bit GTA SA v2.00 required");
    return;
#endif
    const std::string assetDir=JoinPath(aml->GetAndroidDataRootPath(),"mods/NoxxaRewind");
    const std::string rewindWav=JoinPath(assetDir.c_str(),"rewind_user.wav");
    const bool audioReady=g_audio.Init(rewindWav);
    if(!audioReady) logger->Error("rewind_user.wav failed to load; rewind remains usable without audio");

    noxxa::RewindSettings s{};
    s.maxRewindSeconds=cfg->GetFloat("MaxRewindSeconds",13.0f,"Rewind");
    s.historySeconds=cfg->GetFloat("HistorySeconds",13.35f,"Rewind");
    s.snapshotHz=cfg->GetInt("SnapshotHz",24,"Rewind");
    s.rewindSpeed=cfg->GetFloat("RewindSpeed",1.0f,"Rewind");
    s.quickSeconds=cfg->GetFloat("QuickRewindSeconds",3.0f,"Rewind");
    s.radius=cfg->GetFloat("Radius",35.0f,"World");
    s.maxEntities=cfg->GetInt("MaxEntities",28,"World");
    s.rewindTimeScale=cfg->GetFloat("TimeScale",0.10f,"World");
    s.restoreWorldHealth=cfg->GetBool("RestoreHealth",false,"World");
    s.armFrames=cfg->GetInt("ArmFrames",2,"Safety");
    s.recoveryFrames=cfg->GetInt("RecoveryFrames",2,"Safety");
    s.poseEnabled=cfg->GetBool("PoseEnabled",false,"Anchor");
    s.poseAnim=cfg->GetString("PoseAnim","IDLE_TAXI","Anchor");
    s.poseIfp=cfg->GetString("PoseIFP","PED","Anchor");
    s.haptics=cfg->GetBool("Haptics",false,"Effects");

    const float bx=cfg->GetFloat("ButtonX",0.060f,"UI");
    const float by=cfg->GetFloat("ButtonY",0.105f,"UI");
    const float bs=cfg->GetFloat("ButtonScale",0.88f,"UI");
    const float fx=cfg->GetFloat("Strength",1.0f,"Effects");
    g_ui.Init(bx,by,bs); g_fx.Init(fx); g_core.Init(s,audioReady?&g_audio:nullptr);

    Events::gameProcessEvent.before += [](){g_core.BeforeGameProcess();};
    Events::gameProcessEvent.after += [](){const noxxa::RewindInput in=g_ui.PollInput();g_core.Tick(in.held,in.released,in.doubleTapped);};
    Events::touchScreenEvent.after += [](int a,int f,int x,int y){g_ui.OnTouch(a,f,x,y);};
    Events::drawAfterFadeEvent.after += [](){g_fx.Draw(g_core);};
    Events::drawHudEvent.after += [](){g_ui.DrawButton(g_core);};

    logger->Info("V3 loaded: %.2fs, %dHz, radius %.1f, max %d, audio %.3fs",s.maxRewindSeconds,s.snapshotHz,s.radius,s.maxEntities,audioReady?g_audio.DurationSeconds():0.0f);
    aml->ShowToast(false,"NOXXA REWIND v3 loaded");
}
