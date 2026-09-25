#include "video/video_backend.h"

namespace vwii::video {

class NullBackend final : public Backend {
public:
    bool Initialize() override {
        return true;
    }

    void Shutdown() override {
    }

    void Present(const FrameInfo&) override {
    }

    [[nodiscard]] const char* Name() const override {
        return "Null";
    }
};

std::unique_ptr<Backend> CreateNullBackend() {
    return std::make_unique<NullBackend>();
}

} // namespace vwii::video
