#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace noxxa {

template <class T>
class TimelineBuffer {
public:
    explicit TimelineBuffer(std::size_t capacity = 242) { SetCapacity(capacity); }

    void SetCapacity(std::size_t capacity) {
        capacity = capacity > 1 ? capacity : 2;
        if (capacity == m_capacity && !m_storage.empty()) return;

        std::vector<T> fresh(capacity);
        const std::size_t keep = std::min(m_size, capacity);
        const std::size_t first = m_size > keep ? (m_size - keep) : 0;
        for (std::size_t i = 0; i < keep; ++i) fresh[i] = LogicalAt(first + i);

        m_storage.swap(fresh);
        m_capacity = capacity;
        m_head = 0;
        m_size = keep;
        m_rewinding = false;
        m_cursor = 0;
    }

    std::size_t Capacity() const { return m_capacity; }
    std::size_t Size() const { return m_size; }
    bool Empty() const { return m_size == 0; }
    bool IsRewinding() const { return m_rewinding; }

    void Clear() {
        m_head = 0;
        m_size = 0;
        m_rewinding = false;
        m_cursor = 0;
    }

    void Push(const T& frame) {
        if (m_rewinding || m_storage.empty()) return;
        if (m_size < m_capacity) {
            m_storage[PhysicalIndex(m_size)] = frame;
            ++m_size;
        } else {
            m_storage[m_head] = frame;
            m_head = (m_head + 1) % m_capacity;
        }
    }

    bool BeginRewind() {
        if (m_size < 2 || m_rewinding) return false;
        m_rewinding = true;
        m_cursor = m_size - 1;
        return true;
    }

    bool CanStepBack() const { return m_rewinding && m_size > 0 && m_cursor > 0; }

    const T* PeekStepBack() const {
        if (!CanStepBack()) return nullptr;
        return &LogicalAt(m_cursor - 1);
    }

    const T* StepBack() {
        if (!CanStepBack()) return Current();
        --m_cursor;
        return &LogicalAt(m_cursor);
    }

    const T* Current() const {
        if (!m_rewinding || m_size == 0) return nullptr;
        return &LogicalAt(m_cursor);
    }

    std::size_t Cursor() const { return m_cursor; }

    void CommitRewind() {
        if (!m_rewinding || m_size == 0) return;
        m_size = m_cursor + 1;
        m_rewinding = false;
        m_cursor = 0;
    }

    void CancelRewind() {
        m_rewinding = false;
        m_cursor = 0;
    }

    const T& Front() const { return LogicalAt(0); }
    const T& Back() const { return LogicalAt(m_size - 1); }

private:
    std::size_t PhysicalIndex(std::size_t logical) const { return (m_head + logical) % m_capacity; }
    T& LogicalAt(std::size_t logical) { return m_storage[PhysicalIndex(logical)]; }
    const T& LogicalAt(std::size_t logical) const { return m_storage[PhysicalIndex(logical)]; }

    std::vector<T> m_storage{};
    std::size_t m_capacity{0};
    std::size_t m_head{0};
    std::size_t m_size{0};
    std::size_t m_cursor{0};
    bool m_rewinding{false};
};

} // namespace noxxa
