#pragma once

#include <cstdint>
#include <string>

namespace op2 {

// Quadro de informação da unidade selecionada — porta a classe de painel
// @off_4D5220 (pintura sub_45A510): cabeçalho sub_459480 (nome, ícone,
// blindagem, HP), seções sub_45A690 (melhorias), sub_45A8F0 (Status) e
// sub_45AEB0 (Produção). As demais seções (Recurso, Demanda, Rendimento,
// Arma, Observatório, Unidades, Launch Pad, Treinamento) entram com os
// subsistemas correspondentes.
struct PaneRect { int l, t, r, b; };

// Texto marcado do renderizador unk_5772A8 (sub_416650/sub_416A00): '\n'
// quebra linha; <L> alinha à esquerda, <R> à direita, <X> centraliza; <Cn>
// troca a cor (n = 0..5); <N> faz o resto do texto ser literal (sem tags).
// Retorna a altura usada.
// bold = fonte de título (unk_578F08, Arial −11 negrito); senão unk_5772A8.
// lineH = o arg_14 de sub_416A00: −1 usa a altura da fonte (tmHeight); os
// painéis de relatório, as seções do quadro e os itens de lista passam
// kMarkupLine (0x0C), o passo fixo do original.
int drawMarkupText(uint32_t* fb, int fbW, int fbH, const PaneRect& rc,
                   const std::string& text, uint32_t defaultColor, bool bold = false,
                   int lineH = -1);
int markupLineHeight();
constexpr int kMarkupLine = 0x0C;
// Barra (sub_45C260): moldura 0x3DEF, parte feita em c1 e o resto em c2 (RGB555).
// sub_458EF0: o ícone de um veículo numa caixa — corpo na cor do dono e, nos
// chassis armados, a torre da arma por cima. `colorPlayer` é o ÍNDICE do
// jogador (a função busca o colorNumber), não o número da cor.
void drawVehicleIcon(uint32_t* fb, int fbW, int fbH, bool eden, int type, int weapon,
                     int l, int t, int r, int b, const PaneRect& clip, int colorPlayer);

void drawProgressBar(uint32_t* fb, int fbW, int fbH, int l, int t, int r, int b, int cur, int max,
                     uint16_t c1, uint16_t c2);

// Desenha o quadro no retângulo do painel (coordenadas de tela).
void drawUnitInfoPane(uint32_t* fb, int fbW, int fbH, const PaneRect& pane,
                      int unitId, int numSelected, int localPlayer);

// Botão de comando do rodapé do quadro: "IDLE" (CommandPacket 0x12) ou
// "ACTIVATE" (0x13) p/ um prédio selecionado do jogador local.
struct PaneButton { PaneRect rc; const char* label; int command; };
bool infoPaneButton(const PaneRect& pane, int unitId, int localPlayer, PaneButton& out);

} // namespace op2
