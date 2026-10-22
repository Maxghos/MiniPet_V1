#pragma once
#include "FaceRenderer.h"
#include "WeatherData.h"
#include <Adafruit_SSD1306.h>

class Screen {
public:
    Screen() : display_(128,32,&Wire,-1) {}
    bool begin();
    void brightness(uint8_t value);
    void face(uint32_t now, pet::Mood mood, uint8_t humor, float progress);
    void message(const char* text);
    void clock(bool online);
    void weather(const WeatherData& weather, unsigned page, uint32_t now);
    void menu(const pet::Menu& menu, bool validDistance, float cm);
    void portal(const char* name, const char* password, bool online);
    void status(const char* top, const char* bottom);
    void progress(float value);
    void show();
private:
    void clear();
    void text(int x, int y, const char* value, unsigned size = 1);
    void icon(int x, int y, unsigned code);
    Adafruit_SSD1306 display_;
    pet::FaceRenderer faces_{0x93823};
    pet::Canvas canvas_;
};
