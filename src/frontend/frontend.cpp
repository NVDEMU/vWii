#include "frontend/frontend.h"

#include "frontend/game_library.h"
#include "frontend/keyboard_bindings.h"
#include "frontend/update_checker.h"
#include "input/wiimote_keyboard.h"
#include "memory/memory.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace vwii::frontend {

namespace {

struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

constexpr Color Background{10, 14, 22, 255};
constexpr Color Sidebar{16, 22, 34, 255};
constexpr Color Panel{23, 31, 46, 255};
constexpr Color PanelAlt{29, 39, 57, 255};
constexpr Color Border{51, 67, 91, 255};
constexpr Color TextColor{236, 241, 248, 255};
constexpr Color Muted{151, 164, 184, 255};
constexpr Color Accent{63, 177, 255, 255};
constexpr Color AccentSoft{34, 92, 135, 255};
constexpr Color Success{62, 205, 130, 255};
constexpr Color Warning{241, 185, 79, 255};
constexpr Color Danger{232, 92, 92, 255};
constexpr int LogicalWidth = 1280;
constexpr int LogicalHeight = 720;

void SetColor(SDL_Renderer* renderer, Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void FillRect(SDL_Renderer* renderer, float x, float y,
              float w, float h, Color color) {
    SetColor(renderer, color);
    const SDL_FRect rect{x, y, w, h};
    SDL_RenderFillRect(renderer, &rect);
}

void StrokeRect(SDL_Renderer* renderer, float x, float y,
                float w, float h, Color color) {
    SetColor(renderer, color);
    const SDL_FRect rect{x, y, w, h};
    SDL_RenderRect(renderer, &rect);
}

void Text(SDL_Renderer* renderer, float x, float y,
          const std::string& value, Color color = TextColor) {
    SetColor(renderer, color);
    SDL_RenderDebugText(renderer, x, y, value.c_str());
}

std::string Shorten(const std::string& value, std::size_t max_length) {
    if (value.size() <= max_length)
        return value;

    if (max_length <= 3)
        return value.substr(0, max_length);

    return value.substr(0, max_length - 3) + "...";
}

std::string FormatBytes(uintmax_t bytes) {
    constexpr uintmax_t MiB = 1024 * 1024;
    constexpr uintmax_t GiB = 1024 * MiB;

    std::ostringstream output;
    output.setf(std::ios::fixed);
    output.precision(1);

    if (bytes >= GiB)
        output << static_cast<double>(bytes) / static_cast<double>(GiB) << " GB";
    else if (bytes >= MiB)
        output << static_cast<double>(bytes) / static_cast<double>(MiB) << " MB";
    else
        output << static_cast<double>(bytes) / 1024.0 << " KB";

    return output.str();
}

bool Contains(float x, float y, const SDL_FRect& rect) {
    return x >= rect.x && x < rect.x + rect.w &&
           y >= rect.y && y < rect.y + rect.h;
}

std::filesystem::path PrefDirectory() {
    char* pref = SDL_GetPrefPath("NVDEMU", "vWii");
    if (!pref)
        return {};

    std::filesystem::path path(pref);
    SDL_free(pref);
    return path;
}

bool ToggleValue(bool& value) {
    value = !value;
    return value;
}

} // namespace

struct DialogState {
    std::mutex mutex;
    std::vector<std::string> selections;
};

void SDLCALL DialogCallback(void* userdata,
                            const char* const* filelist,
                            int /*filter*/) {
    auto* state = static_cast<DialogState*>(userdata);
    if (!state || !filelist)
        return;

    std::scoped_lock lock(state->mutex);

    for (const char* const* current = filelist; *current; ++current)
        state->selections.emplace_back(*current);
}

enum class Page {
    Library,
    Settings,
    About
};

enum class SettingsSection {
    General,
    Graphics,
    Emulation,
    Input,
    Library,
    Updates
};

struct Frontend::Impl {
    SDL_Window* window{};
    SDL_Renderer* renderer{};
    SDL_Texture* xfb_texture{};

    int xfb_width{};
    int xfb_height{};

    std::vector<uint8_t> xfb_raw;
    std::vector<uint8_t> rgba;

    std::string dropped_file;
    std::string launch_file;
    std::string status_message{"Ready"};

    GameLibrary library;
    KeyboardBindings keyboard_bindings;
    UpdateChecker updater;
    DialogState dialogs;

    Page page{Page::Library};
    SettingsSection settings_section{SettingsSection::General};

    std::size_t selected_game{};
    std::size_t selected_folder{};
    std::size_t selected_binding{};

    bool remap_waiting{};
    bool fullscreen{};
    bool vsync{true};
    bool integer_scale{};
    bool show_stats{true};
    bool auto_update{true};

    int aspect_mode{}; // 0 Auto, 1 4:3, 2 16:9, 3 Stretch
    int emulation_speed_percent{100};

    bool config_loaded{};
};

Frontend::Frontend()
    : impl_(std::make_unique<Impl>()) {
}

Frontend::~Frontend() {
    Shutdown();
}

bool Frontend::Initialize(const char* title, int width, int height) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        return false;

    LoadConfig();

    if (auto_update_)
        impl_->updater.Start();

