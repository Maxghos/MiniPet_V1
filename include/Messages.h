#pragma once
#include <cstddef>
#include <cstdint>

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
        unsigned next = random % count;
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
private:
    const char* recent_[3]{};
    unsigned cursor_ = 0;
};
}
