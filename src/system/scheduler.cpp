#include "system/scheduler.h"

namespace vwii::system {

Scheduler::Scheduler() = default;

void Scheduler::Reset() {
    now_ = 0;
    sequence_ = 0;
    events_ = {};
}

void Scheduler::Schedule(Cycle delay, Callback callback) {
    events_.push(Event{now_ + delay, sequence_++, std::move(callback)});
}

void Scheduler::Advance(Cycle cycles) {
    const Cycle target = now_ + cycles;

    while (!events_.empty() && events_.top().when <= target) {
        Event event = std::move(const_cast<Event&>(events_.top()));
        events_.pop();

        now_ = event.when;
        if (event.callback)
            event.callback();
    }

    now_ = target;
}

} // namespace vwii::system
