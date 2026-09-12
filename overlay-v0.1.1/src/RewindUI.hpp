#pragma once

namespace noxxa {

struct RewindInput {
    bool held{false};
    bool released{false};
    bool doubleTapped{false};
};

class RewindCore;

class RewindUI {
public:
    bool Init(float xNorm, float yNorm, float scale);
    void OnTouch(int actionType, int finger, int x, int y);
    RewindInput PollInput();
    void DrawHud(const RewindCore& core) const;
    bool Ready() const { return true; }

private:
    bool HitTest(int x, int y, float extra = 0.0f) const;
    void GetButtonRect(float& left, float& top, float& right, float& bottom) const;

    float m_xNorm{0.88f};
    float m_yNorm{0.62f};
    float m_scale{1.0f};

    int m_activeFinger{-1};
    unsigned int m_lastTapMs{0};
    bool m_held{false};
    bool m_releasedPulse{false};
    bool m_doubleTapPulse{false};
};

} // namespace noxxa
