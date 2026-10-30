#include "Screen.h"
#include "BoardConfig.h"
#include <Wire.h>
#include <time.h>
#include <cstring>
#include <esp_system.h>

bool Screen::begin() {
    faces_ = pet::FaceRenderer(esp_random());
    Wire.begin(board::sda,board::scl,400000);
    if (!display_.begin(SSD1306_SWITCHCAPVCC,board::oledAddress,false,false)) return false;
    display_.setTextWrap(false); display_.setTextColor(SSD1306_WHITE);
    display_.cp437(true);
    clear(); return true;
}
void Screen::brightness(uint8_t value) {
    display_.ssd1306_command(SSD1306_SETCONTRAST); display_.ssd1306_command(value);
}
void Screen::clear() { display_.clearDisplay(); display_.setTextSize(1); }
void Screen::text(int x, int y, const char* value, unsigned size) {
    display_.setTextSize(size); display_.setCursor(x,y); display_.print(value);
}
void Screen::show() { display_.display(); }
void Screen::pairing(const char* pin,const char* ip,const char* name,const char* password,unsigned seconds) {
    clear();char value[32];snprintf(value,sizeof(value),"PIN %s (%us)",pin,seconds);text(0,0,value);
    text(0,8,ip);
    if(name && *name){text(0,16,name);snprintf(value,sizeof(value),"Clave %s",password);text(0,24,value);}
    else {text(0,16,"Abre app > Vincular");text(0,24,"Boton: cancelar");}
}
void Screen::remote(const pet::app::RemoteView& content) {
    if(content.type==pet::app::Content::Media){memcpy(display_.getBuffer(),content.frame,512);return;}
    if(!content.scrolling){message(content.text.c_str(),content.age);return;}
    clear();const auto encoded=pet::text::encode(content.text.c_str());
    const int width=int(encoded.size()*6)+128;
    int x=128-int(uint64_t(content.age)*content.speed/1000%width);
    display_.setCursor(x,12);
    for(unsigned char c:encoded)display_.write(c=='\n' ? ' ' : c);
}
void Screen::progress(float value) {
    if (value <= 0) return;
    display_.drawFastHLine(0,31,128,SSD1306_BLACK);
    display_.drawFastHLine(0,31,int(value*128),SSD1306_WHITE);
}
void Screen::face(uint32_t now, pet::Mood mood, uint8_t humor, float progress) {
    faces_.draw(canvas_,now,mood,humor);
    memcpy(display_.getBuffer(),canvas_.bytes.data(),canvas_.bytes.size());
    this->progress(progress);
}
void Screen::message(const char* value, uint32_t age) {
    clear();
    if (!value) value="";
    if (messageText_!=value || messageLayout_.pages.empty()) {
        messageText_=value; messageLayout_=pet::text::wrap(value);
    }
    const auto& page=messageLayout_.at(age);
    int y=page.top();
    for (const auto& line:page.lines) {
        display_.setCursor(pet::text::Page::left(line),y);
        for (unsigned char ch:line) display_.write(ch);
        y+=8;
    }
}
void Screen::weatherReaction(const WeatherData& data, uint32_t now, uint32_t age, uint8_t humor, bool flash) {
    pet::drawWeatherReaction(faces_,canvas_,data,now,age,humor,flash);
    memcpy(display_.getBuffer(),canvas_.bytes.data(),canvas_.bytes.size());
}
void Screen::clock(bool online) {
    clear();
    time_t now = time(nullptr);
    if (now < 1704067200) { text(4,4,"Hora sin sincronizar"); text(4,18,online ? "Esperando internet" : "Configura WiFi"); return; }
    tm local{}; localtime_r(&now,&local);
    char value[24]; strftime(value,sizeof(value),"%H:%M",&local); text(34,0,value,2);
    strftime(value,sizeof(value),"%d/%m/%Y",&local); text(0,24,value);
    text(72,24,online ? "Santiago" : "Sin WiFi");
}
void Screen::icon(int x, int y, unsigned code) {
    const uint16_t white = SSD1306_WHITE;
    if (code == 0) {
        display_.drawCircle(x+12,y+12,6,white);
        for (int a = 0; a < 8; ++a) {
            float angle = a*3.14159265f/4;
            display_.drawLine(x+12+int(cos(angle)*9),y+12+int(sin(angle)*9),x+12+int(cos(angle)*12),y+12+int(sin(angle)*12),white);
        }
        return;
    }
    display_.drawCircle(x+8,y+12,5,white); display_.drawCircle(x+15,y+9,7,white);
    display_.drawCircle(x+23,y+13,5,white); display_.drawFastHLine(x+7,y+18,18,white);
    if (code >= 51) {
        if (code >= 95) {
            display_.drawLine(x+16,y+19,x+12,y+22,white); display_.drawLine(x+12,y+22,x+17,y+21,white); display_.drawLine(x+17,y+21,x+14,y+24,white);
        } else for (int i = 0; i < 3; ++i) display_.drawLine(x+8+i*7,y+20,x+6+i*7,y+23,white);
    }
}
void Screen::weather(const WeatherData& w, unsigned page, uint32_t now) {
    clear();
    if (!w.valid) { text(2,1,"Clima: Puente Alto"); text(2,13,w.online ? "Actualizando..." : "Configura WiFi"); text(2,24,"Open-Meteo.com"); return; }
    const bool stale = !weatherFresh(w,now);
    char s[32];
    if (page%3 == 0) {
        icon(0,0,w.code);
        snprintf(s,sizeof(s),"%.0f",w.temperature); text(38,0,s,2);
        int after = 38+strlen(s)*12;
        display_.drawCircle(after+2,2,2,SSD1306_WHITE); text(after+7,0,"C",2);
        text(35,17,weatherLabel(w.code));
        text(0,25,stale ? "Datos anteriores" : "Puente Alto");
        if (!stale) { snprintf(s,sizeof(s),"%u%%",w.days[0].rain); text(98,25,s); }
    } else if (page%3 == 1) {
        snprintf(s,sizeof(s),"Sensacion: %.0f C",w.feels); text(0,0,s);
        snprintf(s,sizeof(s),"Humedad: %u%%",w.humidity); text(0,8,s);
        snprintf(s,sizeof(s),"Viento: %.0f km/h",w.wind); text(0,16,s);
        snprintf(s,sizeof(s),"Lluvia: %u%% %s",w.days[0].rain,stale ? "(antes)" : ""); text(0,24,s);
    } else {
        for (unsigned i = 0; i < 3; ++i) {
            int x = i*43;
            snprintf(s,sizeof(s),"%.2s/%.2s",w.days[i].date+8,w.days[i].date+5); text(x,0,s);
            snprintf(s,sizeof(s),"%.0f C",w.days[i].max); text(x,9,s);
            snprintf(s,sizeof(s),"%.0f C",w.days[i].min); text(x,17,s);
        }
        text(0,25,stale ? "Datos anteriores" : "Max / Min - 3 dias");
    }
}
void Screen::menu(const pet::Menu& m, bool validDistance, float cm) {
    clear(); char s[24];
    snprintf(s,sizeof(s),"AJUSTES %02u/%02u%s",m.item+1,m.count,m.editing ? " *" : ""); text(0,0,s);
    text(0,8,m.label()); m.value(s,sizeof(s)); text(0,16,s);
    if (m.editing && (m.item==8 || m.item==9 || m.item==11)) {
        if (validDistance) snprintf(s,sizeof(s),"%.0fcm C:+ L:listo",cm);
        else snprintf(s,sizeof(s),"Sin eco C:+ L:listo");
        text(0,24,s);
    } else text(0,24,m.editing ? "C:cambia L:listo" : "C:sig L:elige 3s:sal");
}
void Screen::portal(const char* name, const char* password, bool online) {
    clear(); text(0,0,name);
    char s[24]; snprintf(s,sizeof(s),"Clave: %s",password); text(0,8,s);
    text(0,16,"Abre 192.168.4.1"); text(0,24,online ? "Conectado! C:cerrar" : "C:cerrar (5 min max)");
}
void Screen::status(const char* top, const char* bottom) {
    clear(); text(0,3,top); text(0,18,bottom);
}
