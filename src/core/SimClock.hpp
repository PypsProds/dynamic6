#pragma once
#include <algorithm>
#include <cmath>
#include "Types.hpp"

namespace dts {

// Maps a wall-clock input timestamp onto the SIMULATION tick that will first see it.
// It deliberately knows nothing about rendered frames: the caller feeds it
// (a) the last completed sim step + wall time, (b) the sim rate in ticks/second.
// Visual FPS therefore cannot influence the result.
//
// tickAt() = lastTick + 1 + min(floor((wall - lastStepWall) * tps), maxLeadTicks)
class SimClock {
public:
    void setTicksPerSecond(double tps) { if (tps > 0.0) tps_ = tps; }  // incl. any time-warp
    void setMaxLeadTicks(int n) { maxLead_ = std::max(0, n); }

    void onStep(SimTick completedTick, double wallSec) {
        lastTick_ = completedTick; lastWall_ = wallSec; valid_ = true;
    }
    // Call when resuming from pause so paused wall time isn't converted into ticks.
    void onResume(double wallSec) { lastWall_ = wallSec; }
    void reset() { valid_ = false; lastTick_ = 0; lastWall_ = 0; }

    bool valid() const { return valid_; }
    SimTick lastTick() const { return lastTick_; }

    SimTick tickAt(double wallSec) const {
        if (!valid_) return 0;
        double dt = std::max(0.0, wallSec - lastWall_);
        auto lead = static_cast<int64_t>(std::floor(dt * tps_));
        lead = std::min<int64_t>(lead, maxLead_);
        return lastTick_ + 1 + lead;
    }

private:
    double tps_ = 240.0;  // PLACEHOLDER default; the glue must supply the real rate
    int maxLead_ = 1;
    SimTick lastTick_ = 0;
    double lastWall_ = 0.0;
    bool valid_ = false;
};

}  // namespace dts
