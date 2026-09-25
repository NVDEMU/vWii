#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace vwii::input {

enum class Extension : uint8_t {
    None,
    Nunchuk,
    Classic,
    Guitar,
    Drums,
    Turntable,
    UDrawTablet,
    DrawsomeTablet,
    TaTaCon,
    Shinkansen,
};

enum class Key : uint8_t {
    DpadUp, DpadDown, DpadLeft, DpadRight,
    A, B, One, Two, Plus, Minus, Home,
    Shake,

    IRUp, IRDown, IRLeft, IRRight, IRCenter,
    IRZoomIn, IRZoomOut,

    NunchukUp, NunchukDown, NunchukLeft, NunchukRight,
    NunchukC, NunchukZ,

    ClassicUp, ClassicDown, ClassicLeft, ClassicRight,
    ClassicA, ClassicB, ClassicX, ClassicY,
    ClassicL, ClassicR, ClassicZL, ClassicZR,
    ClassicPlus, ClassicMinus, ClassicHome,

    GuitarGreen, GuitarRed, GuitarYellow, GuitarBlue, GuitarOrange,
    GuitarStrumUp, GuitarStrumDown,
    GuitarWhammyUp, GuitarWhammyDown,
    GuitarMinus, GuitarPlus,

    DrumRed, DrumYellow, DrumBlue, DrumGreen, DrumOrange,
    DrumKick, DrumPlus, DrumMinus,

    TurntableGreen, TurntableRed, TurntableBlue,
    TurntableDeckLeft, TurntableDeckRight,
    TurntableCrossfadeLeft, TurntableCrossfadeRight,

    UDrawPen, UDrawLeft, UDrawRight, UDrawUp, UDrawDown,
    UDrawA, UDrawB,

    DrawsomePen, DrawsomeLeft, DrawsomeRight, DrawsomeUp, DrawsomeDown,
    DrawsomeA, DrawsomeB,

    TaTaConHit, TaTaConRim,
    ShinkansenThrottleUp, ShinkansenThrottleDown,
    ShinkansenBrake, ShinkansenHorn,

    MotionPitchUp, MotionPitchDown,
    MotionYawLeft, MotionYawRight,
    MotionRollLeft, MotionRollRight,
    AccelXPositive, AccelXNegative,
    AccelYPositive, AccelYNegative,
    AccelZPositive, AccelZNegative,

    ExtensionNone,
    ExtensionNunchuk,
    ExtensionClassic,
    ExtensionGuitar,
    ExtensionDrums,
    ExtensionTurntable,
    ExtensionUDraw,
    ExtensionDrawsome,
    ExtensionTaTaCon,
    ExtensionShinkansen,
    ToggleMotionPlus,
};

struct WiiRemoteState {
    uint16_t buttons{};
    float ir_x{0.5f};
    float ir_y{0.5f};

    float accel_x{};
    float accel_y{};
    float accel_z{1.0f};

    float gyro_pitch{};
    float gyro_yaw{};
    float gyro_roll{};

    float nunchuk_x{};
    float nunchuk_y{};
    bool nunchuk_c{};
    bool nunchuk_z{};

    float classic_lx{};
    float classic_ly{};
    float classic_rx{};
    float classic_ry{};
    uint16_t classic_buttons{};

    std::array<bool, 5> guitar_frets{};
    int guitar_strum{};
    float guitar_whammy{};

    std::array<bool, 5> drum_pads{};
    bool drum_kick{};

    bool turntable_green{};
    bool turntable_red{};
    bool turntable_blue{};
    float turntable_deck{};
    float turntable_crossfade{};

    float udraw_x{};
    float udraw_y{};
    bool udraw_pen{};
    bool udraw_a{};
    bool udraw_b{};

    float drawsome_x{};
    float drawsome_y{};
    bool drawsome_pen{};
    bool drawsome_a{};
    bool drawsome_b{};

    bool tatacon_hit{};
    bool tatacon_rim{};

    float shinkansen_throttle{};
    float shinkansen_brake{};
    bool shinkansen_horn{};

    Extension extension{Extension::None};
    bool motion_plus{};
};

class WiiRemoteKeyboard {
public:
    void Reset();

    void KeyEvent(Key key, bool pressed);
    void Update(float delta_seconds);

    void CycleExtension();
    void SetExtension(Extension extension);
    void ToggleMotionPlus();

    [[nodiscard]] const WiiRemoteState& State() const { return state_; }

    // Wii Remote report 0x37: core + accel + basic IR + 6 bytes extension.
    [[nodiscard]] std::array<uint8_t, 22> BuildReport() const;

private:
    void ApplyDigitalToAxes();
    void ApplyMotion(float delta_seconds);
    void SetExtensionHotkey(Key key);

    WiiRemoteState state_{};

    struct KeyFlags {
        bool motion_pitch_up{};
        bool motion_pitch_down{};
        bool motion_yaw_left{};
        bool motion_yaw_right{};
        bool motion_roll_left{};
        bool motion_roll_right{};
        bool accel_x_positive{};
        bool accel_x_negative{};
        bool accel_y_positive{};
        bool accel_y_negative{};
        bool accel_z_positive{};
        bool accel_z_negative{};
        bool shake{};
        bool ir_up{};
        bool ir_down{};
        bool ir_left{};
        bool ir_right{};
        bool ir_center{};
        bool ir_zoom_in{};
        bool ir_zoom_out{};
    } keys_{};
};

} // namespace vwii::input
