#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>

#include "DynamicLogic.hpp"
#include "DynamicMenu.hpp"

using namespace geode::prelude;

// Placeholder runtime hook for a safe assist layer.
// This project intentionally avoids broad physics edits.
// The real GD 2.2081 implementation should validate the real jump
// input and collision state before allowing a late input.
class $modify(PlayerObject) {
public:
    bool pushButton(int key) {
        if (!DynamicLogic::isEnabled()) {
            return PlayerObject::pushButton(key);
        }

        for (const auto& point : DynamicLogic::getPoints()) {
            if (!point.enabled) continue;

            const int currentFrame = DynamicLogic::getCurrentFrameEstimate();
            const bool earlyAllowed = DynamicLogic::shouldAcceptEarlyClick(point, currentFrame);
            const bool lateAllowed = DynamicLogic::shouldAcceptLateClick(point, currentFrame);

            if (earlyAllowed || lateAllowed) {
                return PlayerObject::pushButton(key);
            }
        }

        return PlayerObject::pushButton(key);
    }
};

class DynamicMod : public geode::Mod {
public:
    static DynamicMod* get() {
        return static_cast<DynamicMod*>(geode::Mod::get());
    }

    bool init() override {
        if (!Mod::init()) return false;

        this->setKeybind("dynamic-menu", "7", [] {
            DynamicMenu::show();
        });

        DynamicLogic::getPoints().push_back(
            DynamicLogic::makePoint("level-demo", 100, 3, 3)
        );

        return true;
    }
};

$execute {
    geode::Mod::registerMod(new DynamicMod());
}