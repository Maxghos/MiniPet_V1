#include "WeatherEffects.h"

namespace pet {
void drawWeatherReaction(FaceRenderer& faces, Canvas& c, const WeatherData& data,
                         Tick now, Tick age, uint8_t humor, bool flash) {
    const auto expression=weatherExpression(data);
    const bool wet=expression.condition==Condition::Rain || expression.condition==Condition::Drizzle || expression.condition==Condition::Snow;
    faces.draw(c,now,expression.mood,humor,FaceStyle::Count,wet ? -3 : 0);
    if (expression.cold) {
        const unsigned pace=humor<30 ? 260 : humor<70 ? 170 : 100;
        c.translate((age/pace)%2 ? 1 : -1,0);
        c.line(8,18,7,20); c.line(7,20,8,22);
    }
    if (expression.hot) {
        int y=3+int((age/(humor<30 ? 220 : 100))%10);
        c.line(116,y,113,y+5); c.line(116,y,119,y+5);
        c.line(113,y+5,114,y+7); c.line(114,y+7,118,y+7); c.line(118,y+7,119,y+5);
    }
    if (wet) {
        unsigned drops=humor<30 ? 1 : 3;
        for (unsigned i=0;i<drops;++i) {
            int x=i%2 ? 3 : 119;
            int y=int((age/65+i*11)%30);
            if (expression.condition==Condition::Snow) {
                c.line(x-2,y,x+2,y); c.line(x,y-2,x,y+2);
            } else c.line(x,y,x-1,y+3);
        }
    }
    if (expression.condition==Condition::Storm) {
        c.line(66,4,60,14); c.line(60,14,67,12); c.line(67,12,61,23);
        // Framebuffer inversion is self-restoring on the next draw and on interruption.
        if (flash && age<66) for (auto& byte:c.bytes) byte=uint8_t(~byte);
    }
}
}
