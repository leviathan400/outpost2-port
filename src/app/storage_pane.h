#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "info_pane.h"

namespace op2 {

// Painel STORAGE das baias — porta dword_5650F0 (vtable off_4D54B8) no modo
// [+0x6C0] = 1 (Structure Factory 0x2F, Spaceport 0x37, Garage 0x29):
//   sub_463170  botões de ícone 44x44 das baias em 2 colunas (x = 7 / 0x61) e
//               3 linhas (y = 0x39 + 0x3A*k, coordenadas da janela do quadro),
//               teclas 1..6; comando = troca com o veículo na doca (dword_564A50:
//               sub_456BC0 / sub_456D60); botão STATUS;
//   sub_4637B0  título, nome do item de cada baia (ou "Empty Bay") sob o ícone,
//               "Alt+N" à direita dele, e a caixa da doca (veículo ou vazia) com
//               o texto ("X is empty", "Y carried in X", "Dock is empty...").
// Modo de recursos ([+0x6C0] = 0, armazéns 0x19/0x1A, smelters 0x36/0x38,
// Agridome 0x27): botões L&OAD / &UNLOAD / &STATUS (sub_458D10, 3 botões),
// "Stored <recurso>:" (+ "Storage Capacity:" nos de metal) e a doca com
// "<caminhão> is empty" / "<caminhão> with N <carga>" (sub_4637B0).
class StoragePane {
public:
    bool isOpen() const { return m_open; }
    int  building() const { return m_bld; }
    static bool supports(int type) {
        return type == 0x2F || type == 0x37 || type == 0x29 || type == 0x19 || type == 0x1A || type == 0x27 ||
               type == 0x36 || type == 0x38;
    }
    // [+0x6C0] = 0: recursos (armazéns, smelters, Agridome) com LOAD/UNLOAD.
    bool resourceMode() const;
    bool loadEnabled() const;        // estoque, caminhão vazio e sub_456EC0
    bool unloadEnabled() const;      // caminhão com carga e sub_4570D0
    void load();                     // pacote 0x0B
    void unload();                   // pacote 0x0C
    // launchPad: modo [+0x6C4] = 1 (comando LAUNCH PAD, sub_456730) — as baias
    // trocam com a carga do SULV e a caixa mostra a plataforma.
    void open(int buildingId, bool launchPad = false) { m_open = true; m_bld = buildingId; m_launch = launchPad; }
    bool launchMode() const { return m_open && m_launch; }
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
    bool m_launch = false;
    int m_bld = 0;
    bool bayAvailable(int bay) const;
};

} // namespace op2
