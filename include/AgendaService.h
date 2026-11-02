#pragma once
#include "Agenda.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <deque>
#include <atomic>

struct AgendaNotice {
    uint32_t id=0,created=0,ttl=900000;
    std::string text;
    uint8_t kind=0,attempts=0; // 0 reminder, 1 summary, 2 study
};
pet::agenda::Moment agendaMoment();
class AgendaService {
public:
    using MomentClock=pet::agenda::Moment(*)();
    bool begin(bool storage,MomentClock clock=agendaMoment);
    void poll(const pet::study::Snapshot& study,const pet::agenda::Totals& totals,const pet::Settings& settings);
    void execute(JsonVariantConst request,JsonObject reply);
    bool takeDisplay(std::string& output);
    bool peekTelegram(AgendaNotice& output);
    void finishTelegram(uint32_t id,bool delivered);
    void clearTelegram();
    bool companionNotice(std::string& output,uint8_t humor,bool allow);
    bool scenes() const { return scenes_; }
private:
    bool load(const char* path);
    bool save();
    void enqueue(const std::string& text,uint8_t kind,uint32_t ttl);
    SemaphoreHandle_t mutex_=nullptr;
    pet::agenda::Book book_;
    std::deque<AgendaNotice> telegram_,display_;
    uint32_t nextNotice_=1,lastPoll_=0,lastSave_=0,lastAttempt_=0,studyEvent_=0;
    bool storage_=false,persisted_=true,seenStudy_=false;
    pet::agenda::Totals observed_;
    bool dirtyStats_=false;
    bool pendingSave_=false,primaryValid_=false,recovered_=false,loadError_=false;
    std::atomic<bool> scenes_{true};
    MomentClock clock_=agendaMoment;
    uint32_t began_=0,lastCompanion_=0,lastCompanionCheck_=0;
    bool companionSpoke_=false;
};
