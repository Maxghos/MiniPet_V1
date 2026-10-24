#include "WeatherBehavior.h"

namespace pet {
Condition weatherCondition(unsigned code) {
    if (code==0) return Condition::Clear;
    if (code<=3) return Condition::Cloud;
    if (code==45 || code==48) return Condition::Fog;
    if (code==51 || code==53 || code==55 || code==56 || code==57) return Condition::Drizzle;
    if (code==61 || code==63 || code==65 || code==66 || code==67 || code==80 || code==81 || code==82) return Condition::Rain;
    if (code==71 || code==73 || code==75 || code==77 || code==85 || code==86) return Condition::Snow;
    if (code==95 || code==96 || code==99) return Condition::Storm;
    return Condition::Unknown;
}
WeatherExpression weatherExpression(const WeatherData& w) {
    WeatherExpression result;
    result.condition=weatherCondition(w.code);
    result.hot=w.temperature>=30;
    result.cold=w.temperature<=5;
    switch (result.condition) {
    case Condition::Storm: result.mood=Mood::Shocked; break;
    case Condition::Drizzle: case Condition::Rain: case Condition::Snow: result.mood=Mood::Curious; break;
    case Condition::Fog: case Condition::Cloud: result.mood=Mood::Tired; break;
    case Condition::Clear: result.mood=result.hot ? Mood::Tired : result.cold ? Mood::Pout : Mood::Excited; break;
    default: result.mood=Mood::Curious;
    }
    return result;
}
static bool dry(Condition c) { return c==Condition::Clear || c==Condition::Cloud || c==Condition::Fog; }
static bool rain(Condition c) { return c==Condition::Drizzle || c==Condition::Rain || c==Condition::Storm; }
void WeatherChanges::observe(const WeatherData& w, Tick now, bool notify) {
    if (!notify) pending_=false; // quiet/sleep/disabled never queue notifications
    if (!weatherFresh(w,now)) { baseline_=false; pending_=false; return; }
    if (seen_ && w.revision==revision_) return;
    seen_=true; revision_=w.revision;
    const Condition next=weatherCondition(w.code);
    const bool continuous=baseline_ && elapsed(w.fetchedMs,fetched_)<=3600000 &&
        w.observedEpoch>observed_ && w.observedEpoch-observed_<=3600;
    if (continuous && notify && dry(condition_) && rain(next)) { pending_=true; pendingAt_=now; }
    // Same-time cached data and reconnections establish a baseline, not a change.
    condition_=next; fetched_=w.fetchedMs; observed_=w.observedEpoch; baseline_=true;
}
Tick AmbientDirector::interval(const Settings& s, uint32_t random) {
    Tick low,high;
    if (s.humor<20) { low=1200000; high=2400000; }
    else if (s.humor<50) { low=480000; high=900000; }
    else if (s.humor<85) { low=60000; high=180000; }
    else { low=20000; high=45000; }
    Tick value=low+random%(high-low+1);
    if (s.reminderSpeed==0) value*=2;
    if (s.reminderSpeed==2) value/=2;
    return value;
}
Ambient AmbientDirector::poll(Tick now, const Settings& s, bool allowed, bool attention,
                               bool fresh, uint32_t random, bool change) {
    if (!gap_) gap_=interval(s,random);
    if (!allowed || !(s.modules&Assistant) || elapsed(now,last_)<duration_+gap_) return Ambient::None;
    if (change && fresh && s.weatherNotices && s.messages && (s.modules&Weather)) return Ambient::WeatherChange;
    static constexpr Ambient choices[]={Ambient::Phrase,Ambient::Weather,Ambient::Reminder};
    Ambient fallback=Ambient::None;
    for (unsigned i=0;i<3;++i) {
        Ambient candidate=choices[(random%3+i)%3];
        const bool eligible=(candidate==Ambient::Phrase && s.messages && s.randomPhrases) ||
            (candidate==Ambient::Weather && fresh && s.weatherReactions && (s.modules&Weather)) ||
            (candidate==Ambient::Reminder && attention && s.messages);
        if (!eligible) continue;
        if (candidate==previous_) { fallback=candidate; continue; }
        previous_=candidate; return candidate;
    }
    previous_=fallback; return fallback;
}
void AmbientDirector::note(Tick now, Tick duration, const Settings& s, uint32_t random, bool autonomous) {
    last_=now; duration_=duration; gap_=interval(s,random);
    if (autonomous) {
        if (++burst_%3==0) gap_=std::max<Tick>(gap_,180000);
    } else burst_=0;
}
}
