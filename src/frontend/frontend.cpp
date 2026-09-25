#include "frontend/frontend.h"

#include <SDL3/SDL.h>

namespace vwii::frontend {

struct Frontend::Impl {
    SDL_Window* window{};
    SDL_Renderer* renderer{};
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

bool Frontend::PumpEvents() {
    SDL_Event event{};

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT)
            return false;

        if (event.type == SDL_EVENT_DROP_FILE && event.drop.data)
            impl_->dropped_file = event.drop.data;

        if (event.type == SDL_EVENT_KEY_DOWN &&
            event.key.key == SDLK_ESCAPE)
            return false;
    }

    return true;
}

std::string Frontend::ConsumeDroppedFile() {
    std::string result = std::move(impl_->dropped_file);
    impl_->dropped_file.clear();
    return result;
}

void Frontend::Present(const Status& status) {
    if (!impl_->renderer)
        return;

    int width = 0;
    int height = 0;
    SDL_GetRenderOutputSize(impl_->renderer, &width, &height);

    SDL_SetRenderDrawColor(impl_->renderer, 8, 10, 14, 255);
    SDL_RenderClear(impl_->renderer);

    // Until GX/VI are implemented, show an emulator status surface rather
    // than pretending that the black image is a rendered Wii framebuffer.
    SDL_SetRenderDrawColor(impl_->renderer, 35, 39, 48, 255);
    const SDL_FRect panel{
        32.0f,
        32.0f,
        static_cast<float>(width > 64 ? width - 64 : 1),
        128.0f
    };
    SDL_RenderFillRect(impl_->renderer, &panel);

    const uint32_t pc = status.pc;
    const int bar_width = width > 80 ? width - 80 : 1;
    const int progress = static_cast<int>(
        (static_cast<uint64_t>(pc & 0x00FFFFFFu) * static_cast<uint64_t>(bar_width)) /
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
