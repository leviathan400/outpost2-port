#pragma once

#include <cstdint>
#include <vector>

#include "info_pane.h"
#include "produce_pane.h"

namespace op2 {

// Lista de veículos — porta dword_567D80 (vtable off_4D58E8, base de lista
// off_4D5588). Aberta por "&VEHICLES" do Robot Command Center (sub_459F70) e por
// Alt+V.
//   título "Vehicle List (Alt+V)" (sub_46B2A0 → sub_459B60);
//   lista (sub_459940(1, 0, 0xA, 0, 0, 0x12)) com um tipo por linha, 1..0x0F na
//   ordem de off_4E1340: entra se o tópico do tipo foi pesquisado (sub_472C50)
//   ou o jogador tem algum; linha "%ss<R>%i" (sub_46B390) — refeita quando
//   (tick & 3) == 1 (vt[0] sub_46B120 → vt[14] sub_46B1E0);
//   botões "&JUMP" (habilitado se o tipo escolhido tem veículos, sub_46B340) e
//   "&STATUS" (sub_46B2D0).
//   JUMP (sub_46B3D0): o veículo do tipo com o menor nº de série ([+0x2C]) é
//   selecionado e a vista centralizada nele.
class VehicleListPane {
public:
    bool isOpen() const { return m_open; }
    void open();                                         // vt[1] sub_46B180
    void close() { m_open = false; }
    void update();                                       // vt[0] sub_46B120
    void draw(uint32_t* fb, int fbW, int fbH, const PaneRect& pane, int mx, int my) const;
    bool click(const PaneRect& pane, int x, int y);
    bool jumpEnabled() const;
    // sub_46B3D0: devolve o veículo escolhido (0 = nenhum) e o pixel p/ centralizar.
    int jump(int& px, int& py) const;

private:
    void refresh();                                      // vt[14] sub_46B1E0
    ListBoxLayout layout(const PaneRect& pane) const;

    bool m_open = false;
    std::vector<int> m_types;                            // [+0x1CC] (n = [+0x2CC])
    int m_count[0x11] = {};                              // [+0x424] (dword_5681A0) por tipo
    int m_sel = -1;                                      // [+0x4C4]
    int m_top = 0;
    int m_lastTick = -1;
};

} // namespace op2
