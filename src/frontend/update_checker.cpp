#include "frontend/update_checker.h"

#include <SDL3/SDL.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>

#ifndef VWII_GIT_SHA
#define VWII_GIT_SHA "unknown"
#endif

namespace vwii::frontend {

namespace {

constexpr const char* NightlyApi =
    "https://api.github.com/repos/NVDEMU/vWii/releases/tags/nightly";

std::string ExtractJsonString(const std::string& json, const std::string& key) {
    const std::string marker = "\"" + key + "\":\"";
    const std::size_t start = json.find(marker);
    if (start == std::string::npos)
        return {};

    std::string result;
    result.reserve(256);

    bool escaped = false;
    for (std::size_t i = start + marker.size(); i < json.size(); ++i) {
        const char ch = json[i];

        if (escaped) {
            switch (ch) {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            default: result.push_back(ch); break;
            }
            escaped = false;
            continue;
        }

        if (ch == '\\') {
            escaped = true;
            continue;
        }

        if (ch == '"')
            break;

        result.push_back(ch);
    }

    return result;
}

std::string ExtractAssetUrl(const std::string& json,
                            const std::string& suffix) {
    std::size_t search = 0;
    std::string latest_url;

    while (search < json.size()) {
        const std::size_t name_pos =
            json.find("\"name\":\"", search);
        if (name_pos == std::string::npos)
            break;

        const std::size_t name_start = name_pos + 8;
        const std::size_t name_end = json.find('"', name_start);
        if (name_end == std::string::npos)
            break;

        const std::string name =
            json.substr(name_start, name_end - name_start);

        const std::size_t object_end = json.find('}', name_end);
        if (object_end == std::string::npos)
            break;

        if (name.size() >= suffix.size() &&
            name.compare(name.size() - suffix.size(),
                         suffix.size(), suffix) == 0) {
            const std::string url_marker =
                "\"browser_download_url\":\"";
            const std::size_t url_pos =
                json.find(url_marker, name_end);
            if (url_pos != std::string::npos &&
                url_pos < object_end) {
                const std::size_t value_start =
                    url_pos + url_marker.size();
                const std::size_t value_end =
                    json.find('"', value_start);
                if (value_end != std::string::npos)
                    latest_url = json.substr(
                        value_start, value_end - value_start);
            }
        }

        search = object_end + 1;
    }

    return latest_url;
}

std::filesystem::path MakeTempPath() {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();

    return std::filesystem::temp_directory_path() /
           ("vwii-nightly-" + std::to_string(stamp) + ".json");
}

} // namespace

UpdateChecker::UpdateChecker() = default;

UpdateChecker::~UpdateChecker() {
    if (worker_.joinable())
        worker_.join();
}

void UpdateChecker::Start() {
    std::scoped_lock lock(mutex_);
    if (started_)
        return;

    started_ = true;
    result_ = {};
    result_.state = State::Checking;

    worker_ = std::thread([this]() {
        Check();
    });
}

void UpdateChecker::Refresh() {
    if (worker_.joinable())
        worker_.join();

    {
        std::scoped_lock lock(mutex_);
        started_ = false;
    }

    Start();
}

UpdateChecker::Result UpdateChecker::GetResult() const {
    std::scoped_lock lock(mutex_);
    return result_;
}

void UpdateChecker::OpenLatest() const {
    const Result result = GetResult();

    if (!result.asset_url.empty()) {
        SDL_OpenURL(result.asset_url.c_str());
        return;
    }

    if (!result.release_url.empty())
        SDL_OpenURL(result.release_url.c_str());
}

UpdateChecker::Result UpdateChecker::CheckNow() {
    Result result;
    result.state = State::Checking;

    const std::filesystem::path temp_path = MakeTempPath();

    std::ostringstream command;
    command
        << "curl -fsSL --connect-timeout 2 --max-time 5 "
        << "-A \"vWii/" << VWII_VERSION_MAJOR << "."
        << VWII_VERSION_MINOR << "." << VWII_VERSION_PATCH << "\" "
        << "-o \"" << temp_path.string() << "\" "
        << "\"" << NightlyApi << "\"";

    if (std::system(command.str().c_str()) != 0) {
        result.state = State::Unavailable;
        result.message =
            "Nightly update check failed. curl is unavailable or GitHub could not be reached.";

        std::error_code cleanup_error;
        std::filesystem::remove(temp_path, cleanup_error);
        return result;
    }

    std::ifstream input(temp_path, std::ios::binary);
    std::ostringstream contents;
    contents << input.rdbuf();

    std::error_code cleanup_error;
    std::filesystem::remove(temp_path, cleanup_error);

    const std::string json = contents.str();
    if (json.empty()) {
        result.state = State::Unavailable;
        result.message = "Nightly release returned no data.";
        return result;
    }

    const std::string body = ExtractJsonString(json, "body");

    std::smatch sha_match;
    static const std::regex sha_regex("commit ([0-9a-fA-F]{40})");
    if (std::regex_search(body, sha_match, sha_regex))
        result.latest_sha = sha_match[1].str();

    result.release_url = ExtractJsonString(json, "html_url");

#if defined(_WIN32)
    result.asset_url =
        ExtractAssetUrl(json, "-Windows-x64.zip");
#elif defined(__APPLE__)
    result.asset_url =
        ExtractAssetUrl(json, "-macOS.dmg");
#endif

    if (result.release_url.empty()) {
        result.state = State::Unavailable;
        result.message = "The nightly release metadata was incomplete.";
        return result;
    }

    const std::string current_sha = VWII_GIT_SHA;
    if (!result.latest_sha.empty() &&
        current_sha != "unknown" &&
        result.latest_sha == current_sha) {
        result.state = State::Current;
        result.message =
            "You are already running the current nightly build.";
    } else {
        result.state = State::Available;
        result.message =
            "A newer vWii nightly build is available.";
    }

    return result;
}

void UpdateChecker::Check() {
    const Result checked = CheckNow();

    std::scoped_lock lock(mutex_);
    result_ = checked;
}

} // namespace vwii::frontend
