#include "StudyTimer.h"
#include <cstdio>

namespace pet::study {
bool Config::valid() const {
    return focusMinutes>=1&&focusMinutes<=180&&shortBreakMinutes>=1&&shortBreakMinutes<=60&&
        longBreakMinutes>=1&&longBreakMinutes<=120&&blocksBeforeLong>=2&&blocksBeforeLong<=12;
}
const char* name(Phase value) {
    switch(value){case Phase::Focus:return "focus";case Phase::ShortBreak:return "shortBreak";
    case Phase::LongBreak:return "longBreak";default:return "idle";}
}
const char* name(State value) {
    switch(value){case State::Running:return "running";case State::Paused:return "paused";
    case State::Waiting:return "waiting";default:return "idle";}
}
const char* name(Event value) {
    switch(value){case Event::Started:return "started";case Event::FocusDone:return "focusDone";
    case Event::BreakDone:return "breakDone";case Event::Paused:return "paused";
    case Event::Resumed:return "resumed";case Event::Skipped:return "skipped";
    case Event::Stopped:return "stopped";case Event::Recovered:return "recovered";default:return "none";}
}
unsigned Snapshot::round() const {
    if(phase==Phase::Focus||!completedBlocks)return completedBlocks%config.blocksBeforeLong+1;
    return (completedBlocks-1)%config.blocksBeforeLong+1;
}
Tick Timer::duration(Phase phase) const {
    return Tick(phase==Phase::Focus?value_.config.focusMinutes:phase==Phase::ShortBreak?
        value_.config.shortBreakMinutes:phase==Phase::LongBreak?value_.config.longBreakMinutes:0)*60000;
}
void Timer::event(Event value,Tick now) {
    value_.event=value;++value_.eventId;++value_.revision;eventAt_=now;
}
void Timer::enter(Phase phase,State state,Tick now,Event cause) {
    value_.phase=phase;value_.state=state;value_.durationMs=value_.remainingMs=duration(phase);
    last_=now;event(cause,now);
}
bool Timer::configure(const Config& config) {
    if(value_.active()||!config.valid())return false;
    value_.config=config;++value_.revision;return true;
}
bool Timer::start(Tick now) {
    if(value_.active())return false;
    value_.completedBlocks=0;value_.focusMs=value_.breakMs=0;
    enter(Phase::Focus,State::Running,now,Event::Started);return true;
}
bool Timer::update(Tick now) {
    if(value_.state!=State::Running)return false;
    const Tick used=std::min(elapsed(now,last_),value_.remainingMs);last_=now;
    if(value_.phase==Phase::Focus)value_.focusMs+=used;else value_.breakMs+=used;
    value_.remainingMs-=used;
    if(value_.remainingMs)return false;
    if(value_.phase==Phase::Focus){
        ++value_.completedBlocks;
        const auto next=value_.completedBlocks%value_.config.blocksBeforeLong==0?Phase::LongBreak:Phase::ShortBreak;
        enter(next,value_.config.autoBreak?State::Running:State::Waiting,now,Event::FocusDone);
    }else enter(Phase::Focus,value_.config.autoFocus?State::Running:State::Waiting,now,Event::BreakDone);
    return true;
}
bool Timer::pause(Tick now) {
    update(now);
    if(value_.state==State::Paused)return true;
    if(value_.state!=State::Running)return false;
    value_.state=State::Paused;event(Event::Paused,now);return true;
}
bool Timer::resume(Tick now) {
    if(value_.state==State::Running)return true;
    if(value_.state!=State::Paused&&value_.state!=State::Waiting)return false;
    value_.state=State::Running;last_=now;event(Event::Resumed,now);return true;
}
bool Timer::skip(Tick now) {
    update(now);if(!value_.active())return false;
    if(value_.phase==Phase::Focus)
        enter(Phase::ShortBreak,value_.config.autoBreak?State::Running:State::Waiting,now,Event::Skipped);
    else enter(Phase::Focus,value_.config.autoFocus?State::Running:State::Waiting,now,Event::Skipped);
    return true;
}
bool Timer::stop(Tick now) {
    update(now);if(!value_.active())return true;
    enter(Phase::Idle,State::Idle,now,Event::Stopped);return true;
}
Snapshot Timer::snapshot(Tick now) const {
    auto result=value_;result.eventAgeMs=elapsed(now,eventAt_);return result;
}
bool Timer::restore(const Record& record,Tick now) {
    const auto& v=record.value;
    constexpr uint64_t maxStats=uint64_t(10)*366*24*60*60*1000;
    if(record.version!=1||!v.config.valid()||unsigned(v.phase)>unsigned(Phase::LongBreak)||
        unsigned(v.state)>unsigned(State::Waiting)||v.completedBlocks>1000000||v.focusMs>maxStats||v.breakMs>maxStats)return false;
    const Tick expected=Tick(v.phase==Phase::Focus?v.config.focusMinutes:v.phase==Phase::ShortBreak?
        v.config.shortBreakMinutes:v.phase==Phase::LongBreak?v.config.longBreakMinutes:0)*60000;
    if(v.phase==Phase::Idle){if(v.state!=State::Idle||v.remainingMs||v.durationMs)return false;}
    else if(v.state==State::Idle||!v.remainingMs||v.remainingMs>expected||v.durationMs!=expected)return false;
    value_=v;last_=now;
    if(value_.state==State::Running)value_.state=State::Paused;
    event(value_.active()?Event::Recovered:Event::None,now);return true;
}
void Menu::open(const Snapshot& value) {
    active=true;editing=confirming=false;item=0;draft=value.config;
}
const char* Menu::label(const Snapshot& s) const {
    if(item==0)return !s.active()?"Empezar estudio":s.state==State::Running?"Pausar":"Continuar";
    static const char* labels[]={"","Saltar etapa","Terminar sesion","Estudio (min)","Descanso corto","Descanso largo",
        "Bloques por ciclo","Descanso automatico","Estudio automatico","Guardar estudio","Volver a ajustes"};
    return labels[item%count];
}
void Menu::value(char* out,unsigned size,const Snapshot& s) const {
    if(confirming){std::snprintf(out,size,"%s",confirmationRevision_==s.revision?"L:confirmar C:cancel":"Etapa cambio: cancelar");return;}
    if(item>=3&&item<=9&&s.active()){std::snprintf(out,size,"Termina para editar");return;}
    switch(item){
    case 3:std::snprintf(out,size,"%u min",draft.focusMinutes);break;
    case 4:std::snprintf(out,size,"%u min",draft.shortBreakMinutes);break;
    case 5:std::snprintf(out,size,"%u min",draft.longBreakMinutes);break;
    case 6:std::snprintf(out,size,"%u bloques",draft.blocksBeforeLong);break;
    case 7:std::snprintf(out,size,"%s",draft.autoBreak?"SI":"NO");break;
    case 8:std::snprintf(out,size,"%s",draft.autoFocus?"SI":"NO");break;
    default:std::snprintf(out,size,"Mantener: elegir");break;
    }
}
Menu::Action Menu::input(Press p,const Snapshot& s) {
    if(p==Press::None)return Action::None;
    if(p==Press::Back){
        if(editing||confirming)editing=confirming=false;else{active=false;return Action::Closed;}
    }else if(p==Press::Short){
        if(confirming){confirming=false;return Action::None;}
        if(!editing)item=(item+1)%count;
        else switch(item){
        case 3:draft.focusMinutes=draft.focusMinutes>=180?1:draft.focusMinutes+1;break;
        case 4:draft.shortBreakMinutes=draft.shortBreakMinutes>=60?1:draft.shortBreakMinutes+1;break;
        case 5:draft.longBreakMinutes=draft.longBreakMinutes>=120?1:draft.longBreakMinutes+1;break;
        case 6:draft.blocksBeforeLong=draft.blocksBeforeLong>=12?2:draft.blocksBeforeLong+1;break;
        case 7:draft.autoBreak=!draft.autoBreak;break;
        case 8:draft.autoFocus=!draft.autoFocus;break;
        default:break;
        }
    }else if(p==Press::Long){
        if(item==0)return !s.active()?Action::Start:s.state==State::Running?Action::Pause:Action::Resume;
        if(item==1||item==2){
            if(!s.active())return Action::None;
            if(!confirming){confirming=true;confirmationRevision_=s.revision;return Action::None;}
            if(confirmationRevision_!=s.revision){confirming=false;return Action::None;}
            confirming=false;return item==1?Action::Skip:Action::Stop;
        }
        if(item==10){active=false;return Action::Closed;}
        if(s.active())return Action::None;
        if(item==9)return Action::Save;
        editing=!editing;
    }
    return Action::None;
}
}
