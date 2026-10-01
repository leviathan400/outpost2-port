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

// Registro de unidade — no original é um array global (dword_54EC08) de registros
// de 120 bytes; aqui modelamos os campos semânticos decodificados (offset real no
// comentário). O tipo, que no original vem do vtable (+0x00), guardamos direto.
struct UnitRecord {
    map_id   type   = kMapIdNone;  // (+0x00 vtable→GetType) tipo da unidade
    int      id     = -1;          // +0x04 (-1/0xFFFFFFFF = slot livre)
    int      pixelX = 0;           // +0x14 (tile<<5)
    int      pixelY = 0;           // +0x18 (tile<<5)
    uint8_t  owner  = 0;           // +0x1D nibble baixo = dono (jogador)
    uint8_t  action = 0;           // +0x21 (ação/ocupado)
    uint16_t cargo  = 0;           // +0x24 (cargo/arma, map_id)
    uint32_t flags  = 0;           // +0x44
    uint8_t  rotation = 0;         // +0x1C (direção<<5 no original)
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
