#pragma once
// OPTIONAL experiment. Predictions and observations are separate types on purpose:
// a prediction is a model output, an observation is what the game actually did.
// Nothing here feeds back into input scheduling.
#include <cmath>
#include <cstdint>
#include <string>
#include "Types.hpp"

namespace dts {

struct MovementParams {
    // Units are in "60 Hz reference frames". The default numbers are values commonly
    // cited by the GD community for a cube at normal gravity. They are NOT verified
    // against the target GD version by this project. Treat `validated=false` as a
    // hard flag in any analysis output.
    double gravity = 0.958199;
    double jumpVel = 11.180032;
    double ticksPerSecond = 240.0;
    double groundY = 0.0;
    bool validated = false;
    std::string source = "community-documented values, UNVERIFIED";
};

struct Prediction {   // model output
    SimTick launchTick = 0;
    SimTick landTick = 0;     // first tick at/below ground after apex
    double apexY = 0.0;
    int airTicks = 0;
    bool validatedParams = false;
};

struct Observation {  // measured from the real game, filled by the glue
    SimTick landTick = 0;
    double landY = 0.0;
    bool collided = false;  // real collision result (death / hit)
};

struct Comparison {
    int64_t landTickError;  // observed - predicted (ticks). Positive = game landed later.
    bool observedCollision;
    bool paramsValidated;
};

// Flat ground, no object collisions, semi-implicit Euler per sim tick:
//   vy -= g*dt ; y += vy*dt   with dt = 60 / ticksPerSecond (reference-frame units)
// Replace with a validated model before drawing any conclusions.
inline Prediction predictCubeJump(const MovementParams& m, SimTick launchTick,
                                  double startY = 0.0, int maxTicks = 2000) {
    Prediction p;
    p.launchTick = launchTick;
    p.validatedParams = m.validated;
    double dt = 60.0 / m.ticksPerSecond;
    double y = startY, vy = m.jumpVel;
    p.apexY = y;
    for (int i = 1; i <= maxTicks; ++i) {
        vy -= m.gravity * dt;
        y += vy * dt;
        if (y > p.apexY) p.apexY = y;
        if (vy < 0 && y <= m.groundY) {
            p.airTicks = i;
            p.landTick = launchTick + i;
            return p;
        }
    }
    p.airTicks = maxTicks;
    p.landTick = launchTick + maxTicks;
    return p;
}

inline Comparison compare(const Prediction& p, const Observation& o) {
    return {o.landTick - p.landTick, o.collided, p.validatedParams};
}

}  // namespace dts
