#pragma once
#include "PetCore.h"
#include <array>

namespace pet {
// SSD1306 page layout: the exact same 512 bytes are used on device and in previews.
class Canvas {
public:
    std::array<uint8_t, 512> bytes{};
    void clear() { bytes.fill(0); }
    void pixel(int x, int y, bool on = true);
    bool pixel(int x, int y) const;
    void line(int x0, int y0, int x1, int y1, bool on = true);
    void rect(int x, int y, int w, int h, bool on = true);
    void ellipse(int cx, int cy, int rx, int ry, bool on = true);
    void fade(unsigned level);
    void translate(int dx, int dy);
};

enum class FaceStyle : uint8_t {
    CalmClassic, CalmDroopy, CalmLashes, CalmWide, HappyRound, HappySparkle,
    HappyArcs, Curious, Attentive, ShockedRound, ShockedBox, ShockedAnime,
    Tired, Sleeping, SarcasticBlue, DeadpanPurple, SlyOrange, ClosedAngry, Pout, Count
};
const char* faceStyleLabel(FaceStyle style);

class FaceRenderer {
public:
    explicit FaceRenderer(uint32_t seed = 1) : rng_(seed ? seed : 1) {}
    void draw(Canvas& out, Tick now, Mood mood, uint8_t humor,
              FaceStyle forced = FaceStyle::Count, float lookY = 0);
    FaceStyle style() const { return style_; }
    unsigned variant() const { return variant_; }
private:
    struct Pose {
        float sharp = 0, lid = 0, irisX = 5, irisY = 9;
        float brow = 0, asym = 0, surprise = 0;
        float upper[7] = {1,-5,-10,-12,-10,-5,0};
        float width = 1, lowerFull = 0, lashes = 0, sparkle = 0, box = 0;
        float happyClosed = 0, angryClosed = 0, browTilt = 0, irisOffsetY = 0;
    } pose_;
    uint32_t random();
    Pose target(FaceStyle style) const;
    FaceStyle choose(Mood mood) const;
    void eye(Canvas& out, int cx, int d, float gaze, float blink, float asym, float lookY);
    uint32_t rng_;
    Tick lastFrame_ = 0, blinkAt_ = 0, blinkWait_ = 2500;
    Tick gazeAt_ = 0, variantAt_ = 0, moodAt_ = 0;
    unsigned variant_ = 0, previousVariant_ = 0;
    float gaze_ = 0, targetGaze_ = 0;
    Mood mood_ = Mood::Calm;
    FaceStyle style_ = FaceStyle::CalmClassic;
};
}
