#include "disc/disc_image.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>

#include <zstd.h>

namespace vwii::disc {

namespace {

constexpr uint64_t WiiSectorSize = 0x8000;
constexpr uint64_t WiiUserDataSize = 0x7C00;

uint32_t ReadBE32Raw(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

uint64_t ReadBE64Raw(const uint8_t* data) {
    return (static_cast<uint64_t>(ReadBE32Raw(data)) << 32) |
           ReadBE32Raw(data + 4);
}

bool RangeInside(uint64_t offset, uint64_t size, uint64_t total) {
    return offset <= total && size <= total - offset;
}

void AdvancePrng(std::array<uint32_t, 521>& state) {
    for (std::size_t i = 0; i < 32; ++i)
        state[i] ^= state[i + 521 - 32];

    for (std::size_t i = 32; i < state.size(); ++i)
        state[i] ^= state[i - 32];
}

void InitializePrng(std::array<uint32_t, 521>& state, const uint8_t* seed) {
    for (std::size_t i = 0; i < 17; ++i)
        state[i] = ReadBE32Raw(seed + i * 4);

    for (std::size_t i = 17; i < state.size(); ++i) {
        state[i] = (state[i - 17] << 23) ^
                   (state[i - 16] >> 9) ^
                   state[i - 1];
    }

    for (int i = 0; i < 4; ++i)
        AdvancePrng(state);
}

void GeneratePrngBytes(std::array<uint32_t, 521>& state,
                       uint32_t skip_bytes, uint8_t* output,
                       uint32_t count) {
    std::size_t state_index = 0;

    auto next_word = [&]() {
        if (state_index == state.size()) {
            AdvancePrng(state);
            state_index = 0;
        }
        return state[state_index++];
    };

    while (skip_bytes >= 4) {
        (void)next_word();
        skip_bytes -= 4;
    }

    if (skip_bytes != 0 && count != 0) {
        const uint32_t word = next_word();
        const uint8_t bytes[4] = {
            static_cast<uint8_t>(word >> 24),
            static_cast<uint8_t>(word >> 18),
            static_cast<uint8_t>(word >> 8),
            static_cast<uint8_t>(word)
        };

        for (uint32_t i = skip_bytes; i < 4 && count != 0; ++i) {
            *output++ = bytes[i];
            --count;
        }
    }

    while (count != 0) {
        const uint32_t word = next_word();
        const uint8_t bytes[4] = {
            static_cast<uint8_t>(word >> 24),
            static_cast<uint8_t>(word >> 18),
            static_cast<uint8_t>(word >> 8),
            static_cast<uint8_t>(word)
        };

        const uint32_t amount = std::min<uint32_t>(count, 4);
        std::memcpy(output, bytes, amount);
        output += amount;
        count -= amount;
    }
}

std::size_t ExceptionListCount(uint32_t chunk_size) {
    return chunk_size < 0x200000 ? 1 : chunk_size / 0x200000;
}

bool StripExceptionLists(std::vector<uint8_t>& decoded,
                         uint32_t chunk_size,
                         bool compressed) {
    std::size_t cursor = 0;

    for (std::size_t list = 0; list < ExceptionListCount(chunk_size); ++list) {
        if (decoded.size() - cursor < 2)
            return false;

        const uint16_t count = static_cast<uint16_t>(
            (static_cast<uint16_t>(decoded[cursor]) << 8) |
            decoded[cursor + 1]);
        cursor += 2;

        const uint64_t bytes = static_cast<uint64_t>(count) * 22;
        if (bytes > decoded.size() - cursor)
            return false;

        cursor += static_cast<std::size_t>(bytes);
    }

    if (!compressed)
        cursor = (cursor + 3) & ~static_cast<std::size_t>(3);

    if (cursor > decoded.size())
        return false;

    decoded.erase(decoded.begin(),
                  decoded.begin() + static_cast<std::ptrdiff_t>(cursor));
    return true;
}

} // namespace

DiscImage::DiscImage() = default;
DiscImage::~DiscImage() { Close(); }

void DiscImage::Close() {
    if (file_.is_open())
        file_.close();

    file_size_ = 0;
    info_ = {};
    rvz_compression_ = 0;
    rvz_chunk_size_ = 0;
    rvz_group_count_ = 0;
    groups_.clear();
    raw_regions_.clear();
    partitions_.clear();
    disc_header_.clear();
    game_partition_index_ = static_cast<std::size_t>(-1);
    game_partition_disc_offset_ = 0;
    game_partition_data_size_ = 0;
}

bool DiscImage::Open(const std::string& path) {
    Close();

    file_.open(path, std::ios::binary);
    if (!file_) {
        info_.error = "Unable to open disc image";
        return false;
    }

    file_.seekg(0, std::ios::end);
    const std::streampos end = file_.tellg();
    if (end < 0) {
        info_.error = "Unable to determine disc image size";
        Close();
        return false;
    }

    file_size_ = static_cast<uint64_t>(end);
    file_.seekg(0, std::ios::beg);

    std::array<char, 4> magic{};
    file_.read(magic.data(), 4);
    if (file_.gcount() != 4) {
        info_.error = "Disc image is too small";
        Close();
        return false;
    }

    file_.clear();
    file_.seekg(0, std::ios::beg);

    if (std::memcmp(magic.data(), "WIA\x01", 4) != 0) {
        info_.error = "Only RVZ disc images are supported by the current disc backend";
        return false;
    }

    if (!OpenRvz()) {
        if (info_.error.empty())
            info_.error = "Invalid or unsupported RVZ image";
        Close();
        return false;
    }

    return true;
}

bool DiscImage::ReadFile(uint64_t offset, void* destination, std::size_t size) {
    if (!RangeInside(offset, size, file_size_))
        return false;

    if (size == 0)
        return true;

    file_.clear();
    file_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!file_)
        return false;

