#pragma once

namespace op2 {

// Estados de animacao de estrutura ([unit+0x34] >> 16), escolhidos por
// sub_408930 pelo dano ([unit+0x1E]) contra o HP maximo do descritor.
enum StructAnimState {
    STRUCT_INTACT   = 0,   // mostra o ULTIMO frame (construcao concluida)
    STRUCT_DAMAGED  = 1,   // dano >= 30%
    STRUCT_HEAVY    = 2,   // dano >= 60%
    STRUCT_DESTROY  = 3,   // contagem regressiva: frame = (nFrames-1) - sub
    STRUCT_AMBIENT0 = 5,   // 5..9: animacoes ambiente (sub_4835C0)
};

// Indice REAL de animacao no op2_art (ja sem o vies 0x65), ou -1.
// tableA = flags & 0x1000 (mesma convencao dos veiculos: A = Eden).
int structureAnimIndex(int type, int state, bool tableA);

} // namespace op2
