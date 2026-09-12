#include "RewindUI.hpp"
#include "RewindCore.hpp"

#include <aml-psdk/game_sa/base/Timer.h>
#include <aml-psdk/game_sa/engine/RsGlobal.h>
#include <aml-psdk/game_sa/engine/Sprite2d.h>

#include <algorithm>
#include <cmath>

namespace noxxa {

bool RewindUI::Init(float x,float y,float scale) {
    m_xNorm=std::clamp(x,0.035f,0.965f); m_yNorm=std::clamp(y,0.065f,0.935f); m_scale=std::clamp(scale,0.60f,1.50f); return true;
}
void RewindUI::GetButtonRect(float& l,float& t,float& r,float& b) const {
    const float w=static_cast<float>(std::max(RsGlobal.maximumWidth,1)), h=static_cast<float>(std::max(RsGlobal.maximumHeight,1));
    const float size=h*0.078f*m_scale, cx=w*m_xNorm, cy=h*m_yNorm;
    l=cx-size*0.5f; r=cx+size*0.5f; t=cy-size*0.5f; b=cy+size*0.5f;
}
bool RewindUI::HitTest(int x,int y,float extra) const {
    float l,t,r,b; GetButtonRect(l,t,r,b); const float pad=(b-t)*extra;
    return x>=l-pad && x<=r+pad && y>=t-pad && y<=b+pad;
}
void RewindUI::OnTouch(int action,int finger,int x,int y) {
    if (action==2) {
        if (m_activeFinger<0 && HitTest(x,y,0.24f)) {
            const unsigned int now=CTimer::GetTimeInMS();
            if (m_lastTapMs && now-m_lastTapMs<=300u) m_doubleTapPulse=true;
            m_lastTapMs=now; m_activeFinger=finger; m_held=true;
        }
        return;
    }
    if (finger!=m_activeFinger) return;
    if (action==3) {
        if (!HitTest(x,y,0.65f)) { m_held=false; m_releasedPulse=true; m_activeFinger=-1; }
        return;
    }
    if (action==1 || action==4) { if (m_held) m_releasedPulse=true; m_held=false; m_activeFinger=-1; }
}
RewindInput RewindUI::PollInput() { RewindInput o{m_held,m_releasedPulse,m_doubleTapPulse}; m_releasedPulse=false; m_doubleTapPulse=false; return o; }

static void Tri(float cx,float cy,float hw,float hh,const CRGBA& c) {
    CSprite2d::Draw2DPolygon(cx-hw,cy,cx+hw,cy-hh,cx+hw,cy+hh,cx-hw,cy,c);
}
static void Rect(float l,float t,float r,float b,const CRGBA& c) { CSprite2d::DrawRect(CRect(l,t,r,b),c); }

void RewindUI::DrawButton(const RewindCore& core) const {
    if (RsGlobal.maximumWidth<=0 || RsGlobal.maximumHeight<=0) return;
    float l,t,r,b; GetButtonRect(l,t,r,b);
    const float s=b-t,cx=(l+r)*0.5f,cy=(t+b)*0.5f;
    const bool active=core.IsRewinding()||m_held, ready=core.Charge01()>0.08f && core.PlayerCanRewind();
    const float pulse=active?(0.5f+0.5f*std::sin(CTimer::GetTimeInMS()*0.009f)):0.0f;

    // Compact glass tile with open corners: less debug-box, more ability control.
    Rect(l+s*0.06f,t+s*0.06f,r+s*0.06f,b+s*0.06f,CRGBA(0,0,0,62));
    Rect(l,t,r,b,CRGBA(10,13,18,active?205:(ready?165:105)));
    const float in=s*0.085f;
    Rect(l+in,t+in,r-in,b-in,CRGBA(27,31,39,active?180:(ready?125:80)));

    const unsigned char ca=active?static_cast<unsigned char>(205+35*pulse):static_cast<unsigned char>(ready?150:70);
    const CRGBA corner(235,241,248,ca); const float len=s*0.18f, thick=std::max(1.5f,s*0.025f);
    Rect(l,t,l+len,t+thick,corner); Rect(l,t,l+thick,t+len,corner);
    Rect(r-len,t,r,t+thick,corner); Rect(r-thick,t,r,t+len,corner);
    Rect(l,b-thick,l+len,b,corner); Rect(l,b-len,l+thick,b,corner);
    Rect(r-len,b-thick,r,b,corner); Rect(r-thick,b-len,r,b,corner);

    const CRGBA icon=ready?CRGBA(248,250,253,active?255:230):CRGBA(125,130,140,165);
    const float tw=s*0.14f, th=s*0.19f, gap=s*0.045f;
    Tri(cx-tw-gap*0.5f,cy-s*0.02f,tw,th,icon); Tri(cx+tw-gap*0.5f,cy-s*0.02f,tw,th,icon);

    // Charge when idle, remaining rewind while active.
    const float p=active?(1.0f-core.Progress01()):core.Charge01();
    const float bl=l+s*0.15f, br=r-s*0.15f, by=b-s*0.10f, bh=std::max(2.0f,s*0.035f);
    Rect(bl,by-bh,br,by,CRGBA(2,4,7,145));
    Rect(bl,by-bh,bl+(br-bl)*std::clamp(p,0.0f,1.0f),by,ready?CRGBA(232,238,245,220):CRGBA(100,105,115,130));
}

} // namespace noxxa
