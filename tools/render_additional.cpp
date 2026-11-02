#include "LittleActivities.h"
#include <fstream>
#include <string>
int main(){
    for(unsigned scene=0;scene<unsigned(pet::LittleActivity::Count);++scene){
        pet::FaceRenderer faces(123);pet::Canvas frame;
        for(unsigned step=0;step<=18;++step){
            const unsigned age=step*100;
            pet::drawLittleActivity(faces,frame,pet::LittleActivity(scene),10000+age,age,50);
            if(step!=6&&step!=11&&step!=18)continue;
            std::ofstream out("build/activity-"+std::to_string(scene)+"-"+std::to_string(step)+".pgm",std::ios::binary);
            out<<"P5\n128 32\n255\n";
            for(int y=0;y<32;++y)for(int x=0;x<128;++x)out.put(static_cast<const pet::Canvas&>(frame).pixel(x,y)?char(255):char(0));
        }
    }
}