    impl_->window = SDL_CreateWindow(
        title,
        width,
        height,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

    if (!impl_->window) {
        SDL_Quit();
        return false;
    }

    impl_->renderer = SDL_CreateRenderer(impl_->window, nullptr);
    if (!impl_->renderer) {
        SDL_DestroyWindow(impl_->window);
        impl_->window = nullptr;
        SDL_Quit();
        return false;
    }

    ApplyPresentationSettings();

    if (impl_->fullscreen)
        SDL_SetWindowFullscreen(impl_->window, true);

    impl_->keyboard_bindings.Load();
    impl_->library.Load();
    return true;
}

void Frontend::Shutdown() {
    if (!impl_)
        return;

    SaveConfig();

    if (impl_->xfb_texture) {
        SDL_DestroyTexture(impl_->xfb_texture);
        impl_->xfb_texture = nullptr;
    }

    if (impl_->renderer) {
        SDL_DestroyRenderer(impl_->renderer);
        impl_->renderer = nullptr;
    }

    if (impl_->window) {
        SDL_DestroyWindow(impl_->window);
        impl_->window = nullptr;
    }

    SDL_Quit();
}

void Frontend::LoadConfig() {
    const auto directory = PrefDirectory();
    if (directory.empty())
        return;

    std::ifstream input(directory / "frontend.cfg");
    std::string key;
    std::string value;

    while (input >> key >> value) {
        if (key == "fullscreen")
            impl_->fullscreen = value == "1";
        else if (key == "vsync")
            impl_->vsync = value != "0";
        else if (key == "integer_scale")
            impl_->integer_scale = value == "1";
        else if (key == "show_stats")
            impl_->show_stats = value != "0";
        else if (key == "auto_update")
            impl_->auto_update = value != "0";
        else if (key == "aspect")
            impl_->aspect_mode = std::clamp(std::stoi(value), 0, 3);
        else if (key == "speed")
            impl_->emulation_speed_percent =
                std::clamp(std::stoi(value), 25, 300);
    }

    impl_->config_loaded = true;
}

void Frontend::SaveConfig() const {
    const auto directory = PrefDirectory();
    if (directory.empty())
        return;

    std::error_code error;
    std::filesystem::create_directories(directory, error);

    std::ofstream output(directory / "frontend.cfg", std::ios::trunc);
    if (!output)
        return;

    output << "fullscreen " << (impl_->fullscreen ? 1 : 0) << '\n';
    output << "vsync " << (impl_->vsync ? 1 : 0) << '\n';
    output << "integer_scale " << (impl_->integer_scale ? 1 : 0) << '\n';
    output << "show_stats " << (impl_->show_stats ? 1 : 0) << '\n';
    output << "auto_update " << (impl_->auto_update ? 1 : 0) << '\n';
    output << "aspect " << impl_->aspect_mode << '\n';
    output << "speed " << impl_->emulation_speed_percent << '\n';
}

void Frontend::ApplyPresentationSettings() {
    if (!impl_->renderer)
        return;

    SDL_SetRenderLogicalPresentation(
        impl_->renderer,
        LogicalWidth,
        LogicalHeight,
        impl_->integer_scale
            ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
            : SDL_LOGICAL_PRESENTATION_LETTERBOX);

    SDL_SetRenderVSync(
        impl_->renderer,
        impl_->vsync ? 1 : SDL_RENDERER_VSYNC_DISABLED);
}

void Frontend::OpenFolderDialog() {
    if (!impl_->window)
        return;

    SDL_ShowOpenFolderDialog(
        DialogCallback,
        &impl_->dialogs,
        impl_->window,
        nullptr,
        false);
}

void Frontend::OpenGameDialog() {
    if (!impl_->window)
        return;

    static constexpr SDL_DialogFileFilter filters[] = {
        {"Wii games", "rvz;iso;wia;wbfs;dol;elf"},
        {"All files", "*"}
    };

    SDL_ShowOpenFileDialog(
        DialogCallback,
        &impl_->dialogs,
        impl_->window,
        filters,
        2,
        nullptr,
        false);
}

void Frontend::ProcessDialogResults() {
    std::vector<std::string> results;
    {
        std::scoped_lock lock(impl_->dialogs.mutex);
        results.swap(impl_->dialogs.selections);
    }

    for (const std::string& selection : results) {
        const std::filesystem::path path(selection);

        if (std::filesystem::is_directory(path)) {
            if (impl_->library.AddFolder(path))
                impl_->status_message = "Added game folder: " + path.string();
        } else if (std::filesystem::is_regular_file(path)) {
            impl_->launch_file = path.string();
            impl_->library.MarkRecent(path);
            impl_->status_message = "Opening " + path.filename().string();
        }
    }
}

void Frontend::HandleLibraryClick(float x, float y, int clicks) {
    const int sidebar_width = 220;

    if (x < sidebar_width)
        return;

    const SDL_FRect add_folder{968.0f, 24.0f, 120.0f, 34.0f};
    const SDL_FRect open_game{1096.0f, 24.0f, 116.0f, 34.0f};
    const SDL_FRect refresh{850.0f, 24.0f, 106.0f, 34.0f};

    if (Contains(x, y, add_folder)) {
        OpenFolderDialog();
        return;
    }

    if (Contains(x, y, open_game)) {
        OpenGameDialog();
        return;
    }

    if (Contains(x, y, refresh)) {
        impl_->library.Rescan();
        impl_->status_message = "Library refreshed.";
        return;
    }

    if (impl_->library.Games().empty()) {
        if (Contains(x, y, SDL_FRect{292.0f, 270.0f, 220.0f, 54.0f})) {
            OpenFolderDialog();
            return;
        }

        if (Contains(x, y, SDL_FRect{530.0f, 270.0f, 220.0f, 54.0f})) {
            OpenGameDialog();
            return;
        }
    }

    constexpr float card_width = 294.0f;
    constexpr float card_height = 124.0f;
    constexpr float gap = 18.0f;
    constexpr float start_x = 252.0f;
    constexpr float start_y = 132.0f;

    const auto& games = impl_->library.Games();

    for (std::size_t index = 0; index < games.size(); ++index) {
        const std::size_t column = index % 3;
        const std::size_t row = index / 3;

        const SDL_FRect card{
            start_x + static_cast<float>(column) * (card_width + gap),
            start_y + static_cast<float>(row) * (card_height + gap),
            card_width,
            card_height
        };

        if (!Contains(x, y, card))
            continue;

        impl_->selected_game = index;
        if (clicks >= 2) {
            impl_->launch_file = games[index].path.string();
            impl_->library.MarkRecent(games[index].path);
            impl_->status_message =
                "Launching " + games[index].title + "...";
        }
        return;
    }
}

void Frontend::HandleSettingsClick(float x, float y) {
    const float sidebar_x = 242.0f;

    if (x >= sidebar_x && x < sidebar_x + 170.0f &&
        y >= 116.0f && y < 116.0f + 6.0f * 48.0f) {
        const std::size_t index =
            static_cast<std::size_t>((y - 116.0f) / 48.0f);
        impl_->settings_section =
            static_cast<SettingsSection>(
                std::min<std::size_t>(index, 5));
        impl_->remap_waiting = false;
        return;
    }

    const float content_x = 442.0f;

    if (impl_->settings_section == SettingsSection::Input &&
        x >= content_x && x < content_x + 620.0f &&
        y >= 158.0f && y < 158.0f + 17.0f * 28.0f) {
        const std::size_t row =
            static_cast<std::size_t>((y - 158.0f) / 28.0f);
        const std::size_t first =
            (impl_->selected_binding / 17) * 17;
        const auto& entries = impl_->keyboard_bindings.Entries();
        if (first + row < entries.size())
            impl_->selected_binding = first + row;
        return;
    }

    if (impl_->settings_section == SettingsSection::Library) {
        const auto& folders = impl_->library.Folders();
        if (y >= 272.0f &&
            y < 272.0f +
                static_cast<float>(folders.size()) * 38.0f &&
            x >= content_x &&
            x < content_x + 620.0f) {
            impl_->selected_folder =
                static_cast<std::size_t>((y - 272.0f) / 38.0f);
            return;
        }
    }

    switch (impl_->settings_section) {
    case SettingsSection::General:
        if (Contains(x, y, SDL_FRect{content_x, 150, 620, 42})) {
            ToggleValue(impl_->fullscreen);
            SDL_SetWindowFullscreen(impl_->window, impl_->fullscreen);
            SaveConfig();
        } else if (Contains(x, y, SDL_FRect{content_x, 202, 620, 42})) {
            ToggleValue(impl_->vsync);
            ApplyPresentationSettings();
            SaveConfig();
        }
        else if (Contains(x, y, SDL_FRect{content_x, 254, 620, 42})) {
            ToggleValue(impl_->auto_update);
            if (impl_->auto_update)
                impl_->updater.Refresh();
            SaveConfig();
        } else if (Contains(x, y, SDL_FRect{content_x, 306, 620, 42})) {
            ToggleValue(impl_->show_stats);
            SaveConfig();
        }

        if (Contains(x, y, SDL_FRect{content_x, 358, 620, 42})) {
            impl_->page = Page::Library;
        }
        break;

    case SettingsSection::Graphics:
        if (Contains(x, y, SDL_FRect{content_x, 150, 620, 42}))
            impl_->aspect_mode = (impl_->aspect_mode + 1) % 4;
        else if (Contains(x, y, SDL_FRect{content_x, 202, 620, 42})) {
            ToggleValue(impl_->integer_scale);
            ApplyPresentationSettings();
            SaveConfig();
        }
        break;

    case SettingsSection::Emulation:
        if (Contains(x, y, SDL_FRect{content_x, 150, 620, 42})) {
            static constexpr int speeds[] = {25, 50, 75, 100, 125, 150, 200, 300};
            const auto iterator = std::find(
                std::begin(speeds), std::end(speeds),
                impl_->emulation_speed_percent);
            const std::size_t current =
                iterator == std::end(speeds)
                    ? 3
                    : static_cast<std::size_t>(
                          std::distance(std::begin(speeds), iterator));

            impl_->emulation_speed_percent =
                speeds[(current + 1) %
                       (sizeof(speeds) / sizeof(speeds[0]))];
        }
        break;

    case SettingsSection::Input:
        break;

    case SettingsSection::Library:
        if (Contains(x, y, SDL_FRect{442, 150, 620, 42})) {
            OpenFolderDialog();
        } else if (Contains(x, y, SDL_FRect{442, 202, 620, 42})) {
            impl_->library.Rescan();
        } else if (Contains(x, y, SDL_FRect{442, 260, 620, 42})) {
            impl_->library.RemoveFolder(impl_->selected_folder);
            if (!impl_->library.Folders().empty())
                impl_->selected_folder =
                    std::min(
                        impl_->selected_folder,
                        impl_->library.Folders().size() - 1);
            else
                impl_->selected_folder = 0;
        }
        break;

    case SettingsSection::Updates:
        if (Contains(x, y, SDL_FRect{442, 150, 620, 42})) {
            impl_->auto_update = true;
            impl_->updater.Refresh();
        } else if (Contains(x, y, SDL_FRect{442, 202, 620, 42})) {
            impl_->updater.OpenLatest();
        }
        break;
    }
}

bool Frontend::PumpEvents(input::WiiRemoteKeyboard* wiimote,
                          float delta_seconds) {
    ProcessDialogResults();

    SDL_Event event{};

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT)
            return false;

