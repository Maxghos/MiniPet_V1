#pragma once
#include "FaceRenderer.h"
#include "WeatherBehavior.h"

namespace pet {
void drawWeatherReaction(FaceRenderer& faces, Canvas& canvas, const WeatherData& data,
                         Tick now, Tick age, uint8_t humor, bool flash);
}
