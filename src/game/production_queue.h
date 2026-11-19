#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace op2 {

// EXTRA DESTE PORTE (chave "[Game] ProductionQueue", caixa no GAME PREFERENCES
// e linha na aba Options; 0 = como o original, 5.40). O original produz UM item
// por ordem: escolhe na lista, BUILD IT, espera, e repete. Com a chave ligada:
//   - cada clique num item da lista soma +1 ao PEDIDO (o numero aparece na
//     frente do item; clique direito tira 1), em qualquer combinacao de itens,
//     na ordem dos cliques; o clique que o minerio nao cobre nao soma;
//   - o BUILD IT manda o pedido para a FILA da fabrica;
//   - a cada tique, a fabrica parada e pronta para produzir (as mesmas regras
//     do botao BUILD: operando, baia livre, limite de unidades) recebe o
//     proximo item da fila pelo MESMO pacote 9 que o BUILD IT manda.
// A fila e so do lado do jogador local: a simulacao continua recebendo os
// pacotes de sempre, entao a partida em rede e o jogo salvo nao mudam. O
// minerio so e descontado quando cada item comeca, como no original; a
// conferencia do clique soma o que ja esta na fila e no pedido.
void setProductionQueue(bool on);
bool productionQueueOn();

// Custo que o pedido + as filas de TODAS as fabricas do jogador local ja
// comprometem (minerio comum/raro e a comida da Food Cargo) -- o minerio e um
// so para todas. O item em producao ja foi descontado e nao entra.
void productionQueueCommitted(const std::vector<std::pair<int, uint32_t>>& draft,
                              int& common, int& rare, int& food);
// O jogador local pode somar `item` ao pedido `draft` desta fabrica?
bool productionQueueCanAdd(int factoryId, const std::vector<uint32_t>& draft, uint32_t item);

// O BUILD IT: o pedido vai para o fim da fila.
void productionQueueAppend(int factoryId, const std::vector<uint32_t>& items);
const std::vector<uint32_t>& productionQueueOf(int factoryId);
void productionQueueClear(int factoryId);
void productionQueueReset();                     // nova partida / carga

// Por tique (depois do ciclo da simulacao): da a vez ao proximo item de cada
// fabrica parada. So o jogador local, so com a chave ligada.
void productionQueueTick();

} // namespace op2
