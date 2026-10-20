#include "DynamicMenu.hpp"

#include <Geode/Geode.hpp>
#include <Geode/ui.hpp>

#include "DynamicLogic.hpp"

using namespace geode::prelude;

namespace {
    static const int kMenuTag = 0xDYNM;
    static const int kListTag = 0xDYNLIST;

    std::string formatPointRow(const ActivationPoint& point) {
        std::string row = point.levelId.empty() ? "point" : point.levelId;
        row += " | target=" + std::to_string(point.targetFrame);
        row += " | early=" + std::to_string(point.earlyTolerance);
        row += " | late=" + std::to_string(point.lateTolerance);
        return row;
    }
}

DynamicMenu* DynamicMenu::create() {
    auto ret = new DynamicMenu();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

void DynamicMenu::show() {
    auto scene = CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return;

    auto existing = static_cast<DynamicMenu*>(scene->getChildByTag(kMenuTag));
    if (existing) {
        existing->removeFromParentAndCleanup(true);
    }

    auto menu = DynamicMenu::create();
    menu->setTag(kMenuTag);
    scene->addChild(menu, 9999);
}

void DynamicMenu::hide() {
    auto scene = CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return;

    auto existing = static_cast<DynamicMenu*>(scene->getChildByTag(kMenuTag));
    if (existing) {
        existing->removeFromParentAndCleanup(true);
    }
}

bool DynamicMenu::init() {
    if (!CCLayer::init()) return false;

    auto winSize = CCDirector::sharedDirector()->getWinSize();

    auto bg = CCScale9Sprite::create("GJ_button_01.png");
    bg->setContentSize({ 500, 380 });
    bg->setPosition(winSize.width / 2, winSize.height / 2);
    bg->setOpacity(180);
    addChild(bg);

    auto title = CCLabelBMFont::create("Dynamic", "goldFont.fnt");
    title->setPosition(winSize.width / 2, winSize.height / 2 + 145);
    addChild(title);

    auto closeBtn = CCMenuItemSpriteExtra::create(
        CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png"),
        nullptr,
        this,
        menu_selector(DynamicMenu::onClose)
    );
    closeBtn->setPosition(winSize.width / 2 + 200, winSize.height / 2 + 150);

    auto masterLabel = CCLabelBMFont::create("Master", "bigFont.fnt");
    masterLabel->setPosition(winSize.width / 2 - 120, winSize.height / 2 + 85);
    masterLabel->setScale(0.6f);

    m_masterToggle = CCMenuItemToggler::create(
        nullptr,
        nullptr,
        [this](CCObject*) {
            this->onToggleMaster(nullptr);
        }
    );
    m_masterToggle->setPosition(winSize.width / 2 - 20, winSize.height / 2 + 85);
    m_masterToggle->setSelected(DynamicLogic::isEnabled());

    auto addBtn = CCMenuItemSpriteExtra::create(
        CCSprite::createWithSpriteFrameName("GJ_button_01.png"),
        nullptr,
        this,
        menu_selector(DynamicMenu::onAddPoint)
    );
    addBtn->setPosition(winSize.width / 2 + 120, winSize.height / 2 + 85);

    auto addLabel = CCLabelBMFont::create("Add Point", "bigFont.fnt");
    addLabel->setScale(0.5f);
    addLabel->setPosition(0, 0);
    addBtn->addChild(addLabel);

    auto earlyMinus = CCMenuItemSpriteExtra::create(
        CCSprite::createWithSpriteFrameName("GJ_arrow_01.png"),
        nullptr,
        this,
        menu_selector(DynamicMenu::onSetEarlyTolerance)
    );
    earlyMinus->setPosition(winSize.width / 2 - 160, winSize.height / 2 + 25);

    auto earlyPlus = CCMenuItemSpriteExtra::create(
        CCSprite::createWithSpriteFrameName("GJ_arrow_02.png"),
        nullptr,
        this,
        menu_selector(DynamicMenu::onSetEarlyTolerance)
    );
    earlyPlus->setPosition(winSize.width / 2 - 80, winSize.height / 2 + 25);

    auto lateMinus = CCMenuItemSpriteExtra::create(
        CCSprite::createWithSpriteFrameName("GJ_arrow_01.png"),
        nullptr,
        this,
        menu_selector(DynamicMenu::onSetLateTolerance)
    );
    lateMinus->setPosition(winSize.width / 2 + 40, winSize.height / 2 + 25);

    auto latePlus = CCMenuItemSpriteExtra::create(
        CCSprite::createWithSpriteFrameName("GJ_arrow_02.png"),
        nullptr,
        this,
        menu_selector(DynamicMenu::onSetLateTolerance)
    );
    latePlus->setPosition(winSize.width / 2 + 120, winSize.height / 2 + 25);

    auto earlyLabel = CCLabelBMFont::create("Early", "bigFont.fnt");
    earlyLabel->setScale(0.55f);
    earlyLabel->setPosition(winSize.width / 2 - 120, winSize.height / 2 + 55);

    auto lateLabel = CCLabelBMFont::create("Late", "bigFont.fnt");
    lateLabel->setScale(0.55f);
    lateLabel->setPosition(winSize.width / 2 + 80, winSize.height / 2 + 55);

    m_statusLabel = CCLabelBMFont::create("Status: active", "bigFont.fnt");
    m_statusLabel->setScale(0.48f);
    m_statusLabel->setPosition(winSize.width / 2, winSize.height / 2 - 105);
    addChild(m_statusLabel);

    m_debugLabel = CCLabelBMFont::create("Frame: 0 | Window: none", "bigFont.fnt");
    m_debugLabel->setScale(0.42f);
    m_debugLabel->setPosition(winSize.width / 2, winSize.height / 2 - 135);
    addChild(m_debugLabel);

    auto menuItems = CCMenu::create(
        closeBtn,
        m_masterToggle,
        addBtn,
        earlyMinus,
        earlyPlus,
        lateMinus,
        latePlus,
        nullptr
    );
    menuItems->setPosition(0, 0);
    addChild(menuItems);

    m_pointsMenu = CCMenu::create();
    m_pointsMenu->setTag(kListTag);
    m_pointsMenu->setPosition(0, 0);
    addChild(m_pointsMenu);

    refreshList();
    scheduleUpdate();

    return true;
}

void DynamicMenu::update(float dt) {
    CCLayer::update(dt);

    if (m_debugLabel) {
        m_debugLabel->setString(DynamicLogic::getDebugLabel().c_str());
    }

    if (m_statusLabel) {
        const bool enabled = DynamicLogic::isEnabled();
        m_statusLabel->setString(enabled ? "Status: active" : "Status: disabled");
    }
}

void DynamicMenu::onClose(CCObject*) {
    removeFromParentAndCleanup(true);
}

void DynamicMenu::onAddPoint(CCObject*) {
    static int nextTarget = 100;

    ActivationPoint point = DynamicLogic::makePoint(
        "level-" + std::to_string(DynamicLogic::getPoints().size() + 1),
        nextTarget,
        3,
        3
    );

    DynamicLogic::addPoint(point);
    nextTarget += 10;
    refreshList();
}

void DynamicMenu::onToggleMaster(CCObject*) {
    const bool enabled = !DynamicLogic::isEnabled();
    DynamicLogic::setEnabled(enabled);

    if (m_masterToggle) {
        m_masterToggle->setSelected(enabled);
    }
}

void DynamicMenu::onSetEarlyTolerance(CCObject*) {
    auto& points = DynamicLogic::getPoints();
    if (points.empty()) {
        points.push_back(DynamicLogic::makePoint("level-demo", 100, 3, 3));
    }

    auto& point = points.front();
    if (point.earlyTolerance > 0) {
        point.earlyTolerance--;
    } else {
        point.earlyTolerance = 0;
    }
}

void DynamicMenu::onSetLateTolerance(CCObject*) {
    auto& points = DynamicLogic::getPoints();
    if (points.empty()) {
        points.push_back(DynamicLogic::makePoint("level-demo", 100, 3, 3));
    }

    auto& point = points.front();
    if (point.lateTolerance > 0) {
        point.lateTolerance--;
    } else {
        point.lateTolerance = 0;
    }
}

void DynamicMenu::onPointToggle(CCObject* obj) {
    auto item = dynamic_cast<CCMenuItemSpriteExtra*>(obj);
    if (!item) return;

    const int index = reinterpret_cast<intptr_t>(item->getUserData());
    DynamicLogic::togglePoint(static_cast<size_t>(index));
    refreshList();
}

void DynamicMenu::onPointDelete(CCObject* obj) {
    auto item = dynamic_cast<CCMenuItemSpriteExtra*>(obj);
    if (!item) return;

    const int index = reinterpret_cast<intptr_t>(item->getUserData());
    DynamicLogic::removePoint(static_cast<size_t>(index));
    refreshList();
}

void DynamicMenu::refreshList() {
    if (!m_pointsMenu) return;

    m_pointsMenu->removeAllChildren();

    const auto& points = DynamicLogic::getPoints();
    float y = 110.0f;

    for (size_t i = 0; i < points.size(); ++i) {
        const auto& point = points[i];

        auto row = CCLabelBMFont::create(
            formatPointRow(point).c_str(),
            "bigFont.fnt"
        );
        row->setScale(0.42f);
        row->setPosition(-180.0f, y);

        auto toggle = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_checkbox_01.png"),
            nullptr,
            this,
            menu_selector(DynamicMenu::onPointToggle)
        );
        toggle->setPosition(-60.0f, y);
        toggle->setUserData(reinterpret_cast<void*>(static_cast<intptr_t>(i)));

        auto del = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_deleteBtn_001.png"),
            nullptr,
            this,
            menu_selector(DynamicMenu::onPointDelete)
        );
        del->setPosition(150.0f, y);
        del->setUserData(reinterpret_cast<void*>(static_cast<intptr_t>(i)));

        auto rowNode = CCNode::create();
        rowNode->addChild(row);
        rowNode->addChild(toggle);
        rowNode->addChild(del);

        m_pointsMenu->addChild(rowNode);

        y -= 28.0f;
    }
}