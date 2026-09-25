#pragma once

#include <mutex>
#include <string>
#include <thread>

namespace vwii::frontend {

class UpdateChecker {
public:
    enum class State {
        Idle,
        Checking,
        Current,
        Available,
        Unavailable
    };

    struct Result {
        State state{State::Idle};
        std::string latest_sha;
        std::string release_url;
        std::string asset_url;
        std::string message;
    };

    UpdateChecker();
    ~UpdateChecker();

    UpdateChecker(const UpdateChecker&) = delete;
    UpdateChecker& operator=(const UpdateChecker&) = delete;

    void Start();
    void Refresh();
    [[nodiscard]] Result GetResult() const;
    // Downloads the platform-specific nightly asset directly from GitHub Releases.
    // Returns the local path on success, or an empty string on failure.
    [[nodiscard]] std::string DownloadLatest();

private:
    void Check();
    static Result CheckNow();

    mutable std::mutex mutex_;
    std::thread worker_;
    Result result_;
    bool started_{};
};

} // namespace vwii::frontend
