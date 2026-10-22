#pragma once
#include "WeatherData.h"
#include <ArduinoJson.h>
#include <cstring>

// Decode into a temporary object; a partial/bad response never replaces the cache.
inline bool decodeWeather(const JsonDocument& doc, WeatherData& output) {
    auto c = doc["current"];
    auto daily = doc["daily"];
    if (c.isNull() || daily["time"].size() < 3) return false;
    auto number = [](JsonVariantConst v, float lo, float hi) {
        return v.is<float>() && std::isfinite(v.as<float>()) && v.as<float>() >= lo && v.as<float>() <= hi;
    };
    if (!number(c["temperature_2m"],-100,70) || !number(c["apparent_temperature"],-120,90) ||
        !number(c["relative_humidity_2m"],0,100) || !number(c["wind_speed_10m"],0,500) ||
        !number(c["weather_code"],0,99)) return false;
    WeatherData w;
    w.temperature = c["temperature_2m"];
    w.feels = c["apparent_temperature"];
    w.humidity = c["relative_humidity_2m"];
    w.wind = c["wind_speed_10m"];
    w.code = c["weather_code"];
    for (unsigned i = 0; i < 3; ++i) {
        const char* date = daily["time"][i];
        if (!date || std::strlen(date) != 10 || date[4] != '-' || date[7] != '-') return false;
        if (i && std::strcmp(date,w.days[i-1].date) <= 0) return false;
        if (!number(daily["temperature_2m_min"][i],-100,70) || !number(daily["temperature_2m_max"][i],-100,70) ||
            !number(daily["precipitation_probability_max"][i],0,100) || !number(daily["weather_code"][i],0,99)) return false;
        auto& d = w.days[i];
        std::memcpy(d.date,date,11);
        d.min = daily["temperature_2m_min"][i]; d.max = daily["temperature_2m_max"][i];
        if (d.min > d.max) return false;
        d.rain = daily["precipitation_probability_max"][i]; d.code = daily["weather_code"][i];
    }
    w.valid = true;
    output = w;
    return true;
}
