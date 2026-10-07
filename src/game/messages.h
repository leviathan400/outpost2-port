#pragma once

#include <cstdint>
#include <string>

namespace op2 {

// Mensagens do jogo — porta sub_438F70 (objeto de log @dword_54F0B0):
//   - id de som: 0 → som padrão 0x5B; 0x5E..0xE3 (vozes) passam pelo filtro
//     anti-repetição sub_439490 (tempo REAL, ms) e, se barradas, a mensagem
//     nem é registrada; os demais só tocam o som. (Áudio ainda não portado:
//     o id fica registrado.)
//   - log circular de 64 entradas {x, y, "<N>%i: %s" com tick/100, som}.
//   - texto vai p/ a barra de status (sub_49BF10: 80 chars, MAIÚSCULAS).
// x/y em pixels de mapa (-1 = sem posição).
void postMessage(int x, int y, const char* text, int soundId);

struct GameMessage { int x = -1, y = -1, soundId = 0; std::string text; };
int messageCount();
const GameMessage& messageAt(int i);        // 0 = mais antiga
const std::string& statusBarText();
void resetMessages();

} // namespace op2
