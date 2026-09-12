#include "TemporalFX.hpp"
#include "RewindCore.hpp"

#include <aml-psdk/game_sa/engine/Font.h>
#include <aml-psdk/game_sa/engine/RsGlobal.h>
#include <aml-psdk/game_sa/engine/Sprite2d.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace noxxa {
namespace {

unsigned char U8(float v) {
    return static_cast<unsigned char>(std::clamp(v, 0.0f, 255.0f));
}

} // namespace

void TemporalFX::Draw(const RewindCore& core) {
    const float intensity = core.VisualIntensity();
    if (intensity <= 0.001f) return;
    if (RsGlobal.maximumWidth <= 0 || RsGlobal.maximumHeight <= 0) return;

    const float w = static_cast<float>(RsGlobal.maximumWidth);
    const float h = static_cast<float>(RsGlobal.maximumHeight);
    const double realT = std::chrono::duration<double>(Clock::now() - m_epoch).count();
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(realT * 7.5));
    const float progress = core.Progress01();

    CSprite2d::DrawRect(CRect(0.0f, 0.0f, w, h),
                        CRGBA(185, 205, 224, U8((18.0f + pulse * 10.0f) * intensity)));

    const float edgeW = w * (0.035f + 0.015f * pulse);
    const float edgeH = h * (0.055f + 0.020f * pulse);
    const unsigned char edgeA = U8((58.0f + 22.0f * pulse) * intensity);
    CSprite2d::DrawRect(CRect(0.0f, 0.0f, edgeW, h), CRGBA(12, 18, 32, edgeA));
    CSprite2d::DrawRect(CRect(w - edgeW, 0.0f, w, h), CRGBA(28, 18, 36, edgeA));
    CSprite2d::DrawRect(CRect(0.0f, 0.0f, w, edgeH), CRGBA(18, 22, 34, U8(edgeA * 0.75f)));
    CSprite2d::DrawRect(CRect(0.0f, h - edgeH, w, h), CRGBA(18, 22, 34, U8(edgeA * 0.75f)));

    const float chroma = 2.0f + 5.0f * intensity;
    CSprite2d::DrawRect(CRect(chroma, 0.0f, chroma + 2.0f, h),
                        CRGBA(110, 220, 255, U8(85.0f * intensity)));
    CSprite2d::DrawRect(CRect(w - chroma - 2.0f, 0.0f, w - chroma, h),
                        CRGBA(235, 130, 255, U8(70.0f * intensity)));

    for (int i = 0; i < 5; ++i) {
        const float phase = std::fmod(static_cast<float>(realT * (0.32 + i * 0.035) + i * 0.19), 1.0f);
        const float y = phase * h;
        const float thickness = 1.0f + (i % 2);
        CSprite2d::DrawRect(CRect(0.0f, y, w, y + thickness),
                            CRGBA(225, 238, 250, U8((12.0f + i * 2.0f) * intensity)));
    }

    const float early = std::max(0.0f, 1.0f - core.RewoundSeconds() * 3.3f);
    if (early > 0.0f) {
        CSprite2d::DrawRect(CRect(0.0f, 0.0f, w, h),
                            CRGBA(245, 250, 255, U8(38.0f * early * intensity)));
    }

    const float barL = w * 0.18f;
    const float barR = w * 0.82f;
    const float barY = h * 0.885f;
    const float barH = std::max(3.0f, h * 0.006f);
    CSprite2d::DrawRect(CRect(barL, barY, barR, barY + barH), CRGBA(10, 12, 18, U8(160.0f * intensity)));
    const float knobX = barR - (barR - barL) * progress;
    CSprite2d::DrawRect(CRect(knobX - 4.0f, barY - 3.0f, knobX + 4.0f, barY + barH + 3.0f),
                        CRGBA(242, 248, 255, U8(235.0f * intensity)));

    char text[128];
    std::snprintf(text, sizeof(text), "REWIND  -%.1fs   WORLD %u",
                  core.RewoundSeconds(),
                  static_cast<unsigned int>(core.CurrentEntityCount()));

    CFont::SetScale(h / 540.0f * 0.54f);
    CFont::SetWrapx(1000000.0f);
    CFont::SetEdge(2);
    CFont::SetFontStyle(FO_FONT_STYLE_STANDARD);
    CFont::SetOrientation(ALIGN_CENTER);
    CFont::SetProportional(1);
    CFont::SetColor(CRGBA(248, 250, 255, U8(255.0f * intensity)));
    CFont::SetDropColor(CRGBA(0, 0, 0, U8(220.0f * intensity)));
    CFont::PrintString(w * 0.5f, h * 0.82f, text);
    CFont::RenderFontBuffer();
}

} // namespace noxxa