    file_.read(static_cast<char*>(destination),
               static_cast<std::streamsize>(size));
    return file_.gcount() == static_cast<std::streamsize>(size);
}

uint32_t DiscImage::ReadBE32(const std::vector<uint8_t>& data,
                             std::size_t offset) const {
    if (offset + 4 > data.size())
        throw std::out_of_range("RVZ structure truncated");
    return ReadBE32Raw(data.data() + offset);
}

uint64_t DiscImage::ReadBE64(const std::vector<uint8_t>& data,
                             std::size_t offset) const {
    if (offset + 8 > data.size())
        throw std::out_of_range("RVZ structure truncated");
    return ReadBE64Raw(data.data() + offset);
}

bool DiscImage::LoadRvzTables() {
    std::vector<uint8_t> disc(0xDC);
    if (!ReadFile(0x48, disc.data(), disc.size()))
        return false;

    if (ReadBE32(disc, 0x00) != 2)
        return false;

    rvz_compression_ = ReadBE32(disc, 0x04);
    rvz_chunk_size_ = ReadBE32(disc, 0x0C);

    const uint32_t partition_count = ReadBE32(disc, 0x90);
    const uint32_t partition_entry_size = ReadBE32(disc, 0x94);
    const uint64_t partition_table_offset = ReadBE64(disc, 0x98);

    const uint32_t raw_count = ReadBE32(disc, 0xB4);
    const uint64_t raw_offset = ReadBE64(disc, 0xB8);
    const uint32_t raw_size = ReadBE32(disc, 0xC0);

    rvz_group_count_ = ReadBE32(disc, 0xC4);
    const uint64_t group_offset = ReadBE64(disc, 0xC8);
    const uint32_t group_size = ReadBE32(disc, 0xD0);

    if (rvz_chunk_size_ < WiiSectorSize || rvz_group_count_ == 0)
        return false;

    if (partition_entry_size < 48 ||
        partition_count > 4096 ||
        raw_count > 4096 ||
        rvz_group_count_ > 100000000)
        return false;

    if (rvz_compression_ != 0 && rvz_compression_ != 5) {
        info_.error = "RVZ compression is not NONE or Zstandard";
        return false;
    }

    const auto readTable = [&](uint64_t offset, uint32_t stored_size,
                               std::size_t expected_size,
                               std::vector<uint8_t>& output) -> bool {
        if (!RangeInside(offset, stored_size, file_size_))
            return false;

        std::vector<uint8_t> stored(stored_size);
        if (!ReadFile(offset, stored.data(), stored.size()))
            return false;

        output.resize(expected_size);

        if (rvz_compression_ == 0) {
            if (stored.size() != output.size())
                return false;

            output = std::move(stored);
            return true;
        }

        const std::size_t result =
            ZSTD_decompress(output.data(), output.size(),
                            stored.data(), stored.size());

        return !ZSTD_isError(result) && result == output.size();
    };

    if (raw_count > std::numeric_limits<std::size_t>::max() / 24 ||
        rvz_group_count_ > std::numeric_limits<std::size_t>::max() / 12)
        return false;

    std::vector<uint8_t> raw_table;
    std::vector<uint8_t> group_table;

    if (!readTable(raw_offset, raw_size,
                   static_cast<std::size_t>(raw_count) * 24, raw_table))
        return false;

    if (!readTable(group_offset, group_size,
                   static_cast<std::size_t>(rvz_group_count_) * 12,
                   group_table))
        return false;

    groups_.resize(rvz_group_count_);
    for (uint32_t i = 0; i < rvz_group_count_; ++i) {
        const std::size_t off = static_cast<std::size_t>(i) * 12;
        const uint32_t data_off4 = ReadBE32(group_table, off);
        const uint32_t data_size = ReadBE32(group_table, off + 4);

        groups_[i].file_offset = static_cast<uint64_t>(data_off4) * 4;
        groups_[i].stored_size = data_size & 0x7FFFFFFFU;
        groups_[i].packed_size = ReadBE32(group_table, off + 8);
        groups_[i].compressed = (data_size & 0x80000000U) != 0;
    }

    raw_regions_.resize(raw_count);
    for (uint32_t i = 0; i < raw_count; ++i) {
        const std::size_t off = static_cast<std::size_t>(i) * 24;
        auto& region = raw_regions_[i];

        region.logical_offset = ReadBE64(raw_table, off);
        region.logical_size = ReadBE64(raw_table, off + 8);
        region.first_group = ReadBE32(raw_table, off + 16);
        region.group_count = ReadBE32(raw_table, off + 20);

        if (region.first_group > rvz_group_count_ ||
            region.group_count > rvz_group_count_ - region.first_group)
            return false;
    }

    disc_header_.assign(disc.begin() + 0x10, disc.begin() + 0x90);

    if (partition_count > std::numeric_limits<std::size_t>::max() /
                              partition_entry_size)
        return false;

    const uint64_t partition_table_size =
        static_cast<uint64_t>(partition_count) * partition_entry_size;

    if (!RangeInside(partition_table_offset, partition_table_size, file_size_))
        return false;

    std::vector<uint8_t> partition_table(
        static_cast<std::size_t>(partition_table_size));

    if (!ReadFile(partition_table_offset, partition_table.data(),
                  partition_table.size()))
        return false;

    partitions_.resize(partition_count);
    for (uint32_t i = 0; i < partition_count; ++i) {
        const std::size_t base =
            static_cast<std::size_t>(i) * partition_entry_size;

        std::copy_n(partition_table.data() + base, 16,
                    partitions_[i].title_key.begin());

        for (int r = 0; r < 2; ++r) {
            const std::size_t off = base + 16 + static_cast<std::size_t>(r) * 16;
            auto& region = partitions_[i].regions[r];

            region.first_sector = ReadBE32(partition_table, off);
            region.sector_count = ReadBE32(partition_table, off + 4);
            region.first_group = ReadBE32(partition_table, off + 8);
            region.group_count = ReadBE32(partition_table, off + 12);

            if (region.first_group > rvz_group_count_ ||
                region.group_count > rvz_group_count_ - region.first_group)
                return false;
        }
    }

    info_.partition_count = partition_count;
    return true;
}

bool DiscImage::OpenRvz() {
    if (file_size_ < 0xDC)
        return false;

    std::vector<uint8_t> disc(0xDC);
    if (!ReadFile(0x48, disc.data(), disc.size()))
        return false;

    if (ReadBE32(disc, 0x00) != 2)
        return false;

    info_.format = "RVZ";
    info_.wii = true;
    info_.disc_size = ReadBE64(disc, 0x10);
    info_.game_id.assign(
        reinterpret_cast<const char*>(disc.data() + 0x10), 6);

    if (!LoadRvzTables())
        return false;

    info_.valid = true;

    std::array<uint8_t, 0x20> partition_header{};
    if (!Read(0x40000, partition_header.data(), partition_header.size()))
        return false;

    std::vector<std::pair<uint64_t, uint32_t>> disc_partitions;

    for (int table = 0; table < 4; ++table) {
        const uint32_t count =
            ReadBE32Raw(partition_header.data() + table * 8);
        const uint32_t table_words =
            ReadBE32Raw(partition_header.data() + table * 8 + 4);

        if (count == 0)
            continue;

        const uint64_t table_offset =
            static_cast<uint64_t>(table_words) * 4;
        const uint64_t table_size =
            static_cast<uint64_t>(count) * 8;

        if (!RangeInside(table_offset, table_size, info_.disc_size))
            return false;

        std::vector<uint8_t> entries(static_cast<std::size_t>(table_size));
        if (!Read(table_offset, entries.data(), entries.size()))
            return false;

        for (uint32_t i = 0; i < count; ++i) {
            const std::size_t off = static_cast<std::size_t>(i) * 8;
            disc_partitions.emplace_back(
                static_cast<uint64_t>(ReadBE32Raw(entries.data() + off)) * 4,
                ReadBE32Raw(entries.data() + off + 4));
        }
    }

    if (disc_partitions.empty())
        return false;

    std::size_t record = 0;
    for (const auto& [partition_offset, type] : disc_partitions) {
        if (record >= partitions_.size())
            return false;

        if (type == 0) {
            game_partition_index_ = record;
            game_partition_disc_offset_ = partition_offset;
            break;
        }

        ++record;
    }

    if (game_partition_index_ == static_cast<std::size_t>(-1))
        return false;

    game_partition_data_size_ = 0;
    for (const auto& region : partitions_[game_partition_index_].regions) {
        game_partition_data_size_ +=
            static_cast<uint64_t>(region.sector_count) * WiiUserDataSize;
    }

    return game_partition_data_size_ != 0;
}

bool DiscImage::DecodeRvzPacking(const std::vector<uint8_t>& packed,
                                 uint64_t logical_offset,
                                 std::vector<uint8_t>& output) {
    std::size_t cursor = 0;
    output.clear();

    while (cursor < packed.size()) {
        if (packed.size() - cursor < 4)
            return false;

        const uint32_t encoded = ReadBE32Raw(packed.data() + cursor);
        cursor += 4;

        const bool random_data = (encoded & 0x80000000U) != 0;
        const uint32_t size = encoded & 0x7FFFFFFFU;

        if (!random_data) {
            if (size > packed.size() - cursor)
                return false;

            output.insert(output.end(),
                          packed.begin() + static_cast<std::ptrdiff_t>(cursor),
                          packed.begin() + static_cast<std::ptrdiff_t>(cursor + size));
            cursor += size;
            continue;
        }

        if (packed.size() - cursor < 68)
            return false;

        if (size != 0) {
            std::array<uint32_t, 521> state{};
            InitializePrng(state, packed.data() + cursor);

            const uint32_t skip = static_cast<uint32_t>(
                (logical_offset + output.size()) % WiiSectorSize);

            const std::size_t old_size = output.size();
            output.resize(old_size + size);
            GeneratePrngBytes(state, skip, output.data() + old_size, size);
        }

        cursor += 68;
    }

    return true;
}

bool DiscImage::ReadRvzGroup(const Group& group, std::vector<uint8_t>& output) {
    if (group.stored_size == 0) {
        output.assign(group.logical_size, 0);
        return true;
    }

    std::vector<uint8_t> stored(group.stored_size);
    if (!ReadFile(group.file_offset, stored.data(), stored.size()))
        return false;

    std::vector<uint8_t> decoded;

    if (group.compressed) {
        if (rvz_compression_ != 5)
            return false;

        std::size_t expected = group.packed_size;
        if (expected == 0) {
            const unsigned long long frame_size =
                ZSTD_getFrameContentSize(stored.data(), stored.size());

            if (frame_size == ZSTD_CONTENTSIZE_ERROR ||
                frame_size == ZSTD_CONTENTSIZE_UNKNOWN ||
                frame_size > 256ULL * 1024ULL * 1024ULL)
                return false;

            expected = static_cast<std::size_t>(frame_size);
        }

        decoded.resize(expected);
        const std::size_t result =
            ZSTD_decompress(decoded.data(), decoded.size(),
                            stored.data(), stored.size());

        if (ZSTD_isError(result) || result != decoded.size())
            return false;
    } else {
        decoded = std::move(stored);
    }

    if (group.packed_size != 0) {
        std::vector<uint8_t> unpacked;
        if (!DecodeRvzPacking(decoded, group.logical_offset, unpacked))
            return false;
        output = std::move(unpacked);
    } else {
        output = std::move(decoded);
    }

    return output.size() >= group.logical_size;
}

bool DiscImage::ReadRawRvz(uint64_t offset, void* destination,
                           std::size_t size) {
    if (!RangeInside(offset, size, info_.disc_size))
        return false;

    auto* out = static_cast<uint8_t*>(destination);
    std::size_t remaining = size;

    while (remaining != 0) {
        if (offset < disc_header_.size()) {
            const std::size_t amount = std::min<std::size_t>(
                remaining,
                disc_header_.size() - static_cast<std::size_t>(offset));

            std::memcpy(out, disc_header_.data() + offset, amount);
            out += amount;
            offset += amount;
            remaining -= amount;
            continue;
        }

        bool found = false;

        for (const auto& region : raw_regions_) {
            const uint64_t region_end = region.logical_offset +
                                        region.logical_size;

            if (offset < region.logical_offset || offset >= region_end)
                continue;

            const uint64_t relative = offset - region.logical_offset;
            const uint64_t group_data_size = rvz_chunk_size_;
            const uint64_t group_number = relative / group_data_size;

            if (group_number >= region.group_count)
                return false;

            Group group =
                groups_[region.first_group + static_cast<uint32_t>(group_number)];

            group.logical_offset =
                region.logical_offset + group_number * group_data_size;
            group.logical_size = static_cast<uint32_t>(
                std::min<uint64_t>(group_data_size,
                                   region_end - group.logical_offset));

            std::vector<uint8_t> decoded;
            if (!ReadRvzGroup(group, decoded))
                return false;

            const uint64_t in_group = offset - group.logical_offset;
            if (in_group >= decoded.size())
                return false;

            const std::size_t amount = std::min<std::size_t>(
                remaining,
                decoded.size() - static_cast<std::size_t>(in_group));

            std::memcpy(out, decoded.data() + in_group, amount);

            out += amount;
            offset += amount;
            remaining -= amount;
            found = true;
            break;
        }

        if (!found)
            return false;
    }

    return true;
}

bool DiscImage::ReadPartitionRvz(const PartitionRegion& region,
                                 uint64_t offset, void* destination,
                                 std::size_t size) {
    const uint64_t region_size =
        static_cast<uint64_t>(region.sector_count) * WiiUserDataSize;

    if (!RangeInside(offset, size, region_size))
        return false;

    const uint64_t group_logical_size =
        (static_cast<uint64_t>(rvz_chunk_size_) / WiiSectorSize) *
        WiiUserDataSize;

    if (group_logical_size == 0)
        return false;

    auto* out = static_cast<uint8_t*>(destination);
    std::size_t remaining = size;

    while (remaining != 0) {
        const uint64_t group_number = offset / group_logical_size;
        if (group_number >= region.group_count)
            return false;

        Group group =
            groups_[region.first_group + static_cast<uint32_t>(group_number)];

        group.logical_offset = group_number * group_logical_size;
        group.logical_size = static_cast<uint32_t>(
            std::min<uint64_t>(group_logical_size,
                               region_size - group.logical_offset));

        std::vector<uint8_t> decoded;
        if (!ReadRvzGroup(group, decoded))
            return false;

        if (!StripExceptionLists(decoded, rvz_chunk_size_, group.compressed))
            return false;

        if (decoded.size() < group.logical_size)
            return false;

        const uint64_t in_group = offset - group.logical_offset;
        if (in_group >= decoded.size())
            return false;

        const std::size_t amount = std::min<std::size_t>(
            remaining,
            decoded.size() - static_cast<std::size_t>(in_group));

        std::memcpy(out, decoded.data() + in_group, amount);

        out += amount;
        offset += amount;
        remaining -= amount;
    }

    return true;
}

bool DiscImage::Read(uint64_t offset, void* destination, std::size_t size) {
    return info_.valid && ReadRawRvz(offset, destination, size);
}

bool DiscImage::ReadGamePartition(uint64_t offset, void* destination,
                                  std::size_t size) {
    if (!info_.valid ||
        game_partition_index_ == static_cast<std::size_t>(-1) ||
        !RangeInside(offset, size, game_partition_data_size_))
        return false;

    const auto& partition = partitions_[game_partition_index_];
    uint64_t region_base = 0;

    for (const auto& region : partition.regions) {
        const uint64_t region_size =
            static_cast<uint64_t>(region.sector_count) * WiiUserDataSize;

        if (offset < region_base + region_size) {
            const uint64_t in_region = offset - region_base;

            if (size > region_size - in_region)
                return false;

            return ReadPartitionRvz(
                region, in_region, destination, size);
        }

        region_base += region_size;
    }

    return false;
}

bool DiscImage::LoadGameDol(std::vector<uint8_t>& dol,
                            uint32_t& entry_point) {
    if (!info_.valid ||
        game_partition_index_ == static_cast<std::size_t>(-1))
        return false;

    std::array<uint8_t, 0x440> boot{};
    if (!ReadGamePartition(0, boot.data(), boot.size()))
        return false;

    const uint32_t dol_offset = ReadBE32Raw(boot.data() + 0x420);

    std::array<uint8_t, 0x100> dol_header{};
    if (!ReadGamePartition(dol_offset, dol_header.data(), dol_header.size()))
        return false;

    const std::vector<uint8_t> header(dol_header.begin(), dol_header.end());

    uint32_t text_offsets[7]{};
    uint32_t text_sizes[7]{};
    uint32_t data_offsets[11]{};
    uint32_t data_sizes[11]{};

    for (int i = 0; i < 7; ++i) {
        text_offsets[i] = ReadBE32(header, i * 4);
        text_sizes[i] = ReadBE32(header, 0x90 + i * 4);
    }

    for (int i = 0; i < 11; ++i) {
        data_offsets[i] = ReadBE32(header, 0x1C + i * 4);
        data_sizes[i] = ReadBE32(header, 0xA4 + i * 4);
    }

    std::size_t total = 0x100;
    for (int i = 0; i < 7; ++i) {
        total = std::max(
            total,
            static_cast<std::size_t>(text_offsets[i]) + text_sizes[i]);
    }

    for (int i = 0; i < 11; ++i) {
        total = std::max(
            total,
            static_cast<std::size_t>(data_offsets[i]) + data_sizes[i]);
    }

    if (total > 64 * 1024 * 1024)
        return false;

    dol.assign(total, 0);
    std::copy(dol_header.begin(), dol_header.end(), dol.begin());

    for (int i = 0; i < 7; ++i) {
        if (text_sizes[i] == 0)
            continue;

        if (!RangeInside(text_offsets[i], text_sizes[i], total))
            return false;

        if (!ReadGamePartition(text_offsets[i],
                               dol.data() + text_offsets[i],
                               text_sizes[i]))
            return false;
    }

    for (int i = 0; i < 11; ++i) {
        if (data_sizes[i] == 0)
            continue;

        if (!RangeInside(data_offsets[i], data_sizes[i], total))
            return false;

        if (!ReadGamePartition(data_offsets[i],
                               dol.data() + data_offsets[i],
                               data_sizes[i]))
            return false;
    }

    entry_point = ReadBE32(header, 0xD8);
    return true;
}

} // namespace vwii::disc
