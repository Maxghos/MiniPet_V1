#pragma once
#include "PetCore.h"
#include "WeatherData.h"

namespace pet {
enum class Condition : uint8_t { Clear, Cloud, Fog, Drizzle, Rain, Snow, Storm, Unknown };
Condition weatherCondition(unsigned code);
struct WeatherExpression {
    Mood mood = Mood::Curious;
    Condition condition = Condition::Unknown;
    bool hot = false, cold = false;
};
WeatherExpression weatherExpression(const WeatherData& weather);

class WeatherChanges {
public:
    void observe(const WeatherData& data, Tick now, bool notify);
    bool pending(Tick now) const { return pending_ && elapsed(now,pendingAt_)<=300000; }
    void consume() { pending_=false; }
private:
    bool baseline_ = false, seen_ = false, pending_ = false;
    uint32_t revision_ = 0;
    Tick fetched_ = 0, pendingAt_ = 0;
    int64_t observed_ = 0;
    Condition condition_ = Condition::Unknown;
};

enum class Ambient : uint8_t { None, Phrase, Weather, Reminder, WeatherChange, Scene };
class AmbientDirector {
public:
    void begin(Tick now) { last_=now; }
    Ambient poll(Tick now, const Settings& s, bool allowed, bool attentionDue,
                 bool freshWeather, uint32_t random, bool weatherChange,bool scenes=false);
    void note(Tick now, Tick duration, const Settings& s, uint32_t random, bool autonomous=true);
    static Tick interval(const Settings& s, uint32_t random);
private:
    Tick last_=0, duration_=0, gap_=0;
    uint8_t burst_=0;
    Ambient previous_=Ambient::None;
};
}
