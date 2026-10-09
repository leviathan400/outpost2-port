#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "info_pane.h"

namespace op2 {

// Painel STORAGE das baias — porta dword_5650F0 (vtable off_4D54B8) no modo
// [+0x6C0] = 1 (Structure Factory 0x2F, Spaceport 0x37; Garage 0x29 pendente):
//   sub_463170  botões de ícone 44x44 das baias em 2 colunas (x = 7 / 0x61) e
//               3 linhas (y = 0x39 + 0x3A*k, coordenadas da janela do quadro),
//               teclas 1..6; comando = troca com o veículo na doca (dword_564A50:
//               sub_456BC0 / sub_456D60); botão STATUS;
//   sub_4637B0  título, nome do item de cada baia (ou "Empty Bay") sob o ícone,
//               "Alt+N" à direita dele, e a caixa da doca (veículo ou vazia) com
//               o texto ("X is empty", "Y carried in X", "Dock is empty...").
// O modo de recursos ([+0x6C0] = 0: armazéns/fundições com LOAD/UNLOAD de
// caminhão) entra com a mineração.
class StoragePane {
public:
    bool isOpen() const { return m_open; }
    int  building() const { return m_bld; }
    static bool supports(int type) { return type == 0x2F || type == 0x37; }
    void open(int buildingId) { m_open = true; m_bld = buildingId; }
    void close() { m_open = false; }
    void update(const std::vector<int>& sel);
    // winL/winT = canto da janela do quadro; pane = área de conteúdo; statusTop =
    // topo do botão STATUS (a caixa da doca fica 0x33 acima).
    void draw(uint32_t* fb, int fbW, int fbH, int winL, int winT, const PaneRect& pane, int statusTop,
              int mx, int my) const;
    // Clique: true se acertou uma baia (e faz a troca quando disponível).
    bool click(int winL, int winT, int x, int y);
    void pressBay(int bay);                              // tecla 1..6 / clique
    std::string tipAt(int winL, int winT, int x, int y) const;

private:
    PaneRect bayRect(int winL, int winT, int i) const;
    int bays() const;
    bool m_open = false;
    int m_bld = 0;
};

} // namespace op2
