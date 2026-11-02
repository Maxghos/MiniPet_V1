#include "Agenda.h"
#include <algorithm>
#include <cstdio>
#include <set>

namespace pet::agenda {
static unsigned monthDays(unsigned year,unsigned month) {
    static constexpr unsigned days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    return month==2?28+unsigned(year%4==0&&(year%100!=0||year%400==0)):days[month-1];
}
bool validDay(uint32_t day) {
    const unsigned y=day/10000,m=day/100%100,d=day%100;
    return y>=2024&&y<=2100&&m>=1&&m<=12&&d>=1&&d<=monthDays(y,m);
}
uint32_t tomorrow(uint32_t date) {
    if(!validDay(date))return 0;
    unsigned y=date/10000,m=date/100%100,d=date%100;
    if(++d>monthDays(y,m)){d=1;if(++m>12){m=1;++y;}}
    return validDay(y*10000+m*100+d)?y*10000+m*100+d:0;
}
bool Moment::valid() const { return validDay(day)&&minute<1440&&epoch>=1704067200; }
bool validTitle(const std::string& title) {
    if(title.empty()||title.size()>96)return false;
    bool content=false;
    for(unsigned char c:title){if(c<32||c==127)return false;if(c!=' ')content=true;}
    return content;
}
Stats& Book::stats(uint32_t day) {
    for(auto& s:days_)if(s.day==day)return s;
    days_.push_back(Stats{day});
    std::sort(days_.begin(),days_.end(),[](const Stats& a,const Stats& b){return a.day<b.day;});
    if(days_.size()>maxDays){if(days_.front().day==day)days_.pop_back();else days_.erase(days_.begin());}
    for(auto& s:days_)if(s.day==day)return s;
    return days_.back(); // only callers with the current, valid day create entries
}
void Book::observe(const Totals& totals,const Moment& now) {
    const auto focus=totals.focusMs>=last_.focusMs?totals.focusMs-last_.focusMs:totals.focusMs;
    const auto rest=totals.breakMs>=last_.breakMs?totals.breakMs-last_.breakMs:totals.breakMs;
    const auto blocks=totals.blocks>=last_.blocks?totals.blocks-last_.blocks:totals.blocks;
    last_=totals;
    if(!now.valid()||(!focus&&!rest&&!blocks))return;
    auto& s=stats(now.day);
    s.focusMs=std::min<uint64_t>(86400000,s.focusMs+focus);s.breakMs=std::min<uint64_t>(86400000,s.breakMs+rest);
    s.blocks=std::min<uint32_t>(2000,s.blocks+blocks);
}
std::string Book::summary(uint32_t day) const {
    Stats s{day};for(const auto& value:days_)if(value.day==day)s=value;
    unsigned pending=0;for(const auto& t:tasks_)if(!t.done&&t.day<=day)++pending;
    char message[256];
    snprintf(message,sizeof(message),"Resumen %02u/%02u\nEstudio: %llu min\nDescanso: %llu min\nBloques: %u\nActividades completadas: %u\nPendientes: %u",
        unsigned(day%100),unsigned(day/100%100),static_cast<unsigned long long>(s.focusMs/60000),
        static_cast<unsigned long long>(s.breakMs/60000),unsigned(s.blocks),unsigned(s.tasks),pending);
    return message;
}
std::vector<Notice> Book::due(const Moment& now,bool quiet) {
    std::vector<Notice> out;if(!now.valid())return out;
    for(auto& r:reminders_){
        if(r.enabled&&!r.daily&&now.day>r.day){r.enabled=false;r.lastDay=now.day;++revision_;continue;}
        if(!r.enabled||now.day<r.day||r.lastDay>=now.day||now.minute<r.minute)continue;
        r.lastDay=now.day;if(!r.daily)r.enabled=false;++revision_;
        const bool fresh=(r.daily||r.day==now.day)&&now.minute-r.minute<=15;
        if(fresh&&!quiet)out.push_back({"Recordatorio: "+r.title,false});
    }
    if(options_.dailySummary&&summaryDay_<now.day&&now.minute>=options_.summaryMinute){
        summaryDay_=now.day;++revision_;
        if(!quiet&&now.minute-options_.summaryMinute<=60)out.push_back({summary(now.day),true});
    }
    return out;
}
void Book::view(JsonObject out,const Moment& now) const {
    out["clockReady"]=now.valid();out["day"]=now.valid()?now.day:0;out["minute"]=now.minute;out["revision"]=revision_;
    auto options=out["options"].to<JsonObject>();options["dailySummary"]=options_.dailySummary;
    options["summaryMinute"]=options_.summaryMinute;options["scenes"]=options_.scenes;
    auto tasks=out["tasks"].to<JsonArray>();
    for(const auto& t:tasks_){auto v=tasks.add<JsonObject>();v["id"]=t.id;v["day"]=t.day;v["title"]=t.title;v["done"]=t.done;v["doneDay"]=t.doneDay;}
    auto reminders=out["reminders"].to<JsonArray>();
    for(const auto& r:reminders_){auto v=reminders.add<JsonObject>();v["id"]=r.id;v["day"]=r.day;v["minute"]=r.minute;
        v["title"]=r.title;v["daily"]=r.daily;v["enabled"]=r.enabled;v["lastDay"]=r.lastDay;}
    auto days=out["days"].to<JsonArray>();
    for(const auto& s:days_){auto v=days.add<JsonObject>();v["day"]=s.day;v["focusMs"]=s.focusMs;v["breakMs"]=s.breakMs;v["blocks"]=s.blocks;v["tasks"]=s.tasks;}
    out["summary"]=now.valid()?summary(now.day):"Conecta WiFi para sincronizar la hora de Santiago.";
}
bool Book::command(JsonVariantConst req,JsonObject reply,const Moment& now) {
    reply["ok"]=true;
    auto fail=[&](const char* text){reply["ok"]=false;reply["error"]=text;};
    const std::string op=req["op"]|"";const uint32_t id=req["id"]|uint32_t(0);
    auto task=std::find_if(tasks_.begin(),tasks_.end(),[&](const Task& t){return t.id==id;});
    auto reminder=std::find_if(reminders_.begin(),reminders_.end(),[&](const Reminder& r){return r.id==id;});
    if(op=="agenda.get"){}
    else if(op=="agenda.options"){
        auto next=options_;
        if(!req["dailySummary"].isNull()){if(!req["dailySummary"].is<bool>())fail("Opcion invalida");else next.dailySummary=req["dailySummary"].as<bool>();}
        if(!req["scenes"].isNull()){if(!req["scenes"].is<bool>())fail("Opcion invalida");else next.scenes=req["scenes"].as<bool>();}
        if(!req["summaryMinute"].isNull()){if(!req["summaryMinute"].is<unsigned>()||req["summaryMinute"].as<unsigned>()>=1440)fail("Hora invalida");else next.summaryMinute=req["summaryMinute"].as<unsigned>();}
        if(reply["ok"].as<bool>()&&(options_.dailySummary!=next.dailySummary||options_.scenes!=next.scenes||options_.summaryMinute!=next.summaryMinute)){options_=next;++revision_;}
    }else if(op=="task.delete"){
        if(task!=tasks_.end()){tasks_.erase(task);++revision_;}
    }else if(op=="reminder.delete"){
        if(reminder!=reminders_.end()){reminders_.erase(reminder);++revision_;}
    }else if(!now.valid())fail("Sincroniza la hora por WiFi antes de cambiar la agenda.");
    else if(op=="task.add"){
        const std::string title=req["title"]|"";const uint32_t day=req["day"]|now.day;
        const auto duplicate=std::find_if(tasks_.begin(),tasks_.end(),[&](const Task& t){return t.title==title&&t.day==day;});
        if(!validTitle(title)||!validDay(day)||(!req["day"].isNull()&&!req["day"].is<uint32_t>()))fail("Revisa titulo y fecha.");
        else if(duplicate!=tasks_.end())reply["id"]=duplicate->id;
        else if(tasks_.size()>=maxTasks||nextId_==UINT32_MAX)fail("Lista llena (24 actividades). Elimina alguna.");
        else{const auto newId=nextId_++;tasks_.push_back({newId,day,0,title,false});reply["id"]=newId;++revision_;}
    }else if(op=="task.done"){
        if(task==tasks_.end()||!req["done"].is<bool>())fail("Actividad no encontrada.");
        else if(task->done!=req["done"].as<bool>()){
            if(task->done){for(auto& s:days_)if(s.day==task->doneDay&&s.tasks)--s.tasks;task->doneDay=0;}
            else{++stats(now.day).tasks;task->doneDay=now.day;}
            task->done=req["done"].as<bool>();++revision_;
        }
    }else if(op=="task.today"){
        if(task==tasks_.end()||task->done)fail("Actividad pendiente no encontrada.");
        else if(task->day!=now.day){task->day=now.day;++revision_;}
    }else if(op=="reminder.add"){
        const std::string title=req["title"]|"";
        const bool daily=req["daily"]|false;uint32_t day=req["day"]|uint32_t(0);
        const unsigned minute=req["minute"]|unsigned(1440);
        if(!day)day=now.minute>=minute?tomorrow(now.day):now.day;
        const auto duplicate=std::find_if(reminders_.begin(),reminders_.end(),[&](const Reminder& r){return r.enabled&&r.title==title&&r.day==day&&r.minute==minute&&r.daily==daily;});
        if(!validTitle(title)||minute>=1440||!validDay(day)||day<now.day||(day==now.day&&minute<=now.minute)||
            (!req["day"].isNull()&&!req["day"].is<uint32_t>())||
            (!req["daily"].isNull()&&!req["daily"].is<bool>()))fail("Revisa titulo, fecha y hora futura.");
        else if(duplicate!=reminders_.end())reply["id"]=duplicate->id;
        else if(reminders_.size()>=maxReminders||nextId_==UINT32_MAX)fail("Limite de 12 recordatorios.");
        else{const auto newId=nextId_++;reminders_.push_back({newId,day,0,minute,title,daily,true});reply["id"]=newId;++revision_;}
    }else fail("Operacion de agenda desconocida.");
    view(reply["agenda"].to<JsonObject>(),now);return reply["ok"].as<bool>();
}
void Book::store(JsonObject out) const {
    out["version"]=1;out["nextId"]=nextId_;out["summaryDay"]=summaryDay_;view(out["value"].to<JsonObject>(),Moment{});
}
bool Book::load(JsonVariantConst input) {
    if((input["version"]|0)!=1||!input["nextId"].is<uint32_t>()||!input["summaryDay"].is<uint32_t>())return false;
    Book next;next.nextId_=input["nextId"].as<uint32_t>();next.summaryDay_=input["summaryDay"].as<uint32_t>();
    if(!next.nextId_||(next.summaryDay_&&!validDay(next.summaryDay_)))return false;
    auto v=input["value"];auto opts=v["options"];
    if(!opts["dailySummary"].is<bool>()||!opts["scenes"].is<bool>()||!opts["summaryMinute"].is<unsigned>()||opts["summaryMinute"].as<unsigned>()>=1440)return false;
    next.options_={opts["dailySummary"].as<bool>(),opts["scenes"].as<bool>(),opts["summaryMinute"].as<unsigned>()};
    if(!v["tasks"].is<JsonArrayConst>()||!v["reminders"].is<JsonArrayConst>()||!v["days"].is<JsonArrayConst>()||
        v["tasks"].size()>maxTasks||v["reminders"].size()>maxReminders||v["days"].size()>maxDays)return false;
    std::set<uint32_t> ids,dates;
    for(auto x:v["tasks"].as<JsonArrayConst>()){
        if(!x["id"].is<uint32_t>()||!x["day"].is<uint32_t>()||!x["doneDay"].is<uint32_t>()||!x["done"].is<bool>()||!x["title"].is<const char*>())return false;
        Task t{x["id"],x["day"],x["doneDay"],x["title"].as<std::string>(),x["done"]};
        if(!t.id||t.id>=next.nextId_||!ids.insert(t.id).second||!validDay(t.day)||!validTitle(t.title)||
            (t.done?!validDay(t.doneDay):t.doneDay!=0))return false;
        next.tasks_.push_back(t);
    }
    for(auto x:v["reminders"].as<JsonArrayConst>()){
        if(!x["id"].is<uint32_t>()||!x["day"].is<uint32_t>()||!x["lastDay"].is<uint32_t>()||!x["minute"].is<unsigned>()||
            !x["daily"].is<bool>()||!x["enabled"].is<bool>()||!x["title"].is<const char*>())return false;
        Reminder r{x["id"],x["day"],x["lastDay"],x["minute"],x["title"].as<std::string>(),x["daily"],x["enabled"]};
        if(!r.id||r.id>=next.nextId_||!ids.insert(r.id).second||!validDay(r.day)||(r.lastDay&&!validDay(r.lastDay))||r.minute>=1440||!validTitle(r.title))return false;
        next.reminders_.push_back(r);
    }
    for(auto x:v["days"].as<JsonArrayConst>()){
        if(!x["day"].is<uint32_t>()||!x["focusMs"].is<uint64_t>()||!x["breakMs"].is<uint64_t>()||!x["blocks"].is<uint32_t>()||!x["tasks"].is<uint32_t>())return false;
        Stats s{x["day"],x["blocks"],x["tasks"],x["focusMs"],x["breakMs"]};
        if(!validDay(s.day)||!dates.insert(s.day).second||s.focusMs>86400000||s.breakMs>86400000||s.blocks>2000||s.tasks>100000)return false;
        next.days_.push_back(s);
    }
    *this=std::move(next);return true;
}
}
