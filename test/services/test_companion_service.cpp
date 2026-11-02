#include "AgendaService.h"
#include <LittleFS.h>
#include <cstdlib>
#include <iostream>

namespace {
unsigned checks=0,suites=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<__func__<<':'<<__LINE__<<" failed: " #x<<'\n';std::exit(1);}}while(0)
pet::agenda::Moment now{20261102,780,1793613600};
pet::agenda::Moment clockNow(){return now;}
void reset(){LittleFS=FakeFS{};serviceMillis=0;now={20261102,780,1793613600};}
JsonDocument command(AgendaService& service,const char* json){
    JsonDocument req,reply;CHECK(!deserializeJson(req,json));service.execute(req.as<JsonVariantConst>(),reply.to<JsonObject>());return reply;
}
void poll(AgendaService& service,uint32_t tick){serviceMillis=tick;service.poll(pet::study::Snapshot{},pet::agenda::Totals{},pet::Settings{});}
void noticesRespectHumorPriorityAndSharedPacing(){
    ++suites;reset();AgendaService service;CHECK(service.begin(true,clockNow));std::string message;
    serviceMillis=44000;CHECK(!service.companionNotice(message,100,true));serviceMillis=45000;
    CHECK(!service.companionNotice(message,0,true));CHECK(!service.companionNotice(message,100,false));
    CHECK(service.companionNotice(message,100,true));CHECK(message.find("Buenas tardes")!=std::string::npos);
    CHECK(!service.companionNotice(message,100,true));poll(service,46000);
    serviceMillis=224999;CHECK(!service.companionNotice(message,100,true));serviceMillis=225000;
    CHECK(service.companionNotice(message,100,true));CHECK(message.find("Bitacora 1/6")!=std::string::npos);poll(service,226000);
    AgendaService reboot;CHECK(reboot.begin(true,clockNow));serviceMillis=271000;
    CHECK(!reboot.companionNotice(message,100,true)); // no greeting or chapter repeats after reboot
    command(reboot,R"({"op":"habit.add","title":"Leer","targetPerWeek":1})");command(reboot,R"({"op":"habit.done","id":1,"done":true})");
    CHECK(!reboot.companionNotice(message,100,false));serviceMillis=276000;CHECK(reboot.companionNotice(message,100,true));CHECK(message.find("Meta semanal")!=std::string::npos);
}
void dailyMemoryIsSavedEvenWithoutAnIntervention(){
    ++suites;reset();AgendaService service;CHECK(service.begin(true,clockNow));
    CHECK(command(service,R"({"op":"companion.options","greetings":false,"stories":false,"celebrations":false})")["ok"].as<bool>());
    serviceMillis=50000;std::string message;CHECK(!service.companionNotice(message,100,true));poll(service,51000);
    AgendaService reboot;CHECK(reboot.begin(true,clockNow));
    auto read=command(reboot,R"({"op":"companion.get"})");CHECK(read["companion"]["daysTogether"].as<unsigned>()==1);CHECK(read["agenda"]["persisted"].as<bool>());
}
}
int main(){
    noticesRespectHumorPriorityAndSharedPacing();dailyMemoryIsSavedEvenWithoutAnIntervention();
    std::cout<<suites<<" companion service suites passed, "<<checks<<" assertions (real AgendaService.cpp, simulated flash/clock)\n";
}
