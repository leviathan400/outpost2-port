#pragma once

namespace op2 {

// Os cursores de comando do jogo (os .ANI da pasta do Outpost 2).
//
// A tabela do original fica em 0x4E9840 e o sub_488900 a carrega: as três
// primeiras entradas são cursores do Windows (WAIT, SIZE, ARROW) e o resto são
// os .ANI, achados pelo ResManager e com a seta como recuo quando faltam. A
// ORDEM abaixo é a daquela tabela, e é o que diz qual cursor é de qual comando.
//
// Cada comando tem um PAR: o "pode" e o "não pode" — no original a escolha é
// um `cmp eax,1 / adc` sobre o teste de validade (ex.: sub_4515xx escolhe 13
// dock ou 14 nodock). Aqui o teste é o `uiCommandCellOk` da célula sob o
// ponteiro.
enum CursorKind {
    CUR_ARROW = 0,      // 0x7F00, o padrão fora de qualquer mira
    CUR_DESTRUCT,       // 3
    CUR_MOVE,           // 5
    CUR_NOMOVE,         // 6
    CUR_ATTACK,         // 7
    CUR_NOATTACK,       // 8
    CUR_BULLDOZE,       // 9
    CUR_BUILD,          // 11
    CUR_NOBUILD,        // 12
    CUR_DOCK,           // 13
    CUR_NODOCK,         // 14
    CUR_REPAIR,         // 15
    CUR_NOREPAIR,       // 16
    CUR_REPROG,         // 17
    CUR_NOREPROG,       // 18
    CUR_GUARD,          // 19
    CUR_SALVAGE,        // 22
    CUR_WAYPOINT,       // 24
    CUR_COUNT
};

// Lê os .ANI uma vez (silencioso se faltarem: o cursor fica o do sistema).
void cursorsInit();
// Qual cursor mostrar. Só troca de verdade quando muda de tipo.
void cursorSet(CursorKind k);
// Anima: avança o quadro na cadência do próprio arquivo (jiffies de 1/60 s).
void cursorAnimate();
// Quantos arquivos entraram (0 = nenhum; o jogo segue com a seta do sistema).
int cursorsLoaded();

} // namespace op2
