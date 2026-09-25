#include "core/emulator.h"

namespace vwii::core {

Emulator::Emulator()
    : memory_(), cpu_(memory_) {
}

bool Emulator::Initialize() {
    memory_.Reset();
    cpu_.Reset();
    initialized_ = true;
    return true;
}

void Emulator::Shutdown() {
    initialized_ = false;
}

void Emulator::Reset() {
    memory_.Reset();
    cpu_.Reset();
}

void Emulator::Step() {
    if (initialized_) {
        cpu_.Step();
    }
}

void Emulator::RunForInstructions(uint64_t count) {
    for (uint64_t i = 0; i < count; ++i) {
        Step();
    }
}

} // namespace vwii::core
