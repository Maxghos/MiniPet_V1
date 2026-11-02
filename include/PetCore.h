#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace pet {
using Tick = uint32_t;
inline Tick elapsed(Tick now, Tick then) { return now - then; }
enum Module : uint8_t { Assistant = 1, Clock = 2, Weather = 4 };
enum class Mood : uint8_t { Calm, Attentive, Curious, Happy, Shocked, Annoyed, Tired, Sleeping, Smug, Pout, Excited, Count };
enum class View : uint8_t { Pet, Weather, Clock, WeatherReaction };
enum class Press : uint8_t { None, Short, Long, Back };

struct Settings {
    uint8_t modules = Assistant | Clock | Weather;
    uint8_t humor = 50;
    uint8_t brightness = 120;
    uint8_t messages = 1;
    uint8_t quietEnabled = 0;
    uint8_t quietStart = 22;
    uint8_t quietEnd = 8;
    uint8_t reminderSpeed = 1; // 0 slow, 1 normal, 2 frequent
    uint8_t weatherView = 3; // current, details, forecast, rotate
    uint16_t gestureMinCm = 50;
    uint16_t gestureMaxCm = 80;
    uint16_t gestureMs = 5000;
    uint16_t presenceCm = 130;
    uint16_t absenceSeconds = 90;
    uint16_t sleepMinutes = 15;
    uint16_t pageSeconds = 5;
    // Appended: the old 24-byte V1 prefix is preserved for migration.
    uint8_t randomPhrases = 1;
    uint8_t weatherReactions = 1;
    uint8_t weatherNotices = 1;
    uint8_t stormFlash = 0;
    void sanitize();
    bool quiet(int localHour) const;
};

class TouchButton {
public:
    Press update(bool active, Tick now);
private:
    bool raw_ = false, stable_ = false;
    Tick rawSince_ = 0, downSince_ = 0;
};

// Small movements and brief missing echoes pause progress; only valid readings
// count. Rearming requires 1 s of confirmed distance outside the gesture zone.
class HoldGesture {
public:
    bool update(Tick now, bool valid, float cm, const Settings& s);
    bool update(Tick now, bool valid, float cm, float rawCm, const Settings& s);
    void reset(Tick now);
    float progress(Tick now, const Settings& s) const;
    bool holding() const {return holding_;}
private:
    bool armed_ = false, holding_ = false, outside_ = false;
    Tick outsideSince_ = 0, heldMs_ = 0, lastGood_ = 0, lastSample_ = 0;
    bool sampled_ = false, previousGood_ = false;
};

class DistanceFilter {
public:
    bool sample(float cm, Tick now);
    float cm() const { return filtered_; }
    bool fresh(Tick now) const { return count_ && elapsed(now, lastValid_) <= 350; }
private:
    std::array<float, 5> values_{};
    uint8_t count_ = 0, index_ = 0;
    Tick lastValid_ = 0;
    float filtered_ = 0;
};

class Presence {
public:
    bool update(Tick now, bool valid, float cm, uint16_t range);
    bool present() const { return present_; }
private:
    bool present_ = false, candidate_ = false, tracking_ = false;
    Tick candidateSince_ = 0,lastSample_ = 0;
    bool sampled_ = false;
};

class PirMotion {
public:
    void begin(Tick now,Tick warmupMs=60000);
    bool update(bool high,Tick now);
private:
    Tick began_=0,warmup_=60000,changedAt_=0;
    bool warmed_=false,raw_=false,stable_=false,armed_=false;
};

class ApproachReaction {
public:
    bool update(Tick now,bool valid,float cm,bool allow);
private:
    Tick lastSample_=0,lastReaction_=0;
    float previous_=0;
    bool hadSample_=false,reacted_=false;
};

class Query {
public:
    void start(Tick now, const Settings& s, bool joke, bool weatherReaction = false);
    bool update(Tick now);
    bool active() const { return active_; }
    bool joke() const { return active_ && joke_; }
    View view() const;
    Tick stageAge(Tick now) const { return elapsed(now,since_); }
    Tick totalDuration() const;
    void cancel() { active_ = false; }
private:
    bool active_ = false, joke_ = false;
    std::array<View, 3> pages_{};
    uint8_t count_ = 0, index_ = 0;
    Tick since_ = 0, duration_ = 5000;
};

class Personality {
public:
    void begin(Tick now);
    void react(Mood mood, Tick now, Tick duration);
    void interact(Tick now, const Settings& s);
    void update(Tick now, bool present, bool motion, const Settings& s);
    bool attentionDue(Tick now, bool present, const Settings& s) const;
    void remind(Tick now, const Settings& s);
    Mood mood(Tick now) const;
    uint8_t reminders() const { return reminders_; }
private:
    Tick lastCompany_ = 0, lastInteraction_ = 0;
    Tick reactionAt_ = 0, reactionDuration_ = 0;
    Tick lastMotion_ = 0;
    bool wasPresent_ = false;
    uint8_t reminders_ = 0;
    Mood base_ = Mood::Calm, reaction_ = Mood::Calm;
};

class Menu {
public:
    static constexpr unsigned count = 31;
    static constexpr unsigned studyItem = 22, appItem = 23, telegramItem=24, targetItem=25,dodgeItem=26,otaItem=27,wifiItem = 28, resetItem = 29, saveItem = 30;
    void open(const Settings& settings) { draft = settings; active = true; editing = false; item = 0; }
    // Save, Portal, Closed, or None. Wi-Fi is configured separately from settings.
    enum class Action { None, Save, Portal, PairApp, Study, Telegram, Target, Dodge, Ota, Closed };
    Action input(Press press);
    const char* label() const;
    void value(char* out, unsigned size) const;
    bool active = false, editing = false;
    unsigned item = 0;
    Settings draft{};
private:
    void change();
};
}
