#include "Agenda.h"
#include "TelegramProtocol.h"
#include "LittleActivities.h"
#include "WeatherBehavior.h"
#include <cstdlib>
#include <iostream>
#include <set>

namespace {
unsigned groups=0,checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<__func__<<':'<<__LINE__<<" failed: " #x<<'\n';std::exit(1);}}while(0)
using namespace pet;
agenda::Moment moment(uint32_t day=20261102,unsigned minute=10*60){return {day,minute,1793613600};}
JsonDocument request(agenda::Book& book,const char* text,agenda::Moment at=moment()){
    JsonDocument req,reply;CHECK(!deserializeJson(req,text));book.command(req.as<JsonVariantConst>(),reply.to<JsonObject>(),at);return reply;
}
void taskLifecycleAndPersistence(){
    ++groups;agenda::Book book;
    auto add=request(book,"{\"op\":\"task.add\",\"title\":\"Leer álgebra\"}");CHECK(add["ok"].as<bool>());CHECK(add["id"].as<unsigned>()==1);
    CHECK(request(book,"{\"op\":\"task.add\",\"title\":\"Leer álgebra\"}")["id"].as<unsigned>()==1);
    auto done=request(book,"{\"op\":\"task.done\",\"id\":1,\"done\":true}");CHECK(done["agenda"]["days"][0]["tasks"].as<unsigned>()==1);
    done=request(book,"{\"op\":\"task.done\",\"id\":1,\"done\":true}");CHECK(done["agenda"]["days"][0]["tasks"].as<unsigned>()==1);
    auto undo=request(book,"{\"op\":\"task.done\",\"id\":1,\"done\":false}");CHECK(undo["agenda"]["days"][0]["tasks"].as<unsigned>()==0);
    auto moved=request(book,"{\"op\":\"task.today\",\"id\":1}",moment(20261103));CHECK(moved["agenda"]["tasks"][0]["day"].as<unsigned>()==20261103);
    JsonDocument stored;book.store(stored.to<JsonObject>());agenda::Book restored;CHECK(restored.load(stored.as<JsonVariantConst>()));
    CHECK(request(restored,"{\"op\":\"agenda.get\"}")["agenda"]["tasks"].size()==1);
    stored["value"]["tasks"][0]["day"]=20260230;CHECK(!restored.load(stored.as<JsonVariantConst>()));
    CHECK(request(book,"{\"op\":\"task.delete\",\"id\":1}")["ok"].as<bool>());
    CHECK(request(book,"{\"op\":\"task.delete\",\"id\":1}")["ok"].as<bool>());
    for(unsigned i=0;i<24;++i){const std::string text="{\"op\":\"task.add\",\"title\":\"Pendiente "+std::to_string(i)+"\"}";CHECK(request(book,text.c_str())["ok"].as<bool>());}
    CHECK(!request(book,"{\"op\":\"task.add\",\"title\":\"Otra\"}")["ok"].as<bool>());
    CHECK(!request(book,"{\"op\":\"task.add\",\"title\":\"   \"}")["ok"].as<bool>());
    CHECK(!request(book,"{\"op\":\"task.add\",\"title\":\"Otra\",\"day\":\"20261103\"}")["ok"].as<bool>());
}
void reminderSchedulesAndQuiet(){
    ++groups;agenda::Book book;
    CHECK(request(book,"{\"op\":\"reminder.add\",\"title\":\"Agua\",\"minute\":601,\"daily\":true}")["ok"].as<bool>());
    CHECK(book.due(moment(20261102,600),false).empty());
    CHECK(book.due(moment(20261102,601),false).size()==1);CHECK(book.due(moment(20261102,601),false).empty());
    CHECK(book.due(moment(20261103,601),true).empty());CHECK(book.due(moment(20261103,602),false).empty());
    CHECK(book.due(moment(20261104,620),false).empty()); // stale reminders do not flood a reconnect
    CHECK(book.due(moment(20261105,601),false).size()==1);
    auto once=request(book,"{\"op\":\"reminder.add\",\"title\":\"Una vez\",\"minute\":599}");
    CHECK(once["agenda"]["reminders"][1]["day"].as<unsigned>()==20261103);
    CHECK(book.due(moment(20261103,599),false).size()==1);CHECK(book.due(moment(20261104,599),false).empty());
    CHECK(!request(book,"{\"op\":\"reminder.add\",\"title\":\"Pasado\",\"day\":20261101,\"minute\":601}")["ok"].as<bool>());
    CHECK(book.due(agenda::Moment{},false).empty());
    agenda::Book summary;CHECK(summary.due(moment(20261102,1230),false).size()==1);
    CHECK(summary.due(moment(20261102,1231),false).empty());CHECK(summary.due(moment(20261101,1230),false).empty());
    CHECK(summary.due(moment(20261103,1230),true).empty());CHECK(summary.due(moment(20261103,1231),false).empty());
    CHECK(agenda::tomorrow(20240228)==20240229);CHECK(agenda::tomorrow(20240229)==20240301);
    CHECK(agenda::tomorrow(20261231)==20270101);CHECK(!agenda::validDay(20260229));
}
void accountingAcrossSessionsAndDates(){
    ++groups;agenda::Accounting accounting;study::Timer timer;accounting.baseline(timer.snapshot(0));
    timer.start(0);timer.update(60000);accounting.observe(timer.snapshot(60000));
    timer.stop(90000);accounting.observe(timer.snapshot(90000));
    timer.start(100000);accounting.observe(timer.snapshot(100000));timer.pause(160000);accounting.observe(timer.snapshot(160000));
    CHECK(accounting.totals().focusMs==150000);CHECK(accounting.totals().blocks==0);
    agenda::Book book;book.observe(accounting.totals(),moment());CHECK(book.summary(20261102).find("Estudio: 2 min")!=std::string::npos);
    book.observe(accounting.totals(),moment());CHECK(book.summary(20261102).find("Estudio: 2 min")!=std::string::npos);
    auto next=accounting.totals();next.focusMs+=60000;book.observe(next,moment(20261103));
    CHECK(book.summary(20261103).find("Estudio: 1 min")!=std::string::npos);
    JsonDocument data;book.store(data.to<JsonObject>());agenda::Book restored;CHECK(restored.load(data.as<JsonVariantConst>()));
    CHECK(restored.summary(20261102)==book.summary(20261102));
}
JsonDocument update(int id,int64_t user,const char* text,int64_t date=1793613600){
    JsonDocument doc;doc["update_id"]=id;auto m=doc["message"].to<JsonObject>();m["chat"]["id"]=user;m["chat"]["type"]="private";
    m["from"]["id"]=user;m["from"]["is_bot"]=false;m["date"]=date;m["text"]=text;return doc;
}
void pairingOwnerAndReplay(){
    ++groups;telegram::Gate gate;gate.open("392761",0);
    auto msg=update(1,123,"/vincular 392761");msg["message"]["chat"]["type"]="group";
    CHECK(gate.accept(msg.as<JsonVariantConst>(),1793613600,100).kind==telegram::Kind::Ignore);
    msg=update(2,123,"/vincular 392761",1793613000);CHECK(gate.accept(msg.as<JsonVariantConst>(),1793613600,200).kind==telegram::Kind::Ignore);
    msg=update(3,123,"/vincular 392761");msg["message"]["forward_origin"]["type"]="user";
    CHECK(gate.accept(msg.as<JsonVariantConst>(),1793613600,300).kind==telegram::Kind::Ignore);
    msg=update(4,123,"/vincular 392761");CHECK(gate.accept(msg.as<JsonVariantConst>(),1793613600,400).kind==telegram::Kind::Paired);
    CHECK(gate.chat()==123);CHECK(!gate.pairing(400));
    msg=update(5,456,"/pomodoro");CHECK(gate.accept(msg.as<JsonVariantConst>(),1793613600,500).kind==telegram::Kind::Ignore);
    msg=update(6,123,"/pomodoro");CHECK(gate.accept(msg.as<JsonVariantConst>(),1793613600,600).kind==telegram::Kind::Command);
    CHECK(gate.accept(msg.as<JsonVariantConst>(),1793613600,700).kind==telegram::Kind::Ignore);
    gate.drain(100);msg=update(99,123,"/continuar");CHECK(gate.accept(msg.as<JsonVariantConst>(),1793613600,800).kind==telegram::Kind::Ignore);
    telegram::Gate attempts;attempts.open("392761",0);
    for(int i=0;i<5;++i){msg=update(i,123,"/vincular 000000");CHECK(attempts.accept(msg.as<JsonVariantConst>(),1793613600,100).kind==telegram::Kind::RejectedPair);}
    CHECK(!attempts.pairing(101));msg=update(6,123,"/vincular 392761");CHECK(attempts.accept(msg.as<JsonVariantConst>(),1793613600,200).kind==telegram::Kind::Ignore);
    attempts.open("392761",0);CHECK(!attempts.pairing(120000));
    CHECK(!attempts.pairing(0)); // an expired PIN must stay closed after millis() wraps
}
void commandAllowlistAndButtons(){
    ++groups;JsonDocument req;std::string error;unsigned minute;
    CHECK(telegram::validToken(std::string("123456789")+":"+std::string(35,'A')));CHECK(!telegram::validToken(""));
    CHECK(!telegram::validToken(std::string("123456789")+":"+std::string(35,'/')));
    CHECK(telegram::parseClock("23:59",minute)&&minute==1439);CHECK(!telegram::parseClock("24:00",minute));
    CHECK(telegram::command("/recordar 14:30 Repasar algebra",false,req.to<JsonObject>(),error));CHECK(req["minute"].as<unsigned>()==870);
    CHECK(std::string(req["op"].as<const char*>())=="reminder.add");
    CHECK(!telegram::command("/telegram.configure secreto",false,req.to<JsonObject>(),error));
    CHECK(telegram::command("/habito 4 Leer",false,req.to<JsonObject>(),error));CHECK(req["targetPerWeek"].as<unsigned>()==4);
    CHECK(!telegram::command("/habito 8 Leer",false,req.to<JsonObject>(),error));
    CHECK(telegram::command("/rutina 3",false,req.to<JsonObject>(),error));CHECK(req["id"].as<unsigned>()==3);
    CHECK(telegram::command("/jugar punteria",false,req.to<JsonObject>(),error));CHECK(std::string(req["kind"].as<const char*>())=="target");
    CHECK(!telegram::command("/jugar cualquiercosa",false,req.to<JsonObject>(),error));
    CHECK(!telegram::command("p:stop:4294967296",true,req.to<JsonObject>(),error));
    CHECK(telegram::command("p:pause:42",true,req.to<JsonObject>(),error));CHECK(req["expectedRevision"].as<unsigned>()==42);
    CHECK(telegram::command("q:stop:42",true,req.to<JsonObject>(),error));CHECK(std::string(req["op"].as<const char*>())=="confirm");
    CHECK(telegram::command("/hecho 9",false,req.to<JsonObject>(),error));CHECK(req["id"].as<unsigned>()==9&&req["done"].as<bool>());
    JsonDocument study,keys;study["active"]=true;study["state"]="running";study["revision"]=42;
    telegram::keyboard(keys.to<JsonObject>(),study.as<JsonVariantConst>());
    CHECK(std::string(keys["inline_keyboard"][0][0]["callback_data"].as<const char*>())=="p:pause:42");
    CHECK(std::string(keys["inline_keyboard"][1][1]["callback_data"].as<const char*>())=="q:stop:42");
    JsonDocument publicReply;telegram::publicState(publicReply.to<JsonObject>(),telegram::PublicState{});
    for(const char* key:{"token","tokenValue","chat","user","pin"})CHECK(publicReply[key].isNull());
    telegram::Gate gate;gate.owner(123,123);JsonDocument callback;callback["update_id"]=1;
    auto cb=callback["callback_query"].to<JsonObject>();cb["from"]["id"]=456;cb["from"]["is_bot"]=false;
    cb["message"]["chat"]["id"]=123;cb["message"]["chat"]["type"]="private";cb["data"]="p:stop:42";cb["id"]="test";
    CHECK(gate.accept(callback.as<JsonVariantConst>(),1793613600,0).kind==telegram::Kind::Ignore);
    callback["update_id"]=2;cb["from"]["id"]=123;
    CHECK(gate.accept(callback.as<JsonVariantConst>(),1793613600,0).kind==telegram::Kind::Command);
}
void scenesAndBudget(){
    ++groups;FaceRenderer faces(123);Canvas out;
    for(unsigned activity=0;activity<unsigned(LittleActivity::Count);++activity){
        std::set<std::array<uint8_t,512>> frames;
        for(Tick age=0;age<2200;age+=100){drawLittleActivity(faces,out,LittleActivity(activity),10000+age,age,50);frames.insert(out.bytes);}
        CHECK(frames.size()>5);
    }
    FaceRenderer reader(123);drawLittleActivity(reader,out,LittleActivity::Reading,10000,0,50);
    unsigned left=0,right=0;for(int y=0;y<20;++y)for(int x=15;x<45;++x){left+=static_cast<const Canvas&>(out).pixel(x,y);right+=static_cast<const Canvas&>(out).pixel(x+68,y);}
    CHECK(left>5&&right>5); // reading must show eyes, not the PIR alert triangle
    Settings s;s.messages=0;s.weatherReactions=0;AmbientDirector director;director.begin(0);
    CHECK(director.poll(3000000,s,true,false,false,0,false,true)==Ambient::Scene);
    director.note(3000000,2200,s,0);CHECK(director.poll(3001000,s,true,false,false,0,false,true)==Ambient::None);
    CHECK(director.poll(6000000,s,false,false,false,0,false,true)==Ambient::None);
}
void telegramBackoffAndLongIdle(){
    ++groups;telegram::RetryWindow retry;
    CHECK(retry.remaining(100)==0);retry.failure(100,429,30);
    CHECK(retry.remaining(100)==30000);CHECK(retry.remaining(30099)==1);CHECK(retry.remaining(30100)==0);
    CHECK(retry.remaining(100)==0); // an expired cooldown must not return a counter cycle later
    retry.failure(40000,429,1);CHECK(retry.remaining(40000)==3000);
    retry.failure(41000,401);CHECK(retry.remaining(41000)==60000);
    retry.defer(42000,1000);CHECK(retry.remaining(42000)==59000); // a later call cannot shorten a server wait
    retry.failure(43000,429,100000);CHECK(retry.remaining(43000)==600000);
    retry.clear();CHECK(retry.remaining(44000)==0);
    const Tick start=UINT32_MAX-1000;retry.failure(start,500);
    CHECK(retry.remaining(start+5000)==10000);CHECK(retry.remaining(start+15000)==0);
    telegram::Gate gate;gate.owner(123,123);constexpr int64_t epoch=1793613600,week=7*86400;
    auto msg=update(9000,123,"/estudio",epoch);CHECK(gate.accept(msg.as<JsonVariantConst>(),epoch,0).kind==telegram::Kind::Command);
    CHECK(gate.offset(epoch+week-1)==9001);CHECK(gate.offset(epoch+week)==0);
    msg=update(12,123,"/estudio",epoch+week);CHECK(gate.accept(msg.as<JsonVariantConst>(),epoch+week,0).kind==telegram::Kind::Command);
    CHECK(gate.offset(epoch+week)==13);CHECK(gate.accept(msg.as<JsonVariantConst>(),epoch+week,0).kind==telegram::Kind::Ignore);
    gate.drain(3,epoch+week*2);CHECK(gate.offset(epoch+week*2)==4);
    msg=update(4,123,"/estudio",epoch);CHECK(gate.accept(msg.as<JsonVariantConst>(),epoch+week*2,0).kind==telegram::Kind::Ignore);
}
void unchangedAgendaDoesNotWearFlash(){
    ++groups;agenda::Book book;
    request(book,R"({"op":"task.add","title":"Leer"})");const auto revision=book.revision();
    CHECK(request(book,R"({"op":"task.today","id":1})")["ok"].as<bool>());CHECK(book.revision()==revision);
    CHECK(request(book,R"({"op":"agenda.options","dailySummary":true,"summaryMinute":1230,"scenes":true})")["ok"].as<bool>());
    CHECK(book.revision()==revision);
}
}
void testAdditional(unsigned& suites,unsigned& assertions){
    taskLifecycleAndPersistence();reminderSchedulesAndQuiet();accountingAcrossSessionsAndDates();pairingOwnerAndReplay();commandAllowlistAndButtons();scenesAndBudget();telegramBackoffAndLongIdle();unchangedAgendaDoesNotWearFlash();suites+=groups;assertions+=checks;
}
