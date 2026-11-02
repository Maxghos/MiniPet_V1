#pragma once
#include "PetCore.h"
#include <ArduinoJson.h>
#include <string>

namespace pet::telegram {
bool validToken(const std::string& token);
bool parseClock(const std::string& value,unsigned& minute);
struct PublicState {
    bool configured=false,linked=false,enabled=true,ready=false,pairing=false;
    std::string username;int code=0;
    bool studyAlerts=true,reminderAlerts=true,summaryAlerts=true;
};
void publicState(JsonObject output,const PublicState& state);
enum class Kind { Ignore, RejectedPair, Paired, Command };
struct Input { Kind kind=Kind::Ignore; int64_t chat=0,user=0; std::string text,callbackId; bool callback=false; };
class Gate {
public:
    void owner(int64_t chat,int64_t user){chat_=chat;user_=user;}
    int64_t chat() const {return chat_;}
    int64_t user() const {return user_;}
    void open(const std::string& pin,Tick now){pin_=pin;opened_=now;attempts_=0;open_=true;}
    void close(){open_=false;pin_.clear();}
    bool pairing(Tick now) const {return open_&&attempts_<5&&elapsed(now,opened_)<120000;}
    void drain(int64_t update){if(update>=0&&update<INT64_MAX&&update>=lastUpdate_)lastUpdate_=update;}
    int64_t offset() const {return lastUpdate_+1;}
    Input accept(JsonVariantConst update,int64_t epoch,Tick now);
private:
    int64_t chat_=0,user_=0,lastUpdate_=-1;
    Tick opened_=0;unsigned attempts_=0;bool open_=false;
    std::string pin_;
};
// Produces only a fixed allowlist of commands; arbitrary app RPCs cannot be sent.
bool command(const std::string& text,bool callback,JsonObject request,std::string& error);
void keyboard(JsonObject output,JsonVariantConst study,bool confirm=false,const char* action="");
}
