// ============================================================================
//  GEODE GLUE LAYER  --  DRAFT / UNVERIFIED
//
//  Every GD/Geode symbol in this file is a CANDIDATE taken from memory of the
//  public bindings. None has been compiled against your installed SDK. Before
//  building, walk docs/HOOKS.md and confirm each symbol, signature, and member
//  name in <GEODE_SDK>/bindings/<your GD version>/. Anything marked [VERIFY]
//  must be checked; anything that fails to compile should be fixed against the
//  bindings, not worked around with guessed signatures.
//
//  All timing logic lives in src/core (tested, SDK-free). This file only:
//    1. captures raw input,  2. builds a WorldState snapshot,
//    3. injects the scheduler's Apply decisions,  4. resets on lifecycle events.
// ============================================================================
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>   // [VERIFY] handleButton, processCommands
#include <Geode/modify/PlayLayer.hpp>         // [VERIFY] resetLevel, onQuit, destroyPlayer, levelComplete, pauseGame

#include <chrono>
#include <fstream>
#include <memory>

#include "../core/Scheduler.hpp"
#include "../core/SimClock.hpp"
#include "../core/TrajectoryModel.hpp"

using namespace geode::prelude;

namespace {

double nowSec() {
    using clk = std::chrono::steady_clock;
    return std::chrono::duration<double>(clk::now().time_since_epoch()).count();
}

struct Runtime {
    std::ofstream csv;
    std::unique_ptr<dts::CsvLogSink> sink;
    std::unique_ptr<dts::Scheduler> sched;
    dts::SimClock clock;
    dts::SimTick tick = 0;          // our own step counter; see HOOKS.md H2 [VERIFY]
    uint64_t nextId = 1;
    bool injecting = false;         // re-entrancy guard for our own handleButton calls
    bool inLevel = false;

    static Runtime& get() { static Runtime r; return r; }

    void init() {
        auto path = Mod::get()->getSaveDir() / "timing-log.csv";   // [VERIFY] getSaveDir
        csv.open(path, std::ios::app);
        sink = std::make_unique<dts::CsvLogSink>(csv, /*header=*/csv.tellp() == 0);
        sched = std::make_unique<dts::Scheduler>(readConfig(), sink.get());
    }

    static dts::TimingConfig readConfig() {
        auto* m = Mod::get();
        dts::TimingConfig c;
        c.enabled = m->getSettingValue<bool>("enabled");                          // [VERIFY]
        c.windowTicks = static_cast<int>(m->getSettingValue<int64_t>("window-ticks"));
        c.queueCapacity = static_cast<size_t>(m->getSettingValue<int64_t>("queue-capacity"));
        c.logging = m->getSettingValue<bool>("logging");
        c.trajectoryExperiment = m->getSettingValue<bool>("trajectory-experiment");
        c.dualPolicy = m->getSettingValue<bool>("dual-require-all")
                           ? dts::DualPolicy::AllCanAct : dts::DualPolicy::AnyCanAct;
        return c.sanitized();
    }

    void resetAll(dts::ResetReason why, GJBaseGameLayer* layer) {
        if (!sched) return;
        // If we are holding a delivered press, tell the game it was released,
        // otherwise the player can start the next attempt with a phantom held button.
        if (layer && sched->enabled()) {
            for (uint8_t lane = 0; lane < 2; ++lane)
                if (sched->deliveredHeld(lane, dts::Button::Jump)) {
                    injecting = true;
                    layer->GJBaseGameLayer::handleButton(false, 1, lane == 0);   // [VERIFY]
                    injecting = false;
                }
        }
        sched->reset(why, tick);
        clock.reset();
        tick = 0;
    }
};

dts::PlayerSnapshot snap(PlayerObject* p) {
    dts::PlayerSnapshot s;
    if (!p) { s.alive = false; return s; }
    // [VERIFY] every field name below against the bindings for your GD version.
    s.onGround = p->m_isOnGround;
    s.alive = !p->m_isDead;
    s.y = p->getPositionY();
    s.yVel = p->m_yVelocity;
    s.orbReady = false;   // TODO: needs a verified "touching orb/pad" member; until then
                          // only ground buffering is modelled (see KNOWN LIMITATIONS)
    if (p->m_isShip) s.mode = dts::GameMode::Ship;
    else if (p->m_isBall) s.mode = dts::GameMode::Ball;
    else if (p->m_isBird) s.mode = dts::GameMode::Ufo;
    else if (p->m_isDart) s.mode = dts::GameMode::Wave;
    else if (p->m_isRobot) s.mode = dts::GameMode::Robot;
    else if (p->m_isSpider) s.mode = dts::GameMode::Spider;
    else if (p->m_isSwing) s.mode = dts::GameMode::Swing;
    else s.mode = dts::GameMode::Cube;
    return s;
}

dts::WorldState world(GJBaseGameLayer* l, bool paused) {
    dts::WorldState w;
    w.p[0] = snap(l->m_player1);
    w.p[1] = snap(l->m_player2);
    w.dual = l->m_gameState.m_isDualMode;       // [VERIFY] location of the dual flag
    w.twoPlayer = false;                        // TODO: derive from the 2-player option [VERIFY]
    w.paused = paused;
    return w;
}

}  // namespace

