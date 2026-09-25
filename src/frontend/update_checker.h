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
    [[nodiscard]] Result GetResult() const;
    void OpenLatest() const;

private:
    void Check();
    static Result CheckNow();

    mutable std::mutex mutex_;
    std::thread worker_;
    Result result_;
    bool started_{};
};

} // namespace vwii::frontend
