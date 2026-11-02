#include "MiniGames.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <set>

namespace {
unsigned checks=0,groups=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<__func__<<':'<<__LINE__<<" failed: " #x<<'\n';std::exit(1);}}while(0)
using namespace pet;
using namespace pet::games;
void lifecycleAndLostHand(){
    ++groups;Game game;CHECK(!game.active());CHECK(!game.begin(0,Kind::Count));CHECK(game.begin(100,Kind::Target));
    CHECK(!game.begin(200,Kind::Dodge));CHECK(game.snapshot(200).state==State::Countdown);
    game.update(3100,true,45);CHECK(game.snapshot(3100).state==State::Playing);
    game.update(3200,false,0);CHECK(!game.snapshot(3200).inputValid);
    for(Tick now=3300;now<8100;now+=100){game.update(now,false,0);CHECK(game.snapshot(now).score==0);CHECK(game.snapshot(now).state==State::Playing);}
    game.update(8100,false,0);CHECK(game.snapshot(8100).state==State::Result);CHECK(game.snapshot(8100).sensorLost);
    game.update(14099,true,45);CHECK(game.active());game.update(14100,true,45);CHECK(!game.active());
    CHECK(game.begin(15000,Kind::Dodge));game.stop();CHECK(!game.active());game.stop();CHECK(!game.active());
}
void stableTargetsAndNoGhostPoints(){
    ++groups;Game game(123);CHECK(game.begin(0,Kind::Target));const float first=game.snapshot(0).targetCm;
    game.update(0,true,first);game.update(3000,true,first);game.update(3100,true,first);
    for(Tick now=3200;now<3900;now+=100){game.update(now,true,first);CHECK(game.snapshot(now).score==0);}
    game.update(3900,true,first);auto s=game.snapshot(3900);CHECK(s.score==1);CHECK(std::fabs(first-s.targetCm)>=10);CHECK(s.heldMs==0);
    const float target=s.targetCm;
    game.update(4000,true,target);const auto before=game.snapshot(4000);
    game.update(4100,false,target);game.update(4200,true,std::numeric_limits<float>::quiet_NaN());
    game.update(4300,true,300);game.update(4400,true,target);s=game.snapshot(4400);
    CHECK(s.score==1);CHECK(s.heldMs==0);CHECK(s.remainingMs==before.remainingMs);
    for(Tick now=4500;now<=5300;now+=100)game.update(now,true,target);
    CHECK(game.snapshot(5300).score==2);
}
void counterWrapAndSchedulingGap(){
    ++groups;const Tick start=UINT32_MAX-1500;Game game;CHECK(game.begin(start,Kind::Target));
    float target=game.snapshot(start).targetCm;game.update(start,true,target);game.update(start+3000,true,target);
    for(Tick dt=3100;dt<=3900;dt+=100)game.update(start+dt,true,target);
    CHECK(game.snapshot(start+3900).score==1);target=game.snapshot(start+3900).targetCm;
    for(Tick dt=4000;dt<=4500;dt+=100)game.update(start+dt,true,target);
    const auto before=game.snapshot(start+4500);CHECK(before.heldMs>0);
    game.update(start+20000,true,target);const auto after=game.snapshot(start+20000);
    CHECK(after.remainingMs==before.remainingMs);CHECK(after.score==before.score);CHECK(after.heldMs==0);
    game.update(start+20100,true,target);CHECK(game.snapshot(start+20100).remainingMs==before.remainingMs-100);
}
void dodgeCanWinAndCollisionsCountOnce(){
    ++groups;Game winner(123);CHECK(winner.begin(0,Kind::Dodge));winner.update(0,true,winner.snapshot(0).targetCm);
    winner.update(3000,true,winner.snapshot(3000).targetCm);
    for(Tick now=3100;now<=49000&&winner.snapshot(now).state==State::Playing;now+=100)winner.update(now,true,winner.snapshot(now).targetCm);
    auto result=winner.snapshot(49000);CHECK(result.state==State::Result);CHECK(result.remainingMs>0);CHECK(result.lives==3);CHECK(result.score>=18);CHECK(!result.sensorLost);
    Game loser(123);CHECK(loser.begin(0,Kind::Dodge));
    auto wrong=[&](){return loser.snapshot(0).targetCm>45?15.f:80.f;};
    loser.update(0,true,wrong());loser.update(3000,true,wrong());
    for(Tick now=3100;now<=12000&&loser.snapshot(now).state==State::Playing;now+=100)loser.update(now,true,wrong());
    result=loser.snapshot(12000);CHECK(result.state==State::Result);CHECK(result.score==0);CHECK(result.lives==0);CHECK(!result.sensorLost);
}
void missingEchoPausesWallsAndTimer(){
    ++groups;Game game;CHECK(game.begin(0,Kind::Dodge));game.update(0,true,40);game.update(3000,true,40);game.update(3100,true,40);game.update(3200,true,40);
    const auto before=game.snapshot(3200);
    for(Tick now=3300;now<=7000;now+=100)game.update(now,false,0);
    auto s=game.snapshot(7000);CHECK(s.state==State::Playing);CHECK(s.lives==3);CHECK(s.score==0);CHECK(s.remainingMs==before.remainingMs);
    game.update(7100,true,40);s=game.snapshot(7100);CHECK(s.remainingMs==before.remainingMs);CHECK(s.lives==3);
    game.update(7200,true,40);CHECK(game.snapshot(7200).remainingMs==before.remainingMs-100);
}
void framesAndKindValidation(){
    ++groups;Kind kind=Kind::Target;CHECK(parseKind("dodge",kind)&&kind==Kind::Dodge);CHECK(parseKind("target",kind)&&kind==Kind::Target);
    CHECK(!parseKind("unknown",kind));CHECK(std::string(stateName(State::Countdown))=="countdown");
    std::set<std::array<uint8_t,512>> pictures;Canvas out;
    for(auto mode:{Kind::Target,Kind::Dodge}){
        Game game;game.draw(out,0);pictures.insert(out.bytes);CHECK(game.begin(0,mode));game.draw(out,0);pictures.insert(out.bytes);
        game.update(0,true,45);game.update(3000,true,45);game.draw(out,3000);pictures.insert(out.bytes);
        for(Tick now=3100;now<=4000;now+=100)game.update(now,true,60);
        game.draw(out,4000);pictures.insert(out.bytes);
        for(Tick now=4100;now<=9000;now+=100)game.update(now,false,0);
        game.draw(out,9000);pictures.insert(out.bytes);
    }
    CHECK(pictures.size()>=8);
}
}
void testGames(unsigned& suites,unsigned& assertions){
    lifecycleAndLostHand();stableTargetsAndNoGhostPoints();counterWrapAndSchedulingGap();dodgeCanWinAndCollisionsCountOnce();missingEchoPausesWallsAndTimer();framesAndKindValidation();
    suites+=groups;assertions+=checks;
}
