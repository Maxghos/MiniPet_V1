#include "Agenda.h"
#include <algorithm>
#include <cstdio>
#include <set>

namespace pet::agenda {
Book::Book(){
    routines_.push_back({1,"Estudiar",true,false,{}});
    routines_.push_back({2,"Comenzar el dia",false,true,{}});
}
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
    touchMemory(now);
    const auto focus=totals.focusMs>=last_.focusMs?totals.focusMs-last_.focusMs:totals.focusMs;
    const auto rest=totals.breakMs>=last_.breakMs?totals.breakMs-last_.breakMs:totals.breakMs;
    const auto blocks=totals.blocks>=last_.blocks?totals.blocks-last_.blocks:totals.blocks;
    last_=totals;
    if(!now.valid()||(!focus&&!rest&&!blocks))return;
    auto& s=stats(now.day);
    s.focusMs=std::min<uint64_t>(86400000,s.focusMs+focus);s.breakMs=std::min<uint64_t>(86400000,s.breakMs+rest);
    s.blocks=std::min<uint32_t>(2000,s.blocks+blocks);
    if(blocks){memory_.totalBlocks=std::min<uint32_t>(10000000,memory_.totalBlocks+std::min<uint32_t>(blocks,2000));++revision_;}
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
    if(extensionCommand(req,reply,now)){view(reply["agenda"].to<JsonObject>(),now);return reply["ok"].as<bool>();}
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
        if(task!=tasks_.end()){tasks_.erase(task);countedTasks_.erase(std::remove(countedTasks_.begin(),countedTasks_.end(),id),countedTasks_.end());++revision_;}
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
            if(task->done){
                for(auto& s:days_)if(s.day==task->doneDay&&s.tasks)--s.tasks;
                task->doneDay=0;
                auto counted=std::find(countedTasks_.begin(),countedTasks_.end(),id);
                if(counted!=countedTasks_.end()){countedTasks_.erase(counted);if(memory_.totalTasks)--memory_.totalTasks;}
            }else{++stats(now.day).tasks;task->doneDay=now.day;countedTasks_.push_back(id);if(memory_.totalTasks<10000000)++memory_.totalTasks;}
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
    storeExtras(out["extras"].to<JsonObject>());
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
    if(!input["extras"].isNull()){
        if(!next.loadExtras(input["extras"]))return false;
    }else{
        for(const auto& task:next.tasks_)if(task.done){++next.memory_.totalTasks;next.countedTasks_.push_back(task.id);}
        for(const auto& stats:next.days_)next.memory_.totalBlocks+=stats.blocks;
    }
    *this=std::move(next);return true;
}
namespace {
uint32_t ordinal(uint32_t day){
    const unsigned year=day/10000,month=day/100%100,previous=year-1;
    static constexpr unsigned before[]={0,31,59,90,120,151,181,212,243,273,304,334};
    return previous*365+previous/4-previous/100+previous/400+before[month-1]+day%100+
        unsigned(month>2&&year%4==0&&(year%100!=0||year%400==0));
}
uint32_t weekStart(uint32_t day){const auto value=ordinal(day);return value-(value-1)%7;}
unsigned weekly(const Habit& habit,uint32_t day){
    unsigned count=0;const auto start=weekStart(day),end=ordinal(day);
    for(auto done:habit.completed){const auto value=ordinal(done);if(value>=start&&value<=end)++count;}
    return count;
}
unsigned streak(const Habit& habit,uint32_t day){
    unsigned count=0;uint32_t wanted=ordinal(day);
    if(!std::binary_search(habit.completed.begin(),habit.completed.end(),day))--wanted;
    for(auto it=habit.completed.rbegin();it!=habit.completed.rend();++it){
        const auto value=ordinal(*it);if(value>wanted)continue;if(value!=wanted)break;++count;--wanted;
    }
    if(habit.retiredDay&&ordinal(habit.retiredDay)==wanted)count+=habit.retiredStreak;
    return count;
}
bool shortName(const std::string& value,bool empty=false){return (empty&&value.empty())||(value.size()<=24&&validTitle(value));}
void writeRoutine(JsonObject out,const Routine& routine){
    out["id"]=routine.id;out["title"]=routine.title;out["focus"]=routine.focus;out["overview"]=routine.overview;
    auto tasks=out["tasks"].to<JsonArray>();for(const auto& title:routine.tasks)tasks.add(title);
}
bool readRoutine(JsonVariantConst value,Routine& routine){
    if(!value["title"].is<const char*>()||!validTitle(value["title"].as<std::string>())||
        !value["focus"].is<bool>()||!value["overview"].is<bool>()||!value["tasks"].is<JsonArrayConst>()||value["tasks"].size()>6)return false;
    routine.title=value["title"].as<std::string>();routine.focus=value["focus"];routine.overview=value["overview"];
    if(routine.focus&&routine.overview)return false;
    std::set<std::string> titles;
    for(auto title:value["tasks"].as<JsonArrayConst>()){
        if(!title.is<const char*>()||!validTitle(title.as<std::string>())||!titles.insert(title.as<std::string>()).second)return false;
        routine.tasks.push_back(title.as<std::string>());
    }
    return true;
}
}
void Book::touchMemory(const Moment& now){
    if(!now.valid()||memory_.lastSeenDay>=now.day)return;
    if(!memory_.firstDay)memory_.firstDay=now.day;
    memory_.lastSeenDay=now.day;if(memory_.daysTogether<1000000)++memory_.daysTogether;++revision_;
}
void Book::companionView(JsonObject reply,const Moment& now) const {
    auto out=reply["companion"].to<JsonObject>();out["name"]=memory_.name;out["nickname"]=memory_.nickname;
    out["greetings"]=memory_.greetings;out["celebrations"]=memory_.celebrations;out["stories"]=memory_.stories;
    out["clockReady"]=now.valid();out["day"]=now.valid()?now.day:0;out["firstDay"]=memory_.firstDay;
    out["daysTogether"]=memory_.daysTogether;out["totalBlocks"]=memory_.totalBlocks;out["totalTasks"]=memory_.totalTasks;
    out["totalHabitChecks"]=memory_.totalHabitChecks;out["lastStoryDay"]=memory_.lastStoryDay;out["storyChapter"]=memory_.storyChapter;
    auto habits=reply["habits"].to<JsonArray>();
    for(const auto& habit:habits_){
        auto h=habits.add<JsonObject>();h["id"]=habit.id;h["title"]=habit.title;h["targetPerWeek"]=habit.targetPerWeek;
        h["doneToday"]=now.valid()&&std::binary_search(habit.completed.begin(),habit.completed.end(),now.day);
        const auto done=now.valid()?weekly(habit,now.day):0;
        h["weekDone"]=done;h["goalReached"]=done>=habit.targetPerWeek;h["streak"]=now.valid()?streak(habit,now.day):0;h["totalDone"]=habit.totalDone;
    }
    auto routines=reply["routines"].to<JsonArray>();for(const auto& routine:routines_)writeRoutine(routines.add<JsonObject>(),routine);
}
bool Book::extensionCommand(JsonVariantConst req,JsonObject reply,const Moment& now){
    const std::string op=req["op"]|"";
    if(op.rfind("habit.",0)!=0&&op.rfind("companion.",0)!=0&&op.rfind("routine.",0)!=0)return false;
    auto fail=[&](const char* error){reply["ok"]=false;reply["error"]=error;};
    const uint32_t id=req["id"]|uint32_t(0);
    auto habit=std::find_if(habits_.begin(),habits_.end(),[&](const Habit& h){return h.id==id;});
    auto routine=std::find_if(routines_.begin(),routines_.end(),[&](const Routine& r){return r.id==id;});
    if(op=="companion.get"||op=="routine.list"){}
    else if(op=="companion.options"){
        auto next=memory_;
        if(!req["name"].isNull()){
            if(!req["name"].is<const char*>()||!shortName(req["name"].as<std::string>()))fail("Nombre: entre 1 y 24 bytes UTF-8.");
            else next.name=req["name"].as<std::string>();
        }
        if(!req["nickname"].isNull()){
            if(!req["nickname"].is<const char*>()||!shortName(req["nickname"].as<std::string>(),true))fail("Apodo: hasta 24 bytes UTF-8.");
            else next.nickname=req["nickname"].as<std::string>();
        }
        for(const char* key:{"greetings","celebrations","stories"})if(!req[key].isNull()){
            if(!req[key].is<bool>())fail("Opcion de memoria invalida.");
            else if(std::string(key)=="greetings")next.greetings=req[key];
            else if(std::string(key)=="celebrations")next.celebrations=req[key];
            else next.stories=req[key];
        }
        if(reply["ok"].as<bool>()&&(next.name!=memory_.name||next.nickname!=memory_.nickname||next.greetings!=memory_.greetings||next.celebrations!=memory_.celebrations||next.stories!=memory_.stories)){memory_=next;++revision_;}
    }else if(op=="habit.add"){
        const std::string title=req["title"]|"";const unsigned target=req["targetPerWeek"]|7u;
        auto duplicate=std::find_if(habits_.begin(),habits_.end(),[&](const Habit& h){return h.title==title;});
        if(!validTitle(title)||(!req["targetPerWeek"].isNull()&&!req["targetPerWeek"].is<unsigned>())||target<1||target>7)fail("Revisa el titulo y la meta de 1 a 7 dias.");
        else if(duplicate!=habits_.end())reply["id"]=duplicate->id;
        else if(habits_.size()>=maxHabits||nextHabitId_==UINT32_MAX)fail("Limite de ocho habitos.");
        else{Habit h;h.id=nextHabitId_++;h.title=title;h.targetPerWeek=target;reply["id"]=h.id;habits_.push_back(h);++revision_;}
    }else if(op=="habit.delete"){
        if(habit!=habits_.end()){habits_.erase(habit);++revision_;}
    }else if(op=="habit.done"){
        if(!now.valid())fail("Sincroniza la hora de Santiago antes de marcar un habito.");
        else if(habit==habits_.end()||!req["done"].is<bool>())fail("Habito o estado invalido.");
        else if(habit->retiredDay&&now.day<=habit->retiredDay)fail("La fecha retrocedio fuera del historial del habito.");
        else{
            const auto day=std::lower_bound(habit->completed.begin(),habit->completed.end(),now.day);
            const bool done=day!=habit->completed.end()&&*day==now.day;
            if(done!=req["done"].as<bool>()){
                if(done){habit->completed.erase(day);if(habit->totalDone)--habit->totalDone;if(memory_.totalHabitChecks)--memory_.totalHabitChecks;}
                else{habit->completed.insert(day,now.day);if(habit->totalDone<10000000)++habit->totalDone;if(memory_.totalHabitChecks<10000000)++memory_.totalHabitChecks;}
                while(habit->completed.size()>maxHabitDays){
                    const auto old=habit->completed.front();
                    habit->retiredStreak=habit->retiredDay&&ordinal(old)==ordinal(habit->retiredDay)+1?habit->retiredStreak+1:1;
                    habit->retiredDay=old;habit->completed.erase(habit->completed.begin());
                }
                ++revision_;
            }
        }
    }else if(op=="routine.save"){
        Routine next;next.id=id;
        if(!readRoutine(req,next)||(!req["id"].isNull()&&!req["id"].is<uint32_t>()))fail("Rutina invalida: titulo, foco, resumen y hasta seis actividades distintas.");
        else if(id&&routine==routines_.end())fail("Rutina no encontrada.");
        else if(!id&&(routines_.size()>=maxRoutines||nextRoutineId_==UINT32_MAX))fail("Limite de seis rutinas.");
        else{
            if(!id){next.id=nextRoutineId_++;routines_.push_back(next);++revision_;}
            else if(routine->title!=next.title||routine->focus!=next.focus||routine->overview!=next.overview||routine->tasks!=next.tasks){*routine=next;++revision_;}
            reply["id"]=next.id;
        }
    }else if(op=="routine.delete"){
        if(routine!=routines_.end()){routines_.erase(routine);++revision_;}
    }else if(op=="routine.plan"||op=="routine.prepare"){
        if(routine==routines_.end())fail("Rutina no encontrada.");
        else if(op=="routine.prepare"&&!now.valid())fail("Sincroniza la hora de Santiago antes de ejecutar una rutina.");
        else{
            std::vector<std::string> missing;
            if(op=="routine.prepare")for(const auto& title:routine->tasks){
                const auto found=std::find_if(tasks_.begin(),tasks_.end(),[&](const Task& t){return t.day==now.day&&t.title==title;});
                if(found==tasks_.end())missing.push_back(title);
            }
            if(tasks_.size()+missing.size()>maxTasks||missing.size()>uint64_t(UINT32_MAX)-nextId_)fail("No hay espacio para todas las actividades de la rutina. Libera espacio primero.");
            else{
                for(const auto& title:missing){tasks_.push_back({nextId_++,now.day,0,title,false});++revision_;}
                writeRoutine(reply["plan"].to<JsonObject>(),*routine);
            }
        }
    }else fail("Funcion de compania desconocida.");
    companionView(reply,now);return true;
}
bool Book::companionNotice(std::string& out,const Moment& now,uint8_t humor){
    if(!now.valid()||!humor)return false;
    touchMemory(now);
    if(memory_.greetings&&memory_.greetingDay<now.day&&now.minute>=360&&now.minute<1380){
        out=now.minute<720?"Buenos dias":now.minute<1200?"Buenas tardes":"Buenas noches";
        if(!memory_.nickname.empty())out+=", "+memory_.nickname;
        out+=". Soy "+memory_.name+". Vamos a nuestro ritmo.";memory_.greetingDay=now.day;++revision_;return true;
    }
    if(memory_.celebrations){
        for(auto& habit:habits_)if(weekly(habit,now.day)>=habit.targetPerWeek&&habit.celebratedWeek<weekStart(now.day)){
            out="Meta semanal lista: "+habit.title+". Bien ahi!";habit.celebratedWeek=weekStart(now.day);++revision_;return true;
        }
        const uint32_t score=(memory_.totalBlocks+memory_.totalTasks+memory_.totalHabitChecks)/5;
        if(score>memory_.celebratedMilestone){
            out="Ya juntamos "+std::to_string(score*5)+" avances. De a poquito tambien se llega lejos.";
            memory_.celebratedMilestone=score;++revision_;return true;
        }
    }
    if(memory_.stories&&humor>=40&&now.minute>=720&&memory_.lastStoryDay<now.day){
        static const char* chapters[]={
            "Bitacora 1/6: encontre una semilla pixel. La guarde para manana.",
            "Bitacora 2/6: la semilla saco una hojita. Creo que le gusta estudiar contigo.",
            "Bitacora 3/6: aparecio una luciernaga. Ahora somos tres en el escritorio.",
            "Bitacora 4/6: la luciernaga dibujo un mapa. Lo exploraremos sin apuro.",
            "Bitacora 5/6: el mapa llevaba a una estrella dormida. La dejamos descansar.",
            "Bitacora 6/6: la estrella desperto y dejo otra semilla. Nuestra aventura sigue."
        };
        out=chapters[memory_.storyChapter];memory_.storyChapter=(memory_.storyChapter+1)%6;memory_.lastStoryDay=now.day;++revision_;return true;
    }
    return false;
}
void Book::storeExtras(JsonObject out) const {
    out["version"]=1;out["nextHabitId"]=nextHabitId_;out["nextRoutineId"]=nextRoutineId_;
    auto m=out["memory"].to<JsonObject>();
#define MEM(field) m[#field]=memory_.field;
    MEM(name) MEM(nickname) MEM(greetings) MEM(celebrations) MEM(stories) MEM(firstDay) MEM(lastSeenDay) MEM(daysTogether)
    MEM(totalBlocks) MEM(totalTasks) MEM(totalHabitChecks) MEM(greetingDay) MEM(lastStoryDay) MEM(storyChapter) MEM(celebratedMilestone)
#undef MEM
    auto counted=out["countedTasks"].to<JsonArray>();for(auto id:countedTasks_)counted.add(id);
    auto habits=out["habits"].to<JsonArray>();
    for(const auto& h:habits_){
        auto v=habits.add<JsonObject>();v["id"]=h.id;v["title"]=h.title;v["targetPerWeek"]=h.targetPerWeek;v["totalDone"]=h.totalDone;
        v["retiredDay"]=h.retiredDay;v["retiredStreak"]=h.retiredStreak;v["celebratedWeek"]=h.celebratedWeek;
        auto done=v["completed"].to<JsonArray>();for(auto day:h.completed)done.add(day);
    }
    auto routines=out["routines"].to<JsonArray>();for(const auto& r:routines_)writeRoutine(routines.add<JsonObject>(),r);
}
bool Book::loadExtras(JsonVariantConst input){
    if((input["version"]|0)!=1||!input["nextHabitId"].is<uint32_t>()||!input["nextRoutineId"].is<uint32_t>())return false;
    nextHabitId_=input["nextHabitId"];nextRoutineId_=input["nextRoutineId"];if(!nextHabitId_||!nextRoutineId_)return false;
    auto m=input["memory"];
    if(!m["name"].is<const char*>()||!m["nickname"].is<const char*>()||!shortName(m["name"].as<std::string>())||!shortName(m["nickname"].as<std::string>(),true))return false;
    memory_.name=m["name"].as<std::string>();memory_.nickname=m["nickname"].as<std::string>();
#define BOOL(field) if(!m[#field].is<bool>())return false;memory_.field=m[#field];
    BOOL(greetings) BOOL(celebrations) BOOL(stories)
#undef BOOL
#define DATE(field) if(!m[#field].is<uint32_t>())return false;memory_.field=m[#field];if(memory_.field&&!validDay(memory_.field))return false;
    DATE(firstDay) DATE(lastSeenDay) DATE(greetingDay) DATE(lastStoryDay)
#undef DATE
#define COUNT(field,limit) if(!m[#field].is<uint32_t>()||m[#field].as<uint32_t>()>limit)return false;memory_.field=m[#field];
    COUNT(daysTogether,1000000) COUNT(totalBlocks,10000000) COUNT(totalTasks,10000000) COUNT(totalHabitChecks,10000000)
    COUNT(storyChapter,5) COUNT(celebratedMilestone,6000000)
#undef COUNT
    if((memory_.firstDay==0)!=(memory_.lastSeenDay==0)||memory_.firstDay>memory_.lastSeenDay||
        (memory_.firstDay==0)!=(memory_.daysTogether==0))return false;
    if(!input["habits"].is<JsonArrayConst>()||input["habits"].size()>maxHabits||!input["routines"].is<JsonArrayConst>()||input["routines"].size()>maxRoutines||
        !input["countedTasks"].is<JsonArrayConst>()||input["countedTasks"].size()>maxTasks)return false;
    std::set<uint32_t> habitIds,routineIds,countedIds;
    for(auto value:input["countedTasks"].as<JsonArrayConst>()){
        if(!value.is<uint32_t>())return false;
        const uint32_t id=value;
        auto task=std::find_if(tasks_.begin(),tasks_.end(),[&](const Task& t){return t.id==id&&t.done;});
        if(task==tasks_.end()||!countedIds.insert(id).second)return false;
        countedTasks_.push_back(id);
    }
    if(countedTasks_.size()>memory_.totalTasks)return false;
    for(auto value:input["habits"].as<JsonArrayConst>()){
        if(!value["id"].is<uint32_t>()||!value["title"].is<const char*>()||!value["targetPerWeek"].is<unsigned>()||
            !value["totalDone"].is<uint32_t>()||!value["retiredDay"].is<uint32_t>()||!value["retiredStreak"].is<uint32_t>()||!value["celebratedWeek"].is<uint32_t>()||
            !value["completed"].is<JsonArrayConst>()||value["completed"].size()>maxHabitDays)return false;
        Habit h;h.id=value["id"];h.title=value["title"].as<std::string>();h.targetPerWeek=value["targetPerWeek"];h.totalDone=value["totalDone"];
        h.retiredDay=value["retiredDay"];h.retiredStreak=value["retiredStreak"];h.celebratedWeek=value["celebratedWeek"];
        if(!h.id||h.id>=nextHabitId_||!habitIds.insert(h.id).second||!validTitle(h.title)||h.targetPerWeek<1||h.targetPerWeek>7||h.totalDone>10000000||
            (h.retiredDay&&!validDay(h.retiredDay))||h.retiredStreak>h.totalDone||(h.retiredDay==0)!=(h.retiredStreak==0)||h.celebratedWeek>ordinal(21001231))return false;
        uint32_t previous=h.retiredDay;
        for(auto day:value["completed"].as<JsonArrayConst>()){
            if(!day.is<uint32_t>()||!validDay(day.as<uint32_t>())||day.as<uint32_t>()<=previous)return false;
            previous=day.as<uint32_t>();h.completed.push_back(previous);
        }
        if(h.totalDone<h.completed.size())return false;
        habits_.push_back(h);
    }
    routines_.clear();
    for(auto value:input["routines"].as<JsonArrayConst>()){
        Routine r;if(!value["id"].is<uint32_t>()||!readRoutine(value,r))return false;r.id=value["id"];
        if(!r.id||r.id>=nextRoutineId_||!routineIds.insert(r.id).second)return false;
        routines_.push_back(r);
    }
    return true;
}
}
