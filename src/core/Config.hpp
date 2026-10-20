#pragma once
#include <algorithm>
#include <cstddef>

namespace dts {

enum class DualPolicy { AnyCanAct, AllCanAct };

struct TimingConfig {
    static constexpr int kMaxWindowTicks = 60;
    static constexpr size_t kMinCapacity = 4;
    static constexpr size_t kMaxCapacity = 1024;

    bool enabled = false;              // off by default: this is a research tool
    int windowTicks = 3;               // max age (in SIM ticks) a press may wait; 0 = no buffering
    size_t queueCapacity = 64;
    DualPolicy dualPolicy = DualPolicy::AnyCanAct;
    bool logging = true;
    bool trajectoryExperiment = false;

    // Always run user/Geode-provided values through this before use.
    TimingConfig sanitized() const {
        TimingConfig c = *this;
        c.windowTicks = std::clamp(c.windowTicks, 0, kMaxWindowTicks);
        c.queueCapacity = std::clamp(c.queueCapacity, kMinCapacity, kMaxCapacity);
        return c;
    }
};

}  // namespace dts
