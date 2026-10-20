#pragma once
#include <vector>
#include "Config.hpp"
#include "InputQueue.hpp"
#include "Logger.hpp"
#include "TimingEvaluator.hpp"

namespace dts {

enum class ResetReason { Death, Restart, LevelExit, Pause, ConfigChange, TickRegression, Manual };

inline const char* toString(ResetReason r) {
    switch (r) {
        case ResetReason::Death: return "death";
        case ResetReason::Restart: return "restart";
        case ResetReason::LevelExit: return "level-exit";
        case ResetReason::Pause: return "pause";
        case ResetReason::ConfigChange: return "config-change";
        case ResetReason::TickRegression: return "tick-regression";
        case ResetReason::Manual: return "manual";
    }
    return "?";
}

struct Outcome {
    InputEvent event;
    EvalResult result;
    SimTick tick;
};

// Owns the queue and the "was this press delivered?" bookkeeping.
// Output of step() is the ordered list of Apply decisions the glue must inject
// (plus drops/expiries for logging). The scheduler never talks to the game.
class Scheduler {
public:
    Scheduler(TimingConfig cfg, ILogSink* sink)
        : cfg_(cfg.sanitized()), eval_(cfg_), q_(cfg_.queueCapacity), sink_(sink) {}

    const TimingConfig& config() const { return cfg_; }

    void setConfig(const TimingConfig& c) {
        bool wasEnabled = cfg_.enabled;
        cfg_ = c.sanitized();
        eval_.setConfig(cfg_);
        q_.setCapacity(cfg_.queueCapacity);
        if (wasEnabled != cfg_.enabled) reset(ResetReason::ConfigChange, lastStep_);
    }

    // Returns false when disabled -> caller must pass the input straight to the game.
    bool enabled() const { return cfg_.enabled; }

    PushResult push(const InputEvent& e) {
        PushResult r = q_.push(e);
        if (cfg_.logging) {
            LogRecord rec = base("INPUT", e.tick, &e);
            rec.outcome = toString(r);
            emit(rec);
        }
        return r;
    }

    std::vector<Outcome> step(SimTick now, const WorldState& w) {
        std::vector<Outcome> out;
        if (!cfg_.enabled || w.paused) return out;
        if (haveStep_ && now < lastStep_) reset(ResetReason::TickRegression, now);
        lastStep_ = now; haveStep_ = true; world_ = w;

        while (const InputEvent* f = q_.front()) {
            InputEvent e = *f;
            bool applied = applied_[e.lane][static_cast<int>(e.button)];
            EvalResult r = eval_.evaluate(e, now, w, applied);
            if (r.decision == Decision::Defer) {
                if (cfg_.logging && lastDeferId_ != e.id) {  // log first deferral only
                    lastDeferId_ = e.id;
                    logDecision(e, r, now, w);
                }
                break;  // head-of-line: later events must not overtake a waiting press
            }
            if (r.decision == Decision::Apply)
                applied_[e.lane][static_cast<int>(e.button)] = e.down;
            else if (e.down)  // expired/invalid press was never delivered
                applied_[e.lane][static_cast<int>(e.button)] = false;
            if (cfg_.logging) logDecision(e, r, now, w);
            out.push_back({e, r, now});
            q_.pop();
        }
        return out;
    }

    // Drop all pending events and delivery bookkeeping. Safe to call at any time.
    void reset(ResetReason why, SimTick now) {
        size_t dropped = q_.size();
        q_.clear();
        for (auto& l : applied_) for (auto& a : l) a = false;
        lastDeferId_ = 0;
        haveStep_ = false;
        if (cfg_.logging) {
            LogRecord rec = base("RESET", now, nullptr);
            rec.outcome = toString(why);
            rec.extra = "dropped=" + std::to_string(dropped);
            emit(rec);
        }
    }

    size_t pending() const { return q_.size(); }
    // True if the game currently believes this button is held because of us.
    // Glue should inject a synthetic release for these on reset.
    bool deliveredHeld(uint8_t lane, Button b) const {
        return applied_[lane][static_cast<int>(b)];
    }

private:
    LogRecord base(const char* kind, SimTick tick, const InputEvent* e) const {
        LogRecord r;
        r.kind = kind; r.tick = tick;
        if (e) {
            r.wallSec = e->wallSec; r.eventId = e->id; r.button = (int)e->button;
            r.down = e->down; r.lane = e->lane;
        }
        fillWorld(r, world_);
        return r;
    }
    static void fillWorld(LogRecord& r, const WorldState& w) {
        r.p1Mode = (int)w.p[0].mode; r.p1Ground = w.p[0].onGround; r.p1Orb = w.p[0].orbReady;
        r.p2Mode = (int)w.p[1].mode; r.p2Ground = w.p[1].onGround; r.p2Orb = w.p[1].orbReady;
        r.dual = w.dual;
    }
    void logDecision(const InputEvent& e, const EvalResult& res, SimTick now, const WorldState& w) {
        LogRecord r = base("DECISION", now, &e);
        fillWorld(r, w);
        r.outcome = toString(res.decision); r.reason = res.reason; r.age = res.age;
        r.extra = "window=" + std::to_string(cfg_.windowTicks);
        emit(r);
    }
    void emit(const LogRecord& r) { if (sink_) sink_->write(r); }

    TimingConfig cfg_;
    TimingEvaluator eval_;
    InputQueue q_;
    ILogSink* sink_;
    WorldState world_{};
    bool applied_[kLaneCount][kButtonCount] = {};
    SimTick lastStep_ = 0;
    bool haveStep_ = false;
    uint64_t lastDeferId_ = 0;
};

}  // namespace dts
