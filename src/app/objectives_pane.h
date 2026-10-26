#pragma once

#include <cstdint>

#include "info_pane.h"

namespace op2 {

// Painel "Mission Objectives" — porta o modo 1 do painel off_4D5628 (instância
// unk_565AC8, a aba de objetivos): sub_465140 abre a lista com sub_459940
// (margens 0, 0xA, 0, 0x12), sub_4652C0 a preenche com os objetivos da missão
// (sem os cumpridos, salvo o atalho de depuração dword_57563C), o provedor
// sub_4663A0 dá "<C0>texto" (cumprido) / "<C2>texto" (pendente), sub_4653B0
// desenha o título "Mission Objectives (Alt+O)" e sub_465830 o botão largo
// "&GAME OPTIONS".
// dword_57563C: mostrar também os objetivos já cumpridos (a opção "Display
// Completed Objectives" do Game Options).
bool objectivesShowCompleted();
void objectivesSetShowCompleted(bool on);

class ObjectivesPane {
public:
    void draw(uint32_t* fb, int fbW, int fbH, const PaneRect& pane, int mx, int my) const;
    bool click(const PaneRect& pane, int x, int y);      // setas de página
private:
    int m_top = 0;                                       // 1ª linha visível
};

} // namespace op2
