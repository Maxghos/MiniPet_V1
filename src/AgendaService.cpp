#include "AgendaService.h"
#include <LittleFS.h>
#include <time.h>

pet::agenda::Moment agendaMoment(){
    const time_t epoch=time(nullptr);tm local{};
    if(epoch<1704067200||!localtime_r(&epoch,&local))return {};
    return {uint32_t((local.tm_year+1900)*10000+(local.tm_mon+1)*100+local.tm_mday),unsigned(local.tm_hour*60+local.tm_min),int64_t(epoch)};
}
bool AgendaService::begin(bool storage){
    mutex_=xSemaphoreCreateMutex();storage_=storage;if(!mutex_)return false;
    if(storage_){
        File file=LittleFS.open("/agenda.json","r");
        if(file&&file.size()<=16384){JsonDocument doc;if(!deserializeJson(doc,file))book_.load(doc.as<JsonVariantConst>());}
        if(file)file.close();
        LittleFS.remove("/agenda.tmp");
    }
    scenes_=book_.options().scenes;persisted_=storage_;lastSave_=millis();return true;
}
bool AgendaService::save(){
    lastAttempt_=millis();persisted_=false;
    if(!storage_)return false;
    JsonDocument doc;book_.store(doc.to<JsonObject>());File file=LittleFS.open("/agenda.tmp","w");
    if(!file)return false;
    const auto expected=measureJson(doc);const auto bytes=serializeJson(doc,file);file.flush();const auto size=file.size();file.close();
    if(bytes!=expected||size!=expected||!LittleFS.rename("/agenda.tmp","/agenda.json"))return false;
    persisted_=true;dirtyStats_=false;lastSave_=millis();return true;
}
void AgendaService::enqueue(const std::string& text,uint8_t kind,uint32_t ttl){
    AgendaNotice n{nextNotice_++,millis(),ttl,text,kind,0};
    if(telegram_.size()>=16)telegram_.pop_front();telegram_.push_back(n);
    if(kind!=2){
        if(display_.size()>=8)display_.pop_front();
        if(kind==1)n.text="Resumen del dia listo.\nRevisalo en la app\no en Telegram.";
        display_.push_back(n);
    }
}
void AgendaService::poll(const pet::study::Snapshot& study,const pet::agenda::Totals& totals,const pet::Settings& settings){
    const uint32_t now=millis();if(now-lastPoll_<1000||!mutex_||xSemaphoreTake(mutex_,pdMS_TO_TICKS(2))!=pdTRUE)return;
    lastPoll_=now;const auto moment=agendaMoment();const auto before=book_.revision();book_.observe(totals,moment);
    if(moment.valid()&&(totals.focusMs!=observed_.focusMs||totals.breakMs!=observed_.breakMs||totals.blocks!=observed_.blocks))dirtyStats_=true;
    observed_=totals;
    const bool quiet=moment.valid()&&settings.quiet(moment.minute/60);
    for(const auto& notice:book_.due(moment,quiet))enqueue(notice.text,notice.summary?1:0,notice.summary?3600000:900000);
    if(seenStudy_&&study.eventId!=studyEvent_&&study.eventAgeMs<10000&&!quiet){
        if(study.event==pet::study::Event::FocusDone)enqueue(study.phase==pet::study::Phase::LongBreak?
            "Ciclo completado. Te toca un descanso largo.":"Bloque completado. Es momento de descansar.",2,120000);
        else if(study.event==pet::study::Event::BreakDone)enqueue(study.state==pet::study::State::Waiting?
            "Termino el descanso. MiniPet te espera: pulsa Continuar cuando estes listo.":"Termino el descanso. Empieza el siguiente bloque de estudio.",2,120000);
    }
    seenStudy_=true;studyEvent_=study.eventId;
    if((before!=book_.revision()||(dirtyStats_&&now-lastSave_>=60000))&&(persisted_||now-lastAttempt_>=5000))save();
    xSemaphoreGive(mutex_);
}
void AgendaService::execute(JsonVariantConst req,JsonObject reply){
    if(!mutex_||xSemaphoreTake(mutex_,pdMS_TO_TICKS(2000))!=pdTRUE){reply["ok"]=false;reply["error"]="Agenda ocupada";return;}
    const auto before=book_.revision();book_.command(req,reply,agendaMoment());
    if(book_.revision()!=before)save();
    scenes_=book_.options().scenes;reply["agenda"]["persisted"]=persisted_;xSemaphoreGive(mutex_);
}
bool AgendaService::takeDisplay(std::string& out){
    if(!mutex_||xSemaphoreTake(mutex_,pdMS_TO_TICKS(2))!=pdTRUE)return false;
    bool result=false;
    while(!display_.empty()){
        const auto n=display_.front();display_.pop_front();
        if(uint32_t(millis()-n.created)<n.ttl){out=n.text;result=true;break;}
    }
    xSemaphoreGive(mutex_);return result;
}
bool AgendaService::peekTelegram(AgendaNotice& out){
    if(!mutex_||xSemaphoreTake(mutex_,pdMS_TO_TICKS(20))!=pdTRUE)return false;
    while(!telegram_.empty()&&(uint32_t(millis()-telegram_.front().created)>=telegram_.front().ttl||telegram_.front().attempts>=3))telegram_.pop_front();
    const bool result=!telegram_.empty();if(result)out=telegram_.front();xSemaphoreGive(mutex_);return result;
}
void AgendaService::finishTelegram(uint32_t id,bool delivered){
    if(!mutex_||xSemaphoreTake(mutex_,pdMS_TO_TICKS(20))!=pdTRUE)return;
    if(!telegram_.empty()&&telegram_.front().id==id){if(delivered)telegram_.pop_front();else ++telegram_.front().attempts;}
    xSemaphoreGive(mutex_);
}
void AgendaService::clearTelegram(){if(mutex_&&xSemaphoreTake(mutex_,pdMS_TO_TICKS(20))==pdTRUE){telegram_.clear();xSemaphoreGive(mutex_);}}
