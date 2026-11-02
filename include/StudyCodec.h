#pragma once
#include "StudyTimer.h"
#include <ArduinoJson.h>

namespace pet::study {
void writeConfig(JsonObject output,const Config& config);
bool readConfig(JsonVariantConst input,Config& config);
void writeSnapshot(JsonObject output,const Snapshot& value);
void writeRecord(JsonObject output,const Record& record);
bool readRecord(JsonVariantConst input,Record& record);
// Authentication and locking belong to AppService/StudyService. This common
// handler is also exercised by host tests and used by the physical settings menu.
bool command(Timer& timer,JsonVariantConst request,JsonObject reply,Tick now);
}
