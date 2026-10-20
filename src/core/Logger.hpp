#pragma once
#include <cstdint>
#include <ostream>
#include <string>
#include <vector>
#include "Types.hpp"

namespace dts {

struct LogRecord {
    std::string kind;      // INPUT | DECISION | RESET | PRED | OBS | NOTE
    double wallSec = 0.0;
    SimTick tick = 0;
    uint64_t eventId = 0;
    int button = -1;
    int down = -1;
    int lane = -1;
    int p1Mode = -1, p1Ground = -1, p1Orb = -1;
    int p2Mode = -1, p2Ground = -1, p2Orb = -1;
    int dual = -1;
    std::string outcome;   // decision / push result / reset reason
    std::string reason;
    int64_t age = -1;
    std::string extra;     // free-form key=value pairs (no commas)
};

class ILogSink {
public:
    virtual ~ILogSink() = default;
    virtual void write(const LogRecord& r) = 0;
};

class NullLogSink : public ILogSink {
public:
    void write(const LogRecord&) override {}
};

class MemoryLogSink : public ILogSink {
public:
    std::vector<LogRecord> records;
    void write(const LogRecord& r) override { records.push_back(r); }
    size_t count(const std::string& kind, const std::string& outcome) const {
        size_t n = 0;
        for (auto& r : records) if (r.kind == kind && r.outcome == outcome) ++n;
        return n;
    }
};

class CsvLogSink : public ILogSink {
public:
    explicit CsvLogSink(std::ostream& os, bool header = true) : os_(os) {
        if (header)
            os_ << "kind,wall_sec,tick,event_id,button,down,lane,p1_mode,p1_ground,p1_orb,"
                   "p2_mode,p2_ground,p2_orb,dual,outcome,reason,age,extra\n";
    }
    void write(const LogRecord& r) override {
        os_ << r.kind << ',' << r.wallSec << ',' << r.tick << ',' << r.eventId << ','
            << r.button << ',' << r.down << ',' << r.lane << ',' << r.p1Mode << ','
            << r.p1Ground << ',' << r.p1Orb << ',' << r.p2Mode << ',' << r.p2Ground << ','
            << r.p2Orb << ',' << r.dual << ',' << r.outcome << ',' << r.reason << ','
            << r.age << ',' << r.extra << '\n';
    }
private:
    std::ostream& os_;
};

}  // namespace dts
