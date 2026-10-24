#include "FaceRenderer.h"
#include "WeatherEffects.h"
#include "TextLayout.h"
#include "Messages.h"
#include <fstream>
#include <iostream>
#include <vector>

namespace {
struct Sequence {
    const char* title;
    pet::Mood mood;
    pet::FaceStyle style=pet::FaceStyle::Count;
    int weatherCode=-1;
    float temperature=20;
};
std::string base64(const std::vector<uint8_t>& data) {
    static const char* alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result; unsigned accumulator=0; int bits=-6;
    for (uint8_t byte:data) {
        accumulator=(accumulator<<8)|byte; bits+=8;
        while (bits>=0) { result+=alphabet[(accumulator>>bits)&63]; bits-=6; }
    }
    if (bits>-6) result+=alphabet[((accumulator<<8)>>(bits+8))&63];
    while (result.size()%4) result+='=';
    return result;
}
std::vector<uint8_t> compress(const std::vector<uint8_t>& frames) {
    std::vector<uint8_t> delta(frames.size()), packed;
    for (size_t i=0;i<frames.size();++i) delta[i]=frames[i]^(i>=512 ? frames[i-512] : 0);
    for (size_t i=0;i<delta.size();) {
        size_t end=i+1;
        while (end<delta.size() && delta[end]==delta[i] && end-i<255) ++end;
        packed.push_back(uint8_t(end-i)); packed.push_back(delta[i]); i=end;
    }
    return packed;
}
void layouts() {
    std::ofstream out("build/layout_cases.json");
    const char* examples[]={"Hola", "¿Todo bien?", messages::randomPhrases[0],messages::randomPhrases[27]};
    out<<'[';
    for (unsigned i=0;i<4;++i) {
        if (i) out<<',';
        out<<'[';
        auto layout=pet::text::wrap(examples[i]);
        for (unsigned p=0;p<layout.pages.size();++p) {
            if (p) out<<',';
            out<<'['; int y=layout.pages[p].top();
            for (unsigned l=0;l<layout.pages[p].lines.size();++l) {
                if (l) out<<',';
                const auto& line=layout.pages[p].lines[l];
                out<<"{\"x\":"<<pet::text::Page::left(line)<<",\"y\":"<<y<<",\"bytes\":[";
                for (unsigned n=0;n<line.size();++n) { if (n) out<<','; out<<unsigned(uint8_t(line[n])); }
                out<<"]}"; y+=8;
            }
            out<<']';
        }
        out<<']';
    }
    out<<']';
}
}
int main() {
    std::ofstream raw("build/faces.raw",std::ios::binary),styles("build/styles.raw",std::ios::binary),weatherRaw("build/weather.raw",std::ios::binary);
    std::ofstream html("docs/caras.html");
    if (!raw || !styles || !html || !weatherRaw) return 1;
    const char* moods[]={"Tranquilo","Atento","Curioso","Contento","Impactado","Molesto","Cansado","Dormido","Ironico","Ofendido","Entusiasmado"};
    std::vector<Sequence> sequences;
    for (unsigned m=0;m<unsigned(pet::Mood::Count);++m) sequences.push_back({moods[m],pet::Mood(m)});
    for (unsigned s=0;s<unsigned(pet::FaceStyle::Count);++s) sequences.push_back({pet::faceStyleLabel(pet::FaceStyle(s)),pet::Mood::Calm,pet::FaceStyle(s)});
    sequences.push_back({"Calor y sudor",pet::Mood::Tired,pet::FaceStyle::Count,0,33});
    sequences.push_back({"Sol agradable",pet::Mood::Happy,pet::FaceStyle::Count,0,22});
    sequences.push_back({"Nublado",pet::Mood::Tired,pet::FaceStyle::Count,3,17});
    sequences.push_back({"Niebla",pet::Mood::Tired,pet::FaceStyle::Count,45,9});
    sequences.push_back({"Lluvia",pet::Mood::Curious,pet::FaceStyle::Count,61,14});
    sequences.push_back({"Tormenta",pet::Mood::Shocked,pet::FaceStyle::Count,95,14});
    sequences.push_back({"Frio",pet::Mood::Pout,pet::FaceStyle::Count,0,3});
    sequences.push_back({"Nieve y frio",pet::Mood::Curious,pet::FaceStyle::Count,73,1});
    html<<R"(<!doctype html><html lang="es"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>MiniPet V2: caras</title><style>body{font:16px system-ui;background:#121a22;color:#e4f4f2;margin:32px auto;max-width:800px;padding:0 20px}canvas{width:100%;max-width:768px;image-rendering:pixelated;border:2px solid #345;border-radius:8px;background:black}select,button{font:inherit;padding:8px;margin:8px 10px 16px 0;max-width:100%}p{line-height:1.6}</style><h1>MiniPet V2 · expresiones</h1><p>11 estados, 19 estilos de ojos y 8 ejemplos de clima. Dibujados por el mismo motor C++ del firmware en 128 × 32 píxeles. Las animaciones se muestran ampliadas y muestreadas a unos 15 fps.</p><label>Ver <select id="mood">)";
    for (unsigned i=0;i<sequences.size();++i) {
        if (i==0) html<<"<optgroup label='Estados'>";
        if (i==unsigned(pet::Mood::Count)) html<<"</optgroup><optgroup label='Estilos individuales'>";
        if (i==unsigned(pet::Mood::Count)+unsigned(pet::FaceStyle::Count)) html<<"</optgroup><optgroup label='Reacciones al clima'>";
        html<<"<option value='"<<i<<"'>"<<sequences[i].title<<"</option>";
    }
    html<<"</optgroup></select></label><button id='pause'>Pausar</button><br><canvas id='oled' width='128' height='32'></canvas><p>El clima usa datos ilustrativos; no realiza peticiones. El destello de tormenta está desactivado por defecto en la placa. El porcentaje de humor regula la frecuencia y expresividad, con el mismo objetivo de refresco de ~30 fps.</p><script>const packed=[";
    for (unsigned index=0;index<sequences.size();++index) {
        const auto& sequence=sequences[index];
        pet::FaceRenderer renderer(1700+index); pet::Canvas c;
        std::vector<uint8_t> frames;
        const unsigned end=sequence.weatherCode<0 ? 6000 : 3000;
        WeatherData weather; weather.code=sequence.weatherCode; weather.temperature=sequence.temperature;
        for (unsigned t=0;t<end;t+=33) {
            if (sequence.weatherCode<0) renderer.draw(c,t,sequence.mood,75,sequence.style);
            else pet::drawWeatherReaction(renderer,c,weather,t,t,75,false);
            if (t==1980) {
                auto& output=sequence.weatherCode>=0 ? weatherRaw : sequence.style!=pet::FaceStyle::Count ? styles : raw;
                output.write(reinterpret_cast<const char*>(c.bytes.data()),512);
            }
            if ((t/33)%2==0) frames.insert(frames.end(),c.bytes.begin(),c.bytes.end());
        }
        if (index) html<<',';
        html<<"'"<<base64(compress(frames))<<"'";
    }
    html<<R"(];const frames=packed.map(s=>{const bytes=Uint8Array.from(atob(s),c=>c.charCodeAt(0));let length=0;for(let i=0;i<bytes.length;i+=2)length+=bytes[i];if(length%512)throw Error('Datos inválidos');const data=new Uint8Array(length);let cursor=0;for(let i=0;i<bytes.length;i+=2){data.fill(bytes[i+1],cursor,cursor+bytes[i]);cursor+=bytes[i]}for(let i=512;i<data.length;i++)data[i]^=data[i-512];return data});let n=0,paused=false;const ctx=document.getElementById('oled').getContext('2d'),image=ctx.createImageData(128,32),select=document.getElementById('mood');select.onchange=()=>n=0;document.getElementById('pause').onclick=e=>{paused=!paused;e.target.textContent=paused?'Reproducir':'Pausar'};function draw(){const data=frames[Number(select.value)],offset=(n%(data.length/512))*512;for(let y=0;y<32;y++)for(let x=0;x<128;x++){const lit=data[offset+x+(y>>3)*128]&(1<<(y&7)),i=(y*128+x)*4;image.data[i]=lit?220:0;image.data[i+1]=lit?255:0;image.data[i+2]=lit?250:0;image.data[i+3]=255}ctx.putImageData(image,0,0);if(!paused)n++}setInterval(draw,66);draw();</script></html>)";
    layouts();
    std::cout<<"Preview: "<<sequences.size()<<" sequences, native framebuffers and text layouts generated\n";
}
