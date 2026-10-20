#include "DynamicLogic.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace {
    std::vector<ActivationPoint>& internalPoints() {
        static std::vector<ActivationPoint> points;
        return points;
    }
}

bool DynamicLogic::isEnabled() {
    auto mod = geode::Mod::get();
    if (!mod) return true;
    return mod->getSavedValue<bool>("dynamic-enabled", true);
}

void DynamicLogic::setEnabled(bool enabled) {
    auto mod = geode::Mod::get();
    if (mod) {
        mod->setSavedValue("dynamic-enabled", enabled);
    }
}

std::vector<ActivationPoint>& DynamicLogic::getPoints() {
    return internalPoints();
}

void DynamicLogic::addPoint(const ActivationPoint& point) {
    internalPoints().push_back(point);
}

void DynamicLogic::removePoint(size_t index) {
    if (index >= internalPoints().size()) return;
    internalPoints().erase(internalPoints().begin() + static_cast<std::ptrdiff_t>(index));
}

void DynamicLogic::togglePoint(size_t index) {
    if (index >= internalPoints().size()) return;
    internalPoints()[index].enabled = !internalPoints()[index].enabled;
}

bool DynamicLogic::isWindowActive(const ActivationPoint& point, int currentFrame) {
    if (!point.enabled) return false;

    return currentFrame >= point.targetFrame - point.earlyTolerance &&
           currentFrame <= point.targetFrame + point.lateTolerance;
}

bool DynamicLogic::shouldAcceptEarlyClick(const ActivationPoint& point, int currentFrame) {
    if (!point.enabled) return false;
    if (currentFrame < point.targetFrame - point.earlyTolerance) return false;
    if (currentFrame > point.targetFrame) return false;
    return true;
}

bool DynamicLogic::shouldAcceptLateClick(const ActivationPoint& point, int currentFrame) {
    if (!point.enabled) return false;
    if (currentFrame < point.targetFrame) return false;
    if (currentFrame > point.targetFrame + point.lateTolerance) return false;
    return true;
}

int DynamicLogic::getCurrentFrameEstimate() {
    // Constrained placeholder value.
    // In a production GD 2.2081 implementation this should use the actual game frame counter.
    return 0;
}

std::string DynamicLogic::getDebugLabel() {
    std::string windowInfo = "none";
    const int frame = getCurrentFrameEstimate();

    for (const auto& point : getPoints()) {
        if (point.enabled && isWindowActive(point, frame)) {
            windowInfo = point.getDebugWindowText();
            break;
        }
    }

    return "Frame: " + std::to_string(frame) + " | Window: " + windowInfo;
}

ActivationPoint DynamicLogic::makePoint(
    const std::string& levelId,
    int targetFrame,
    int earlyTolerance,
    int lateTolerance
) {
    ActivationPoint point;
    point.id = levelId + "-" + std::to_string(targetFrame);
    point.levelId = levelId;
    point.targetFrame = targetFrame;
    point.earlyTolerance = earlyTolerance;
    point.lateTolerance = lateTolerance;
    point.enabled = true;
    return point;
}