#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include <time.h>
#include "BoardConfig.h"
#include "PetCore.h"
#include "Sensors.h"
#include "Screen.h"
#include "NetworkService.h"
#include "Messages.h"

namespace {
pet::Settings settings;
pet::Menu menu;
pet::TouchButton button;
pet::HoldGesture gesture;
pet::Presence presence;
pet::Personality personality;
pet::Query query;
Sensors sensors;
Screen screen;
NetworkService network;
messages::Picker picker;
bool screenReady = false, networkReady = false;
uint32_t lastFrame = 0, messageAt = 0, lastJokeAt = 0;
bool joked = false;
const char* message = nullptr;
const char* jokeMessage = nullptr;
uint8_t nextWeatherPage = 0, queryWeatherPage = 0;
uint32_t lastDistanceAt = 0;
float lastDistance = 0;
bool hadDistance = false;

void loadSettings() {
    Preferences p;
    if (!p.begin("minipet",true)) return;
    if (p.getUInt("version",0)==1 && p.getBytesLength("settings")==sizeof(settings))
        p.getBytes("settings",&settings,sizeof(settings));
    p.end(); settings.sanitize();
}
bool saveSettings() {
    Preferences p;
    if (!p.begin("minipet",false)) return false;
    bool ok = p.putBytes("settings",&settings,sizeof(settings))==sizeof(settings);
    ok = p.putUInt("version",1)==sizeof(uint32_t) && ok; p.end(); return ok;
}
bool quietNow() {
    time_t now = time(nullptr);
    if (now < 1704067200) return false;
    tm local{}; localtime_r(&now,&local);
    return settings.quiet(local.tm_hour);
}
void startQuery(uint32_t now, bool quiet) {
    personality.interact(now,settings);
    bool joke = !quiet && settings.messages && settings.humor >= 40 &&
        (!joked || pet::elapsed(now,lastJokeAt)>=60000) && esp_random()%100 < settings.humor;
    query.start(now,settings,joke);
    if (query.active()) {
        queryWeatherPage = settings.weatherView==3 ? nextWeatherPage++%3 : settings.weatherView;
        if (query.joke()) {
            jokeMessage = picker.pick(esp_random(),true,true);
            lastJokeAt = now; joked = true;
            personality.react(pet::Mood::Annoyed,now,1600);
        }
    }
    message = nullptr;
}
pet::View homeView(uint32_t now) {
    if (settings.modules & pet::Assistant) return pet::View::Pet;
    if ((settings.modules & (pet::Clock|pet::Weather))==(pet::Clock|pet::Weather)) {
        unsigned weatherPages = settings.weatherView==3 ? 3 : 1;
        unsigned part = (now/(settings.pageSeconds*1000u))%(weatherPages+1);
        return part < weatherPages ? pet::View::Weather : pet::View::Clock;
    }
    return settings.modules & pet::Weather ? pet::View::Weather : pet::View::Clock;
}
}
void setup() {
    Serial.begin(115200);
    loadSettings(); sensors.begin();
    screenReady = screen.begin();
    if (screenReady) { screen.brightness(settings.brightness); screen.status("MiniPet", "Hola, Puente Alto!"); screen.show(); }
    else Serial.println("OLED no disponible: revisar alimentacion, SDA 8, SCL 9 y direccion 0x3C");
    personality.begin(millis()); gesture.reset(millis());
    networkReady = network.begin();
    if (!networkReady) Serial.println("No se pudo iniciar servicio de red");
    Serial.println("MiniPet iniciado. Boton: ajustes. Gesto: 50-80 cm durante 5 s (configurable).");
}
void loop() {
    const uint32_t now = millis();
    const bool quiet = quietNow();
    const pet::Press press = button.update(sensors.touch(),now);
    if (networkReady) network.poll();
    if (press != pet::Press::None) {
        if (network.portalActive()) network.stopPortal();
        else if (!menu.active) {
            menu.open(settings); query.cancel(); message = nullptr;
        } else {
            auto action = menu.input(press);
            if (action == pet::Menu::Action::Save) {
                auto previous = settings; settings = menu.draft;
                if (saveSettings()) { if (screenReady) screen.brightness(settings.brightness); Serial.println("Ajustes guardados"); }
                else {
                    settings = previous; menu.open(menu.draft);
                    Serial.println("No se pudo guardar ajustes");
                }
            } else if (action == pet::Menu::Action::Portal && networkReady) network.startPortal();
        }
        personality.interact(now,settings);
        gesture.reset(now);
    }
    if (sensors.poll(now)) {
        presence.update(now,sensors.valid,sensors.cm,settings.presenceCm);
        if (sensors.valid && hadDistance && pet::elapsed(now,lastDistanceAt)<350 &&
            lastDistance-sensors.cm>25 && sensors.cm<45 && !menu.active && !query.active())
            personality.react(pet::Mood::Shocked,now,1500);
        hadDistance = sensors.valid;
        if (sensors.valid) { lastDistance = sensors.cm; lastDistanceAt = now; }
        if (menu.active || network.portalActive() || query.active()) gesture.reset(now);
        else if (gesture.update(now,sensors.valid,sensors.cm,settings)) startQuery(now,quiet);
    }
    const bool motion = sensors.motion(now);
    if (personality.update(now,presence.present(),motion && !menu.active && !query.active(),quiet,
        !menu.active && !query.active() && !network.portalActive(),settings,esp_random())) {
        message = picker.pick(esp_random(),presence.present()); messageAt = now;
    }
    query.update(now);
    if (message && pet::elapsed(now,messageAt)>=3500) message = nullptr;
    if (screenReady && pet::elapsed(now,lastFrame)>=board::frameMs) {
        lastFrame = now;
        if (network.portalActive()) screen.portal(network.apName(),network.apPassword(),network.connected());
        else if (menu.active) screen.menu(menu,sensors.valid,sensors.cm);
        else if (query.joke()) screen.message(jokeMessage ? jokeMessage : "Ya voy...");
        else if (message && (settings.modules & pet::Assistant)) screen.message(message);
        else {
            auto view = query.active() ? query.view() : homeView(now);
            if (view == pet::View::Pet) screen.face(now,personality.mood(now),settings.humor,gesture.progress(now,settings));
            else if (view == pet::View::Clock) screen.clock(networkReady && network.connected());
            else {
                unsigned part = now/(settings.pageSeconds*1000u);
                unsigned cycle = (settings.modules & pet::Clock) && !(settings.modules & pet::Assistant) ? 4 : 3;
                unsigned page = settings.weatherView==3 ? part%cycle : settings.weatherView;
                if (query.active()) page = queryWeatherPage;
                screen.weather(networkReady ? network.weather() : WeatherData{},page,now);
            }
            if (!query.active()) screen.progress(gesture.progress(now,settings));
        }
        screen.show();
    }
    delay(1);
}
