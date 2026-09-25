#include "frontend/keyboard_bindings.h"

#include <fstream>
#include <sstream>

namespace vwii::frontend {

namespace {

std::vector<KeyboardBinding> DefaultBindings() {
    using input::Key;
    using SDL = SDL_Scancode;

    return {
        {Key::DpadUp, "Wii Remote D-pad Up", SDL_SCANCODE_UP, SDL_SCANCODE_UP},
        {Key::DpadDown, "Wii Remote D-pad Down", SDL_SCANCODE_DOWN, SDL_SCANCODE_DOWN},
        {Key::DpadLeft, "Wii Remote D-pad Left", SDL_SCANCODE_LEFT, SDL_SCANCODE_LEFT},
        {Key::DpadRight, "Wii Remote D-pad Right", SDL_SCANCODE_RIGHT, SDL_SCANCODE_RIGHT},
        {Key::A, "Wii Remote A", SDL_SCANCODE_SPACE, SDL_SCANCODE_SPACE},
        {Key::B, "Wii Remote B", SDL_SCANCODE_RCTRL, SDL_SCANCODE_RCTRL},
        {Key::One, "Wii Remote 1", SDL_SCANCODE_Z, SDL_SCANCODE_Z},
        {Key::Two, "Wii Remote 2", SDL_SCANCODE_X, SDL_SCANCODE_X},
        {Key::Plus, "Wii Remote Plus", SDL_SCANCODE_EQUALS, SDL_SCANCODE_EQUALS},
        {Key::Minus, "Wii Remote Minus", SDL_SCANCODE_MINUS, SDL_SCANCODE_MINUS},
        {Key::Home, "Wii Remote Home", SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_BACKSPACE},

        {Key::IRUp, "IR Pointer Up", SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_8},
        {Key::IRDown, "IR Pointer Down", SDL_SCANCODE_KP_2, SDL_SCANCODE_KP_2},
        {Key::IRLeft, "IR Pointer Left", SDL_SCANCODE_KP_4, SDL_SCANCODE_KP_4},
        {Key::IRRight, "IR Pointer Right", SDL_SCANCODE_KP_6, SDL_SCANCODE_KP_6},
        {Key::IRCenter, "IR Pointer Center", SDL_SCANCODE_KP_5, SDL_SCANCODE_KP_5},
        {Key::IRZoomOut, "IR Pointer Zoom Out", SDL_SCANCODE_KP_7, SDL_SCANCODE_KP_7},
        {Key::IRZoomIn, "IR Pointer Zoom In", SDL_SCANCODE_KP_9, SDL_SCANCODE_KP_9},

        {Key::NunchukUp, "Nunchuk Stick Up", SDL_SCANCODE_W, SDL_SCANCODE_W},
        {Key::NunchukDown, "Nunchuk Stick Down", SDL_SCANCODE_S, SDL_SCANCODE_S},
        {Key::NunchukLeft, "Nunchuk Stick Left", SDL_SCANCODE_A, SDL_SCANCODE_A},
        {Key::NunchukRight, "Nunchuk Stick Right", SDL_SCANCODE_D, SDL_SCANCODE_D},
        {Key::NunchukC, "Nunchuk C", SDL_SCANCODE_Q, SDL_SCANCODE_Q},
        {Key::NunchukZ, "Nunchuk Z", SDL_SCANCODE_E, SDL_SCANCODE_E},

        {Key::ClassicUp, "Classic Left Stick Up", SDL_SCANCODE_I, SDL_SCANCODE_I},
        {Key::ClassicDown, "Classic Left Stick Down", SDL_SCANCODE_K, SDL_SCANCODE_K},
        {Key::ClassicLeft, "Classic Left Stick Left", SDL_SCANCODE_J, SDL_SCANCODE_J},
        {Key::ClassicRight, "Classic Left Stick Right", SDL_SCANCODE_L, SDL_SCANCODE_L},
        {Key::ClassicA, "Classic A", SDL_SCANCODE_U, SDL_SCANCODE_U},
        {Key::ClassicB, "Classic B", SDL_SCANCODE_O, SDL_SCANCODE_O},
        {Key::ClassicX, "Classic X", SDL_SCANCODE_P, SDL_SCANCODE_P},
        {Key::ClassicY, "Classic Y", SDL_SCANCODE_LEFTBRACKET, SDL_SCANCODE_LEFTBRACKET},
        {Key::ClassicL, "Classic L", SDL_SCANCODE_N, SDL_SCANCODE_N},
        {Key::ClassicR, "Classic R", SDL_SCANCODE_M, SDL_SCANCODE_M},
        {Key::ClassicZL, "Classic ZL", SDL_SCANCODE_COMMA, SDL_SCANCODE_COMMA},
        {Key::ClassicZR, "Classic ZR", SDL_SCANCODE_PERIOD, SDL_SCANCODE_PERIOD},
        {Key::ClassicPlus, "Classic Plus", SDL_SCANCODE_0, SDL_SCANCODE_0},
        {Key::ClassicMinus, "Classic Minus", SDL_SCANCODE_9, SDL_SCANCODE_9},
        {Key::ClassicHome, "Classic Home", SDL_SCANCODE_6, SDL_SCANCODE_6},

        {Key::GuitarGreen, "Guitar Green Fret", SDL_SCANCODE_1, SDL_SCANCODE_1},
        {Key::GuitarRed, "Guitar Red Fret", SDL_SCANCODE_2, SDL_SCANCODE_2},
        {Key::GuitarYellow, "Guitar Yellow Fret", SDL_SCANCODE_3, SDL_SCANCODE_3},
        {Key::GuitarBlue, "Guitar Blue Fret", SDL_SCANCODE_4, SDL_SCANCODE_4},
        {Key::GuitarOrange, "Guitar Orange Fret", SDL_SCANCODE_5, SDL_SCANCODE_5},
        {Key::GuitarStrumUp, "Guitar Strum Up", SDL_SCANCODE_G, SDL_SCANCODE_G},
        {Key::GuitarStrumDown, "Guitar Strum Down", SDL_SCANCODE_H, SDL_SCANCODE_H},
        {Key::GuitarWhammyDown, "Guitar Whammy Down", SDL_SCANCODE_T, SDL_SCANCODE_T},
        {Key::GuitarWhammyUp, "Guitar Whammy Up", SDL_SCANCODE_Y, SDL_SCANCODE_Y},
        {Key::GuitarMinus, "Guitar Minus", SDL_SCANCODE_9, SDL_SCANCODE_9},
        {Key::GuitarPlus, "Guitar Plus", SDL_SCANCODE_0, SDL_SCANCODE_0},

        {Key::DrumRed, "Drums Red", SDL_SCANCODE_6, SDL_SCANCODE_6},
        {Key::DrumYellow, "Drums Yellow", SDL_SCANCODE_7, SDL_SCANCODE_7},
        {Key::DrumBlue, "Drums Blue", SDL_SCANCODE_8, SDL_SCANCODE_8},
        {Key::DrumGreen, "Drums Green", SDL_SCANCODE_9, SDL_SCANCODE_9},
        {Key::DrumOrange, "Drums Orange", SDL_SCANCODE_0, SDL_SCANCODE_0},
        {Key::DrumKick, "Drums Kick", SDL_SCANCODE_N, SDL_SCANCODE_N},
        {Key::DrumPlus, "Drums Plus", SDL_SCANCODE_EQUALS, SDL_SCANCODE_EQUALS},
        {Key::DrumMinus, "Drums Minus", SDL_SCANCODE_MINUS, SDL_SCANCODE_MINUS},

        {Key::TurntableGreen, "Turntable Green", SDL_SCANCODE_F, SDL_SCANCODE_F},
        {Key::TurntableRed, "Turntable Red", SDL_SCANCODE_G, SDL_SCANCODE_G},
        {Key::TurntableBlue, "Turntable Blue", SDL_SCANCODE_H, SDL_SCANCODE_H},
        {Key::TurntableDeckLeft, "Turntable Deck Left", SDL_SCANCODE_J, SDL_SCANCODE_J},
        {Key::TurntableDeckRight, "Turntable Deck Right", SDL_SCANCODE_L, SDL_SCANCODE_L},
        {Key::TurntableCrossfadeLeft, "Turntable Crossfade Left", SDL_SCANCODE_COMMA, SDL_SCANCODE_COMMA},
        {Key::TurntableCrossfadeRight, "Turntable Crossfade Right", SDL_SCANCODE_PERIOD, SDL_SCANCODE_PERIOD},

        {Key::UDrawPen, "UDraw Pen", SDL_SCANCODE_APOSTROPHE, SDL_SCANCODE_APOSTROPHE},
        {Key::UDrawLeft, "UDraw Left", SDL_SCANCODE_COMMA, SDL_SCANCODE_COMMA},
        {Key::UDrawRight, "UDraw Right", SDL_SCANCODE_PERIOD, SDL_SCANCODE_PERIOD},
        {Key::UDrawUp, "UDraw Up", SDL_SCANCODE_SEMICOLON, SDL_SCANCODE_SEMICOLON},
        {Key::UDrawDown, "UDraw Down", SDL_SCANCODE_SLASH, SDL_SCANCODE_SLASH},
        {Key::UDrawA, "UDraw A", SDL_SCANCODE_RIGHTBRACKET, SDL_SCANCODE_RIGHTBRACKET},
        {Key::UDrawB, "UDraw B", SDL_SCANCODE_LEFTBRACKET, SDL_SCANCODE_LEFTBRACKET},

        {Key::DrawsomePen, "Drawsome Pen", SDL_SCANCODE_APOSTROPHE, SDL_SCANCODE_APOSTROPHE},
        {Key::DrawsomeLeft, "Drawsome Left", SDL_SCANCODE_COMMA, SDL_SCANCODE_COMMA},
        {Key::DrawsomeRight, "Drawsome Right", SDL_SCANCODE_PERIOD, SDL_SCANCODE_PERIOD},
        {Key::DrawsomeUp, "Drawsome Up", SDL_SCANCODE_SEMICOLON, SDL_SCANCODE_SEMICOLON},
        {Key::DrawsomeDown, "Drawsome Down", SDL_SCANCODE_SLASH, SDL_SCANCODE_SLASH},
        {Key::DrawsomeA, "Drawsome A", SDL_SCANCODE_RIGHTBRACKET, SDL_SCANCODE_RIGHTBRACKET},
        {Key::DrawsomeB, "Drawsome B", SDL_SCANCODE_LEFTBRACKET, SDL_SCANCODE_LEFTBRACKET},

        {Key::TaTaConHit, "TaTaCon Hit", SDL_SCANCODE_V, SDL_SCANCODE_V},
        {Key::TaTaConRim, "TaTaCon Rim", SDL_SCANCODE_B, SDL_SCANCODE_B},

        {Key::ShinkansenThrottleUp, "Shinkansen Throttle Up", SDL_SCANCODE_R, SDL_SCANCODE_R},
        {Key::ShinkansenThrottleDown, "Shinkansen Throttle Down", SDL_SCANCODE_F, SDL_SCANCODE_F},
        {Key::ShinkansenBrake, "Shinkansen Brake", SDL_SCANCODE_G, SDL_SCANCODE_G},
        {Key::ShinkansenHorn, "Shinkansen Horn", SDL_SCANCODE_H, SDL_SCANCODE_H},

        {Key::MotionPitchUp, "Motion Pitch Up", SDL_SCANCODE_I, SDL_SCANCODE_I},
        {Key::MotionPitchDown, "Motion Pitch Down", SDL_SCANCODE_K, SDL_SCANCODE_K},
        {Key::MotionYawLeft, "Motion Yaw Left", SDL_SCANCODE_J, SDL_SCANCODE_J},
        {Key::MotionYawRight, "Motion Yaw Right", SDL_SCANCODE_L, SDL_SCANCODE_L},
        {Key::MotionRollLeft, "Motion Roll Left", SDL_SCANCODE_U, SDL_SCANCODE_U},
        {Key::MotionRollRight, "Motion Roll Right", SDL_SCANCODE_O, SDL_SCANCODE_O},
        {Key::AccelXNegative, "Motion Acceleration X-", SDL_SCANCODE_T, SDL_SCANCODE_T},
        {Key::AccelXPositive, "Motion Acceleration X+", SDL_SCANCODE_G, SDL_SCANCODE_G},
        {Key::AccelYPositive, "Motion Acceleration Y+", SDL_SCANCODE_R, SDL_SCANCODE_R},
        {Key::AccelYNegative, "Motion Acceleration Y-", SDL_SCANCODE_F, SDL_SCANCODE_F},
        {Key::AccelZPositive, "Motion Acceleration Z+", SDL_SCANCODE_Y, SDL_SCANCODE_Y},
        {Key::AccelZNegative, "Motion Acceleration Z-", SDL_SCANCODE_H, SDL_SCANCODE_H},
        {Key::Shake, "Motion Shake", SDL_SCANCODE_Q, SDL_SCANCODE_Q},

        {Key::ExtensionNone, "Extension: None", SDL_SCANCODE_F1, SDL_SCANCODE_F1},
        {Key::ExtensionNunchuk, "Extension: Nunchuk", SDL_SCANCODE_F2, SDL_SCANCODE_F2},
        {Key::ExtensionClassic, "Extension: Classic", SDL_SCANCODE_F3, SDL_SCANCODE_F3},
        {Key::ExtensionGuitar, "Extension: Guitar", SDL_SCANCODE_F4, SDL_SCANCODE_F4},
        {Key::ExtensionDrums, "Extension: Drums", SDL_SCANCODE_F5, SDL_SCANCODE_F5},
        {Key::ExtensionTurntable, "Extension: Turntable", SDL_SCANCODE_F6, SDL_SCANCODE_F6},
        {Key::ExtensionUDraw, "Extension: UDraw", SDL_SCANCODE_F7, SDL_SCANCODE_F7},
        {Key::ExtensionDrawsome, "Extension: Drawsome", SDL_SCANCODE_F8, SDL_SCANCODE_F8},
        {Key::ExtensionTaTaCon, "Extension: TaTaCon", SDL_SCANCODE_F9, SDL_SCANCODE_F9},
        {Key::ExtensionShinkansen, "Extension: Shinkansen", SDL_SCANCODE_F11, SDL_SCANCODE_F11},
        {Key::ToggleMotionPlus, "MotionPlus Toggle", SDL_SCANCODE_F10, SDL_SCANCODE_F10},
    };
}

} // namespace

KeyboardBindings::KeyboardBindings()
    : bindings_(DefaultBindings()) {
}

void KeyboardBindings::Load() {
    const char* pref = SDL_GetPrefPath("NVDEMU", "vWii");
    if (!pref)
        return;

    config_path_ = std::string(pref) + "keyboard.cfg";
    SDL_free(const_cast<char*>(pref));

    std::ifstream file(config_path_);
    if (!file)
        return;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream stream(line);
        std::size_t index = 0;
        int scancode = static_cast<int>(SDL_SCANCODE_UNKNOWN);
        if (!(stream >> index >> scancode))
            continue;
        if (index >= bindings_.size())
            continue;
        if (scancode < 0 || scancode >= SDL_SCANCODE_COUNT)
            bindings_[index].scancode = SDL_SCANCODE_UNKNOWN;
        else
            bindings_[index].scancode = static_cast<SDL_Scancode>(scancode);
    }
}

