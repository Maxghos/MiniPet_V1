#include "TelegramService.h"
#include "ActivityService.h"
#include "NetworkGuard.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <time.h>

extern const uint8_t telegramRoots[] asm("_binary_certs_telegram_roots_bin_start");
namespace {
class BoundedStream:public Stream {
public:
    explicit BoundedStream(Stream& stream):stream_(stream),started_(millis()){}
    int available() override {return left_&&millis()-started_<10000?std::min<int>(left_,stream_.available()):0;}
    int read() override {if(!left_||millis()-started_>=10000)return -1;const int c=stream_.read();if(c>=0)--left_;return c;}
    int peek() override {return left_&&millis()-started_<10000?stream_.peek():-1;}
    void flush() override {}
    size_t write(uint8_t) override {return 0;}
private:Stream& stream_;uint32_t started_;unsigned left_=65536;
};
const char* help="MiniPet\n/estudio - estado y botones\n/pomodoro - empezar programa guardado\n/pausa /continuar\n/saltar /terminar - piden confirmacion\n/actividad titulo\n/actividades\n/hecho ID /pendiente ID /hoy ID\n/recordar HH:MM titulo\n/diario HH:MM titulo\n/avisos /borraraviso ID\n/resumen\n/habitos /habito 4 Leer\n/cumpli ID /desmarcarhabito ID\n/rutinas /rutina ID\n/estudiar /dia - rutinas 1 y 2\n/compania\n/jugar punteria /jugar esquivar\n/salirjuego\nHorarios: Santiago. Configura tiempos y avisos desde la app.";
}
bool TelegramService::save(const std::string& token,int64_t chat,int64_t user,bool enabled,bool studyAlerts,bool reminderAlerts,bool summaryAlerts){
    JsonDocument doc;doc["token"]=token;doc["chat"]=chat;doc["user"]=user;doc["enabled"]=enabled;
    doc["studyAlerts"]=studyAlerts;doc["reminderAlerts"]=reminderAlerts;doc["summaryAlerts"]=summaryAlerts;
    String data;serializeJson(doc,data);Preferences p;if(!p.begin("minipet-tg",false))return false;
    const bool ok=p.putString("config",data)==data.length();p.end();return ok;
}
bool TelegramService::begin(StudyService& study,AgendaService& agenda){
    study_=&study;agenda_=&agenda;mutex_=xSemaphoreCreateMutex();if(!mutex_)return false;
    Preferences p;if(p.begin("minipet-tg",true)){
        const String data=p.getString("config","");p.end();JsonDocument doc;
        if(data.length()<1024&&!deserializeJson(doc,data)){
            const std::string token=doc["token"]|"";
            const int64_t chat=doc["chat"]|int64_t(0),user=doc["user"]|int64_t(0);
            if(pet::telegram::validToken(token)&&chat>=0&&chat==user){token_=token;gate_.owner(chat,user);}
            enabled_=doc["enabled"]|true;studyAlerts_=doc["studyAlerts"]|true;
            reminderAlerts_=doc["reminderAlerts"]|true;summaryAlerts_=doc["summaryAlerts"]|true;
        }
    }
    return xTaskCreatePinnedToCore(taskEntry,"minipet-telegram",12288,this,1,nullptr,0)==pdPASS;
}
void TelegramService::status(JsonObject out){
    pet::telegram::publicState(out,{!token_.empty(),gate_.chat()!=0,enabled_,ready_.load(),gate_.pairing(millis()),username_,code_.load(),studyAlerts_,reminderAlerts_,summaryAlerts_});
}
void TelegramService::execute(JsonVariantConst req,JsonObject reply,bool secureBle){
    if(!mutex_||xSemaphoreTake(mutex_,pdMS_TO_TICKS(2000))!=pdTRUE){reply["ok"]=false;reply["error"]="Telegram ocupado";return;}
    reply["ok"]=true;const std::string op=req["op"]|"";
    auto fail=[&](const char* text){reply["ok"]=false;reply["error"]=text;};
    if(op=="telegram.get"){}
    else if(op=="telegram.configure"){
        const std::string next=req["tokenValue"]|"";
        if(!secureBle)fail("Configura el token desde una conexion BLE cifrada.");
        else if(!pet::telegram::validToken(next))fail("Revisa el token de BotFather.");
        else if(!save(next,0,0,true,studyAlerts_,reminderAlerts_,summaryAlerts_))fail("No se pudo guardar la configuracion.");
        else{token_=next;gate_=pet::telegram::Gate{};enabled_=true;ready_=false;code_=0;username_.clear();++generation_;agenda_->clearTelegram();}
    }else if(op=="telegram.clear"){
        Preferences p;bool ok=p.begin("minipet-tg",false);
        if(ok){ok=!p.isKey("config")||p.remove("config");p.end();}
        if(!ok)fail("No se pudo borrar la configuracion.");
        else{token_.clear();gate_=pet::telegram::Gate{};username_.clear();ready_=false;code_=0;++generation_;agenda_->clearTelegram();}
    }else if(op=="telegram.settings"){
        bool enabled=enabled_,study=studyAlerts_,reminders=reminderAlerts_,summary=summaryAlerts_;
#define FLAG(key,target) if(!req[key].isNull()){if(!req[key].is<bool>())fail("Opcion invalida");else target=req[key].as<bool>();}
        FLAG("enabled",enabled) FLAG("studyAlerts",study) FLAG("reminderAlerts",reminders) FLAG("summaryAlerts",summary)
#undef FLAG
        if(reply["ok"].as<bool>()){
            if(!save(token_,gate_.chat(),gate_.user(),enabled,study,reminders,summary))fail("No se pudo guardar.");
            else{if(enabled_!=enabled){ready_=false;code_=0;gate_.close();++generation_;agenda_->clearTelegram();}enabled_=enabled;studyAlerts_=study;reminderAlerts_=reminders;summaryAlerts_=summary;}
        }
    }else fail("Operacion de Telegram desconocida.");
    status(reply["telegram"].to<JsonObject>());xSemaphoreGive(mutex_);
}
bool TelegramService::openPairing(){
    if(!mutex_||xSemaphoreTake(mutex_,pdMS_TO_TICKS(20))!=pdTRUE)return false;
    const bool ok=enabled_&&!token_.empty()&&ready_;
    if(ok){snprintf(pin_,sizeof(pin_),"%06lu",static_cast<unsigned long>(esp_random()%1000000));pairAt_=millis();gate_.open(pin_,pairAt_);}
    xSemaphoreGive(mutex_);return ok;
}
void TelegramService::closePairing(){if(mutex_&&xSemaphoreTake(mutex_,pdMS_TO_TICKS(20))==pdTRUE){gate_.close();xSemaphoreGive(mutex_);}}
bool TelegramService::pairing(){
    bool result=false;if(mutex_&&xSemaphoreTake(mutex_,pdMS_TO_TICKS(2))==pdTRUE){result=gate_.pairing(millis());xSemaphoreGive(mutex_);}return result;
}
unsigned TelegramService::pairingSeconds(){return pairing()?(120000-uint32_t(millis()-pairAt_))/1000:0;}
void TelegramService::taskEntry(void* self){static_cast<TelegramService*>(self)->run();}
bool TelegramService::api(const std::string& token,const char* method,JsonDocument& args,JsonDocument& output,const AgendaNotice* notice){
    if(workerGeneration_!=generation_||retry_.remaining(millis()))return false;
    NetworkGuard guard;if(!guard){retry_.defer(millis(),1000);return false;}
    if(workerGeneration_!=generation_)return false;
    if(notice){
        if(xSemaphoreTake(mutex_,pdMS_TO_TICKS(20))!=pdTRUE)return false;
        const bool allowed=enabled_&&!quiet_&&token==token_&&args["chat_id"].as<int64_t>()==gate_.chat()&&
            pet::elapsed(millis(),notice->created)<notice->ttl&&(notice->kind==2?studyAlerts_:notice->kind==1?summaryAlerts_:reminderAlerts_);
        xSemaphoreGive(mutex_);
        if(!allowed)return true; // Consume a muted/expired notice without making an HTTP request.
    }
    WiFiClientSecure client;client.setCACertBundle(telegramRoots);client.setHandshakeTimeout(8);
    HTTPClient http;http.setConnectTimeout(5000);http.setTimeout(!strcmp(method,"getUpdates")&&(args["timeout"]|0)>0?15000:6000);http.useHTTP10(true);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    if(!http.begin(client,("https://api.telegram.org/bot"+token+"/"+method).c_str())){retry_.failure(millis(),-1);return false;}
    http.addHeader("Content-Type","application/json");String payload;serializeJson(args,payload);
    const int code=http.POST(payload);if(workerGeneration_==generation_)code_=code;
    JsonDocument filter;filter["ok"]=true;filter["description"]=true;filter["parameters"]["retry_after"]=true;
    if(!strcmp(method,"getUpdates")){
        auto item=filter["result"].add<JsonObject>();item["update_id"]=true;
        auto msg=item["message"].to<JsonObject>();
        for(const char* key:{"text","date","chat","from","forward_origin","forward_from","forward_date"})msg[key]=true;
        auto cb=item["callback_query"].to<JsonObject>();cb["id"]=true;cb["data"]=true;cb["from"]=true;
        auto m=cb["message"].to<JsonObject>();m["date"]=true;m["chat"]=true;
    }else{filter["result"]["username"]=true;filter["result"]["message_id"]=true;}
    bool ok=false;
    if(code>0&&(http.getSize()<0||http.getSize()<=65536)){
        BoundedStream stream(http.getStream());
        if(!deserializeJson(output,stream,DeserializationOption::Filter(filter),DeserializationOption::NestingLimit(16)))
            ok=code==200&&(output["ok"]|false);
    }
    if(!ok)retry_.failure(millis(),code,output["parameters"]["retry_after"]|30u);
    http.end();return ok;
}
bool TelegramService::send(const std::string& token,int64_t chat,const std::string& text,JsonVariantConst keyboard,const AgendaNotice* notice){
    if(xSemaphoreTake(mutex_,pdMS_TO_TICKS(20))!=pdTRUE)return false;
    const bool permitted=enabled_&&token==token_&&(chat==gate_.chat()||gate_.pairing(millis()));
    xSemaphoreGive(mutex_);if(!permitted)return false;
    JsonDocument args,reply;args["chat_id"]=chat;args["text"]=text.substr(0,3500);
    if(!keyboard.isNull())args["reply_markup"]=keyboard;
    return api(token,"sendMessage",args,reply,notice);
}
void TelegramService::handle(const pet::telegram::Input& input,const std::string& token){
    if(!input.callbackId.empty()){JsonDocument a,r;a["callback_query_id"]=input.callbackId;api(token,"answerCallbackQuery",a,r);}
    JsonDocument req,reply;std::string error;
    if(!pet::telegram::command(input.text,input.callback,req.to<JsonObject>(),error)){send(token,input.chat,error);return;}
    std::string op=req["op"]|"";
    if(op=="help"){send(token,input.chat,help);return;}
    if(op.rfind("habit.",0)==0||op.rfind("companion.",0)==0||op.rfind("routine.",0)==0||op.rfind("game.",0)==0){
        const bool read=op=="companion.get"||op=="routine.list"||op=="game.get";
        if(!read&&localBusy_&&op!="game.stop"){send(token,input.chat,"Cierra los ajustes de la OLED antes de aplicar cambios.");return;}
        if(op=="routine.run"||op.rfind("game.",0)==0){
            if(!activities_){send(token,input.chat,"Actividades no disponibles.");return;}
            activities_->execute(req.as<JsonVariantConst>(),reply.to<JsonObject>());
        }else agenda_->execute(req.as<JsonVariantConst>(),reply.to<JsonObject>());
        if(!(reply["ok"]|false)){send(token,input.chat,reply["error"]|"No se pudo aplicar.");return;}
        if(!read)interaction_=true;
        std::string result;
        if(op=="routine.list"){
            result="Rutinas (usa /rutina ID):\n";
            for(auto r:reply["routines"].as<JsonArray>())result+=std::to_string(r["id"]|0u)+" "+std::string(r["title"]|"")+"\n";
        }else if(op=="companion.get"&&std::string(req["view"]|"")=="habits"){
            result="Habitos: ID, dias esta semana/meta\n";
            for(auto h:reply["habits"].as<JsonArray>())result+=std::to_string(h["id"]|0u)+" "+std::string(h["title"]|"")+" "+std::to_string(h["weekDone"]|0u)+"/"+std::to_string(h["targetPerWeek"]|1u)+((h["doneToday"]|false)?" [hoy hecho]\n":"\n");
        }else if(op=="companion.get"){
            auto c=reply["companion"];result=std::string(c["name"]|"MiniPet")+"\nDias juntos: "+std::to_string(c["daysTogether"]|0u)+"\nBloques recordados: "+std::to_string(c["totalBlocks"]|0u)+"\nActividades: "+std::to_string(c["totalTasks"]|0u);
        }else if(op=="routine.run")result="Rutina preparada: "+std::string(reply["plan"]["title"]|"");
        else if(op=="game.start")result="Juego iniciado. Coloca la mano entre 15 y 80 cm. Para salir: /salirjuego o boton de ajustes.";
        else if(op=="game.stop")result="Juego cerrado.";
        else result="Habito actualizado. Consulta /habitos.";
        if(reply["agenda"]["persisted"].is<bool>()&&!(reply["agenda"]["persisted"]|true))result+="\nPendiente de guardar en memoria persistente.";
        send(token,input.chat,result);return;
    }
    if(op=="confirm"){
        JsonDocument read,current;read["op"]="study.get";study_->execute(read.as<JsonVariantConst>(),current.to<JsonObject>());
        auto s=current["study"];
        if(!req["expectedRevision"].isNull()&&req["expectedRevision"].as<uint32_t>()!=(s["revision"]|uint32_t(0))){send(token,input.chat,"La etapa cambio. Usa /estudio para actualizar los botones.");return;}
        if(!(s["active"]|false)){send(token,input.chat,"No hay una sesion activa.");return;}
        JsonDocument keys;const char* action=req["action"]|"stop";pet::telegram::keyboard(keys.to<JsonObject>(),s,true,action);
        send(token,input.chat,!strcmp(action,"stop")?"Confirmas terminar la sesion?":"Confirmas saltar esta etapa? El bloque incompleto no suma como completado.",keys.as<JsonVariantConst>());return;
    }
    if(op.rfind("study.",0)==0){
        if(localBusy_&&(op=="study.start"||op=="study.resume"||op=="study.skip")){send(token,input.chat,"Cierra los ajustes de la OLED primero.");return;}
        if(op!="study.get"&&req["expectedRevision"].isNull()){
            JsonDocument read,current;read["op"]="study.get";study_->execute(read.as<JsonVariantConst>(),current.to<JsonObject>());
            req["expectedRevision"]=current["study"]["revision"];
        }
        study_->execute(req.as<JsonVariantConst>(),reply.to<JsonObject>());
        if(!(reply["ok"]|false)){send(token,input.chat,reply["error"]|"No se pudo aplicar.");return;}
        if(op!="study.get")interaction_=true;
        auto s=reply["study"];const std::string phase=s["phase"]|"idle",state=s["state"]|"idle";
        const uint32_t seconds=((s["remainingMs"]|uint32_t(0))+999)/1000;char text[220];
        snprintf(text,sizeof(text),"%s\n%s\n%02lu:%02lu restantes\nBloques completados: %u",phase=="idle"?"Sin sesion":phase=="focus"?"Estudio":phase=="longBreak"?"Descanso largo":"Descanso corto",
            state=="running"?"En marcha":state=="paused"?"Pausado":state=="waiting"?"Esperando confirmacion":"Listo para empezar",static_cast<unsigned long>(seconds/60),static_cast<unsigned long>(seconds%60),s["completedBlocks"]|0u);
        JsonDocument keys;pet::telegram::keyboard(keys.to<JsonObject>(),s);send(token,input.chat,text,keys.as<JsonVariantConst>());return;
    }
    agenda_->execute(req.as<JsonVariantConst>(),reply.to<JsonObject>());
    if(!(reply["ok"]|false)){send(token,input.chat,reply["error"]|"No se pudo aplicar.");return;}
    if(op!="agenda.get")interaction_=true;
    auto agenda=reply["agenda"];const std::string view=req["view"]|"";
    if(view=="summary"){send(token,input.chat,agenda["summary"]|"Hora sin sincronizar.");return;}
    if(view=="tasks"||view=="reminders"){
        std::string result=view=="tasks"?"Actividades (ID para /hecho):\n":"Recordatorios (ID para /borraraviso):\n";
        const uint32_t day=agenda["day"]|uint32_t(0);
        for(auto v:agenda[view=="tasks"?"tasks":"reminders"].as<JsonArray>()){
            if(view=="tasks"&&((v["day"]|uint32_t(0))>day))continue;
            char prefix[80];
            if(view=="tasks")snprintf(prefix,sizeof(prefix),"%u %s ",v["id"]|0u,(v["done"]|false)?"[hecho]":"[ ]");
            else snprintf(prefix,sizeof(prefix),"%u %02u:%02u %s%s ",v["id"]|0u,(v["minute"]|0u)/60,(v["minute"]|0u)%60,(v["daily"]|false)?"diario":"puntual",(v["enabled"]|false)?"":" (finalizado)");
            result+=prefix;result+=v["title"]|"";result+='\n';
        }
        send(token,input.chat,result);return;
    }
    send(token,input.chat,"Agenda actualizada. Consulta /actividades, /avisos o /resumen.");
}
void TelegramService::run(){
    uint32_t knownGeneration=UINT32_MAX;bool primed=false;
    for(;;){
        vTaskDelay(pdMS_TO_TICKS(1000));std::string token;uint32_t generation;
        bool enabled=false;int64_t offset=0,chat=0;bool studyAlerts=false,reminders=false,summary=false;
        if(xSemaphoreTake(mutex_,pdMS_TO_TICKS(20))!=pdTRUE)continue;
        token=token_;generation=generation_;enabled=enabled_;offset=gate_.offset(time(nullptr));chat=gate_.chat();
        studyAlerts=studyAlerts_;reminders=reminderAlerts_;summary=summaryAlerts_;xSemaphoreGive(mutex_);
        if(generation!=knownGeneration){knownGeneration=generation;primed=false;retry_.clear();}
        workerGeneration_=generation;
        if(!enabled||token.empty()||WiFi.status()!=WL_CONNECTED||time(nullptr)<1704067200){ready_=false;primed=false;continue;}
        bool success=true;
        if(!primed){
            JsonDocument args,me,drain;
            success=api(token,"getMe",args,me);
            if(success){args["offset"]=-1;args["limit"]=1;args["timeout"]=0;success=api(token,"getUpdates",args,drain);}
            if(success&&generation==generation_&&xSemaphoreTake(mutex_,pdMS_TO_TICKS(20))==pdTRUE){
                if(generation==generation_){username_=me["result"]["username"]|"";
                    for(auto u:drain["result"].as<JsonArray>())gate_.drain(u["update_id"]|int64_t(-1),time(nullptr));
                    offset=gate_.offset(time(nullptr));ready_=true;primed=true;}
                xSemaphoreGive(mutex_);
            }
        }else{
            JsonDocument args,updates;args["offset"]=offset;args["limit"]=5;args["timeout"]=10;
            auto allowed=args["allowed_updates"].to<JsonArray>();allowed.add("message");allowed.add("callback_query");
            success=api(token,"getUpdates",args,updates);
            if(success&&generation==generation_)for(auto update:updates["result"].as<JsonArray>()){
                pet::telegram::Input input;
                if(xSemaphoreTake(mutex_,pdMS_TO_TICKS(20))!=pdTRUE)continue;
                if(generation==generation_){
                    const auto previousChat=gate_.chat(),previousUser=gate_.user();
                    input=gate_.accept(update,time(nullptr),millis());
                    if(input.kind==pet::telegram::Kind::Paired){
                        if(save(token_,gate_.chat(),gate_.user(),enabled_,studyAlerts_,reminderAlerts_,summaryAlerts_))paired_=true;
                        else{gate_.owner(previousChat,previousUser);input.kind=pet::telegram::Kind::RejectedPair;}
                    }
                }
                xSemaphoreGive(mutex_);
                if(input.kind==pet::telegram::Kind::Paired){agenda_->clearTelegram();send(token,input.chat,"MiniPet vinculado a tu cuenta. Usa /ayuda o /estudio.");}
                else if(input.kind==pet::telegram::Kind::RejectedPair)send(token,input.chat,"No se pudo vincular. Revisa el PIN y vuelve a abrir Vincular Telegram en la OLED.");
                else if(input.kind==pet::telegram::Kind::Command&&generation==generation_)handle(input,token);
                if(retry_.remaining(millis()))break;
            }
            if(success&&chat&&generation==generation_&&!retry_.remaining(millis())){
                AgendaNotice notice;
                if(agenda_->peekTelegram(notice)){
                    const bool permitted=notice.kind==2?studyAlerts:notice.kind==1?summary:reminders;
                    if(!permitted||quiet_)agenda_->finishTelegram(notice.id,true);
                    else agenda_->finishTelegram(notice.id,send(token,chat,notice.text,JsonVariantConst{},&notice));
                }
            }
        }
        if(generation==generation_)ready_=success&&primed;
        const uint32_t pause=std::max<uint32_t>(2000,retry_.remaining(millis())),started=millis();
        while(pet::elapsed(millis(),started)<pause&&generation==generation_)vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
