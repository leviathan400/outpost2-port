#pragma once

#include <cstdint>
#include <string>
#include <vector>

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
// Testes: as linhas que a lista mostraria numa largura (pendentes, quebradas).
std::vector<std::string> objectivesDebugLines(int width);
void objectivesSetShowCompleted(bool on);

class ObjectivesPane {
public:
    void draw(uint32_t* fb, int fbW, int fbH, const PaneRect& pane, int mx, int my) const;
    bool click(const PaneRect& pane, int x, int y);      // setas de página
    // Testes (5.54): a rolagem e o que a lista mostra.
    int debugTop() const { return m_top; }
    void debugSetTop(int t) { m_top = t; }
    int debugRows(const PaneRect& pane) const;
    int debugVisible(const PaneRect& pane) const;        // linhas com texto na janela
private:
    // sub_430CE0 (5.54): quando os objetivos mudam, a lista procura o ITEM que
    // estava no topo e continua dele; se ele sumiu (um objetivo cumprido sai
    // da lista), volta ao comeco. O porte guardava so o numero da linha: a
    // lista encolhia, a linha passava do fim e os objetivos restantes ficavam
    // fora da vista.
    void reanchor(const std::vector<std::string>& lines, const std::vector<std::string>& owners, int rows) const;
    int m_top = 0;                                       // 1ª linha visível
    mutable std::vector<std::string> m_lastLines;        // a lista do ultimo desenho
    mutable std::string m_anchor;                        // o objetivo no topo
    mutable int m_anchorOff = 0;                         // linha do topo dentro dele
};

} // namespace op2
