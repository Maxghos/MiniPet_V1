#include "AgendaService.h"
#include <LittleFS.h>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
unsigned checks=0,suites=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<__func__<<':'<<__LINE__<<" failed: " #x<<'\n';std::exit(1);}}while(0)
pet::agenda::Moment current{20261102,600,1793613600};
pet::agenda::Moment clockNow(){return current;}
void reset(){LittleFS=FakeFS{};serviceMillis=0;current={20261102,600,1793613600};}
JsonDocument command(AgendaService& service,const char* json){
    JsonDocument req,reply;CHECK(!deserializeJson(req,json));service.execute(req.as<JsonVariantConst>(),reply.to<JsonObject>());return reply;
}
void poll(AgendaService& service,uint32_t now,const pet::agenda::Totals& totals={}){
    serviceMillis=now;service.poll(pet::study::Snapshot{},totals,pet::Settings{});
}
void failedWriteRetriesWithoutOtherChanges(){
    ++suites;reset();AgendaService service;CHECK(service.begin(true,clockNow));
    LittleFS.failWrite=true;
    auto reply=command(service,R"({"op":"task.add","title":"Leer"})");
    CHECK(reply["ok"].as<bool>());CHECK(!reply["agenda"]["persisted"].as<bool>());
    const auto attempts=LittleFS.writeOpens;LittleFS.failWrite=false;
    poll(service,4000);CHECK(LittleFS.writeOpens==attempts);
    poll(service,5000);CHECK(command(service,R"({"op":"agenda.get"})")["agenda"]["persisted"].as<bool>());
    AgendaService reboot;CHECK(reboot.begin(true,clockNow));
    CHECK(command(reboot,R"({"op":"agenda.get"})")["agenda"]["tasks"].size()==1);
}
void interruptedWriteKeepsOldDataAndRecovers(){
    ++suites;reset();AgendaService service;CHECK(service.begin(true,clockNow));
    command(service,R"({"op":"task.add","title":"Primera"})");const auto previous=LittleFS.files.at("/state/agenda.json");
    LittleFS.budget=12;
    CHECK(!command(service,R"({"op":"task.add","title":"Segunda"})")["agenda"]["persisted"].as<bool>());
    CHECK(LittleFS.files.at("/state/agenda.json")==previous);
    AgendaService interrupted;CHECK(interrupted.begin(true,clockNow));
    CHECK(command(interrupted,R"({"op":"agenda.get"})")["agenda"]["tasks"].size()==1);
    LittleFS.budget=std::numeric_limits<size_t>::max();poll(service,5000);
    CHECK(command(service,R"({"op":"agenda.get"})")["agenda"]["persisted"].as<bool>());
    AgendaService reboot;CHECK(reboot.begin(true,clockNow));
    CHECK(command(reboot,R"({"op":"agenda.get"})")["agenda"]["tasks"].size()==2);
}
void corruptPrimaryFallsBackAndInvalidStateIsReported(){
    ++suites;reset();AgendaService service;CHECK(service.begin(true,clockNow));
    command(service,R"({"op":"task.add","title":"Primera"})");
    command(service,R"({"op":"task.add","title":"Segunda"})");
    LittleFS.files["/state/agenda.json"]="{broken";
    AgendaService reboot;CHECK(reboot.begin(true,clockNow));
    auto reply=command(reboot,R"({"op":"agenda.get"})");
    CHECK(reply["agenda"]["tasks"].size()==1);CHECK(reply["agenda"]["recovered"].as<bool>());
    poll(reboot,5000);CHECK(command(reboot,R"({"op":"agenda.get"})")["agenda"]["persisted"].as<bool>());
    reset();LittleFS.files["/state/agenda.json"]="{broken";AgendaService invalid;CHECK(invalid.begin(true,clockNow));
    reply=command(invalid,R"({"op":"agenda.get"})");CHECK(reply["agenda"]["loadError"].as<bool>());CHECK(!reply["agenda"]["persisted"].as<bool>());
    poll(invalid,65000);CHECK(LittleFS.files.at("/state/agenda.json")=="{broken");
}
void pendingRenameAndQuietDoNotLeakNotices(){
    ++suites;reset();AgendaService service;CHECK(service.begin(true,clockNow));
    command(service,R"({"op":"task.add","title":"Primera"})");LittleFS.failRename=true;
    CHECK(!command(service,R"({"op":"task.add","title":"Segunda"})")["agenda"]["persisted"].as<bool>());
    LittleFS.failRename=false;poll(service,5000);
    CHECK(command(service,R"({"op":"agenda.get"})")["agenda"]["persisted"].as<bool>());
    const auto writes=LittleFS.writeOpens;poll(service,65000);poll(service,500000);CHECK(LittleFS.writeOpens==writes);
    command(service,R"({"op":"reminder.add","title":"Agua","minute":601})");current.minute=601;poll(service,501000);
    AgendaNotice notice;CHECK(service.peekTelegram(notice));std::string display;CHECK(service.takeDisplay(display));
    CHECK(display.find("Agua")!=std::string::npos);service.finishTelegram(notice.id,true);CHECK(!service.peekTelegram(notice));
    command(service,R"({"op":"reminder.add","title":"Silencio","minute":602})");current.minute=602;serviceMillis=502000;
    pet::Settings quiet;quiet.quietEnabled=1;quiet.quietStart=quiet.quietEnd=0;
    service.poll(pet::study::Snapshot{},pet::agenda::Totals{},quiet);CHECK(!service.peekTelegram(notice));CHECK(!service.takeDisplay(display));
}
void powerLossBetweenRenamesRecoversCompleteTemporary(){
    ++suites;reset();AgendaService service;CHECK(service.begin(true,clockNow));
    command(service,R"({"op":"task.add","title":"Primera"})");
    LittleFS.failRenameAt=LittleFS.renameCalls+2;
    CHECK(!command(service,R"({"op":"task.add","title":"Segunda"})")["agenda"]["persisted"].as<bool>());
    CHECK(!LittleFS.exists("/state/agenda.json"));CHECK(LittleFS.exists("/state/agenda.bak"));
    AgendaService reboot;CHECK(reboot.begin(true,clockNow));
    auto reply=command(reboot,R"({"op":"agenda.get"})");
    CHECK(reply["agenda"]["tasks"].size()==2);CHECK(reply["agenda"]["recovered"].as<bool>());
    poll(reboot,5000);CHECK(command(reboot,R"({"op":"agenda.get"})")["agenda"]["persisted"].as<bool>());
}
void legacyMigrationDoesNotShareMediaMetadata(){
    ++suites;reset();pet::agenda::Book legacy;JsonDocument req,reply,stored;
    req["op"]="task.add";req["title"]="Conservar al actualizar";
    CHECK(legacy.command(req.as<JsonVariantConst>(),reply.to<JsonObject>(),current));
    legacy.store(stored.to<JsonObject>());serializeJson(stored,LittleFS.files["/agenda.json"]);
    AgendaService service;CHECK(service.begin(true,clockNow));
    CHECK(command(service,R"({"op":"agenda.get"})")["agenda"]["tasks"].size()==1);
    poll(service,5000);CHECK(LittleFS.exists("/state/agenda.json"));
    LittleFS.files["/agenda.json"]=R"({"title":"Una imagen llamada agenda"})";
    AgendaService reboot;CHECK(reboot.begin(true,clockNow));
    CHECK(command(reboot,R"({"op":"agenda.get"})")["agenda"]["tasks"].size()==1);
}
}
int main(){
    failedWriteRetriesWithoutOtherChanges();interruptedWriteKeepsOldDataAndRecovers();corruptPrimaryFallsBackAndInvalidStateIsReported();pendingRenameAndQuietDoNotLeakNotices();powerLossBetweenRenamesRecoversCompleteTemporary();legacyMigrationDoesNotShareMediaMetadata();
    std::cout<<suites<<" agenda service suites passed, "<<checks<<" assertions (real AgendaService.cpp, simulated flash/clock)\n";
}
