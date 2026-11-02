#include "OtaPackage.h"
#include <algorithm>
#include <cstring>

namespace pet::ota {
namespace {
uint16_t le16(const uint8_t* p){return uint16_t(p[0])|uint16_t(p[1])<<8;}
uint32_t le32(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
bool field(const uint8_t* p,std::string& output){
    const auto end=std::find(p,p+32,uint8_t(0));if(end==p||end==p+32)return false;
    for(auto at=p;at!=end;++at)if(!((*at>='a'&&*at<='z')||(*at>='A'&&*at<='Z')||(*at>='0'&&*at<='9')||*at=='.'||*at=='-'||*at=='_'))return false;
    if(!std::all_of(end,p+32,[](uint8_t c){return c==0;}))return false;
    output.assign(reinterpret_cast<const char*>(p),size_t(end-p));return true;
}
}
bool parsePackage(const uint8_t* bytes,size_t size,size_t total,const std::string& target,size_t limit,Package& output){
    if(!bytes||size<headerBytes||memcmp(bytes,"MPFW",4)||le16(bytes+4)!=1||le16(bytes+6)!=headerBytes)return false;
    Package next;next.imageBytes=le32(bytes+8);
    if(next.imageBytes<1024||next.imageBytes>limit||total<headerBytes||total-headerBytes!=next.imageBytes||
       !field(bytes+12,next.target)||!field(bytes+44,next.version)||next.target!=target||
       !std::all_of(bytes+108,bytes+128,[](uint8_t c){return c==0;}))return false;
    std::copy(bytes+76,bytes+108,next.digest.begin());output=std::move(next);return true;
}
bool validImagePrefix(const uint8_t* bytes,size_t size){
    return bytes&&size>=24&&bytes[0]==0xe9&&bytes[1]>=1&&bytes[1]<=16&&bytes[12]==9&&bytes[13]==0&&
        (bytes[3]>>4)==4&&bytes[23]==1; // S3, 16 MB flash, appended ESP image SHA-256
}
void Marker::add(const uint8_t* bytes,size_t size){
    if(found_||value_.empty())return;
    for(size_t i=0;i<size;++i){
        const char c=char(bytes[i]);
        if(c==value_[matched_])++matched_;else matched_=c==value_[0]?1:0;
        if(matched_==value_.size()){found_=true;return;}
    }
}
}
