#pragma once
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <cstring>
#include <utility>

struct FakeFS;
class File {
public:
    File()=default;
    File(std::string* data,FakeFS* fs):data_(data),fs_(fs){}
    explicit operator bool() const{return data_!=nullptr;}
    size_t size() const{return data_?data_->size():0;}
    int read(){return data_&&position_<data_->size()?static_cast<unsigned char>((*data_)[position_++]):-1;}
    size_t readBytes(char* dest,size_t size){
        if(!data_)return 0;
        const auto count=std::min(size,data_->size()-position_);memcpy(dest,data_->data()+position_,count);position_+=count;return count;
    }
    size_t write(uint8_t value){return write(&value,1);}
    size_t write(const uint8_t* bytes,size_t size);
    void flush(){}
    void close(){data_=nullptr;}
private:
    std::string* data_=nullptr;FakeFS* fs_=nullptr;size_t position_=0;
};
struct FakeFS {
    std::map<std::string,std::string> files;
    std::set<std::string> directories;
    bool failWrite=false,failRename=false;
    size_t budget=std::numeric_limits<size_t>::max();
    unsigned writeOpens=0,renameCalls=0,failRenameAt=0;
    bool exists(const char* name) const{return files.count(name)!=0||directories.count(name)!=0;}
    bool mkdir(const char* name){return directories.insert(name).second;}
    bool remove(const char* name){return files.erase(name)!=0;}
    File open(const char* name,const char* mode){
        if(mode[0]=='w'){++writeOpens;if(failWrite)return {};files[name].clear();return File(&files[name],this);}
        auto item=files.find(name);return item==files.end()?File{}:File(&item->second,this);
    }
    bool rename(const char* from,const char* to){
        if(++renameCalls==failRenameAt||failRename)return false;
        auto item=files.find(from);if(item==files.end())return false;
        auto data=std::move(item->second);files.erase(item);files[to]=std::move(data);return true;
    }
};
inline size_t File::write(const uint8_t* bytes,size_t size){
    if(!data_)return 0;
    const auto count=std::min(size,fs_->budget);data_->append(reinterpret_cast<const char*>(bytes),count);fs_->budget-=count;return count;
}
inline FakeFS LittleFS;
