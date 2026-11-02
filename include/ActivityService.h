#pragma once
#include "MiniGames.h"
#include "StudyService.h"
#include "AgendaService.h"
#include <atomic>
class Screen;
class ActivityService {
public:
    bool begin(StudyService& study,AgendaService& agenda);
    void execute(JsonVariantConst request,JsonObject reply);
    bool startLocal(pet::games::Kind kind);
    void stop();
    void poll(uint32_t now,bool valid,float cm);
    void render(Screen& screen,uint32_t now);
    bool active() const{return active_;}
    void localBusy(bool value){busy_=value;}
    bool takeOverview(){return overview_.exchange(false);}
    bool takeInteraction(){return interaction_.exchange(false);}
private:
    void status(JsonObject output,uint32_t now);
    SemaphoreHandle_t mutex_=nullptr;
    StudyService* study_=nullptr;AgendaService* agenda_=nullptr;
    pet::games::Game game_;
    std::atomic<bool> active_{false},busy_{false},overview_{false},interaction_{false};
};
