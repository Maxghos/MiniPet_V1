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
class RetryWindow {
public:
    void clear(){duration_=0;}
    void failure(Tick now,int code,unsigned retryAfter=30);
    void defer(Tick now,Tick wait){duration_=std::max(wait,remaining(now));started_=now;}
    Tick remaining(Tick now){const Tick age=elapsed(now,started_);if(age>=duration_){duration_=0;return 0;}return duration_-age;}
private: Tick started_=0,duration_=0;
};
enum class Kind { Ignore, RejectedPair, Paired, Command };
struct Input { Kind kind=Kind::Ignore; int64_t chat=0,user=0; std::string text,callbackId; bool callback=false; };
class Gate {
public:
    void owner(int64_t chat,int64_t user){chat_=chat;user_=user;}
    int64_t chat() const {return chat_;}
    int64_t user() const {return user_;}
    void open(const std::string& pin,Tick now){pin_=pin;opened_=now;attempts_=0;open_=true;}
    void close(){open_=false;pin_.clear();}
    bool pairing(Tick now){if(open_&&(attempts_>=5||elapsed(now,opened_)>=120000))close();return open_;}
    void drain(int64_t update,int64_t epoch=0){if(update>=0&&update<INT64_MAX&&(update>=lastUpdate_||idleWeek(epoch))){lastUpdate_=update;lastEpoch_=epoch;}}
    // Telegram may restart its update IDs after a week with no updates.
    int64_t offset(int64_t epoch=0) const {return idleWeek(epoch)?0:lastUpdate_+1;}
    Input accept(JsonVariantConst update,int64_t epoch,Tick now);
private:
    int64_t chat_=0,user_=0,lastUpdate_=-1;
    Tick opened_=0;unsigned attempts_=0;bool open_=false;
    std::string pin_;
    int64_t lastEpoch_=0;
    bool idleWeek(int64_t epoch) const {return lastEpoch_>=1704067200&&epoch>=lastEpoch_&&epoch-lastEpoch_>=7*86400;}
};
// Produces only a fixed allowlist of commands; arbitrary app RPCs cannot be sent.
bool command(const std::string& text,bool callback,JsonObject request,std::string& error);
void keyboard(JsonObject output,JsonVariantConst study,bool confirm=false,const char* action="");
}
