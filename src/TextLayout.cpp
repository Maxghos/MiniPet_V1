#include "TextLayout.h"
#include <algorithm>

namespace pet::text {
std::string encode(const char* input) {
    std::string out;
    if (!input) return out;
    const auto* p = reinterpret_cast<const unsigned char*>(input);
    while (*p) {
        uint32_t code = *p++;
        if (code >= 0xc2 && code <= 0xf4) {
            unsigned extra = code < 0xe0 ? 1 : code < 0xf0 ? 2 : 3;
            code &= (1u << (6-extra))-1;
            bool valid = true;
            for (unsigned n=0;n<extra;++n) {
                if ((*p&0xc0)!=0x80) { valid=false; break; }
                code=(code<<6)|(*p++&63);
            }
            if (!valid) code='?';
        } else if (code>=0x80) code='?';
        if (code<128) { if (code=='\n' || code>=32) out+=char(code); continue; }
        // Values from the CP437 font shipped by Adafruit GFX (cp437(true)).
        switch (code) {
        case 0xe1: out+=char(160); break; case 0xe9: out+=char(130); break;
        case 0xed: out+=char(161); break; case 0xf3: out+=char(162); break;
        case 0xfa: out+=char(163); break; case 0xf1: out+=char(164); break;
        case 0xd1: out+=char(165); break; case 0xfc: out+=char(129); break;
        case 0xdc: out+=char(154); break; case 0xbf: out+=char(168); break;
        case 0xa1: out+=char(173); break; case 0xb0: out+=char(248); break;
        case 0xc9: out+=char(144); break;
        case 0xc1: out+='A'; break; case 0xcd: out+='I'; break;
        case 0xd3: out+='O'; break; case 0xda: out+='U'; break;
        case 0x201c: case 0x201d: out+='"'; break;
        case 0x2018: case 0x2019: out+='\''; break;
        case 0x2013: case 0x2014: out+='-'; break;
        case 0x2026: out+="..."; break;
        case 0xa0: out+=' '; break;
        default: out+='?';
        }
    }
    return out;
}
uint32_t Page::duration() const {
    size_t length=0; for (const auto& line:lines) length+=line.size();
    return std::clamp<uint32_t>(uint32_t(length)*80,3000,7000);
}
uint32_t Layout::duration() const {
    uint32_t total=0; for (const auto& p:pages) total+=p.duration(); return total;
}
const Page& Layout::at(uint32_t elapsed) const {
    for (const auto& p:pages) { if (elapsed<p.duration()) return p; elapsed-=p.duration(); }
    return pages.back(); // hold last page, never restart a sentence in a loop
}
Layout wrap(const char* input) {
    const auto value=encode(input);
    Layout result;
    std::vector<std::string> lines;
    std::string line;
    auto emit=[&] {
        lines.push_back(line); line.clear();
    };
    size_t offset=0;
    while (offset<value.size()) {
        if (value[offset]=='\n') { emit(); ++offset; continue; }
        if (value[offset]==' ') { ++offset; continue; }
        size_t end=value.find_first_of(" \n",offset);
        if (end==std::string::npos) end=value.size();
        auto word=value.substr(offset,end-offset);
        if (!line.empty() && line.size()+1+word.size()>21) emit();
        if (!line.empty()) line+=' ';
        while (word.size()>21) {
            size_t count=21;
            const auto punctuation=word.find_last_of("_/-.",20);
            if (punctuation!=std::string::npos && punctuation>=6) count=punctuation+1;
            line=word.substr(0,count); emit(); word.erase(0,count);
        }
        line+=word;
        offset=end;
    }
    if (!line.empty()) emit();
    if (lines.empty()) lines.emplace_back();
    // Balance pages: five lines become 3+2, rather than leaving one orphan line.
    const size_t count=(lines.size()+3)/4;
    size_t position=0;
    for (size_t p=0;p<count;++p) {
        result.pages.emplace_back();
        const size_t length=lines.size()/count+(p<lines.size()%count);
        for (size_t n=0;n<length;++n) result.pages.back().lines.push_back(lines[position++]);
    }
    return result;
}
}
