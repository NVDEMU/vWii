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

void InitializePrng(std::array<uint32_t, 521>& state,
                    const uint8_t* seed) {
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
    uint32_t state_index = 0;
    uint32_t skip_words = skip_bytes / 4;
    const uint32_t skip_remainder = skip_bytes % 4;

    while (skip_words != 0) {
        if (state_index == 521) {
            AdvancePrng(state);
            state_index = 0;
        }
        ++state_index;
        --skip_words;
    }

    if (skip_remainder != 0) {
        if (state_index == 521) {
            AdvancePrng(state);
            state_index = 0;
        }

        const uint32_t word = state[state_index++];
        const uint8_t bytes[4] = {
            static_cast<uint8_t>(word >> 24),
            static_cast<uint8_t>(word >> 18),
            static_cast<uint8_t>(word >> 8),
            static_cast<uint8_t>(word)
        };

        for (uint32_t i = skip_remainder; i < 4 && count != 0; ++i) {
            *output++ = bytes[i];
            --count;
        }
    }

    while (count != 0) {
        if (state_index == 521) {
            AdvancePrng(state);
            state_index = 0;
        }

        const uint32_t word = state[state_index++];
        const uint8_t bytes[4] = {
            static_cast<uint8_t>(word >> 24),
            static_cast<uint8_t>(word >> 18),
            static_cast<uint8_t>(word >> 8),
            static_cast<uint8_t>(word)
        };

        const uint32_t amount = std::min<uint32_t>(4, count);
        std::memcpy(output, bytes, amount);
        output += amount;
        count -= amount;
    }
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
    if (!file_)
        return false;

    file_.seekg(0, std::ios::end);
    const std::streampos end = file_.tellg();
    if (end < 0) {
        Close();
        return false;
    }

    file_size_ = static_cast<uint64_t>(end);
    file_.seekg(0, std::ios::beg);

    std::array<char, 4> magic{};
    file_.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (file_.gcount() != 4) {
        Close();
        return false;
    }

    if (std::memcmp(magic.data(), "WIA\x01", 4) != 0) {
        info_.error = "Only RVZ disc images are supported by the current disc backend";
        return false;
    }

    file_.clear();
    file_.seekg(0, std::ios::beg);

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

    file_.read(static_cast<char*>(destination), static_cast<std::streamsize>(size));
    return file_.gcount() == static_cast<std::streamsize>(size);
}

uint32_t DiscImage::ReadBE32(const std::vector<uint8_t>& data,
                             std::size_t offset) const {
    if (offset + 4 > data.size())
        throw std::out_of_range("RVZ: truncated structure");
    return ReadBE32Raw(data.data() + offset);
}

uint64_t DiscImage::ReadBE64(const std::vector<uint8_t>& data,
                             std::size_t offset) const {
    if (offset + 8 > data.size())
        throw std::out_of_range("RVZ: truncated structure");
    return ReadBE64Raw(data.data() + offset);
}

