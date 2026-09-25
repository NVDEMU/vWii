#include "ios/ios_hle.h"

#include "disc/disc_image.h"
#include "memory/memory.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <span>
#include <string>
#include <vector>

namespace vwii::ios {

namespace {

constexpr uint32_t IOS_IPC_BASE = 0x0D800000;
constexpr uint32_t IPC_PPCMSG = IOS_IPC_BASE + 0x00;
constexpr uint32_t IPC_PPCCTRL = IOS_IPC_BASE + 0x04;
constexpr uint32_t IPC_ARMMSG = IOS_IPC_BASE + 0x08;

constexpr uint32_t FD_DI = 1;
constexpr uint32_t FD_FS = 2;
constexpr uint32_t FD_ES = 3;
constexpr uint32_t FD_STM = 4;

constexpr uint32_t DI_READ_DISK_ID = 0x70;
constexpr uint32_t DI_OPEN_PARTITION = 0x8B;
constexpr uint32_t DI_READ = 0x8D;
constexpr uint32_t DI_ENABLE_DVD_VIDEO = 0x8E;
constexpr uint32_t DI_GET_STATUS = 0x95;
constexpr uint32_t DI_GET_CONTROL = 0x96;

uint32_t ReadBE32(const vwii::memory::Memory& memory, uint32_t address) {
    return memory.Read32(address);
}

void WriteBE32(vwii::memory::Memory& memory, uint32_t address,
               uint32_t value) {
    memory.Write32(address, value);
}

std::string ReadCString(const vwii::memory::Memory& memory, uint32_t address,
                        std::size_t max_length = 0x100) {
    std::string value;
    value.reserve(max_length);

    for (std::size_t i = 0; i < max_length; ++i) {
        const char c = static_cast<char>(memory.Read8(address + static_cast<uint32_t>(i)));
        if (c == '\0')
            break;
        value.push_back(c);
    }

    return value;
}

} // namespace

IOSHLE::IOSHLE(memory::Memory& memory)
    : memory_(memory), nand_(std::make_unique<NandFS>(memory)) {
    Reset();
}

void IOSHLE::SetNandRoot(const std::string& root) {
    nand_->SetRoot(root);
}

void IOSHLE::Reset() {
    nand_->Reset();
    di_partition_open_ = false;

    for (auto& fd : fds_)
        fd = {};

    fds_[FD_DI].used = true;
    fds_[FD_DI].device = FD_DI;

    fds_[FD_FS].used = true;
    fds_[FD_FS].device = FD_FS;

    fds_[FD_ES].used = true;
    fds_[FD_ES].device = FD_ES;

    fds_[FD_STM].used = true;
    fds_[FD_STM].device = FD_STM;

    fds_[FD_USB_OH1].used = true;
    fds_[FD_USB_OH1].device = FD_USB_OH1;

    running_ = true;
}

void IOSHLE::AttachDisc(disc::DiscImage* disc) {
    disc_ = disc;
}

void IOSHLE::AttachWiimote(input::WiiRemoteKeyboard* wiimote) {
    wiimote_ = wiimote;
}

bool IOSHLE::ReadRequest(uint32_t request_address, uint32_t& command,
                          uint32_t& fd,
                          std::array<uint32_t, 5>& args) const {
    command = ReadBE32(memory_, request_address + 0x00);
    fd = ReadBE32(memory_, request_address + 0x08);

    for (std::size_t i = 0; i < args.size(); ++i)
        args[i] = ReadBE32(memory_, request_address + 0x0C +
                           static_cast<uint32_t>(i * 4));

    return true;
}

uint32_t IOSHLE::OpenDevice(const std::string& path, uint32_t mode) {
    if (path == "/dev/di")
        return FD_DI;
    if (path == "/dev/fs")
        return FD_FS;
    if (path == "/dev/es")
        return FD_ES;
    if (path == "/dev/stm/eventhook")
        return FD_STM;
    if (path == "/dev/usb/oh1" ||
        path.rfind("/dev/usb/oh1/", 0) == 0)
        return FD_USB_OH1;

    if (!path.empty() && path.front() == '/') {
        if (path.rfind("/dev/", 0) == 0)
            return ErrorNoSuchDevice;
        if (nand_)
            return static_cast<uint32_t>(nand_->Open(path, mode));
    }

    return ErrorNoSuchDevice;
}

uint32_t IOSHLE::Open(const std::array<uint32_t, 5>& args) {
    const std::string path = ReadCString(memory_, args[0]);
    const uint32_t mode = args[1];

    uint32_t fd = OpenDevice(path, mode);
    if (fd < fds_.size())
        fds_[fd].position = 0;
    return fd;
}

uint32_t IOSHLE::Close(uint32_t fd) {
    if (fd >= fds_.size())
        return ErrorInvalidArgument;

    if (fd >= 8 && nand_)
        return static_cast<uint32_t>(nand_->Close(fd));

    if (!fds_[fd].used)
        return ErrorInvalidArgument;

    return 0;
}

uint32_t IOSHLE::Read(uint32_t fd, const std::array<uint32_t, 5>& args) {
    if (fd >= fds_.size())
        return ErrorInvalidArgument;

    if (fd >= 8 && nand_)
        return static_cast<uint32_t>(nand_->Read(fd, args[0], args[1]));

    if (!fds_[fd].used)
        return ErrorInvalidArgument;

    const uint32_t address = args[0];
    const uint32_t size = args[1];

    if (fd == FD_DI && disc_) {
        if (size == 0)
            return 0;

        std::vector<uint8_t> data(size);
        if (!disc_->Read(fds_[fd].position, data.data(), data.size()))
            return ErrorNoSuchFile;

        memory_.WriteBlock(address, data);
        fds_[fd].position += size;
        return size;
    }

    return ErrorNoSuchDevice;
}

uint32_t IOSHLE::Write(uint32_t fd, const std::array<uint32_t, 5>& args) {
    if (fd >= fds_.size())
        return ErrorInvalidArgument;

    if (fd >= 8 && nand_)
        return static_cast<uint32_t>(nand_->Write(fd, args[0], args[1]));

    if (!fds_[fd].used)
        return ErrorInvalidArgument;

    (void)args;
    return 0;
}

uint32_t IOSHLE::Seek(uint32_t fd, const std::array<uint32_t, 5>& args) {
    if (fd >= fds_.size())
        return ErrorInvalidArgument;

    if (fd >= 8 && nand_)
        return static_cast<uint32_t>(nand_->Seek(fd, static_cast<int32_t>(args[0]), args[1]));

    if (!fds_[fd].used)
        return ErrorInvalidArgument;

    const int32_t offset = static_cast<int32_t>(args[0]);
    const uint32_t whence = args[1];

    uint64_t base = 0;
    if (whence == 1)
        base = fds_[fd].position;
    else if (whence == 2 && disc_)
        base = disc_->Info().disc_size;

    const int64_t next = static_cast<int64_t>(base) + offset;
    if (next < 0)
        return ErrorInvalidArgument;

    fds_[fd].position = static_cast<uint64_t>(next);
    return static_cast<uint32_t>(fds_[fd].position);
}

uint32_t IOSHLE::HandleDI(uint32_t ioctl, uint32_t in_address,
                          uint32_t in_size, uint32_t out_address,
                          uint32_t out_size) {
    if (in_size != 0x20 && ioctl != DI_ENABLE_DVD_VIDEO)
        return 0x80;

    if (ioctl == DI_ENABLE_DVD_VIDEO)
        return 0;

    if (ioctl == DI_READ_DISK_ID) {
        if (!disc_ || out_size < 0x20)
            return 0x80;

        std::array<uint8_t, 0x20> id{};
        if (!disc_->Read(0, id.data(), id.size()))
            return 0x80;

        memory_.WriteBlock(out_address, id);
        return 1;
    }

    if (ioctl == DI_OPEN_PARTITION) {
        if (!disc_ || in_size < 0x20)
            return 0x20;

        di_partition_open_ = true;
        return 1;
    }

    if (ioctl == DI_GET_STATUS) {
        if (out_size < 4)
            return 0x80;

        WriteBE32(memory_, out_address, 0);
        return 1;
    }

    if (ioctl == DI_GET_CONTROL) {
        if (out_size < 4)
            return 0x80;

        WriteBE32(memory_, out_address, 0);
        return 1;
    }

    if (ioctl == DI_READ) {
        if (!disc_ || in_size < 0x20)
            return 0x80;

        const uint32_t length = ReadBE32(memory_, in_address + 4);
        const uint32_t position_words = ReadBE32(memory_, in_address + 8);
        const uint64_t position = static_cast<uint64_t>(position_words) << 2;

        if (out_size < length)
            return 0x80;

        std::vector<uint8_t> data(length);

        const bool ok = di_partition_open_
            ? disc_->ReadGamePartition(position, data.data(), data.size())
            : disc_->Read(position, data.data(), data.size());

        if (!ok)
            return 0x80;

        memory_.WriteBlock(out_address, data);
        return 1;
    }

    return 0x80;
}

uint32_t IOSHLE::Ioctl(uint32_t fd, const std::array<uint32_t, 5>& args) {
    if (fd >= fds_.size() || !fds_[fd].used)
        return ErrorInvalidArgument;

    const uint32_t ioctl = args[0];
    const uint32_t in_address = args[1];
    const uint32_t in_size = args[2];
    const uint32_t out_address = args[3];
    const uint32_t out_size = args[4];

    if (fd == FD_DI)
        return HandleDI(ioctl, in_address, in_size, out_address, out_size);

    if (fd == FD_FS && nand_) {
        switch (ioctl) {
        case 3: { // CreateDir
            return static_cast<uint32_t>(
                nand_->CreateDirectory(ReadCString(memory_, in_address)));
        }

        case 4: { // ReadDir
            return static_cast<uint32_t>(
                nand_->ReadDirectory(ReadCString(memory_, in_address),
                                      out_address, out_size));
        }

        case 7: // Delete
            return static_cast<uint32_t>(
                nand_->Remove(ReadCString(memory_, in_address)));

        case 8: { // Rename
            const std::string source = ReadCString(memory_, in_address);
            const std::string destination =
                ReadCString(memory_, in_address + 0x40);
            return static_cast<uint32_t>(
                nand_->Rename(source, destination));
        }

        case 9: // CreateFile
            return static_cast<uint32_t>(
                nand_->CreateFile(ReadCString(memory_, in_address)));

        default:
            return 0;
        }
    }

    return 0;
}


uint32_t IOSHLE::HandleUsbIoctlV(uint32_t request,
                                  uint32_t in_count,
                                  uint32_t out_count,
                                  uint32_t vector_address) {
    struct IoVector {
        uint32_t address;
        uint32_t size;
    };

    const uint32_t total_vectors = in_count + out_count;
    if (total_vectors > 32)
        return ErrorInvalidArgument;

    std::vector<IoVector> vectors(total_vectors);

    for (uint32_t i = 0; i < total_vectors; ++i) {
        const uint32_t address = vector_address + i * 8;
        vectors[i].address = memory_.Read32(address);
        vectors[i].size = memory_.Read32(address + 4);
    }

    // /dev/usb/oh1 is backed by the internal Bluetooth adapter. The initial
    // HLE does not emulate Bluetooth packets yet, but it accepts the
    // control/bulk/interrupt plumbing and returns deterministic empty data.
    // This lets software initialize the resource manager cleanly.
    switch (request) {
    case 0: { // USB control message
        if (in_count < 6 || out_count < 1)
            return ErrorInvalidArgument;

        const uint32_t output_address = vectors[in_count].address;
        const uint32_t output_size = vectors[in_count].size;

        if (output_size != 0)
            memory_.Fill(output_address, output_size, 0);

        return static_cast<uint32_t>(output_size);
    }

    case 1: // USB bulk transfer
    case 2: { // USB interrupt transfer
        if (in_count < 2 || out_count < 1)
            return ErrorInvalidArgument;

        const uint8_t endpoint =
            memory_.Read8(vectors[0].address);

        const uint32_t requested =
            (static_cast<uint32_t>(memory_.Read8(vectors[1].address)) << 8) |
            memory_.Read8(vectors[1].address + 1);

        const uint32_t output_address = vectors[in_count].address;
        const uint32_t output_size = vectors[in_count].size;

        if ((endpoint & 0x80u) != 0 && output_size != 0) {
            if (fd == FD_USB_OH1 && request == 2 && wiimote_) {
                const auto report = wiimote_->BuildReport();
                const std::size_t count =
                    std::min<std::size_t>(report.size(), output_size);
                memory_.WriteBlock(
                    output_address,
                    std::span<const uint8_t>(report.data(), count));
                if (count < output_size)
                    memory_.Fill(output_address + static_cast<uint32_t>(count),
                                 output_size - count, 0);
                return static_cast<uint32_t>(count);
            }

            memory_.Fill(output_address, output_size, 0);
        }

        return static_cast<uint32_t>(
            std::min<uint32_t>(requested, output_size));
    }

    default:
        return 0;
    }
}

uint32_t IOSHLE::IoctlV(uint32_t fd,
                        const std::array<uint32_t, 5>& args) {
    if (fd >= fds_.size() || !fds_[fd].used)
        return ErrorInvalidArgument;

    const uint32_t request = args[0];
    const uint32_t in_count = args[1];
    const uint32_t out_count = args[2];
    const uint32_t vector_address = args[3];

    if (fd == FD_USB_OH1)
        return HandleUsbIoctlV(request, in_count, out_count, vector_address);

    return 0;
}

void IOSHLE::WriteResult(uint32_t request_address, uint32_t result) {
    WriteBE32(memory_, request_address + 0x04, result);
}

void IOSHLE::CompleteRequest(uint32_t request_address) {
    memory_.Hollywood().Write32(IPC_ARMMSG, request_address);
    memory_.Hollywood().CompleteIpcReply();
}

bool IOSHLE::Tick() {
    if (!running_)
        return false;

    const uint32_t control = memory_.Hollywood().Read32(IPC_PPCCTRL);
    if ((control & IPC_X1) == 0)
        return false;

    const uint32_t request_address =
        memory_.Hollywood().Read32(IPC_PPCMSG) | 0x80000000U;

    uint32_t command = 0;
    uint32_t fd = 0;
    std::array<uint32_t, 5> args{};

    if (!ReadRequest(request_address, command, fd, args)) {
        WriteResult(request_address, ErrorInvalidArgument);
        CompleteRequest(request_address);
        return true;
    }

    uint32_t result = 0;

    switch (command) {
    case 1: result = Open(args); break;
    case 2: result = Close(fd); break;
    case 3: result = Read(fd, args); break;
    case 4: result = Write(fd, args); break;
    case 5: result = Seek(fd, args); break;
    case 6: result = Ioctl(fd, args); break;
    case 7: result = IoctlV(fd, args); break;
    default: result = ErrorInvalidArgument; break;
    }

    WriteResult(request_address, result);
    CompleteRequest(request_address);
    return true;
}

} // namespace vwii::ios
