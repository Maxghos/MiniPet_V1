#include "AppProtocol.h"
#include "TextLayout.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstdlib>

namespace {
unsigned checks=0,groups=0;
#define REQUIRE(x) do{++checks;if(!(x)){std::cerr<<__func__<<':'<<__LINE__<<" failed: " #x<<'\n';std::exit(1);}}while(0)
std::vector<uint8_t> fixture(){std::ifstream in("protocol/fixtures/frame_clip.mpet",std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void formatAndIntegrity(){
    ++groups;auto bytes=fixture();pet::app::MediaInfo info;
    REQUIRE(pet::app::validateMedia(bytes.data(),bytes.size(),info));REQUIRE(info.frames==2&&info.fps==10&&info.bytes==1024);
    auto bad=bytes;bad[24]^=1;REQUIRE(!pet::app::validateMedia(bad.data(),bad.size(),info));
    for(unsigned index:{0u,4u,5u,6u,8u,10u,12u,20u}){bad=bytes;bad[index]=255;REQUIRE(!pet::app::validateMedia(bad.data(),bad.size(),info));}
    REQUIRE(!pet::app::validateMedia(bytes.data(),bytes.size()-1,info));REQUIRE(!pet::app::validateMedia(nullptr,0,info));
    const uint8_t sample[]={'1','2','3','4','5','6','7','8','9'};REQUIRE(pet::app::crc32(sample,9)==0xcbf43926);
}
void identityAndAuthentication(){
    ++groups;
    for(const char* good:{"hello","id_123","a-b","ABC9"})REQUIRE(pet::app::validAssetId(good));
    for(const char* bad:{"","../secret","/file","file.mpet","a/b","a?x","abcdefghijklmnopqrstuvwxyz"})REQUIRE(!pet::app::validAssetId(bad));
    REQUIRE(pet::app::sameToken(std::string(64,'a'),std::string(64,'a')));
    REQUIRE(!pet::app::sameToken("",""));REQUIRE(!pet::app::sameToken(std::string(63,'a'),std::string(64,'a')));
    auto a=std::string(64,'a');auto b=a;b[63]='b';REQUIRE(!pet::app::sameToken(a,b));
    pet::Menu menu;menu.open(pet::Settings{});menu.item=pet::Menu::appItem;REQUIRE(menu.input(pet::Press::Long)==pet::Menu::Action::PairApp);
}
void standaloneAndLeases(){
    ++groups;pet::app::Playback player;
    REQUIRE(!player.active(0));player.heartbeat(0);REQUIRE(!player.active(0));
    REQUIRE(player.text("¿Todo bien?",100,0,false,25));REQUIRE(player.active(101));
    player.heartbeat(14100);REQUIRE(player.active(28000));
    REQUIRE(!player.active(29100));player.heartbeat(30000);REQUIRE(!player.active(30000)); // an expired pin never revives
    REQUIRE(player.text("Hola",40000,-1,false,25));REQUIRE(player.active(42999));REQUIRE(!player.active(43000));
    player.expire(43000);REQUIRE(player.type(43000)==pet::app::Content::None);
    REQUIRE(!player.text("",0,-1,false,25));REQUIRE(!player.text(std::string(4097,'x'),0,0,false,25));
    REQUIRE(!player.text("hola",0,-2,false,25));REQUIRE(!player.text("hola",0,0,true,9));
    REQUIRE(player.text("Una frase larga para leer en la pantalla con bastante calma y sin perder palabras.",50000,-1,false,25));
    REQUIRE(player.active(53000));player.stop();REQUIRE(!player.active(53001));
    const auto boot=UINT32_MAX-5000;REQUIRE(player.text("Pin",boot,0,false,25));REQUIRE(player.active(boot+10000));
    REQUIRE(!player.active(boot+15000));
}
void frameTimingAndRestore(){
    ++groups;pet::app::Playback player;pet::app::MediaInfo info;auto bytes=fixture();REQUIRE(pet::app::validateMedia(bytes.data(),bytes.size(),info));
    player.media(info,0,5000,false);REQUIRE(player.frame(0)==0);REQUIRE(player.frame(100)==1);
    REQUIRE(player.active(200));REQUIRE(player.frame(200)==1);REQUIRE(!player.active(5000));
    player.media(info,0,-1,false);REQUIRE(!player.active(200));
    player.media(info,1000,1000,true);REQUIRE(player.active(1800));REQUIRE(player.frame(1300)==1);REQUIRE(!player.active(2000));
    info.frames=1;info.fps=1;info.bytes=512;player.media(info,3000,10000,false);
    REQUIRE(player.active(8000)); // a still image must not disappear after one frame
    REQUIRE(!player.active(13000));player.media(info,20000,0,true);player.stop();REQUIRE(!player.active(20001));
}
}
void testApp(unsigned& suites,unsigned& assertions){formatAndIntegrity();identityAndAuthentication();standaloneAndLeases();frameTimingAndRestore();suites+=groups;assertions+=checks;}
