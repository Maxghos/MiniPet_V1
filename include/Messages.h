#pragma once
#include <cstddef>
#include <cstdint>
#include "RandomPhrases.h"
#include "WeatherBehavior.h"

// Adapted from Esperar_1.txt. Short ASCII phrases for the built-in 6x8 font.
namespace messages {
inline constexpr const char* waiting[] = {
    "Estas ahi?", "Sigues ahi?", "Todo bien?", "Estas por ahi?",
    "Sigo esperando?", "Te demoras mucho?", "Me dejas esperando?",
    "No me olvides, porfa", "Te perdiste?", "Estas cerca?",
    "Sigo aqui, eh?", "Estas dormido?", "Sigues despierto?",
    "Te puedo molestar un ratito?", "Tienes un minuto?", "Te espero un poco mas?",
    "Ya estas listo?", "Me avisas cuando puedas?", "Sigo en espera?", "Ya me toca?"
};
inline constexpr const char* company[] = {
    "Estas ocupado?", "Es mal momento?", "Tienes un minuto?", "Puedo molestar un ratito?",
    "Todo en orden?", "Como vamos?", "Me pescas un poquito?", "Te espero, tranquilo"
};
inline constexpr const char* jokes[] = {
    "Y mi hola?", "Ahora soy termometro", "Ya, ya... ahi va", "Primero saludame, po",
    "Mucho clima, poco carino", "Servicio de bolsillo"
};
class Picker {
public:
    const char* pick(uint32_t random, bool present, bool joke = false) {
        const char* const* bank = joke ? jokes : present ? company : waiting;
        size_t count = joke ? sizeof(jokes)/sizeof(*jokes) : present ? sizeof(company)/sizeof(*company) : sizeof(waiting)/sizeof(*waiting);
        return from(bank,count,random);
    }
    const char* random(uint32_t value) { return from(randomPhrases,sizeof(randomPhrases)/sizeof(*randomPhrases),value); }
    const char* weather(const WeatherData& data, uint32_t value) {
        static constexpr const char* hot[]={"Hoy hasta yo sudo", "Ahora soy mascota al vapor", "¿Quién prendió el horno?", "Busco sombra, gracias"};
        static constexpr const char* cold[]={"Se me congelan los píxeles", "¿Una mantita para mí?", "Hoy abrígate, po", "Frío hasta en los circuitos"};
        static constexpr const char* clear[]={"Buen día para brillar", "Solcito, por fin", "Hoy salimos a cargar ánimo", "El cielo anda de buen humor"};
        static constexpr const char* cloud[]={"El cielo también tiene sueño", "Hoy las nubes hacen turno", "Un día con filtro gris", "Modo nublado activado"};
        static constexpr const char* rain[]={"Hoy toca paraguas", "Yo no soy impermeable, eh", "Parece día de gotitas", "Agüita afuera, café adentro"};
        static constexpr const char* storm[]={"El cielo se puso dramático", "Ese trueno no fui yo", "Tormenta: ojos bien abiertos", "Hoy el cielo está enojado"};
        static constexpr const char* snow[]={"Modo copito de nieve", "Hoy el cielo se viste blanco", "¿Hacemos un muñeco?", "Mis píxeles piden bufanda"};
        static constexpr const char* fog[]={"¿Quién desenfocó el cielo?", "Hoy toca mirar con calma", "La nube bajó a saludar", "Veo poco, opino igual"};
        const auto expression=pet::weatherExpression(data);
        const char* const* bank=nullptr;
        switch (expression.condition) {
        case pet::Condition::Storm: bank=storm; break;
        case pet::Condition::Rain: case pet::Condition::Drizzle: bank=rain; break;
        case pet::Condition::Snow: bank=snow; break;
        default:
            if (expression.hot) bank=hot;
            else if (expression.cold) bank=cold;
            else if (expression.condition==pet::Condition::Fog) bank=fog;
            else if (expression.condition==pet::Condition::Cloud) bank=cloud;
            else if (expression.condition==pet::Condition::Clear) bank=clear;
        }
        return bank ? from(bank,4,value) : pick(value,true,true);
    }
private:
    const char* from(const char* const* bank, size_t count, uint32_t value) {
        unsigned next = value % count;
        // Avoid the last three messages in this bank, without rejection loops.
        for (unsigned tries = 0; tries < count; ++tries) {
            bool seen = false;
            for (auto old : recent_) if (old == bank[next]) seen = true;
            if (!seen) break;
            next = (next + 1) % count;
        }
        recent_[cursor_++ % 3] = bank[next];
        return bank[next];
    }
    const char* recent_[3]{};
    unsigned cursor_ = 0;
};
}
