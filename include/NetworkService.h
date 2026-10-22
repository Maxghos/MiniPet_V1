#pragma once
#include "WeatherData.h"
#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class NetworkService {
public:
    bool begin();
    WeatherData weather();
    void poll();
    void startPortal();
    void stopPortal();
    bool portalActive() const { return portal_; }
    const char* apName() const { return apName_; }
    const char* apPassword() const { return apPassword_; }
    bool connected() const;
private:
    static void taskEntry(void* ctx);
    void run();
    void fetchWeather();
    void submitWifi();
    SemaphoreHandle_t mutex_ = nullptr;
    TaskHandle_t task_ = nullptr;
    WeatherData weather_{};
    char ssid_[33]{}, password_[64]{};
    bool credentialsChanged_ = false;
    bool portal_ = false;
    uint32_t portalAt_ = 0;
    char apName_[20]{}, apPassword_[16]{};
    WebServer server_{80};
    DNSServer dns_;
};
