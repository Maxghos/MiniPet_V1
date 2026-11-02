#include "StudyService.h"
#include "AgendaService.h"
#include <Arduino.h>
#include <Preferences.h>

bool StudyService::begin() {
    mutex_=xSemaphoreCreateMutex();if(!mutex_)return false;
    Preferences p;
    if(p.begin("minipet-study",true)){
        const String data=p.getString("state","");p.end();
        if(data.length()&&data.length()<=4096){
            JsonDocument doc;pet::study::Record record;
            if(!deserializeJson(doc,data)&&pet::study::readRecord(doc.as<JsonVariantConst>(),record))
                timer_.restore(record,millis());
        }
    }
    const auto now=millis();const auto value=timer_.snapshot(now);
    accounting_.baseline(value);
    active_=value.active();savedRevision_=value.revision;lastSave_=now;lastAttempt_=now-5000;
    return true;
}
bool StudyService::save(uint32_t now) {
    lastAttempt_=now;JsonDocument doc;pet::study::writeRecord(doc.to<JsonObject>(),timer_.record(now));
    String data;serializeJson(doc,data);Preferences p;
    persisted_=p.begin("minipet-study",false);
    if(persisted_){persisted_=p.putString("state",data)==data.length();p.end();}
    if(persisted_){savedRevision_=timer_.snapshot(now).revision;lastSave_=now;}
    return persisted_;
}
void StudyService::poll(pet::study::Snapshot& output) {
    if(!mutex_||xSemaphoreTake(mutex_,pdMS_TO_TICKS(2))!=pdTRUE)return;
    const uint32_t now=millis();
    timer_.update(now);const auto value=timer_.snapshot(now);active_=value.active();
    accounting_.observe(value);
    const bool dirty=value.revision!=savedRevision_||
        (value.state==pet::study::State::Running&&pet::elapsed(now,lastSave_)>=60000);
    if(dirty&&(persisted_||pet::elapsed(now,lastAttempt_)>=5000))save(now);
    output=value;
    xSemaphoreGive(mutex_);
}
bool StudyService::read(pet::study::Snapshot& out) {
    if(!mutex_||xSemaphoreTake(mutex_,pdMS_TO_TICKS(2))!=pdTRUE)return false;
    const uint32_t now=millis();
    timer_.update(now);out=timer_.snapshot(now);accounting_.observe(out);active_=out.active();xSemaphoreGive(mutex_);return true;
}
bool StudyService::totals(pet::agenda::Totals& out){
    if(!mutex_||xSemaphoreTake(mutex_,pdMS_TO_TICKS(2))!=pdTRUE)return false;
    out=accounting_.totals();xSemaphoreGive(mutex_);return true;
}
void StudyService::execute(JsonVariantConst request,JsonObject reply) {
    if(!mutex_||xSemaphoreTake(mutex_,pdMS_TO_TICKS(2000))!=pdTRUE){
        reply["ok"]=false;reply["error"]="Estudio ocupado. Vuelve a intentar.";return;
    }
    const uint32_t now=millis();const auto before=timer_.snapshot(now).revision;
    timer_.update(now);accounting_.observe(timer_.snapshot(now));
    pet::study::command(timer_,request,reply,now);
    const auto value=timer_.snapshot(now);active_=value.active();
    accounting_.observe(value);
    if(value.revision!=before||(value.revision!=savedRevision_&&pet::elapsed(now,lastAttempt_)>=5000))save(now);
    reply["study"]["persisted"]=persisted_;
    xSemaphoreGive(mutex_);
}
bool StudyService::control(const char* operation,uint32_t expectedRevision,const pet::study::Config* config) {
    JsonDocument req,reply;req["op"]=operation;req["expectedRevision"]=expectedRevision;
    if(config)pet::study::writeConfig(req["config"].to<JsonObject>(),*config);
    execute(req.as<JsonVariantConst>(),reply.to<JsonObject>());return reply["ok"]|false;
}
void StudyService::executeRoutine(AgendaService& agenda,JsonVariantConst request,JsonObject reply){
    if(!mutex_||xSemaphoreTake(mutex_,pdMS_TO_TICKS(2000))!=pdTRUE){reply["ok"]=false;reply["error"]="Estudio ocupado";return;}
    timer_.update(millis());accounting_.observe(timer_.snapshot(millis()));
    if(timer_.snapshot(millis()).active()){reply["ok"]=false;reply["error"]="Termina la sesion actual antes de ejecutar una rutina.";}
    else{
        JsonDocument prepare;prepare["op"]="routine.prepare";prepare["id"]=request["id"];
        agenda.execute(prepare.as<JsonVariantConst>(),reply);
        if((reply["ok"]|false)&&(reply["plan"]["focus"]|false)){
            const auto now=millis();timer_.start(now);accounting_.observe(timer_.snapshot(now));active_=true;save(now);
        }
    }
    pet::study::writeSnapshot(reply["study"].to<JsonObject>(),timer_.snapshot(millis()));reply["study"]["persisted"]=persisted_;
    xSemaphoreGive(mutex_);
}
