#include "NetworkGuard.h"
namespace {SemaphoreHandle_t tlsMutex=nullptr;}
bool NetworkGuard::begin(){if(!tlsMutex)tlsMutex=xSemaphoreCreateMutex();return tlsMutex!=nullptr;}
NetworkGuard::NetworkGuard(unsigned waitMs){locked_=tlsMutex&&xSemaphoreTake(tlsMutex,pdMS_TO_TICKS(waitMs))==pdTRUE;}
NetworkGuard::~NetworkGuard(){if(locked_)xSemaphoreGive(tlsMutex);}