        if (event.type == SDL_EVENT_DROP_FILE && event.drop.data) {
            const std::filesystem::path dropped(event.drop.data);
            if (std::filesystem::is_directory(dropped))
                impl_->library.AddFolder(dropped);
            else
                impl_->launch_file = dropped.string();
            continue;
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            event.button.button == SDL_BUTTON_LEFT) {
            float x = event.button.x;
            float y = event.button.y;

            SDL_ConvertEventToRenderCoordinates(
                impl_->renderer, &event);

            x = event.button.x;
            y = event.button.y;

            if (impl_->page == Page::Library)
                HandleLibraryClick(x, y, event.button.clicks);
            else if (impl_->page == Page::Settings)
                HandleSettingsClick(x, y);

            if (x < 220.0f) {
                if (y >= 164.0f && y < 224.0f)
                    impl_->page = Page::Library;
                else if (y >= 230.0f && y < 290.0f) {
                    impl_->page = Page::Settings;
                    impl_->remap_waiting = false;
                } else if (y >= 296.0f && y < 356.0f)
                    impl_->page = Page::About;
            }

            continue;
        }

        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
            const SDL_Scancode scancode = event.key.scancode;

            if (scancode == SDL_SCANCODE_F12) {
                if (impl_->page == Page::Settings) {
                    impl_->page = Page::Library;
                    impl_->remap_waiting = false;
                } else {
                    impl_->page = Page::Settings;
                    impl_->settings_section = SettingsSection::General;
                    impl_->remap_waiting = false;
                    if (wiimote)
                        impl_->keyboard_bindings.ReleaseAll(wiimote);
                }
                continue;
            }

