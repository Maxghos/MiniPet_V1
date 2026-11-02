#include "Sensors.h"
#include "BoardConfig.h"

namespace {
portMUX_TYPE echoMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t triggeredAt = 0,risingAt = 0, pulseWidth = 0;
volatile bool accepting = false, rose = false, ready = false;
constexpr uint32_t echoWindowUs=28000;
}
void Sensors::begin() {
    pinMode(board::trigger,OUTPUT); digitalWrite(board::trigger,LOW);
    pinMode(board::echo,INPUT); pinMode(board::pir,INPUT);
#ifdef MINIPET_MECHANICAL_BUTTON
    pinMode(board::touch,INPUT_PULLUP);
#else
    pinMode(board::touch,INPUT);
#endif
    pending_=valid=false;cm=rawCm=0;filter_=pet::DistanceFilter{};
    lastTrigger_=sentAt_=millis();pir_.begin(millis(),board::pirWarmupMs);
    portENTER_CRITICAL(&echoMux);accepting=rose=ready=false;portEXIT_CRITICAL(&echoMux);
    attachInterrupt(digitalPinToInterrupt(board::echo),echoEdge,CHANGE);
}
void IRAM_ATTR Sensors::echoEdge() {
    const bool high = digitalRead(board::echo);
    const uint32_t now = micros();
    portENTER_CRITICAL_ISR(&echoMux);
    if (accepting) {
        if(uint32_t(now-triggeredAt)>=echoWindowUs){accepting=rose=false;}
        else if (high&&!rose) { risingAt = now; rose = true; }
        else if (!high&&rose) {
            const uint32_t width=now-risingAt;rose=false;
            // Ignore a short electrical spike and keep listening in this window.
            if(width>=100){pulseWidth=width;ready=true;accepting=false;}
        }
    }
    portEXIT_CRITICAL_ISR(&echoMux);
}
bool Sensors::poll(uint32_t now) {
    bool done = false;
    uint32_t width = 0;
    portENTER_CRITICAL(&echoMux);
    if(pending_&&(ready||pet::elapsed(now,sentAt_)>=28)){
        width=ready?pulseWidth:0;
        ready=rose=accepting=false;done=true;
    }
    portEXIT_CRITICAL(&echoMux);
    if (done) {
        pending_ = false;
        rawCm=width*.0343f/2.f;
        valid=width&&pet::elapsed(now,sentAt_)<board::distanceMs&&filter_.sample(rawCm,now);
        if (valid) cm = filter_.cm();
        return true;
    }
    if (!pending_ && pet::elapsed(now,lastTrigger_) >= board::distanceMs) {
        lastTrigger_=now;
        // A previous/stuck HIGH echo is not a new measurement. Keep the loop
        // responsive and retry once the sensor returns LOW, without driving ECHO.
        if(digitalRead(board::echo)){valid=false;rawCm=0;return true;}
        portENTER_CRITICAL(&echoMux);
        ready = rose = false;triggeredAt=micros();accepting = true;
        portEXIT_CRITICAL(&echoMux);
        digitalWrite(board::trigger,HIGH); delayMicroseconds(10); digitalWrite(board::trigger,LOW);
        sentAt_ = now; pending_ = true;
    }
    return false;
}
bool Sensors::motion(uint32_t now) {
    return pir_.update(digitalRead(board::pir),now);
}
bool Sensors::touch() const {
#ifdef MINIPET_MECHANICAL_BUTTON
    return !digitalRead(board::touch);
#else
    return digitalRead(board::touch);
#endif
}
