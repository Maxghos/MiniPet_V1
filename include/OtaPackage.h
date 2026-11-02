#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace pet::ota {
constexpr size_t headerBytes=128;
struct Package {
    uint32_t imageBytes=0;
    std::string target,version;
    std::array<uint8_t,32> digest{};
};
bool parsePackage(const uint8_t* bytes,size_t size,size_t total,const std::string& target,size_t limit,Package& output);
bool validImagePrefix(const uint8_t* bytes,size_t size);
class Marker {
public:
    void reset(const std::string& value){value_=value;matched_=0;found_=false;}
    void add(const uint8_t* bytes,size_t size);
    bool found() const{return found_;}
private:
    std::string value_;size_t matched_=0;bool found_=false;
};
}