            if (scancode == SDL_SCANCODE_L &&
                (event.key.mod & SDL_KMOD_CTRL)) {
                OpenFolderDialog();
                continue;
            }

            if (scancode == SDL_SCANCODE_O &&
                (event.key.mod & SDL_KMOD_CTRL)) {
                OpenGameDialog();
                continue;
            }

            if (scancode == SDL_SCANCODE_RETURN &&
                impl_->page == Page::Library &&
                !impl_->library.Games().empty()) {
                const std::size_t index =
                    std::min(impl_->selected_game,
                             impl_->library.Games().size() - 1);
                impl_->launch_file =
                    impl_->library.Games()[index].path.string();
                impl_->library.MarkRecent(impl_->library.Games()[index].path);
                continue;
            }

            if (event.key.mod & SDL_KMOD_ALT &&
                scancode == SDL_SCANCODE_RETURN) {
                impl_->fullscreen = !impl_->fullscreen;
                SDL_SetWindowFullscreen(
                    impl_->window, impl_->fullscreen);
                SaveConfig();
                continue;
            }

            if (impl_->page == Page::Settings) {
                if (impl_->settings_section == SettingsSection::Input) {
                    if (impl_->remap_waiting) {
                        if (scancode == SDL_SCANCODE_ESCAPE) {
                            impl_->remap_waiting = false;
                            continue;
                        }

                        if (scancode != SDL_SCANCODE_F12 &&
                            scancode != SDL_SCANCODE_UNKNOWN) {
                            impl_->keyboard_bindings.Set(
                                impl_->selected_binding, scancode);
                            impl_->keyboard_bindings.Save();
                            impl_->remap_waiting = false;
                        }
                        continue;
                    }

                    const auto& entries =
                        impl_->keyboard_bindings.Entries();

                    if (!entries.empty()) {
                        if (scancode == SDL_SCANCODE_UP) {
                            if (impl_->selected_binding > 0)
                                --impl_->selected_binding;
                        } else if (scancode == SDL_SCANCODE_DOWN) {
                            if (impl_->selected_binding + 1 <
                                entries.size())
                                ++impl_->selected_binding;
                        } else if (scancode == SDL_SCANCODE_PAGEUP) {
                            impl_->selected_binding =
                                impl_->selected_binding > 10
                                    ? impl_->selected_binding - 10
                                    : 0;
                        } else if (scancode == SDL_SCANCODE_PAGEDOWN) {
                            impl_->selected_binding =
                                std::min(
                                    impl_->selected_binding + 10,
                                    entries.size() - 1);
                        } else if (scancode == SDL_SCANCODE_HOME) {
                            impl_->selected_binding = 0;
                        } else if (scancode == SDL_SCANCODE_END) {
                            impl_->selected_binding = entries.size() - 1;
                        } else if (scancode == SDL_SCANCODE_RETURN ||
                                   scancode == SDL_SCANCODE_KP_ENTER) {
                            impl_->remap_waiting = true;
                        } else if (scancode == SDL_SCANCODE_DELETE) {
                            impl_->keyboard_bindings.Set(
                                impl_->selected_binding,
                                SDL_SCANCODE_UNKNOWN);
                            impl_->keyboard_bindings.Save();
                        } else if (scancode == SDL_SCANCODE_R) {
                            impl_->keyboard_bindings.Reset(
                                impl_->selected_binding);
                            impl_->keyboard_bindings.Save();
                        } else if (scancode == SDL_SCANCODE_F5) {
                            impl_->keyboard_bindings.ResetAll();
                            impl_->keyboard_bindings.Save();
                        }
                    }

                    if (scancode == SDL_SCANCODE_ESCAPE)
                        impl_->page = Page::Library;

                    continue;
                }

                if (scancode == SDL_SCANCODE_ESCAPE) {
                    impl_->page = Page::Library;
                    continue;
                }

                if (scancode == SDL_SCANCODE_F6 &&
                    impl_->settings_section == SettingsSection::Updates) {
                    impl_->updater.OpenLatest();
                    continue;
                }

                continue;
            }

            if (scancode == SDL_SCANCODE_F5) {
                impl_->library.Rescan();
                impl_->status_message = "Library refreshed.";
                continue;
            }

            if (scancode == SDL_SCANCODE_ESCAPE)
                return false;

            if (wiimote && impl_->keyboard_bindings.Has(scancode))
                impl_->keyboard_bindings.Send(
                    wiimote, scancode, true);
        }

        if (wiimote &&
            event.type == SDL_EVENT_KEY_UP &&
            impl_->page != Page::Settings &&
            impl_->keyboard_bindings.Has(event.key.scancode)) {
            impl_->keyboard_bindings.Send(
                wiimote, event.key.scancode, false);
        }
    }

    if (wiimote && impl_->page != Page::Settings)
        wiimote->Update(delta_seconds);

    return true;
}

bool Frontend::SettingsOpen() const {
    return impl_->page == Page::Settings;
}

std::string Frontend::ConsumeDroppedFile() {
    std::string result = std::move(impl_->dropped_file);
    impl_->dropped_file.clear();
    return result;
}

std::string Frontend::ConsumeLaunchFile() {
    std::string result = std::move(impl_->launch_file);
    impl_->launch_file.clear();
    return result;
}

