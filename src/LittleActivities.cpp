#include "LittleActivities.h"
namespace pet {
void drawLittleActivity(FaceRenderer& faces,Canvas& out,LittleActivity activity,Tick now,Tick age,uint8_t humor){
    constexpr float pi=3.14159265f;const float wave=std::sin(float(age)*pi/1000);
    faces.draw(out,now,activity==LittleActivity::Stretching?Mood::Happy:activity==LittleActivity::Reading?Mood::Attentive:Mood::Calm,humor,
        activity==LittleActivity::Reading?FaceStyle::Attentive:FaceStyle::Count);
    if(activity==LittleActivity::Reading){
        Canvas eyes=out;out.clear();
        for(int y=0;y<20;++y)for(int x=0;x<128;++x)if(static_cast<const Canvas&>(eyes).pixel(x,y*32/20))out.pixel(x,y);
        out.line(49,23,63,26);out.line(63,26,78,23);out.line(49,23,49,29);out.line(78,23,78,29);
        out.line(49,29,63,31);out.line(63,31,78,29);out.line(63,26,63,31);
        const int page=64+int((age/160)%10);out.line(64,26,page,21);
    }else if(activity==LittleActivity::Breathing){
        const int offset=int(wave*2);out.translate(0,offset);
        for(int i=0;i<3;++i){const int x=8+i*6;out.line(x,27,x+2,25-offset);out.line(117-x,27,115-x,25-offset);}
    }else if(activity==LittleActivity::Stretching){
        const int reach=3+int((wave+1)*3);
        out.line(3,27,3+reach,23);out.line(3+reach,23,15,27);
        out.line(124,27,124-reach,23);out.line(124-reach,23,112,27);
    }else{
        const int x=12+int(age%2000)*104/2000,y=4+int(std::sin(float(age)*pi/700)*2);
        out.line(x-2,y,x+2,y);out.line(x,y-2,x,y+2);out.pixel(x-1,y-1);out.pixel(x+1,y+1);
    }
}
}
