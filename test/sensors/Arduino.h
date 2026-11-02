#pragma once
#include <cstdint>
#define IRAM_ATTR
using portMUX_TYPE=int;
constexpr int portMUX_INITIALIZER_UNLOCKED=0;
constexpr int LOW=0,HIGH=1,INPUT=0,OUTPUT=1,INPUT_PULLUP=2,CHANGE=3;
namespace fake {
inline uint64_t clockUs=0;
inline int pins[64]{},modes[64]{};
inline unsigned triggers=0;
inline void (*interrupt)()=nullptr;
inline void reset(){clockUs=0;triggers=0;interrupt=nullptr;for(auto& p:pins)p=0;for(auto& p:modes)p=INPUT;}
}
inline uint32_t millis(){return uint32_t(fake::clockUs/1000);}
inline uint32_t micros(){return uint32_t(fake::clockUs);}
inline void pinMode(int pin,int mode){fake::modes[pin]=mode;}
inline int digitalRead(int pin){return fake::pins[pin];}
inline void digitalWrite(int pin,int value){if(pin==5&&value==HIGH)++fake::triggers;fake::pins[pin]=value;}
inline void delayMicroseconds(unsigned value){fake::clockUs+=value;}
inline int digitalPinToInterrupt(int pin){return pin;}
inline void attachInterrupt(int,void (*callback)(),int){fake::interrupt=callback;}
inline void portENTER_CRITICAL(portMUX_TYPE*){}
inline void portEXIT_CRITICAL(portMUX_TYPE*){}
inline void portENTER_CRITICAL_ISR(portMUX_TYPE*){}
inline void portEXIT_CRITICAL_ISR(portMUX_TYPE*){}
