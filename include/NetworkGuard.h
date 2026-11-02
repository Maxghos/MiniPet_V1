#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
class NetworkGuard {
public:
    static bool begin();
    explicit NetworkGuard(unsigned waitMs=20000);
    ~NetworkGuard();
    explicit operator bool() const {return locked_;}
private: bool locked_=false;
};
