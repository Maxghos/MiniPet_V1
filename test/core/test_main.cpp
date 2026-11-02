#include "PetCore.h"
#include "FaceRenderer.h"
#include "Messages.h"
#include "WeatherCodec.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <set>
#include <string>

using namespace pet;
static unsigned assertions = 0, suites = 0;
#define CHECK(x) do { ++assertions; if (!(x)) { std::cerr << __func__ << ':' << __LINE__ << " failed: " #x << '\n'; std::exit(1); } } while (0)

void gestureLifecycle() {
    ++suites; Settings s; HoldGesture g;
    // A stationary background in the band at boot never requests anything.
    for (Tick t=0;t<10000;t+=100) CHECK(!g.update(t,true,60,s));
    for (Tick t=10000;t<=11100;t+=100) CHECK(!g.update(t,true,200,s));
    for (Tick t=11200;t<16200;t+=100) {
        CHECK(!g.update(t,true,60,s));
        if(t==13700)CHECK(g.progress(t,s)>.49f&&g.progress(t,s)<.51f);
    }
    CHECK(g.update(16200,true,60,s));
    for (Tick t=16300;t<30000;t+=100) CHECK(!g.update(t,true,60,s));
    for (Tick t=30000;t<=31100;t+=100) CHECK(!g.update(t,true,200,s));
    for (Tick t=31200;t<36200;t+=100) CHECK(!g.update(t,true,62,s));
    CHECK(g.update(36200,true,62,s));
}
void gestureFailuresAndRollover() {
    ++suites; Settings s; HoldGesture g;
    Tick start = std::numeric_limits<Tick>::max()-2500;
    for (Tick dt=0;dt<=1100;dt+=100) g.update(start+dt,true,180,s);
    for (Tick dt=1200;dt<6200;dt+=100) CHECK(!g.update(start+dt,true,60,s));
    CHECK(g.update(start+6200,true,60,s));
    g.reset(10000);
    for (Tick t=10000;t<=11100;t+=100) g.update(t,true,200,s);
    for (Tick t=11200;t<14000;t+=100) CHECK(!g.update(t,true,60,s));
    for(Tick t=14000;t<=14400;t+=100)CHECK(!g.update(t,false,0,s));
    for (Tick t=14500;t<19400;t+=100) CHECK(!g.update(t,true,60,s));
    CHECK(!g.update(24000,true,60,s)); // a data gap cannot count as a continuous hold
    CHECK(g.progress(24000,s)==0);
}
void sensorFiltering() {
    ++suites; DistanceFilter f;
    CHECK(f.sample(60,0)); CHECK(f.sample(61,100)); CHECK(f.sample(300,200));
    CHECK(f.cm()==61); CHECK(!f.sample(NAN,250)); CHECK(!f.sample(0,250));
    CHECK(!f.fresh(551)); CHECK(f.sample(30,1000)); CHECK(f.cm()==30);
    Presence p;
    for (Tick t=0;t<600;t+=100) { p.update(t,true,80,130); CHECK(!p.present()); }
    CHECK(p.update(600,true,80,130)); CHECK(p.present());
    p.update(700,false,0,130); p.update(800,true,80,130); CHECK(p.present());
    for (Tick t=900;t<2900;t+=100) { p.update(t,false,0,130); CHECK(p.present()); }
    CHECK(p.update(2900,false,0,130)); CHECK(!p.present());
}
void touchAndMenu() {
    ++suites; TouchButton b; Menu m; Settings s;
    CHECK(b.update(true,0)==Press::None); CHECK(b.update(false,10)==Press::None);
    CHECK(b.update(false,100)==Press::None); // bounce is ignored
    b.update(true,200); b.update(true,240);
    for (Tick t=300;t<3300;t+=100) CHECK(b.update(true,t)==Press::None);
    b.update(false,3300); CHECK(b.update(false,3340)==Press::Back);
    b.update(true,4000); b.update(true,4040); b.update(false,5000);
    CHECK(b.update(false,5040)==Press::Long);
    b.update(true,6000); b.update(true,6040); b.update(false,6200);
    CHECK(b.update(false,6240)==Press::Short);
    m.open(s); m.item=3; CHECK(m.input(Press::Long)==Menu::Action::None);
    CHECK(m.editing); m.input(Press::Short); CHECK(m.draft.humor==60);
    m.input(Press::Long); CHECK(!m.editing); CHECK(s.humor==50);
    m.item=Menu::saveItem; CHECK(m.input(Press::Long)==Menu::Action::Save); CHECK(!m.active);
    m.open(s); m.draft.modules=Assistant; m.item=0;
    m.input(Press::Long); m.input(Press::Short); CHECK(m.draft.modules==Assistant);
    m.input(Press::Back); CHECK(m.active); CHECK(!m.editing);
    CHECK(m.input(Press::Back)==Menu::Action::Closed);
}
void enabledModulesAndQuery() {
    ++suites;
    for (uint8_t mask=1;mask<=7;++mask) {
        Settings s; s.modules=mask; Query q; q.start(0,s,true);
        const bool pages = mask & (Weather|Clock);
        CHECK(q.active()==pages);
        if (!pages) continue;
        Tick t=0;
        if (mask & Assistant) { CHECK(q.joke()); CHECK(q.view()==View::Pet); q.update(t=1600); }
        CHECK(!q.joke());
        if (mask & Weather) { CHECK(q.view()==View::Weather); q.update(t+=5000); }
        if (mask & Clock) { CHECK(q.view()==View::Clock); q.update(t+=5000); }
        CHECK(!q.active());
    }
}
unsigned reminders(uint8_t humor, bool quiet=false, bool enabled=true, bool allow=true) {
    Settings s; s.humor=humor; s.sleepMinutes=60; s.messages=enabled;
    s.randomPhrases=0; s.weatherReactions=0;
    Personality p; p.begin(0); AmbientDirector director; director.begin(0); unsigned count=0;
    for (Tick t=0;t<1800000;t+=100) {
        p.update(t,true,false,s);
        if (director.poll(t,s,!quiet && allow,p.attentionDue(t,true,s),false,12345,false)==Ambient::Reminder) {
            p.remind(t,s); director.note(t,3500,s,12345); ++count;
        }
    }
    return count;
}
void humorAndPriorities() {
    ++suites;
    unsigned calm=reminders(0), active=reminders(100);
    CHECK(calm<=1); CHECK(active>calm+5); CHECK(active<40);
    CHECK(reminders(100,true)==0); CHECK(reminders(100,false,false)==0);
    CHECK(reminders(100,false,true,false)==0); // menu/query blocks reminders
    Settings s; Personality p; p.begin(0);
    p.update(1000,true,false,s);
    p.update(600000,true,false,s);
    CHECK(p.mood(600000)==Mood::Calm); // stationary company is still company
    p.update(1600000,false,false,s);
    CHECK(p.mood(1600000)==Mood::Sleeping);
    p.update(1601000,true,false,s); CHECK(p.mood(1601000)==Mood::Happy);
    p.interact(1601100,s); CHECK(p.reminders()==0);
}
void settingsAndQuietHours() {
    ++suites; Settings s; s.modules=0; s.humor=255; s.gestureMinCm=200; s.gestureMaxCm=10;
    s.gestureMs=0; s.pageSeconds=0; s.sanitize();
    CHECK(s.modules==Assistant); CHECK(s.humor==100); CHECK(s.gestureMaxCm>s.gestureMinCm);
    CHECK(s.gestureMs>=2000); CHECK(s.pageSeconds>=3);
    s.quietEnabled=1;
    CHECK(s.quiet(22)); CHECK(s.quiet(0)); CHECK(s.quiet(7)); CHECK(!s.quiet(8)); CHECK(!s.quiet(-1));
    s.quietStart=9; s.quietEnd=17;
    CHECK(s.quiet(9)); CHECK(!s.quiet(8)); CHECK(!s.quiet(17));
}
void faceFramesAndMessages() {
    ++suites; FaceRenderer renderer(123); Canvas canvas;
    unsigned changed=0; auto previous=canvas.bytes;
    for (unsigned mood=0;mood<unsigned(Mood::Count);++mood) {
        std::set<std::array<uint8_t,512>> unique;
        for (Tick t=0;t<10000;t+=33) {
            renderer.draw(canvas,t+10000*mood,Mood(mood),75);
            unique.insert(canvas.bytes);
            if (canvas.bytes!=previous) ++changed;
            previous=canvas.bytes;
        }
        unsigned lit=0; for (auto byte:canvas.bytes) lit+=__builtin_popcount(byte);
        CHECK(lit>20); CHECK(lit<1500);
        CHECK(unique.size()>(mood==7 ? 3u : 8u));
        std::cout << "Face " << mood << ": " << unique.size() << " distinct frames\n";
    }
    CHECK(changed>100);
    canvas.rect(-3,-3,10,10); canvas.line(-5,-5,135,35); canvas.fade(0);
    for (auto v:canvas.bytes) CHECK(v==0);
    messages::Picker picker; const char* recent[3]{};
    for (unsigned i=0;i<200;++i) {
        auto next=picker.pick(0,false);
        for (auto old:recent) CHECK(next!=old);
        recent[i%3]=next;
        CHECK(std::strlen(next)<=80);
    }
}
void weatherValidation(const char* path) {
    ++suites;
    std::ifstream file(path); CHECK(file.good());
    JsonDocument doc; CHECK(!deserializeJson(doc,file));
    WeatherData w; CHECK(decodeWeather(doc,w)); CHECK(w.valid);
    CHECK(w.days[0].min<=w.days[0].max);
    auto good=w;
    doc["current"]["temperature_2m"]=nullptr;
    CHECK(!decodeWeather(doc,w)); CHECK(w.temperature==good.temperature);
    doc["current"]["temperature_2m"]=25;
    doc["daily"]["temperature_2m_min"][1]=80;
    CHECK(!decodeWeather(doc,w)); CHECK(w.days[1].min==good.days[1].min);
    CHECK(std::string(weatherLabel(95))=="Tormenta");
    CHECK(std::string(weatherLabel(61))=="Lluvia");
}
void testV2(unsigned& suites,unsigned& assertions);
void testApp(unsigned& suites,unsigned& assertions);
void testStudy(unsigned& suites,unsigned& assertions);
void testSensorBehavior(unsigned& suites,unsigned& assertions);
int main(int argc,char** argv) {
    gestureLifecycle(); gestureFailuresAndRollover(); sensorFiltering(); touchAndMenu();
    enabledModulesAndQuery(); humorAndPriorities(); settingsAndQuietHours(); faceFramesAndMessages();
    weatherValidation(argc>1 ? argv[1] : "test/core/weather_sample.json");
    testV2(suites,assertions);
    testApp(suites,assertions);
    testStudy(suites,assertions);
    testSensorBehavior(suites,assertions);
    std::cout << suites << " suites passed, " << assertions << " assertions\n";
}
