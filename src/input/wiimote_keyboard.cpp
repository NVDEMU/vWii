#include "input/wiimote_keyboard.h"

#include <algorithm>
#include <cmath>

namespace vwii::input {

namespace {

constexpr uint16_t PAD_LEFT = 0x01;
constexpr uint16_t PAD_RIGHT = 0x02;
constexpr uint16_t PAD_DOWN = 0x04;
constexpr uint16_t PAD_UP = 0x08;
constexpr uint16_t BUTTON_PLUS = 0x10;
constexpr uint16_t BUTTON_TWO = 0x0100;
constexpr uint16_t BUTTON_ONE = 0x0200;
constexpr uint16_t BUTTON_B = 0x0400;
constexpr uint16_t BUTTON_A = 0x0800;
constexpr uint16_t BUTTON_MINUS = 0x1000;
constexpr uint16_t BUTTON_HOME = 0x8000;

constexpr uint8_t ClampByte(float value) {
    return static_cast<uint8_t>(
        std::clamp(value, 0.0f, 255.0f));
}

uint8_t AxisToUnsigned(float value) {
    return ClampByte((value + 1.0f) * 127.5f);
}

void Put16BE(std::array<uint8_t, 22>& report,
             std::size_t index, uint16_t value) {
    report[index] = static_cast<uint8_t>(value >> 8);
    report[index + 1] = static_cast<uint8_t>(value);
}

} // namespace

void WiiRemoteKeyboard::Reset() {
    state_ = {};
    state_.ir_x = 0.5f;
    state_.ir_y = 0.5f;
    state_.accel_z = 1.0f;
    state_.extension = Extension::None;
    state_.motion_plus = false;
    keys_ = {};
}

void WiiRemoteKeyboard::SetExtension(Extension extension) {
    state_.extension = extension;

    state_.nunchuk_x = 0;
    state_.nunchuk_y = 0;
    state_.classic_lx = 0;
    state_.classic_ly = 0;
    state_.classic_rx = 0;
    state_.classic_ry = 0;
    state_.guitar_strum = 0;
    state_.guitar_whammy = 0;
    state_.turntable_deck = 0;
    state_.turntable_crossfade = 0;
    state_.shinkansen_throttle = 0;
    state_.shinkansen_brake = 0;
}

void WiiRemoteKeyboard::CycleExtension() {
    const auto next = static_cast<unsigned>(
        state_.extension) + 1;

    const unsigned count =
        static_cast<unsigned>(Extension::Shinkansen) + 1;

    SetExtension(static_cast<Extension>(next % count));
}

void WiiRemoteKeyboard::ToggleMotionPlus() {
    state_.motion_plus = !state_.motion_plus;
}

void WiiRemoteKeyboard::SetExtensionHotkey(Key key) {
    switch (key) {
    case Key::ExtensionNone: SetExtension(Extension::None); break;
    case Key::ExtensionNunchuk: SetExtension(Extension::Nunchuk); break;
    case Key::ExtensionClassic: SetExtension(Extension::Classic); break;
    case Key::ExtensionGuitar: SetExtension(Extension::Guitar); break;
    case Key::ExtensionDrums: SetExtension(Extension::Drums); break;
    case Key::ExtensionTurntable: SetExtension(Extension::Turntable); break;
    case Key::ExtensionUDraw: SetExtension(Extension::UDrawTablet); break;
    case Key::ExtensionDrawsome: SetExtension(Extension::DrawsomeTablet); break;
    case Key::ExtensionTaTaCon: SetExtension(Extension::TaTaCon); break;
    case Key::ExtensionShinkansen: SetExtension(Extension::Shinkansen); break;
    default: break;
    }
}

void WiiRemoteKeyboard::KeyEvent(Key key, bool pressed) {
    if (!pressed) {
        switch (key) {
        case Key::DpadUp: state_.buttons &= ~PAD_UP; break;
        case Key::DpadDown: state_.buttons &= ~PAD_DOWN; break;
        case Key::DpadLeft: state_.buttons &= ~PAD_LEFT; break;
        case Key::DpadRight: state_.buttons &= ~PAD_RIGHT; break;
        case Key::A: state_.buttons &= ~BUTTON_A; break;
        case Key::B: state_.buttons &= ~BUTTON_B; break;
        case Key::One: state_.buttons &= ~BUTTON_ONE; break;
        case Key::Two: state_.buttons &= ~BUTTON_TWO; break;
        case Key::Plus: state_.buttons &= ~BUTTON_PLUS; break;
        case Key::Minus: state_.buttons &= ~BUTTON_MINUS; break;
        case Key::Home: state_.buttons &= ~BUTTON_HOME; break;
        case Key::IRZoomIn: keys_.ir_zoom_in = false; break;
        case Key::IRZoomOut: keys_.ir_zoom_out = false; break;

        case Key::MotionPitchUp: keys_.motion_pitch_up = false; break;
        case Key::MotionPitchDown: keys_.motion_pitch_down = false; break;
        case Key::MotionYawLeft: keys_.motion_yaw_left = false; break;
        case Key::MotionYawRight: keys_.motion_yaw_right = false; break;
        case Key::MotionRollLeft: keys_.motion_roll_left = false; break;
        case Key::MotionRollRight: keys_.motion_roll_right = false; break;
        case Key::AccelXPositive: keys_.accel_x_positive = false; break;
        case Key::AccelXNegative: keys_.accel_x_negative = false; break;
        case Key::AccelYPositive: keys_.accel_y_positive = false; break;
        case Key::AccelYNegative: keys_.accel_y_negative = false; break;
        case Key::AccelZPositive: keys_.accel_z_positive = false; break;
        case Key::AccelZNegative: keys_.accel_z_negative = false; break;
        case Key::IRUp: keys_.ir_up = false; break;
        case Key::IRDown: keys_.ir_down = false; break;
        case Key::IRLeft: keys_.ir_left = false; break;
        case Key::IRRight: keys_.ir_right = false; break;

        case Key::NunchukC: state_.nunchuk_c = false; break;
        case Key::NunchukZ: state_.nunchuk_z = false; break;
        case Key::ClassicA: state_.classic_buttons &= ~(1u << 0); break;
        case Key::ClassicB: state_.classic_buttons &= ~(1u << 1); break;
        case Key::ClassicX: state_.classic_buttons &= ~(1u << 2); break;
        case Key::ClassicY: state_.classic_buttons &= ~(1u << 3); break;
        case Key::ClassicL: state_.classic_buttons &= ~(1u << 4); break;
        case Key::ClassicR: state_.classic_buttons &= ~(1u << 5); break;
        case Key::ClassicZL: state_.classic_buttons &= ~(1u << 6); break;
        case Key::ClassicZR: state_.classic_buttons &= ~(1u << 7); break;
        case Key::ClassicPlus: state_.classic_buttons &= ~(1u << 8); break;
        case Key::ClassicMinus: state_.classic_buttons &= ~(1u << 9); break;
        case Key::ClassicHome: state_.classic_buttons &= ~(1u << 10); break;

        case Key::GuitarGreen: state_.guitar_frets[0] = false; break;
        case Key::GuitarRed: state_.guitar_frets[1] = false; break;
        case Key::GuitarYellow: state_.guitar_frets[2] = false; break;
        case Key::GuitarBlue: state_.guitar_frets[3] = false; break;
        case Key::GuitarOrange: state_.guitar_frets[4] = false; break;
        case Key::GuitarStrumUp:
        case Key::GuitarStrumDown: state_.guitar_strum = 0; break;
        case Key::GuitarWhammyUp:
        case Key::GuitarWhammyDown: state_.guitar_whammy = 0; break;
        case Key::GuitarMinus: state_.buttons &= ~BUTTON_MINUS; break;
        case Key::GuitarPlus: state_.buttons &= ~BUTTON_PLUS; break;

        case Key::DrumRed: state_.drum_pads[0] = false; break;
        case Key::DrumYellow: state_.drum_pads[1] = false; break;
        case Key::DrumBlue: state_.drum_pads[2] = false; break;
        case Key::DrumGreen: state_.drum_pads[3] = false; break;
        case Key::DrumOrange: state_.drum_pads[4] = false; break;
        case Key::DrumKick: state_.drum_kick = false; break;
        case Key::DrumPlus: state_.buttons &= ~BUTTON_PLUS; break;
        case Key::DrumMinus: state_.buttons &= ~BUTTON_MINUS; break;

        case Key::TurntableGreen: state_.turntable_green = false; break;
        case Key::TurntableRed: state_.turntable_red = false; break;
        case Key::TurntableBlue: state_.turntable_blue = false; break;
        case Key::TurntableDeckLeft:
        case Key::TurntableDeckRight: state_.turntable_deck = 0; break;
        case Key::TurntableCrossfadeLeft:
        case Key::TurntableCrossfadeRight: state_.turntable_crossfade = 0; break;

        case Key::UDrawPen: state_.udraw_pen = false; break;
        case Key::UDrawA: state_.udraw_a = false; break;
        case Key::UDrawB: state_.udraw_b = false; break;
        case Key::DrawsomePen: state_.drawsome_pen = false; break;
        case Key::DrawsomeA: state_.drawsome_a = false; break;
        case Key::DrawsomeB: state_.drawsome_b = false; break;

        case Key::TaTaConHit: state_.tatacon_hit = false; break;
        case Key::TaTaConRim: state_.tatacon_rim = false; break;
        case Key::ShinkansenThrottleUp:
        case Key::ShinkansenThrottleDown: state_.shinkansen_throttle = 0; break;
        case Key::ShinkansenBrake: state_.shinkansen_brake = 0; break;
        case Key::ShinkansenHorn: state_.shinkansen_horn = false; break;
        default: break;
        }
        return;
    }

    SetExtensionHotkey(key);

    switch (key) {
    case Key::DpadUp: state_.buttons |= PAD_UP; break;
    case Key::DpadDown: state_.buttons |= PAD_DOWN; break;
    case Key::DpadLeft: state_.buttons |= PAD_LEFT; break;
    case Key::DpadRight: state_.buttons |= PAD_RIGHT; break;
    case Key::A: state_.buttons |= BUTTON_A; break;
    case Key::B: state_.buttons |= BUTTON_B; break;
    case Key::One: state_.buttons |= BUTTON_ONE; break;
    case Key::Two: state_.buttons |= BUTTON_TWO; break;
    case Key::Plus: state_.buttons |= BUTTON_PLUS; break;
    case Key::Minus: state_.buttons |= BUTTON_MINUS; break;
    case Key::Home: state_.buttons |= BUTTON_HOME; break;

    case Key::IRCenter:
        state_.ir_x = 0.5f;
        state_.ir_y = 0.5f;
        break;
    case Key::IRZoomIn: keys_.ir_zoom_in = true; break;
    case Key::IRZoomOut: keys_.ir_zoom_out = true; break;

    case Key::NunchukUp: state_.nunchuk_y = 1; break;
    case Key::NunchukDown: state_.nunchuk_y = -1; break;
    case Key::NunchukLeft: state_.nunchuk_x = -1; break;
    case Key::NunchukRight: state_.nunchuk_x = 1; break;
    case Key::NunchukC: state_.nunchuk_c = true; break;
    case Key::NunchukZ: state_.nunchuk_z = true; break;

    case Key::ClassicUp: state_.classic_ly = 1; break;
    case Key::ClassicDown: state_.classic_ly = -1; break;
    case Key::ClassicLeft: state_.classic_lx = -1; break;
    case Key::ClassicRight: state_.classic_lx = 1; break;
    case Key::ClassicA: state_.classic_buttons |= 1u << 0; break;
    case Key::ClassicB: state_.classic_buttons |= 1u << 1; break;
    case Key::ClassicX: state_.classic_buttons |= 1u << 2; break;
    case Key::ClassicY: state_.classic_buttons |= 1u << 3; break;
    case Key::ClassicL: state_.classic_buttons |= 1u << 4; break;
    case Key::ClassicR: state_.classic_buttons |= 1u << 5; break;
    case Key::ClassicZL: state_.classic_buttons |= 1u << 6; break;
    case Key::ClassicZR: state_.classic_buttons |= 1u << 7; break;
    case Key::ClassicPlus: state_.classic_buttons |= 1u << 8; break;
    case Key::ClassicMinus: state_.classic_buttons |= 1u << 9; break;
    case Key::ClassicHome: state_.classic_buttons |= 1u << 10; break;

    case Key::GuitarGreen: state_.guitar_frets[0] = true; break;
    case Key::GuitarRed: state_.guitar_frets[1] = true; break;
    case Key::GuitarYellow: state_.guitar_frets[2] = true; break;
    case Key::GuitarBlue: state_.guitar_frets[3] = true; break;
    case Key::GuitarOrange: state_.guitar_frets[4] = true; break;
    case Key::GuitarStrumUp: state_.guitar_strum = 1; break;
    case Key::GuitarStrumDown: state_.guitar_strum = -1; break;
    case Key::GuitarWhammyUp: state_.guitar_whammy = 1; break;
    case Key::GuitarWhammyDown: state_.guitar_whammy = -1; break;
    case Key::GuitarMinus: state_.guitar_strum = 0; state_.buttons |= BUTTON_MINUS; break;
    case Key::GuitarPlus: state_.buttons |= BUTTON_PLUS; break;

    case Key::DrumRed: state_.drum_pads[0] = true; break;
    case Key::DrumYellow: state_.drum_pads[1] = true; break;
    case Key::DrumBlue: state_.drum_pads[2] = true; break;
    case Key::DrumGreen: state_.drum_pads[3] = true; break;
    case Key::DrumOrange: state_.drum_pads[4] = true; break;
    case Key::DrumKick: state_.drum_kick = true; break;
    case Key::DrumPlus: state_.buttons |= BUTTON_PLUS; break;
    case Key::DrumMinus: state_.buttons |= BUTTON_MINUS; break;

    case Key::TurntableGreen: state_.turntable_green = true; break;
    case Key::TurntableRed: state_.turntable_red = true; break;
    case Key::TurntableBlue: state_.turntable_blue = true; break;
    case Key::TurntableDeckLeft: state_.turntable_deck = -1; break;
    case Key::TurntableDeckRight: state_.turntable_deck = 1; break;
    case Key::TurntableCrossfadeLeft: state_.turntable_crossfade = -1; break;
    case Key::TurntableCrossfadeRight: state_.turntable_crossfade = 1; break;

    case Key::UDrawPen: state_.udraw_pen = true; break;
    case Key::UDrawLeft: state_.udraw_x = -1; break;
    case Key::UDrawRight: state_.udraw_x = 1; break;
    case Key::UDrawUp: state_.udraw_y = 1; break;
    case Key::UDrawDown: state_.udraw_y = -1; break;
    case Key::UDrawA: state_.udraw_a = true; break;
    case Key::UDrawB: state_.udraw_b = true; break;

    case Key::DrawsomePen: state_.drawsome_pen = true; break;
    case Key::DrawsomeLeft: state_.drawsome_x = -1; break;
    case Key::DrawsomeRight: state_.drawsome_x = 1; break;
    case Key::DrawsomeUp: state_.drawsome_y = 1; break;
    case Key::DrawsomeDown: state_.drawsome_y = -1; break;
    case Key::DrawsomeA: state_.drawsome_a = true; break;
    case Key::DrawsomeB: state_.drawsome_b = true; break;

    case Key::TaTaConHit: state_.tatacon_hit = true; break;
    case Key::TaTaConRim: state_.tatacon_rim = true; break;
    case Key::ShinkansenThrottleUp: state_.shinkansen_throttle = 1; break;
    case Key::ShinkansenThrottleDown: state_.shinkansen_throttle = -1; break;
    case Key::ShinkansenBrake: state_.shinkansen_brake = 1; break;
    case Key::ShinkansenHorn: state_.shinkansen_horn = true; break;

    case Key::MotionPitchUp: keys_.motion_pitch_up = true; break;
    case Key::MotionPitchDown: keys_.motion_pitch_down = true; break;
    case Key::MotionYawLeft: keys_.motion_yaw_left = true; break;
    case Key::MotionYawRight: keys_.motion_yaw_right = true; break;
    case Key::MotionRollLeft: keys_.motion_roll_left = true; break;
    case Key::MotionRollRight: keys_.motion_roll_right = true; break;
    case Key::AccelXPositive: keys_.accel_x_positive = true; break;
    case Key::AccelXNegative: keys_.accel_x_negative = true; break;
    case Key::AccelYPositive: keys_.accel_y_positive = true; break;
    case Key::AccelYNegative: keys_.accel_y_negative = true; break;
    case Key::AccelZPositive: keys_.accel_z_positive = true; break;
    case Key::AccelZNegative: keys_.accel_z_negative = true; break;
    case Key::Shake: keys_.shake = true; break;

    case Key::ToggleMotionPlus:
        ToggleMotionPlus();
        break;

    default:
        break;
    }

}

void WiiRemoteKeyboard::ApplyDigitalToAxes() {
    if (!keys_.ir_center) {
        if (keys_.ir_left) state_.ir_x -= 0.015f;
        if (keys_.ir_right) state_.ir_x += 0.015f;
        if (keys_.ir_up) state_.ir_y -= 0.015f;
        if (keys_.ir_down) state_.ir_y += 0.015f;
    }

    state_.ir_x = std::clamp(state_.ir_x, 0.0f, 1.0f);
    state_.ir_y = std::clamp(state_.ir_y, 0.0f, 1.0f);

    if (keys_.ir_zoom_in) {
        state_.ir_x += (state_.ir_x - 0.5f) * 0.02f;
        state_.ir_y += (state_.ir_y - 0.5f) * 0.02f;
    }
    if (keys_.ir_zoom_out) {
        state_.ir_x += (0.5f - state_.ir_x) * 0.02f;
        state_.ir_y += (0.5f - state_.ir_y) * 0.02f;
    }
}

void WiiRemoteKeyboard::ApplyMotion(float delta_seconds) {
    constexpr float AngularSpeed = 2.5f;
    constexpr float AccelSpeed = 2.0f;

    const auto axis = [](bool positive, bool negative) {
        return static_cast<float>(positive) -
               static_cast<float>(negative);
    };

    state_.gyro_pitch = axis(keys_.motion_pitch_up, keys_.motion_pitch_down) *
                        AngularSpeed;
    state_.gyro_yaw = axis(keys_.motion_yaw_right, keys_.motion_yaw_left) *
                      AngularSpeed;
    state_.gyro_roll = axis(keys_.motion_roll_right, keys_.motion_roll_left) *
                       AngularSpeed;

    state_.accel_x = axis(keys_.accel_x_positive, keys_.accel_x_negative) *
                     AccelSpeed;
    state_.accel_y = axis(keys_.accel_y_positive, keys_.accel_y_negative) *
                     AccelSpeed;
    state_.accel_z =
        1.0f +
        axis(keys_.accel_z_positive, keys_.accel_z_negative) *
        AccelSpeed;

    if (keys_.shake) {
        const float phase = std::sin(static_cast<float>(
            delta_seconds * 180.0));
        state_.accel_x += 3.0f * phase;
        state_.accel_y += 3.0f * std::cos(static_cast<float>(
            delta_seconds * 200.0));
        state_.accel_z += 3.0f;
    }
}

void WiiRemoteKeyboard::Update(float delta_seconds) {
    ApplyDigitalToAxes();
    ApplyMotion(delta_seconds);

    // Center analog sticks when their corresponding key is not held.
    const float stick_decay = std::clamp(delta_seconds * 16.0f, 0.0f, 1.0f);
    const auto decay = [stick_decay](float& value) {
        value += (0.0f - value) * stick_decay;
    };

    decay(state_.nunchuk_x);
    decay(state_.nunchuk_y);
    decay(state_.classic_lx);
    decay(state_.classic_ly);
    decay(state_.classic_rx);
    decay(state_.classic_ry);
    decay(state_.guitar_whammy);
    decay(state_.turntable_deck);
    decay(state_.turntable_crossfade);
    decay(state_.udraw_x);
    decay(state_.udraw_y);
    decay(state_.drawsome_x);
    decay(state_.drawsome_y);
    decay(state_.shinkansen_throttle);
    decay(state_.shinkansen_brake);
}

std::array<uint8_t, 22> WiiRemoteKeyboard::BuildReport() const {
    std::array<uint8_t, 22> report{};
    report[0] = 0x37;

    Put16BE(report, 1, state_.buttons);

    constexpr float ZeroG = 0x80;
    constexpr float OneG = 0x9A;
    const auto accel = [&](float value) {
        return ClampByte(ZeroG + value * (OneG - ZeroG));
    };

    report[3] = accel(state_.accel_x);
    report[4] = accel(state_.accel_y);
    report[5] = accel(state_.accel_z);

    const uint16_t ir_x = static_cast<uint16_t>(
        0x007F + state_.ir_x * (0x0380 - 0x007F));
    const uint16_t ir_y = static_cast<uint16_t>(
        0x005D + state_.ir_y * (0x02A2 - 0x005D));

    // Basic IR report occupies bytes 6..15.
    report[6] = static_cast<uint8_t>(ir_x & 0xFF);
    report[7] = static_cast<uint8_t>(ir_y & 0xFF);
    report[8] = static_cast<uint8_t>(
        ((ir_x >> 8) & 0x03) |
        (((ir_y >> 8) & 0x03) << 2));
    report[9] = report[6];
    report[10] = report[7];
    report[11] = report[8];
    report[12] = 0;
    report[13] = 0;
    report[14] = 0;
    report[15] = 0;

    // Extension payload occupies bytes 16..21.
    switch (state_.extension) {
    case Extension::Nunchuk:
        report[16] = AxisToUnsigned(state_.nunchuk_x);
        report[17] = AxisToUnsigned(state_.nunchuk_y);
        report[18] = accel(state_.accel_x);
        report[19] = accel(state_.accel_y);
        report[20] = accel(state_.accel_z);
        report[21] = static_cast<uint8_t>(
            (state_.nunchuk_c ? 0x02 : 0) |
            (state_.nunchuk_z ? 0x01 : 0));
        break;

    case Extension::Classic:
        report[16] = AxisToUnsigned(state_.classic_lx);
        report[17] = AxisToUnsigned(state_.classic_ly);
        report[18] = AxisToUnsigned(state_.classic_rx);
        report[19] = AxisToUnsigned(state_.classic_ry);
        report[20] = static_cast<uint8_t>(state_.classic_buttons >> 8);
        report[21] = static_cast<uint8_t>(state_.classic_buttons);
        break;

    case Extension::Guitar:
        report[16] = static_cast<uint8_t>(
            (state_.guitar_frets[0] ? 0x01 : 0) |
            (state_.guitar_frets[1] ? 0x02 : 0) |
            (state_.guitar_frets[2] ? 0x04 : 0) |
            (state_.guitar_frets[3] ? 0x08 : 0) |
            (state_.guitar_frets[4] ? 0x10 : 0));
        report[17] = static_cast<uint8_t>(state_.guitar_strum + 1);
        report[18] = AxisToUnsigned(state_.guitar_whammy);
        break;

    case Extension::Drums:
        report[16] = static_cast<uint8_t>(
            (state_.drum_pads[0] ? 0x01 : 0) |
            (state_.drum_pads[1] ? 0x02 : 0) |
            (state_.drum_pads[2] ? 0x04 : 0) |
            (state_.drum_pads[3] ? 0x08 : 0) |
            (state_.drum_pads[4] ? 0x10 : 0) |
            (state_.drum_kick ? 0x20 : 0));
        break;

    case Extension::Turntable:
        report[16] = static_cast<uint8_t>(
            (state_.turntable_green ? 0x01 : 0) |
            (state_.turntable_red ? 0x02 : 0) |
            (state_.turntable_blue ? 0x04 : 0));
        report[17] = AxisToUnsigned(state_.turntable_deck);
        report[18] = AxisToUnsigned(state_.turntable_crossfade);
        break;

    case Extension::UDrawTablet:
        report[16] = AxisToUnsigned(state_.udraw_x);
        report[17] = AxisToUnsigned(state_.udraw_y);
        report[18] = static_cast<uint8_t>(
            (state_.udraw_pen ? 0x01 : 0) |
            (state_.udraw_a ? 0x02 : 0) |
            (state_.udraw_b ? 0x04 : 0));
        break;

    case Extension::DrawsomeTablet:
        report[16] = AxisToUnsigned(state_.drawsome_x);
        report[17] = AxisToUnsigned(state_.drawsome_y);
        report[18] = static_cast<uint8_t>(
            (state_.drawsome_pen ? 0x01 : 0) |
            (state_.drawsome_a ? 0x02 : 0) |
            (state_.drawsome_b ? 0x04 : 0));
        break;

    case Extension::TaTaCon:
        report[16] = static_cast<uint8_t>(
            (state_.tatacon_hit ? 0x01 : 0) |
            (state_.tatacon_rim ? 0x02 : 0));
        break;

    case Extension::Shinkansen:
        report[16] = AxisToUnsigned(state_.shinkansen_throttle);
        report[17] = AxisToUnsigned(state_.shinkansen_brake);
        report[18] = static_cast<uint8_t>(state_.shinkansen_horn);
        break;

    case Extension::None:
        break;
    }

    if (state_.motion_plus) {
        const int16_t pitch =
            static_cast<int16_t>(state_.gyro_pitch * 819.0f);
        const int16_t yaw =
            static_cast<int16_t>(state_.gyro_yaw * 819.0f);
        const int16_t roll =
            static_cast<int16_t>(state_.gyro_roll * 819.0f);

        report[16] = static_cast<uint8_t>(roll);
        report[17] = static_cast<uint8_t>(roll >> 8);
        report[18] = static_cast<uint8_t>(pitch);
        report[19] = static_cast<uint8_t>(pitch >> 8);
        report[20] = static_cast<uint8_t>(yaw);
        report[21] = static_cast<uint8_t>(yaw >> 8);
    }

    return report;
}

} // namespace vwii::input
