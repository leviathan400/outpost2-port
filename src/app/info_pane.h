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
// troca a cor (n = 0..5). Retorna a altura usada.
// bold = fonte de título (unk_578F08, Arial −11 negrito); senão unk_5772A8.
int drawMarkupText(uint32_t* fb, int fbW, int fbH, const PaneRect& rc,
                   const std::string& text, uint32_t defaultColor, bool bold = false);
int markupLineHeight();

// Desenha o quadro no retângulo do painel (coordenadas de tela).
void drawUnitInfoPane(uint32_t* fb, int fbW, int fbH, const PaneRect& pane,
                      int unitId, int numSelected, int localPlayer);

// Botão de comando do rodapé do quadro: "IDLE" (CommandPacket 0x12) ou
// "ACTIVATE" (0x13) p/ um prédio selecionado do jogador local.
struct PaneButton { PaneRect rc; const char* label; int command; };
bool infoPaneButton(const PaneRect& pane, int unitId, int localPlayer, PaneButton& out);

} // namespace op2
