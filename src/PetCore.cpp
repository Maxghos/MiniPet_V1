#include "PetCore.h"
#include <cstdio>

namespace pet {
void Settings::sanitize() {
    modules &= 7;
    if (!modules) modules = Assistant;
    humor = std::min<uint8_t>(humor, 100);
    brightness = std::max<uint8_t>(brightness, 10);
    messages = !!messages;
    quietEnabled = !!quietEnabled;
    quietStart %= 24; quietEnd %= 24;
    reminderSpeed = std::min<uint8_t>(reminderSpeed, 2);
    weatherView = std::min<uint8_t>(weatherView, 3);
    randomPhrases = !!randomPhrases;
    weatherReactions = !!weatherReactions;
    weatherNotices = !!weatherNotices;
    stormFlash = !!stormFlash;
    gestureMinCm = std::clamp<uint16_t>(gestureMinCm, 10, 200);
    gestureMaxCm = std::clamp<uint16_t>(gestureMaxCm, gestureMinCm + 10, 300);
    gestureMs = std::clamp<uint16_t>(gestureMs, 2000, 10000);
    presenceCm = std::clamp<uint16_t>(presenceCm, 30, 350);
    absenceSeconds = std::clamp<uint16_t>(absenceSeconds, 30, 600);
    sleepMinutes = std::clamp<uint16_t>(sleepMinutes, 5, 60);
    pageSeconds = std::clamp<uint16_t>(pageSeconds, 3, 15);
}
bool Settings::quiet(int hour) const {
    if (!quietEnabled || hour < 0 || hour > 23) return false;
    if (quietStart == quietEnd) return true;
    return quietStart < quietEnd ? hour >= quietStart && hour < quietEnd
                                 : hour >= quietStart || hour < quietEnd;
}
Press TouchButton::update(bool active, Tick now) {
    if (active != raw_) { raw_ = active; rawSince_ = now; }
    if (raw_ == stable_ || elapsed(now, rawSince_) < 35) return Press::None;
    stable_ = raw_;
    if (stable_) { downSince_ = now; return Press::None; }
    const Tick held = elapsed(now, downSince_);
    if (held >= 2400) return Press::Back;
    if (held >= 700) return Press::Long;
    return Press::Short;
}
void HoldGesture::reset(Tick now) {
    armed_ = holding_ = outside_ = false;
    outsideSince_ = holdSince_ = now;
    sampled_ = false;
}
bool HoldGesture::update(Tick now, bool valid, float cm, const Settings& s) {
    if (sampled_ && elapsed(now,lastSample_) > 350) holding_ = outside_ = false;
    lastSample_ = now; sampled_ = true;
    const bool inside = valid && cm >= s.gestureMinCm && cm <= s.gestureMaxCm;
    if (!inside) {
        holding_ = false;
        if (!outside_) { outside_ = true; outsideSince_ = now; }
        if (elapsed(now, outsideSince_) >= 1000) armed_ = true;
        return false;
    }
    outside_ = false;
    if (!armed_) return false;
    if (!holding_ || std::fabs(cm - anchor_) > 10) {
        holding_ = true; holdSince_ = now; anchor_ = cm;
    }
    if (elapsed(now, holdSince_) < s.gestureMs) return false;
    armed_ = holding_ = false;
    return true;
}
float HoldGesture::progress(Tick now, const Settings& s) const {
    return holding_ ? std::min(1.0f, float(elapsed(now, holdSince_)) / s.gestureMs) : 0;
}
bool DistanceFilter::sample(float cm, Tick now) {
    if (!std::isfinite(cm) || cm < 2 || cm > 400) return false;
    if (count_ && elapsed(now, lastValid_) > 350) count_ = index_ = 0;
    values_[index_] = cm;
    index_ = (index_ + 1) % values_.size();
    count_ = std::min<unsigned>(count_ + 1, values_.size());
    auto sorted = values_;
    std::sort(sorted.begin(), sorted.begin() + count_);
    filtered_ = sorted[count_ / 2];
    lastValid_ = now;
    return true;
}
bool Presence::update(Tick now, bool valid, float cm, uint16_t range) {
    // Hysteresis plus 600 ms entry / 2 s exit also tolerates missed echoes.
    const bool target = valid && cm < range + (present_ ? 8 : 0);
    if (target == present_) { tracking_ = false; return false; }
    if (!tracking_ || candidate_ != target) {
        candidate_ = target; candidateSince_ = now; tracking_ = true;
    }
    if (elapsed(now, candidateSince_) < (target ? 600u : 2000u)) return false;
    present_ = target; tracking_ = false;
    return true;
}
void Query::start(Tick now, const Settings& s, bool joke, bool reaction) {
    count_ = index_ = 0;
    if (s.modules & Weather) {
        pages_[count_++] = View::Weather;
        if (reaction && s.weatherReactions && (s.modules & Assistant)) pages_[count_++] = View::WeatherReaction;
    }
    if (s.modules & Clock) pages_[count_++] = View::Clock;
    duration_ = Tick(s.pageSeconds) * 1000;
    joke_ = joke && (s.modules & Assistant);
    active_ = count_ > 0;
    since_ = now;
}
bool Query::update(Tick now) {
    if (!active_) return false;
    const Tick duration = joke_ ? 1600u : pages_[index_]==View::WeatherReaction ? 2000u : duration_;
    if (elapsed(now, since_) < duration) return false;
    since_ = now;
    if (joke_) joke_ = false;
    else if (++index_ >= count_) active_ = false;
    return true;
}
View Query::view() const { return active_ && !joke_ ? pages_[index_] : View::Pet; }
Tick Query::totalDuration() const {
    Tick total=joke_ ? 1600 : 0;
    for (unsigned i=0;i<count_;++i) total+=pages_[i]==View::WeatherReaction ? 2000 : duration_;
    return total;
}

void Personality::begin(Tick now) {
    lastCompany_ = lastInteraction_ = now;
    lastMotion_ = now - 10000;
}
void Personality::react(Mood m, Tick now, Tick duration) {
    reaction_ = m; reactionAt_ = now; reactionDuration_ = duration;
}
void Personality::interact(Tick now, const Settings& s) {
    lastInteraction_ = now;
    reminders_ = 0;
    react(s.humor >= 70 ? Mood::Excited : Mood::Curious, now, 2500);
}
Mood Personality::mood(Tick now) const {
    return reactionDuration_ && elapsed(now, reactionAt_) < reactionDuration_ ? reaction_ : base_;
}
void Personality::update(Tick now, bool present, bool motion, const Settings& s) {
    const Tick aloneBefore = elapsed(now, lastCompany_);
    if (present) {
        if (!wasPresent_) {
            if (aloneBefore >= 10000) react(s.humor >= 50 ? Mood::Happy : Mood::Curious, now, 3500);
            reminders_ = 0;
        }
        lastCompany_ = now;
    }
    wasPresent_ = present;
    if (motion && elapsed(now, lastMotion_) >= 10000) {
        lastMotion_ = now;
        react(Mood::Attentive, now, 4500);
    }
    const Tick alone = elapsed(now, lastCompany_);
    const Tick sleep = Tick(s.sleepMinutes) * 60000;
    base_ = alone >= sleep ? Mood::Sleeping : alone >= sleep / 2 ? Mood::Tired : Mood::Calm;
}
bool Personality::attentionDue(Tick now, bool present, const Settings& s) const {
    return elapsed(now,present ? lastInteraction_ : lastCompany_)>=Tick(s.absenceSeconds)*1000;
}
void Personality::remind(Tick now, const Settings& s) {
    ++reminders_;
    Mood expression=Mood::Curious;
    if (s.humor>=70 && reminders_>1) expression=reminders_%2 ? Mood::Annoyed : Mood::Pout;
    react(expression,now,5000);
}

static const char* labels[Menu::count] = {
    "Asistente", "Reloj", "Clima", "Humor", "Mensajes", "Frecuencia",
    "Espera inicial", "Dormir tras", "Gesto desde", "Gesto hasta",
    "Mantener gesto", "Presencia hasta", "Tiempo pantalla", "Brillo",
    "No molestar", "Desde hora", "Hasta hora", "Vista clima",
    "Frases random", "Reaccion al clima", "Avisos del clima", "Destello tormenta", "Vincular app", "Configurar WiFi",
    "Restablecer", "Guardar y salir"
};
const char* Menu::label() const { return labels[item % count]; }
void Menu::value(char* out, unsigned size) const {
    switch (item) {
    case 0: case 1: case 2: std::snprintf(out, size, "%s", draft.modules & (1 << item) ? "SI" : "NO"); break;
    case 3: std::snprintf(out, size, "%u%%", draft.humor); break;
    case 4: std::snprintf(out, size, "%s", draft.messages ? "SI" : "NO"); break;
    case 5: std::snprintf(out, size, "%s", draft.reminderSpeed == 0 ? "Pausada" : draft.reminderSpeed == 1 ? "Normal" : "Frecuente"); break;
    case 6: std::snprintf(out, size, "%u s", draft.absenceSeconds); break;
    case 7: std::snprintf(out, size, "%u min", draft.sleepMinutes); break;
    case 8: std::snprintf(out, size, "%u cm", draft.gestureMinCm); break;
    case 9: std::snprintf(out, size, "%u cm", draft.gestureMaxCm); break;
    case 10: std::snprintf(out, size, "%u s", draft.gestureMs / 1000); break;
    case 11: std::snprintf(out, size, "%u cm", draft.presenceCm); break;
    case 12: std::snprintf(out, size, "%u s", draft.pageSeconds); break;
    case 13: std::snprintf(out, size, "%u", draft.brightness); break;
    case 14: std::snprintf(out, size, "%s", draft.quietEnabled ? "Horario" : "Desactivado"); break;
    case 15: std::snprintf(out, size, "%02u:00", draft.quietStart); break;
    case 16: std::snprintf(out, size, "%02u:00", draft.quietEnd); break;
    case 17: {
        static const char* names[] = {"Principal", "Detalles", "Proximos dias", "Alternar"};
        std::snprintf(out, size, "%s", names[draft.weatherView]); break;
    }
    case 18: std::snprintf(out,size,"%s",draft.randomPhrases ? "SI" : "NO"); break;
    case 19: std::snprintf(out,size,"%s",draft.weatherReactions ? "SI" : "NO"); break;
    case 20: std::snprintf(out,size,"%s",draft.weatherNotices ? "SI" : "NO"); break;
    case 21: std::snprintf(out,size,"%s",draft.stormFlash ? "SI" : "NO"); break;
    default: std::snprintf(out, size, "Mantener: elegir");
    }
}
void Menu::change() {
    switch (item) {
    case 0: case 1: case 2: {
        auto mask = uint8_t(draft.modules ^ (1 << item));
        if (mask) draft.modules = mask; // at least one module remains enabled
        break;
    }
    case 3: draft.humor = draft.humor >= 100 ? 0 : std::min(100, draft.humor + 10); break;
    case 4: draft.messages = !draft.messages; break;
    case 5: draft.reminderSpeed = (draft.reminderSpeed + 1) % 3; break;
    case 6: draft.absenceSeconds = draft.absenceSeconds >= 600 ? 30 : draft.absenceSeconds + 30; break;
    case 7: draft.sleepMinutes = draft.sleepMinutes >= 60 ? 5 : draft.sleepMinutes + 5; break;
    case 8: draft.gestureMinCm = draft.gestureMinCm >= 200 ? 10 : draft.gestureMinCm + 10; break;
    case 9: draft.gestureMaxCm = draft.gestureMaxCm >= 300 ? draft.gestureMinCm + 10 : draft.gestureMaxCm + 10; break;
    case 10: draft.gestureMs = draft.gestureMs >= 10000 ? 2000 : draft.gestureMs + 1000; break;
    case 11: draft.presenceCm = draft.presenceCm >= 350 ? 30 : draft.presenceCm + 10; break;
    case 12: draft.pageSeconds = draft.pageSeconds >= 15 ? 3 : draft.pageSeconds + 1; break;
    case 13: draft.brightness = draft.brightness >= 250 ? 10 : draft.brightness + 20; break;
    case 14: draft.quietEnabled = !draft.quietEnabled; break;
    case 15: draft.quietStart = (draft.quietStart + 1) % 24; break;
    case 16: draft.quietEnd = (draft.quietEnd + 1) % 24; break;
    case 17: draft.weatherView = (draft.weatherView + 1) % 4; break;
    case 18: draft.randomPhrases=!draft.randomPhrases; break;
    case 19: draft.weatherReactions=!draft.weatherReactions; break;
    case 20: draft.weatherNotices=!draft.weatherNotices; break;
    case 21: draft.stormFlash=!draft.stormFlash; break;
    }
    draft.sanitize();
}
Menu::Action Menu::input(Press p) {
    if (p == Press::None) return Action::None;
    if (!active) { open(draft); return Action::None; }
    if (p == Press::Back) {
        if (editing) editing = false;
        else { active = false; return Action::Closed; }
    } else if (p == Press::Short) {
        if (editing) change(); else item = (item + 1) % count;
    } else if (p == Press::Long) {
        if (item == saveItem) { draft.sanitize(); active = false; return Action::Save; }
        if (item == resetItem) { draft = Settings{}; item = saveItem; editing = false; }
        else if (item == wifiItem) return Action::Portal;
        else if (item == appItem) return Action::PairApp;
        else editing = !editing;
    }
    return Action::None;
}
}
