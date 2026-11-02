#include "Agenda.h"
#include <cstdlib>
#include <iostream>

namespace {
unsigned suites=0,checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<__func__<<':'<<__LINE__<<" failed: " #x<<'\n';std::exit(1);}}while(0)
using namespace pet::agenda;
Moment at(uint32_t day=20261102,unsigned minute=600){return {day,minute,1793613600};}
JsonDocument call(Book& book,const char* op,const Moment& now=at()){
    JsonDocument req,out;CHECK(!deserializeJson(req,op));book.command(req.as<JsonVariantConst>(),out.to<JsonObject>(),now);return out;
}
JsonDocument invoke(Book& book,const JsonDocument& request,const Moment& now=at()){
    JsonDocument out;book.command(request.as<JsonVariantConst>(),out.to<JsonObject>(),now);return out;
}
JsonDocument companion(Book& book,const Moment& now=at()){return call(book,R"({"op":"companion.get"})",now);}
void habitsAreDailyIdempotentAndWeekly(){
    ++suites;Book book;
    auto add=call(book,R"({"op":"habit.add","title":"Leer","targetPerWeek":3})");CHECK(add["ok"].as<bool>());CHECK(add["id"].as<unsigned>()==1);
    CHECK(call(book,R"({"op":"habit.add","title":"Leer","targetPerWeek":3})")["id"].as<unsigned>()==1);
    CHECK(!call(book,R"({"op":"habit.add","title":"Mal","targetPerWeek":0})")["ok"].as<bool>());
    CHECK(!call(book,R"({"op":"habit.done","id":1,"done":true})",Moment{})["ok"].as<bool>());
    for(unsigned day=20261102;day<=20261108;++day){
        auto done=call(book,R"({"op":"habit.done","id":1,"done":true})",at(day));CHECK(done["ok"].as<bool>());
        const auto revision=book.revision();
        CHECK(call(book,R"({"op":"habit.done","id":1,"done":true})",at(day))["ok"].as<bool>());CHECK(book.revision()==revision);
    }
    auto state=companion(book,at(20261108));CHECK(state["habits"][0]["weekDone"].as<unsigned>()==7);CHECK(state["habits"][0]["streak"].as<unsigned>()==7);
    state=companion(book,at(20261109));CHECK(state["habits"][0]["weekDone"].as<unsigned>()==0);CHECK(state["habits"][0]["streak"].as<unsigned>()==7);
    CHECK(!state["habits"][0]["goalReached"].as<bool>());
    call(book,R"({"op":"habit.done","id":1,"done":true})",at(20261109));
    call(book,R"({"op":"habit.done","id":1,"done":false})",at(20261109));
    state=companion(book,at(20261109));CHECK(state["habits"][0]["totalDone"].as<unsigned>()==7);CHECK(state["companion"]["totalHabitChecks"].as<unsigned>()==7);
    state=companion(book,at(20261110));CHECK(state["habits"][0]["streak"].as<unsigned>()==0);
    for(unsigned i=1;i<8;++i){JsonDocument req;req["op"]="habit.add";req["title"]="Habito "+std::to_string(i);CHECK(invoke(book,req)["ok"].as<bool>());}
    CHECK(!call(book,R"({"op":"habit.add","title":"Noveno"})")["ok"].as<bool>());
}
void compactedHistoryKeepsLongStreaks(){
    ++suites;Book book;call(book,R"({"op":"habit.add","title":"Constancia","targetPerWeek":7})");uint32_t day=20260101;
    for(unsigned i=0;i<120;++i){CHECK(call(book,R"({"op":"habit.done","id":1,"done":true})",at(day))["ok"].as<bool>());if(i<119)day=tomorrow(day);}
    auto state=companion(book,at(day));CHECK(state["habits"][0]["streak"].as<unsigned>()==120);CHECK(state["habits"][0]["totalDone"].as<unsigned>()==120);
    JsonDocument record;book.store(record.to<JsonObject>());CHECK(record["extras"]["habits"][0]["completed"].size()==56);
    Book loaded;CHECK(loaded.load(record.as<JsonVariantConst>()));state=companion(loaded,at(day));CHECK(state["habits"][0]["streak"].as<unsigned>()==120);
    call(loaded,R"({"op":"habit.done","id":1,"done":false})",at(day));CHECK(companion(loaded,at(day))["habits"][0]["streak"].as<unsigned>()==119);
    CHECK(!call(loaded,R"({"op":"habit.done","id":1,"done":true})",at(20260101))["ok"].as<bool>());
}
void routinePreparationIsAllOrNothing(){
    ++suites;Book book;
    CHECK(call(book,R"({"op":"routine.list"})")["routines"].size()==2);
    auto saved=call(book,R"({"op":"routine.save","title":"Repaso","focus":true,"overview":false,"tasks":["Algebra","Historia"]})");
    CHECK(saved["ok"].as<bool>());CHECK(saved["id"].as<unsigned>()==3);
    CHECK(!call(book,R"({"op":"routine.save","title":"Conflicto","focus":true,"overview":true,"tasks":[]})")["ok"].as<bool>());
    CHECK(!call(book,R"({"op":"routine.save","title":"Duplicada","focus":false,"overview":false,"tasks":["A","A"]})")["ok"].as<bool>());
    for(unsigned i=0;i<23;++i){JsonDocument req;req["op"]="task.add";req["title"]="Pendiente "+std::to_string(i);CHECK(invoke(book,req)["ok"].as<bool>());}
    const auto revision=book.revision();
    auto prepared=call(book,R"({"op":"routine.prepare","id":3})");CHECK(!prepared["ok"].as<bool>());CHECK(prepared["agenda"]["tasks"].size()==23);CHECK(book.revision()==revision);
    call(book,R"({"op":"task.delete","id":1})");prepared=call(book,R"({"op":"routine.prepare","id":3})");CHECK(prepared["ok"].as<bool>());CHECK(prepared["agenda"]["tasks"].size()==24);
    CHECK(prepared["plan"]["focus"].as<bool>());CHECK(prepared["plan"]["tasks"].size()==2);
    const auto readyRevision=book.revision();CHECK(call(book,R"({"op":"routine.prepare","id":3})")["ok"].as<bool>());CHECK(book.revision()==readyRevision);
    CHECK(!call(book,R"({"op":"routine.prepare","id":3})",Moment{})["ok"].as<bool>());
    JsonDocument stored;book.store(stored.to<JsonObject>());Book loaded;CHECK(loaded.load(stored.as<JsonVariantConst>()));CHECK(call(loaded,R"({"op":"routine.plan","id":3})")["plan"]["tasks"].size()==2);
}
void memoryAndStoriesRespectChoicesAndDates(){
    ++suites;Book book;book.observe({},at());std::string text;
    CHECK(!book.companionNotice(text,at(),0));CHECK(book.companionNotice(text,at(),1));CHECK(text.find("Buenos dias")!=std::string::npos);
    CHECK(!book.companionNotice(text,at(),1));
    CHECK(call(book,R"({"op":"companion.options","name":"Luna","nickname":"Max","greetings":false,"celebrations":false,"stories":true})")["ok"].as<bool>());
    CHECK(!call(book,R"({"op":"companion.options","name":""})")["ok"].as<bool>());
    for(unsigned i=0;i<6;++i){
        const auto now=at(20261102+i,780);book.observe({},now);
        CHECK(!book.companionNotice(text,now,39));CHECK(book.companionNotice(text,now,40));CHECK(text.find("Bitacora "+std::to_string(i+1)+"/6")!=std::string::npos);
        CHECK(!book.companionNotice(text,now,100));
        JsonDocument stored;book.store(stored.to<JsonObject>());Book restored;CHECK(restored.load(stored.as<JsonVariantConst>()));book=std::move(restored);
    }
    auto state=companion(book,at(20261107));CHECK(state["companion"]["daysTogether"].as<unsigned>()==6);CHECK(state["companion"]["storyChapter"].as<unsigned>()==0);
    CHECK(state["companion"]["name"].as<std::string>()=="Luna");
    call(book,R"({"op":"companion.options","stories":false})");CHECK(!book.companionNotice(text,at(20261108,780),100));
}
void legacyMigrationAndMalformedExtras(){
    ++suites;Book book;call(book,R"({"op":"task.add","title":"Anterior"})");call(book,R"({"op":"task.done","id":1,"done":true})");
    book.observe({60000,0,1},at());JsonDocument record;book.store(record.to<JsonObject>());record.as<JsonObject>().remove("extras");
    Book migrated;CHECK(migrated.load(record.as<JsonVariantConst>()));auto state=companion(migrated);CHECK(state["companion"]["totalTasks"].as<unsigned>()==1);CHECK(state["companion"]["totalBlocks"].as<unsigned>()==1);
    CHECK(state["routines"].size()==2);call(migrated,R"({"op":"task.done","id":1,"done":false})");CHECK(companion(migrated)["companion"]["totalTasks"].as<unsigned>()==0);
    migrated.store(record.to<JsonObject>());record["extras"]["memory"]["storyChapter"]=7;CHECK(!migrated.load(record.as<JsonVariantConst>()));
    CHECK(companion(migrated)["companion"]["name"].as<std::string>()=="MiniPet");
}
void weeklyCelebrationIsNotRepeated(){
    ++suites;Book book;std::string text;
    call(book,R"({"op":"companion.options","greetings":false,"stories":false})");call(book,R"({"op":"habit.add","title":"Leer","targetPerWeek":1})");
    call(book,R"({"op":"habit.done","id":1,"done":true})");CHECK(book.companionNotice(text,at(),20));CHECK(text.find("Meta semanal")!=std::string::npos);CHECK(!book.companionNotice(text,at(),20));
    call(book,R"({"op":"habit.done","id":1,"done":false})");call(book,R"({"op":"habit.done","id":1,"done":true})");CHECK(!book.companionNotice(text,at(),100));
}
void largestAllowedStateFitsStorage(){
    ++suites;Book book;
    for(unsigned i=0;i<24;++i){JsonDocument req;req["op"]="task.add";req["title"]=std::string(95,'"')+char('A'+i);CHECK(invoke(book,req)["ok"].as<bool>());}
    for(unsigned i=0;i<12;++i){JsonDocument req;req["op"]="reminder.add";req["title"]=std::string(95,'"')+char('A'+i);req["minute"]=601+i;CHECK(invoke(book,req)["ok"].as<bool>());}
    for(unsigned i=0;i<8;++i){JsonDocument req;req["op"]="habit.add";req["title"]=std::string(95,'"')+char('A'+i);CHECK(invoke(book,req)["ok"].as<bool>());}
    for(unsigned i=1;i<=6;++i){
        JsonDocument req;req["op"]="routine.save";if(i<=2)req["id"]=i;req["title"]=std::string(95,'"')+char('A'+i);req["focus"]=false;req["overview"]=false;
        auto tasks=req["tasks"].to<JsonArray>();for(unsigned j=0;j<6;++j)tasks.add(std::string(95,'"')+char('a'+j));
        CHECK(invoke(book,req)["ok"].as<bool>());
    }
    uint32_t day=20261102;
    for(unsigned n=0;n<56;++n){
        for(unsigned i=1;i<=8;++i){JsonDocument req;req["op"]="habit.done";req["id"]=i;req["done"]=true;CHECK(invoke(book,req,at(day))["ok"].as<bool>());}
        book.observe({60000u*(n+1),30000u*(n+1),n+1},at(day));day=tomorrow(day);
    }
    JsonDocument record;book.store(record.to<JsonObject>());CHECK(measureJson(record)<32768);
    Book loaded;CHECK(loaded.load(record.as<JsonVariantConst>()));CHECK(companion(loaded,at(day))["habits"].size()==8);
}
}
void testCompanion(unsigned& totalSuites,unsigned& totalChecks){
    habitsAreDailyIdempotentAndWeekly();compactedHistoryKeepsLongStreaks();routinePreparationIsAllOrNothing();memoryAndStoriesRespectChoicesAndDates();legacyMigrationAndMalformedExtras();weeklyCelebrationIsNotRepeated();largestAllowedStateFitsStorage();
    totalSuites+=suites;totalChecks+=checks;
}
#ifdef COMPANION_STANDALONE
int main(){unsigned count=0,total=0;testCompanion(count,total);std::cout<<count<<" companion suites passed, "<<total<<" assertions\n";}
#endif
