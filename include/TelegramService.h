#pragma once
#include "TelegramProtocol.h"
#include "AgendaService.h"
#include "StudyService.h"
#include <Preferences.h>
#include <atomic>

class TelegramService {
public:
    bool begin(StudyService& study,AgendaService& agenda);
    void execute(JsonVariantConst request,JsonObject reply,bool secureBle);
    bool openPairing();
    void closePairing();
    bool pairing();
    unsigned pairingSeconds();
    const char* pin() const {return pin_;}
    bool takePaired(){return paired_.exchange(false);}
    bool takeInteraction(){return interaction_.exchange(false);}
    void localBusy(bool value){localBusy_=value;}
    void quiet(bool value){quiet_=value;}
private:
    static void taskEntry(void* owner);
    void run();
    bool api(const std::string& token,const char* method,JsonDocument& args,JsonDocument& output);
    void handle(const pet::telegram::Input& input,const std::string& token);
    bool send(const std::string& token,int64_t chat,const std::string& text,JsonVariantConst keyboard=JsonVariantConst{});
    bool save(const std::string& token,int64_t chat,int64_t user,bool enabled,bool studyAlerts,bool reminderAlerts,bool summaryAlerts);
    void status(JsonObject out);
    SemaphoreHandle_t mutex_=nullptr;
    StudyService* study_=nullptr;AgendaService* agenda_=nullptr;
    std::string token_,username_;
    pet::telegram::Gate gate_;
    bool enabled_=true,studyAlerts_=true,reminderAlerts_=true,summaryAlerts_=true;
    char pin_[7]{};uint32_t pairAt_=0;
    std::atomic<uint32_t> generation_{0};
    std::atomic<bool> ready_{false},paired_{false},localBusy_{false};
    std::atomic<bool> quiet_{false},interaction_{false};
    std::atomic<int> code_{0};unsigned retryAfter_=3;
    uint32_t workerGeneration_=UINT32_MAX;
};
