#pragma once

#include <string>
#include <vector>

#include "ActivationPoint.hpp"

class DynamicLogic {
public:
    static bool isEnabled();
    static void setEnabled(bool enabled);

    static std::vector<ActivationPoint>& getPoints();
    static void addPoint(const ActivationPoint& point);
    static void removePoint(size_t index);
    static void togglePoint(size_t index);

    static bool isWindowActive(const ActivationPoint& point, int currentFrame);
    static bool shouldAcceptEarlyClick(const ActivationPoint& point, int currentFrame);
    static bool shouldAcceptLateClick(const ActivationPoint& point, int currentFrame);

    static int getCurrentFrameEstimate();
    static std::string getDebugLabel();

    static ActivationPoint makePoint(
        const std::string& levelId,
        int targetFrame,
        int earlyTolerance,
        int lateTolerance
    );
};