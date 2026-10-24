#include "FaceRenderer.h"
#include <cstdlib>

namespace pet {
void Canvas::pixel(int x, int y, bool on) {
    if (x < 0 || x >= 128 || y < 0 || y >= 32) return;
    auto& b = bytes[x + (y / 8) * 128];
    if (on) b |= 1 << (y % 8); else b &= ~(1 << (y % 8));
}
bool Canvas::pixel(int x, int y) const {
    return x >= 0 && x < 128 && y >= 0 && y < 32 && (bytes[x + (y / 8) * 128] & (1 << (y % 8)));
}
void Canvas::line(int x0, int y0, int x1, int y1, bool on) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int e = dx + dy;
    for (;;) {
        pixel(x0, y0, on);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * e;
        if (e2 >= dy) { e += dy; x0 += sx; }
        if (e2 <= dx) { e += dx; y0 += sy; }
    }
}
void Canvas::rect(int x, int y, int w, int h, bool on) {
    for (int yy = y; yy < y + h; ++yy)
        for (int xx = x; xx < x + w; ++xx) pixel(xx, yy, on);
}
void Canvas::ellipse(int cx, int cy, int rx, int ry, bool on) {
    if (rx < 1 || ry < 1) return;
    for (int y = -ry; y <= ry; ++y) {
        int dx = int(rx * std::sqrt(std::max(0.f, 1.f - float(y*y) / (ry*ry))));
        line(cx - dx, cy + y, cx + dx, cy + y, on);
    }
}
void Canvas::fade(unsigned level) {
    static constexpr uint8_t bayer[4][4] = {{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
    if (level >= 16) return;
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 128; ++x)
            if (bayer[y & 3][x & 3] >= level) pixel(x, y, false);
}
void Canvas::translate(int dx, int dy) {
    const Canvas source=*this; clear();
    for (int y=0;y<32;++y) for (int x=0;x<128;++x)
        if (source.pixel(x,y)) pixel(x+dx,y+dy);
}
const char* faceStyleLabel(FaceStyle style) {
    static const char* labels[]={"Tranquilo clasico","Tranquilo caido","Tranquilo pestanas","Tranquilo amplio",
        "Contento redondo","Contento brillante","Contento arcos","Curioso","Atento afilado",
        "Sorpresa redonda","Sorpresa rectangular","Sorpresa anime","Cansado","Dormido",
        "Ironico (ref. azul)","Entrecerrado (ref. violeta)","Picaro (ref. naranja)","Enfado cerrado","Ofendido"};
    return unsigned(style)<unsigned(FaceStyle::Count) ? labels[unsigned(style)] : "Automatico";
}
uint32_t FaceRenderer::random() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
    return rng_;
}
FaceStyle FaceRenderer::choose(Mood mood) const {
    switch (mood) {
    case Mood::Calm: return FaceStyle(unsigned(FaceStyle::CalmClassic)+variant_%4);
    case Mood::Happy: return variant_%3==0 ? FaceStyle::HappyRound : variant_%3==1 ? FaceStyle::HappySparkle : FaceStyle::HappyArcs;
    case Mood::Excited: return variant_%2 ? FaceStyle::HappySparkle : FaceStyle::HappyArcs;
    case Mood::Shocked: return FaceStyle(unsigned(FaceStyle::ShockedRound)+variant_%3);
    case Mood::Annoyed: return variant_%3==0 ? FaceStyle::Attentive : variant_%3==1 ? FaceStyle::DeadpanPurple : FaceStyle::ClosedAngry;
    case Mood::Smug: return variant_%2 ? FaceStyle::SarcasticBlue : FaceStyle::SlyOrange;
    case Mood::Pout: return variant_%2 ? FaceStyle::DeadpanPurple : FaceStyle::Pout;
    case Mood::Curious: return FaceStyle::Curious;
    case Mood::Attentive: return FaceStyle::Attentive;
    case Mood::Tired: return FaceStyle::Tired;
    case Mood::Sleeping: return FaceStyle::Sleeping;
    default: return FaceStyle::CalmClassic;
    }
}
FaceRenderer::Pose FaceRenderer::target(FaceStyle style) const {
    Pose p;
    auto upper=[&p](std::initializer_list<float> points){ std::copy(points.begin(),points.end(),p.upper); };
    switch (style) {
    case FaceStyle::CalmClassic: break;
    case FaceStyle::CalmDroopy: upper({0,-4,-6,-3,-3,-1,1}); p.irisX=6; p.irisY=7; p.lowerFull=1; p.brow=1; break;
    case FaceStyle::CalmLashes: upper({2,-4,-10,-13,-12,-7,-1}); p.lashes=1; p.irisY=10; p.brow=1; p.lowerFull=.5; break;
    case FaceStyle::CalmWide: upper({0,-6,-11,-13,-11,-6,0}); p.width=1.1; p.irisX=6; p.sparkle=1; p.lowerFull=.7; break;
    case FaceStyle::HappyRound: upper({-1,-6,-12,-13,-12,-6,-1}); p.irisX=7; p.irisY=10; p.lowerFull=1; p.brow=1; break;
    case FaceStyle::HappySparkle: upper({0,-3,-9,-13,-12,-7,-1}); p.irisX=6; p.irisY=10; p.sparkle=2; p.lashes=1; p.lowerFull=1; break;
    case FaceStyle::HappyArcs: p.happyClosed=1; break;
    case FaceStyle::Curious: p.asym=3; p.brow=1; p.irisX=6; p.irisY=8; break;
    case FaceStyle::Attentive: upper({-7,-6,-5,-3,-1,0,0}); p.sharp=1; p.irisX=2; p.irisY=3; break;
    case FaceStyle::ShockedRound: p.lid=-1; p.irisX=2; p.irisY=3; p.surprise=1; p.brow=1; p.lowerFull=1; break;
    case FaceStyle::ShockedBox: upper({-10,-12,-13,-13,-13,-12,-10}); p.width=.8; p.box=1; p.irisX=1.5; p.irisY=2.5; p.lowerFull=1; p.surprise=1; break;
    case FaceStyle::ShockedAnime: upper({-2,-5,-10,-11,-9,-3,1}); p.irisX=1.5; p.irisY=5; p.lowerFull=1; p.surprise=1; p.lashes=1; p.brow=1; break;
    case FaceStyle::Tired: p.lid=12; p.irisX=6; p.irisY=8; p.brow=1; p.lowerFull=1; break;
    case FaceStyle::Sleeping: p.lid=23; break;
    case FaceStyle::SarcasticBlue: upper({-1,-1,-1,0,0,0,0}); p.irisX=7; p.irisY=8; p.irisOffsetY=3; p.asym=1.5; p.lowerFull=1; p.lashes=1; p.brow=1; p.browTilt=3; p.sparkle=1; break;
    case FaceStyle::DeadpanPurple: upper({1,0,-1,-1,-1,-1,-1}); p.irisX=7; p.irisY=6; p.irisOffsetY=3; p.lowerFull=1; p.lashes=2; p.brow=1; p.browTilt=-2; break;
    case FaceStyle::SlyOrange: upper({-8,-7,-5,-2,0,1,1}); p.sharp=.7; p.irisX=4; p.irisY=5; p.width=1.05; p.brow=1; p.browTilt=-1; break;
    case FaceStyle::ClosedAngry: p.angryClosed=1; p.brow=1; p.browTilt=-3; break;
    case FaceStyle::Pout: upper({1,-1,-3,-4,-3,-1,1}); p.irisX=5; p.irisY=6; p.lowerFull=1; p.brow=1; p.browTilt=2; p.asym=-1.5; break;
    default: break;
    }
    return p;
}
void FaceRenderer::eye(Canvas& c, int cx, int d, float gaze, float blink, float asym, float lookY) {
    const int cy = 16;
    if (pose_.happyClosed>.6f || pose_.angryClosed>.6f) {
        int lastX=cx-18, lastY=cy+2;
        for (int x=-17;x<=18;++x) {
            float arch=1-std::pow(float(x)/18,2);
            int y=cy+2+int((pose_.happyClosed>.6f ? -9+blink*6 : 4-blink*2)*arch+asym*.3f);
            c.line(lastX,lastY,cx+x,y); c.line(lastX,lastY+1,cx+x,y+1);
            lastX=cx+x; lastY=y;
        }
        if (pose_.angryClosed>.6f) {
            c.line(cx-d*17,3,cx+d*12,10); c.line(cx-d*17,4,cx+d*12,11);
            c.line(cx-d*17,cy+2,cx-d*22,cy+4);
        }
        return;
    }
    if (pose_.lid > 18) {
        int px = cx-18, py = cy+2+int(asym);
        for (int x = -17; x <= 18; ++x) {
            int y = cy+2+int(5*(1-std::pow(float(x)/18,2))+asym);
            c.line(px,py,cx+x,y); c.line(px,py+1,cx+x,y+1);
            px = cx+x; py = y;
        }
        c.line(cx-d*18,cy+2,cx-d*22,cy+5);
        return;
    }
    static constexpr int xs[] = {-18,-15,-9,0,9,15,18};
    float top[7];
    for (int i = 0; i < 7; ++i) {
        float open = pose_.upper[i];
        if (pose_.lid > 0) {
            open += (2.f-open)*std::min(1.f,pose_.lid/12.f);
            open += std::max(0.f,pose_.lid-12.f);
        } else open += pose_.lid;
        open += asym;
        const float closed = 13.f - 8.f * std::pow(float(xs[i]) / 18, 2);
        top[i] = open + blink * (closed - open);
    }
    const int px = cx + int(std::round(gaze)) + d * int(3 * pose_.sharp);
    const int py = cy + int(2 * pose_.sharp+pose_.irisOffsetY+lookY);
    const float rx = std::max(1.f, pose_.irisX), ry = std::max(1.f, pose_.irisY);
    for (int x = -18; x <= 18; ++x) {
        int seg = 0;
        while (seg < 5 && x > xs[seg + 1]) ++seg;
        float upper = top[seg] + (top[seg+1] - top[seg]) * float(x - xs[seg]) / (xs[seg+1] - xs[seg]);
        float normalLower=12-8*std::pow(float(x)/18,2);
        float lower=normalLower+pose_.box*(12-2*std::pow(float(x)/18,8)-normalLower);
        int screenX=cx+int(std::round(d*x*pose_.width));
        for (int y = -14; y <= 14; ++y) {
            const float ex = (screenX - px) / rx, ey = (cy+y-py) / ry;
            if (y > upper + 1 && y <= lower && ex*ex + ey*ey <= 1) {
                bool on = true;
                if (pose_.sharp < .5f && pose_.surprise < .5f && y > 3 && ((x+y)&1) == 0) on = false;
                if (std::abs(cx+d*x - (px-1)) <= 1 && y >= -5 && y <= -2 && rx > 3) on = false;
                if (pose_.sparkle>.5f && rx>3 && y>=3 && y<=5 && std::abs(screenX-(px+2))<=1) on=false;
                if (pose_.sparkle>1.5f && rx>3 && y>=-1 && y<=1 && std::abs(screenX-(px+3))<=1) on=false;
                c.pixel(screenX,cy+y,on);
            }
        }
    }
    // Upper eyelid is a continuous interpolated contour; all variants share anchors.
    for (int i = 0; i < 6; ++i) {
        c.line(cx+int(d*xs[i]*pose_.width), cy+int(top[i]), cx+int(d*xs[i+1]*pose_.width), cy+int(top[i+1]));
        c.line(cx+int(d*xs[i]*pose_.width), cy+int(top[i])+1, cx+int(d*xs[i+1]*pose_.width), cy+int(top[i+1])+1);
    }
    if (pose_.sharp > .4f) {
        c.line(cx-d*18, cy-7+int(pose_.lid), cx-d*4, cy+9);
        c.line(cx-d*4, cy+9, cx+d*18, cy+int(pose_.lid));
        c.line(cx+d*18, cy, cx+d*23, cy+3);
    } else if (pose_.lid < 19) {
        if (pose_.lowerFull > .4f) {
            int lastX=cx-int(18*pose_.width),lastY=cy+int(4+6*pose_.box);
            for (int x=-17;x<=18;++x) {
                float a=float(x)/18;
                int y=cy+int(12-8*a*a+pose_.box*(8*a*a-2*std::pow(a,8)));
                int screenX=cx+int(x*pose_.width);
                c.line(lastX,lastY,screenX,y); lastX=screenX; lastY=y;
            }
            if (pose_.box>.5f) {
                c.line(cx-int(18*pose_.width),cy+int(top[0]),cx-int(18*pose_.width),cy+10);
                c.line(cx+int(18*pose_.width),cy+int(top[6]),cx+int(18*pose_.width),cy+10);
            }
        } else {
            c.line(cx-7, cy+12, cx, cy+13);
            c.line(cx, cy+13, cx+7, cy+12);
        }
    }
    if (pose_.brow > .5f) {
        int by = std::max(0, cy-15+int(asym));
        c.line(cx-d*14,by+2-int(pose_.browTilt),cx,by+2);
        c.line(cx,by+2,cx+d*12,by+2+int(pose_.browTilt));
    }
    if (pose_.lashes>.5f) {
        int x=cx-int(d*18*pose_.width), y=cy+int(top[0]);
        c.line(x,y,x-d*5,y-3); c.line(x,y+1,x-d*5,y);
        if (pose_.lashes>1.5f) c.line(x,y+1,x-d*4,y+4);
    }
}
void FaceRenderer::draw(Canvas& c, Tick now, Mood m, uint8_t humor, FaceStyle forced, float lookY) {
    c.clear();
    if (m != mood_) { mood_ = m; moodAt_ = now; }
    if (elapsed(now, variantAt_) > 12000u + (100-humor)*150u) {
        previousVariant_ = variant_;
        do { variant_ = random() % 4; } while (variant_ == previousVariant_);
        variantAt_ = now;
    }
    const float dt = float(std::min<Tick>(elapsed(now, lastFrame_), 100));
    lastFrame_ = now;
    const float ease = 1.f - std::exp(-dt / 130.f);
    style_=forced==FaceStyle::Count ? choose(m) : forced;
    const Pose t = target(style_);
    auto approach = [ease](float& v, float target) { v += (target-v)*ease; };
    approach(pose_.sharp,t.sharp); approach(pose_.lid,t.lid);
    approach(pose_.irisX,t.irisX); approach(pose_.irisY,t.irisY);
    approach(pose_.brow,t.brow); approach(pose_.asym,t.asym); approach(pose_.surprise,t.surprise);
    for (unsigned i=0;i<7;++i) approach(pose_.upper[i],t.upper[i]);
    approach(pose_.width,t.width); approach(pose_.lowerFull,t.lowerFull);
    approach(pose_.lashes,t.lashes); approach(pose_.sparkle,t.sparkle); approach(pose_.box,t.box);
    approach(pose_.happyClosed,t.happyClosed); approach(pose_.angryClosed,t.angryClosed);
    approach(pose_.browTilt,t.browTilt); approach(pose_.irisOffsetY,t.irisOffsetY);
    if (elapsed(now, gazeAt_) > 1600u + (100-humor)*20u) {
        targetGaze_ = int(random()%7)-3;
        gazeAt_ = now;
    }
    approach(gaze_, targetGaze_);
    float blink = 0;
    Tick age = elapsed(now, blinkAt_);
    const Tick duration = m == Mood::Tired ? 600 : 200;
    if (age >= blinkWait_) {
        Tick phase = age-blinkWait_;
        blink = phase < duration/2 ? float(phase)/(duration/2) : 1.f-float(phase-duration/2)/(duration/2);
        blink = std::clamp(blink, 0.f, 1.f);
        if (phase >= duration) { blinkAt_ = now; blinkWait_ = 2200+random()%3300; }
    }
    if (m == Mood::Sleeping || style_==FaceStyle::Sleeping) blink = 0;
    // Original example: alert sign, ordered-dither fade out, attentive eyes fade in.
    if (forced==FaceStyle::Count && m == Mood::Attentive && elapsed(now,moodAt_) < 1200) {
        c.line(64,1,46,29); c.line(46,29,82,29); c.line(82,29,64,1);
        c.rect(63,10,3,10); c.rect(63,23,3,3);
        Tick a = elapsed(now,moodAt_);
        if (a > 650) c.fade(16 - std::min<Tick>(16,(a-650)*16/550));
        return;
    }
    float breath = m == Mood::Sleeping ? std::sin(now/900.f)*.8f : 0;
    eye(c,36,1,gaze_,blink,pose_.asym+breath,lookY);
    eye(c,92,-1,gaze_,blink,-pose_.asym+breath,lookY);
    if (pose_.angryClosed>.6f && humor>=60 && (now/400)%3!=0) {
        c.line(116,1,116,5); c.line(116,5,112,5);
        c.line(120,1,120,5); c.line(120,5,124,5);
        c.line(112,9,116,9); c.line(116,9,116,13);
        c.line(124,9,120,9); c.line(120,9,120,13);
    }
    if (forced==FaceStyle::Count && m == Mood::Attentive && elapsed(now,moodAt_) < 1650)
        c.fade(std::min<Tick>(16,(elapsed(now,moodAt_)-1200)*16/450));
    if ((m == Mood::Sleeping || style_==FaceStyle::Sleeping) && (now/3500)%3 == 1) {
        c.clear();
        // Three pixel Zs; positions rise gently without needing a font.
        for (int i = 0; i < 3; ++i) {
            int x = 42+i*15, y = 18-i*5;
            c.line(x,y,x+8,y); c.line(x+8,y,x,y+7); c.line(x,y+7,x+8,y+7);
        }
    }
}
}
