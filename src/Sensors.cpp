#include "Sensors.h"
#include "BoardConfig.h"

namespace {
portMUX_TYPE echoMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t risingAt = 0, pulseWidth = 0;
volatile bool accepting = false, rose = false, ready = false;
}
void Sensors::begin() {
    pinMode(board::trigger,OUTPUT); digitalWrite(board::trigger,LOW);
    pinMode(board::echo,INPUT); pinMode(board::pir,INPUT);
#ifdef MINIPET_MECHANICAL_BUTTON
    pinMode(board::touch,INPUT_PULLUP);
#else
    pinMode(board::touch,INPUT);
#endif
    attachInterrupt(digitalPinToInterrupt(board::echo),echoEdge,CHANGE);
}
void IRAM_ATTR Sensors::echoEdge() {
    const bool high = digitalRead(board::echo);
    const uint32_t now = micros();
    portENTER_CRITICAL_ISR(&echoMux);
    if (accepting) {
        if (high) { risingAt = now; rose = true; }
        else if (rose) { pulseWidth = now-risingAt; ready = true; accepting = false; }
    }
    portEXIT_CRITICAL_ISR(&echoMux);
}
bool Sensors::poll(uint32_t now) {
    bool done = false;
    uint32_t width = 0;
    portENTER_CRITICAL(&echoMux);
    if (ready) { width = pulseWidth; ready = false; done = true; }
    portEXIT_CRITICAL(&echoMux);
    if (pending_ && (done || pet::elapsed(now,sentAt_) >= 28)) {
        portENTER_CRITICAL(&echoMux); accepting = false; portEXIT_CRITICAL(&echoMux);
        pending_ = false;
        valid = width && filter_.sample(width * .0343f / 2.f,now);
        if (valid) cm = filter_.cm();
        return true;
    }
    if (!pending_ && pet::elapsed(now,lastTrigger_) >= board::distanceMs) {
        portENTER_CRITICAL(&echoMux);
        ready = rose = false; accepting = true;
        portEXIT_CRITICAL(&echoMux);
        digitalWrite(board::trigger,HIGH); delayMicroseconds(10); digitalWrite(board::trigger,LOW);
        lastTrigger_ = sentAt_ = now; pending_ = true;
    }
    return false;
}
bool Sensors::motion(uint32_t now) {
    bool high = digitalRead(board::pir);
    if (now < board::pirWarmupMs) { pirWas_ = false; return false; }
    bool edge = high && !pirWas_;
    pirWas_ = high;
    return edge;
}
bool Sensors::touch() const {
#ifdef MINIPET_MECHANICAL_BUTTON
    return !digitalRead(board::touch);
#else
    return digitalRead(board::touch);
#endif
}
