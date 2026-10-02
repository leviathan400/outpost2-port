#pragma once

#include <cstdint>
#include <vector>

#include "location.h"

namespace op2 {

// map_id — enum de tipos de unidade do OP2. A fronteira prédio/veículo é
// confirmada no CreateUnit (@195141): tipos 0x15..0x3A (21..58) são PRÉDIOS;
// os demais (>=1) são veículos. O enum completo (nomes) virá com as tabelas de
// dados (building.txt/vehicles.txt, [[op2-sheets-format]]); por ora usamos o id.
using map_id = int;
constexpr int kMapIdNone = 0;
constexpr int kFirstBuilding = 0x15;  // 21
constexpr int kLastBuilding  = 0x3A;  // 58
inline bool mapIdIsBuilding(map_id t) { return t >= kFirstBuilding && t <= kLastBuilding; }

// Flags do registro de unidade (+0x44 no original).
enum UnitFlags : uint32_t {
    UNIT_VEHICLE  = 1u << 1,   // bit1
    UNIT_BUILDING = 1u << 2,   // bit2
    UNIT_DEAD     = 1u << 17,  // bit17 (morta/armazenada; IsLive testa isto)
};

// Bloco de ordem — porta o bloco alocado em unit+0x38 por sub_4230F0 (@asm 57211;
// no original 0x35 dwords = 212 bytes, alocado de unk_5565E0 via sub_446A50).
// Guarda a lista de waypoints da ordem de movimento e o progresso do pathfinder.
struct OrderBlock {
    int count       = 0;   // [0x00] nº de waypoints
    int countMinus1 = 0;   // [0x04] count-1
    uint32_t flags  = 0;   // [0x34] (|= 1 ao setar a ordem)
    int multiFlag   = 0;   // [0x3C] (setado p/ grupos > 4 unidades)
    int field40     = 0;   // [0x40]
    // [0x14...] lista de LOCATIONs EMPACOTADOS (ver packLocation): x em bits5-14,
    // y em bits20-28, campo de 29 bits. Copiada verbatim do buffer de comando.
    std::vector<uint32_t> waypoints;
    int curWaypoint = 0;   // índice do waypoint atual (consumido no tick/busca)
};

// Registro de unidade — no original é um array global (dword_54EC08) de registros
// de 120 bytes (0x78); aqui modelamos os campos semânticos decodificados (offset
// real no comentário). O tipo, que no original vem do vtable (+0x00), guardamos direto.
struct UnitRecord {
    map_id   type   = kMapIdNone;  // (+0x00 vtable→GetType) tipo da unidade
    int      id     = -1;          // +0x04 (-1/0xFFFFFFFF = slot livre)
    int      pixelX = 0;           // +0x14 (tile<<5)
    int      pixelY = 0;           // +0x18 (tile<<5)
    uint8_t  owner  = 0;           // +0x1D nibble baixo = dono (jogador)
    uint8_t  field20 = 0;          // +0x20 (=1 quando há ordem ativa)
    uint8_t  action = 0;           // +0x21 (tipo da ação/ordem atual; 2 = move)
    uint8_t  field22 = 0;          // +0x22 (sub-estado; 0x0D = não-comandável)
    uint16_t cargo  = 0;           // +0x24 (cargo/arma, map_id)
    int      field3C = 0;          // +0x3C (progresso/contador da ordem)
    uint32_t flags  = 0;           // +0x44
    uint8_t  rotation = 0;         // +0x1C (direção<<5 no original)
    int      moveClass = 0;        // classe de movimento (no original [objUnit+0x1E8];
                                   // índice na tabela de custo; vem do descritor do tipo)
    uint8_t  curDir = 0;           // direção atual (0..7; no original rotation>>5, +0x1C)
    uint16_t field4E = 0x7FFF;     // +0x4E (usado por sub_44BD60; 0x7FFF = padrão)
    OrderBlock order;              // +0x38 (ponteiro no original; aqui embutido)
    bool hasOrder = false;         // order block alocado? (unit+0x38 != null)

    // --- Execução de rota (consumo da rota do pathfinder) --------------------
    // NB: executor sobre a rota REAL de pfSearchRoute; o integrador byte-exato
    // (sub_44B650, com o modelo completo do objeto Unit/descritor) é refinamento.
    std::vector<int> routeDirs;    // direções (0..7) da rota atual
    int   routeIdx   = 0;          // próximo passo da rota
    int   moveSpeed  = 6;          // pixels por tick do deslize (TODO: descritor vehicles.txt)
    bool  sliding    = false;      // deslizando entre células?
    int   slideDir   = 0;          // direção do deslize atual
    int   slideProg  = 0;          // progresso 0..32 (px) dentro da célula
    int   slideSX    = 0, slideSY = 0;  // pixel de origem do deslize
};

// Pool global de unidades (índice = id da unidade). O id 0 é "nenhuma unidade".
extern std::vector<UnitRecord> g_units;
void resetUnits();

// Unit — wrapper do id da unidade (porta a classe Unit, asm @189861+). this+0=id.
class Unit {
public:
    Unit() : m_id(0) {}
    explicit Unit(int id) : m_id(id) {}

    void SetId(int id) { m_id = id; }           // ??SetId
    int  id() const { return m_id; }
    UnitRecord* rec() const;                     // registro ou nullptr se inválido

    int IsLive();                                // vivo? (zera o wrapper se não)
    LOCATION Location() const;                   // (pixelX>>5, pixelY>>5)
    int OwnerID() const;                         // dono & 0x0F
    map_id GetType() const;
    int IsBuilding() const;                      // flags bit2
    int IsVehicle() const;                       // flags bit1
    map_id GetCargo() const;
    void SetCargo(map_id c);
    void DoDeath();                              // marca morta (flags bit17)

    // DoMove — porta Unit::DoMove @asm 190538. Monta um CommandPacket tipo 2
    // (MOVE) com o tile alvo empacotado e despacha pelo objeto de comando do
    // dono (sub_40E210). Valida slot vivo + alvo dentro do mapa, como o original.
    void DoMove(LOCATION dest);

private:
    int m_id;   // this+0 = id da unidade
};

// CreateUnit (porta TethysGame::CreateUnit @195141, versão pragmática): aloca um
// slot no pool e preenche o registro (tipo, posição, dono, cargo, rotação),
// marcando prédio/veículo pela faixa do tipo; grava o id em `out`. Simplifica os
// alocadores/vtables/reserva de tiles do original, mas o resultado observável é
// o mesmo (unidade viva, do tipo, na posição, do dono). Retorna true em sucesso.
bool createUnit(Unit& out, map_id type, LOCATION loc, int owner,
                map_id cargo = kMapIdNone, int rotation = 0);

} // namespace op2
