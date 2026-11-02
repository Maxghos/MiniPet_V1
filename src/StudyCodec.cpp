#include "StudyCodec.h"
#include <cstring>

namespace pet::study {
void writeConfig(JsonObject out,const Config& c) {
    out["focusMinutes"]=c.focusMinutes;out["shortBreakMinutes"]=c.shortBreakMinutes;
    out["longBreakMinutes"]=c.longBreakMinutes;out["blocksBeforeLong"]=c.blocksBeforeLong;
    out["autoBreak"]=c.autoBreak;out["autoFocus"]=c.autoFocus;
}
bool readConfig(JsonVariantConst input,Config& config) {
    if(!input.is<JsonObjectConst>())return false;
    Config c=config;
    for(JsonPairConst pair:input.as<JsonObjectConst>()){
        const char* key=pair.key().c_str();auto value=pair.value();
        if(!strcmp(key,"autoBreak")||!strcmp(key,"autoFocus")){
            if(!value.is<bool>())return false;
            if(!strcmp(key,"autoBreak"))c.autoBreak=value.as<bool>();else c.autoFocus=value.as<bool>();
        }else{
            if(!value.is<unsigned>())return false;
            const auto number=value.as<unsigned>();
            if(!strcmp(key,"focusMinutes")){if(number<1||number>180)return false;c.focusMinutes=number;}
            else if(!strcmp(key,"shortBreakMinutes")){if(number<1||number>60)return false;c.shortBreakMinutes=number;}
            else if(!strcmp(key,"longBreakMinutes")){if(number<1||number>120)return false;c.longBreakMinutes=number;}
            else if(!strcmp(key,"blocksBeforeLong")){if(number<2||number>12)return false;c.blocksBeforeLong=number;}
            else return false;
        }
    }
    if(!c.valid())return false;
    config=c;return true;
}
void writeSnapshot(JsonObject out,const Snapshot& s) {
    writeConfig(out["config"].to<JsonObject>(),s.config);
    out["phase"]=name(s.phase);out["state"]=name(s.state);out["active"]=s.active();
    out["remainingMs"]=s.remainingMs;out["durationMs"]=s.durationMs;out["round"]=s.round();
    out["completedBlocks"]=s.completedBlocks;out["focusMs"]=s.focusMs;out["breakMs"]=s.breakMs;
    out["revision"]=s.revision;out["eventId"]=s.eventId;out["event"]=name(s.event);out["eventAgeMs"]=s.eventAgeMs;
}
void writeRecord(JsonObject out,const Record& r) {
    out["version"]=r.version;writeSnapshot(out["value"].to<JsonObject>(),r.value);
}
bool readRecord(JsonVariantConst input,Record& record) {
    if(!input["version"].is<unsigned>()||input["version"].as<unsigned>()!=1)return false;
    auto value=input["value"];Record next;
    if(!readConfig(value["config"],next.value.config))return false;
    // Records must contain the entire config; a truncated record is not a preset.
    if(value["config"].size()!=6)return false;
    const char* phase=value["phase"]|"";const char* state=value["state"]|"";
    if(!strcmp(phase,"focus"))next.value.phase=Phase::Focus;
    else if(!strcmp(phase,"shortBreak"))next.value.phase=Phase::ShortBreak;
    else if(!strcmp(phase,"longBreak"))next.value.phase=Phase::LongBreak;
    else if(strcmp(phase,"idle"))return false;
    if(!strcmp(state,"running"))next.value.state=State::Running;
    else if(!strcmp(state,"paused"))next.value.state=State::Paused;
    else if(!strcmp(state,"waiting"))next.value.state=State::Waiting;
    else if(strcmp(state,"idle"))return false;
#define NUMBER(key,type) if(!value[#key].is<type>())return false;next.value.key=value[#key].as<type>();
    NUMBER(remainingMs,uint32_t) NUMBER(durationMs,uint32_t) NUMBER(completedBlocks,uint32_t)
    NUMBER(revision,uint32_t) NUMBER(eventId,uint32_t) NUMBER(focusMs,uint64_t) NUMBER(breakMs,uint64_t)
#undef NUMBER
    Timer validate;if(!validate.restore(next,0))return false;
    record=next;return true;
}
bool command(Timer& timer,JsonVariantConst req,JsonObject reply,Tick now) {
    timer.update(now);reply["ok"]=true;
    const char* op=req["op"]|"";
    auto fail=[&](const char* error){reply["ok"]=false;reply["error"]=error;};
    auto current=timer.snapshot(now);
    if(strcmp(op,"study.get")&&(!req["expectedRevision"].isNull())&&
        (!req["expectedRevision"].is<uint32_t>()||req["expectedRevision"].as<uint32_t>()!=current.revision))
        fail("La etapa cambio. Sincroniza antes de intentarlo otra vez.");
    else if(!strcmp(op,"study.get")){}
    else if(!strcmp(op,"study.start")||!strcmp(op,"study.configure")){
        if(current.active())fail("Termina la sesion antes de cambiar los tiempos o empezar otra.");
        else{
            Config next=current.config;
            if(!req["config"].isNull()&&!readConfig(req["config"],next))fail("Revisa los tiempos y las opciones de estudio.");
            else{
                timer.configure(next);
                if(!strcmp(op,"study.start"))timer.start(now);
            }
        }
    }else if(!strcmp(op,"study.pause")){
        if(!timer.pause(now))fail("No hay una etapa en marcha para pausar.");
    }else if(!strcmp(op,"study.resume")){
        if(!timer.resume(now))fail("No hay una etapa pendiente para continuar.");
    }else if(!strcmp(op,"study.skip")){
        if(!timer.skip(now))fail("No hay una sesion para saltar.");
    }else if(!strcmp(op,"study.stop"))timer.stop(now);
    else fail("Operacion de estudio desconocida.");
    writeSnapshot(reply["study"].to<JsonObject>(),timer.snapshot(now));
    return reply["ok"].as<bool>();
}
}
