#pragma once

#include <array>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace vwii::disc {

struct DiscInfo {
    bool valid{};
    bool wii{};
    std::string format;
    std::string game_id;
    uint64_t disc_size{};
    uint32_t partition_count{};
    std::string error;
};

class DiscImage {
public:
    DiscImage();
    ~DiscImage();

    DiscImage(const DiscImage&) = delete;
    DiscImage& operator=(const DiscImage&) = delete;

    bool Open(const std::string& path);
    void Close();

    [[nodiscard]] const DiscInfo& Info() const { return info_; }
    [[nodiscard]] bool Read(uint64_t offset, void* destination, std::size_t size);

    // Reads the decrypted/hash-stripped data stream of the selected Wii game
    // partition. This is the representation exposed by RVZ.
    [[nodiscard]] bool ReadGamePartition(uint64_t offset, void* destination,
                                         std::size_t size);

    [[nodiscard]] bool LoadGameDol(std::vector<uint8_t>& dol,
                                   uint32_t& entry_point);

private:
    struct Group {
        uint64_t file_offset{};
        uint32_t stored_size{};
        uint32_t packed_size{};
        uint64_t logical_offset{};
        uint32_t logical_size{};
        bool compressed{};
    };

    struct RawRegion {
        uint64_t logical_offset{};
        uint64_t logical_size{};
        uint32_t first_group{};
        uint32_t group_count{};
    };

    struct PartitionRegion {
        uint32_t first_sector{};
        uint32_t sector_count{};
        uint32_t first_group{};
        uint32_t group_count{};
    };

    struct Partition {
        std::array<uint8_t, 16> title_key{};
        PartitionRegion regions[2]{};
    };

    bool OpenRvz();
    bool LoadRvzTables();

    bool ReadRvzGroup(const Group& group, std::vector<uint8_t>& output);
    bool DecodeRvzPacking(const std::vector<uint8_t>& packed,
                          uint64_t logical_offset,
                          std::vector<uint8_t>& output);

    bool ReadRawRvz(uint64_t offset, void* destination, std::size_t size);
    bool ReadPartitionRvz(const PartitionRegion& region, uint64_t offset,
                          void* destination, std::size_t size);

    bool ReadFile(uint64_t offset, void* destination, std::size_t size);

    [[nodiscard]] uint32_t ReadBE32(const std::vector<uint8_t>& data,
                                     std::size_t offset) const;
    [[nodiscard]] uint64_t ReadBE64(const std::vector<uint8_t>& data,
                                     std::size_t offset) const;

    std::ifstream file_;
    uint64_t file_size_{};

    DiscInfo info_{};

    uint32_t rvz_compression_{};
    uint32_t rvz_chunk_size_{};
    uint32_t rvz_group_count_{};

    std::vector<Group> groups_;
    std::vector<RawRegion> raw_regions_;
    std::vector<Partition> partitions_;
    std::vector<uint8_t> disc_header_;

    std::size_t game_partition_index_{static_cast<std::size_t>(-1)};
    uint64_t game_partition_disc_offset_{};
    uint64_t game_partition_data_size_{};
    uint32_t game_partition_first_sector_{};
};

} // namespace vwii::disc