uint64_t Frontend::InstructionBudget() const {
    constexpr uint64_t BaseBudget = 250000;
    return std::max<uint64_t>(
        1000,
        (BaseBudget * static_cast<uint64_t>(
            impl_->emulation_speed_percent)) / 100);
}

void Frontend::RenderSidebar() {
    FillRect(impl_->renderer, 0, 0, 220, LogicalHeight, Sidebar);

    Text(impl_->renderer, 28, 28, "vWii", TextColor);
    Text(impl_->renderer, 28, 48,
         "WII EMULATOR", Muted);

    struct Item {
        const char* label;
        float y;
        Page page;
    };

    const Item items[] = {
        {"LIBRARY", 164, Page::Library},
        {"SETTINGS", 230, Page::Settings},
        {"ABOUT", 296, Page::About}
    };

    for (const Item& item : items) {
        const bool active = impl_->page == item.page;
        FillRect(
            impl_->renderer,
            16,
            item.y,
            188,
            52,
            active ? AccentSoft : Sidebar);

        Text(
            impl_->renderer,
            32,
            item.y + 22,
            item.label,
            active ? TextColor : Muted);
    }

    Text(impl_->renderer, 28, 610,
         "CTRL+O  OPEN GAME", Muted);
    Text(impl_->renderer, 28, 628,
         "CTRL+L  ADD FOLDER", Muted);
    Text(impl_->renderer, 28, 646,
         "F12     SETTINGS", Muted);
    Text(impl_->renderer, 28, 678,
         Shorten(impl_->status_message, 28), Success);
}

void Frontend::RenderLibrary(const Status& status) {
    RenderSidebar();

    FillRect(impl_->renderer, 220, 0,
             LogicalWidth - 220, LogicalHeight, Background);

    Text(impl_->renderer, 252, 34, "GAME LIBRARY", TextColor);
    Text(
        impl_->renderer,
        252, 54,
        std::to_string(impl_->library.Games().size()) +
            " game" +
            (impl_->library.Games().size() == 1 ? "" : "s") +
            " indexed",
        Muted);

    FillRect(impl_->renderer, 850, 24, 106, 34, Panel);
    FillRect(impl_->renderer, 968, 24, 120, 34, AccentSoft);
    FillRect(impl_->renderer, 1096, 24, 116, 34, Success);

    Text(impl_->renderer, 877, 37, "REFRESH", TextColor);
    Text(impl_->renderer, 985, 37, "ADD FOLDER", TextColor);
    Text(impl_->renderer, 1114, 37, "OPEN GAME", TextColor);

    const auto& games = impl_->library.Games();

    if (games.empty()) {
        FillRect(impl_->renderer, 252, 140, 932, 340, Panel);

        Text(impl_->renderer, 292, 198,
             "YOUR LIBRARY IS EMPTY", TextColor);
        Text(impl_->renderer, 292, 224,
             "Add a folder containing Wii games, or open a game directly.",
             Muted);

        FillRect(impl_->renderer, 292, 270, 220, 54, AccentSoft);
        FillRect(impl_->renderer, 530, 270, 220, 54, PanelAlt);

        Text(impl_->renderer, 314, 292,
             "ADD GAME FOLDER", TextColor);
        Text(impl_->renderer, 554, 292,
             "OPEN WII GAME", TextColor);

        Text(impl_->renderer, 292, 370,
             "Supported files: RVZ, ISO, WIA, WBFS, DOL, ELF", Muted);
        Text(impl_->renderer, 292, 396,
             "Your default vWii library folder is created automatically.",
             Muted);
    } else {
        constexpr float card_width = 294;
        constexpr float card_height = 124;
        constexpr float gap = 18;
        constexpr float start_x = 252;
        constexpr float start_y = 132;

        for (std::size_t index = 0; index < games.size(); ++index) {
            const std::size_t column = index % 3;
            const std::size_t row = index / 3;

            const float x =
                start_x + static_cast<float>(column) *
                    (card_width + gap);
            const float y =
                start_y + static_cast<float>(row) *
                    (card_height + gap);

            const bool selected = index == impl_->selected_game;

            FillRect(
                impl_->renderer,
                x, y, card_width, card_height,
                selected ? AccentSoft : Panel);
            StrokeRect(
                impl_->renderer,
                x, y, card_width, card_height,
                selected ? Accent : Border);

            Text(
                impl_->renderer,
                x + 18, y + 18,
                Shorten(games[index].title, 31),
                Text);

            Text(
                impl_->renderer,
                x + 18, y + 44,
                games[index].extension + "  " +
                    FormatBytes(games[index].size),
                Muted);

            Text(
                impl_->renderer,
                x + 18, y + 68,
                Shorten(games[index].path.parent_path().string(), 35),
                Muted);

            Text(
                impl_->renderer,
                x + 18, y + 98,
                selected ? "ENTER / DOUBLE CLICK TO PLAY" : "CLICK TO SELECT",
                selected ? Success : Muted);
        }
    }

    const float recent_y = 586;
    FillRect(impl_->renderer, 252, recent_y,
             932, 88, Panel);

    Text(impl_->renderer, 272, recent_y + 18,
         "RECENT GAMES", TextColor);

    if (impl_->library.Recent().empty()) {
        Text(impl_->renderer, 272, recent_y + 44,
             "No games launched yet.", Muted);
    } else {
        std::size_t x = 272;
        const auto& recent = impl_->library.Recent();
        for (std::size_t index = 0;
             index < std::min<std::size_t>(recent.size(), 4);
             ++index) {
            Text(
                impl_->renderer,
                static_cast<float>(x),
                recent_y + 44,
                Shorten(recent[index].stem().string(), 24),
                index == 0 ? Success : Muted);
            x += 220;
        }
    }

    if (status.loaded) {
        FillRect(impl_->renderer, 780, 102, 404, 22,
                 Success);
        Text(
            impl_->renderer,
            792, 109,
            "GAME LOADED - PRESS F12 FOR SETTINGS",
            Background);
    }
}

