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
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(realT * 4.2));
    const float progress = core.Progress01();

    CSprite2d::DrawRect(CRect(0.0f, 0.0f, w, h),
                        CRGBA(214, 216, 214, U8((10.0f + pulse * 8.0f) * intensity)));

    const float edgeW = w * (0.055f + 0.010f * pulse);
    const float edgeH = h * (0.075f + 0.010f * pulse);
    const unsigned char edgeA = U8((44.0f + 18.0f * pulse) * intensity);
    CSprite2d::DrawRect(CRect(0.0f, 0.0f, edgeW, h), CRGBA(9, 10, 12, edgeA));
    CSprite2d::DrawRect(CRect(w - edgeW, 0.0f, w, h), CRGBA(9, 10, 12, edgeA));
    CSprite2d::DrawRect(CRect(0.0f, 0.0f, w, edgeH), CRGBA(14, 14, 16, U8(edgeA * 0.72f)));
    CSprite2d::DrawRect(CRect(0.0f, h - edgeH, w, h), CRGBA(14, 14, 16, U8(edgeA * 0.72f)));

    for (int i = 0; i < 6; ++i) {
        const float speed = 0.13f + 0.021f * i;
        const float phase = std::fmod(static_cast<float>(realT * speed + i * 0.173f), 1.0f);
        const float y = h * (0.10f + 0.78f * phase);
        const float length = w * (0.08f + 0.025f * (i % 3));
        const float xBase = (i & 1) ? w * 0.68f : w * 0.18f;
        const float x = xBase + std::sin(static_cast<float>(realT * 0.7 + i)) * w * 0.035f;
        const float a = (10.0f + 8.0f * pulse + i) * intensity;
        CSprite2d::DrawRect(CRect(x, y, std::min(w, x + length), y + 2.0f + (i % 2)),
                            CRGBA(245, 245, 242, U8(a)));
    }

    const float early = std::max(0.0f, 1.0f - core.RewoundSeconds() * 4.5f);
    if (early > 0.0f) {
        CSprite2d::DrawRect(CRect(0.0f, 0.0f, w, h),
                            CRGBA(250, 249, 245, U8(32.0f * early * intensity)));
    }

    const float barL = w * 0.24f;
    const float barR = w * 0.76f;
    const float barY = h * 0.902f;
    const float barH = std::max(2.0f, h * 0.0035f);
    CSprite2d::DrawRect(CRect(barL, barY, barR, barY + barH),
                        CRGBA(16, 16, 18, U8(118.0f * intensity)));
    const float knobX = barR - (barR - barL) * progress;
    CSprite2d::DrawRect(CRect(knobX - 2.0f, barY - 4.0f, knobX + 2.0f, barY + barH + 4.0f),
                        CRGBA(248, 247, 242, U8(225.0f * intensity)));

    char text[48];
    std::snprintf(text, sizeof(text), "-%.1fs", core.RewoundSeconds());
    CFont::SetScale(h / 540.0f * 0.50f);
    CFont::SetWrapx(1000000.0f);
    CFont::SetEdge(1);
    CFont::SetFontStyle(FO_FONT_STYLE_STANDARD);
    CFont::SetOrientation(ALIGN_CENTER);
    CFont::SetProportional(1);
    CFont::SetColor(CRGBA(247, 246, 242, U8(235.0f * intensity)));
    CFont::SetDropColor(CRGBA(0, 0, 0, U8(170.0f * intensity)));
    CFont::PrintString(w * 0.5f, h * 0.845f, text);
    CFont::RenderFontBuffer();
}

} // namespace noxxa
