// Dependency-free test harness. Build: cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests
#include <cstdio>
#include <functional>
#include <iostream>
#include <sstream>
#include <vector>
#include "../src/core/Scheduler.hpp"
#include "../src/core/SimClock.hpp"
#include "../src/core/TrajectoryModel.hpp"

using namespace dts;

static int g_fail = 0, g_run = 0;
#define CHECK(c) do { if(!(c)) { ++g_fail; std::printf("    FAIL %s:%d  %s\n", __FILE__, __LINE__, #c);} } while(0)
static void run(const char* name, const std::function<void()>& f) {
    ++g_run; int before = g_fail; f();
    std::printf("[%s] %s\n", g_fail == before ? " ok " : "FAIL", name);
}

static uint64_t g_id = 0;
static InputEvent ev(SimTick t, bool down, Button b = Button::Jump, uint8_t lane = 0) {
    InputEvent e; e.id = ++g_id; e.tick = t; e.down = down; e.button = b; e.lane = lane; return e;
}
static WorldState air(GameMode m = GameMode::Cube) { WorldState w; w.p[0].mode = m; w.p[1].mode = m; return w; }
static WorldState ground(GameMode m = GameMode::Cube) { WorldState w = air(m); w.p[0].onGround = w.p[1].onGround = true; return w; }
static TimingConfig cfg(int win, bool en = true) { TimingConfig c; c.enabled = en; c.windowTicks = win; return c; }