void Frontend::RenderSettings() {
    RenderSidebar();

    FillRect(impl_->renderer, 220, 0,
             LogicalWidth - 220, LogicalHeight, Background);

    Text(impl_->renderer, 252, 34, "SETTINGS", TextColor);
    Text(impl_->renderer, 252, 54,
         "Everyday controls, graphics, emulation, paths and updates.",
         Muted);

    const char* section_names[] = {
        "GENERAL",
        "GRAPHICS",
        "EMULATION",
        "INPUT",
        "LIBRARY",
        "UPDATES"
    };

    for (std::size_t i = 0; i < 6; ++i) {
        const float y = 116 + static_cast<float>(i) * 48;
        const bool selected =
            static_cast<std::size_t>(impl_->settings_section) == i;

        FillRect(
            impl_->renderer,
            242, y, 170, 40,
            selected ? AccentSoft : Panel);

        Text(
            impl_->renderer,
            258, y + 15,
            section_names[i],
            selected ? TextColor : Muted);
    }

    const float x = 442;

    auto option = [&](float y,
                      const std::string& name,
                      const std::string& value,
                      Color value_color = TextColor) {
        FillRect(impl_->renderer, x, y, 620, 42, Panel);
        StrokeRect(impl_->renderer, x, y, 620, 42, Border);
        Text(impl_->renderer, x + 16, y + 16, name, TextColor);
        Text(impl_->renderer, x + 420, y + 16, value, value_color);
    };

    switch (impl_->settings_section) {
    case SettingsSection::General:
        Text(impl_->renderer, x, 114, "GENERAL", TextColor);
        option(150, "Fullscreen", impl_->fullscreen ? "ON" : "OFF",
               impl_->fullscreen ? Success : Muted);
        option(202, "VSync", impl_->vsync ? "ON" : "OFF",
               impl_->vsync ? Success : Muted);
        option(254, "Check nightly updates", impl_->auto_update ? "ON" : "OFF",
               impl_->auto_update ? Success : Muted);
        option(306, "Show performance overlay",
               impl_->show_stats ? "ON" : "OFF",
               impl_->show_stats ? Success : Muted);
        Text(impl_->renderer, x, 382,
             "Alt+Enter toggles fullscreen. Changes are saved automatically.",
             Muted);
        break;

    case SettingsSection::Graphics:
        Text(impl_->renderer, x, 114, "GRAPHICS", TextColor);
        {
            const char* aspects[] = {"AUTO", "4:3", "16:9", "STRETCH"};
            option(150, "Aspect ratio",
                   aspects[std::clamp(impl_->aspect_mode, 0, 3)]);
        }
        option(202, "Integer scaling",
               impl_->integer_scale ? "ON" : "OFF",
               impl_->integer_scale ? Success : Muted);
        option(254, "Presentation",
               "LETTERBOXED / RESIZABLE", Accent);
        Text(impl_->renderer, x, 330,
             "The game framebuffer preserves its native aspect when Auto is selected.",
             Muted);
        break;

    case SettingsSection::Emulation:
        Text(impl_->renderer, x, 114, "EMULATION", TextColor);
        option(150, "Emulation speed",
               std::to_string(impl_->emulation_speed_percent) + "%",
               Accent);
        option(202, "PowerPC interpreter",
               "ACTIVE", Success);
        option(254, "IOS",
               "HLE / HOST NAND", Success);
        option(306, "GX renderer",
               "IN DEVELOPMENT", Warning);
        Text(
            impl_->renderer,
            x, 358,
            "Speed changes the amount of emulated work performed per GUI frame.",
            Muted);
        break;

    case SettingsSection::Input: {
        Text(impl_->renderer, x, 114,
             "INPUT / KEYBOARD MAPPING", TextColor);

        Text(
            impl_->renderer, x, 134,
            "ENTER remap   DELETE clear   R reset   F5 reset all",
            Muted);

        const auto& entries = impl_->keyboard_bindings.Entries();
        constexpr std::size_t VisibleRows = 17;
        const std::size_t first =
            (impl_->selected_binding / VisibleRows) * VisibleRows;
        const std::size_t last =
            std::min(first + VisibleRows, entries.size());

        for (std::size_t i = first; i < last; ++i) {
            const float y =
                162 + static_cast<float>(i - first) * 28;

            if (i == impl_->selected_binding)
                FillRect(impl_->renderer, x, y - 4, 620, 24,
                         AccentSoft);

            Text(
                impl_->renderer,
                x + 12, y,
                Shorten(entries[i].name, 34),
                Text);
            const char* name =
                entries[i].scancode == SDL_SCANCODE_UNKNOWN
                    ? "UNBOUND"
                    : SDL_GetScancodeName(entries[i].scancode);
            Text(
                impl_->renderer,
                x + 430, y,
                name && *name ? name : "UNKNOWN",
                i == impl_->selected_binding ? Accent : Muted);
        }

        if (impl_->remap_waiting) {
            FillRect(impl_->renderer, x, 640, 620, 38, Warning);
            Text(
                impl_->renderer,
                x + 14, 652,
                "PRESS A KEY TO ASSIGN  (ESC CANCELS)",
                Background);
        }

        break;
    }

    case SettingsSection::Library:
        Text(impl_->renderer, x, 114, "GAME FOLDERS", TextColor);
        FillRect(impl_->renderer, x, 150, 620, 42, AccentSoft);
        Text(impl_->renderer, x + 16, 166,
             "ADD FOLDER...", TextColor);

        FillRect(impl_->renderer, x, 202, 620, 42, Panel);
        Text(impl_->renderer, x + 16, 218,
             "RESCAN LIBRARY", TextColor);

        const auto& folders = impl_->library.Folders();
        for (std::size_t i = 0; i < folders.size(); ++i) {
            const float y = 272 + static_cast<float>(i) * 38;
            const bool selected = i == impl_->selected_folder;

            FillRect(
                impl_->renderer,
                x, y, 620, 32,
                selected ? AccentSoft : Panel);

            Text(
                impl_->renderer,
                x + 12, y + 10,
                Shorten(folders[i].string(), 72),
                selected ? TextColor : Muted);
        }

        FillRect(impl_->renderer, x, 650, 620, 36, Danger);
        Text(impl_->renderer, x + 16, 661,
             "REMOVE SELECTED FOLDER", TextColor);
        break;

    case SettingsSection::Updates: {
        Text(impl_->renderer, x, 114, "UPDATES", TextColor);

        const auto update = impl_->updater.GetResult();

        std::string state = "IDLE";
        Color state_color = Muted;

        switch (update.state) {
        case UpdateChecker::State::Checking:
            state = "CHECKING...";
            state_color = Accent;
            break;
        case UpdateChecker::State::Current:
            state = "UP TO DATE";
            state_color = Success;
            break;
        case UpdateChecker::State::Available:
            state = "UPDATE AVAILABLE";
            state_color = Warning;
            break;
        case UpdateChecker::State::Unavailable:
            state = "CHECK FAILED";
            state_color = Danger;
            break;
        case UpdateChecker::State::Idle:
            break;
        }

        option(150, "Nightly status", state, state_color);
        FillRect(impl_->renderer, x, 202, 300, 42, PanelAlt);
        FillRect(impl_->renderer, x + 320, 202, 300, 42, AccentSoft);
        Text(impl_->renderer, x + 18, 218, "CHECK NOW", TextColor);
        Text(impl_->renderer, x + 338, 218, "DOWNLOAD NIGHTLY", TextColor);

        Text(impl_->renderer, x, 272,
             update.message.empty()
                 ? "Automatic nightly checks run in the background."
                 : update.message,
             Muted);

        if (!update.latest_sha.empty())
            Text(impl_->renderer, x, 306,
                 "Latest build: " +
                     update.latest_sha.substr(0, 12),
                 Accent);

        Text(
            impl_->renderer,
            x, 348,
            "The updater downloads the platform-specific nightly artifact.",
            Muted);
        break;
    }
    }
}

