#include "ios/nand_fs.h"

#include "memory/memory.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace vwii::ios {

namespace {

constexpr int ErrorInvalidArgument = -4;
constexpr int ErrorNoSuchFile = -106;
constexpr int ErrorAlreadyExists = -105;
constexpr int ErrorNoSpace = -22;

} // namespace

NandFS::NandFS(memory::Memory& memory)
    : memory_(memory), root_(DefaultRoot()) {
    Reset();
}

std::filesystem::path NandFS::DefaultRoot() {
#if defined(_WIN32)
    if (const char* appdata = std::getenv("APPDATA"); appdata && *appdata)
        return std::filesystem::path(appdata) / "vWii" / "NAND";
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME"); home && *home)
        return std::filesystem::path(home) /
               "Library" / "Application Support" / "vWii" / "NAND";
#else
    if (const char* home = std::getenv("HOME"); home && *home)
        return std::filesystem::path(home) / ".vwii" / "NAND";
#endif

    return std::filesystem::current_path() / "nand";
}

void NandFS::SetRoot(const std::filesystem::path& root) {
    root_ = root.empty() ? DefaultRoot() : root;
    Reset();
}

void NandFS::Reset() {
    for (auto& handle : handles_)
        handle.reset();

    std::error_code error;
    std::filesystem::create_directories(root_, error);
}

