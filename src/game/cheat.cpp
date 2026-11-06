#include "cheat.h"

#include <cstring>
#include <vector>

#include "building_sheet.h"
#include "colony.h"
#include "player.h"

namespace op2 {

namespace {

uint32_t rol32(uint32_t v, int n) {
    n &= 31;
    return n ? ((v << n) | (v >> (32 - n))) : v;
}
uint32_t ror32(uint32_t v, int n) {
    n &= 31;
    return n ? ((v >> n) | (v << (32 - n))) : v;
}

// ---------------------------------------------------------------------------
// Os quatro efeitos. `p` é o jogador que digitou.

// sub_49E030 — "Dan's dog could make more ore.": os dois estoques vão para a
// capacidade (dword_56E2FC = dword_56E2F4, dword_56E300 = dword_56E2F8).
void cheatMaxMetals(int p) {
    g_playerData[p].ore     = g_colony[p].oreCapacity;     // +0x20 = +0x18
    g_playerData[p].rareOre = g_colony[p].rareCapacity;    // +0x24 = +0x1C
    colonyMarkDirty(p);
}

// sub_49E070 — "Dan's car could hold more people.": cientistas (+0x98) e
// trabalhadores (+0x94) ganham 25%. O original faz `v + v/4` com a divisão
// truncada para zero (cdq / and edx,3 / sar 2), que é o que `v / 4` dá em C++.
void cheatMorePeople(int p) {
    auto mais25 = [](int v) { return v + v / 4; };
    g_playerData[p].scientists = mais25(g_playerData[p].scientists);
    g_playerData[p].workers    = mais25(g_playerData[p].workers);
    colonyMarkDirty(p);
}

// sub_49E0D0 — "Dan's bike could backfire more often": a recarga (bloco +0x30)
// das DEZESSEIS armas 0x3B..0x4A vira min(recarga − 2, 20). Menos recarga =
// atira mais rápido, e repetir o cheat continua baixando.
//
// sub_49E120 — a quarta frase: a penetração (bloco +0x2C) das CATORZE armas
// 0x3D..0x4A vira max(pen × 33/32, 500) — o piso de 500 é o salto grande, e
// cada repetição soma ~3%. Note que esta pula a 0x3B e a EMP (0x3C), que não
// fazem dano.
//
// As duas escrevem no mesmo bloco que as melhorias de pesquisa usam
// (`unitStatBlock`), então entram no jogo salvo e `reloadSheets()` as desfaz no
// começo da partida seguinte — igual ao original. As duas também põem
// dword_4E0BD0 = 1; esse sinalizador tem UM leitor no original (sub_431EB0,
// um caminho de criação de unidade tipo 4 que não identifiquei), e por isso
// aqui não há nada para acionar — ver PORT_STATUS.
void escreveBloco(int type, int p, int off, int valor) {
    if (uint8_t* b = unitStatBlock(type, p)) std::memcpy(b + off, &valor, 4);
}
int leBloco(int type, int p, int off) {
    int v = 0;
    if (const uint8_t* b = unitStatBlock(type, p)) std::memcpy(&v, b + off, 4);
    return v;
}

void cheatFasterFire(int p) {
    for (int t = 0x3B; t <= 0x4A; ++t) {                  // off_4E142C..off_4E146C
        const int v = leBloco(t, p, 0x30) - 2;
        escreveBloco(t, p, 0x30, v < 20 ? v : 20);
    }
}

void cheatStrongerWeapons(int p) {
    for (int t = 0x3D; t <= 0x4A; ++t) {                  // off_4E1434..off_4E146C
        const int v = leBloco(t, p, 0x2C);
        const int n = (v * 33) / 32;                      // shl 5 / add / sar 5
        escreveBloco(t, p, 0x2C, n > 500 ? n : 500);
    }
}

using Handler = void (*)(int);

struct Entrada { uint32_t hash; Handler efeito; const char* nota; };

// A tabela do original (VA 0x5863B8) tem CINCO dwords e os handlers vêm de
// sub_58650C. A quinta entrada é zero e o `jcxz` depois do `repne scasd` a
// torna inalcançável de propósito: é o "nenhum casou" do laço.
//
// A quarta frase do original nunca foi publicada — só o hash 0xABE0CC11 está
// no executável, e tentar adivinhá-la por força bruta não deu em nada. Ela fica
// aqui: se alguém descobrir a frase, ela funciona. A entrada seguinte é um
// EXTRA DO PORTE (o original não a tem), para que o quarto efeito tenha uma
// frase utilizável.
const Entrada kTabela[] = {
    { 0xFF27BC04u, &cheatMaxMetals,        "Dan's dog could make more ore." },
    { 0x48D60158u, &cheatMorePeople,       "Dan's car could hold more people." },
    { 0xAC0FF0E5u, &cheatFasterFire,       "Dan's bike could backfire more often" },
    { 0xABE0CC11u, &cheatStrongerWeapons,  "(a frase original, desconhecida)" },
    { 0x9AC1C27Eu, &cheatStrongerWeapons,  "Dan's gun could die more people. (extra do porte)" },
};

} // namespace

// sub_58642A. O laço anda em grupos de 4 bytes; por grupo o hash é rodado por
// `cl` (o nibble alto do byte baixo do grupo ANTERIOR) e XORado com esse grupo
// anterior, uma vez por byte, parando no primeiro zero. Fechado o grupo, o
// byte baixo do hash leva um XOR com o 4º byte.
//
// Detalhe que importa: ao achar o zero a função devolve na hora, então os bytes
// depois do fim da frase nunca entram na conta — o hash depende só do texto.
uint32_t cheatHash(const char* text) {
    const size_t n = std::strlen(text);
    std::vector<uint8_t> buf(((n / 4) + 2) * 4, 0);        // zerado, como o pacote
    std::memcpy(buf.data(), text, n);
    auto dwordEm = [&](size_t i) {
        uint32_t v = 0;
        std::memcpy(&v, buf.data() + i, 4);
        return v;
    };

    uint32_t ebx = 0;
    size_t pos = 0;
    uint32_t edx = dwordEm(pos);
    uint32_t edi = edx;
    for (;;) {
        const int cl = static_cast<int>((edi & 0xFF) >> 4);
        int ch = 4;
        uint32_t eax = rol32(edx, 8);
        uint32_t al = 0;
        bool achouZero = false;
        for (;;) {
            ebx = rol32(ebx, cl) ^ edi;
            eax = ror32(eax, 8);
            al = eax & 0xFF;
            if (al == 0) { achouZero = true; break; }      // or al,al / jz
            if (--ch == 0) break;
        }
        if (achouZero) return ebx;
        ebx = (ebx & 0xFFFFFF00u) | ((ebx ^ al) & 0xFF);   // xor bl, al
        edi = edx;
        pos += 4;
        edx = dwordEm(pos);
    }
}

bool cheatCheckChat(int from, std::string& linha) {
    if (from < 0 || from >= kMaxPlayers) return false;
    const uint32_t h = cheatHash(linha.c_str());
    for (const Entrada& e : kTabela) {
        if (e.hash != h) continue;
        e.efeito(from);
        linha = "Woof Woof!";                              // o que o chat mostra
        return true;
    }
    return false;
}

} // namespace op2
