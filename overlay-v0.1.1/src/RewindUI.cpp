#include "RewindUI.hpp"
#include "RewindCore.hpp"

#include <aml-psdk/game_sa/base/Timer.h>
#include <aml-psdk/game_sa/engine/Font.h>
#include <aml-psdk/game_sa/engine/RsGlobal.h>
#include <aml-psdk/game_sa/engine/Sprite2d.h>
#include <algorithm>
#include <cstdio>

namespace noxxa {

bool RewindUI::Init(float xNorm, float yNorm, float scale) {
    m_xNorm = std::clamp(xNorm, 0.05f, 0.95f);
    m_yNorm = std::clamp(yNorm, 0.08f, 0.92f);
    m_scale = std::clamp(scale, 0.60f, 1.80f);
    return true;
}

void RewindUI::GetButtonRect(float& left, float& top, float& right, float& bottom) const {
    const float w = static_cast<float>(std::max(RsGlobal.maximumWidth, 1));
    const float h = static_cast<float>(std::max(RsGlobal.maximumHeight, 1));
    const float size = h * 0.095f * m_scale;
    const float cx = w * m_xNorm;
    const float cy = h * m_yNorm;

    left = cx - size * 0.5f;
    right = cx + size * 0.5f;
    top = cy - size * 0.5f;
    bottom = cy + size * 0.5f;
}

bool RewindUI::HitTest(int x, int y, float extra) const {
    float l, t, r, b;
    GetButtonRect(l, t, r, b);
    const float pad = (b - t) * extra;
    return static_cast<float>(x) >= l - pad && static_cast<float>(x) <= r + pad &&
           static_cast<float>(y) >= t - pad && static_cast<float>(y) <= b + pad;
}

void RewindUI::OnTouch(int actionType, int finger, int x, int y) {
    // GTA Android/NVIDIA multitouch actions: 1=UP, 2=DOWN, 3=MOVE, 4=CANCEL.
    if (actionType == 2) {
        if (m_activeFinger < 0 && HitTest(x, y, 0.10f)) {
            const unsigned int now = CTimer::GetTimeInMS();
            if (m_lastTapMs != 0 && (now - m_lastTapMs) <= 320u) {
                m_doubleTapPulse = true;
            }
            m_lastTapMs = now;
            m_activeFinger = finger;
            m_held = true;
        }
        return;
    }

    if (finger != m_activeFinger) return;

    if (actionType == 3) {
        if (!HitTest(x, y, 0.45f)) {
            m_held = false;
            m_releasedPulse = true;
            m_activeFinger = -1;
        }
        return;
    }

    if (actionType == 1 || actionType == 4) {
        if (m_held) m_releasedPulse = true;
        m_held = false;
        m_activeFinger = -1;
    }
}

RewindInput RewindUI::PollInput() {
    RewindInput out{};
    out.held = m_held;
    out.released = m_releasedPulse;
    out.doubleTapped = m_doubleTapPulse;
    m_releasedPulse = false;
    m_doubleTapPulse = false;
    return out;
}

void RewindUI::DrawHud(const RewindCore& core) const {
    if (RsGlobal.maximumWidth <= 0 || RsGlobal.maximumHeight <= 0) return;

    float l, t, r, b;
    GetButtonRect(l, t, r, b);

    const bool active = core.IsRewinding() || m_held;
    const float border = std::max(2.0f, static_cast<float>(RsGlobal.maximumHeight) * 0.004f);

    CSprite2d::DrawRect(CRect(l - border, t - border, r + border, b + border),
                        CRGBA(235, 235, 240, active ? 220 : 155));
    CSprite2d::DrawRect(CRect(l, t, r, b),
                        CRGBA(18, 19, 24, active ? 220 : 150));

    const float h = static_cast<float>(RsGlobal.maximumHeight);
    CFont::SetScale(h / 540.0f * 0.72f);
    CFont::SetWrapx(1000000.0f);
    CFont::SetJustify(0);
    CFont::SetEdge(1);
    CFont::SetFontStyle(FO_FONT_STYLE_HEADING);
    CFont::SetOrientation(ALIGN_CENTER);
    CFont::SetProportional(1);
    CFont::SetColor(CRGBA(245, 245, 250, 255));
    CFont::SetDropColor(CRGBA(0, 0, 0, 255));
    CFont::PrintString((l + r) * 0.5f, t + (b - t) * 0.25f, "<<");
    CFont::RenderFontBuffer();

    if (!core.IsRewinding()) return;

    char text[96];
    std::snprintf(text, sizeof(text), "REWINDING  -%.1fs / %.1fs", core.RewoundSeconds(), core.AvailableSeconds());

    CFont::SetScale(h / 540.0f * 0.60f);
    CFont::SetEdge(2);
    CFont::SetFontStyle(FO_FONT_STYLE_STANDARD);
    CFont::SetOrientation(ALIGN_LEFT);
    CFont::SetColor(CRGBA(255, 255, 255, 255));
    CFont::SetDropColor(CRGBA(0, 0, 0, 220));
    CFont::PrintString(0.04f * RsGlobal.maximumWidth, 0.08f * RsGlobal.maximumHeight, text);
    CFont::RenderFontBuffer();
}

} // namespace noxxa
