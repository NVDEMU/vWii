#include "frontend/frontend.h"

#include "memory/memory.h"
#include "input/wiimote_keyboard.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
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

void SendMappedKey(input::WiiRemoteKeyboard* wiimote,
                   SDL_Scancode scancode, bool pressed) {
    if (!wiimote)
        return;

    auto send = [&](input::Key key) {
        wiimote->KeyEvent(key, pressed);
    };

    switch (scancode) {
    case SDL_SCANCODE_UP: send(input::Key::DpadUp); break;
    case SDL_SCANCODE_DOWN: send(input::Key::DpadDown); break;
    case SDL_SCANCODE_LEFT: send(input::Key::DpadLeft); break;
    case SDL_SCANCODE_RIGHT: send(input::Key::DpadRight); break;
    case SDL_SCANCODE_SPACE: send(input::Key::A); break;
    case SDL_SCANCODE_RCTRL: send(input::Key::B); break;
    case SDL_SCANCODE_Z: send(input::Key::One); break;
    case SDL_SCANCODE_X: send(input::Key::Two); break;
    case SDL_SCANCODE_EQUALS: send(input::Key::Plus); break;
    case SDL_SCANCODE_MINUS: send(input::Key::Minus); break;
    case SDL_SCANCODE_BACKSPACE: send(input::Key::Home); break;

    case SDL_SCANCODE_KP_8: send(input::Key::IRUp); break;
    case SDL_SCANCODE_KP_2: send(input::Key::IRDown); break;
    case SDL_SCANCODE_KP_4: send(input::Key::IRLeft); break;
    case SDL_SCANCODE_KP_6: send(input::Key::IRRight); break;
    case SDL_SCANCODE_KP_5: send(input::Key::IRCenter); break;
    case SDL_SCANCODE_KP_7: send(input::Key::IRZoomOut); break;
    case SDL_SCANCODE_KP_9: send(input::Key::IRZoomIn); break;

    case SDL_SCANCODE_W: send(input::Key::NunchukUp); break;
    case SDL_SCANCODE_S: send(input::Key::NunchukDown); break;
    case SDL_SCANCODE_A: send(input::Key::NunchukLeft); break;
    case SDL_SCANCODE_D: send(input::Key::NunchukRight); break;
    case SDL_SCANCODE_Q: send(input::Key::NunchukC); break;
    case SDL_SCANCODE_E: send(input::Key::NunchukZ); break;

    case SDL_SCANCODE_I: send(input::Key::ClassicUp); break;
    case SDL_SCANCODE_K: send(input::Key::ClassicDown); break;
    case SDL_SCANCODE_J: send(input::Key::ClassicLeft); break;
    case SDL_SCANCODE_L: send(input::Key::ClassicRight); break;
    case SDL_SCANCODE_U: send(input::Key::ClassicA); break;
    case SDL_SCANCODE_O: send(input::Key::ClassicB); break;
    case SDL_SCANCODE_P: send(input::Key::ClassicX); break;
    case SDL_SCANCODE_LEFTBRACKET: send(input::Key::ClassicY); break;
    case SDL_SCANCODE_N: send(input::Key::ClassicL); break;
    case SDL_SCANCODE_M: send(input::Key::ClassicR); break;
    case SDL_SCANCODE_COMMA: send(input::Key::ClassicZL); break;
    case SDL_SCANCODE_PERIOD: send(input::Key::ClassicZR); break;

    case SDL_SCANCODE_F2: send(input::Key::ExtensionNunchuk); break;
    case SDL_SCANCODE_F3: send(input::Key::ExtensionClassic); break;
    case SDL_SCANCODE_F4: send(input::Key::ExtensionGuitar); break;
    case SDL_SCANCODE_F5: send(input::Key::ExtensionDrums); break;
    case SDL_SCANCODE_F6: send(input::Key::ExtensionTurntable); break;
    case SDL_SCANCODE_F7: send(input::Key::ExtensionUDraw); break;
    case SDL_SCANCODE_F8: send(input::Key::ExtensionDrawsome); break;
    case SDL_SCANCODE_F9: send(input::Key::ExtensionTaTaCon); break;
    case SDL_SCANCODE_F10: send(input::Key::ToggleMotionPlus); break;
    case SDL_SCANCODE_F11: send(input::Key::ExtensionShinkansen); break;
    default: break;
    }

    switch (scancode) {
    case SDL_SCANCODE_1:
        send(input::Key::One);
        send(input::Key::GuitarGreen);
        send(input::Key::DrumRed);
        break;
    case SDL_SCANCODE_2:
        send(input::Key::Two);
        send(input::Key::GuitarRed);
        send(input::Key::DrumYellow);
        break;
    case SDL_SCANCODE_3:
        send(input::Key::GuitarYellow);
        send(input::Key::DrumBlue);
        break;
    case SDL_SCANCODE_4:
        send(input::Key::GuitarBlue);
        send(input::Key::DrumGreen);
        break;
    case SDL_SCANCODE_5:
        send(input::Key::GuitarOrange);
        send(input::Key::DrumOrange);
        break;
    case SDL_SCANCODE_6:
        send(input::Key::DrumRed);
        break;
    case SDL_SCANCODE_7:
        send(input::Key::DrumYellow);
        break;
    case SDL_SCANCODE_8:
        send(input::Key::DrumBlue);
        break;
    case SDL_SCANCODE_9:
        send(input::Key::DrumGreen);
        send(input::Key::ClassicMinus);
        break;
    case SDL_SCANCODE_0:
        send(input::Key::DrumOrange);
        send(input::Key::ClassicPlus);
        break;
    default:
        break;
    }

    switch (scancode) {
    case SDL_SCANCODE_G:
        send(input::Key::GuitarStrumUp);
        send(input::Key::TurntableRed);
        send(input::Key::ShinkansenBrake);
        break;
    case SDL_SCANCODE_H:
        send(input::Key::GuitarStrumDown);
        send(input::Key::TurntableBlue);
        send(input::Key::ShinkansenHorn);
        break;
    case SDL_SCANCODE_T:
        send(input::Key::GuitarWhammyDown);
        send(input::Key::AccelYNegative);
        break;
    case SDL_SCANCODE_Y:
        send(input::Key::GuitarWhammyUp);
        send(input::Key::AccelZPositive);
        break;
    case SDL_SCANCODE_F:
        send(input::Key::TurntableGreen);
        send(input::Key::AccelYPositive);
        send(input::Key::ShinkansenThrottleDown);
        break;
    case SDL_SCANCODE_R:
        send(input::Key::AccelYPositive);
        send(input::Key::ShinkansenThrottleUp);
        break;
    case SDL_SCANCODE_V:
        send(input::Key::TaTaConHit);
        send(input::Key::ShinkansenBrake);
        break;
    case SDL_SCANCODE_B:
        send(input::Key::TaTaConRim);
        break;
    default:
        break;
    }

    switch (scancode) {
    case SDL_SCANCODE_SEMICOLON:
        send(input::Key::UDrawUp);
        send(input::Key::DrawsomeUp);
        break;
    case SDL_SCANCODE_SLASH:
        send(input::Key::UDrawDown);
        send(input::Key::DrawsomeDown);
        break;
    case SDL_SCANCODE_APOSTROPHE:
        send(input::Key::UDrawPen);
        send(input::Key::DrawsomePen);
        break;
    case SDL_SCANCODE_RIGHTBRACKET:
        send(input::Key::UDrawA);
        send(input::Key::DrawsomeA);
        break;
    case SDL_SCANCODE_LEFTBRACKET:
        send(input::Key::UDrawB);
        send(input::Key::DrawsomeB);
        send(input::Key::ClassicY);
        break;
    case SDL_SCANCODE_COMMA:
        send(input::Key::UDrawLeft);
        send(input::Key::DrawsomeLeft);
        send(input::Key::TurntableCrossfadeLeft);
        send(input::Key::ClassicZL);
        break;
    case SDL_SCANCODE_PERIOD:
        send(input::Key::UDrawRight);
        send(input::Key::DrawsomeRight);
        send(input::Key::TurntableCrossfadeRight);
        send(input::Key::ClassicZR);
        break;
    case SDL_SCANCODE_6:
        send(input::Key::ClassicHome);
        send(input::Key::DrumRed);
        break;
    default:
        break;
    }

    switch (scancode) {
    case SDL_SCANCODE_I: send(input::Key::MotionPitchUp); break;
    case SDL_SCANCODE_K: send(input::Key::MotionPitchDown); break;
    case SDL_SCANCODE_J: send(input::Key::MotionYawLeft); break;
    case SDL_SCANCODE_L: send(input::Key::MotionYawRight); break;
    case SDL_SCANCODE_U: send(input::Key::MotionRollLeft); break;
    case SDL_SCANCODE_O: send(input::Key::MotionRollRight); break;
    case SDL_SCANCODE_T: send(input::Key::AccelXNegative); break;
    case SDL_SCANCODE_G: send(input::Key::AccelXPositive); break;
    case SDL_SCANCODE_R: send(input::Key::AccelYPositive); break;
    case SDL_SCANCODE_F: send(input::Key::AccelYNegative); break;
    case SDL_SCANCODE_Y: send(input::Key::AccelZPositive); break;
    case SDL_SCANCODE_H: send(input::Key::AccelZNegative); break;
    case SDL_SCANCODE_Q: send(input::Key::Shake); break;
    default: break;
    }
}

