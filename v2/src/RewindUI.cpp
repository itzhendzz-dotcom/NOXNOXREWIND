#include "RewindUI.hpp"
#include "RewindCore.hpp"

#include <aml-psdk/game_sa/base/Timer.h>
#include <aml-psdk/game_sa/engine/RsGlobal.h>
#include <aml-psdk/game_sa/engine/Sprite2d.h>

#include <algorithm>
#include <cmath>

namespace noxxa {

bool RewindUI::Init(float xNorm, float yNorm, float scale) {
    // Keep the control inside a conservative safe area for phones with rounded
    // corners/notches while still allowing config overrides.
    m_xNorm = std::clamp(xNorm, 0.04f, 0.96f);
    m_yNorm = std::clamp(yNorm, 0.07f, 0.93f);
    m_scale = std::clamp(scale, 0.60f, 1.80f);
    return true;
}

void RewindUI::GetButtonRect(float& left, float& top, float& right, float& bottom) const {
    const float w = static_cast<float>(std::max(RsGlobal.maximumWidth, 1));
    const float h = static_cast<float>(std::max(RsGlobal.maximumHeight, 1));
    const float size = h * 0.088f * m_scale;
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
        if (m_activeFinger < 0 && HitTest(x, y, 0.14f)) {
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
        if (!HitTest(x, y, 0.55f)) {
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

static void DrawLeftTriangle(float cx, float cy, float halfW, float halfH, const CRGBA& color) {
    // Draw2DPolygon is a quad primitive. Repeating the tip vertex creates a
    // stable filled triangle without introducing texture/font dependencies.
    CSprite2d::Draw2DPolygon(
        cx - halfW, cy,
        cx + halfW, cy - halfH,
        cx + halfW, cy + halfH,
        cx - halfW, cy,
        color
    );
}

void RewindUI::DrawButton(const RewindCore& core) const {
    if (RsGlobal.maximumWidth <= 0 || RsGlobal.maximumHeight <= 0) return;

    float l, t, r, b;
    GetButtonRect(l, t, r, b);

    const bool active = core.IsRewinding() || m_held;
    const bool available = core.AvailableSeconds() > 0.25f && core.PlayerCanRewind();
    const float size = b - t;
    const float cx = (l + r) * 0.5f;
    const float cy = (t + b) * 0.5f;

    const float now = static_cast<float>(CTimer::GetTimeInMS());
    const float pulse = active ? (0.5f + 0.5f * std::sin(now * 0.010f)) : 0.0f;

    // Soft shadow / separation from bright sky backgrounds.
    const float shadow = size * 0.075f;
    CSprite2d::DrawRect(CRect(l + shadow, t + shadow, r + shadow, b + shadow),
                        CRGBA(0, 0, 0, active ? 100 : 75));

    // Outer frame. Active state gets a subtle breathing halo without turning
    // the button into a giant neon overlay.
    const float halo = active ? size * (0.045f + 0.018f * pulse) : size * 0.028f;
    const unsigned char frameA = active ? static_cast<unsigned char>(205 + 30 * pulse)
                                        : static_cast<unsigned char>(available ? 175 : 90);
    CSprite2d::DrawRect(CRect(l - halo, t - halo, r + halo, b + halo),
                        CRGBA(224, 235, 246, frameA));

    // Main dark glass body + inner plate. Two layers look much cleaner than
    // the old debug-style outlined rectangle.
    CSprite2d::DrawRect(CRect(l, t, r, b),
                        active ? CRGBA(18, 23, 32, 235)
                               : CRGBA(10, 13, 19, available ? 198 : 125));

    const float inset = size * 0.075f;
    CSprite2d::DrawRect(CRect(l + inset, t + inset, r - inset, b - inset),
                        active ? CRGBA(35, 43, 55, 205)
                               : CRGBA(22, 27, 36, available ? 165 : 105));

    // Tiny top accent doubles as visual state feedback while keeping the
    // icon area uncluttered.
    const float accentH = std::max(2.0f, size * 0.035f);
    const unsigned char accentA = active ? 245 : static_cast<unsigned char>(available ? 165 : 75);
    CSprite2d::DrawRect(CRect(l + inset, t + inset, r - inset, t + inset + accentH),
                        CRGBA(228, 239, 250, accentA));

    // Proper rewind glyph: two geometric left-pointing triangles. No GTA font,
    // no PNG loader, no missing-glyph issue.
    const CRGBA icon = available ? CRGBA(247, 250, 255, active ? 255 : 235)
                                 : CRGBA(132, 137, 148, 180);
    const float triW = size * 0.145f;
    const float triH = size * 0.205f;
    const float gap = size * 0.055f;
    DrawLeftTriangle(cx - triW - gap * 0.5f, cy + size * 0.025f, triW, triH, icon);
    DrawLeftTriangle(cx + triW - gap * 0.5f, cy + size * 0.025f, triW, triH, icon);

    // When rewinding, show a slim depletion/progress strip at the bottom.
    if (active) {
        const float p = std::clamp(core.Progress01(), 0.0f, 1.0f);
        const float barPad = size * 0.12f;
        const float barH = std::max(2.0f, size * 0.045f);
        const float barL = l + barPad;
        const float barR = r - barPad;
        const float barY = b - barPad;
        CSprite2d::DrawRect(CRect(barL, barY - barH, barR, barY), CRGBA(5, 7, 10, 180));
        CSprite2d::DrawRect(CRect(barL, barY - barH, barL + (barR - barL) * (1.0f - p), barY),
                            CRGBA(235, 242, 250, 225));
    }
}

} // namespace noxxa
