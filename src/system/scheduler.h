#pragma once

#include <cstdint>
#include <functional>
#include <queue>
#include <vector>

namespace vwii::system {

using Cycle = uint64_t;

class Scheduler {
public:
    using Callback = std::function<void()>;

    Scheduler();

    void Reset();
    void Advance(Cycle cycles);
    [[nodiscard]] Cycle Now() const { return now_; }

    void Schedule(Cycle delay, Callback callback);

private:
    struct Event {
        Cycle when{};
        uint64_t sequence{};
        Callback callback;
    };

    struct Compare {
        bool operator()(const Event& a, const Event& b) const {
            if (a.when != b.when)
                return a.when > b.when;
            return a.sequence > b.sequence;
        }
    };

    Cycle now_{};
    uint64_t sequence_{};
    std::priority_queue<Event, std::vector<Event>, Compare> events_;
};

} // namespace vwii::system
