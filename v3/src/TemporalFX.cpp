#include "TemporalFX.hpp"
#include "RewindCore.hpp"

#include <aml-psdk/game_sa/engine/RsGlobal.h>
#include <aml-psdk/game_sa/engine/Sprite2d.h>

#include <algorithm>
#include <cmath>

namespace noxxa {
namespace {
unsigned char U8(float v){return static_cast<unsigned char>(std::clamp(v,0.0f,255.0f));}
void R(float l,float t,float r,float b,const CRGBA& c){CSprite2d::DrawRect(CRect(l,t,r,b),c);}
}

void TemporalFX::Draw(const RewindCore& core) {
    float intensity=core.VisualIntensity()*std::clamp(m_strength,0.0f,1.5f);
    if (intensity<=0.001f || RsGlobal.maximumWidth<=0 || RsGlobal.maximumHeight<=0) return;
    const float w=static_cast<float>(RsGlobal.maximumWidth), h=static_cast<float>(RsGlobal.maximumHeight);
    const double rt=std::chrono::duration<double>(Clock::now()-m_epoch).count();
    const float pulse=0.5f+0.5f*std::sin(static_cast<float>(rt*3.1));
    const float progress=core.Progress01();

    // Memory wash: low-alpha neutral veil, not the old colored scanline look.
    R(0,0,w,h,CRGBA(222,220,215,U8((7.0f+4.0f*pulse)*intensity)));

    // Multi-layer vignette gives soft directional blur impression without shaders.
    for (int i=0;i<4;++i) {
        const float k=(i+1)/4.0f;
        const float ew=w*(0.018f+0.018f*k), eh=h*(0.025f+0.020f*k);
        const unsigned char a=U8((8.0f+9.0f*k+4.0f*pulse)*intensity);
        R(0,0,ew,h,CRGBA(4,5,7,a)); R(w-ew,0,w,h,CRGBA(4,5,7,a));
        R(0,0,w,eh,CRGBA(5,6,8,U8(a*0.78f))); R(0,h-eh,w,h,CRGBA(5,6,8,U8(a*0.78f)));
    }

    // Backward-moving memory ribbons. Diagonal quads feel like frame smearing,
    // while remaining extremely cheap on Mali GPUs.
    if (core.IsWorldRewinding()) {
        for (int i=0;i<7;++i) {
            const float phase=std::fmod(static_cast<float>(rt*(0.16+0.018*i)+0.137*i),1.0f);
            const float x=w*(1.05f-phase*1.22f);
            const float y=h*(0.12f+0.105f*i+0.018f*std::sin(static_cast<float>(rt*1.7+i)));
            const float len=w*(0.09f+0.018f*(i%3)), thick=std::max(1.5f,h*(0.0020f+0.0005f*(i%2)));
            const CRGBA c(244,243,239,U8((7.0f+5.0f*pulse+i*0.7f)*intensity));
            CSprite2d::Draw2DPolygon(x,y,x+len,y-thick*1.8f,x+len,y+thick*0.3f,x,y+thick,c);
        }
    }

    // Camera-memory echo bands near the edges, deliberately sparse.
    const float drift=std::fmod(static_cast<float>(rt*0.21),1.0f);
    for (int i=0;i<3;++i) {
        const float x=w*(0.08f+0.30f*i+0.08f*drift), bw=w*0.004f;
        R(x,0,x+bw,h,CRGBA(250,249,245,U8((4.0f+2.0f*i)*intensity)));
    }

    // Short exposure bloom only at activation; no permanent white fog.
    const float early=std::max(0.0f,1.0f-core.RewoundSeconds()*5.0f);
    if (core.IsWorldRewinding() && early>0.0f) R(0,0,w,h,CRGBA(252,250,245,U8(30.0f*early*intensity)));

    // 13-second segmented timeline. It reads instantly without text clutter.
    const float l=w*0.28f, r=w*0.72f, y=h*0.915f, bh=std::max(2.0f,h*0.0030f);
    R(l,y,r,y+bh,CRGBA(5,6,8,U8(105.0f*intensity)));
    for (int i=0;i<=13;++i) {
        const float x=l+(r-l)*(static_cast<float>(i)/13.0f);
        const float tick=(i%3==0)?h*0.010f:h*0.006f;
        R(x-0.7f,y-tick,x+0.7f,y+bh+tick*0.25f,CRGBA(236,235,231,U8((95.0f+(i==13?55.0f:0.0f))*intensity)));
    }
    const float marker=r-(r-l)*progress;
    R(marker-1.8f,y-h*0.010f,marker+1.8f,y+bh+h*0.004f,CRGBA(252,250,245,U8(230.0f*intensity)));
}

} // namespace noxxa
