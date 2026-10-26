#pragma once

#include <cstdint>
#include <vector>

#include "info_pane.h"

namespace op2 {

// Caixa de lista dos painéis (base off_4D5588): sub_459940(provedor, 1, arg8,
// argC, arg10, arg14) posiciona a lista em (arg8+0xD, topo+argC+0xD) até
// (W-arg10-0xE, H-alt(seta)-arg14-0x10) e as setas de página 0x1C6/0x1C5
// logo abaixo (esquerda / direita). sub_430400: fundo 0x40, moldura 0x4E0.
struct ListBoxLayout { PaneRect list, up, down; int lineH, rows; };
ListBoxLayout listBoxLayout(const PaneRect& pane, int arg8, int argC, int arg10, int arg14);
void drawListBoxFrame(uint32_t* fb, int fbW, int fbH, const ListBoxLayout& L);
void drawListBoxArrows(uint32_t* fb, int fbW, int fbH, const ListBoxLayout& L, const PaneRect& clip,
                       bool canUp, bool canDown, int mx, int my);

// Painel de produção da fábrica — porta dword_5674E8 (vtable off_4D5808,
// base de lista off_4D5588). Aberto pelo botão BUILD (sub_456250 →
// sub_45D020: o gerenciador do painel direito troca o painel atual).
//   - título (sub_468320): "<nome da fábrica>\nSelect Item to Produce";
//   - lista rolável (controle em +4, sub_459940/sub_430400) com os itens do
//     provedor (+0x1C8, nomes sub_4685C0); setas de página 0x1C6/0x1C5;
//   - botões "BUILD IT"/"STATUS" (sub_4682C0) montados pela GameState.
class ProducePane {
public:
    bool isOpen() const { return m_open; }
    int  factory() const { return m_factory; }
    uint32_t selected() const { return m_sel; }          // [+0x434]
    bool buildEnabled() const { return m_buildOk; }     // botão BUILD IT habilitado
    // Texto que a lista manda ao objeto de ajuda (unk_575570) — o custo do item
    // sob o cursor; vazio quando o cursor não está sobre nenhum.
    const std::string& helpText() const { return m_help; }

    void open(int factoryId);                            // vt1 sub_468090
    void close() { m_open = false; }
    // vt0 sub_467FF0 (por quadro): fábrica produzindo (ação 9) → volta ao
    // status; a cada 4 ticks refaz a lista. Fecha também se a seleção mudou.
    void update(const std::vector<int>& sel);
    void draw(uint32_t* fb, int fbW, int fbH, const PaneRect& pane, int mouseX, int mouseY) const;
    // Clique no painel. Retorna true se consumiu; buildNow = clique duplo num
    // item pagável (sub_4683B0 aciona o BUILD IT).
    bool click(const PaneRect& pane, int x, int y, bool dbl, bool& buildNow);

private:
    struct Layout { PaneRect list, up, down; int lineH, rows; };
    Layout layout(const PaneRect& pane) const;
    void refresh();                                      // vt13 sub_468120
    void select(uint32_t item);                          // sub_4683F0

    bool m_open = false;
    int  m_factory = 0;
    std::vector<uint32_t> m_items;
    uint32_t m_sel = 0;
    int  m_top = 0;                                      // 1º item visível
    bool m_buildOk = false;
    int  m_lastTick = -1;
    mutable std::string m_help;                          // preenchido no desenho
};

} // namespace op2