int main() {
    run("config sanitize clamps", [] {
        TimingConfig c; c.windowTicks = 999; c.queueCapacity = 1;
        auto s = c.sanitized();
        CHECK(s.windowTicks == TimingConfig::kMaxWindowTicks);
        CHECK(s.queueCapacity == TimingConfig::kMinCapacity);
        c.windowTicks = -5; CHECK(c.sanitized().windowTicks == 0);
    });

    run("queue: order preserved, FIFO", [] {
        InputQueue q(8);
        CHECK(q.push(ev(1, true)) == PushResult::Accepted);
        CHECK(q.push(ev(2, false)) == PushResult::Accepted);
        CHECK(q.push(ev(3, true)) == PushResult::Accepted);
        CHECK(q.front()->tick == 1 && q.front()->down); q.pop();
        CHECK(q.front()->tick == 2 && !q.front()->down); q.pop();
        CHECK(q.front()->tick == 3 && q.front()->down);
    });

    run("queue: duplicates rejected (state + id)", [] {
        InputQueue q(8);
        CHECK(q.push(ev(1, true)) == PushResult::Accepted);
        CHECK(q.push(ev(2, true)) == PushResult::DuplicateState);     // double press
        InputEvent stale = ev(3, false); stale.id = 1;                 // reused id
        CHECK(q.push(stale) == PushResult::DuplicateEvent);
        CHECK(q.push(ev(4, false)) == PushResult::Accepted);
        CHECK(q.push(ev(5, false)) == PushResult::DuplicateState);    // release while up
        CHECK(q.size() == 2);
    });

    run("queue: bounded, rejects newest, keeps order, stays consistent", [] {
        InputQueue q(4);
        bool d = true; int accepted = 0, overflow = 0, dup = 0;
        for (int i = 0; i < 10; ++i, d = !d) {
            auto r = q.push(ev(i, d));
            if (r == PushResult::Accepted) ++accepted;
            else if (r == PushResult::Overflow) ++overflow;
            else if (r == PushResult::DuplicateState) ++dup;  // release of a rejected press
        }
        CHECK(accepted == 4 && q.size() == 4);
        CHECK(overflow >= 1 && accepted + overflow + dup == 10);
        CHECK(q.front()->tick == 0);
        // queue content is still a valid alternating press/release sequence
        bool expect = true;
        while (const InputEvent* f = q.front()) { CHECK(f->down == expect); expect = !expect; q.pop(); }
    });

    run("queue: invalid lane/button rejected", [] {
        InputQueue q(4);
        InputEvent e = ev(1, true); e.lane = 5;
        CHECK(q.push(e) == PushResult::Invalid);
    });

    run("evaluator: press applies immediately when able", [] {
        TimingEvaluator t(cfg(3));
        auto r = t.evaluate(ev(10, true), 10, ground(), false);
        CHECK(r.decision == Decision::Apply && r.age == 0);
    });

    run("evaluator: boundary — window=3 allows age 3 if able, expires at age 3 if not", [] {
        TimingEvaluator t(cfg(3));
        InputEvent e = ev(10, true);
        CHECK(t.evaluate(e, 12, air(), false).decision == Decision::Defer);
        CHECK(t.evaluate(e, 13, ground(), false).decision == Decision::Apply);
        CHECK(t.evaluate(e, 13, air(), false).decision == Decision::Expire);
    });

    run("evaluator: window=0 never defers", [] {
        TimingEvaluator t(cfg(0));
        CHECK(t.evaluate(ev(5, true), 5, air(), false).decision == Decision::Expire);
        CHECK(t.evaluate(ev(5, true), 5, ground(), false).decision == Decision::Apply);
    });

    run("evaluator: hold modes never need buffering", [] {
        TimingEvaluator t(cfg(3));
        for (auto m : {GameMode::Ship, GameMode::Ufo, GameMode::Wave, GameMode::Swing})
            CHECK(t.evaluate(ev(1, true), 1, air(m), false).decision == Decision::Apply);
    });

    run("evaluator: dead player cannot act; future timestamp invalid", [] {
        TimingEvaluator t(cfg(3));
        WorldState w = ground(); w.p[0].alive = false;
        CHECK(t.evaluate(ev(1, true), 1, w, false).decision != Decision::Apply);
        CHECK(t.evaluate(ev(9, true), 5, ground(), false).decision == Decision::DropInvalid);
    });

    run("evaluator: dual policies", [] {
        WorldState w = air(); w.dual = true; w.p[0].onGround = true;  // only P1 grounded
        TimingConfig a = cfg(3); a.dualPolicy = DualPolicy::AnyCanAct;
        TimingConfig b = cfg(3); b.dualPolicy = DualPolicy::AllCanAct;
        CHECK(TimingEvaluator(a).evaluate(ev(1, true), 1, w, false).decision == Decision::Apply);
        CHECK(TimingEvaluator(b).evaluate(ev(1, true), 1, w, false).decision == Decision::Defer);
        w.p[1].alive = false;  // dead P2 should not block AllCanAct
        CHECK(TimingEvaluator(b).evaluate(ev(1, true), 1, w, false).decision == Decision::Apply);
    });

    run("evaluator: two-player mode evaluates only own lane", [] {
        WorldState w = air(); w.dual = true; w.twoPlayer = true; w.p[0].onGround = true;
        TimingEvaluator t(cfg(3));
        CHECK(t.evaluate(ev(1, true, Button::Jump, 0), 1, w, false).decision == Decision::Apply);
        CHECK(t.evaluate(ev(1, true, Button::Jump, 1), 1, w, false).decision == Decision::Defer);
    });

    run("scheduler: buffered press fires on landing (within window)", [] {
        MemoryLogSink log; Scheduler s(cfg(4), &log);
        s.push(ev(100, true));
        CHECK(s.step(100, air()).empty());
        CHECK(s.step(101, air()).empty());
        auto out = s.step(102, ground());
        CHECK(out.size() == 1 && out[0].result.decision == Decision::Apply && out[0].result.age == 2);
        CHECK(log.count("DECISION", "apply") == 1);
        CHECK(log.count("DECISION", "defer") == 1);  // logged once, not per step
    });

    run("scheduler: stale press expires; its release is dropped as orphan", [] {
        MemoryLogSink log; Scheduler s(cfg(2), &log);
        s.push(ev(10, true)); s.push(ev(11, false));
        CHECK(s.step(10, air()).empty());
        auto out = s.step(12, air());
        CHECK(out.size() == 2);
        CHECK(out[0].result.decision == Decision::Expire);
        CHECK(out[1].result.decision == Decision::DropOrphanRelease);
        CHECK(s.pending() == 0 && !s.deliveredHeld(0, Button::Jump));
    });

    run("scheduler: press then release both delivered in order", [] {
        Scheduler s(cfg(3), nullptr);
        s.push(ev(10, true)); s.push(ev(11, false));
        auto out = s.step(12, ground());
        CHECK(out.size() == 2 && out[0].event.down && !out[1].event.down);
        CHECK(!s.deliveredHeld(0, Button::Jump));
    });

    run("scheduler: head-of-line blocking preserves order", [] {
        Scheduler s(cfg(5), nullptr);
        s.push(ev(10, true)); s.push(ev(11, false)); s.push(ev(12, true));
        CHECK(s.step(12, air()).empty());       // first press waits, later events must not pass it
        CHECK(s.pending() == 3);
        auto out = s.step(13, ground());
        CHECK(out.size() == 3 && out[0].event.tick == 10 && out[2].event.tick == 12);
    });

    run("scheduler: disabled => no output, enabled() false", [] {
        Scheduler s(cfg(3, false), nullptr);
        CHECK(!s.enabled());
        CHECK(s.step(1, ground()).empty());
    });

    run("scheduler: paused => no processing, no aging side effects", [] {
        Scheduler s(cfg(3), nullptr);
        s.push(ev(10, true));
        WorldState w = ground(); w.paused = true;
        CHECK(s.step(50, w).empty());
        CHECK(s.pending() == 1);
    });

    run("scheduler: reset clears queue + bookkeeping (death/restart/exit)", [] {
        for (auto why : {ResetReason::Death, ResetReason::Restart, ResetReason::LevelExit}) {
            MemoryLogSink log; Scheduler s(cfg(5), &log);
            s.push(ev(1, true)); s.step(1, ground());          // delivered, key held
            CHECK(s.deliveredHeld(0, Button::Jump));
            s.push(ev(2, false)); s.push(ev(3, true));
            s.reset(why, 3);
            CHECK(s.pending() == 0 && !s.deliveredHeld(0, Button::Jump));
            CHECK(log.count("RESET", toString(why)) == 1);
            // after reset a fresh press is accepted (raw-held state cleared)
            CHECK(s.push(ev(4, true)) == PushResult::Accepted);
        }
    });

    run("scheduler: tick regression (restart w/o hook) auto-resets stale input", [] {
        MemoryLogSink log; Scheduler s(cfg(50), &log);
        s.push(ev(500, true));
        s.step(500, air());
        CHECK(s.pending() == 1);
        s.step(3, ground());                                   // tick went backwards
        CHECK(s.pending() == 0);
        CHECK(log.count("RESET", "tick-regression") == 1);
    });

    run("scheduler: toggling enabled resets state", [] {
        Scheduler s(cfg(5), nullptr);
        s.push(ev(1, true));
        s.setConfig(cfg(5, false));
        CHECK(s.pending() == 0);
    });

    run("scheduler: dual mode — one press, applies when either player able", [] {
        Scheduler s(cfg(3), nullptr);
        WorldState w = air(); w.dual = true;
        s.push(ev(10, true));
        CHECK(s.step(10, w).empty());
        w.p[1].onGround = true;
        CHECK(s.step(11, w).size() == 1);
    });

    run("simclock: independent of visual frame time; depends on tps", [] {
        SimClock c; c.setTicksPerSecond(240); c.setMaxLeadTicks(1);
        c.onStep(100, 10.000);
        CHECK(c.tickAt(10.001) == 101);        // 0.24 ticks of lead -> next step
        CHECK(c.tickAt(10.0045) == 102);       // 1.08 ticks -> capped lead of 1
        CHECK(c.tickAt(10.5) == 102);          // long gap capped, not unbounded
        // same wall offsets, "different FPS" = different gap between onStep calls: same answer
        SimClock d; d.setTicksPerSecond(240); d.onStep(100, 77.0);
        CHECK(d.tickAt(77.001) == 101);
        c.setTicksPerSecond(480);
        CHECK(c.tickAt(10.0025) == 102);       // rate change is honoured
        c.reset(); CHECK(!c.valid());
    });

    run("simclock: pause time is not converted into ticks", [] {
        SimClock c; c.setTicksPerSecond(240); c.onStep(10, 1.0);
        c.onResume(31.0);                      // paused for 30 s
        CHECK(c.tickAt(31.0005) == 11);
    });

    run("trajectory: deterministic, flagged unvalidated, pred != obs types", [] {
        MovementParams m;
        auto p1 = predictCubeJump(m, 100), p2 = predictCubeJump(m, 100);
        CHECK(p1.landTick == p2.landTick && p1.airTicks == p2.airTicks);
        CHECK(p1.apexY > 0 && p1.airTicks > 0 && !p1.validatedParams);
        Observation o; o.landTick = p1.landTick + 2; o.collided = true;
        auto c = compare(p1, o);
        CHECK(c.landTickError == 2 && c.observedCollision && !c.paramsValidated);
    });

    run("scenario: end-to-end CSV log is produced", [] {
        std::ostringstream os; CsvLogSink csv(os); Scheduler s(cfg(3), &csv);
        s.push(ev(1, true)); s.step(1, air()); s.step(2, ground());
        s.reset(ResetReason::Death, 2);
        std::string out = os.str();
        CHECK(out.find("kind,wall_sec") == 0);
        CHECK(out.find("DECISION") != std::string::npos && out.find("RESET") != std::string::npos);
        if (std::getenv("DTS_PRINT_LOG")) std::cout << out;
    });

    std::printf("\n%d tests, %d failed checks\n", g_run, g_fail);
    return g_fail ? 1 : 0;
}
