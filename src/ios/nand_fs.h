#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace vwii::memory {
class Memory;
}

namespace vwii::ios {

class NandFS {
public:
    explicit NandFS(memory::Memory& memory);

    void Reset();
    void SetRoot(const std::filesystem::path& root);

    [[nodiscard]] const std::filesystem::path& Root() const { return root_; }

    int Open(const std::string& path, uint32_t mode);
    int Close(uint32_t fd);

    int Read(uint32_t fd, uint32_t address, uint32_t size);
    int Write(uint32_t fd, uint32_t address, uint32_t size);
    int Seek(uint32_t fd, int32_t offset, uint32_t origin);

    int CreateDirectory(const std::string& path);
    int CreateFile(const std::string& path);
    int Remove(const std::string& path);
    int Rename(const std::string& from, const std::string& to);

    int ReadDirectory(const std::string& path, uint32_t out_address,
                      uint32_t out_size);

private:
    struct Handle {
        std::filesystem::path path;
        std::fstream stream;
        bool readable{};
        bool writable{};
    };

    [[nodiscard]] std::filesystem::path Resolve(const std::string& path) const;
    [[nodiscard]] Handle* GetHandle(uint32_t fd);

    static std::filesystem::path DefaultRoot();

    memory::Memory& memory_;
    std::filesystem::path root_;
    std::array<std::unique_ptr<Handle>, 120> handles_{};
};

} // namespace vwii::ios
