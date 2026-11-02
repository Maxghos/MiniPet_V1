#include "MiniGames.h"
#include <cstdio>

namespace pet::games {
namespace {
// Original 3x5 lettering keeps the whole game readable in a 128x32 framebuffer.
uint16_t glyph(char c){
    switch(c){
    case '0':return 0b111101101101111;case '1':return 0b010110010010111;
    case '2':return 0b111001111100111;case '3':return 0b111001111001111;
    case '4':return 0b101101111001001;case '5':return 0b111100111001111;
    case '6':return 0b111100111101111;case '7':return 0b111001010010010;
    case '8':return 0b111101111101111;case '9':return 0b111101111001111;
    case 'A':return 0b010101111101101;case 'B':return 0b110101110101110;
    case 'C':return 0b111100100100111;case 'D':return 0b110101101101110;
    case 'E':return 0b111100110100111;case 'F':return 0b111100110100100;
    case 'G':return 0b111100101101111;case 'H':return 0b101101111101101;
    case 'I':return 0b111010010010111;case 'J':return 0b001001001101111;
    case 'K':return 0b101101110101101;case 'L':return 0b100100100100111;
    case 'M':return 0b101111111101101;case 'N':return 0b101111111111101;
    case 'O':return 0b111101101101111;case 'P':return 0b111101111100100;
    case 'Q':return 0b111101101111001;case 'R':return 0b110101110101101;
    case 'S':return 0b111100111001111;case 'T':return 0b111010010010010;
    case 'U':return 0b101101101101111;case 'V':return 0b101101101101010;
    case 'W':return 0b101101111111101;case 'X':return 0b101101010101101;
    case 'Y':return 0b101101010010010;case 'Z':return 0b111001010100111;
    case ':':return 0b000010000010000;case '-':return 0b000000111000000;
    case '+':return 0b000010111010000;default:return 0;
    }
}
void text(Canvas& out,int x,int y,const char* str,int scale=1){
    for(;*str;++str,x+=4*scale){const uint16_t bits=glyph(*str);
        for(int row=0;row<5;++row)for(int col=0;col<3;++col)
            if(bits&(1u<<(14-row*3-col)))out.rect(x+col*scale,y+row*scale,scale,scale);
    }
}
void centered(Canvas& out,int y,const char* str,int scale=1){text(out,(128-int(std::char_traits<char>::length(str))*4*scale+scale)/2,y,str,scale);}
int distanceX(float cm){return std::clamp(3+int((cm-15.f)*121.f/65.f),3,124);}
int playerY(float cm){return std::clamp(24-int((cm-15.f)*13.f/65.f),11,24);}
}
const char* kindName(Kind kind){return kind==Kind::Dodge?"dodge":"target";}
const char* stateName(State state){switch(state){case State::Idle:return "idle";case State::Countdown:return "countdown";case State::Playing:return "playing";default:return "result";}}
bool parseKind(const std::string& name,Kind& kind){
    if(name=="target"){kind=Kind::Target;return true;}if(name=="dodge"){kind=Kind::Dodge;return true;}return false;
}
unsigned Game::random(){rng_=rng_*1664525u+1013904223u;return rng_;}
void Game::nextTarget(){
    const float old=target_;
    do{target_=25.f+float(random()%10)*5.f;}while(std::fabs(target_-old)<10.f);
    held_=0;
}
void Game::nextWall(){gapY_=12+int(random()%11);wallAge_=0;}
bool Game::begin(Tick now,Kind kind){
    if(active()||kind>=Kind::Count)return false;
    kind_=kind;state_=State::Countdown;stateAt_=lastUpdate_=lastGood_=now;
    played_=held_=wallAge_=0;score_=0;lives_=3;valid_=previousValid_=filtered_=sensorLost_=false;
    hitAt_=now-500;nextTarget();nextWall();return true;
}
void Game::finish(Tick now,bool sensorLost){state_=State::Result;stateAt_=now;sensorLost_=sensorLost;valid_=previousValid_=false;}
void Game::update(Tick now,bool valid,float distance){
    if(!active())return;
    if(state_==State::Result){if(elapsed(now,stateAt_)>=6000)stop();return;}
    const Tick delta=elapsed(now,lastUpdate_);lastUpdate_=now;
    if(delta>200){held_=0;previousValid_=false;}
    const bool usable=valid&&std::isfinite(distance)&&distance>=15&&distance<=80;
    valid_=usable;
    if(usable){
        const bool uninterrupted=previousValid_&&delta<=200;
        if(!filtered_||!uninterrupted)cm_=distance;
        else cm_+=(distance-cm_)*std::min(1.f,float(delta)/150.f);
        filtered_=true;lastGood_=now;
    }else if(elapsed(now,lastGood_)>250){held_=0;filtered_=false;}
    if(elapsed(now,lastGood_)>=5000){finish(now,true);return;}
    if(state_==State::Countdown){
        previousValid_=usable;
        if(elapsed(now,stateAt_)>=3000){state_=State::Playing;stateAt_=now;previousValid_=false;}
        return;
    }
    // A lost echo, reacquisition or long scheduling gap cannot create points,
    // collisions or catch up several seconds of unseen gameplay.
    const Tick step=usable&&previousValid_&&delta<=200?delta:0;previousValid_=usable;
    if(!step)return;
    played_=std::min<Tick>(45000,played_+step);
    if(kind_==Kind::Target){
        if(std::fabs(distance-target_)<=5.f&&std::fabs(cm_-target_)<=5.f){
            held_+=step;
            if(held_>=800){++score_;nextTarget();}
        }else held_=0;
    }else{
        wallAge_+=step;
        const Tick round=2400-std::min(score_,12u)*50u;
        if(wallAge_>=round){
            if(std::abs(playerY(cm_)-gapY_)>3){if(lives_)--lives_;hitAt_=now;}
            else ++score_;
            nextWall();if(!lives_){finish(now,false);return;}
        }
    }
    if(played_>=45000)finish(now,false);
}
Snapshot Game::snapshot(Tick now) const {
    Snapshot out;out.kind=kind_;out.state=state_;out.score=score_;out.lives=lives_;out.heldMs=held_;
    out.cm=filtered_?cm_:0;out.targetCm=kind_==Kind::Target?target_:15.f+float(24-gapY_)*5.f;out.inputValid=valid_;out.sensorLost=sensorLost_;
    out.remainingMs=state_==State::Playing?45000-played_:state_==State::Countdown?3000-std::min<Tick>(3000,elapsed(now,stateAt_)):
        state_==State::Result?6000-std::min<Tick>(6000,elapsed(now,stateAt_)):0;
    return out;
}
void Game::draw(Canvas& out,Tick now) const {
    out.clear();char value[40];const auto s=snapshot(now);
    if(state_==State::Idle){centered(out,4,"JUEGOS CON LA MANO");centered(out,17,"ABRE APP O AJUSTES");return;}
    if(state_==State::Countdown){
        centered(out,0,kind_==Kind::Target?"PUNTERIA: SOSTEN EL PUNTO":"ESQUIVA: ACERCA Y ALEJA");
        snprintf(value,sizeof(value),"%lu",static_cast<unsigned long>((s.remainingMs+999)/1000));centered(out,9,value,2);
        centered(out,25,"MANO ENTRE 15 Y 80 CM");return;
    }
    if(state_==State::Result){
        centered(out,0,sensorLost_?"NO ENCUENTRO TU MANO":"PARTIDA TERMINADA");
        snprintf(value,sizeof(value),"%u PUNTOS",score_);centered(out,10,value,2);
        centered(out,26,sensorLost_?"PRUEBA ENTRE 15 Y 80 CM":"TOQUE: SALIR");return;
    }
    text(out,0,0,kind_==Kind::Target?"PUNTERIA":"ESQUIVA");
    snprintf(value,sizeof(value),"%uP %02luS",score_,static_cast<unsigned long>((45000-played_+999)/1000));text(out,88,0,value);
    if(!valid_){centered(out,11,"MANO ENTRE 15 Y 80 CM");centered(out,21,"ESPERANDO SIN AVANZAR");return;}
    if(kind_==Kind::Target){
        const int target=distanceX(target_),hand=distanceX(cm_);
        out.line(2,18,125,18);out.rect(target-8,10,17,15,false);out.line(target-8,10,target+8,10);out.line(target-8,24,target+8,24);
        out.line(target-8,10,target-8,24);out.line(target+8,10,target+8,24);
        out.line(hand-3,15,hand+3,15);out.line(hand,12,hand,18);out.pixel(hand,19);
        snprintf(value,sizeof(value),"%02uCM",unsigned(target_));text(out,0,27,value);
        out.rect(33,27,int(std::min<Tick>(800,held_))*64/800,4);text(out,105,27,"SALIR");
    }else{
        const int y=playerY(cm_);const Tick round=2400-std::min(score_,12u)*50u;
        const int x=124-int(std::min(wallAge_,round))*114/int(round);
        out.line(x,8,x,gapY_-4);if(gapY_+4<=25)out.line(x,gapY_+4,x,25);
        if(elapsed(now,hitAt_)>=350||(now/80)%2){out.line(7,y-2,12,y);out.line(12,y,7,y+2);out.line(7,y-2,7,y+2);}
        for(unsigned i=0;i<lives_;++i){out.pixel(2+int(i)*5,29);out.line(1+int(i)*5,28,3+int(i)*5,28);}
        text(out,31,27,"CERCA:ABAJO LEJOS:ARRIBA");
    }
}
}
