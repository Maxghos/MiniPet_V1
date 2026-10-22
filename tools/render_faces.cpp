#include "FaceRenderer.h"
#include <fstream>
#include <iostream>
#include <vector>

static const char* labels[] = {"Tranquilo","Atento","Curioso","Contento","Impactado","Molesto","Cansado","Dormido"};
static std::string base64(const std::vector<uint8_t>& data) {
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
int main() {
    std::ofstream raw("build/faces.raw",std::ios::binary);
    std::ofstream html("docs/caras.html");
    if (!raw || !html) return 1;
    html << R"(<!doctype html><html lang="es"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>MiniPet: caras</title><style>body{font:16px system-ui;background:#121a22;color:#e4f4f2;margin:32px auto;max-width:800px;padding:0 20px}canvas{width:100%;max-width:768px;image-rendering:pixelated;border:2px solid #345;border-radius:8px;background:black}select,button{font:inherit;padding:8px;margin:8px 10px 16px 0}p{line-height:1.6}</style><h1>MiniPet · expresiones</h1><p>Animaciones dibujadas por el mismo motor C++ del firmware. Vista ampliada de una pantalla monocroma de 128 × 32. La cadencia física dependerá de la placa y del bus I²C.</p><label>Expresión <select id="mood">)";
    for (unsigned i=0;i<8;++i) html << "<option value='" << i << "'>" << labels[i] << "</option>";
    html << "</select></label><button id='pause'>Pausar</button><br><canvas id='oled' width='128' height='32'></canvas><p>Incluye parpadeos, movimientos de mirada, alerta con fade y descanso con Zzz. Muestra ilustrativa al 75 % de humor; las reacciones por sensores se ejecutan en la placa.</p><script>const frames=[";
    for (unsigned m=0;m<8;++m) {
        pet::FaceRenderer renderer(1700+m); pet::Canvas c;
        std::vector<uint8_t> frames;
        // Simulate at the firmware's rate, sample 15 fps for a compact preview.
        for (unsigned t=0;t<9000;t+=33) {
            renderer.draw(c,t,pet::Mood(m),75);
            if (t==1980) raw.write(reinterpret_cast<const char*>(c.bytes.data()),512);
            if ((t/33)%2==0) frames.insert(frames.end(),c.bytes.begin(),c.bytes.end());
        }
        if (m) html << ',';
        html << "'" << base64(frames) << "'";
    }
    html << R"(].map(s=>Uint8Array.from(atob(s),c=>c.charCodeAt(0)));let n=0,paused=false;const ctx=document.getElementById('oled').getContext('2d'),image=ctx.createImageData(128,32),select=document.getElementById('mood');select.onchange=()=>n=0;document.getElementById('pause').onclick=e=>{paused=!paused;e.target.textContent=paused?'Reproducir':'Pausar'};function draw(){const data=frames[Number(select.value)],offset=(n%(data.length/512))*512;for(let y=0;y<32;y++)for(let x=0;x<128;x++){const lit=data[offset+x+(y>>3)*128]&(1<<(y&7)),i=(y*128+x)*4;image.data[i]=lit?220:0;image.data[i+1]=lit?255:0;image.data[i+2]=lit?250:0;image.data[i+3]=255}ctx.putImageData(image,0,0);if(!paused)n++}setInterval(draw,66);draw();</script></html>)";
    std::cout << "Generated docs/caras.html and build/faces.raw\n";
}