std::filesystem::path NandFS::Resolve(const std::string& path) const {
    std::string normalized = path;
    while (!normalized.empty() && (normalized.front() == '/' ||
                                   normalized.front() == '\')) {
        normalized.erase(normalized.begin());
    }

    const std::filesystem::path relative =
        std::filesystem::path(normalized).lexically_normal();

    if (relative.empty() || relative == ".")
        return root_;

    for (const auto& component : relative) {
        if (component == "..")
            return {};
    }

    return root_ / relative;
}

NandFS::Handle* NandFS::GetHandle(uint32_t fd) {
    if (fd < 8 || fd >= 128)
        return nullptr;

    const std::size_t index = static_cast<std::size_t>(fd - 8);
    return handles_[index].get();
}

int NandFS::Open(const std::string& path, uint32_t mode) {
    const auto resolved = Resolve(path);
    if (resolved.empty())
        return ErrorInvalidArgument;

    const bool writable = (mode & 2U) != 0;
    const bool readable = (mode & 1U) != 0 || !writable;

    if (std::filesystem::is_directory(resolved))
        return ErrorInvalidArgument;

    if (writable) {
        std::error_code error;
        std::filesystem::create_directories(resolved.parent_path(), error);
    }

    for (std::size_t i = 0; i < handles_.size(); ++i) {
        if (handles_[i])
            continue;

        auto handle = std::make_unique<Handle>();

        std::ios::openmode openmode = std::ios::binary;
        if (readable)
            openmode |= std::ios::in;
        if (writable)
            openmode |= std::ios::out;

        handle->stream.open(resolved, openmode);

        if (!handle->stream.is_open()) {
            if (!writable)
                return ErrorNoSuchFile;

            handle->stream.clear();
            {
                std::ofstream create(resolved, std::ios::binary);
                if (!create)
                    return ErrorNoSpace;
            }

            handle->stream.open(resolved, openmode);
            if (!handle->stream.is_open())
                return ErrorNoSpace;
        }

        handle->path = resolved;
        handle->readable = readable;
        handle->writable = writable;
        handles_[i] = std::move(handle);
        return static_cast<int>(8 + i);
    }

    return ErrorNoSpace;
}

int NandFS::Close(uint32_t fd) {
    Handle* handle = GetHandle(fd);
    if (!handle)
        return ErrorInvalidArgument;

    handles_[fd - 8].reset();
    return 0;
}

int NandFS::Read(uint32_t fd, uint32_t address, uint32_t size) {
    Handle* handle = GetHandle(fd);
    if (!handle || !handle->readable)
        return ErrorInvalidArgument;

    if (size == 0)
        return 0;

    std::vector<uint8_t> buffer(size);
    handle->stream.read(reinterpret_cast<char*>(buffer.data()),
                        static_cast<std::streamsize>(size));

    const std::streamsize read = handle->stream.gcount();
    if (read > 0)
        memory_.WriteBlock(address,
                           std::span<const uint8_t>(
                               buffer.data(),
                               static_cast<std::size_t>(read)));

    if (read < 0)
        return ErrorNoSuchFile;

    return static_cast<int>(read);
}

int NandFS::Write(uint32_t fd, uint32_t address, uint32_t size) {
    Handle* handle = GetHandle(fd);
    if (!handle || !handle->writable)
        return ErrorInvalidArgument;

    if (size == 0)
        return 0;

    std::vector<uint8_t> buffer(size);
    for (uint32_t i = 0; i < size; ++i)
        buffer[i] = memory_.Read8(address + i);

    handle->stream.write(reinterpret_cast<const char*>(buffer.data()),
                         static_cast<std::streamsize>(buffer.size()));

    if (!handle->stream)
        return ErrorNoSpace;

    handle->stream.flush();
    return static_cast<int>(size);
}

int NandFS::Seek(uint32_t fd, int32_t offset, uint32_t origin) {
    Handle* handle = GetHandle(fd);
    if (!handle)
        return ErrorInvalidArgument;

    std::streamoff base = 0;
    if (origin == 1) {
        const auto position = handle->readable
            ? handle->stream.tellg()
            : handle->stream.tellp();
        base = position >= 0 ? position : 0;
    } else if (origin == 2) {
        std::error_code error;
        const auto size = std::filesystem::file_size(handle->path, error);
        if (error)
            return ErrorNoSuchFile;
        base = static_cast<std::streamoff>(size);
    } else if (origin != 0) {
        return ErrorInvalidArgument;
    }

    const std::streamoff target = base + offset;
    if (target < 0)
        return ErrorInvalidArgument;

    handle->stream.clear();
    if (handle->readable)
        handle->stream.seekg(target, std::ios::beg);
    if (handle->writable)
        handle->stream.seekp(target, std::ios::beg);

    if (!handle->stream)
        return ErrorInvalidArgument;

    return static_cast<int>(target);
}

int NandFS::CreateDirectory(const std::string& path) {
    const auto resolved = Resolve(path);
    if (resolved.empty())
        return ErrorInvalidArgument;

    std::error_code error;
    if (std::filesystem::exists(resolved, error))
        return ErrorAlreadyExists;

    return std::filesystem::create_directories(resolved, error) ? 0
                                                                  : ErrorInvalidArgument;
}

int NandFS::CreateFile(const std::string& path) {
    const auto resolved = Resolve(path);
    if (resolved.empty())
        return ErrorInvalidArgument;

    std::error_code error;
    if (std::filesystem::exists(resolved, error))
        return ErrorAlreadyExists;

    std::filesystem::create_directories(resolved.parent_path(), error);
    std::ofstream output(resolved, std::ios::binary);
    return output ? 0 : ErrorNoSpace;
}

int NandFS::Remove(const std::string& path) {
    const auto resolved = Resolve(path);
    if (resolved.empty())
        return ErrorInvalidArgument;

    std::error_code error;
    if (!std::filesystem::exists(resolved, error))
        return ErrorNoSuchFile;

    return std::filesystem::remove_all(resolved, error) != 0 && !error
        ? 0
        : ErrorInvalidArgument;
}

int NandFS::Rename(const std::string& from, const std::string& to) {
    const auto source = Resolve(from);
    const auto destination = Resolve(to);
    if (source.empty() || destination.empty())
        return ErrorInvalidArgument;

    std::error_code error;
    std::filesystem::create_directories(destination.parent_path(), error);
    if (error)
        return ErrorNoSpace;

    std::filesystem::rename(source, destination, error);
    return error ? ErrorInvalidArgument : 0;
}

int NandFS::ReadDirectory(const std::string& path, uint32_t out_address,
                          uint32_t out_size) {
    const auto resolved = Resolve(path);
    if (resolved.empty())
        return ErrorInvalidArgument;

    if (!std::filesystem::is_directory(resolved))
        return ErrorInvalidArgument;

    std::vector<std::string> names;
    std::error_code error;

    for (const auto& entry : std::filesystem::directory_iterator(resolved, error)) {
        if (error)
            return ErrorInvalidArgument;
        names.push_back(entry.path().filename().string());
    }

    std::sort(names.begin(), names.end());

    // Each entry is returned as a pointer-style string structure in a Wii
    // libc-compatible buffer. For the initial HLE, expose NUL-terminated names
    // separated by 4-byte alignment.
    uint32_t cursor = out_address;

    for (const std::string& name : names) {
        if (cursor + name.size() + 1 > out_address + out_size)
            return ErrorNoSpace;

        for (char c : name)
            memory_.Write8(cursor++, static_cast<uint8_t>(c));

        memory_.Write8(cursor++, 0);
        cursor = (cursor + 3) & ~3u;
    }

    return static_cast<int>(names.size());
}

} // namespace vwii::ios
