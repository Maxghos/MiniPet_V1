#include "AppProtocol.h"
#include "TextLayout.h"

namespace pet::app {
static uint16_t u16(const uint8_t* p){ return uint16_t(p[0]) | uint16_t(p[1])<<8; }
static uint32_t u32(const uint8_t* p){ return uint32_t(u16(p)) | uint32_t(u16(p+2))<<16; }
uint32_t crc32(const uint8_t* data,size_t length) {
    uint32_t crc=0xffffffff;
    for (size_t i=0;i<length;++i) {
        crc^=data[i];
        for (unsigned bit=0;bit<8;++bit) crc=(crc>>1)^(0xedb88320u & (0u-(crc&1)));
    }
    return ~crc;
}
bool validAssetId(const std::string& id) {
    if (id.empty() || id.size()>24) return false;
    for (char c:id) if (!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-')) return false;
    return true;
}
bool sameToken(const std::string& a,const std::string& b) {
    if (a.size()!=64 || b.size()!=64) return false;
    uint8_t mismatch=0;
    for (size_t i=0;i<64;++i) mismatch|=uint8_t(a[i]^b[i]);
    return mismatch==0;
}
bool parseMediaHeader(const uint8_t* data,size_t size,MediaInfo& info) {
    if (!data || size<headerBytes || std::memcmp(data,"MPET",4) || data[4]!=1 || data[5]<1 || data[5]>15 ||
        u16(data+6)!=128 || u16(data+8)!=32 || u32(data+20)!=0) return false;
    MediaInfo result; result.fps=data[5]; result.frames=u16(data+10); result.bytes=u32(data+12); result.crc=u32(data+16);
    if (result.frames<1 || result.frames>maxFrames || result.bytes!=result.frames*frameBytes ||
        size!=result.bytes+headerBytes) return false;
    info=result; return true;
}
bool validateMedia(const uint8_t* data,size_t size,MediaInfo& info) {
    return parseMediaHeader(data,size,info) && crc32(data+headerBytes,info.bytes)==info.crc;
}
bool Playback::text(const std::string& value,Tick now,int32_t duration,bool scroll,unsigned speed) {
    if (value.empty() || value.size()>maxTextBytes || duration < -1 || duration>3600000 || speed<10 || speed>80) return false;
    text_=value; started_=leaseAt_=now; speed_=speed; scroll_=scroll;
    duration_=duration==-1 ? (scroll ? Tick((pet::text::encode(value.c_str()).size()*6+128)*1000/speed) : pet::text::wrap(value.c_str()).duration()) : Tick(duration);
    type_=Content::Text; ++revision_; return true;
}
void Playback::media(MediaInfo info,Tick now,int32_t duration,bool loop) {
    media_=info; started_=leaseAt_=now; duration_=duration<0 ? (uint32_t(info.frames)*1000+info.fps-1)/info.fps : Tick(duration);
    loop_=loop || info.frames==1; type_=Content::Media; ++revision_;
}
bool Playback::active(Tick now) const {
    if (type_==Content::None || elapsed(now,leaseAt_)>=leaseMs || (duration_ && elapsed(now,started_)>=duration_)) return false;
    return true;
}
size_t Playback::frame(Tick now) const {
    if(!media_.frames)return 0;
    const uint64_t index=uint64_t(elapsed(now,started_))*media_.fps/1000;
    return loop_ ? index%media_.frames : std::min<uint64_t>(index,media_.frames-1);
}
}