void Frontend::RenderAbout() {
    RenderSidebar();

    FillRect(impl_->renderer, 220, 0,
             LogicalWidth - 220, LogicalHeight, Background);

    Text(impl_->renderer, 252, 34, "ABOUT VWII", TextColor);

    FillRect(impl_->renderer, 252, 92, 932, 190, Panel);
    Text(impl_->renderer, 282, 124,
         "A Wii emulator written from scratch for Windows and macOS.",
         Text);
    Text(impl_->renderer, 282, 152,
         "Current frontend", Muted);
    Text(impl_->renderer, 470, 152,
         "Modern launcher / SDL3 UI", Accent);
    Text(impl_->renderer, 282, 180,
         "Input", Muted);
    Text(impl_->renderer, 470, 180,
         "Wii Remote + extensions + remapping", Accent);
    Text(impl_->renderer, 282, 208,
         "Library", Muted);
    Text(impl_->renderer, 470, 208,
         "Persistent folder scanning + recent games", Accent);

    Text(impl_->renderer, 252, 324,
         "COMPATIBILITY", TextColor);
    FillRect(impl_->renderer, 252, 352, 932, 86, Panel);
    Text(
        impl_->renderer,
        276, 378,
        "PowerPC / IOS / VI foundations are active. GX rendering is still",
        Muted);
    Text(
        impl_->renderer,
        276, 400,
        "being implemented, so graphical compatibility remains limited.",
        Muted);

    Text(impl_->renderer, 252, 478,
         "QUALITY OF LIFE", TextColor);
    Text(
        impl_->renderer,
        252, 506,
        "Drag-and-drop  |  Folder dialogs  |  Persistent settings  |  Nightly updater",
        Success);
    Text(
        impl_->renderer,
        252, 532,
        "Keyboard: F12 settings, Ctrl+O open game, Ctrl+L add folder, F5 rescan",
        Muted);
}

