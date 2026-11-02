#pragma once
#include "FaceRenderer.h"
namespace pet {
enum class LittleActivity : uint8_t { Reading, Breathing, Stretching, Starlight, Count };
void drawLittleActivity(FaceRenderer& faces,Canvas& out,LittleActivity activity,Tick now,Tick age,uint8_t humor);
}
