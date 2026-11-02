#pragma once
#include "StudyTimer.h"
#include <ArduinoJson.h>
#include <string>
#include <vector>

namespace pet::agenda {
struct Moment { uint32_t day=0; unsigned minute=0; int64_t epoch=0; bool valid() const; };
bool validDay(uint32_t day);
uint32_t tomorrow(uint32_t day);
bool validTitle(const std::string& title);
struct Task { uint32_t id=0,day=0,doneDay=0; std::string title; bool done=false; };
struct Reminder { uint32_t id=0,day=0,lastDay=0; unsigned minute=0; std::string title; bool daily=false,enabled=true; };
struct Stats { uint32_t day=0,blocks=0,tasks=0; uint64_t focusMs=0,breakMs=0; };
struct Totals { uint64_t focusMs=0,breakMs=0; uint32_t blocks=0; };
class Accounting {
public:
    void baseline(const study::Snapshot& s){last_={s.focusMs,s.breakMs,s.completedBlocks};}
    void observe(const study::Snapshot& s){
        total_.focusMs+=s.focusMs>=last_.focusMs?s.focusMs-last_.focusMs:s.focusMs;
        total_.breakMs+=s.breakMs>=last_.breakMs?s.breakMs-last_.breakMs:s.breakMs;
        total_.blocks+=s.completedBlocks>=last_.blocks?s.completedBlocks-last_.blocks:s.completedBlocks;
        baseline(s);
    }
    Totals totals() const {return total_;}
private: Totals last_,total_;
};
struct Options { bool dailySummary=true,scenes=true; unsigned summaryMinute=20*60+30; };
struct Notice { std::string text; bool summary=false; };
class Book {
public:
    static constexpr unsigned maxTasks=24,maxReminders=12,maxDays=14;
    void observe(const Totals& totals,const Moment& now);
    std::vector<Notice> due(const Moment& now,bool quiet);
    void view(JsonObject output,const Moment& now) const;
    bool command(JsonVariantConst request,JsonObject response,const Moment& now);
    void store(JsonObject output) const;
    bool load(JsonVariantConst input);
    std::string summary(uint32_t day) const;
    uint32_t revision() const { return revision_; }
    const Options& options() const { return options_; }
private:
    Stats& stats(uint32_t day);
    std::vector<Task> tasks_;
    std::vector<Reminder> reminders_;
    std::vector<Stats> days_;
    Options options_;
    Totals last_;
    uint32_t nextId_=1,revision_=0,summaryDay_=0;
};
}
