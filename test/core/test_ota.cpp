#include "OtaPackage.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>

namespace {
unsigned groups=0,checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<__func__<<':'<<__LINE__<<" failed: " #x<<'\n';std::exit(1);}}while(0)
using namespace pet::ota;
const std::string target="minipet-s3-n16r8-touch",version="3.3.0";
std::array<uint8_t,headerBytes> header(){
    std::array<uint8_t,headerBytes> raw{};memcpy(raw.data(),"MPFW",4);raw[4]=1;raw[6]=128;raw[9]=8;
    memcpy(raw.data()+12,target.data(),target.size());memcpy(raw.data()+44,version.data(),version.size());
    for(unsigned i=0;i<32;++i)raw[76+i]=uint8_t(i*7+3);
    return raw;
}
bool parse(const std::array<uint8_t,headerBytes>& raw,Package& out){return parsePackage(raw.data(),raw.size(),headerBytes+2048,target,6553600,out);}
void validPackageAndFailureAtomicity(){
    ++groups;auto raw=header();Package out;CHECK(parse(raw,out));CHECK(out.target==target);CHECK(out.version==version);CHECK(out.imageBytes==2048);
    for(unsigned i=0;i<32;++i)CHECK(out.digest[i]==uint8_t(i*7+3));
    Package sentinel;sentinel.target="preservar";sentinel.imageBytes=42;
    for(size_t size=0;size<headerBytes;++size){CHECK(!parsePackage(raw.data(),size,headerBytes+2048,target,6553600,sentinel));CHECK(sentinel.target=="preservar");CHECK(sentinel.imageBytes==42);}
    CHECK(!parsePackage(nullptr,128,headerBytes+2048,target,6553600,sentinel));
    CHECK(!parsePackage(raw.data(),raw.size(),headerBytes+2049,target,6553600,sentinel));
    CHECK(!parsePackage(raw.data(),raw.size(),headerBytes+2047,target,6553600,sentinel));
    CHECK(!parsePackage(raw.data(),raw.size(),headerBytes+2048,target,2047,sentinel));
    CHECK(!parsePackage(raw.data(),raw.size(),127,target,6553600,sentinel));
    CHECK(sentinel.target=="preservar");CHECK(sentinel.imageBytes==42);
}
void malformedFieldsAndReservedBytes(){
    ++groups;const auto valid=header();Package out;
    for(unsigned index:{0u,1u,2u,3u,4u,5u,6u,7u,108u,127u}){auto raw=valid;raw[index]^=1;CHECK(!parse(raw,out));}
    for(unsigned offset:{12u,44u}){
        auto raw=valid;std::fill(raw.begin()+offset,raw.begin()+offset+32,0);CHECK(!parse(raw,out));
        raw=valid;std::fill(raw.begin()+offset,raw.begin()+offset+32,'x');CHECK(!parse(raw,out));
        raw=valid;raw[offset]='/';CHECK(!parse(raw,out));
        raw=valid;raw[offset+31]='x';CHECK(!parse(raw,out));
    }
    auto raw=valid;raw[8]=255;raw[9]=3;CHECK(!parse(raw,out)); // image smaller than 1024 bytes
    raw=valid;raw[8]=raw[9]=raw[10]=raw[11]=255;CHECK(!parse(raw,out));
    CHECK(!parsePackage(valid.data(),valid.size(),headerBytes+2048,"minipet-s3-n16r8-button",6553600,out));
}
void espImagePrefix(){
    ++groups;std::array<uint8_t,24> image{};image[0]=0xe9;image[1]=5;image[3]=0x4f;image[12]=9;image[23]=1;
    CHECK(validImagePrefix(image.data(),image.size()));CHECK(!validImagePrefix(nullptr,image.size()));
    for(size_t size=0;size<24;++size)CHECK(!validImagePrefix(image.data(),size));
    for(unsigned index:{0u,12u,13u,23u}){auto bad=image;bad[index]^=1;CHECK(!validImagePrefix(bad.data(),bad.size()));}
    auto bad=image;bad[1]=0;CHECK(!validImagePrefix(bad.data(),bad.size()));bad[1]=17;CHECK(!validImagePrefix(bad.data(),bad.size()));
    bad=image;bad[3]=0x3f;CHECK(!validImagePrefix(bad.data(),bad.size()));
}
void markersAcrossEveryFragmentBoundary(){
    ++groups;const std::string value="\nMPFW-TARGET:"+target+"\n";
    for(size_t split=0;split<=value.size();++split){
        Marker marker;marker.reset(value);marker.add(reinterpret_cast<const uint8_t*>(value.data()),split);
        if(split<value.size())CHECK(!marker.found());
        marker.add(reinterpret_cast<const uint8_t*>(value.data()+split),value.size()-split);CHECK(marker.found());
        marker.add(reinterpret_cast<const uint8_t*>("otra cosa"),9);CHECK(marker.found());
    }
    Marker marker;marker.reset(value);const std::string wrong="\nMPFW-TARGET:minipet-s3-n16r8-button\n";
    for(unsigned i=0;i<2;++i)marker.add(reinterpret_cast<const uint8_t*>(wrong.data()),wrong.size());
    CHECK(!marker.found());
    for(unsigned char byte:value)marker.add(&byte,1);
    CHECK(marker.found());
    marker.reset("\nMPFW-VERSION:3.3.0\n");CHECK(!marker.found());
    const std::string recovery="noise\nMPFW-VE\nMPFW-VERSION:3.3.0\nextra";
    marker.add(reinterpret_cast<const uint8_t*>(recovery.data()),recovery.size());CHECK(marker.found());
    marker.reset("");marker.add(reinterpret_cast<const uint8_t*>(recovery.data()),recovery.size());CHECK(!marker.found());
}
}
void testOta(unsigned& suites,unsigned& assertions){
    validPackageAndFailureAtomicity();malformedFieldsAndReservedBytes();espImagePrefix();markersAcrossEveryFragmentBoundary();suites+=groups;assertions+=checks;
}
