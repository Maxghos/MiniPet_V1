#include "NetworkService.h"
#include "NetworkGuard.h"
#include "WeatherCodec.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <esp_system.h>
#include <time.h>

extern const char weatherRoot[] asm("_binary_certs_isrgrootx1_pem_start");
namespace {
constexpr const char* santiagoTimezone = "CLT4CLST3,M9.1.0/0,M4.1.0/0";
constexpr const char* forecastUrl =
    "https://api.open-meteo.com/v1/forecast?latitude=-33.6117&longitude=-70.5758"
    "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max"
    "&timezone=America%2FSantiago&forecast_days=3";
constexpr const char* form = R"HTML(<!doctype html><html lang="es"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>MiniPet - Wi-Fi</title><style>body{font:18px system-ui;background:#151c28;color:#eef;padding:24px;max-width:420px;margin:auto}input,button{box-sizing:border-box;width:100%;padding:12px;margin:8px 0 20px;border-radius:8px;border:0}button{background:#9fe6bd}p{line-height:1.5}</style><h1>Conecta tu MiniPet</h1><p>Elige una red Wi-Fi de 2,4 GHz. Los datos se guardan en tu mascota.</p><form method="post" action="/wifi"><label>Nombre de la red<input name="ssid" maxlength="32" required autocomplete="off"></label><label>Clave<input name="password" type="password" maxlength="63" autocomplete="new-password"></label><button>Guardar y conectar</button></form><p>Vuelve a conectar tu telefono a su red habitual cuando termines.</p></html>)HTML";
}
bool NetworkService::begin() {
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) return false;
    Preferences p;
    if (p.begin("minipet-wifi",true)) {
        p.getString("ssid",ssid_,sizeof(ssid_));
        p.getString("password",password_,sizeof(password_)); p.end();
    }
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    // The system timezone is also valid offline, once time has been synchronized.
    setenv("TZ",santiagoTimezone,1); tzset();
    server_.on("/",HTTP_GET,[this]{ server_.send(200,"text/html; charset=utf-8",form); });
    server_.on("/wifi",HTTP_POST,[this]{ submitWifi(); });
    server_.onNotFound([this]{ server_.sendHeader("Location","/",true); server_.send(302,"text/plain",""); });
    if (xTaskCreatePinnedToCore(taskEntry,"minipet-net",16384,this,1,&task_,0) != pdPASS) return false;
    return true;
}
bool NetworkService::connected() const { return WiFi.status() == WL_CONNECTED; }
WeatherData NetworkService::weather() {
    WeatherData copy;
    if (mutex_ && xSemaphoreTake(mutex_,pdMS_TO_TICKS(10)) == pdTRUE) {
        copy = weather_; xSemaphoreGive(mutex_);
    }
    copy.online = connected();
    return copy;
}
void NetworkService::taskEntry(void* ctx) { static_cast<NetworkService*>(ctx)->run(); }
void NetworkService::run() {
    uint32_t lastConnect = millis()-30000, lastFetch = millis()-1800000;
    uint32_t lastNtp = millis()-21600000;
    bool previousConnected = false;
    for (;;) {
        const uint32_t now = millis();
        bool changed = false;
        char ssid[33]{}, password[64]{};
        if (xSemaphoreTake(mutex_,pdMS_TO_TICKS(20)) == pdTRUE) {
            changed = credentialsChanged_; credentialsChanged_ = false;
            memcpy(ssid,ssid_,sizeof(ssid)); memcpy(password,password_,sizeof(password));
            xSemaphoreGive(mutex_);
        }
        if (changed) { WiFi.disconnect(false,false); lastConnect = now-30000; lastFetch = now-1800000; }
        const bool online = connected();
        if (!online && ssid[0] && uint32_t(now-lastConnect) >= 30000) {
            WiFi.begin(ssid,password); lastConnect = now;
        }
        if (online && (!previousConnected || uint32_t(now-lastNtp) >= 21600000)) {
            configTzTime(santiagoTimezone,"pool.ntp.org","time.google.com"); lastNtp = now;
        }
        // Authenticate HTTPS only after a plausible SNTP/system time is available.
        if (online && time(nullptr) > 1704067200 && uint32_t(now-lastFetch) >= 1800000) {
            const bool fetched=fetchWeather();lastFetch=fetched?now:now-1740000;
        }
        previousConnected = online;
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
bool NetworkService::fetchWeather() {
    NetworkGuard lock;if(!lock)return false;
    WiFiClientSecure client;
    client.setCACert(weatherRoot);
    client.setHandshakeTimeout(8);
    HTTPClient http;
    http.setConnectTimeout(5000); http.setTimeout(6000);
    http.useHTTP10(true); // avoid chunked stream parsing on this Arduino core
    if (!http.begin(client,forecastUrl)) return false;
    bool accepted=false;
    int status = http.GET();
    if (status == HTTP_CODE_OK && (http.getSize() < 0 || http.getSize() <= 16384)) {
        JsonDocument doc;
        auto error = deserializeJson(doc,http.getStream(),DeserializationOption::NestingLimit(8));
        WeatherData parsed;
        if (!error && decodeWeather(doc,parsed)) {
            parsed.fetchedMs = millis(); parsed.fetchedEpoch = time(nullptr);
            if (xSemaphoreTake(mutex_,pdMS_TO_TICKS(20)) == pdTRUE) {
                if (!weather_.valid || parsed.observedEpoch>=weather_.observedEpoch) {
                    parsed.revision=weather_.revision+1;
                    weather_ = parsed;
                    accepted=true;
                }
                xSemaphoreGive(mutex_);
            }
            Serial.println(accepted ? "Clima actualizado: Puente Alto" : "Observacion anterior o servicio ocupado; se conserva el clima");
        } else Serial.println("Respuesta de clima invalida; se conserva la anterior");
    } else Serial.printf("Clima: HTTP %d; se conserva la ultima lectura\n",status);
    http.end();
    return accepted;
}
void NetworkService::startPortal(bool forApp) {
    appPortal_=forApp;
    if (portal_) return;
    snprintf(apName_,sizeof(apName_),"MiniPet-%04X",unsigned(ESP.getEfuseMac() & 0xffff));
    snprintf(apPassword_,sizeof(apPassword_),"%08lu",static_cast<unsigned long>(esp_random()%100000000));
    WiFi.mode(WIFI_AP_STA);
    if (!WiFi.softAP(apName_,apPassword_)) { Serial.println("No se pudo iniciar configuracion WiFi"); return; }
    dns_.start(53,"*",WiFi.softAPIP());
    server_.begin(); portalAt_ = millis(); portal_ = true;
    // The temporary AP key is shown on the physical OLED, never logged.
    Serial.println("Configuracion WiFi abierta; ver pantalla de MiniPet");
}
void NetworkService::stopPortal() {
    if (!portal_) return;
    server_.stop(); dns_.stop(); WiFi.softAPdisconnect(true); portal_ = false;
}
void NetworkService::poll() {
    if (!portal_) return;
    dns_.processNextRequest(); server_.handleClient();
    if (uint32_t(millis()-portalAt_) >= 300000) stopPortal();
}
void NetworkService::submitWifi() {
    String ssid = server_.arg("ssid"), password = server_.arg("password");
    if (!ssid.length() || ssid.length()>32 || (password.length() && password.length()<8) || password.length()>63) {
        server_.send(400,"text/plain; charset=utf-8","Revisa el nombre y la clave (8 a 63 caracteres, o vacia para red abierta)."); return;
    }
    Preferences p;
    bool saved = p.begin("minipet-wifi",false);
    if (saved) {
        saved = p.putString("ssid",ssid) > 0 && p.putString("password",password) > 0;
        p.end();
    }
    if (!saved) { server_.send(500,"text/plain","No se pudo guardar. Intenta otra vez."); return; }
    if (xSemaphoreTake(mutex_,pdMS_TO_TICKS(100)) == pdTRUE) {
        ssid.toCharArray(ssid_,sizeof(ssid_)); password.toCharArray(password_,sizeof(password_));
        credentialsChanged_ = true; xSemaphoreGive(mutex_);
    }
    server_.send(200,"text/html; charset=utf-8","<meta name='viewport' content='width=device-width'><h1>Datos guardados</h1><p>MiniPet intentara conectarse. Puedes cerrar la configuracion desde su boton.</p>");
}
