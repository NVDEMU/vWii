#include "core/emulator.h"

#include <array>
#include <vector>

namespace vwii::core {

Emulator::Emulator()
    : memory_(), cpu_(memory_), ios_(memory_) {
}

bool Emulator::Initialize() {
    memory_.Reset();
    cpu_.Reset();
    ios_.Reset();
    scheduler_.Reset();
    disc_.reset();

    initialized_ = true;
    loaded_image_ = false;

    ScheduleVideoInterrupt();
    return true;
}

void Emulator::Shutdown() {
    scheduler_.Reset();
    disc_.reset();
    initialized_ = false;
    loaded_image_ = false;
}

void Emulator::Reset() {
    memory_.Reset();
    cpu_.Reset();
    ios_.Reset();
    scheduler_.Reset();

    disc_.reset();
    loaded_image_ = false;

    if (initialized_)
        ScheduleVideoInterrupt();
}

void Emulator::ScheduleVideoInterrupt() {
    scheduler_.Schedule(16667, [this]() {
        memory_.Hollywood().RaisePpcInterrupt(24);
        if (initialized_)
            ScheduleVideoInterrupt();
    });
}

void Emulator::Step() {
    if (!initialized_)
        return;

    // HLE IOS on the Starlet before running another Broadway instruction.
    ios_.Tick();

    cpu_.Step();

    // One scheduler tick per interpreted guest instruction. The value is
    // intentionally abstract until the Broadway timebase is implemented.
    scheduler_.Advance(1);
}

void Emulator::RunForInstructions(uint64_t count) {
    for (uint64_t i = 0; i < count; ++i)
        Step();
}

boot::LoadResult Emulator::LoadImage(const std::vector<uint8_t>& image) {
    memory_.Reset();
    disc_.reset();

    const boot::LoadResult result = boot::LoadImage(image, memory_);
    if (result.success) {
        cpu_.Reset(result.entry_point);
        loaded_image_ = true;
    } else {
        loaded_image_ = false;
    }

    return result;
}

boot::LoadResult Emulator::LoadImageFile(const std::string& path) {
    memory_.Reset();
    disc_.reset();

    const boot::LoadResult result = boot::LoadImageFile(path, memory_);
    if (result.success) {
        cpu_.Reset(result.entry_point);
        loaded_image_ = true;
    } else {
        loaded_image_ = false;
    }

    return result;
}

boot::WiiBootResult Emulator::LoadWiiGame(const std::string& path) {
    memory_.Reset();
    disc_.reset();

    auto candidate = std::make_unique<disc::DiscImage>();
    if (!candidate->Open(path)) {
        boot::WiiBootResult result;
        result.error = candidate->Info().error.empty()
            ? "Unable to open RVZ image"
            : candidate->Info().error;
        return result;
    }

    boot::WiiBootResult result =
        boot::LoadWiiGame(*candidate, memory_);

    if (!result.success) {
        loaded_image_ = false;
        return result;
    }

    disc_ = std::move(candidate);
    ios_.AttachDisc(disc_.get());

    // Parse the game FST early so IOS/boot code can reuse it.
    if (!game_fst_.Load(*disc_)) {
        // Keep booting for now; some early compatibility tests intentionally
        // use incomplete images.
    }

    // Recreate the disc/system globals normally prepared by the early Wii
    // boot chain. The Main DOL is still loaded directly at this stage, but
    // the title sees the same core memory metadata.
    std::array<uint8_t, 0x100> disc_header{};
    if (disc_->Read(0, disc_header.data(), disc_header.size()))
        memory_.WriteBlock(0x80000000, disc_header);

    memory_.Write32(0x80000028, memory::Memory::MEM1_SIZE);
    memory_.Write32(0x8000002C, 0x00000023);
    memory_.Write32(0x800000F0, memory::Memory::MEM1_SIZE);
    memory_.Write32(0x800000F8, 0x0E7BE2C0);
    memory_.Write32(0x800000FC, 0x2B73A840);
    memory_.Write8(0x8000319C, 0x80);

    std::array<uint8_t, 0x440> boot_header{};
    if (!disc_->ReadGamePartition(0, boot_header.data(), boot_header.size())) {
        loaded_image_ = false;
        result.success = false;
        result.error = "Unable to read the Wii partition boot header";
        return result;
    }

    auto ReadBootBE32 = [&](std::size_t offset) -> uint32_t {
        return (static_cast<uint32_t>(boot_header[offset]) << 24) |
               (static_cast<uint32_t>(boot_header[offset + 1]) << 16) |
               (static_cast<uint32_t>(boot_header[offset + 2]) << 8) |
               static_cast<uint32_t>(boot_header[offset + 3]);
    };

    const uint32_t fst_offset = ReadBootBE32(0x424);
    const uint32_t fst_size = ReadBootBE32(0x428);

    if (fst_size != 0 && fst_size <= 2 * 1024 * 1024) {
        std::vector<uint8_t> fst(fst_size);
        if (disc_->ReadGamePartition(fst_offset, fst.data(), fst.size())) {
            // Keep the FST below the bi2 scratch area and the initial stack.
            constexpr uint32_t fst_address = 0x81600000;
            constexpr uint32_t fst_capacity = 0x00200000;

            memory_.WriteBlock(fst_address, fst);
            memory_.Write32(0x80000038, fst_address);
            memory_.Write32(
                0x8000003C,
                (fst_size + 31u) & ~31u);
            (void)fst_capacity;
        }
    }

    std::array<uint8_t, 0x2000> bi2{};
    if (disc_->ReadGamePartition(0x440, bi2.data(), bi2.size())) {
        memory_.WriteBlock(0x817FDF80, bi2);
        memory_.Write32(0x800000F4, 0x817FDF80);
    }

    cpu_.Reset(result.dol_result.entry_point);

    // The real boot chain establishes an initial PPC stack before entering
    // the apploader/Main DOL. We bypass executable apploader code for now,
    // so establish a conservative stack near the top of MEM1.
    cpu_.SetGPR(1, 0x817F8000);
    cpu_.SetGPR(2, 0);
    loaded_image_ = true;
    return result;
}

} // namespace vwii::core