bool IsMappedKey(SDL_Scancode scancode) {
    switch (scancode) {
    case SDL_SCANCODE_UP:
    case SDL_SCANCODE_DOWN:
    case SDL_SCANCODE_LEFT:
    case SDL_SCANCODE_RIGHT:
    case SDL_SCANCODE_SPACE:
    case SDL_SCANCODE_RCTRL:
    case SDL_SCANCODE_Z:
    case SDL_SCANCODE_X:
    case SDL_SCANCODE_EQUALS:
    case SDL_SCANCODE_MINUS:
    case SDL_SCANCODE_BACKSPACE:
    case SDL_SCANCODE_KP_8:
    case SDL_SCANCODE_KP_2:
    case SDL_SCANCODE_KP_4:
    case SDL_SCANCODE_KP_6:
    case SDL_SCANCODE_KP_5:
    case SDL_SCANCODE_KP_7:
    case SDL_SCANCODE_KP_9:
    case SDL_SCANCODE_W:
    case SDL_SCANCODE_S:
    case SDL_SCANCODE_A:
    case SDL_SCANCODE_D:
    case SDL_SCANCODE_Q:
    case SDL_SCANCODE_E:
    case SDL_SCANCODE_I:
    case SDL_SCANCODE_K:
    case SDL_SCANCODE_J:
    case SDL_SCANCODE_L:
    case SDL_SCANCODE_U:
    case SDL_SCANCODE_O:
    case SDL_SCANCODE_P:
    case SDL_SCANCODE_LEFTBRACKET:
    case SDL_SCANCODE_N:
    case SDL_SCANCODE_M:
    case SDL_SCANCODE_COMMA:
    case SDL_SCANCODE_PERIOD:
    case SDL_SCANCODE_0:
    case SDL_SCANCODE_1:
    case SDL_SCANCODE_2:
    case SDL_SCANCODE_3:
    case SDL_SCANCODE_4:
    case SDL_SCANCODE_5:
    case SDL_SCANCODE_6:
    case SDL_SCANCODE_7:
    case SDL_SCANCODE_8:
    case SDL_SCANCODE_9:
    case SDL_SCANCODE_B:
    case SDL_SCANCODE_F:
    case SDL_SCANCODE_G:
    case SDL_SCANCODE_H:
    case SDL_SCANCODE_R:
    case SDL_SCANCODE_T:
    case SDL_SCANCODE_V:
    case SDL_SCANCODE_Y:
    case SDL_SCANCODE_APOSTROPHE:
    case SDL_SCANCODE_SEMICOLON:
    case SDL_SCANCODE_SLASH:
    case SDL_SCANCODE_RIGHTBRACKET:
    case SDL_SCANCODE_F2:
    case SDL_SCANCODE_F3:
    case SDL_SCANCODE_F4:
    case SDL_SCANCODE_F5:
    case SDL_SCANCODE_F6:
    case SDL_SCANCODE_F7:
    case SDL_SCANCODE_F8:
    case SDL_SCANCODE_F9:
    case SDL_SCANCODE_F10:
    case SDL_SCANCODE_F11:
        return true;
    default:
        return false;
    }
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

        if (wiimote && event.type == SDL_EVENT_KEY_DOWN &&
            !event.key.repeat && IsMappedKey(event.key.scancode)) {
            SendMappedKey(wiimote, event.key.scancode, true);
        }

        if (wiimote && event.type == SDL_EVENT_KEY_UP &&
            IsMappedKey(event.key.scancode)) {
            SendMappedKey(wiimote, event.key.scancode, false);
        }

        if (event.type == SDL_EVENT_KEY_DOWN &&
            event.key.scancode == SDL_SCANCODE_ESCAPE)
            return false;
    }

    if (wiimote)
        wiimote->Update(delta_seconds);

    return true;
}

std::string Frontend::ConsumeDroppedFile() {
    std::string result = std::move(impl_->dropped_file);
    impl_->dropped_file.clear();
    return result;
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
    SDL_RenderPresent(impl_->renderer);
}

} // namespace vwii::frontend
