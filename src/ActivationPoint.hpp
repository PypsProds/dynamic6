#pragma once

#include <string>

struct ActivationPoint {
    std::string id;
    std::string levelId;
    int targetFrame = 0;
    int earlyTolerance = 3;
    int lateTolerance = 3;
    bool enabled = true;

    bool containsFrame(int frame) const {
        const int startFrame = targetFrame - earlyTolerance;
        const int endFrame = targetFrame + lateTolerance;
        return enabled && frame >= startFrame && frame <= endFrame;
    }

    int getWindowStart() const {
        return targetFrame - earlyTolerance;
    }

    int getWindowEnd() const {
        return targetFrame + lateTolerance;
    }

    std::string getDebugWindowText() const {
        return std::to_string(getWindowStart()) + "-" + std::to_string(getWindowEnd());
    }
};