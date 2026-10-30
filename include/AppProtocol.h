#pragma once
#include "PetCore.h"
#include <cstring>
#include <string>

namespace pet::app {
constexpr size_t frameBytes=512, headerBytes=24, maxFrames=450, maxAssetBytes=headerBytes+frameBytes*maxFrames;
constexpr size_t maxTextBytes=4096;
constexpr Tick leaseMs=15000;
constexpr const char* serviceUuid="899a1000-48a0-4d42-8bd6-602c08a43c7e";
constexpr const char* commandUuid="899a1001-48a0-4d42-8bd6-602c08a43c7e";
constexpr const char* responseUuid="899a1002-48a0-4d42-8bd6-602c08a43c7e";
uint32_t crc32(const uint8_t* data,size_t length);
bool validAssetId(const std::string& id);
bool sameToken(const std::string& a,const std::string& b);
struct MediaInfo { uint8_t fps=1; uint16_t frames=1; uint32_t bytes=512; uint32_t crc=0; };
bool parseMediaHeader(const uint8_t* data,size_t size,MediaInfo& info);
bool validateMedia(const uint8_t* data,size_t size,MediaInfo& info);
enum class Content : uint8_t { None, Text, Media };
class Playback {
public:
    bool text(const std::string& value,Tick now,int32_t duration,bool scroll,unsigned speed);
    void media(MediaInfo info,Tick now,int32_t duration,bool loop);
    void heartbeat(Tick now) { if(type_!=Content::None && elapsed(now,leaseAt_)>=leaseMs)stop(); leaseAt_=now; }
    void expire(Tick now) { if(type_!=Content::None && !active(now))stop(); }
    void stop() { type_=Content::None; ++revision_; }
    bool active(Tick now) const;
    size_t frame(Tick now) const;
    Tick age(Tick now) const { return elapsed(now,started_); }
    Content type(Tick now) const { return active(now) ? type_ : Content::None; }
    const std::string& value() const { return text_; }
    unsigned speed() const { return speed_; }
    bool scrolling() const { return scroll_; }
    uint32_t revision() const { return revision_; }
private:
    Content type_=Content::None;
    Tick started_=0,leaseAt_=0,duration_=0;
    MediaInfo media_;
    std::string text_;
    uint32_t revision_=0;
    unsigned speed_=25;
    bool scroll_=false,loop_=false;
};
struct RemoteView {
    Content type=Content::None;
    std::string text;
    uint8_t frame[frameBytes]{};
    Tick age=0;
    uint32_t revision=UINT32_MAX;
    unsigned speed=25;
    bool scrolling=false;
};
}
