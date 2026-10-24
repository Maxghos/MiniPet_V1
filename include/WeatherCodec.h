#pragma once
#include "WeatherData.h"
#include <ArduinoJson.h>
#include <cstring>

inline int64_t weatherEpoch(const char* value, int offset) {
    if (!value || std::strlen(value)!=16 || value[4]!='-' || value[7]!='-' || value[10]!='T' || value[13]!=':') return 0;
    for (unsigned i=0;i<16;++i) if (i!=4 && i!=7 && i!=10 && i!=13 && (value[i]<'0' || value[i]>'9')) return 0;
    auto two=[value](unsigned i){ return (value[i]-'0')*10+value[i+1]-'0'; };
    int y=two(0)*100+two(2), m=two(5), d=two(8), h=two(11), minute=two(14);
    if (y<2020 || y>2100 || m<1 || m>12 || h>23 || minute>59) return 0;
    static constexpr int lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};
    int days=lengths[m-1]+(m==2 && y%4==0 && (y%100!=0 || y%400==0));
    if (d<1 || d>days) return 0;
    y-=m<=2;
    const int era=y/400, yo=y-era*400;
    const int dayOfYear=(153*(m+(m>2 ? -3 : 9))+2)/5+d-1;
    const int dayOfEra=yo*365+yo/4-yo/100+dayOfYear;
    return int64_t(era*146097+dayOfEra-719468)*86400+h*3600+minute*60-offset;
}

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
    if (!doc["utc_offset_seconds"].is<int>()) return false;
    int offset=doc["utc_offset_seconds"];
    if (offset < -43200 || offset > 50400) return false;
    w.observedEpoch=weatherEpoch(c["time"],offset);
    if (!w.observedEpoch) return false;
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
