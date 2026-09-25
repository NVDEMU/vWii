#include "frontend/frontend.h"

#include "memory/memory.h"
#include "input/wiimote_keyboard.h"
#include "frontend/keyboard_bindings.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace vwii::frontend {

namespace {

uint8_t ClampByte(float value) {
    return static_cast<uint8_t>(
        std::clamp(value, 0.0f, 255.0f));
}

void YuvToRgb(uint8_t y, uint8_t u, uint8_t v,
              uint8_t& r, uint8_t& g, uint8_t& b) {
    const float yf = static_cast<float>(y);
    const float uf = static_cast<float>(u) - 128.0f;
    const float vf = static_cast<float>(v) - 128.0f;

    r = ClampByte(1.164f * (yf - 16.0f) + 1.596f * vf);
    g = ClampByte(1.164f * (yf - 16.0f) - 0.392f * uf - 0.813f * vf);
    b = ClampByte(1.164f * (yf - 16.0f) + 2.017f * uf);
}

} // namespace

struct Frontend::Impl {
    SDL_Window* window{};
    SDL_Renderer* renderer{};
    SDL_Texture* xfb_texture{};

    int xfb_width{};
    int xfb_height{};

    std::vector<uint8_t> xfb_raw;
    std::vector<uint8_t> rgba;
    std::string dropped_file;

    KeyboardBindings keyboard_bindings;
    bool settings_open{};
    bool remap_waiting{};
    std::size_t selected_binding{};
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

    impl_->keyboard_bindings.Load();
    return true;
}

void Frontend::Shutdown() {
    if (!impl_)
        return;

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

namespace {

void SendMappedKey(const KeyboardBindings& bindings,
                   input::WiiRemoteKeyboard* wiimote,
                   SDL_Scancode scancode,
                   bool pressed) {
    bindings.Send(wiimote, scancode, pressed);
}

const char* ScancodeLabel(SDL_Scancode scancode) {
    if (scancode == SDL_SCANCODE_UNKNOWN)
        return "Unbound";

    const char* name = SDL_GetScancodeName(scancode);
    return (name && *name) ? name : "Unknown";
}

} // namespace


bool Frontend::PumpEvents(input::WiiRemoteKeyboard* wiimote,
                          float delta_seconds) {
    SDL_Event event{};

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT)
            return false;

        if (event.type == SDL_EVENT_DROP_FILE && event.drop.data)
            impl_->dropped_file = event.drop.data;

        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
            const SDL_Scancode scancode = event.key.scancode;

            if (scancode == SDL_SCANCODE_F12) {
                impl_->settings_open = !impl_->settings_open;
                impl_->remap_waiting = false;
                if (impl_->settings_open)
                    impl_->keyboard_bindings.ReleaseAll(wiimote);
                continue;
            }

            if (impl_->settings_open) {
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

                const auto& entries = impl_->keyboard_bindings.Entries();
                if (entries.empty())
                    continue;

                if (scancode == SDL_SCANCODE_ESCAPE) {
                    impl_->settings_open = false;
                } else if (scancode == SDL_SCANCODE_UP) {
                    if (impl_->selected_binding > 0)
                        --impl_->selected_binding;
                } else if (scancode == SDL_SCANCODE_DOWN) {
                    if (impl_->selected_binding + 1 < entries.size())
                        ++impl_->selected_binding;
                } else if (scancode == SDL_SCANCODE_PAGEUP) {
                    impl_->selected_binding =
                        impl_->selected_binding > 10
                            ? impl_->selected_binding - 10
                            : 0;
                } else if (scancode == SDL_SCANCODE_PAGEDOWN) {
                    impl_->selected_binding = std::min(
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
                        impl_->selected_binding, SDL_SCANCODE_UNKNOWN);
                    impl_->keyboard_bindings.Save();
                } else if (scancode == SDL_SCANCODE_R) {
                    impl_->keyboard_bindings.Reset(
                        impl_->selected_binding);
                    impl_->keyboard_bindings.Save();
                } else if (scancode == SDL_SCANCODE_F5) {
                    impl_->keyboard_bindings.ResetAll();
                    impl_->keyboard_bindings.Save();
                }
                continue;
            }

            if (scancode == SDL_SCANCODE_ESCAPE)
                return false;

            if (wiimote && impl_->keyboard_bindings.Has(scancode))
                SendMappedKey(impl_->keyboard_bindings, wiimote,
                              scancode, true);
        }

        if (wiimote && event.type == SDL_EVENT_KEY_UP &&
            !impl_->settings_open &&
            impl_->keyboard_bindings.Has(event.key.scancode)) {
            SendMappedKey(impl_->keyboard_bindings, wiimote,
                          event.key.scancode, false);
        }
    }

    if (wiimote && !impl_->settings_open)
        wiimote->Update(delta_seconds);

    return true;
}

