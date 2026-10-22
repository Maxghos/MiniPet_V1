#pragma once
#include "PetCore.h"
#include <Arduino.h>

class Sensors {
public:
    void begin();
    bool poll(uint32_t now); // New echo or timeout; never blocks for an echo.
    bool valid = false;
    float cm = 0;
    bool motion(uint32_t now);
    bool touch() const;
private:
    static void IRAM_ATTR echoEdge();
    uint32_t lastTrigger_ = 0, sentAt_ = 0;
    bool pending_ = false, pirWas_ = false;
    pet::DistanceFilter filter_;
};
