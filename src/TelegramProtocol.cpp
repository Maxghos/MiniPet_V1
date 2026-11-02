#include "TelegramProtocol.h"
#include <algorithm>
#include <cstdio>

namespace pet::telegram {
void publicState(JsonObject out,const PublicState& s){
    out["configured"]=s.configured;out["linked"]=s.linked;out["enabled"]=s.enabled;out["ready"]=s.ready;out["pairing"]=s.pairing;
    out["username"]=s.username;out["code"]=s.code;out["studyAlerts"]=s.studyAlerts;out["reminderAlerts"]=s.reminderAlerts;out["summaryAlerts"]=s.summaryAlerts;
}
void RetryWindow::failure(Tick now,int code,unsigned retryAfter){
    const unsigned seconds=code==429?std::clamp(retryAfter,3u,600u):code==401||code==409?60u:15u;
    defer(now,seconds*1000u);
}
bool validToken(const std::string& token){
    const auto colon=token.find(':');if(colon<5||colon>16||token.size()<colon+26||token.size()>100)return false;
    for(size_t i=0;i<token.size();++i){const unsigned char c=token[i];
        if(i<colon){if(c<'0'||c>'9')return false;}
        else if(i>colon&&!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;
    }
    return true;
}
bool parseClock(const std::string& s,unsigned& minute){
    if(s.size()!=5||s[2]!=':')return false;
    for(unsigned i:{0u,1u,3u,4u})if(s[i]<'0'||s[i]>'9')return false;
    const unsigned h=(s[0]-'0')*10+s[1]-'0',m=(s[3]-'0')*10+s[4]-'0';
    if(h>23||m>59)return false;
    minute=h*60+m;return true;
}
Input Gate::accept(JsonVariantConst update,int64_t epoch,Tick now){
    Input result;if(!update["update_id"].is<int64_t>())return result;
    const auto id=update["update_id"].as<int64_t>();if(id<0||id==INT64_MAX)return result;
    if(idleWeek(epoch))lastUpdate_=-1;
    if(id<=lastUpdate_)return result;
    lastUpdate_=id;lastEpoch_=epoch;
    const bool callback=update["callback_query"].is<JsonObjectConst>();
    auto msg=callback?update["callback_query"]["message"]:update["message"];
    auto from=callback?update["callback_query"]["from"]:msg["from"];
    if(!msg.is<JsonObjectConst>()||!from.is<JsonObjectConst>()||std::string(msg["chat"]["type"]|"")!="private"||
        !msg["chat"]["id"].is<int64_t>()||!from["id"].is<int64_t>()||(from["is_bot"]|false)||
        !msg["forward_origin"].isNull()||!msg["forward_date"].isNull()||!msg["forward_from"].isNull())return result;
    const int64_t chat=msg["chat"]["id"],user=from["id"];
    if(chat<=0||user<=0||chat!=user||chat>4503599627370495LL)return result;
    if(!callback){
        if(!msg["date"].is<int64_t>()||!msg["text"].is<const char*>()||epoch<1704067200)return result;
        const int64_t date=msg["date"];if(date<epoch-120||date>epoch+15)return result;
    }
    const std::string text=callback?std::string(update["callback_query"]["data"]|""):std::string(msg["text"]|"");
    if(text.empty()||text.size()>4096)return result;
    if(!callback&&text.rfind("/vincular ",0)==0){
        if(!pairing(now))return result;
        ++attempts_;
        if(text.substr(10)!=pin_){result.kind=Kind::RejectedPair;result.chat=chat;return result;}
        chat_=chat;user_=user;close();result.kind=Kind::Paired;result.chat=chat;result.user=user;return result;
    }
    if(!chat_||chat!=chat_||user!=user_)return result;
    result.kind=Kind::Command;result.chat=chat;result.user=user;result.text=text;result.callback=callback;
    if(callback){result.callbackId=update["callback_query"]["id"]|"";if(result.callbackId.size()>128)return Input{};}
    return result;
}
static bool number(const std::string& s,uint32_t& value){
    if(s.empty()||s.size()>10)return false;
    uint64_t n=0;
    for(char c:s){if(c<'0'||c>'9')return false;n=n*10+unsigned(c-'0');if(n>UINT32_MAX)return false;}
    value=uint32_t(n);return true;
}
bool command(const std::string& text,bool callback,JsonObject req,std::string& error){
    auto fail=[&](){error="Comando no reconocido. Usa /ayuda.";return false;};
    if(callback){
        if(text=="menu"){req["op"]="study.get";return true;}
        const auto a=text.find(':'),b=text.find(':',a==std::string::npos?0:a+1);
        if(a!=1||b==std::string::npos)return fail();
        const auto action=text.substr(a+1,b-a-1);uint32_t revision;
        if(!number(text.substr(b+1),revision))return fail();
        if(action!="start"&&action!="pause"&&action!="resume"&&action!="skip"&&action!="stop")return fail();
        if(text[0]!='p'&&text[0]!='q')return fail();
        req["op"]=text[0]=='q'?"confirm":"study."+action;req["action"]=action;req["expectedRevision"]=revision;return true;
    }
    const auto space=text.find(' ');auto verb=text.substr(0,space);const auto mention=verb.find('@');
    if(mention!=std::string::npos)verb.resize(mention);
    const std::string arg=space==std::string::npos?"":text.substr(space+1);
    if(verb=="/start"||verb=="/ayuda"){req["op"]="help";return true;}
    if(verb=="/estado"||verb=="/estudio"){req["op"]="study.get";return true;}
    if(verb=="/pomodoro"&&arg.empty()){req["op"]="study.start";return true;}
    if(verb=="/pausa"){req["op"]="study.pause";return true;}
    if(verb=="/continuar"){req["op"]="study.resume";return true;}
    if(verb=="/habitos"||verb=="/compania"){req["op"]="companion.get";req["view"]=verb=="/habitos"?"habits":"companion";return true;}
    if(verb=="/rutinas"){req["op"]="routine.list";return true;}
    if(verb=="/estudiar"||verb=="/dia"){req["op"]="routine.run";req["id"]=verb=="/estudiar"?1:2;return true;}
    if(verb=="/rutina"||verb=="/cumpli"||verb=="/desmarcarhabito"){
        uint32_t id;if(!number(arg,id)||!id)return fail();req["id"]=id;
        req["op"]=verb=="/rutina"?"routine.run":"habit.done";req["done"]=verb=="/cumpli";return true;
    }
    if(verb=="/habito"){
        const auto split=arg.find(' ');uint32_t target;
        if(split==std::string::npos||!number(arg.substr(0,split),target)||target<1||target>7){error="Usa /habito 4 Leer: meta de 1 a 7 dias por semana.";return false;}
        req["op"]="habit.add";req["title"]=arg.substr(split+1);req["targetPerWeek"]=target;return true;
    }
    if(verb=="/jugar"){
        if(arg!="punteria"&&arg!="esquivar"){error="Usa /jugar punteria o /jugar esquivar.";return false;}
        req["op"]="game.start";req["kind"]=arg=="punteria"?"target":"dodge";return true;
    }
    if(verb=="/salirjuego"){req["op"]="game.stop";return true;}
    if(verb=="/terminar"||verb=="/saltar"){req["op"]="confirm";req["action"]=verb=="/terminar"?"stop":"skip";return true;}
    if(verb=="/actividades"||verb=="/hoy"||verb=="/avisos"||verb=="/resumen"){
        if(verb=="/hoy"&&!arg.empty()){uint32_t id;if(!number(arg,id))return fail();req["op"]="task.today";req["id"]=id;return true;}
        req["op"]="agenda.get";req["view"]=verb=="/resumen"?"summary":verb=="/avisos"?"reminders":"tasks";return true;
    }
    if(verb=="/actividad"){req["op"]="task.add";req["title"]=arg;return true;}
    if(verb=="/hecho"||verb=="/pendiente"||verb=="/borraraviso"){
        uint32_t id;if(!number(arg,id)||!id)return fail();req["id"]=id;
        req["op"]=verb=="/borraraviso"?"reminder.delete":"task.done";req["done"]=verb=="/hecho";return true;
    }
    if(verb=="/recordar"||verb=="/diario"){
        unsigned minute;const auto split=arg.find(' ');
        if(split!=5||!parseClock(arg.substr(0,5),minute)){error="Usa /recordar HH:MM titulo o /diario HH:MM titulo.";return false;}
        req["op"]="reminder.add";req["minute"]=minute;req["title"]=arg.substr(6);req["daily"]=verb=="/diario";return true;
    }
    return fail();
}
void keyboard(JsonObject out,JsonVariantConst study,bool confirm,const char* action){
    auto rows=out["inline_keyboard"].to<JsonArray>();const uint32_t revision=study["revision"]|uint32_t(0);
    auto button=[&](JsonArray row,const char* label,const char* op,char prefix){
        auto b=row.add<JsonObject>();b["text"]=label;char data[48];snprintf(data,sizeof(data),"%c:%s:%lu",prefix,op,static_cast<unsigned long>(revision));b["callback_data"]=data;
    };
    if(confirm){auto r=rows.add<JsonArray>();button(r,"Confirmar",action,'p');auto c=r.add<JsonObject>();c["text"]="Cancelar";c["callback_data"]="menu";return;}
    const bool active=study["active"]|false;const std::string state=study["state"]|"";
    auto r=rows.add<JsonArray>();button(r,!active?"Empezar":state=="running"?"Pausar":"Continuar",!active?"start":state=="running"?"pause":"resume",'p');
    if(active){auto extra=rows.add<JsonArray>();button(extra,"Saltar etapa","skip",'q');button(extra,"Terminar","stop",'q');}
}
}
