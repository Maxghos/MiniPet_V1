#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include <time.h>
#include "BoardConfig.h"
#include "PetCore.h"
#include "SettingsMigration.h"
#include "Sensors.h"
#include "Screen.h"
#include "NetworkService.h"
#include "Messages.h"
#include "WeatherBehavior.h"
#include "AppService.h"
#include <WiFi.h>

namespace {
pet::Settings settings;
pet::Menu menu;
pet::TouchButton button;
pet::HoldGesture gesture;
pet::Presence presence;
pet::Personality personality;
pet::Query query;
pet::AmbientDirector ambient;
pet::WeatherChanges weatherChanges;
Sensors sensors;
Screen screen;
NetworkService network;
AppService app;
StudyService study;
pet::study::Menu studyMenu;
pet::study::Snapshot studyState;
pet::app::RemoteView remoteContent;
bool appReady=false;
bool wasRemoteActive=false;
bool wasStudying=false;
uint32_t studyErrorAt=0;
bool studyError=false;
messages::Picker picker;
WeatherData weather,queryWeather,ambientWeather;
bool screenReady=false,networkReady=false;
uint32_t lastFrame=0,messageAt=0,messageDuration=0,lastJokeAt=0,lastWeatherPoll=0;
uint32_t ambientAt=0;
bool joked=false,ambientReaction=false;
const char* message=nullptr;
const char* jokeMessage=nullptr;
uint8_t nextWeatherPage=0,queryWeatherPage=0;
uint32_t lastDistanceAt=0;
float lastDistance=0;
bool hadDistance=false;

void loadSettings() {
    Preferences p;
    if (!p.begin("minipet",true)) return;
    const auto version=p.getUInt("version",0);
    const auto size=p.getBytesLength("settings");
    std::array<uint8_t,sizeof(pet::Settings)> data{};
    if ((size==pet::legacySettingsSize || size==sizeof(pet::Settings)) && p.getBytes("settings",data.data(),size)==size)
        pet::restoreSettings(version,data.data(),size,settings);
    p.end(); settings.sanitize();
}
bool saveSettings() {
    Preferences p;
    if (!p.begin("minipet",false)) return false;
    bool ok=p.putBytes("settings",&settings,sizeof(settings))==sizeof(settings);
    ok=p.putUInt("version",2)==sizeof(uint32_t) && ok;
    p.end(); return ok;
}
bool quietNow() {
    time_t now=time(nullptr);
    if (now<1704067200) return false;
    tm local{}; localtime_r(&now,&local);
    return settings.quiet(local.tm_hour);
}
void startQuery(uint32_t now, bool quiet) {
    personality.interact(now,settings);
    message=nullptr; ambientReaction=false;
    queryWeather=weather;
    if (settings.modules&pet::Weather)
        queryWeatherPage=settings.weatherView==3 ? nextWeatherPage++%3 : settings.weatherView;
    const bool currentWeather=(settings.modules&pet::Weather) && queryWeatherPage!=2 && weatherFresh(queryWeather,now);
    const bool joke=!quiet && settings.messages && settings.humor>=40 &&
        (!joked || pet::elapsed(now,lastJokeAt)>=60000) && esp_random()%100<settings.humor;
    query.start(now,settings,joke,currentWeather && !quiet);
    if (query.joke()) {
        jokeMessage=currentWeather ? picker.weather(queryWeather,esp_random()) : picker.pick(esp_random(),true,true);
        lastJokeAt=now; joked=true;
        personality.react(pet::Mood::Smug,now,query.totalDuration()+2200);
    }
    ambient.note(now,query.totalDuration(),settings,esp_random(),false);
}
pet::View homeView(uint32_t now) {
    if (settings.modules&pet::Assistant) return pet::View::Pet;
    if ((settings.modules&(pet::Clock|pet::Weather))==(pet::Clock|pet::Weather)) {
        const unsigned weatherPages=settings.weatherView==3 ? 3 : 1;
        const unsigned part=(now/(settings.pageSeconds*1000u))%(weatherPages+1);
        return part<weatherPages ? pet::View::Weather : pet::View::Clock;
    }
    return settings.modules&pet::Weather ? pet::View::Weather : pet::View::Clock;
}
void showMessage(const char* value, uint32_t now, pet::Mood mood) {
    message=value; messageAt=now; messageDuration=pet::text::wrap(value).duration();
    personality.react(mood,now,messageDuration+2200);
    ambient.note(now,messageDuration,settings,esp_random());
}
}
void setup() {
    Serial.begin(115200);
    loadSettings(); sensors.begin();
    screenReady=screen.begin();
    if (screenReady) { screen.brightness(settings.brightness); screen.status("MiniPet V3.1","Hola, Puente Alto!"); screen.show(); }
    else Serial.println("OLED no disponible: revisar alimentacion, SDA 8, SCL 9 y direccion 0x3C");
    const uint32_t now=millis();
    personality.begin(now); gesture.reset(now); ambient.begin(now);
    networkReady=network.begin();
    if(!study.begin())Serial.println("No se pudo iniciar modo estudio");
    appReady=app.begin(settings,study);
    if (!networkReady) Serial.println("No se pudo iniciar servicio de red");
    Serial.println("MiniPet V3.1 iniciado. Boton: ajustes. App por WiFi :8080 o BLE.");
}
void loop() {
    const uint32_t now=millis();
    const bool quiet=quietNow();
    const pet::Press press=button.update(sensors.touch(),now);
    study.poll();study.read(studyState);
    const bool studying=studyState.active();
    if(studying!=wasStudying){
        query.cancel();message=nullptr;ambientReaction=false;gesture.reset(now);
        ambient.note(now,0,settings,esp_random(),false);
        if(studying&&appReady)app.stop();
        wasStudying=studying;
    }
    if(appReady){
        app.poll();
        if(app.recentlyContacted())network.keepPortalAlive();
        if(app.takePaired()){menu.active=false;studyMenu.active=false;gesture.reset(now);}
        pet::Settings requested;
        if(app.takeSettings(requested)){
            const auto previous=settings;settings=requested;
            if(!saveSettings()){settings=previous;app.updateSettings(settings);}
            if(screenReady)screen.brightness(settings.brightness);
            if(menu.active)menu.open(settings);
        }
        if(app.takeInteraction()){
            query.cancel();message=nullptr;ambientReaction=false;
            personality.interact(now,settings);ambient.note(now,0,settings,esp_random(),false);
        }
        app.read(remoteContent,now);
    }
    const bool remoteActive=remoteContent.type!=pet::app::Content::None;
    if(wasRemoteActive&&!remoteActive){ambient.note(now,0,settings,esp_random(),false);gesture.reset(now);}
    wasRemoteActive=remoteActive;
    if (networkReady) {
        network.poll();
        if (pet::elapsed(now,lastWeatherPoll)>=250) { weather=network.weather(); lastWeatherPoll=now; }
    }
    if (press!=pet::Press::None) {
        if(appReady){app.stop();remoteContent.type=pet::app::Content::None;}
        if(appReady && app.pairing())app.closePairing();
        else if (network.configuring()) network.stopPortal();
        else if (studyMenu.active) {
            const auto action=studyMenu.input(press,studyState);
            const char* operation=nullptr;
            using Action=pet::study::Menu::Action;
            switch(action){
            case Action::Start:operation="study.start";break;
            case Action::Pause:operation="study.pause";break;
            case Action::Resume:operation="study.resume";break;
            case Action::Skip:operation="study.skip";break;
            case Action::Stop:operation="study.stop";break;
            case Action::Save:operation="study.configure";break;
            default:break;
            }
            if(operation){
                const auto* config=(action==Action::Start||action==Action::Save)?&studyMenu.draft:nullptr;
                if(study.control(operation,studyState.revision,config)){
                    study.read(studyState);
                    if(action!=Action::Save){studyMenu.active=false;menu.active=false;}
                }else{studyError=true;studyErrorAt=now;}
            }
        } else if (!menu.active) {
            menu.open(settings); query.cancel(); message=nullptr; ambientReaction=false;
            if(studyState.active())menu.item=pet::Menu::studyItem;
        } else {
            const auto action=menu.input(press);
            if (action==pet::Menu::Action::Save) {
                const auto previous=settings; settings=menu.draft;
                if (saveSettings()) { if (screenReady) screen.brightness(settings.brightness); if(appReady)app.updateSettings(settings); Serial.println("Ajustes guardados"); }
                else { settings=previous; menu.open(menu.draft); Serial.println("No se pudo guardar ajustes"); }
            } else if (action==pet::Menu::Action::Portal && networkReady) network.startPortal();
            else if(action==pet::Menu::Action::Study){study.read(studyState);studyMenu.open(studyState);}
            else if(action==pet::Menu::Action::PairApp && appReady){
                if(!network.connected())network.startPortal(true);
                app.openPairing();
            }
        }
        personality.interact(now,settings);
        gesture.reset(now); ambient.note(now,0,settings,esp_random(),false);
    }
    if(appReady)app.localBusy(menu.active||studyMenu.active||network.configuring()||app.pairing());
    if (sensors.poll(now)) {
        presence.update(now,sensors.valid,sensors.cm,settings.presenceCm);
        if (sensors.valid && hadDistance && pet::elapsed(now,lastDistanceAt)<350 &&
            lastDistance-sensors.cm>25 && sensors.cm<45 && !menu.active && !query.active())
            personality.react(pet::Mood::Shocked,now,1500);
        hadDistance=sensors.valid;
        if (sensors.valid) { lastDistance=sensors.cm; lastDistanceAt=now; }
        if (menu.active || network.configuring() || query.active() || remoteActive || studying) gesture.reset(now);
        else if (gesture.update(now,sensors.valid,sensors.cm,settings)) startQuery(now,quiet);
    }
    personality.update(now,presence.present(),sensors.motion(now) && !menu.active && !query.active() && !remoteActive && !studying,settings);
    query.update(now);
    if (message && pet::elapsed(now,messageAt)>=messageDuration) message=nullptr;
    if (ambientReaction && pet::elapsed(now,ambientAt)>=2000) ambientReaction=false;
    const auto mood=personality.mood(now);
    const bool awake=mood!=pet::Mood::Sleeping;
    const bool notices=awake && !quiet && !studying && settings.messages && settings.weatherNotices &&
        (settings.modules&(pet::Assistant|pet::Weather))==(pet::Assistant|pet::Weather);
    weatherChanges.observe(weather,now,notices);
    const bool allow=awake && !quiet && !menu.active && !network.configuring() && !query.active() &&
        !message && !ambientReaction && !remoteActive && !studying && gesture.progress(now,settings)==0 &&
        mood!=pet::Mood::Shocked && mood!=pet::Mood::Attentive && homeView(now)==pet::View::Pet;
    const auto event=ambient.poll(now,settings,allow,personality.attentionDue(now,presence.present(),settings),
        weatherFresh(weather,now),esp_random(),weatherChanges.pending(now));
    switch (event) {
    case pet::Ambient::Phrase:
        showMessage(picker.random(esp_random()),now,settings.humor>=70 ? pet::Mood::Smug : settings.humor>=40 ? pet::Mood::Happy : pet::Mood::Calm);
        break;
    case pet::Ambient::Reminder:
        personality.remind(now,settings);
        showMessage(picker.pick(esp_random(),presence.present()),now,personality.mood(now));
        break;
    case pet::Ambient::Weather:
        ambientReaction=true; ambientAt=now; ambientWeather=weather;
        ambient.note(now,2000,settings,esp_random());
        break;
    case pet::Ambient::WeatherChange:
        weatherChanges.consume(); showMessage("El clima ahora indica lluvia",now,pet::Mood::Curious);
        break;
    default: break;
    }
    if (screenReady && pet::elapsed(now,lastFrame)>=board::frameMs) {
        lastFrame=now;
        if(appReady&&app.pairing()){
            const String ip=network.connected()?WiFi.localIP().toString():WiFi.softAPIP().toString();
            screen.pairing(app.pin(),ip.c_str(),network.portalActive()?network.apName():"",network.apPassword(),app.pairingSeconds());
        }
        else if (network.configuring()) screen.portal(network.apName(),network.apPassword(),network.connected());
        else if(studyError&&pet::elapsed(now,studyErrorAt)<2500)screen.message("No se pudo aplicar.\nRevisa el estado\ny los tiempos.");
        else if(studyMenu.active)screen.studyMenu(studyMenu,studyState);
        else if (menu.active) screen.menu(menu,sensors.valid,sensors.cm);
        else if(studyState.active()||(studyState.event==pet::study::Event::Stopped&&studyState.eventAgeMs<6500))screen.study(studyState,now,quiet?0:settings.humor);
        else if(remoteContent.type!=pet::app::Content::None)screen.remote(remoteContent);
        else if (query.joke()) screen.message(jokeMessage ? jokeMessage : "Ya voy...",query.stageAge(now));
        else if (query.active()) {
            if (query.view()==pet::View::WeatherReaction) screen.weatherReaction(queryWeather,now,query.stageAge(now),settings.humor,settings.stormFlash);
            else if (query.view()==pet::View::Weather) screen.weather(queryWeather,queryWeatherPage,now);
            else screen.clock(networkReady && network.connected());
        } else if (message && (settings.modules&pet::Assistant)) screen.message(message,pet::elapsed(now,messageAt));
        else if (ambientReaction) screen.weatherReaction(ambientWeather,now,pet::elapsed(now,ambientAt),settings.humor,settings.stormFlash);
        else {
            const auto view=homeView(now);
            if (view==pet::View::Pet) screen.face(now,personality.mood(now),settings.humor,gesture.progress(now,settings));
            else if (view==pet::View::Clock) screen.clock(networkReady && network.connected());
            else {
                const unsigned part=now/(settings.pageSeconds*1000u);
                const unsigned cycle=(settings.modules&pet::Clock) && !(settings.modules&pet::Assistant) ? 4 : 3;
                screen.weather(weather,settings.weatherView==3 ? part%cycle : settings.weatherView,now);
            }
            screen.progress(gesture.progress(now,settings));
        }
        screen.show();
    }
    delay(1);
}