bool DiscImage::LoadRvzTables() {
    // wia_disc_t / rvz_disc_t starts at file offset 0x48.
    // The RVZ descriptor is 0xDC bytes in current files.
    std::vector<uint8_t> disc_struct(0xDC);
    if (!ReadFile(0x48, disc_struct.data(), disc_struct.size()))
        return false;

    const uint32_t disc_type = ReadBE32(disc_struct, 0x00);
    if (disc_type != 2)
        return false;

    rvz_compression_ = ReadBE32(disc_struct, 0x04);
    rvz_chunk_size_ = ReadBE32(disc_struct, 0x0C);
    rvz_group_count_ = ReadBE32(disc_struct, 0x7C);

    const uint32_t partition_count = ReadBE32(disc_struct, 0x90);
    const uint32_t partition_entry_size = ReadBE32(disc_struct, 0x94);
    const uint64_t partition_table_offset = ReadBE64(disc_struct, 0x98);

    const uint32_t raw_count = ReadBE32(disc_struct, 0xB4);
    const uint64_t raw_offset = ReadBE64(disc_struct, 0xB8);
    const uint32_t raw_size = ReadBE32(disc_struct, 0xC0);

    const uint32_t group_count_from_header = ReadBE32(disc_struct, 0xC4);
    const uint64_t group_offset = ReadBE64(disc_struct, 0xC8);
    const uint32_t group_size = ReadBE32(disc_struct, 0xD0);

    rvz_group_count_ = group_count_from_header;

    if (rvz_chunk_size_ < 0x8000 || rvz_group_count_ == 0)
        return false;

    if (partition_entry_size < 48)
        return false;

    if (partition_count > 4096 || raw_count > 4096 ||
        rvz_group_count_ > 100000000)
        return false;

    if (rvz_compression_ != 0 && rvz_compression_ != 5) {
        info_.error = "This RVZ uses a compression method vWii does not support yet";
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
            std::copy(stored.begin(), stored.end(), output.begin());
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
                   static_cast<std::size_t>(rvz_group_count_) * 12, group_table))
        return false;

    groups_.resize(rvz_group_count_);
    for (uint32_t i = 0; i < rvz_group_count_; ++i) {
        const std::size_t off = static_cast<std::size_t>(i) * 12;
        const uint32_t data_off4 = ReadBE32(group_table, off);
        const uint32_t data_size = ReadBE32(group_table, off + 4);
        const uint32_t packed_size = ReadBE32(group_table, off + 8);

        groups_[i].file_offset = static_cast<uint64_t>(data_off4) * 4;
        groups_[i].stored_size = data_size & 0x7FFFFFFFU;
        groups_[i].packed_size = packed_size;
        groups_[i].compressed = (data_size & 0x80000000U) != 0;
    }

    raw_regions_.resize(raw_count);
    for (uint32_t i = 0; i < raw_count; ++i) {
        const std::size_t off = static_cast<std::size_t>(i) * 24;
        RawRegion region;
        region.logical_offset = ReadBE64(raw_table, off);
        region.logical_size = ReadBE64(raw_table, off + 8);
        region.first_group = ReadBE32(raw_table, off + 16);
        region.group_count = ReadBE32(raw_table, off + 20);

        if (region.first_group > rvz_group_count_ ||
            region.group_count > rvz_group_count_ - region.first_group)
            return false;

        raw_regions_[i] = region;
    }

    // The first 0x80 bytes of the disc are copied into wia_disc_t::dhead.
    disc_header_.assign(disc_struct.begin() + 0x10, disc_struct.begin() + 0x90);

    bool partition_table_in_file = true;
    if (partition_count > std::numeric_limits<std::size_t>::max() / partition_entry_size)
        partition_table_in_file = false;

    if (!partition_table_in_file)
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

        if (base + 48 > partition_table.size())
            return false;

        std::copy_n(partition_table.data() + base, 16,
                    partitions_[i].title_key.begin());

        for (int region_index = 0; region_index < 2; ++region_index) {
            const std::size_t r = base + 16 +
                                  static_cast<std::size_t>(region_index) * 16;

            auto& region = partitions_[i].regions[region_index];
            region.first_sector = ReadBE32(partition_table, r);
            region.sector_count = ReadBE32(partition_table, r + 4);
            region.first_group = ReadBE32(partition_table, r + 8);
            region.group_count = ReadBE32(partition_table, r + 12);

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

    std::vector<uint8_t> disc_struct(0xDC);
    if (!ReadFile(0x48, disc_struct.data(), disc_struct.size()))
        return false;

    if (ReadBE32(disc_struct, 0x00) != 2)
        return false;

    info_.format = "RVZ";
    info_.wii = true;
    info_.disc_size = ReadBE64(disc_struct, 0x10);
    info_.game_id.assign(
        reinterpret_cast<const char*>(disc_struct.data() + 0x10), 6);

    if (!LoadRvzTables())
        return false;

    info_.valid = true;

    // Locate partition entries in the logical Wii disc system area.
    std::array<uint8_t, 0x20> partition_map_header{};
    if (!Read(0x40000, partition_map_header.data(), partition_map_header.size()))
        return false;

    std::vector<std::pair<uint64_t, uint32_t>> disc_partitions;

    for (int map = 0; map < 4; ++map) {
        const uint32_t count =
            ReadBE32Raw(partition_map_header.data() + map * 8);
        const uint32_t offset_words =
            ReadBE32Raw(partition_map_header.data() + map * 8 + 4);

        if (count == 0)
            continue;

        const uint64_t table_offset = static_cast<uint64_t>(offset_words) * 4;
        const uint64_t table_size = static_cast<uint64_t>(count) * 8;

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

    std::size_t partition_record = 0;
    for (const auto& [partition_offset, type] : disc_partitions) {
        if (partition_record >= partitions_.size())
            return false;

        if (type == 0) {
            game_partition_index_ = partition_record;
            game_partition_disc_offset_ = partition_offset;
            break;
        }

        ++partition_record;
    }

    if (game_partition_index_ == static_cast<std::size_t>(-1))
        return false;

    const auto& partition = partitions_[game_partition_index_];

    for (const auto& region : partition.regions) {
        if (region.sector_count != 0)
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

        const uint32_t encoded_size = ReadBE32Raw(packed.data() + cursor);
        cursor += 4;

        const bool pseudo_random = (encoded_size & 0x80000000U) != 0;
        const uint32_t size = encoded_size & 0x7FFFFFFFU;

        if (!pseudo_random) {
            if (size > packed.size() - cursor)
                return false;
            output.insert(output.end(), packed.begin() + cursor,
                          packed.begin() + cursor + size);
            cursor += size;
            continue;
        }

        if (size > std::numeric_limits<std::size_t>::max() - output.size())
            return false;

        if (packed.size() - cursor < 68)
            return false;

        if (size == 0) {
            cursor += 68;
            continue;
        }

        std::array<uint32_t, 521> state{};
        InitializePrng(state, packed.data() + cursor);
        cursor += 68;

        std::vector<uint8_t> generated(size);
        const uint32_t skip = static_cast<uint32_t>(
            (logical_offset + output.size()) % WiiSectorSize);

        GeneratePrngBytes(state, skip, generated.data(), size);
        output.insert(output.end(), generated.begin(), generated.end());
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

    const std::size_t frame_size = group.packed_size != 0
        ? group.packed_size
        : ZSTD_getFrameContentSize(stored.data(), stored.size());

    if (group.compressed) {
        if (rvz_compression_ != 5)
            return false;

        if (frame_size == ZSTD_CONTENTSIZE_ERROR ||
            frame_size == ZSTD_CONTENTSIZE_UNKNOWN ||
            frame_size > 256ULL * 1024ULL * 1024ULL)
            return false;

        std::vector<uint8_t> decompressed(static_cast<std::size_t>(frame_size));
        const std::size_t result =
            ZSTD_decompress(decompressed.data(), decompressed.size(),
                            stored.data(), stored.size());

        if (ZSTD_isError(result) || result != decompressed.size())
            return false;

        if (group.packed_size != 0)
            return DecodeRvzPacking(decompressed, group.logical_offset, output);

        output = std::move(decompressed);
        return output.size() >= group.logical_size;
    }

    if (group.packed_size != 0)
        return DecodeRvzPacking(stored, group.logical_offset, output);

    output = std::move(stored);
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
                remaining, disc_header_.size() - static_cast<std::size_t>(offset));

            std::memcpy(out, disc_header_.data() + offset, amount);
            out += amount;
            offset += amount;
            remaining -= amount;
            continue;
        }

        bool found = false;

        for (const auto& region : raw_regions_) {
            if (!RangeInside(offset, 1, region.logical_offset) &&
                offset < region.logical_offset)
                continue;

            const uint64_t region_end =
                region.logical_offset + region.logical_size;

            if (offset < region.logical_offset || offset >= region_end)
                continue;

            const uint64_t relative = offset - region.logical_offset;
            const uint64_t group_size = rvz_chunk_size_;
            const uint64_t local_group = relative / group_size;

            if (local_group >= region.group_count)
                return false;

            Group group = groups_[region.first_group +
                                  static_cast<uint32_t>(local_group)];
            group.logical_offset =
                region.logical_offset + local_group * group_size;
            group.logical_size = static_cast<uint32_t>(
                std::min<uint64_t>(group_size,
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
        const uint64_t local_group = offset / group_logical_size;
        if (local_group >= region.group_count)
            return false;

        Group group = groups_[region.first_group +
                              static_cast<uint32_t>(local_group)];
        group.logical_offset = local_group * group_logical_size;
        group.logical_size = static_cast<uint32_t>(
            std::min<uint64_t>(group_logical_size,
                               region_size - group.logical_offset));

        std::vector<uint8_t> decoded;
        if (!ReadRvzGroup(group, decoded))
            return false;

        if (group.packed_size == 0) {
            std::size_t exception_lists =
                rvz_chunk_size_ < 0x200000 ? 1 : rvz_chunk_size_ / 0x200000;

            if (!group.compressed) {
                // The exception lists are still present in uncompressed RVZ
                // groups. They are followed by up-to-3 bytes of alignment.
            }

            std::size_t cursor = 0;
            for (std::size_t list = 0; list < exception_lists; ++list) {
                if (decoded.size() - cursor < 2)
                    return false;

                const uint16_t n =
                    static_cast<uint16_t>((decoded[cursor] << 8) |
                                          decoded[cursor + 1]);
                cursor += 2;

                const uint64_t exception_bytes =
                    static_cast<uint64_t>(n) * 22;

                if (exception_bytes > decoded.size() - cursor)
                    return false;

                cursor += static_cast<std::size_t>(exception_bytes);
            }

            if (!group.compressed)
                cursor = (cursor + 3) & ~static_cast<std::size_t>(3);

            if (cursor > decoded.size() ||
                decoded.size() - cursor < group.logical_size)
                return false;

            decoded.erase(decoded.begin(), decoded.begin() +
                                        static_cast<std::ptrdiff_t>(cursor));
        }

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
    if (!info_.valid)
        return false;

    return ReadRawRvz(offset, destination, size);
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

    // boot.bin occupies the start of the decrypted game-partition stream.
    std::array<uint8_t, 0x440> boot{};
    if (!ReadGamePartition(0, boot.data(), boot.size()))
        return false;

    const uint32_t dol_offset = ReadBE32Raw(boot.data() + 0x420);

    std::array<uint8_t, 0x100> dol_header{};
    if (!ReadGamePartition(dol_offset, dol_header.data(), dol_header.size()))
        return false;

    std::vector<uint8_t> header(dol_header.begin(), dol_header.end());

    uint32_t text_offsets[7]{};
    uint32_t text_addresses[7]{};
    uint32_t text_sizes[7]{};
    uint32_t data_offsets[11]{};
    uint32_t data_addresses[11]{};
    uint32_t data_sizes[11]{};

    for (int i = 0; i < 7; ++i) {
        text_offsets[i] = ReadBE32(header, i * 4);
        text_addresses[i] = ReadBE32(header, 0x48 + i * 4);
        text_sizes[i] = ReadBE32(header, 0x90 + i * 4);
    }

    for (int i = 0; i < 11; ++i) {
        data_offsets[i] = ReadBE32(header, 0x1C + i * 4);
        data_addresses[i] = ReadBE32(header, 0x64 + i * 4);
        data_sizes[i] = ReadBE32(header, 0xA4 + i * 4);
    }

    std::size_t total = 0x100;

    for (int i = 0; i < 7; ++i)
        total = std::max(total,
                         static_cast<std::size_t>(text_offsets[i]) +
                         text_sizes[i]);

    for (int i = 0; i < 11; ++i)
        total = std::max(total,
                         static_cast<std::size_t>(data_offsets[i]) +
                         data_sizes[i]);

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

    entry_point = ReadBE32(header, 0xE0 - 8);
    return true;
}

} // namespace vwii::disc
