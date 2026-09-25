#include "boot/image_loader.h"

#include "memory/memory.h"

#include <fstream>
#include <stdexcept>

namespace vwii::boot {

namespace {

uint32_t ReadBE32(const std::vector<uint8_t>& data, std::size_t offset) {
    if (offset + 4 > data.size())
        throw std::out_of_range("image header is truncated");

    return (static_cast<uint32_t>(data[offset]) << 24) |
           (static_cast<uint32_t>(data[offset + 1]) << 16) |
           (static_cast<uint32_t>(data[offset + 2]) << 8) |
           static_cast<uint32_t>(data[offset + 3]);
}

bool RangeInside(std::size_t offset, std::size_t size, std::size_t total) {
    return offset <= total && size <= total - offset;
}

LoadResult LoadDol(const std::vector<uint8_t>& image, memory::Memory& memory) {
    if (image.size() < 0xE0)
        return {false, ImageType::Dol, 0, "DOL image is smaller than its header"};

    uint32_t text_offsets[7]{};
    uint32_t data_offsets[11]{};
    uint32_t text_addresses[7]{};
    uint32_t data_addresses[11]{};
    uint32_t text_sizes[7]{};
    uint32_t data_sizes[11]{};

    for (std::size_t i = 0; i < 7; ++i) {
        text_offsets[i] = ReadBE32(image, i * 4);
        text_addresses[i] = ReadBE32(image, 0x48 + i * 4);
        text_sizes[i] = ReadBE32(image, 0x90 + i * 4);
    }

    for (std::size_t i = 0; i < 11; ++i) {
        data_offsets[i] = ReadBE32(image, 0x1C + i * 4);
        data_addresses[i] = ReadBE32(image, 0x64 + i * 4);
        data_sizes[i] = ReadBE32(image, 0xA4 + i * 4);
    }

    const uint32_t bss_address = ReadBE32(image, 0xD0);
    const uint32_t bss_size = ReadBE32(image, 0xD4);
    const uint32_t entry_point = ReadBE32(image, 0xD8);

    for (std::size_t i = 0; i < 7; ++i) {
        if (text_sizes[i] == 0)
            continue;
        if (!RangeInside(text_offsets[i], text_sizes[i], image.size()))
            return {false, ImageType::Dol, 0, "DOL text segment is outside the file"};

        memory.WriteBlock(
            text_addresses[i],
            std::span<const uint8_t>(image.data() + text_offsets[i], text_sizes[i]));
    }

    for (std::size_t i = 0; i < 11; ++i) {
        if (data_sizes[i] == 0)
            continue;
        if (!RangeInside(data_offsets[i], data_sizes[i], image.size()))
            return {false, ImageType::Dol, 0, "DOL data segment is outside the file"};

        memory.WriteBlock(
            data_addresses[i],
            std::span<const uint8_t>(image.data() + data_offsets[i], data_sizes[i]));
    }

    if (bss_size != 0)
        memory.Fill(bss_address, bss_size, 0);

    return {true, ImageType::Dol, entry_point, {}};
}

LoadResult LoadElf32(const std::vector<uint8_t>& image, memory::Memory& memory) {
    if (image.size() < 52)
        return {false, ImageType::Elf32, 0, "ELF32 image is smaller than its header"};

    if (image[0] != 0x7F || image[1] != 'E' || image[2] != 'L' || image[3] != 'F')
        return {false, ImageType::Elf32, 0, "not an ELF image"};

    if (image[4] != 1)
        return {false, ImageType::Elf32, 0, "only ELF32 is supported"};

    if (image[5] != 2)
        return {false, ImageType::Elf32, 0, "only big-endian ELF images are supported"};

    if (image[6] != 1)
        return {false, ImageType::Elf32, 0, "unsupported ELF identification version"};

    const uint16_t machine =
        static_cast<uint16_t>((static_cast<uint16_t>(image[0x12]) << 8) | image[0x13]);

    if (machine != 20)
        return {false, ImageType::Elf32, 0, "ELF machine is not PowerPC"};

    const uint32_t entry = ReadBE32(image, 0x18);
    const uint32_t program_header_offset = ReadBE32(image, 0x1C);
    const uint16_t program_header_size =
        static_cast<uint16_t>((static_cast<uint16_t>(image[0x2A]) << 8) | image[0x2B]);
    const uint16_t program_header_count =
        static_cast<uint16_t>((static_cast<uint16_t>(image[0x2C]) << 8) | image[0x2D]);

    if (program_header_size < 32)
        return {false, ImageType::Elf32, 0, "ELF32 program header is too small"};

    const std::size_t table_size =
        static_cast<std::size_t>(program_header_size) * program_header_count;

    if (!RangeInside(program_header_offset, table_size, image.size()))
        return {false, ImageType::Elf32, 0, "ELF32 program headers are outside the file"};

    bool loaded_segment = false;

    for (uint16_t i = 0; i < program_header_count; ++i) {
        const std::size_t offset =
            program_header_offset +
            static_cast<std::size_t>(i) * program_header_size;

        if (ReadBE32(image, offset) != 1) // PT_LOAD
            continue;

        const uint32_t file_offset = ReadBE32(image, offset + 4);
        const uint32_t virtual_address = ReadBE32(image, offset + 8);
        const uint32_t file_size = ReadBE32(image, offset + 16);
        const uint32_t memory_size = ReadBE32(image, offset + 20);

        if (file_size > memory_size)
            return {false, ImageType::Elf32, 0, "ELF32 load segment has invalid sizes"};

        if (!RangeInside(file_offset, file_size, image.size()))
            return {false, ImageType::Elf32, 0, "ELF32 load segment is outside the file"};

        memory.WriteBlock(
            virtual_address,
            std::span<const uint8_t>(image.data() + file_offset, file_size));

        if (memory_size > file_size)
            memory.Fill(virtual_address + file_size, memory_size - file_size, 0);

        loaded_segment = true;
    }

    if (!loaded_segment)
        return {false, ImageType::Elf32, 0, "ELF32 contains no loadable segments"};

    return {true, ImageType::Elf32, entry, {}};
}

} // namespace

LoadResult LoadImage(const std::vector<uint8_t>& image, memory::Memory& memory) {
    if (image.size() >= 4 &&
        image[0] == 0x7F && image[1] == 'E' && image[2] == 'L' && image[3] == 'F') {
        return LoadElf32(image, memory);
    }

    return LoadDol(image, memory);
}

LoadResult LoadImageFile(const std::string& path, memory::Memory& memory) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        return {false, ImageType::Dol, 0, "unable to open image: " + path};

    const std::streampos end = file.tellg();
    if (end < 0)
        return {false, ImageType::Dol, 0, "unable to determine image size: " + path};

    const auto size = static_cast<std::size_t>(end);
    std::vector<uint8_t> image(size);

    file.seekg(0);
    if (size != 0 &&
        !file.read(reinterpret_cast<char*>(image.data()), static_cast<std::streamsize>(size))) {
        return {false, ImageType::Dol, 0, "unable to read image: " + path};
    }

    return LoadImage(image, memory);
}

} // namespace vwii::boot
