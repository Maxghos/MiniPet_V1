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
uint32_t FaceRenderer::random() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
    return rng_;
}
FaceRenderer::Pose FaceRenderer::target(Mood m) const {
    Pose p;
    switch (m) {
    case Mood::Calm:
        p.lid = variant_ == 1 ? 4 : variant_ == 2 ? -1 : 0;
        p.irisX = variant_ == 3 ? 6 : 5;
        p.irisY = variant_ == 1 ? 7 : 9;
        p.brow = variant_ == 2 ? 1 : 0;
        break;
    case Mood::Attentive: p.sharp = 1; p.irisX = 2; p.irisY = 3; break;
    case Mood::Annoyed: p.sharp = 1; p.lid = 2; p.irisX = 2; p.irisY = 3; p.brow = -1; break;
    case Mood::Curious: p.asym = 3; p.brow = 1; p.irisX = 6; p.irisY = 8; break;
    case Mood::Happy: p.lid = -1; p.irisX = variant_ % 2 ? 7 : 6; p.irisY = 10; p.brow = 1; break;
    case Mood::Shocked: p.lid = -1; p.irisX = 2; p.irisY = 3; p.surprise = 1; p.brow = 1; break;
    case Mood::Tired: p.lid = 12; p.irisX = 6; p.irisY = 8; p.brow = 1; break;
    case Mood::Sleeping: p.lid = 23; p.irisX = 5; p.irisY = 8; break;
    }
    return p;
}
void FaceRenderer::eye(Canvas& c, int cx, int d, float gaze, float blink, float asym) {
    const int cy = 16;
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
    static constexpr float roundTop[] = {1,-5,-10,-12,-10,-5,0};
    static constexpr float sharpTop[] = {-7,-6,-5,-3,-1,0,0};
    float top[7];
    for (int i = 0; i < 7; ++i) {
        float open = roundTop[i] + pose_.sharp * (sharpTop[i] - roundTop[i]);
        if (pose_.lid > 0) {
            open += (2.f-open)*std::min(1.f,pose_.lid/12.f);
            open += std::max(0.f,pose_.lid-12.f);
        } else open += pose_.lid;
        open += asym;
        const float closed = 13.f - 8.f * std::pow(float(xs[i]) / 18, 2);
        top[i] = open + blink * (closed - open);
    }
    const int px = cx + int(std::round(gaze)) + d * int(3 * pose_.sharp);
    const int py = cy + int(2 * pose_.sharp);
    const float rx = std::max(1.f, pose_.irisX), ry = std::max(1.f, pose_.irisY);
    for (int x = -18; x <= 18; ++x) {
        int seg = 0;
        while (seg < 5 && x > xs[seg + 1]) ++seg;
        float upper = top[seg] + (top[seg+1] - top[seg]) * float(x - xs[seg]) / (xs[seg+1] - xs[seg]);
        float lower = 12 - 8 * std::pow(float(x) / 18, 2);
        for (int y = -14; y <= 14; ++y) {
            const float ex = (cx + d*x - px) / rx, ey = (cy+y-py) / ry;
            if (y > upper + 1 && y <= lower && ex*ex + ey*ey <= 1) {
                bool on = true;
                if (pose_.sharp < .5f && pose_.surprise < .5f && y > 3 && ((x+y)&1) == 0) on = false;
                if (std::abs(cx+d*x - (px-1)) <= 1 && y >= -5 && y <= -2 && rx > 3) on = false;
                c.pixel(cx + d*x, cy+y, on);
            }
        }
    }
    // Upper eyelid is a continuous interpolated contour; all variants share anchors.
    for (int i = 0; i < 6; ++i) {
        c.line(cx+d*xs[i], cy+int(top[i]), cx+d*xs[i+1], cy+int(top[i+1]));
        c.line(cx+d*xs[i], cy+int(top[i])+1, cx+d*xs[i+1], cy+int(top[i+1])+1);
    }
    if (pose_.sharp > .4f) {
        c.line(cx-d*18, cy-7+int(pose_.lid), cx-d*4, cy+9);
        c.line(cx-d*4, cy+9, cx+d*18, cy+int(pose_.lid));
        c.line(cx+d*18, cy, cx+d*23, cy+3);
    } else if (pose_.lid < 19) {
        if (pose_.surprise > .5f) {
            c.line(cx-18,cy+1,cx-11,cy+10); c.line(cx-11,cy+10,cx,cy+13);
            c.line(cx,cy+13,cx+11,cy+10); c.line(cx+11,cy+10,cx+18,cy+1);
        } else {
            c.line(cx-7, cy+12, cx, cy+13);
            c.line(cx, cy+13, cx+7, cy+12);
        }
    }
    if (pose_.brow > .5f) {
        int by = std::max(0, cy-15+int(asym));
        c.line(cx-11, by+2, cx, by);
        c.line(cx, by, cx+10, by+2);
    }
}
void FaceRenderer::draw(Canvas& c, Tick now, Mood m, uint8_t humor) {
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
    const Pose t = target(m);
    auto approach = [ease](float& v, float target) { v += (target-v)*ease; };
    approach(pose_.sharp,t.sharp); approach(pose_.lid,t.lid);
    approach(pose_.irisX,t.irisX); approach(pose_.irisY,t.irisY);
    approach(pose_.brow,t.brow); approach(pose_.asym,t.asym); approach(pose_.surprise,t.surprise);
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
    if (m == Mood::Sleeping) blink = 0;
    // Original example: alert sign, ordered-dither fade out, attentive eyes fade in.
    if (m == Mood::Attentive && elapsed(now,moodAt_) < 1200) {
        c.line(64,1,46,29); c.line(46,29,82,29); c.line(82,29,64,1);
        c.rect(63,10,3,10); c.rect(63,23,3,3);
        Tick a = elapsed(now,moodAt_);
        if (a > 650) c.fade(16 - std::min<Tick>(16,(a-650)*16/550));
        return;
    }
    float breath = m == Mood::Sleeping ? std::sin(now/900.f)*.8f : 0;
    eye(c,36,1,gaze_,blink,pose_.asym+breath);
    eye(c,92,-1,gaze_,blink,-pose_.asym+breath);
    if (m == Mood::Attentive && elapsed(now,moodAt_) < 1650)
        c.fade(std::min<Tick>(16,(elapsed(now,moodAt_)-1200)*16/450));
    if (m == Mood::Sleeping && (now/3500)%3 == 1) {
        c.clear();
        // Three pixel Zs; positions rise gently without needing a font.
        for (int i = 0; i < 3; ++i) {
            int x = 42+i*15, y = 18-i*5;
            c.line(x,y,x+8,y); c.line(x+8,y,x,y+7); c.line(x,y+7,x+8,y+7);
        }
    }
}
}
