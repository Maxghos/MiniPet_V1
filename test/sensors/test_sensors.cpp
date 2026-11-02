#include "Sensors.h"
#include "BoardConfig.h"
#include <cstdlib>
#include <iostream>
#include <cmath>

namespace {
unsigned assertions=0,suites=0;
#define CHECK(x) do{++assertions;if(!(x)){std::cerr<<__func__<<':'<<__LINE__<<" failed: " #x<<'\n';std::exit(1);}}while(0)
void edge(uint64_t at,bool high){fake::clockUs=at;fake::pins[board::echo]=high;fake::interrupt();}
bool poll(Sensors& sensors,uint64_t at){fake::clockUs=at;return sensors.poll(millis());}
void echo(Sensors& sensors,uint64_t start,unsigned width){CHECK(!poll(sensors,start));edge(start+200,true);edge(start+200+width,false);}
void normalAndShortGlitch(){
    ++suites;fake::reset();Sensors sensors;sensors.begin();
    CHECK(!poll(sensors,99000));CHECK(fake::triggers==0);
    CHECK(!poll(sensors,100000));CHECK(fake::clockUs==100010);CHECK(fake::triggers==1);
    edge(100100,true);edge(100120,false);edge(100300,true);edge(103800,false);
    CHECK(poll(sensors,104000));CHECK(sensors.valid);CHECK(std::fabs(sensors.rawCm-60)<.1f);
    CHECK(std::fabs(sensors.cm-60)<.1f);CHECK(fake::modes[board::echo]==INPUT);
    CHECK(!poll(sensors,199000));CHECK(fake::triggers==1);
}
void lateEchoAndTimeoutRecovery(){
    ++suites;fake::reset();Sensors sensors;sensors.begin();
    CHECK(!poll(sensors,100000));edge(125000,true);edge(129000,false);
    CHECK(poll(sensors,130000));CHECK(!sensors.valid); // plausible width, wrong window
    echo(sensors,200000,3500);CHECK(poll(sensors,204000));CHECK(sensors.valid);
    CHECK(!poll(sensors,300000));CHECK(poll(sensors,328000));CHECK(!sensors.valid);
    edge(340000,true);edge(343500,false);CHECK(!poll(sensors,344000));
    echo(sensors,400000,3500);CHECK(poll(sensors,404000));CHECK(sensors.valid);
}
void stuckHighAndOldSamples(){
    ++suites;fake::reset();Sensors sensors;sensors.begin();fake::pins[board::echo]=HIGH;
    CHECK(poll(sensors,100000));CHECK(!sensors.valid);CHECK(fake::triggers==0);
    CHECK(poll(sensors,200000));CHECK(fake::triggers==0);
    edge(220000,false);echo(sensors,300000,3500);CHECK(poll(sensors,304000));CHECK(sensors.valid);
    echo(sensors,400000,3500);CHECK(poll(sensors,520000));CHECK(!sensors.valid);
    echo(sensors,530000,3500);CHECK(poll(sensors,534000));CHECK(sensors.valid);
}
void clockRolloverAndRepeatedLoss(){
    ++suites;
    for(uint64_t start:{uint64_t(UINT32_MAX)-100500,uint64_t(UINT32_MAX-80)*1000}){
        fake::reset();fake::clockUs=start;Sensors sensors;sensors.begin();
        echo(sensors,start+100000,3500);CHECK(poll(sensors,start+104000));CHECK(sensors.valid);
    }
    fake::reset();Sensors sensors;sensors.begin();
    for(unsigned cycle=1;cycle<=50;++cycle){
        const uint64_t start=uint64_t(cycle)*100000;
        if(cycle%2){echo(sensors,start,3500);CHECK(poll(sensors,start+4000));CHECK(sensors.valid);}
        else {CHECK(!poll(sensors,start));CHECK(poll(sensors,start+28000));CHECK(!sensors.valid);}
    }
    CHECK(fake::triggers==50);
}
void movingHandWithMissingEchoAndOutlier(){
    ++suites;fake::reset();Sensors sensors;sensors.begin();pet::HoldGesture gesture;pet::Settings settings;
    unsigned queries=0;
    for(unsigned cycle=1;cycle<=95;++cycle){
        const uint64_t start=uint64_t(cycle)*100000;
        CHECK(!poll(sensors,start));
        if(cycle!=40){
            const float cm=cycle<=15||cycle==50?200.f:cycle%2?55.f:75.f;
            if(cycle==45){edge(start+50,true);edge(start+70,false);}
            const unsigned width=unsigned(cm*2/.0343f);
            edge(start+200,true);edge(start+200+width,false);
        }
        CHECK(poll(sensors,start+28000));
        if(gesture.update(millis(),sensors.valid,sensors.cm,sensors.rawCm,settings)){
            ++queries;CHECK(millis()>=7228); // five confirmed seconds, excluding the two interrupted intervals
        }
    }
    CHECK(queries==1); // moving hand works, and keeping it there never spams requests
}
}
int main(){
    normalAndShortGlitch();lateEchoAndTimeoutRecovery();stuckHighAndOldSamples();clockRolloverAndRepeatedLoss();movingHandWithMissingEchoAndOutlier();
    std::cout<<suites<<" sensor driver suites passed, "<<assertions<<" assertions (real Sensors.cpp, simulated GPIO/clock)\n";
}
