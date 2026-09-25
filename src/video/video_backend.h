#pragma once

#include <cstdint>
#include <memory>

namespace vwii::video {

struct FrameInfo {
    uint32_t width = 0;
    uint32_t height = 0;
    uint64_t frame_number = 0;
};

class Backend {
public:
    virtual ~Backend() = default;

    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Present(const FrameInfo& frame) = 0;
    [[nodiscard]] virtual const char* Name() const = 0;
};

std::unique_ptr<Backend> CreateNullBackend();

} // namespace vwii::video
