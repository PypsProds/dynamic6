#pragma once
#include <cstddef>
#include <deque>
#include "Types.hpp"

namespace dts {

enum class PushResult { Accepted, DuplicateEvent, DuplicateState, Overflow, Invalid };

inline const char* toString(PushResult r) {
    switch (r) {
        case PushResult::Accepted: return "accepted";
        case PushResult::DuplicateEvent: return "dup-event-id";
        case PushResult::DuplicateState: return "dup-state";
        case PushResult::Overflow: return "overflow";
        case PushResult::Invalid: return "invalid";
    }
    return "?";
}

// Bounded FIFO. Order of presses/releases is never changed.
// Overflow policy: reject the NEWEST event (dropping the oldest could orphan a
// press/release pair and reorder semantics).
class InputQueue {
public:
    explicit InputQueue(size_t capacity) : cap_(capacity) {}

    PushResult push(const InputEvent& e) {
        int b = static_cast<int>(e.button);
        if (e.lane >= kLaneCount || b < 0 || b >= kButtonCount) return PushResult::Invalid;
        if (haveId_ && e.id <= lastId_) return PushResult::DuplicateEvent;
        // Raw device state: a press while already held (or release while not held) is a repeat.
        if (held_[e.lane][b] == e.down) return PushResult::DuplicateState;
        if (q_.size() >= cap_) return PushResult::Overflow;
        q_.push_back(e);
        held_[e.lane][b] = e.down;
        lastId_ = e.id;
        haveId_ = true;
        return PushResult::Accepted;
    }

    const InputEvent* front() const { return q_.empty() ? nullptr : &q_.front(); }
    void pop() { if (!q_.empty()) q_.pop_front(); }
    size_t size() const { return q_.size(); }
    bool empty() const { return q_.empty(); }
    size_t capacity() const { return cap_; }
    void setCapacity(size_t c) { cap_ = c; }  // does not evict; existing items drain normally

    // Clears events and raw-held state. Event-id monotonicity is intentionally kept.
    void clear() {
        q_.clear();
        for (auto& l : held_) for (auto& h : l) h = false;
    }

private:
    std::deque<InputEvent> q_;
    size_t cap_;
    bool held_[kLaneCount][kButtonCount] = {};
    uint64_t lastId_ = 0;
    bool haveId_ = false;
};

}  // namespace dts
