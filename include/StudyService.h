#pragma once
#include "StudyCodec.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <atomic>

class StudyService {
public:
    bool begin();
    void poll(pet::study::Snapshot& output);
    bool read(pet::study::Snapshot& output);
    void execute(JsonVariantConst request,JsonObject reply);
    bool control(const char* operation,uint32_t expectedRevision,const pet::study::Config* config=nullptr);
    bool active() const { return active_; }
private:
    bool save(uint32_t now);
    SemaphoreHandle_t mutex_=nullptr;
    pet::study::Timer timer_;
    uint32_t lastSave_=0,lastAttempt_=0,savedRevision_=0;
    bool persisted_=true;
    std::atomic<bool> active_{false};
};
