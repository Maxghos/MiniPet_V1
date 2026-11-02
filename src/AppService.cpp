#include "AppService.h"
#include <AsyncJson.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <mbedtls/base64.h>
#include <new>
#include <vector>

namespace {
constexpr size_t maxJson=12288,quota=2*1024*1024;
struct BleRequest {std::string value;uint32_t generation;};
std::string path(const std::string& id,const char* ext){ return "/"+id+ext; }
std::string randomToken(){ char out[65];for(unsigned i=0;i<8;++i)snprintf(out+i*8,9,"%08lx",static_cast<unsigned long>(esp_random()));return out; }
class Writes:public NimBLECharacteristicCallbacks {
public: explicit Writes(AppService* owner):owner_(owner){}
    void onWrite(NimBLECharacteristic* characteristic) override { owner_->receiveBle(characteristic->getValue()); }
private: AppService* owner_;
};
class Connections:public NimBLEServerCallbacks {
public:explicit Connections(AppService* owner):owner_(owner){}
    void onDisconnect(NimBLEServer*) override {owner_->resetBle();NimBLEDevice::startAdvertising();}
private:AppService* owner_;
};
class Responses:public NimBLECharacteristicCallbacks {
public:explicit Responses(AppService* owner):owner_(owner){}
    void onStatus(NimBLECharacteristic*,Status status,int) override {owner_->acknowledgeBle(status==Status::SUCCESS_INDICATE);}
private:AppService* owner_;
};
void settingsJson(JsonObject out,const pet::Settings& s) {
#define FIELD(name) out[#name]=s.name;
    FIELD(modules) FIELD(humor) FIELD(brightness) FIELD(messages) FIELD(quietEnabled)
    FIELD(quietStart) FIELD(quietEnd) FIELD(reminderSpeed) FIELD(weatherView)
    FIELD(gestureMinCm) FIELD(gestureMaxCm) FIELD(gestureMs) FIELD(presenceCm)
    FIELD(absenceSeconds) FIELD(sleepMinutes) FIELD(pageSeconds) FIELD(randomPhrases)
    FIELD(weatherReactions) FIELD(weatherNotices) FIELD(stormFlash)
#undef FIELD
}
bool applySettings(JsonVariantConst json,pet::Settings& s) {
    if(!json.is<JsonObjectConst>())return false;
    auto number=[&](const char* key,int lo,int hi,int& value){
        if(json[key].isNull())return true;
        if(!json[key].is<int>())return false;
        value=json[key].as<int>();return value>=lo&&value<=hi;
    };
#define FIELD(name,lo,hi) {int value=s.name;if(!number(#name,lo,hi,value))return false;s.name=value;}
    FIELD(modules,1,7) FIELD(humor,0,100) FIELD(brightness,10,255) FIELD(messages,0,1)
    FIELD(quietEnabled,0,1) FIELD(quietStart,0,23) FIELD(quietEnd,0,23) FIELD(reminderSpeed,0,2)
    FIELD(weatherView,0,3) FIELD(gestureMinCm,10,200) FIELD(gestureMaxCm,20,300)
    FIELD(gestureMs,2000,10000) FIELD(presenceCm,30,350) FIELD(absenceSeconds,30,600)
    FIELD(sleepMinutes,5,60) FIELD(pageSeconds,3,15) FIELD(randomPhrases,0,1)
    FIELD(weatherReactions,0,1) FIELD(weatherNotices,0,1) FIELD(stormFlash,0,1)
#undef FIELD
    return s.gestureMaxCm>=s.gestureMinCm+10;
}
}
bool AppService::begin(const pet::Settings& settings,StudyService& study) {
    study_=&study;
    settings_=settings;mutex_=xSemaphoreCreateMutex();bleQueue_=xQueueCreate(3,sizeof(BleRequest*));
    bleAck_=xSemaphoreCreateBinary();
    if(!mutex_||!bleQueue_||!bleAck_)return false;
    Preferences p;bool initialized=false;
    if(p.begin("minipet-app",false)) {
        token_=p.getString("token","").c_str();initialized=p.getBool("fsReady",false);
        fsReady_=LittleFS.begin(!initialized,"/littlefs",10,"spiffs");
        if(fsReady_)p.putBool("fsReady",true);p.end();
    }
    if(fsReady_)LittleFS.remove("/upload.part");
    server_.on("/api/v1/info",HTTP_GET,[this](AsyncWebServerRequest* request){
        JsonDocument doc;doc["name"]="MiniPet";doc["protocol"]=1;doc["width"]=128;doc["height"]=32;
        doc["port"]=8080;doc["maxFrames"]=pet::app::maxFrames;doc["pairing"]=pairing();
        String body;serializeJson(doc,body);request->send(200,"application/json",body);
    });
    auto* handler=new AsyncCallbackJsonWebHandler("/api/v1/command",[this](AsyncWebServerRequest* request,JsonVariant& json){
        std::string bearer;
        if(request->hasHeader("Authorization")) {
            const String value=request->getHeader("Authorization")->value();
            if(value.startsWith("Bearer "))bearer=value.substring(7).c_str();
        }
        String input;serializeJson(json,input);
        const auto reply=dispatch(input.c_str(),bearer);
        request->send(200,"application/json; charset=utf-8",reply.c_str());
    });
    handler->setMaxContentLength(maxJson);server_.addHandler(handler);server_.begin();
    char name[20];snprintf(name,sizeof(name),"MiniPet-%04X",unsigned(ESP.getEfuseMac()&0xffff));
    NimBLEDevice::init(name);NimBLEDevice::setMTU(247);
    NimBLEDevice::setSecurityAuth(true,true,true);NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
    NimBLEDevice::setSecurityPasskey(esp_random()%1000000);
    bleServer_=NimBLEDevice::createServer();bleServer_->setCallbacks(new Connections(this));
    auto* service=bleServer_->createService(pet::app::serviceUuid);
    auto* command=service->createCharacteristic(pet::app::commandUuid,NIMBLE_PROPERTY::WRITE|NIMBLE_PROPERTY::WRITE_ENC);
    command->setCallbacks(new Writes(this));
    responses_=service->createCharacteristic(pet::app::responseUuid,NIMBLE_PROPERTY::INDICATE|NIMBLE_PROPERTY::READ|NIMBLE_PROPERTY::READ_ENC);
    responses_->setCallbacks(new Responses(this));
    service->start();auto* advertising=NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(service->getUUID());advertising->setScanResponse(true);advertising->start();
    return xTaskCreatePinnedToCore(bleWorker,"minipet-app",8192,this,1,nullptr,0)==pdPASS;
}
void AppService::poll() {
    if(pairing_&&millis()-pairAt_>=120000)closePairing();
    if(!mdns_&&millis()-lastMdns_>=5000&&(WiFi.status()==WL_CONNECTED||WiFi.getMode()==WIFI_AP_STA)) {
        lastMdns_=millis();char host[24];snprintf(host,sizeof(host),"minipet-%04x",unsigned(ESP.getEfuseMac()&0xffff));
        if(MDNS.begin(host)){MDNS.addService("minipet","tcp",8080);mdns_=true;}
    }
}
void AppService::openPairing() {
    if(xSemaphoreTake(mutex_,pdMS_TO_TICKS(20))!=pdTRUE)return;
    snprintf(pin_,sizeof(pin_),"%06lu",static_cast<unsigned long>(esp_random()%1000000));
    pairAt_=millis();pairing_=true;attempts_=0;NimBLEDevice::setSecurityPasskey(strtoul(pin_,nullptr,10));
    xSemaphoreGive(mutex_);
}
void AppService::closePairing(){ pairing_=false; }
bool AppService::pairing() const { return pairing_&&uint32_t(millis()-pairAt_)<120000; }
unsigned AppService::pairingSeconds() const {return pairing()?(120000-(millis()-pairAt_))/1000:0;}
void AppService::updateSettings(const pet::Settings& s){if(xSemaphoreTake(mutex_,pdMS_TO_TICKS(2000))==pdTRUE){settings_=s;xSemaphoreGive(mutex_);}}
bool AppService::takeSettings(pet::Settings& s){bool result=false;if(xSemaphoreTake(mutex_,pdMS_TO_TICKS(2))==pdTRUE){if(pendingSettings_){s=settings_;pendingSettings_=false;result=true;}xSemaphoreGive(mutex_);}return result;}
bool AppService::takeInteraction(){bool result=false;if(xSemaphoreTake(mutex_,pdMS_TO_TICKS(2))==pdTRUE){result=interaction_;interaction_=false;xSemaphoreGive(mutex_);}return result;}
void AppService::stop(){stopRequested_=true;}
bool AppService::active(){bool result=false;if(xSemaphoreTake(mutex_,pdMS_TO_TICKS(2))==pdTRUE){result=playback_.active(millis());xSemaphoreGive(mutex_);}return result;}
void AppService::read(pet::app::RemoteView& view){
    if(xSemaphoreTake(mutex_,pdMS_TO_TICKS(2))!=pdTRUE)return;
    const uint32_t now=millis(); // A command may have started playback after the caller's loop timestamp.
    if(stopRequested_.exchange(false)){playback_.stop();playingId_.clear();}
    playback_.expire(now);
    view.type=playback_.type(now);view.age=playback_.age(now);
    if(view.revision!=playback_.revision()){
        view.revision=playback_.revision();view.text=playback_.value();view.speed=playback_.speed();view.scrolling=playback_.scrolling();
    }
    if(view.type==pet::app::Content::Media&&media_)memcpy(view.frame,media_+pet::app::headerBytes+playback_.frame(now)*512,512);
    xSemaphoreGive(mutex_);
}
std::string AppService::dispatch(const std::string& json,const std::string& bearer){
    JsonDocument input,output;
    if(json.size()>maxJson||deserializeJson(input,json)||!input.is<JsonObject>())return "{\"ok\":false,\"error\":\"Solicitud invalida\"}";
    if(xSemaphoreTake(mutex_,pdMS_TO_TICKS(2000))!=pdTRUE)return "{\"ok\":false,\"error\":\"Ocupado; intenta otra vez\"}";
    if(stopRequested_.exchange(false)){playback_.stop();playingId_.clear();}
    output["ok"]=true;output["req"]=input["req"]|0;
    execute(input.as<JsonVariantConst>(),output,bearer);xSemaphoreGive(mutex_);
    std::string result;serializeJson(output,result);return result;
}
void AppService::execute(JsonVariantConst req,JsonDocument& reply,const std::string& bearer){
    const std::string op=req["op"]|"";
    auto fail=[&](const char* error){reply["ok"]=false;reply["error"]=error;};
    if(op=="pair"){
        if(!pairing()||attempts_>=5){fail("Abre Vincular app en MiniPet");return;}
        ++attempts_;
        const char* code=req["code"]|"";
        if(strlen(code)!=6||strcmp(code,pin_)){if(attempts_>=5)pairing_=false;fail("Codigo incorrecto");return;}
        auto next=randomToken();Preferences p;
        if(!p.begin("minipet-app",false)){fail("No se pudo vincular");return;}
        const bool stored=p.putString("token",next.c_str())==next.size();p.end();
        if(!stored){fail("No se pudo guardar vinculacion");return;}
        token_=next;pairing_=false;paired_=true;lastContact_=millis();contacted_=true;
        reply["token"]=token_;reply["name"]="MiniPet";return;
    }
    const std::string supplied=bearer.empty()?std::string(req["token"]|""):bearer;
    if(!pet::app::sameToken(supplied,token_)){fail("Vinculacion requerida");return;}
    lastContact_=millis();contacted_=true;
    playback_.heartbeat(millis());
    const bool studyCommand=op.rfind("study.",0)==0;
    if(localBusy_&&(op=="study.start"||op=="study.resume"||op=="study.skip"||op=="study.configure")){
        fail("Cierra los ajustes de la OLED primero");return;
    }
    if(studyCommand){
        study_->execute(req,reply.as<JsonObject>());
        if(reply["ok"].as<bool>()&&op!="study.get"){
            interaction_=true;
            if(study_->active()){playback_.stop();playingId_.clear();}
        }
        return;
    }
    if(localBusy_&&(op=="text"||op=="play"||op=="settings.set")){fail("Cierra los ajustes de la OLED primero");return;}
    if(study_->active()&&(op=="text"||op=="play")){fail("Termina el modo estudio antes de mostrar otro contenido");return;}
    if(op=="ping")return;
    if(op=="status"||op=="settings.get"){
        settingsJson(reply["settings"].to<JsonObject>(),settings_);reply["active"]=playback_.active(millis());
        reply["storage"]=fsReady_;reply["ip"]=WiFi.localIP().toString();reply["firmware"]="3.1.1";
        JsonDocument studyRequest;studyRequest["op"]="study.get";
        study_->execute(studyRequest.as<JsonVariantConst>(),reply.as<JsonObject>());return;
    }
    if(op=="settings.set"){
        auto next=settings_;if(!applySettings(req["settings"],next)){fail("Revisa los valores de ajustes");return;}
        settings_=next;pendingSettings_=true;interaction_=true;settingsJson(reply["settings"].to<JsonObject>(),settings_);return;
    }
    if(op=="text"){
        if(!req["text"].is<const char*>()){fail("Falta texto");return;}
        if(!playback_.text(req["text"].as<std::string>(),millis(),req["durationMs"]|-1,req["scroll"]|false,req["speed"]|25))fail("Texto o duracion fuera de limite");
        else {interaction_=true;playingId_.clear();}return;
    }
    if(op=="stop"){playback_.stop();playingId_.clear();interaction_=true;return;}
    if(!fsReady_){fail("Biblioteca no disponible");return;}
    if(op=="library"){library(reply["items"].to<JsonArray>());reply["usedBytes"]=LittleFS.usedBytes();return;}
    const std::string id=req["id"]|"";
    if(op=="upload.cancel"){cancelUpload();return;}
    if(!pet::app::validAssetId(id)){fail("Identificador invalido");return;}
    if(op=="delete"){
        if(playingId_==id){playback_.stop();playingId_.clear();}
        LittleFS.remove(path(id,".mpet").c_str());LittleFS.remove(path(id,".json").c_str());return;
    }
    if(op=="play"){
        const int32_t duration=req["durationMs"]|-1;
        if(duration < -1||duration>3600000||!play(id.c_str(),duration,req["loop"]|false))fail("No se pudo abrir ese contenido");
        else interaction_=true;return;
    }
    if(op=="upload.begin"){
        const size_t bytes=req["bytes"]|size_t(0);
        if(bytes<536||bytes>pet::app::maxAssetBytes){fail("Archivo fuera de limite");return;}
        if(!LittleFS.exists(path(id,".mpet").c_str())&&assetCount()>=8){fail("Biblioteca llena (8 archivos)");return;}
        cancelUpload();
        if(LittleFS.usedBytes()+bytes+4096>quota||LittleFS.totalBytes()-LittleFS.usedBytes()<bytes+4096){fail("Espacio insuficiente");return;}
        const std::string title=req["title"]|"Contenido";
        if(title.size()>96){fail("Titulo demasiado largo");return;}
        upload_=LittleFS.open("/upload.part","w");
        if(!upload_){fail("No se pudo iniciar carga");return;}
        uploadId_=id;uploadTitle_=title;
        uploadExpected_=bytes;uploadAt_=millis();reply["offset"]=0;return;
    }
    if(uploadId_!=id||!upload_||millis()-uploadAt_>120000){cancelUpload();fail("Reinicia la transferencia");return;}
    if(op=="upload.chunk"){
        const size_t offset=req["offset"]|size_t(-1);const char* value=req["data"]|"";
        const size_t length=strlen(value);size_t decoded=0;
        std::vector<uint8_t> bytes(6144);
        if(offset!=uploadOffset_||length>8192||mbedtls_base64_decode(bytes.data(),bytes.size(),&decoded,reinterpret_cast<const uint8_t*>(value),length)||
           !decoded||uploadOffset_+decoded>uploadExpected_){fail("Bloque de transferencia invalido");return;}
        if(upload_.write(bytes.data(),decoded)!=decoded){cancelUpload();fail("Fallo al guardar bloque");return;}
        uploadOffset_+=decoded;uploadAt_=millis();reply["offset"]=uploadOffset_;return;
    }
    if(op=="upload.commit"){
        if(uploadOffset_!=uploadExpected_||!commitUpload()){cancelUpload();fail("Archivo incompleto o CRC invalido");return;}
        reply["id"]=id;return;
    }
    fail("Operacion desconocida");
}
void AppService::cancelUpload(){if(upload_)upload_.close();if(fsReady_)LittleFS.remove("/upload.part");uploadId_.clear();uploadTitle_.clear();uploadOffset_=uploadExpected_=0;}
bool AppService::commitUpload(){
    upload_.close();File input=LittleFS.open("/upload.part","r");if(!input)return false;
    const size_t length=input.size();auto* bytes=static_cast<uint8_t*>(heap_caps_malloc(length,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(!bytes)bytes=static_cast<uint8_t*>(malloc(length));if(!bytes){input.close();return false;}
    const bool read=input.read(bytes,length)==length;input.close();pet::app::MediaInfo info;
    const bool valid=read&&pet::app::validateMedia(bytes,length,info);free(bytes);if(!valid)return false;
    const auto destination=path(uploadId_,".mpet");
    // LittleFS rename replaces an existing destination atomically.
    if(!LittleFS.rename("/upload.part",destination.c_str()))return false;
    JsonDocument metadata;metadata["id"]=uploadId_;metadata["title"]=uploadTitle_;metadata["bytes"]=length;metadata["frames"]=info.frames;metadata["fps"]=info.fps;
    File meta=LittleFS.open(path(uploadId_,".json").c_str(),"w");if(meta){serializeJson(metadata,meta);meta.close();}
    uploadId_.clear();uploadTitle_.clear();uploadOffset_=uploadExpected_=0;return true;
}
bool AppService::play(const char* id,int32_t duration,bool loop){
    File input=LittleFS.open(path(id,".mpet").c_str(),"r");if(!input||input.size()>pet::app::maxAssetBytes)return false;
    const size_t length=input.size();auto* bytes=static_cast<uint8_t*>(heap_caps_malloc(length,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(!bytes)bytes=static_cast<uint8_t*>(malloc(length));if(!bytes){input.close();return false;}
    const bool read=input.read(bytes,length)==length;input.close();pet::app::MediaInfo info;
    if(!read||!pet::app::validateMedia(bytes,length,info)){free(bytes);return false;}
    playback_.stop();free(media_);media_=bytes;playingId_=id;playback_.media(info,millis(),duration,loop);return true;
}
unsigned AppService::assetCount(){unsigned count=0;File dir=LittleFS.open("/");for(File f=dir.openNextFile();f;f=dir.openNextFile())if(String(f.name()).endsWith(".mpet"))++count;return count;}
void AppService::library(JsonArray array){
    File dir=LittleFS.open("/");for(File f=dir.openNextFile();f;f=dir.openNextFile()){
        String name=f.name();if(!name.endsWith(".mpet"))continue;
        if(name.startsWith("/"))name.remove(0,1);
        const std::string id=name.substring(0,name.length()-5).c_str();
        uint8_t header[24];pet::app::MediaInfo info;
        if(!pet::app::validAssetId(id)||f.read(header,24)!=24||!pet::app::parseMediaHeader(header,f.size(),info))continue;
        std::string title=id;File meta=LittleFS.open(path(id,".json").c_str(),"r");
        if(meta){JsonDocument doc;if(!deserializeJson(doc,meta)&&doc["title"].is<const char*>())title=doc["title"].as<std::string>();meta.close();}
        auto item=array.add<JsonObject>();item["id"]=id;item["title"]=title;item["bytes"]=f.size();item["frames"]=info.frames;item["fps"]=info.fps;
    }
}
void AppService::receiveBle(const std::string& fragment){
    for(char c:fragment){
        if(c=='\n'){
            if(!rxOverflow_&&!rx_.empty()){
                auto* message=new(std::nothrow)BleRequest{std::move(rx_),bleGeneration_};rx_.clear();
                if(message&&xQueueSend(bleQueue_,&message,0)!=pdTRUE)delete message;
            }
            rx_.clear();rxOverflow_=false;
        }else if(!rxOverflow_){if(rx_.size()<maxJson)rx_+=c;else{rx_.clear();rxOverflow_=true;}}
    }
}
void AppService::resetBle(){rx_.clear();rxOverflow_=false;++bleGeneration_;acknowledgeBle(false);}
void AppService::bleWorker(void* self){
    auto& owner=*static_cast<AppService*>(self);
    for(;;){
        BleRequest* message=nullptr;
        if(xQueueReceive(owner.bleQueue_,&message,portMAX_DELAY)!=pdTRUE)continue;
        const auto generation=message->generation;
        if(generation!=owner.bleGeneration_){delete message;continue;}
        const auto response=owner.dispatch(message->value)+"\n";delete message;
        const auto peers=owner.bleServer_->getPeerDevices();
        const unsigned mtu=peers.empty()?23:owner.bleServer_->getPeerMTU(peers.front());
        const size_t step=std::min<unsigned>(180,mtu>23?mtu-3:20);
        for(size_t offset=0;offset<response.size();offset+=step){
            if(!owner.bleServer_->getConnectedCount()||generation!=owner.bleGeneration_)break;
            xSemaphoreTake(owner.bleAck_,0);owner.ackOk_=false;
            owner.responses_->setValue(response.substr(offset,step));owner.responses_->notify(false);
            if(xSemaphoreTake(owner.bleAck_,pdMS_TO_TICKS(2000))!=pdTRUE||!owner.ackOk_)break;
        }
    }
}
