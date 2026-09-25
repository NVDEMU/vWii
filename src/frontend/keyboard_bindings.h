#pragma once

#include "input/wiimote_keyboard.h"

#include <SDL3/SDL.h>

#include <string>
#include <vector>

namespace vwii::frontend {

struct KeyboardBinding {
    input::Key key;
    const char* name;
    SDL_Scancode default_scancode;
    SDL_Scancode scancode;
};

class KeyboardBindings {
public:
    KeyboardBindings();

    void Load();
    void Save() const;
    void ResetAll();
    void Reset(std::size_t index);
    void Set(std::size_t index, SDL_Scancode scancode);

    [[nodiscard]] const std::vector<KeyboardBinding>& Entries() const {
        return bindings_;
    }

    [[nodiscard]] bool Has(SDL_Scancode scancode) const;
    void Send(input::WiiRemoteKeyboard* wiimote,
              SDL_Scancode scancode,
              bool pressed) const;
    void ReleaseAll(input::WiiRemoteKeyboard* wiimote) const;

private:
    std::vector<KeyboardBinding> bindings_;
    std::string config_path_;
};

} // namespace vwii::frontend