$on_mod(Loaded) {
    Runtime::get().init();
    // [VERIFY] listenForSettingChanges availability/signature in your Geode version
    auto reload = [](auto) {
        auto& rt = Runtime::get();
        if (rt.sched) rt.sched->setConfig(Runtime::readConfig());
    };
    listenForSettingChanges<bool>("enabled", reload);
    listenForSettingChanges<int64_t>("window-ticks", reload);
    listenForSettingChanges<int64_t>("queue-capacity", reload);
    listenForSettingChanges<bool>("logging", reload);
    listenForSettingChanges<bool>("dual-require-all", reload);
    listenForSettingChanges<bool>("trajectory-experiment", reload);
}

class $modify(DtsBaseGameLayer, GJBaseGameLayer) {
    // H1: input capture. GD 2.2 also has its own queued-button path [VERIFY which
    // function is the *earliest common point* for both keyboard and touch input].
    void handleButton(bool down, int button, bool isPlayer1) {
        auto& rt = Runtime::get();
        if (rt.injecting || !rt.sched || !rt.sched->enabled() || !rt.inLevel || button < 1 || button > 3) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }
        dts::InputEvent e;
        e.id = rt.nextId++;
        e.wallSec = nowSec();
        e.tick = rt.clock.valid() ? rt.clock.tickAt(e.wallSec) : rt.tick + 1;
        e.button = static_cast<dts::Button>(button - 1);   // PlayerButton 1/2/3 -> 0/1/2 [VERIFY enum]
        e.down = down;
        e.lane = isPlayer1 ? 0 : 1;
        auto r = rt.sched->push(e);
        // Duplicates/overflow are swallowed on purpose; accepted events are held
        // in the queue and re-injected from processCommands.
        (void)r;
    }

    // H2: once-per-sim-step hook. [VERIFY] that this is called once per physics
    // step (not once per rendered frame). If it is per frame, move to the
    // per-step function and re-validate the SimClock mapping.
    void processCommands(float dt) {
        auto& rt = Runtime::get();
        if (rt.sched && rt.sched->enabled() && rt.inLevel) {
            rt.tick += 1;
            rt.clock.onStep(rt.tick, nowSec());
            // rt.clock.setTicksPerSecond(...) <- must come from the game's real step rate [VERIFY]
            auto out = rt.sched->step(rt.tick, world(this, /*paused=*/false));
            for (auto& o : out) {
                if (o.result.decision != dts::Decision::Apply) continue;
                rt.injecting = true;
                GJBaseGameLayer::handleButton(o.event.down,
                                              static_cast<int>(o.event.button) + 1,
                                              o.event.lane == 0);
                rt.injecting = false;
            }
        }
        GJBaseGameLayer::processCommands(dt);
    }
};

class $modify(DtsPlayLayer, PlayLayer) {
    // Lifecycle hooks: all queued input is discarded to prevent stale replay.
    void resetLevel() {
        Runtime::get().resetAll(dts::ResetReason::Restart, this);
        PlayLayer::resetLevel();
        Runtime::get().inLevel = true;
    }
    void destroyPlayer(PlayerObject* p, GameObject* o) {            // [VERIFY signature]
        Runtime::get().resetAll(dts::ResetReason::Death, this);
        PlayLayer::destroyPlayer(p, o);
    }
    void levelComplete() {
        Runtime::get().resetAll(dts::ResetReason::LevelExit, this);
        PlayLayer::levelComplete();
    }
    void onQuit() {
        Runtime::get().inLevel = false;
        Runtime::get().resetAll(dts::ResetReason::LevelExit, this);
        PlayLayer::onQuit();
    }
    void pauseGame(bool b) {                                         // [VERIFY signature]
        Runtime::get().resetAll(dts::ResetReason::Pause, this);
        PlayLayer::pauseGame(b);
        Runtime::get().clock.reset();
    }
};
