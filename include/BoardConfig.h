#pragma once

// GPIO numbers, not physical header positions. Verify the clone's silkscreen.
namespace board {
constexpr int sda = 8;
constexpr int scl = 9;
constexpr int pir = 4;
constexpr int trigger = 5;
constexpr int echo = 6;   // MUST use a 5 V -> 3.3 V divider/level shifter.
constexpr int touch = 7;  // TTP223 supplied at 3.3 V, momentary active HIGH.
constexpr int oledAddress = 0x3c;
constexpr unsigned frameMs = 33;
constexpr unsigned distanceMs = 100;
constexpr unsigned pirWarmupMs = 60000;
}
