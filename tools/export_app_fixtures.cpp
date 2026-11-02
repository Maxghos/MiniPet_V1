#include "AppProtocol.h"
#include "TextLayout.h"
#include "StudyCodec.h"
#include "Agenda.h"
#include "TelegramProtocol.h"
#include <ArduinoJson.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <algorithm>

static void le16(std::vector<uint8_t>& v,unsigned i,uint16_t x){v[i]=x;v[i+1]=x>>8;}
static void le32(std::vector<uint8_t>& v,unsigned i,uint32_t x){le16(v,i,x);le16(v,i+2,x>>16);}
static bool save(const char* name,const std::string& value,bool check){
    const std::string file=std::string("protocol/fixtures/")+name;
    if(check){std::ifstream in(file,std::ios::binary);std::ostringstream current;current<<in.rdbuf();return current.str()==value;}
    std::ofstream out(file,std::ios::binary);out.write(value.data(),value.size());return bool(out);
}
int main(int argc,char** argv){
    const bool check=argc>1&&std::string(argv[1])=="--check";
    std::vector<uint8_t> media(24+1024);
    memcpy(media.data(),"MPET",4);media[4]=1;media[5]=10;le16(media,6,128);le16(media,8,32);le16(media,10,2);le32(media,12,1024);
    for(unsigned frame=0;frame<2;++frame)for(unsigned y=0;y<32;++y)for(unsigned x=0;x<128;++x)
        if((x+y+frame*17)%11==0)media[24+frame*512+x+(y/8)*128]|=1<<(y%8);
    le32(media,16,pet::app::crc32(media.data()+24,1024));
    JsonDocument doc;auto cases=doc.to<JsonArray>();
    const char* values[]={"Hola","¿Qué tal? ¡Sí, ñandú!","Uno\nDos\nTres\nCuatro\nCinco",
        "Mi cama y yo tenemos una relación seria: nos vemos todas las noches, pero por la mañana ella no quiere que me vaya.",
        "Guardo archivos importantes con nombres como “final_final_v3_definitivo”."};
    for(const char* text:values){
        auto item=cases.add<JsonObject>();item["text"]=text;
        auto layout=pet::text::wrap(text);item["duration"]=layout.duration();auto pages=item["pages"].to<JsonArray>();
        for(const auto& page:layout.pages){auto p=pages.add<JsonObject>();p["top"]=page.top();p["duration"]=page.duration();auto lines=p["lines"].to<JsonArray>();
            for(const auto& line:page.lines){auto l=lines.add<JsonArray>();for(unsigned char c:line)l.add(c);}
        }
    }
    std::string json;serializeJsonPretty(doc,json);
    json.erase(std::remove(json.begin(),json.end(),'\r'),json.end());json+='\n';
    JsonDocument study;auto stages=study.to<JsonArray>();pet::study::Timer timer;
    auto capture=[&](uint32_t now){auto stage=stages.add<JsonObject>();stage["atMs"]=now;
        pet::study::writeSnapshot(stage["study"].to<JsonObject>(),timer.snapshot(now));};
    capture(0);
    pet::study::Config config;config.focusMinutes=config.shortBreakMinutes=1;config.longBreakMinutes=2;config.blocksBeforeLong=2;
    timer.configure(config);timer.start(0);capture(0);
    timer.update(59999);capture(59999);timer.update(60000);capture(60000);
    timer.update(120000);capture(120000);timer.resume(180000);timer.pause(190000);capture(190000);
    auto saved=timer.record(190000);timer.restore(saved,200000);capture(200000);
    timer.resume(200000);timer.update(250000);capture(250000);timer.stop(251000);capture(251000);
    std::string studyJson;serializeJsonPretty(study,studyJson);
    studyJson.erase(std::remove(studyJson.begin(),studyJson.end(),'\r'),studyJson.end());studyJson+='\n';
    pet::agenda::Book book;pet::agenda::Moment moment{20261102,600,1793613600};JsonDocument req,response,additional;
    req["op"]="task.add";req["title"]="Repasar algebra";book.command(req.as<JsonVariantConst>(),response.to<JsonObject>(),moment);
    req.clear();req["op"]="reminder.add";req["title"]="Descansar la vista";req["minute"]=620;req["daily"]=true;
    book.command(req.as<JsonVariantConst>(),response.to<JsonObject>(),moment);
    book.observe({1500000,300000,1},moment);book.view(additional["agenda"].to<JsonObject>(),moment);
    additional["agenda"]["persisted"]=true;
    pet::telegram::PublicState tg;tg.configured=tg.linked=tg.ready=true;tg.username="MiniPetDemoBot";tg.code=200;
    pet::telegram::publicState(additional["telegram"].to<JsonObject>(),tg);
    std::string additionalJson;serializeJsonPretty(additional,additionalJson);
    additionalJson.erase(std::remove(additionalJson.begin(),additionalJson.end(),'\r'),additionalJson.end());additionalJson+='\n';
    if(!save("text_layout.json",json,check)||!save("frame_clip.mpet",std::string(reinterpret_cast<char*>(media.data()),media.size()),check)||
        !save("study_cycle.json",studyJson,check)||!save("additional.json",additionalJson,check))return 1;
    std::cout<<"App protocol fixtures "<<(check?"verified":"generated")<<"\n";
}