void KeyboardBindings::Save() const {
    if (config_path_.empty())
        return;

    std::ofstream file(config_path_, std::ios::trunc);
    if (!file)
        return;

    file << "# vWii keyboard bindings\n";
    for (std::size_t index = 0; index < bindings_.size(); ++index)
        file << index << ' ' << static_cast<int>(bindings_[index].scancode) << '\n';
}

void KeyboardBindings::ResetAll() {
    for (auto& binding : bindings_)
        binding.scancode = binding.default_scancode;
}

void KeyboardBindings::Reset(std::size_t index) {
    if (index < bindings_.size())
        bindings_[index].scancode = bindings_[index].default_scancode;
}

void KeyboardBindings::Set(std::size_t index, SDL_Scancode scancode) {
    if (index < bindings_.size())
        bindings_[index].scancode = scancode;
}

bool KeyboardBindings::Has(SDL_Scancode scancode) const {
    if (scancode == SDL_SCANCODE_UNKNOWN)
        return false;

    for (const auto& binding : bindings_) {
        if (binding.scancode == scancode)
            return true;
    }
    return false;
}

void KeyboardBindings::Send(input::WiiRemoteKeyboard* wiimote,
                            SDL_Scancode scancode,
                            bool pressed) const {
    if (!wiimote || scancode == SDL_SCANCODE_UNKNOWN)
        return;

    for (const auto& binding : bindings_) {
        if (binding.scancode == scancode)
            wiimote->KeyEvent(binding.key, pressed);
    }
}

void KeyboardBindings::ReleaseAll(input::WiiRemoteKeyboard* wiimote) const {
    if (!wiimote)
        return;

    for (const auto& binding : bindings_)
        wiimote->KeyEvent(binding.key, false);
}

} // namespace vwii::frontend
