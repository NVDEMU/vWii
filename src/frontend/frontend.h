#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace vwii::memory {
class Memory;
}

namespace vwii::frontend {

struct Status {
    std::string title{"vWii"};
    std::string game_id;
    uint32_t pc{};
    uint64_t instructions{};
    bool loaded{};
    bool halted{};
    uint32_t xfb_address{};
    uint32_t xfb_width{};
    uint32_t xfb_stride{};
    uint32_t xfb_height{};
};

class Frontend {
public:
    Frontend();
    ~Frontend();

    Frontend(const Frontend&) = delete;
    Frontend& operator=(const Frontend&) = delete;

    bool Initialize(const char* title = "vWii",
                    int width = 960,
                    int height = 720);

    void Shutdown();
    [[nodiscard]] bool PumpEvents();
    [[nodiscard]] std::string ConsumeDroppedFile();
    void Present(const Status& status, const memory::Memory* memory = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace vwii::frontend
