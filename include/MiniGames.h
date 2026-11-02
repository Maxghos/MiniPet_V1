#pragma once
#include "FaceRenderer.h"
#include <string>

namespace pet::games {
enum class Kind : uint8_t { Target,Dodge,Count };
enum class State : uint8_t { Idle,Countdown,Playing,Result };
struct Snapshot {
    Kind kind=Kind::Target;
    State state=State::Idle;
    unsigned score=0,lives=3;
    Tick remainingMs=0,heldMs=0;
    float cm=0,targetCm=0;
    bool inputValid=false,sensorLost=false;
};
const char* kindName(Kind kind);
const char* stateName(State state);
bool parseKind(const std::string& name,Kind& kind);
class Game {
public:
    explicit Game(uint32_t seed=0x93723):rng_(seed?seed:1){}
    bool begin(Tick now,Kind kind);
    void update(Tick now,bool valid,float cm);
    void stop(){state_=State::Idle;}
    bool active() const{return state_!=State::Idle;}
    Snapshot snapshot(Tick now) const;
    void draw(Canvas& out,Tick now) const;
private:
    void finish(Tick now,bool sensorLost);
    void nextTarget();
    void nextWall();
    unsigned random();
    State state_=State::Idle;
    Kind kind_=Kind::Target;
    Tick stateAt_=0,lastUpdate_=0,lastGood_=0,played_=0,held_=0,wallAge_=0,hitAt_=0;
    bool valid_=false,previousValid_=false,filtered_=false,sensorLost_=false;
    unsigned score_=0,lives_=3;
    float cm_=0,target_=45;
    int gapY_=17;
    uint32_t rng_;
};
}
