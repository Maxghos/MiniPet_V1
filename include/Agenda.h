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
struct Habit { uint32_t id=0;std::string title;unsigned targetPerWeek=7;std::vector<uint32_t> completed;uint32_t totalDone=0,retiredDay=0,retiredStreak=0,celebratedWeek=0; };
struct Routine { uint32_t id=0;std::string title;bool focus=false,overview=false;std::vector<std::string> tasks; };
struct Memory {
    std::string name="MiniPet",nickname;
    bool greetings=true,celebrations=true,stories=true;
    uint32_t firstDay=0,lastSeenDay=0,daysTogether=0,totalBlocks=0,totalTasks=0,totalHabitChecks=0;
    uint32_t greetingDay=0,lastStoryDay=0,storyChapter=0,celebratedMilestone=0;
};
class Book {
public:
    static constexpr unsigned maxTasks=24,maxReminders=12,maxDays=14;
    static constexpr unsigned maxHabits=8,maxRoutines=6,maxHabitDays=56;
    Book();
    void observe(const Totals& totals,const Moment& now);
    std::vector<Notice> due(const Moment& now,bool quiet);
    void view(JsonObject output,const Moment& now) const;
    bool command(JsonVariantConst request,JsonObject response,const Moment& now);
    void store(JsonObject output) const;
    bool load(JsonVariantConst input);
    std::string summary(uint32_t day) const;
    uint32_t revision() const { return revision_; }
    const Options& options() const { return options_; }
    bool companionNotice(std::string& output,const Moment& now,uint8_t humor);
private:
    bool extensionCommand(JsonVariantConst request,JsonObject response,const Moment& now);
    void companionView(JsonObject response,const Moment& now) const;
    void storeExtras(JsonObject output) const;
    bool loadExtras(JsonVariantConst input);
    void touchMemory(const Moment& now);
    Stats& stats(uint32_t day);
    std::vector<Task> tasks_;
    std::vector<Reminder> reminders_;
    std::vector<Stats> days_;
    Options options_;
    Totals last_;
    uint32_t nextId_=1,revision_=0,summaryDay_=0;
    std::vector<Habit> habits_;
    std::vector<Routine> routines_;
    Memory memory_;
    std::vector<uint32_t> countedTasks_;
    uint32_t nextHabitId_=1,nextRoutineId_=3;
};
}
