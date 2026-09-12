#include "RewindUI.hpp"
#include "RewindCore.hpp"

#include <aml-psdk/game_sa/base/Timer.h>
#include <aml-psdk/game_sa/engine/Font.h>
#include <aml-psdk/game_sa/engine/RsGlobal.h>
#include <aml-psdk/game_sa/engine/Sprite2d.h>

#include <algorithm>

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
    const float size = h * 0.092f * m_scale;
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
    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);
    return fx >= l - pad && fx <= r + pad && fy >= t - pad && fy <= b + pad;
}

void RewindUI::OnTouch(int actionType, int finger, int x, int y) {
    if (actionType == 2) {
        if (m_activeFinger < 0 && HitTest(x, y, 0.12f)) {
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
        if (!HitTest(x, y, 0.50f)) {
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

void RewindUI::DrawButton(const RewindCore& core) const {
    if (RsGlobal.maximumWidth <= 0 || RsGlobal.maximumHeight <= 0) return;

    float l, t, r, b;
    GetButtonRect(l, t, r, b);
    const bool active = core.IsRewinding() || m_held;
    const bool available = core.AvailableSeconds() > 0.25f && core.PlayerCanRewind();

    const float border = std::max(2.0f, static_cast<float>(RsGlobal.maximumHeight) * 0.0035f);
    const unsigned char outerA = active ? 235 : (available ? 160 : 80);
    const unsigned char innerA = active ? 220 : (available ? 150 : 90);

    CSprite2d::DrawRect(CRect(l - border, t - border, r + border, b + border),
                        CRGBA(230, 236, 245, outerA));
    CSprite2d::DrawRect(CRect(l, t, r, b),
                        active ? CRGBA(20, 24, 34, innerA) : CRGBA(12, 14, 20, innerA));

    const float h = static_cast<float>(RsGlobal.maximumHeight);
    CFont::SetScale(h / 540.0f * 0.70f);
    CFont::SetWrapx(1000000.0f);
    CFont::SetJustify(0);
    CFont::SetEdge(1);
    CFont::SetFontStyle(FO_FONT_STYLE_HEADING);
    CFont::SetOrientation(ALIGN_CENTER);
    CFont::SetProportional(1);
    CFont::SetColor(available ? CRGBA(248, 250, 255, 255) : CRGBA(145, 145, 155, 220));
    CFont::SetDropColor(CRGBA(0, 0, 0, 255));
    CFont::PrintString((l + r) * 0.5f, t + (b - t) * 0.23f, "<<");
    CFont::RenderFontBuffer();
}

} // namespace noxxa
