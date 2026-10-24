#include "SettingsMigration.h"
#include "TextLayout.h"
#include "Messages.h"
#include "WeatherEffects.h"
#include "WeatherCodec.h"
#include <cstdlib>
#include <iostream>
#include <set>
#include <sstream>

using namespace pet;
namespace {
unsigned checks=0,groups=0;
#define REQUIRE(x) do { ++checks; if (!(x)) { std::cerr << __func__ << ':' << __LINE__ << " failed: " #x << '\n'; std::exit(1); } } while (0)
WeatherData sample(unsigned code=0,float temperature=20,Tick now=0,uint32_t revision=1,int64_t epoch=1792857600) {
    WeatherData w; w.valid=w.online=true; w.code=code; w.temperature=temperature;
    w.fetchedMs=now; w.observedEpoch=w.fetchedEpoch=epoch; w.revision=revision;
    return w;
}
std::string withoutSpaces(std::string value) {
    value.erase(std::remove_if(value.begin(),value.end(),[](char c){return c==' ' || c=='\n';}),value.end());
    return value;
}
void centeredAndCompleteText() {
    ++groups;
    REQUIRE(text::wrap("Hola").pages.front().top()==12);
    REQUIRE(text::Page::left("Hola")==52);
    REQUIRE(text::wrap("Uno\nDos").pages.front().top()==8);
    REQUIRE(text::wrap("Uno\nDos\nTres").pages.front().top()==4);
    REQUIRE(text::wrap("Uno\nDos\nTres\nCuatro").pages.front().top()==0);
    REQUIRE(text::encode("áéíóúñü¿¡").size()==9);
    REQUIRE(text::encode("¿Sí?")==std::string("\xa8S\xa1?"));
    REQUIRE(text::encode("“hola”…")=="\"hola\"...");
    unsigned multiPage=0;
    for (const char* phrase:messages::randomPhrases) {
        const auto layout=text::wrap(phrase); std::string rebuilt;
        REQUIRE(!layout.pages.empty());
        if (layout.pages.size()>1) ++multiPage;
        for (const auto& page:layout.pages) {
            REQUIRE(page.lines.size()>=1 && page.lines.size()<=4);
            REQUIRE(page.top()*2+int(page.lines.size())*8==32);
            REQUIRE(page.duration()>=3000 && page.duration()<=7000);
            for (const auto& line:page.lines) {
                REQUIRE(line.size()<=21);
                int remainder=128-(text::Page::left(line)*2+int(line.size())*6);
                REQUIRE(remainder==0 || remainder==1);
                if (!rebuilt.empty()) rebuilt+=' ';
                rebuilt+=line;
            }
        }
        // A word wider than 21 cells must split; no character may be lost or reordered.
        REQUIRE(withoutSpaces(rebuilt)==withoutSpaces(text::encode(phrase)));
        REQUIRE(&layout.at(layout.duration()+10000)==&layout.pages.back());
    }
    REQUIRE(multiPage>10);
    auto longWord=text::wrap("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
    REQUIRE(longWord.pages[0].lines.size()==3);
    REQUIRE(longWord.pages[0].lines[0].size()==21);
    REQUIRE(longWord.pages[0].lines[2].size()==10);
    auto balanced=text::wrap("Uno\nDos\nTres\nCuatro\nCinco");
    REQUIRE(balanced.pages[0].lines.size()==3 && balanced.pages[1].lines.size()==2);
}
void migrationAndNewControls() {
    ++groups;
    Settings old; old.humor=80; old.modules=Assistant|Weather; old.gestureMinCm=90; old.gestureMaxCm=120;
    old.quietEnabled=1; old.quietStart=23;
    std::array<uint8_t,legacySettingsSize> bytes{}; std::memcpy(bytes.data(),&old,bytes.size());
    Settings restored; restored.humor=0;
    REQUIRE(restoreSettings(1,bytes.data(),bytes.size(),restored));
    REQUIRE(restored.humor==80 && restored.modules==(Assistant|Weather));
    REQUIRE(restored.gestureMinCm==90 && restored.gestureMaxCm==120);
    REQUIRE(restored.quietEnabled && restored.quietStart==23);
    REQUIRE(restored.randomPhrases && restored.weatherReactions && restored.weatherNotices && !restored.stormFlash);
    restored.randomPhrases=0; restored.stormFlash=1;
    Settings v2; REQUIRE(restoreSettings(2,&restored,sizeof(restored),v2));
    REQUIRE(!v2.randomPhrases && v2.stormFlash && v2.humor==80);
    REQUIRE(!restoreSettings(9,&restored,sizeof(restored),v2));
    REQUIRE(!restoreSettings(1,&restored,5,v2));
    Menu menu; menu.open(Settings{}); menu.item=18;
    menu.input(Press::Long); menu.input(Press::Short); REQUIRE(!menu.draft.randomPhrases);
    menu.input(Press::Long); menu.item=21;
    menu.input(Press::Long); menu.input(Press::Short); REQUIRE(menu.draft.stormFlash);
    menu.input(Press::Long); menu.item=Menu::wifiItem;
    REQUIRE(menu.input(Press::Long)==Menu::Action::Portal);
    menu.item=Menu::saveItem; REQUIRE(menu.input(Press::Long)==Menu::Action::Save);
}
void weatherQueryOrder() {
    ++groups;
    for (unsigned mask=1;mask<=7;++mask) {
        Settings s; s.modules=mask; Query q; q.start(0,s,true,true);
        Tick now=0;
        if (!q.active()) continue;
        if (mask&Assistant) { REQUIRE(q.joke()); q.update(now=1600); }
        if (mask&Weather) {
            REQUIRE(q.view()==View::Weather); q.update(now+=5000);
            if (mask&Assistant) {
                REQUIRE(q.view()==View::WeatherReaction); REQUIRE(q.stageAge(now)==0);
                REQUIRE(!q.update(now+1999)); q.update(now+=2000);
            }
        }
        if (mask&Clock) { REQUIRE(q.view()==View::Clock); q.update(now+=5000); }
        REQUIRE(!q.active());
    }
    Settings s; Query q; q.start(0,s,true,true); REQUIRE(q.totalDuration()==13600);
    s.weatherReactions=0; q.start(0,s,false,true); REQUIRE(q.totalDuration()==10000);
    q.cancel(); REQUIRE(!q.active());
}
void realUpdatesAndWeatherChanges() {
    ++groups;
    WeatherChanges changes;
    auto clear=sample(); changes.observe(clear,0,true); REQUIRE(!changes.pending(0));
    auto rain=sample(61,12,1800000,2,clear.observedEpoch+1800);
    changes.observe(rain,1800000,true); REQUIRE(changes.pending(1800000));
    changes.consume();
    for (unsigned n=0;n<10;++n) changes.observe(rain,1800100+n,true);
    REQUIRE(!changes.pending(1800200)); // gesture/cache reads are not network updates
    auto heavier=sample(65,12,3600000,3,rain.observedEpoch+1800);
    changes.observe(heavier,3600000,true); REQUIRE(!changes.pending(3600000));
    changes=WeatherChanges{}; changes.observe(clear,0,true);
    changes.observe(rain,1800000,false); REQUIRE(!changes.pending(1800000));
    changes.observe(rain,1800010,true); REQUIRE(!changes.pending(1800010)); // quiet never backlogs
    changes=WeatherChanges{}; changes.observe(clear,0,true);
    auto offline=clear; offline.online=false; changes.observe(offline,100,false);
    changes.observe(rain,1800000,true); REQUIRE(!changes.pending(1800000));
    changes=WeatherChanges{}; changes.observe(clear,0,true);
    auto gap=sample(61,10,7200000,2,clear.observedEpoch+7200);
    changes.observe(gap,7200000,true); REQUIRE(!changes.pending(7200000));
    changes=WeatherChanges{}; changes.observe(clear,0,true);
    auto sameTime=sample(61,10,1000,2,clear.observedEpoch);
    changes.observe(sameTime,1000,true); REQUIRE(!changes.pending(1000));
    changes=WeatherChanges{}; changes.observe(clear,0,true); changes.observe(rain,1800000,true);
    REQUIRE(!changes.pending(2100001));
    REQUIRE(!weatherFresh(clear,3600001));
    clear.fetchedEpoch=clear.observedEpoch+6000; REQUIRE(!weatherFresh(clear,0));
    REQUIRE(weatherEpoch("2026-10-22T12:00",-10800)==1792681200);
    REQUIRE(!weatherEpoch("2026-02-30T12:00",0)); REQUIRE(!weatherEpoch("2026-10-22T25:00",0));
    REQUIRE(weatherEpoch("2028-02-29T12:00",0)>0);
}
void sharedBudgetAndPriorities() {
    ++groups; Settings s; s.humor=100;
    AmbientDirector director; director.begin(0);
    REQUIRE(director.poll(0,s,true,true,true,0,false)==Ambient::None);
    REQUIRE(director.poll(20000,s,false,true,true,0,true)==Ambient::None);
    REQUIRE(director.poll(20000,s,true,true,true,0,true)==Ambient::WeatherChange);
    director.note(20000,10000,s,0);
    REQUIRE(director.poll(49999,s,true,true,true,0,true)==Ambient::None);
    REQUIRE(director.poll(50000,s,true,true,true,0,false)!=Ambient::None);
    director.note(50000,2000,s,0);
    REQUIRE(director.poll(72000,s,true,true,true,0,false)!=Ambient::None);
    director.note(72000,2000,s,0); // third intervention must rest for 3 min
    REQUIRE(director.poll(100000,s,true,true,true,0,false)==Ambient::None);
    REQUIRE(director.poll(253999,s,true,true,true,0,false)==Ambient::None);
    REQUIRE(director.poll(254000,s,true,true,true,0,false)!=Ambient::None);
    s.messages=0; s.weatherReactions=0;
    REQUIRE(director.poll(300000,s,true,true,true,0,true)==Ambient::None);
    s.messages=1; s.modules=Clock;
    REQUIRE(director.poll(300000,s,true,true,true,0,true)==Ambient::None);
    s.modules=Assistant; s.randomPhrases=0;
    REQUIRE(director.poll(300000,s,true,false,false,0,false)==Ambient::None);
    s.humor=0; REQUIRE(AmbientDirector::interval(s,0)>=1200000);
    s.humor=100; REQUIRE(AmbientDirector::interval(s,0)==20000);
    Tick boot=UINT32_MAX-10000; AmbientDirector rollover; rollover.begin(boot);
    s.randomPhrases=1;
    REQUIRE(rollover.poll(boot,s,true,true,false,0,false)==Ambient::None);
    REQUIRE(rollover.poll(boot+20000,s,true,true,false,0,false)!=Ambient::None);
}
void distinctStylesAndWeatherEffects() {
    ++groups; std::set<std::array<uint8_t,512>> shapes;
    for (unsigned style=0;style<unsigned(FaceStyle::Count);++style) {
        FaceRenderer renderer(123); Canvas c;
        for (Tick t=0;t<=1980;t+=33) renderer.draw(c,t,Mood::Calm,80,FaceStyle(style));
        REQUIRE(shapes.insert(c.bytes).second);
        unsigned lit=0; for (auto byte:c.bytes) lit+=__builtin_popcount(byte);
        REQUIRE(lit>20 && lit<1400);
    }
    REQUIRE(shapes.size()==19);
    auto storm=sample(95,33); auto e=weatherExpression(storm);
    REQUIRE(e.mood==Mood::Shocked && e.hot);
    auto rain=sample(61,3); e=weatherExpression(rain); REQUIRE(e.cold && e.condition==Condition::Rain);
    REQUIRE(weatherExpression(sample(0,30)).hot); REQUIRE(weatherExpression(sample(0,5)).cold);
    REQUIRE(weatherCondition(59)==Condition::Unknown);
    FaceRenderer a(7),b(7); Canvas flash,normal;
    drawWeatherReaction(a,flash,storm,0,0,100,true);
    drawWeatherReaction(b,normal,storm,0,0,100,false);
    for (unsigned i=0;i<512;++i) REQUIRE(flash.bytes[i]==uint8_t(~normal.bytes[i]));
    drawWeatherReaction(a,flash,storm,100,100,100,true);
    drawWeatherReaction(b,normal,storm,100,100,100,false);
    REQUIRE(flash.bytes==normal.bytes); // no latched inversion
    a.draw(flash,133,Mood::Calm,100); b.draw(normal,133,Mood::Calm,100);
    REQUIRE(flash.bytes==normal.bytes); // interruption also restores normal display
}
void phraseVarietyAndWeatherBanks() {
    ++groups; messages::Picker picker; const char* recent[3]{};
    std::set<const char*> selected;
    uint32_t random=123;
    for (unsigned i=0;i<1000;++i) {
        random^=random<<13; random^=random>>17; random^=random<<5;
        auto phrase=picker.random(random);
        for (auto old:recent) REQUIRE(phrase!=old);
        recent[i%3]=phrase; selected.insert(phrase);
    }
    REQUIRE(selected.size()>95);
    auto data=sample(61,14); std::set<const char*> rainBank;
    for (unsigned n=0;n<4;++n) rainBank.insert(picker.weather(data,n));
    REQUIRE(rainBank.size()==4);
    data.code=95; REQUIRE(!rainBank.count(picker.weather(data,0)));
}
}
void testV2(unsigned& suites,unsigned& assertions) {
    centeredAndCompleteText(); migrationAndNewControls(); weatherQueryOrder(); realUpdatesAndWeatherChanges();
    sharedBudgetAndPriorities(); distinctStylesAndWeatherEffects(); phraseVarietyAndWeatherBanks();
    suites+=groups; assertions+=checks;
}
