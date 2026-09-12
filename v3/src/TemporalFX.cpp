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

const EntitySnapshot* FindMatch(const WorldFrame& f, const EntitySnapshot& needle) {
    for (uint16_t i=0;i<f.count;++i) {
        const EntitySnapshot& s=f.entities[i];
        if (s.kind==needle.kind && s.ref==needle.ref && s.modelIndex==needle.modelIndex) return &s;
    }
    return nullptr;
}

void DrawWorldTrails(const RewindCore& core,float intensity,float w,float h) {
    const WorldFrame* newer=core.FxCurrentFrame();
    const WorldFrame* older=core.FxOlderFrame();
    if (!newer || !older) return;

    int drawn=0;
    for (uint16_t i=0;i<newer->count && drawn<12;++i) {
        const EntitySnapshot& a=newer->entities[i];
        const EntitySnapshot* b=FindMatch(*older,a);
        if (!b) continue;

        CVector sa{},sb{};
        if (!CalcScreenCoors(a.physical.transform.position,&sa) || !CalcScreenCoors(b->physical.transform.position,&sb)) continue;
        if (!std::isfinite(sa.x)||!std::isfinite(sa.y)||!std::isfinite(sb.x)||!std::isfinite(sb.y)) continue;
        if (sa.x<0||sa.x>w||sa.y<0||sa.y>h||sb.x<0||sb.x>w||sb.y<0||sb.y>h) continue;

        float dx=sb.x-sa.x, dy=sb.y-sa.y;
        const float len=std::sqrt(dx*dx+dy*dy);
        if (len<2.0f || len>w*0.22f) continue;
        dx/=len; dy/=len;
        const float px=-dy, py=dx;
        const float half=std::max(1.0f,h*0.0018f);
        const unsigned char alpha=U8((16.0f+std::min(18.0f,len*0.16f))*intensity);
        const CRGBA trail(244,243,239,alpha);
        CSprite2d::Draw2DPolygon(
            sa.x+px*half,sa.y+py*half,
            sb.x+px*half,sb.y+py*half,
            sb.x-px*half,sb.y-py*half,
            sa.x-px*half,sa.y-py*half,
            trail);

        const float dot=std::max(1.5f,h*0.0025f);
        R(sb.x-dot,sb.y-dot,sb.x+dot,sb.y+dot,CRGBA(250,249,245,U8(alpha*1.35f)));
        ++drawn;
    }
}
}

void TemporalFX::Draw(const RewindCore& core) {
    float intensity=core.VisualIntensity()*std::clamp(m_strength,0.0f,1.5f);
    if (intensity<=0.001f || RsGlobal.maximumWidth<=0 || RsGlobal.maximumHeight<=0) return;
    const float w=static_cast<float>(RsGlobal.maximumWidth), h=static_cast<float>(RsGlobal.maximumHeight);
    const double rt=std::chrono::duration<double>(Clock::now()-m_epoch).count();
    const float pulse=0.5f+0.5f*std::sin(static_cast<float>(rt*3.1));
    const float progress=core.Progress01();

    // Neutral photographic memory wash. Kept intentionally low-alpha.
    R(0,0,w,h,CRGBA(222,220,215,U8((7.0f+4.0f*pulse)*intensity)));

    // Soft edge pressure instead of hard colored scanlines.
    for (int i=0;i<4;++i) {
        const float k=(i+1)/4.0f;
        const float ew=w*(0.018f+0.018f*k), eh=h*(0.025f+0.020f*k);
        const unsigned char a=U8((8.0f+9.0f*k+4.0f*pulse)*intensity);
        R(0,0,ew,h,CRGBA(4,5,7,a)); R(w-ew,0,w,h,CRGBA(4,5,7,a));
        R(0,0,w,eh,CRGBA(5,6,8,U8(a*0.78f))); R(0,h-eh,w,h,CRGBA(5,6,8,U8(a*0.78f)));
    }

    if (core.IsWorldRewinding()) {
        // World-linked trails: peds/cars leave a real screen-space echo between
        // adjacent historical samples. This makes the FX react to gameplay.
        DrawWorldTrails(core,intensity,w,h);

        // Sparse atmospheric ribbons fill negative space without dominating it.
        for (int i=0;i<5;++i) {
            const float phase=std::fmod(static_cast<float>(rt*(0.15+0.017*i)+0.173*i),1.0f);
            const float x=w*(1.04f-phase*1.18f);
            const float y=h*(0.14f+0.145f*i+0.016f*std::sin(static_cast<float>(rt*1.6+i)));
            const float len=w*(0.065f+0.015f*(i%3)), thick=std::max(1.2f,h*(0.0017f+0.0004f*(i%2)));
            const CRGBA c(244,243,239,U8((5.0f+4.0f*pulse+i*0.6f)*intensity));
            CSprite2d::Draw2DPolygon(x,y,x+len,y-thick*1.7f,x+len,y+thick*0.25f,x,y+thick,c);
        }
    }

    // Very subtle exposure ghosts near the edges.
    const float drift=std::fmod(static_cast<float>(rt*0.20),1.0f);
    for (int i=0;i<2;++i) {
        const float x=w*(0.10f+0.48f*i+0.07f*drift), bw=w*0.0032f;
        R(x,0,x+bw,h,CRGBA(250,249,245,U8((3.5f+1.5f*i)*intensity)));
    }

    // Brief activation flash only.
    const float early=std::max(0.0f,1.0f-core.RewoundSeconds()*5.0f);
    if (core.IsWorldRewinding() && early>0.0f) R(0,0,w,h,CRGBA(252,250,245,U8(28.0f*early*intensity)));

    // 13-second segmented timeline: quiet, readable, no giant text overlay.
    const float l=w*0.28f, r=w*0.72f, y=h*0.915f, bh=std::max(2.0f,h*0.0030f);
    R(l,y,r,y+bh,CRGBA(5,6,8,U8(100.0f*intensity)));
    for (int i=0;i<=13;++i) {
        const float x=l+(r-l)*(static_cast<float>(i)/13.0f);
        const float tick=(i%3==0)?h*0.010f:h*0.0055f;
        R(x-0.7f,y-tick,x+0.7f,y+bh+tick*0.25f,CRGBA(236,235,231,U8((88.0f+(i==13?45.0f:0.0f))*intensity)));
    }
    const float marker=r-(r-l)*progress;
    R(marker-1.7f,y-h*0.010f,marker+1.7f,y+bh+h*0.004f,CRGBA(252,250,245,U8(225.0f*intensity)));
}

} // namespace noxxa
