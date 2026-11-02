#pragma once
#include "PetCore.h"
#include <cstdint>

namespace pet::study {
struct Config {
    uint16_t focusMinutes=25, shortBreakMinutes=5, longBreakMinutes=15;
    uint8_t blocksBeforeLong=4;
    bool autoBreak=true, autoFocus=false;
    bool valid() const;
};
enum class Phase : uint8_t { Idle, Focus, ShortBreak, LongBreak };
enum class State : uint8_t { Idle, Running, Paused, Waiting };
enum class Event : uint8_t { None, Started, FocusDone, BreakDone, Paused, Resumed, Skipped, Stopped, Recovered };
const char* name(Phase value);
const char* name(State value);
const char* name(Event value);
struct Snapshot {
    Config config;
    Phase phase=Phase::Idle;
    State state=State::Idle;
    Tick remainingMs=0, durationMs=0, eventAgeMs=0;
    uint32_t completedBlocks=0, revision=0, eventId=0;
    uint64_t focusMs=0, breakMs=0;
    Event event=Event::None;
    bool active() const { return phase!=Phase::Idle; }
    unsigned round() const;
};
struct Record {
    uint32_t version=1;
    Snapshot value;
};
// Only active, unpaused time counts. A delayed poll advances at most one phase,
// so a disconnected phone never invents completed work during missed transitions.
class Timer {
public:
    bool configure(const Config& config);
    bool start(Tick now);
    bool pause(Tick now);
    bool resume(Tick now);
    bool skip(Tick now);
    bool stop(Tick now);
    bool update(Tick now);
    Snapshot snapshot(Tick now) const;
    Record record(Tick now) const { return {1,snapshot(now)}; }
    bool restore(const Record& record,Tick now);
private:
    void enter(Phase phase,State state,Tick now,Event event);
    void event(Event value,Tick now);
    Tick duration(Phase phase) const;
    Snapshot value_;
    Tick last_=0,eventAt_=0;
};

// Nested in the existing settings button; the button never changes the timer
// directly from the home screen. Destructive actions need a second long press.
class Menu {
public:
    static constexpr unsigned count=11;
    enum class Action { None, Start, Pause, Resume, Skip, Stop, Save, Closed };
    void open(const Snapshot& value);
    Action input(Press press,const Snapshot& value);
    const char* label(const Snapshot& value) const;
    void value(char* output,unsigned size,const Snapshot& value) const;
    bool active=false,editing=false,confirming=false;
    unsigned item=0;
    Config draft;
private:
    uint32_t confirmationRevision_=0;
};
}
