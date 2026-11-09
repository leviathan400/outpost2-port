#pragma once

#include <vector>

namespace op2 {

struct UnitRecord;

// Obras de terreno — porta:
//   Earthworker (muro/tubo): comando sub_452640 (modo 4: arrastar uma reta de
//     até 8 tiles, sub_41C410) → sub_452660 pacote 7 {retângulo, tipo} →
//     sub_4234C0 (ação 7, retângulo [+0x62..+0x68] com x2/y2 − 1, tipo [+0x24]);
//     validação do cursor sub_452770 (alguma célula aceita por sub_438C60).
//   Robo-Dozer: comando sub_453760 (modo 1: retângulo) → sub_453780 pacote 1 →
//     sub_423780 (só a 1ª unidade; ação 1, retângulo [+0x62..+0x68]).
//   Início (jpt_43AD56 casos 1/7): sub_40A0A0 — canto do retângulo mais
//     próximo vira o código de percurso ([+0x70] muro / [+0x6A] dozer) e
//     sub_409DB0 anda pela área (serpentina) até a 1ª célula aceita pelo vt[0x90]
//     (sub_40A470 → sub_438C60 / sub_40A300) e manda o veículo: muro → ao lado
//     (objetivo modo 3, célula em [+0x6C/+0x6E]); dozer → em cima (modo 0).
//   Passo do muro (jpt_43B312 case 7): ao lado, gira p/ a célula, valida
//     (sub_4389E0), cobra o custo do tipo e espera (BuildTime + custo do
//     terreno p/ tubo) × ProductionRate / 128 em 0x0A; depois sub_4235E0 assenta
//     (createWallOrTube) e segue (sub_40A2B0). "Wall/Tube completed" no fim.
//   Passo do dozer (case 1): em cima, espera custo(terreno)*3/64 ×
//     ProductionRate / 128 (sub-estado 6) e terraplana (0x15); "Bulldozing completed".

// sub_438C60: a célula aceita muro/tubo do tipo? (contexto do pathfinder já setado)
bool wallCellOk(int x, int y, int wallType);
// CORRECAO (nao e do original): larga a celula reservada por um Earthworker que
// estava levantando muro. Sem isto, morrer no meio da obra deixa a reserva do
// bit 31 na celula para sempre -- um "perma-wall" invisivel que bloqueia o
// caminho e nao aparece em lugar nenhum. Chamada na morte da unidade.
void earthworkAbort(int id);
// sub_438E50: a célula aceita terraplanagem (cursor)?
bool dozeCellOk(int x, int y);

// Pacotes (jogador local). Retângulo em tiles, x2/y2 EXCLUSIVOS (como o filtro).
// Retornam false se nenhuma célula serve (o clique é recusado: som 0x58).
bool sendBuildWall(const std::vector<int>& sel, int x1, int y1, int x2, int y2, int wallType);
// sub_40E210 case 7 → sub_4234C0 p/ uma unidade (qualquer dono); x2/y2 exclusivos.
bool earthworkWallOrder(int id, int x1, int y1, int x2, int y2, int wallType);
bool sendBulldoze(const std::vector<int>& sel, int x1, int y1, int x2, int y2);

// sub_41C410: restringe o fim do arrasto (pixels) a uma reta de até 8 tiles.
void wallDragConstrain(int sx, int sy, int& ex, int& ey);

// Remover muro/tubo (off_4D4C38): clique (modo 3) num muro 0x17..0x19 ou num
// tubo 0x1A sem prédio (sub_452B60) → pacote 8 (sub_452D10) → vt[0x94] da
// Earthworker (sub_423500: ação 8, ponto e célula em [+0x62..+0x68]).
// Ação 8: vai p/ o lado (modo 3), gira, espera ProductionRate×375/128/2 (tubo:
// poeira 0x619) e destrói (sub_4A25F0); tubo custa 100 de dano à máquina.
bool sendRemoveWall(const std::vector<int>& sel, int tileX, int tileY);
void removeWallTick(int id);     // jpt_43B312 case 8

// Varredura em serpentina da área (sub_40A0A0/sub_409DB0), modo dozer (em cima),
// reutilizada pelo SALVAGE do GORF: `pred` é o vt[0x90] (célula é um alvo?).
// Retorna 0 (fim), 1 (andando até o alvo) ou 2 (já no alvo). O código do percurso
// fica em `code` (como [+0x6A]/[+0x70] do original).
int earthAreaScanStart(int id, unsigned char& code, bool (*pred)(const UnitRecord&, int, int));
int earthAreaScanNext(int id, unsigned char& code, bool (*pred)(const UnitRecord&, int, int));

void earthworkStart(int id);     // jpt_43AD56 casos 1, 7 e 8 ([+0x20] = 1)
void wallBuildTick(int id);      // jpt_43B312 case 7
void bulldozeTick(int id);       // jpt_43B312 case 1

} // namespace op2
