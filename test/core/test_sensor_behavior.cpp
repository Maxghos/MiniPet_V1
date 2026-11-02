#include "PetCore.h"
#include <cstdlib>
#include <iostream>
namespace {
using namespace pet;
unsigned suites=0,checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<__func__<<':'<<__LINE__<<" failed: " #x<<'\n';std::exit(1);}}while(0)
void arm(HoldGesture& g){Settings s;for(Tick t=0;t<=1000;t+=100)CHECK(!g.update(t,true,200,s));}
void missingEchoNeverRearms(){
    ++suites;Settings s;HoldGesture g;
    for(Tick t=0;t<=2000;t+=100)CHECK(!g.update(t,false,0,s));
    for(Tick t=2100;t<10000;t+=100)CHECK(!g.update(t,true,60,s));
    CHECK(g.progress(9900,s)==0);
}
void smallMovementsAndIntermittentEcho(){
    ++suites;Settings s;HoldGesture g;arm(g);
    for(Tick t=1100;t<6100;t+=100)CHECK(!g.update(t,true,((t/100)%3==0)?77.f:52.f,s));
    CHECK(g.update(6100,true,65,s));
    g.reset(0);arm(g);
    for(Tick t=1100;t<=3000;t+=100)CHECK(!g.update(t,true,60,s));
    const float before=g.progress(3000,s);
    CHECK(!g.update(3100,false,0,s));CHECK(g.progress(3100,s)==before);
    CHECK(!g.update(3200,true,60,s));CHECK(g.progress(3200,s)==before);
    for(Tick t=3300;t<6300;t+=100)CHECK(!g.update(t,true,60,s));
    CHECK(g.update(6300,true,60,s));CHECK(!g.update(6400,true,60,s));
    g.reset(0);arm(g);
    for(Tick t=1100;t<=3000;t+=100)CHECK(!g.update(t,true,60,s));
    CHECK(!g.update(3400,false,0,s));CHECK(g.progress(3400,s)==0);
    CHECK(!g.update(3500,true,60,s));CHECK(g.progress(3500,s)==0);
}
void bordersWithdrawalAndFilterLag(){
    ++suites;Settings s;HoldGesture g;arm(g);
    for(Tick t=1100;t<6100;t+=100)CHECK(!g.update(t,true,t%200?48.f:82.f,s));
    CHECK(g.progress(6000,s)==0);CHECK(!g.update(6100,true,60,s));
    for(Tick t=6200;t<11100;t+=100)CHECK(!g.update(t,true,t%200?48.f:82.f,s));
    CHECK(g.update(11100,true,60,s));
    g.reset(0);arm(g);
    for(Tick t=1100;t<=6000;t+=100)CHECK(!g.update(t,true,60,60,s));
    CHECK(!g.update(6100,true,60,180,s)); // old median cannot finish a withdrawn hand's hold
    CHECK(!g.update(6200,true,180,180,s));CHECK(g.progress(6200,s)==0);
    g.reset(0);
    for(Tick t=0;t<2000;t+=100)CHECK(!g.update(t,true,200,60,s));
    for(Tick t=2000;t<=8000;t+=100)CHECK(!g.update(t,true,60,60,s));
}
void presenceGapsAndAlertCooldown(){
    ++suites;Presence p;p.update(0,true,60,130);p.update(100,true,60,130);
    CHECK(!p.update(5000,true,60,130));CHECK(!p.present());
    for(Tick t=5100;t<5600;t+=100)CHECK(!p.update(t,true,60,130));
    CHECK(p.update(5600,true,60,130));CHECK(p.present());
    ApproachReaction a;CHECK(!a.update(0,true,100,true));CHECK(a.update(100,true,30,true));
    CHECK(!a.update(200,true,100,true));CHECK(!a.update(300,true,30,true));
    CHECK(!a.update(3000,true,100,true));CHECK(a.update(3100,true,30,true));
    CHECK(!a.update(3200,true,100,false));CHECK(!a.update(3300,true,30,false));
    CHECK(!a.update(4000,false,0,true));CHECK(!a.update(4100,true,20,true));
}
void pirNoiseWarmupAndRollover(){
    ++suites;PirMotion p;p.begin(0);
    CHECK(!p.update(true,59999));CHECK(!p.update(true,60000));CHECK(!p.update(true,70000));
    CHECK(!p.update(false,70100));CHECK(!p.update(false,70180));
    CHECK(!p.update(true,70200));CHECK(!p.update(false,70230));CHECK(!p.update(false,70310));
    CHECK(!p.update(true,70400));CHECK(!p.update(true,70479));CHECK(p.update(true,70480));
    CHECK(!p.update(true,71000));CHECK(!p.update(false,71100));CHECK(!p.update(true,71140));
    CHECK(!p.update(true,71220));CHECK(!p.update(false,71300));CHECK(!p.update(false,71380));
    CHECK(!p.update(true,71400));CHECK(p.update(true,71480));
    const Tick boot=UINT32_MAX-30000;p.begin(boot);CHECK(!p.update(false,boot+60000));
    CHECK(!p.update(true,boot+60100));CHECK(p.update(true,boot+60180));
}
}
void testSensorBehavior(unsigned& groups,unsigned& assertions){
    missingEchoNeverRearms();smallMovementsAndIntermittentEcho();bordersWithdrawalAndFilterLag();
    presenceGapsAndAlertCooldown();pirNoiseWarmupAndRollover();groups+=suites;assertions+=checks;
}
