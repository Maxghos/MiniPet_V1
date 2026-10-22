#pragma once
#include <cstdint>
#include <cmath>

struct ForecastDay {
    char date[11]{};
    float min = 0, max = 0;
    uint8_t rain = 0;
    uint16_t code = 0;
};
struct WeatherData {
    bool valid = false;
    bool online = false;
    float temperature = 0, feels = 0, wind = 0;
    uint8_t humidity = 0;
    uint16_t code = 0;
    uint32_t fetchedMs = 0;
    int64_t fetchedEpoch = 0;
    ForecastDay days[3]{};
};

// Human-readable labels for WMO weather codes (ASCII for the 6x8 OLED font).
inline const char* weatherLabel(unsigned code) {
    if (code == 0) return "Despejado";
    if (code <= 2) return "Algo nublado";
    if (code == 3) return "Nublado";
    if (code == 45 || code == 48) return "Niebla";
    if (code >= 51 && code <= 57) return "Llovizna";
    if (code >= 61 && code <= 67) return "Lluvia";
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) return "Nieve";
    if (code >= 80 && code <= 82) return "Chubascos";
    if (code >= 95 && code <= 99) return "Tormenta";
    return "Variable";
}
