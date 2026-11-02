#include "StudyCodec.h"
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
using namespace pet::study;
unsigned groups=0,checks=0;
#define REQUIRE(x) do{++checks;if(!(x)){std::cerr<<__func__<<':'<<__LINE__<<" failed: " #x<<'\n';std::exit(1);}}while(0)
void completeDefaultCycle() {
    ++groups;Timer timer;REQUIRE(timer.snapshot(0).config.valid());REQUIRE(timer.start(100));
    uint32_t now=100;
    for(unsigned round=1;round<=4;++round){
        const auto work=timer.snapshot(now);REQUIRE(work.phase==Phase::Focus&&work.state==State::Running);
        REQUIRE(work.round()==round);REQUIRE(work.remainingMs==25*60000);
        now+=25*60000;REQUIRE(timer.update(now));
        const auto rest=timer.snapshot(now);REQUIRE(rest.completedBlocks==round);
        REQUIRE(rest.focusMs==uint64_t(round)*25*60000);REQUIRE(rest.round()==round);
        REQUIRE(rest.phase==(round==4?Phase::LongBreak:Phase::ShortBreak));REQUIRE(rest.state==State::Running);
        REQUIRE(rest.remainingMs==(round==4?15u:5u)*60000);
        now+=rest.remainingMs;REQUIRE(timer.update(now));
        const auto waiting=timer.snapshot(now);REQUIRE(waiting.phase==Phase::Focus&&waiting.state==State::Waiting);
        const auto revision=waiting.revision;now+=600000;REQUIRE(!timer.update(now));
        REQUIRE(timer.snapshot(now).remainingMs==25*60000);REQUIRE(timer.snapshot(now).revision==revision);
        REQUIRE(timer.resume(now));
    }
    auto s=timer.snapshot(now);REQUIRE(s.round()==1);REQUIRE(s.completedBlocks==4);
    REQUIRE(s.focusMs==100*60000);REQUIRE(s.breakMs==30*60000);
    REQUIRE(!timer.start(now));REQUIRE(!timer.configure(Config{}));
}
void pausesAndRealTime() {
    ++groups;Timer timer;REQUIRE(timer.start(0));REQUIRE(timer.pause(12345));
    auto s=timer.snapshot(12345);REQUIRE(s.focusMs==12345);REQUIRE(s.remainingMs==1500000-12345);
    const auto revision=s.revision;REQUIRE(timer.pause(999999));REQUIRE(timer.snapshot(999999).revision==revision);
    REQUIRE(!timer.update(1200000));REQUIRE(timer.snapshot(1200000).focusMs==12345);
    REQUIRE(timer.resume(1300000));REQUIRE(timer.resume(1300001));REQUIRE(!timer.update(1303333));
    s=timer.snapshot(1303333);REQUIRE(s.focusMs==15678);REQUIRE(s.remainingMs==1500000-15678);
    REQUIRE(timer.stop(1304444));s=timer.snapshot(1304444);REQUIRE(!s.active());REQUIRE(s.focusMs==16789);
    REQUIRE(s.remainingMs==0&&s.durationMs==0);REQUIRE(!timer.pause(1400000));REQUIRE(!timer.resume(1400000));
    REQUIRE(timer.start(1500000));REQUIRE(timer.snapshot(1500000).focusMs==0);
}
void skipsDoNotInventWork() {
    ++groups;Timer timer;REQUIRE(timer.start(100));REQUIRE(timer.skip(5100));
    auto s=timer.snapshot(5100);REQUIRE(s.phase==Phase::ShortBreak);REQUIRE(s.completedBlocks==0);
    REQUIRE(s.focusMs==5000);REQUIRE(s.event==Event::Skipped);
    REQUIRE(timer.skip(7100));s=timer.snapshot(7100);REQUIRE(s.phase==Phase::Focus&&s.state==State::Waiting);
    REQUIRE(s.breakMs==2000&&s.round()==1);REQUIRE(timer.resume(8000));
    REQUIRE(timer.update(1508000));REQUIRE(timer.snapshot(1508000).completedBlocks==1);
    REQUIRE(timer.snapshot(1508000).phase==Phase::ShortBreak);
}
void automationAndDelayedPolling() {
    ++groups;Timer timer;Config c;c.focusMinutes=1;c.shortBreakMinutes=1;c.longBreakMinutes=2;
    c.blocksBeforeLong=2;c.autoBreak=false;c.autoFocus=true;
    REQUIRE(timer.configure(c));REQUIRE(timer.start(0));REQUIRE(timer.update(60000));
    REQUIRE(timer.snapshot(60000).state==State::Waiting);REQUIRE(!timer.update(3600000));
    REQUIRE(timer.snapshot(3600000).breakMs==0);REQUIRE(timer.resume(3600000));
    REQUIRE(timer.update(3660000));REQUIRE(timer.snapshot(3660000).state==State::Running);
    REQUIRE(timer.snapshot(3660000).phase==Phase::Focus);
    REQUIRE(timer.update(100000000));auto s=timer.snapshot(100000000);
    REQUIRE(s.completedBlocks==2&&s.phase==Phase::LongBreak&&s.state==State::Waiting);
    REQUIRE(s.remainingMs==120000&&s.focusMs==120000); // no catch-up through unseen stages
}
void millisRollover() {
    ++groups;Timer timer;Config c;c.focusMinutes=c.shortBreakMinutes=c.longBreakMinutes=1;
    REQUIRE(timer.configure(c));const uint32_t start=UINT32_MAX-40000;REQUIRE(timer.start(start));
    REQUIRE(!timer.update(start+59999));REQUIRE(timer.snapshot(start+59999).remainingMs==1);
    REQUIRE(timer.update(start+60000));REQUIRE(timer.snapshot(start+60000).phase==Phase::ShortBreak);
    REQUIRE(timer.pause(start+62000));REQUIRE(timer.snapshot(start+62000).breakMs==2000);
    REQUIRE(timer.resume(start+90000));REQUIRE(timer.update(start+148000));
    REQUIRE(timer.snapshot(start+148000).state==State::Waiting);
}
void persistenceAndCorruption() {
    ++groups;Timer timer;REQUIRE(timer.start(100));REQUIRE(!timer.update(73123));
    JsonDocument data;writeRecord(data.to<JsonObject>(),timer.record(73123));
    std::string json;serializeJson(data,json);JsonDocument decoded;REQUIRE(!deserializeJson(decoded,json));
    Record record;REQUIRE(readRecord(decoded.as<JsonVariantConst>(),record));Timer restored;
    REQUIRE(restored.restore(record,999));auto s=restored.snapshot(999);
    REQUIRE(s.state==State::Paused&&s.event==Event::Recovered);REQUIRE(s.focusMs==73023);
    REQUIRE(s.remainingMs==1500000-73023);REQUIRE(!restored.update(1000000));
    REQUIRE(restored.resume(2000000));REQUIRE(!restored.update(2001000));REQUIRE(restored.snapshot(2001000).focusMs==74023);
    auto bad=record;bad.version=2;REQUIRE(!restored.restore(bad,0));
    bad=record;bad.value.remainingMs=0;REQUIRE(!restored.restore(bad,0));
    bad=record;bad.value.durationMs++;REQUIRE(!restored.restore(bad,0));
    bad=record;bad.value.phase=Phase(99);REQUIRE(!restored.restore(bad,0));
    bad=record;bad.value.focusMs=UINT64_MAX;REQUIRE(!restored.restore(bad,0));
    decoded["value"]["remainingMs"]="300";REQUIRE(!readRecord(decoded.as<JsonVariantConst>(),record));
    decoded=data;decoded["value"]["config"].remove("autoBreak");REQUIRE(!readRecord(decoded.as<JsonVariantConst>(),record));
}
JsonDocument request(Timer& timer,const char* value,uint32_t now) {
    JsonDocument req,reply;REQUIRE(!deserializeJson(req,value));
    command(timer,req.as<JsonVariantConst>(),reply.to<JsonObject>(),now);return reply;
}
void protocolAndStaleControls() {
    ++groups;Timer timer;
    auto reply=request(timer,"{\"op\":\"study.start\",\"expectedRevision\":0}",0);REQUIRE(reply["ok"].as<bool>());
    const auto revision=timer.snapshot(0).revision;
    reply=request(timer,"{\"op\":\"study.start\",\"expectedRevision\":0}",1000);REQUIRE(!reply["ok"].as<bool>());
    REQUIRE(timer.snapshot(1000).revision==revision&&timer.snapshot(1000).focusMs==1000);
    reply=request(timer,"{\"op\":\"study.configure\",\"config\":{\"focusMinutes\":1}}",2000);REQUIRE(!reply["ok"].as<bool>());
    REQUIRE(timer.snapshot(2000).config.focusMinutes==25);
    JsonDocument req;req["op"]="study.skip";req["expectedRevision"]=revision;JsonDocument out;
    REQUIRE(!command(timer,req.as<JsonVariantConst>(),out.to<JsonObject>(),1500000));
    REQUIRE(timer.snapshot(1500000).phase==Phase::ShortBreak);REQUIRE(timer.snapshot(1500000).completedBlocks==1);
    REQUIRE(std::string(out["study"]["phase"].as<const char*>())=="shortBreak");
    reply=request(timer,"{\"op\":\"study.stop\"}",1500010);REQUIRE(reply["ok"].as<bool>());
    for(const char* config:{"{\"focusMinutes\":0}","{\"focusMinutes\":181}","{\"focusMinutes\":\"25\"}",
        "{\"focusMinutes\":-1}","{\"autoBreak\":1}","{\"unknown\":5}","{\"blocksBeforeLong\":1}","{\"longBreakMinutes\":121}"}){
        JsonDocument input,output;input["op"]="study.configure";REQUIRE(!deserializeJson(input["config"],config));
        REQUIRE(!command(timer,input.as<JsonVariantConst>(),output.to<JsonObject>(),1500011));
        REQUIRE(timer.snapshot(1500011).config.focusMinutes==25);
    }
    reply=request(timer,"{\"op\":\"study.get\"}",1600000);REQUIRE(reply["ok"].as<bool>());
    REQUIRE(reply["study"]["completedBlocks"].as<unsigned>()==1);
}
void physicalControls() {
    ++groups;Timer timer;Menu menu;auto s=timer.snapshot(0);menu.open(s);
    REQUIRE(menu.input(pet::Press::Long,s)==Menu::Action::Start);
    timer.start(0);s=timer.snapshot(0);menu.open(s);
    REQUIRE(menu.input(pet::Press::Long,s)==Menu::Action::Pause);
    menu.item=2;REQUIRE(menu.input(pet::Press::Long,s)==Menu::Action::None);REQUIRE(menu.confirming);
    REQUIRE(menu.input(pet::Press::Short,s)==Menu::Action::None);REQUIRE(!menu.confirming);
    menu.input(pet::Press::Long,s);REQUIRE(menu.input(pet::Press::Long,s)==Menu::Action::Stop);
    menu.item=3;menu.input(pet::Press::Long,s);REQUIRE(!menu.editing);
    timer.stop(0);s=timer.snapshot(0);menu.open(s);menu.item=3;
    menu.input(pet::Press::Long,s);REQUIRE(menu.editing);menu.input(pet::Press::Short,s);REQUIRE(menu.draft.focusMinutes==26);
    menu.input(pet::Press::Back,s);REQUIRE(menu.active&&!menu.editing);
    REQUIRE(menu.input(pet::Press::Back,s)==Menu::Action::Closed);REQUIRE(!menu.active);
    pet::Menu parent;parent.open(pet::Settings{});parent.item=pet::Menu::studyItem;
    REQUIRE(parent.input(pet::Press::Long)==pet::Menu::Action::Study);
    Timer advancing;advancing.start(0);auto original=advancing.snapshot(0);Menu guarded;
    guarded.open(original);guarded.item=1;guarded.input(pet::Press::Long,original);
    advancing.update(1500000);auto next=advancing.snapshot(1500000);
    REQUIRE(guarded.input(pet::Press::Long,next)==Menu::Action::None);
    REQUIRE(!guarded.confirming);
    REQUIRE(advancing.snapshot(1500000).phase==Phase::ShortBreak);
}
}
void testStudy(unsigned& suites,unsigned& assertions) {
    completeDefaultCycle();pausesAndRealTime();skipsDoNotInventWork();automationAndDelayedPolling();
    millisRollover();persistenceAndCorruption();protocolAndStaleControls();physicalControls();
    suites+=groups;assertions+=checks;
}
