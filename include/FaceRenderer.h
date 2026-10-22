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
};

class FaceRenderer {
public:
    explicit FaceRenderer(uint32_t seed = 1) : rng_(seed ? seed : 1) {}
    void draw(Canvas& out, Tick now, Mood mood, uint8_t humor);
    unsigned variant() const { return variant_; }
private:
    struct Pose {
        float sharp = 0, lid = 0, irisX = 5, irisY = 9;
        float brow = 0, asym = 0, surprise = 0;
    } pose_;
    uint32_t random();
    Pose target(Mood mood) const;
    void eye(Canvas& out, int cx, int d, float gaze, float blink, float asym);
    uint32_t rng_;
    Tick lastFrame_ = 0, blinkAt_ = 0, blinkWait_ = 2500;
    Tick gazeAt_ = 0, variantAt_ = 0, moodAt_ = 0;
    unsigned variant_ = 0, previousVariant_ = 0;
    float gaze_ = 0, targetGaze_ = 0;
    Mood mood_ = Mood::Calm;
};
}