bool Frontend::SettingsOpen() const {
    return impl_->settings_open;
}

std::string Frontend::ConsumeDroppedFile() {
    std::string result = std::move(impl_->dropped_file);
    impl_->dropped_file.clear();
    return result;
}

void Frontend::RenderSettings() {
    if (!impl_->settings_open || !impl_->renderer)
        return;

    int width = 0;
    int height = 0;
    SDL_GetRenderOutputSize(impl_->renderer, &width, &height);

    SDL_SetRenderDrawColor(impl_->renderer, 15, 18, 24, 255);
    const SDL_FRect background{
        24.0f, 24.0f,
        static_cast<float>(width > 48 ? width - 48 : 1),
        static_cast<float>(height > 48 ? height - 48 : 1)
    };
    SDL_RenderFillRect(impl_->renderer, &background);

    SDL_SetRenderDrawColor(impl_->renderer, 54, 64, 82, 255);
    const SDL_FRect header{40.0f, 40.0f,
                           static_cast<float>(width > 80 ? width - 80 : 1),
                           48.0f};
    SDL_RenderFillRect(impl_->renderer, &header);

    SDL_SetRenderDrawColor(impl_->renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(impl_->renderer, 56.0f, 56.0f,
                        "SETTINGS - KEYBOARD CONTROLS");

    const auto& entries = impl_->keyboard_bindings.Entries();
    constexpr std::size_t VisibleRows = 21;
    const std::size_t page =
        impl_->selected_binding / VisibleRows;
    const std::size_t first = page * VisibleRows;
    const std::size_t last =
        std::min(first + VisibleRows, entries.size());

    SDL_RenderDebugText(
        impl_->renderer,
        56.0f,
        96.0f,
        "UP/DOWN select  PgUp/PgDn page  ENTER remap  DELETE clear  R reset  F5 reset all  F12 close");

    for (std::size_t i = first; i < last; ++i) {
        const float y = 120.0f +
                        static_cast<float>(i - first) * 24.0f;

        if (i == impl_->selected_binding) {
            SDL_SetRenderDrawColor(impl_->renderer, 64, 90, 126, 255);
            const SDL_FRect row{
                48.0f,
                y - 3.0f,
                static_cast<float>(width > 96 ? width - 96 : 1),
                20.0f
            };
            SDL_RenderFillRect(impl_->renderer, &row);
            SDL_SetRenderDrawColor(impl_->renderer, 255, 255, 255, 255);
        }

        SDL_RenderDebugTextFormat(
            impl_->renderer,
            56.0f,
            y,
            "%02u %-34s : %s",
            static_cast<unsigned>(i + 1),
            entries[i].name,
            ScancodeLabel(entries[i].scancode));
    }

    const float footer_y =
        static_cast<float>(height > 24 ? height - 24 : 0);
    SDL_RenderDebugTextFormat(
        impl_->renderer,
        48.0f,
        footer_y,
        "Bindings %u-%u of %u",
        static_cast<unsigned>(first + 1),
        static_cast<unsigned>(last),
        static_cast<unsigned>(entries.size()));

    if (impl_->remap_waiting) {
        SDL_SetRenderDrawColor(impl_->renderer, 255, 220, 100, 255);
        SDL_RenderDebugText(
            impl_->renderer,
            56.0f,
            104.0f,
            "PRESS A KEY TO ASSIGN IT (ESC cancels)");
    }
}

void Frontend::Present(const Status& status, const memory::Memory* memory) {
    if (!impl_->renderer)
        return;

    int window_width = 0;
    int window_height = 0;
    SDL_GetRenderOutputSize(
        impl_->renderer,
        &window_width,
        &window_height);

    SDL_SetRenderDrawColor(impl_->renderer, 8, 10, 14, 255);
    SDL_RenderClear(impl_->renderer);

    const bool valid_xfb =
        memory != nullptr &&
        status.loaded &&
        status.xfb_address != 0 &&
        status.xfb_width >= 2 &&
        status.xfb_width <= 1024 &&
        status.xfb_height >= 2 &&
        status.xfb_height <= 1024 &&
        status.xfb_stride >= status.xfb_width * 2 &&
        status.xfb_stride <= 8192;

    if (valid_xfb) {
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
                std::span<uint8_t>(impl_->xfb_raw.data(), impl_->xfb_raw.size()));

            for (uint32_t y = 0; y < status.xfb_height; ++y) {
                const uint8_t* source =
                    impl_->xfb_raw.data() +
                    static_cast<std::size_t>(y) * status.xfb_stride;

                uint8_t* destination =
                    impl_->rgba.data() +
                    static_cast<std::size_t>(y) * status.xfb_width * 4;

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

                    uint8_t r0, g0, b0;
                    uint8_t r1, g1, b1;

                    YuvToRgb(y0, u, v, r0, g0, b0);
                    YuvToRgb(y1, u, v, r1, g1, b1);

                    const std::size_t d0 =
                        static_cast<std::size_t>(x) * 4;

                    destination[d0 + 0] = r0;
                    destination[d0 + 1] = g0;
                    destination[d0 + 2] = b0;
                    destination[d0 + 3] = 255;

                    if (x + 1 < status.xfb_width) {
                        const std::size_t d1 = d0 + 4;
                        destination[d1 + 0] = r1;
                        destination[d1 + 1] = g1;
                        destination[d1 + 2] = b1;
                        destination[d1 + 3] = 255;
                    }
                }
            }

            if (!impl_->xfb_texture ||
                impl_->xfb_width != static_cast<int>(status.xfb_width) ||
                impl_->xfb_height != static_cast<int>(status.xfb_height)) {
                if (impl_->xfb_texture) {
                    SDL_DestroyTexture(impl_->xfb_texture);
                    impl_->xfb_texture = nullptr;
                }

                impl_->xfb_texture = SDL_CreateTexture(
                    impl_->renderer,
                    SDL_PIXELFORMAT_RGBA8888,
                    SDL_TEXTUREACCESS_STREAMING,
                    static_cast<int>(status.xfb_width),
                    static_cast<int>(status.xfb_height));

                if (impl_->xfb_texture) {
                    impl_->xfb_width = static_cast<int>(status.xfb_width);
                    impl_->xfb_height = static_cast<int>(status.xfb_height);
                }
            }

            if (impl_->xfb_texture) {
                SDL_UpdateTexture(
                    impl_->xfb_texture,
                    nullptr,
                    impl_->rgba.data(),
                    static_cast<int>(status.xfb_width * 4));

                const float framebuffer_aspect =
                    static_cast<float>(status.xfb_width) /
                    static_cast<float>(status.xfb_height);

                const float window_aspect =
                    window_height > 0
                        ? static_cast<float>(window_width) /
                          static_cast<float>(window_height)
                        : framebuffer_aspect;

                SDL_FRect destination{};

                if (window_aspect > framebuffer_aspect) {
                    destination.h = static_cast<float>(window_height);
                    destination.w = destination.h * framebuffer_aspect;
                    destination.x =
                        (static_cast<float>(window_width) - destination.w) / 2.0f;
                    destination.y = 0.0f;
                } else {
                    destination.w = static_cast<float>(window_width);
                    destination.h = destination.w / framebuffer_aspect;
                    destination.x = 0.0f;
                    destination.y =
                        (static_cast<float>(window_height) - destination.h) / 2.0f;
                }

                SDL_RenderTexture(
                    impl_->renderer,
                    impl_->xfb_texture,
                    nullptr,
                    &destination);
                RenderSettings();
                SDL_RenderPresent(impl_->renderer);
                return;
            }
        } catch (const std::out_of_range&) {
            // Invalid XFB addresses remain a normal early-compatibility failure.
        }
    }

    // Fallback status surface until a title produces a usable XFB.
    SDL_SetRenderDrawColor(impl_->renderer, 35, 39, 48, 255);

    const SDL_FRect panel{
        32.0f,
        32.0f,
        static_cast<float>(window_width > 64 ? window_width - 64 : 1),
        128.0f
    };

    SDL_RenderFillRect(impl_->renderer, &panel);

    const uint32_t pc = status.pc;
    const int bar_width = window_width > 80 ? window_width - 80 : 1;
    const int progress = static_cast<int>(
        (static_cast<uint64_t>(pc & 0x00FFFFFFu) *
         static_cast<uint64_t>(bar_width)) /
        0x01000000u);

    SDL_SetRenderDrawColor(impl_->renderer, 75, 145, 220, 255);

    const SDL_FRect pc_bar{
        40.0f,
        80.0f,
        static_cast<float>(progress > 0 ? progress : 1),
        16.0f
    };

    SDL_RenderFillRect(impl_->renderer, &pc_bar);

    SDL_SetRenderDrawColor(
        impl_->renderer,
        status.halted ? 220 : 70,
        status.halted ? 65 : 200,
        status.loaded ? 80 : 70,
        255);

    const SDL_FRect state_bar{
        40.0f,
        120.0f,
        static_cast<float>(bar_width > 1 ? bar_width : 1),
        12.0f
    };

    SDL_RenderFillRect(impl_->renderer, &state_bar);
    RenderSettings();
    SDL_RenderPresent(impl_->renderer);
}

} // namespace vwii::frontend
