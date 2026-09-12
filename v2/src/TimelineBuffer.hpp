#pragma once

#include <cstddef>
#include <deque>

namespace noxxa {

template <class T>
class TimelineBuffer {
public:
    explicit TimelineBuffer(std::size_t capacity = 242) : m_capacity(capacity) {}

    void SetCapacity(std::size_t capacity) {
        m_capacity = capacity > 1 ? capacity : 2;
        while (m_frames.size() > m_capacity) m_frames.pop_front();
        if (m_rewinding && m_cursor >= m_frames.size()) {
            m_cursor = m_frames.empty() ? 0 : m_frames.size() - 1;
        }
    }

    std::size_t Capacity() const { return m_capacity; }
    std::size_t Size() const { return m_frames.size(); }
    bool Empty() const { return m_frames.empty(); }
    bool IsRewinding() const { return m_rewinding; }

    void Clear() {
        m_frames.clear();
        m_rewinding = false;
        m_cursor = 0;
    }

    void Push(const T& frame) {
        if (m_rewinding) return;
        m_frames.push_back(frame);
        while (m_frames.size() > m_capacity) m_frames.pop_front();
    }

    bool BeginRewind() {
        if (m_frames.size() < 2 || m_rewinding) return false;
        m_rewinding = true;
        m_cursor = m_frames.size() - 1;
        return true;
    }

    bool CanStepBack() const {
        return m_rewinding && m_cursor > 0 && !m_frames.empty();
    }

    const T* PeekStepBack() const {
        if (!CanStepBack()) return nullptr;
        return &m_frames[m_cursor - 1];
    }

    const T* StepBack() {
        if (!CanStepBack()) return Current();
        --m_cursor;
        return &m_frames[m_cursor];
    }

    const T* Current() const {
        if (!m_rewinding || m_frames.empty()) return nullptr;
        return &m_frames[m_cursor];
    }

    std::size_t Cursor() const { return m_cursor; }

    void CommitRewind() {
        if (!m_rewinding || m_frames.empty()) return;
        const auto keep = m_cursor + 1;
        while (m_frames.size() > keep) m_frames.pop_back();
        m_rewinding = false;
        m_cursor = 0;
    }

    void CancelRewind() {
        m_rewinding = false;
        m_cursor = 0;
    }

    const T& Front() const { return m_frames.front(); }
    const T& Back() const { return m_frames.back(); }

private:
    std::deque<T> m_frames;
    std::size_t m_capacity{242};
    std::size_t m_cursor{0};
    bool m_rewinding{false};
};

} // namespace noxxa
