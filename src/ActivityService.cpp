#include "ActivityService.h"
#include "Screen.h"
#include <Arduino.h>

bool ActivityService::begin(StudyService& study,AgendaService& agenda){study_=&study;agenda_=&agenda;mutex_=xSemaphoreCreateMutex();return mutex_!=nullptr;}
void ActivityService::status(JsonObject out,uint32_t now){
    const auto s=game_.snapshot(now);out["active"]=game_.active();out["kind"]=pet::games::kindName(s.kind);
    out["state"]=pet::games::stateName(s.state);out["score"]=s.score;out["lives"]=s.lives;out["remainingMs"]=s.remainingMs;
    out["reason"]=s.sensorLost?"Coloca la mano entre 15 y 80 cm":"";out["inputValid"]=s.inputValid;
}
void ActivityService::execute(JsonVariantConst req,JsonObject reply){
    if(!mutex_||xSemaphoreTake(mutex_,pdMS_TO_TICKS(2000))!=pdTRUE){reply["ok"]=false;reply["error"]="Actividad ocupada";return;}
    reply["ok"]=true;const std::string op=req["op"]|"";
    if(op=="game.get"){}
    else if(op=="game.stop"){game_.stop();active_=false;interaction_=true;}
    else if(op=="game.start"){
        pet::games::Kind kind;
        if(busy_||study_->active()){reply["ok"]=false;reply["error"]="Cierra ajustes y termina estudio antes de jugar.";}
        else if(!pet::games::parseKind(req["kind"]|"",kind)||!game_.begin(millis(),kind)){reply["ok"]=false;reply["error"]="Juego invalido o partida ya activa.";}
        else{active_=true;interaction_=true;}
    }else if(op=="routine.run"){
        if(busy_||game_.active()){reply["ok"]=false;reply["error"]="Cierra ajustes o el juego antes de ejecutar una rutina.";}
        else{
            study_->executeRoutine(*agenda_,req,reply);
            if(reply["ok"]|false){overview_=reply["plan"]["overview"]|false;interaction_=true;}
        }
    }else{reply["ok"]=false;reply["error"]="Actividad desconocida";}
    status(reply["game"].to<JsonObject>(),millis());xSemaphoreGive(mutex_);
}
bool ActivityService::startLocal(pet::games::Kind kind){
    if(!mutex_||!study_||study_->active()||xSemaphoreTake(mutex_,pdMS_TO_TICKS(20))!=pdTRUE)return false;
    const bool ok=game_.begin(millis(),kind);active_=game_.active();if(ok)interaction_=true;xSemaphoreGive(mutex_);return ok;
}
void ActivityService::stop(){if(mutex_&&xSemaphoreTake(mutex_,pdMS_TO_TICKS(20))==pdTRUE){game_.stop();active_=false;xSemaphoreGive(mutex_);}}
void ActivityService::poll(uint32_t now,bool valid,float cm){
    if(!mutex_||xSemaphoreTake(mutex_,pdMS_TO_TICKS(2))!=pdTRUE)return;
    if(study_->active())game_.stop();else game_.update(now,valid,cm);
    active_=game_.active();xSemaphoreGive(mutex_);
}
void ActivityService::render(Screen& screen,uint32_t now){
    if(mutex_&&xSemaphoreTake(mutex_,pdMS_TO_TICKS(2))==pdTRUE){screen.game(game_,now);xSemaphoreGive(mutex_);}
}
