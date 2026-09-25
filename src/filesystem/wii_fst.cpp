#include "filesystem/wii_fst.h"

#include "disc/disc_image.h"

#include <algorithm>
#include <array>
#include <string>

namespace vwii::filesystem {

namespace {

uint32_t ReadBE32(const std::vector<uint8_t>& data, std::size_t offset) {
    return (static_cast<uint32_t>(data[offset]) << 24) |
           (static_cast<uint32_t>(data[offset + 1]) << 16) |
           (static_cast<uint32_t>(data[offset + 2]) << 8) |
           static_cast<uint32_t>(data[offset + 3]);
}

} // namespace

bool WiiFST::Load(disc::DiscImage& disc) {
    entries_.clear();

    std::array<uint8_t, 0x30> boot{};
    if (!disc.ReadGamePartition(0, boot.data(), boot.size()))
        return false;

    const uint32_t fst_offset = ReadBE32(
        std::vector<uint8_t>(boot.begin(), boot.end()), 0x24);
    const uint32_t fst_size = ReadBE32(
        std::vector<uint8_t>(boot.begin(), boot.end()), 0x28);

    if (fst_size < 12 || fst_size > 64 * 1024 * 1024)
        return false;

    std::vector<uint8_t> fst(fst_size);
    if (!disc.ReadGamePartition(fst_offset, fst.data(), fst.size()))
        return false;

    if (fst.size() < 12)
        return false;

    const uint32_t entry_count = ReadBE32(fst, 8);
    if (entry_count == 0 ||
        12ULL * entry_count > fst.size())
        return false;

    const uint64_t string_table_start = 12ULL * entry_count;
    if (string_table_start > fst.size())
        return false;

    entries_.reserve(entry_count);

    struct DirectoryContext {
        uint32_t end{};
        std::string path;
    };

    std::vector<DirectoryContext> stack;
    stack.push_back({entry_count, {}});

    for (uint32_t i = 0; i < entry_count; ++i) {
        while (stack.size() > 1 && i >= stack.back().end)
            stack.pop_back();

        const std::size_t base = static_cast<std::size_t>(i) * 12;
        const uint32_t type_name = ReadBE32(fst, base);
        const bool directory = (type_name & 0x01000000U) != 0;
        const uint32_t name_offset = type_name & 0x00FFFFFFU;

        if (string_table_start + name_offset >= fst.size())
            return false;

        std::string name;
        for (std::size_t p = string_table_start + name_offset; p < fst.size(); ++p) {
            const char c = static_cast<char>(fst[p]);
            if (c == '\0')
                break;
            name.push_back(c);
        }

        if (string_table_start + name_offset + name.size() >= fst.size())
            return false;

        FileEntry entry;
        if (i == 0) {
            entry.path = "/";
            entry.directory = true;
        } else {
            entry.path = stack.back().path;
            if (entry.path.empty())
                entry.path = "/" + name;
            else
                entry.path += "/" + name;

            entry.directory = directory;
        }

        if (directory) {
            const uint32_t parent = ReadBE32(fst, base + 4);
            const uint32_t next = ReadBE32(fst, base + 8);

            if (i != 0 && parent >= entry_count)
                return false;
            if (next <= i || next > entry_count)
                return false;

            entry.offset = parent;
            entry.size = next;

            entries_.push_back(entry);

            if (i == 0)
                stack.back().path.clear();
            else
                stack.push_back({next, entry.path});
        } else {
            entry.offset = ReadBE32(fst, base + 4);
            entry.size = ReadBE32(fst, base + 8);
            entries_.push_back(entry);
        }
    }

    return true;
}

const FileEntry* WiiFST::Find(std::string_view path) const {
    if (path.empty())
        path = "/";

    for (const auto& entry : entries_) {
        if (entry.path == path)
            return &entry;
    }

    return nullptr;
}

bool WiiFST::ReadFile(disc::DiscImage& disc,
                      std::string_view path,
                      std::vector<uint8_t>& output) const {
    const FileEntry* entry = Find(path);
    if (!entry || entry->directory)
        return false;

    output.resize(entry->size);
    return disc.ReadGamePartition(entry->offset,
                                  output.data(),
                                  output.size());
}

} // namespace vwii::filesystem
