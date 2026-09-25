#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include "ios/nand_fs.h"
#include <string>

namespace vwii::disc {
class DiscImage;
}

namespace vwii::memory {
class Memory;
}

namespace vwii::ios {

class IOSHLE {
public:
    explicit IOSHLE(memory::Memory& memory);

    void Reset();
    void AttachDisc(disc::DiscImage* disc);

    // Processes one pending PPC->IOS IPC request.
    // Returns true when a request was consumed.
    bool Tick();

    [[nodiscard]] bool IsRunning() const { return running_; }

    void SetNandRoot(const std::string& root);

private:
    struct FileDescriptor {
        bool used{};
        uint32_t device{};
        uint64_t position{};
    };

    static constexpr uint32_t ErrorNoSuchDevice = static_cast<uint32_t>(-6);
    static constexpr uint32_t ErrorInvalidArgument = static_cast<uint32_t>(-4);
    static constexpr uint32_t ErrorNoSuchFile = static_cast<uint32_t>(-106);

    bool ReadRequest(uint32_t request_address, uint32_t& command,
                     uint32_t& fd, std::array<uint32_t, 5>& args) const;

    uint32_t Open(const std::array<uint32_t, 5>& args);
    uint32_t Close(uint32_t fd);
    uint32_t Read(uint32_t fd, const std::array<uint32_t, 5>& args);
    uint32_t Write(uint32_t fd, const std::array<uint32_t, 5>& args);
    uint32_t Seek(uint32_t fd, const std::array<uint32_t, 5>& args);
    uint32_t Ioctl(uint32_t fd, const std::array<uint32_t, 5>& args);
    uint32_t IoctlV(uint32_t fd, const std::array<uint32_t, 5>& args);

    uint32_t OpenDevice(const std::string& path, uint32_t mode);
    uint32_t HandleDI(uint32_t ioctl,
                      uint32_t in_address, uint32_t in_size,
                      uint32_t out_address, uint32_t out_size);

    void WriteResult(uint32_t request_address, uint32_t result);
    void CompleteRequest(uint32_t request_address);

    memory::Memory& memory_;
    disc::DiscImage* disc_{};
    std::array<FileDescriptor, 128> fds_{};
    std::unique_ptr<NandFS> nand_;
    bool di_partition_open_{};
    bool running_{};
};

} // namespace vwii::ios
