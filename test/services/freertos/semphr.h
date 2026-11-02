#pragma once
#include "FreeRTOS.h"
using SemaphoreHandle_t=void*;
inline SemaphoreHandle_t xSemaphoreCreateMutex(){static int mutex;return &mutex;}
inline int xSemaphoreTake(SemaphoreHandle_t,TickType_t){return pdTRUE;}
inline void xSemaphoreGive(SemaphoreHandle_t){}
