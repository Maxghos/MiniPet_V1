#pragma once
#include "AppProtocol.h"
#include "StudyService.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <NimBLEDevice.h>
#include <FS.h>
#include <atomic>
#include <freertos/semphr.h>
#include <freertos/queue.h>

// Wi-Fi and BLE share a bounded command protocol; the display loop owns settings.
class AppService {
public:
    bool begin(const pet::Settings& settings,StudyService& study);
    void poll();
    void openPairing();
    void closePairing();
    bool pairing() const;
    const char* pin() const { return pin_; }
    unsigned pairingSeconds() const;
    void updateSettings(const pet::Settings& settings);
    bool takeSettings(pet::Settings& settings);
    bool takeInteraction();
    bool takePaired() { return paired_.exchange(false); }
    void localBusy(bool value) { localBusy_=value; }
    void resetBle();
    void acknowledgeBle(bool success) { ackOk_=success; xSemaphoreGive(bleAck_); }
    bool recentlyContacted() const { return contacted_ && uint32_t(millis()-lastContact_)<15000; }
    void stop();
    bool active();
    void read(pet::app::RemoteView& view,uint32_t now);
    std::string dispatch(const std::string& json,const std::string& bearer="");
    void receiveBle(const std::string& fragment);
private:
    static void bleWorker(void* self);
    void execute(JsonVariantConst request,JsonDocument& response,const std::string& bearer);
    bool play(const char* id,int32_t duration,bool loop);
    bool commitUpload();
    void library(JsonArray array);
    unsigned assetCount();
    void cancelUpload();
    AsyncWebServer server_{8080};
    StudyService* study_=nullptr;
    SemaphoreHandle_t mutex_=nullptr;
    QueueHandle_t bleQueue_=nullptr;
    SemaphoreHandle_t bleAck_=nullptr;
    std::atomic<bool> ackOk_{false};
    NimBLECharacteristic* responses_=nullptr;
    NimBLEServer* bleServer_=nullptr;
    pet::Settings settings_;
    pet::app::Playback playback_;
    bool pendingSettings_=false,interaction_=false,fsReady_=false,mdns_=false;
    uint8_t* media_=nullptr;
    char pin_[7]{};
    std::string token_,rx_,playingId_;
    std::atomic<uint32_t> pairAt_{0};
    uint32_t lastMdns_=0;
    std::atomic<bool> pairing_{false};
    std::atomic<bool> paired_{false},contacted_{false};
    std::atomic<bool> localBusy_{false};
    std::atomic<bool> stopRequested_{false};
    std::atomic<uint32_t> lastContact_{0};
    std::atomic<uint32_t> bleGeneration_{0};
    bool rxOverflow_=false;
    unsigned attempts_=0;
    File upload_;
    std::string uploadId_,uploadTitle_;
    size_t uploadExpected_=0,uploadOffset_=0;
    uint32_t uploadAt_=0;
};
