#pragma once
// Core value types. No Geode / GD includes allowed in src/core.
#include <cstdint>

namespace dts {

using SimTick = int64_t;  // index of a physics/simulation step, NOT a rendered frame

enum class Button : uint8_t { Jump = 0, Left = 1, Right = 2 };
constexpr int kButtonCount = 3;
constexpr int kLaneCount = 2;  // lane 0 = player 1 input, lane 1 = player 2 input

enum class GameMode : uint8_t { Cube, Ship, Ball, Ufo, Wave, Robot, Spider, Swing };

struct InputEvent {
    uint64_t id = 0;        // monotonic, assigned by capture; must increase
    SimTick tick = 0;       // sim step that first could observe this input
    double wallSec = 0.0;   // wall-clock time of capture (diagnostics only)
    Button button = Button::Jump;
    bool down = true;       // true = press, false = release
    uint8_t lane = 0;       // 0 or 1
};

struct PlayerSnapshot {
    GameMode mode = GameMode::Cube;
    bool onGround = false;
    bool orbReady = false;  // an orb/pad-like interaction is currently available (model input)
    bool alive = true;
    double y = 0.0;
    double yVel = 0.0;
};

struct WorldState {
    PlayerSnapshot p[2];
    bool dual = false;       // dual mode active
    bool twoPlayer = false;  // separate inputs per player (2-player option)
    bool paused = false;
};

}  // namespace dts
