#include "OtaService.h"
#include "FirmwareVersion.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>

namespace {
#ifdef MINIPET_MECHANICAL_BUTTON
const char buildMarker[]="\nMPFW-TARGET:minipet-s3-n16r8-button\n";
#else
const char buildMarker[]="\nMPFW-TARGET:minipet-s3-n16r8-touch\n";
#endif
const char versionMarker[]="\nMPFW-VERSION:" MINIPET_VERSION "\n";
constexpr size_t targetPrefix=13,versionPrefix=14;
struct RequestState {int code=0;char error[128]{};};
void reject(RequestState* state,int code,const char* error){state->code=code;snprintf(state->error,sizeof(state->error),"%s",error);}
void respond(AsyncWebServerRequest* request,int code,const char* error){
    JsonDocument doc;doc["ok"]=code==200;
    if(code==200)doc["reboot"]=true;else doc["error"]=error;
    String data;serializeJson(doc,data);auto* response=request->beginResponse(code,"application/json; charset=utf-8",data);
    response->addHeader("Connection","close");request->send(response);
}
}
const char* OtaService::target(){
#ifdef MINIPET_MECHANICAL_BUTTON
    return "minipet-s3-n16r8-button";
#else
    return "minipet-s3-n16r8-touch";
#endif
}
bool OtaService::begin(AsyncWebServer& server,Authorizer authorize,StudyService& study){
    mutex_=xSemaphoreCreateMutex();if(!mutex_)return false;
    authorize_=std::move(authorize);study_=&study;
    partition_=esp_ota_get_next_update_partition(nullptr);
    server.on("/api/v1/firmware",HTTP_POST,[this](AsyncWebServerRequest* request){finish(request);},nullptr,
        [this](AsyncWebServerRequest* request,uint8_t* data,size_t len,size_t index,size_t total){body(request,data,len,index,total);});
    return true;
}
bool OtaService::openWindow(){
    if(!mutex_||xSemaphoreTake(mutex_,pdMS_TO_TICKS(20))!=pdTRUE)return false;
    const bool allowed=partition_&&!blocked_&&!busy_&&!reboot_&&!study_->active();
    if(allowed){
        snprintf(pin_,sizeof(pin_),"%06lu",static_cast<unsigned long>(esp_random()%1000000));
        opened_=millis();window_=true;attempts_=0;error_.clear();progress_=0;received_=expected_=0;
    }
    xSemaphoreGive(mutex_);return allowed;
}
unsigned OtaService::windowSeconds() const{
    const uint32_t age=millis()-opened_;return window_&&age<120000?(120000-age)/1000:0;
}
void OtaService::abort(){
    if(handle_){esp_ota_abort(handle_);handle_=0;}
    if(hashing_){mbedtls_sha256_free(&sha_);hashing_=false;}
    owner_=nullptr;busy_=false;window_=false;
}
void OtaService::fail(const char* text){error_=text;abort();}
void OtaService::cancelWindow(){
    if(!mutex_||xSemaphoreTake(mutex_,pdMS_TO_TICKS(20))!=pdTRUE)return;
    if(!reboot_){if(busy_)fail("Actualizacion cancelada desde la OLED.");else window_=false;}
    xSemaphoreGive(mutex_);
}
void OtaService::poll(){
    if(!mutex_||xSemaphoreTake(mutex_,pdMS_TO_TICKS(2))!=pdTRUE)return;
    const uint32_t now=millis();
    if(window_&&uint32_t(now-opened_)>=120000)window_=false;
    if(busy_&&(uint32_t(now-lastChunk_)>=30000||uint32_t(now-startedUpload_)>=300000))fail("Transferencia sin completar. Abre otra ventana e intenta de nuevo.");
    const bool restart=reboot_&&uint32_t(now-rebootAt_)>=3000;
    xSemaphoreGive(mutex_);
    if(restart)ESP.restart();
}
void OtaService::status(JsonObject output){
    if(!mutex_||xSemaphoreTake(mutex_,pdMS_TO_TICKS(20))!=pdTRUE){output["error"]="Actualizacion ocupada";return;}
    // Referencing complete marker arrays keeps their prefixes in the binary package.
    output["target"]=std::string(buildMarker+targetPrefix,sizeof(buildMarker)-targetPrefix-2);
    output["firmware"]=std::string(versionMarker+versionPrefix,sizeof(versionMarker)-versionPrefix-2);
    output["maxImageBytes"]=partition_?partition_->size:0;
    output["window"]=windowOpen();output["seconds"]=windowSeconds();output["busy"]=busy_.load();
    output["received"]=received_;output["expected"]=expected_;output["progress"]=progress_.load();output["error"]=error_;
    output["rebootPending"]=reboot_.load();output["phase"]=reboot_?"rebooting":busy_?"receiving":!error_.empty()?"error":windowOpen()?"ready":"idle";
    xSemaphoreGive(mutex_);
}
void OtaService::body(AsyncWebServerRequest* request,uint8_t* data,size_t len,size_t index,size_t total){
    if(index==0){
        auto* state=static_cast<RequestState*>(calloc(1,sizeof(RequestState)));request->_tempObject=state;
        if(!state)return;
        request->onDisconnect([this,request](){disconnected(request);});
        std::string supplied;
        if(request->hasHeader("Authorization")){
            const auto value=request->getHeader("Authorization")->value();
            if(value.startsWith("Bearer "))supplied=value.substring(7).c_str();
        }
        // Authenticate before taking our mutex: app RPCs acquire the locks in the other order.
        if(!authorize_(supplied)){reject(state,401,"Vincula la app antes de actualizar.");return;}
        if(!request->contentType().equalsIgnoreCase("application/octet-stream")||request->multipart()||
           !partition_||total<pet::ota::headerBytes+1024||total-pet::ota::headerBytes>partition_->size||request->contentLength()!=total){
            reject(state,400,"Selecciona un paquete MPFW valido para esta placa.");return;
        }
        if(xSemaphoreTake(mutex_,pdMS_TO_TICKS(2000))!=pdTRUE){reject(state,409,"Actualizacion ocupada.");return;}
        if(busy_||reboot_){reject(state,409,"Ya hay una actualizacion en curso.");}
        else if(!windowOpen()||blocked_||study_->active()){reject(state,403,"Termina estudio/juego y abre Actualizar WiFi en la OLED.");}
        else{
            const String code=request->hasHeader("X-MiniPet-Code")?request->getHeader("X-MiniPet-Code")->value():String();
            ++attempts_;
            if(code.length()!=6||memcmp(code.c_str(),pin_,6)){
                if(attempts_>=5)window_=false;
                reject(state,403,"PIN de actualizacion incorrecto.");
            }else{
                owner_=request;bodyOffset_=0;prefixSize_=0;received_=0;expected_=total-pet::ota::headerBytes;
                header_.fill(0);prefix_.fill(0);package_={};error_.clear();progress_=0;
                busy_=true;window_=false;startedUpload_=lastChunk_=millis();
            }
        }
        xSemaphoreGive(mutex_);
    }
    auto* state=static_cast<RequestState*>(request->_tempObject);
    if(!state||state->code)return;
    if(xSemaphoreTake(mutex_,pdMS_TO_TICKS(2000))!=pdTRUE){reject(state,409,"Actualizacion ocupada.");return;}
    if(owner_!=request){reject(state,409,error_.empty()?"Transferencia cancelada.":error_.c_str());}
    else if(index!=bodyOffset_||len>total-index||uint32_t(millis()-startedUpload_)>=300000){
        fail("Paquete incompleto o fuera de secuencia.");reject(state,400,error_.c_str());
    }else{
        lastChunk_=millis();size_t at=0;
        if(bodyOffset_<pet::ota::headerBytes){
            const size_t bytes=std::min(len,pet::ota::headerBytes-bodyOffset_);
            memcpy(header_.data()+bodyOffset_,data,bytes);bodyOffset_+=bytes;at+=bytes;
            if(bodyOffset_==pet::ota::headerBytes){
                if(!pet::ota::parsePackage(header_.data(),header_.size(),total,target(),partition_->size,package_)){
                    fail("Cabecera, placa, tamano o version de paquete incorrectos.");reject(state,400,error_.c_str());
                }else{
                    targetMarker_.reset("\nMPFW-TARGET:"+package_.target+"\n");
                    versionMarker_.reset("\nMPFW-VERSION:"+package_.version+"\n");
                }
            }
        }
        if(!state->code&&at<len){
            if(!write(data+at,len-at)){reject(state,400,error_.c_str());}
            else bodyOffset_+=len-at;
        }
    }
    xSemaphoreGive(mutex_);
}
bool OtaService::write(const uint8_t* data,size_t len){
    if(received_+len>expected_){fail("La imagen supera su tamano declarado.");return false;}
    targetMarker_.add(data,len);versionMarker_.add(data,len);
    if(prefixSize_<prefix_.size()){
        const size_t take=std::min(len,prefix_.size()-prefixSize_);
        memcpy(prefix_.data()+prefixSize_,data,take);prefixSize_+=take;received_+=take;data+=take;len-=take;
        if(prefixSize_<prefix_.size())return true;
        if(!pet::ota::validImagePrefix(prefix_.data(),prefix_.size())){fail("La imagen no es ESP32-S3 N16R8.");return false;}
        const auto* running=esp_ota_get_running_partition();
        if(!running||partition_->address==running->address||esp_ota_begin(partition_,OTA_WITH_SEQUENTIAL_WRITES,&handle_)!=ESP_OK){
            fail("No se pudo abrir la particion de actualizacion.");return false;
        }
        mbedtls_sha256_init(&sha_);hashing_=true;
        if(mbedtls_sha256_starts_ret(&sha_,0)||esp_ota_write(handle_,prefix_.data(),prefix_.size())!=ESP_OK||
           mbedtls_sha256_update_ret(&sha_,prefix_.data(),prefix_.size())){fail("Fallo al escribir el firmware.");return false;}
    }
    if(len&&(esp_ota_write(handle_,data,len)!=ESP_OK||mbedtls_sha256_update_ret(&sha_,data,len))){fail("Fallo al escribir el firmware.");return false;}
    received_+=len;progress_=expected_?unsigned(uint64_t(received_)*100/expected_):0;return true;
}
void OtaService::finish(AsyncWebServerRequest* request){
    auto* state=static_cast<RequestState*>(request->_tempObject);
    if(!state){respond(request,400,"No se recibio un paquete de firmware.");return;}
    if(state->code){disconnected(request);respond(request,state->code,state->error);return;}
    if(xSemaphoreTake(mutex_,pdMS_TO_TICKS(2000))!=pdTRUE){respond(request,409,"Actualizacion ocupada.");return;}
    int code=400;std::string message;
    if(owner_!=request){message=error_.empty()?"Transferencia cancelada.":error_;}
    else if(!handle_||bodyOffset_!=request->contentLength()||received_!=expected_||!targetMarker_.found()||!versionMarker_.found()){
        fail("Imagen incompleta o sin identificacion MiniPet compatible.");message=error_;
    }else{
        std::array<uint8_t,32> digest{};
        const bool hashOk=!mbedtls_sha256_finish_ret(&sha_,digest.data());mbedtls_sha256_free(&sha_);hashing_=false;
        unsigned different=0;for(size_t i=0;i<digest.size();++i)different|=digest[i]^package_.digest[i];
        if(!hashOk||different){fail("SHA-256 incorrecto. Se conserva el firmware actual.");message=error_;}
        else{
            const auto handle=handle_;handle_=0;
            if(esp_ota_end(handle)!=ESP_OK){fail("El verificador ESP rechazo la imagen.");message=error_;}
            else if(esp_ota_set_boot_partition(partition_)!=ESP_OK){fail("No se pudo seleccionar el nuevo firmware.");message=error_;}
            else{owner_=nullptr;busy_=false;reboot_=true;rebootAt_=millis();code=200;}
        }
    }
    xSemaphoreGive(mutex_);respond(request,code,message.c_str());
}
void OtaService::disconnected(AsyncWebServerRequest* request){
    if(!mutex_||xSemaphoreTake(mutex_,portMAX_DELAY)!=pdTRUE)return;
    if(owner_==request)fail("Conexion interrumpida. Se conserva el firmware actual.");
    xSemaphoreGive(mutex_);
}
