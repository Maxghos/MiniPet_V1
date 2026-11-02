#pragma once
#include "OtaPackage.h"
#include "StudyService.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>
#include <freertos/semphr.h>
#include <atomic>
#include <functional>

class OtaService {
public:
    using Authorizer=std::function<bool(const std::string&)>;
    bool begin(AsyncWebServer& server,Authorizer authorize,StudyService& study);
    bool openWindow();
    void cancelWindow();
    bool windowOpen() const{return window_&&uint32_t(millis()-opened_)<120000;}
    unsigned windowSeconds() const;
    const char* pin() const{return pin_;}
    bool busy() const{return busy_;}
    void block(bool value){blocked_=value;}
    bool rebootPending() const{return reboot_;}
    unsigned progress() const{return progress_;}
    void poll();
    void status(JsonObject output);
    static const char* target();
private:
    void body(AsyncWebServerRequest* request,uint8_t* data,size_t len,size_t index,size_t total);
    void finish(AsyncWebServerRequest* request);
    void disconnected(AsyncWebServerRequest* request);
    bool write(const uint8_t* data,size_t len);
    void fail(const char* text);
    void abort();
    SemaphoreHandle_t mutex_=nullptr;
    Authorizer authorize_;StudyService* study_=nullptr;
    AsyncWebServerRequest* owner_=nullptr;
    const esp_partition_t* partition_=nullptr;
    esp_ota_handle_t handle_=0;
    mbedtls_sha256_context sha_{};
    bool hashing_=false;
    std::array<uint8_t,128> header_{};
    std::array<uint8_t,24> prefix_{};
    pet::ota::Package package_;
    pet::ota::Marker targetMarker_,versionMarker_;
    size_t bodyOffset_=0,prefixSize_=0,received_=0,expected_=0;
    uint32_t lastChunk_=0,startedUpload_=0,rebootAt_=0;
    std::atomic<uint32_t> opened_{0};
    std::atomic<bool> window_{false},busy_{false},reboot_{false},blocked_{false};
    std::atomic<unsigned> progress_{0};
    unsigned attempts_=0;
    char pin_[7]{};
    std::string error_;
};
