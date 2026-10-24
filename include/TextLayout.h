#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace pet::text {
struct Page {
    std::vector<std::string> lines; // CP437: one byte is one 6x8 cell.
    int top() const { return (32-int(lines.size())*8)/2; }
    static int left(const std::string& line) { return (128-int(line.size())*6)/2; }
    uint32_t duration() const;
};
struct Layout {
    std::vector<Page> pages;
    uint32_t duration() const;
    const Page& at(uint32_t elapsed) const;
};
std::string encode(const char* utf8);
Layout wrap(const char* utf8);
}