void Frontend::RenderXfb(const Status& status,
                         const memory::Memory* memory) {
    if (!memory)
        return;

    const bool valid =
        status.loaded &&
        status.xfb_address != 0 &&
        status.xfb_width >= 2 &&
        status.xfb_width <= 1024 &&
        status.xfb_height >= 2 &&
        status.xfb_height <= 1024 &&
        status.xfb_stride >= status.xfb_width * 2 &&
        status.xfb_stride <= 8192;

    if (!valid)
        return;

    try {
        const std::size_t raw_size =
            static_cast<std::size_t>(status.xfb_stride) *
            status.xfb_height;
        const std::size_t rgba_size =
            static_cast<std::size_t>(status.xfb_width) *
            status.xfb_height * 4;

        impl_->xfb_raw.resize(raw_size);
        impl_->rgba.resize(rgba_size);

        memory->ReadBlock(
            status.xfb_address,
            std::span<uint8_t>(
                impl_->xfb_raw.data(),
                impl_->xfb_raw.size()));

        for (uint32_t y = 0; y < status.xfb_height; ++y) {
            const uint8_t* source =
                impl_->xfb_raw.data() +
                static_cast<std::size_t>(y) * status.xfb_stride;

            uint8_t* destination =
                impl_->rgba.data() +
                static_cast<std::size_t>(y) *
                    status.xfb_width * 4;

            for (uint32_t x = 0; x < status.xfb_width; x += 2) {
                const std::size_t source_index =
                    static_cast<std::size_t>(x) * 2;

                const uint8_t y0 = source[source_index];
                const uint8_t u = source[source_index + 1];
                const uint8_t y1 =
                    x + 1 < status.xfb_width
                        ? source[source_index + 2]
                        : y0;
                const uint8_t v =
                    x + 1 < status.xfb_width
                        ? source[source_index + 3]
                        : u;

                auto clamp = [](float value) {
                    return static_cast<uint8_t>(
                        std::clamp(value, 0.0f, 255.0f));
                };

                auto convert = [&](uint8_t y_value,
                                   uint8_t& r,
                                   uint8_t& g,
                                   uint8_t& b) {
                    const float yf =
                        static_cast<float>(y_value);
                    const float uf =
                        static_cast<float>(u) - 128.0f;
                    const float vf =
                        static_cast<float>(v) - 128.0f;

                    r = clamp(
                        1.164f * (yf - 16.0f) + 1.596f * vf);
                    g = clamp(
                        1.164f * (yf - 16.0f) -
                        0.392f * uf -
                        0.813f * vf);
                    b = clamp(
                        1.164f * (yf - 16.0f) +
                        2.017f * uf);
                };

                uint8_t r0, g0, b0;
                uint8_t r1, g1, b1;
                convert(y0, r0, g0, b0);
                convert(y1, r1, g1, b1);

                const std::size_t destination_index =
                    static_cast<std::size_t>(x) * 4;

                destination[destination_index + 0] = r0;
                destination[destination_index + 1] = g0;
                destination[destination_index + 2] = b0;
                destination[destination_index + 3] = 255;

                if (x + 1 < status.xfb_width) {
                    destination[destination_index + 4] = r1;
                    destination[destination_index + 5] = g1;
                    destination[destination_index + 6] = b1;
                    destination[destination_index + 7] = 255;
                }
            }
        }

        if (!impl_->xfb_texture ||
            impl_->xfb_width !=
                static_cast<int>(status.xfb_width) ||
            impl_->xfb_height !=
                static_cast<int>(status.xfb_height)) {
            if (impl_->xfb_texture)
                SDL_DestroyTexture(impl_->xfb_texture);

            impl_->xfb_texture = SDL_CreateTexture(
                impl_->renderer,
                SDL_PIXELFORMAT_RGBA8888,
                SDL_TEXTUREACCESS_STREAMING,
                static_cast<int>(status.xfb_width),
                static_cast<int>(status.xfb_height));

            impl_->xfb_width =
                static_cast<int>(status.xfb_width);
            impl_->xfb_height =
                static_cast<int>(status.xfb_height);
        }

        if (!impl_->xfb_texture)
            return;

        SDL_UpdateTexture(
            impl_->xfb_texture,
            nullptr,
            impl_->rgba.data(),
            static_cast<int>(status.xfb_width * 4));

        const float frame_aspect =
            static_cast<float>(status.xfb_width) /
            static_cast<float>(status.xfb_height);

        float destination_aspect = frame_aspect;
        if (impl_->aspect_mode == 1)
            destination_aspect = 4.0f / 3.0f;
        else if (impl_->aspect_mode == 2)
            destination_aspect = 16.0f / 9.0f;
        else if (impl_->aspect_mode == 3)
            destination_aspect = 0.0f;

        SDL_FRect destination{};

        int window_width = LogicalWidth;
        int window_height = LogicalHeight;
        SDL_GetRenderOutputSize(
            impl_->renderer, &window_width, &window_height);

        if (destination_aspect == 0.0f) {
            destination = {
                0.0f, 0.0f,
                static_cast<float>(LogicalWidth),
                static_cast<float>(LogicalHeight)
            };
        } else if (static_cast<float>(LogicalWidth) /
                       static_cast<float>(LogicalHeight) >
                   destination_aspect) {
            destination.h = static_cast<float>(LogicalHeight);
            destination.w =
                destination.h * destination_aspect;
            destination.x =
                (LogicalWidth - destination.w) / 2.0f;
        } else {
            destination.w = static_cast<float>(LogicalWidth);
            destination.h =
                destination.w / destination_aspect;
            destination.y =
                (LogicalHeight - destination.h) / 2.0f;
        }

        SDL_RenderTexture(
            impl_->renderer,
            impl_->xfb_texture,
            nullptr,
            &destination);

        if (impl_->show_stats) {
            FillRect(
                impl_->renderer,
                12, 12, 260, 36,
                Color{0, 0, 0, 180});
            Text(
                impl_->renderer,
                22, 25,
                status.game_id.empty()
                    ? "WII GAME"
                    : status.game_id,
                Text);
        }
    } catch (const std::out_of_range&) {
        // Invalid XFB mappings fall through to the launcher surface.
    }
}

void Frontend::Present(const Status& status,
                       const memory::Memory* memory) {
    if (!impl_->renderer)
        return;

    SetColor(impl_->renderer, Background);
    SDL_RenderClear(impl_->renderer);

    if (status.loaded) {
        const std::size_t before = impl_->xfb_raw.size();
        RenderXfb(status, memory);

        const bool rendered_xfb =
            impl_->xfb_texture != nullptr &&
            impl_->xfb_raw.size() == before &&
            status.xfb_address != 0 &&
            status.xfb_width >= 2 &&
            status.xfb_height >= 2;

        if (rendered_xfb && impl_->page != Page::Settings) {
            if (impl_->show_stats) {
                Text(
                    impl_->renderer,
                    18, 690,
                    "F12 SETTINGS   ALT+ENTER FULLSCREEN   ESC EXIT",
                    Muted);
            }
            if (impl_->updater.GetResult().state ==
                UpdateChecker::State::Available) {
                FillRect(
                    impl_->renderer,
                    900, 14, 350, 34,
                    Warning);
                Text(
                    impl_->renderer,
                    914, 25,
                    "NIGHTLY UPDATE AVAILABLE",
                    Background);
            }
            SDL_RenderPresent(impl_->renderer);
            return;
        }
    }

    switch (impl_->page) {
    case Page::Library:
        RenderLibrary(status);
        break;
    case Page::Settings:
        RenderSettings();
        break;
    case Page::About:
        RenderAbout();
        break;
    }

    SDL_RenderPresent(impl_->renderer);
}

} // namespace vwii::frontend
