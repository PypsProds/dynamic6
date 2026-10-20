#pragma once
#include "Config.hpp"
#include "Types.hpp"

namespace dts {

enum class Decision { Apply, Defer, Expire, DropOrphanRelease, DropInvalid };

inline const char* toString(Decision d) {
    switch (d) {
        case Decision::Apply: return "apply";
        case Decision::Defer: return "defer";
        case Decision::Expire: return "expire";
        case Decision::DropOrphanRelease: return "drop-orphan-release";
        case Decision::DropInvalid: return "drop-invalid";
    }
    return "?";
}

struct EvalResult {
    Decision decision;
    const char* reason;
    int64_t age;  // now - event.tick, in sim ticks
};

// Pure function of (config, event, now, world, press-applied flag). No state, no I/O.
//
// NOTE: canAct() is a *research model* of "would a click do something right now?".
// It is NOT a reimplementation of GD's rules. Replace/extend it as you verify
// real behavior (orb/pad contact flags, robot jump hold, etc.).
class TimingEvaluator {
public:
    explicit TimingEvaluator(const TimingConfig& c) : cfg_(c) {}
    void setConfig(const TimingConfig& c) { cfg_ = c; }

    static bool canAct(const PlayerSnapshot& p, Button b) {
        if (!p.alive) return false;
        if (b != Button::Jump) return true;  // platformer movement: never buffered
        switch (p.mode) {
            case GameMode::Ship:
            case GameMode::Ufo:
            case GameMode::Wave:
            case GameMode::Swing:
                return true;  // hold/tap modes that don't need a surface or orb
            case GameMode::Cube:
            case GameMode::Ball:
            case GameMode::Robot:
            case GameMode::Spider:
                return p.onGround || p.orbReady;
        }
        return false;
    }

    bool targetsCanAct(const InputEvent& e, const WorldState& w) const {
        if (w.dual && !w.twoPlayer) {  // one input drives both players
            bool a = canAct(w.p[0], e.button), b = canAct(w.p[1], e.button);
            bool aAlive = w.p[0].alive, bAlive = w.p[1].alive;
            if (cfg_.dualPolicy == DualPolicy::AnyCanAct) return a || b;
            // AllCanAct: every living player must be able to act (and at least one alive)
            return (!aAlive || a) && (!bAlive || b) && (aAlive || bAlive);
        }
        return canAct(w.p[e.lane < kLaneCount ? e.lane : 0], e.button);
    }

    EvalResult evaluate(const InputEvent& e, SimTick now, const WorldState& w,
                        bool pressApplied) const {
        if (e.tick > now) return {Decision::DropInvalid, "timestamp-in-future", 0};
        int64_t age = now - e.tick;
        if (!e.down) {
            return pressApplied ? EvalResult{Decision::Apply, "release-of-applied-press", age}
                                : EvalResult{Decision::DropOrphanRelease, "press-not-applied", age};
        }
        if (targetsCanAct(e, w)) return {Decision::Apply, "can-act", age};
        if (age >= cfg_.windowTicks) return {Decision::Expire, "window-elapsed", age};
        return {Decision::Defer, "waiting-for-state", age};
    }

private:
    TimingConfig cfg_;
};

}  // namespace dts
