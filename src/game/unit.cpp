#include "unit.h"

#include "game_map.h"
#include "command.h"
#include "structure.h"

namespace op2 {

// Pool global. Índice 0 reservado como "nenhuma unidade" (IsLive trata id 0).
std::vector<UnitRecord> g_units;

// Footprint por map_id REAL (building.txt X-size/Y-size), id começando em 0x15.
// Ordem = off_4E1340 (enum do SDK), NÃO a ordem das linhas da planilha.
// Mesma fonte que popula off_4E1340[type]->[+0x248/+0x24A] no original.
static const struct { int w, h; } kBuildingFootprint[] = {
    { 2, 1 },  // 0x15 MINE
    { 2, 1 },  // 0x16 MINE_ADV
    { 1, 1 },  // 0x17 TOWER_GUARD
    { 1, 1 },  // 0x18 TOWER_LIGHT
    { 1, 2 },  // 0x19 STORAGE_ORE
    { 1, 2 },  // 0x1A STORAGE_RARE
    { 2, 2 },  // 0x1B FORUM
    { 3, 2 },  // 0x1C COMMAND
    { 2, 2 },  // 0x1D ASE
    { 2, 2 },  // 0x1E RESIDENCE
    { 2, 2 },  // 0x1F ROBOT_COMMAND
    { 2, 2 },  // 0x20 TRADE
    { 2, 2 },  // 0x21 LAB
    { 2, 2 },  // 0x22 MED_CENTER
    { 2, 2 },  // 0x23 NURSERY
    { 3, 2 },  // 0x24 SOLAR
    { 2, 2 },  // 0x25 RECREATION
    { 2, 2 },  // 0x26 UNIVERSITY
    { 3, 2 },  // 0x27 AGRIDOME
    { 3, 2 },  // 0x28 DIRT
    { 3, 2 },  // 0x29 GARAGE
    { 2, 1 },  // 0x2A MAGMA_WELL
    { 2, 2 },  // 0x2B METEOR_DEF
    { 2, 1 },  // 0x2C GEOTHERMAL
    { 2, 2 },  // 0x2D FACT_ANDROID
    { 3, 3 },  // 0x2E FACT_LUXURY
    { 4, 3 },  // 0x2F FACT_STRUCTURE
    { 4, 3 },  // 0x30 FACT_VEHICLE
    { 3, 2 },  // 0x31 LAB_STANDARD
    { 3, 3 },  // 0x32 LAB_ADV
    { 2, 2 },  // 0x33 OBSERVATORY
    { 3, 2 },  // 0x34 RESIDENCE_P
    { 3, 3 },  // 0x35 RESIDENCE_E
    { 4, 3 },  // 0x36 SMELTER
    { 5, 4 },  // 0x37 SPACEPORT
    { 4, 3 },  // 0x38 SMELTER_ADV
    { 3, 2 },  // 0x39 GORF
    { 2, 2 },  // 0x3A TOKAMAK
};

void buildingFootprint(map_id type, int& w, int& h) {
    w = 1; h = 1;
    if (type >= kFirstBuilding && type <= kLastBuilding) {
        const auto& f = kBuildingFootprint[type - kFirstBuilding];
        w = f.w; h = f.h;
    }
}

void resetUnits() {
    g_units.clear();
    g_units.emplace_back();   // slot 0 = nulo
}

UnitRecord* Unit::rec() const {
    if (m_id <= 0 || m_id >= static_cast<int>(g_units.size())) return nullptr;
    return &g_units[m_id];
}

int Unit::IsLive() {
    // Original: id!=0, [rec+4]!=0xFFFFFFFF e !(flags & 0x20000); senão zera o wrapper.
    UnitRecord* r = rec();
    if (m_id != 0 && r && r->id != -1 && !(r->flags & UNIT_DEAD))
        return 1;
    m_id = 0;
    return 0;
}

LOCATION Unit::Location() const {
    UnitRecord* r = rec();
    if (!r) return LOCATION{0, 0};
    return LOCATION{ r->pixelX >> 5, r->pixelY >> 5 };   // tile = pixel>>5
}

int Unit::OwnerID() const {
    UnitRecord* r = rec();
    return r ? (r->owner & 0x0F) : 0;
}

map_id Unit::GetType() const {
    UnitRecord* r = rec();
    return r ? r->type : kMapIdNone;
}

int Unit::IsBuilding() const {
    UnitRecord* r = rec();
    return (r && (r->flags & UNIT_BUILDING)) ? 1 : 0;
}

int Unit::IsVehicle() const {
    UnitRecord* r = rec();
    return (r && (r->flags & UNIT_VEHICLE)) ? 1 : 0;
}

map_id Unit::GetCargo() const {
    UnitRecord* r = rec();
    return r ? static_cast<map_id>(static_cast<int16_t>(r->cargo)) : kMapIdNone;
}

void Unit::SetCargo(map_id c) {
    UnitRecord* r = rec();
    if (r) r->cargo = static_cast<uint16_t>(c);
}

void Unit::DoDeath() {
    UnitRecord* r = rec();
    if (!r || r->id == -1) return;
    // Unit::DoDeath @191271 = sub_43A890(1): só agenda a morte (ação 0x2F); a
    // máquina de estados faz o resto (prédio: animação de destruição, entulho,
    // remoção — src/game/structure.cpp).
    if (!(r->flags & UNIT_VEHICLE)) { killUnit(m_id, 1); return; }
    // Veículo: o caminho de morte de veículo (loc_43D8B3: destroços, explosão)
    // ainda não foi portado — por ora libera a célula e marca como morto.
    if (g_gameMap.ok() && !(r->flags & UNIT_DEAD)) {
        int cx = r->pixelX >> 5, cy = r->pixelY >> 5;
        if (g_gameMap.cellOccupant(cx, cy) == m_id) g_gameMap.clearUnitOccupancy(cx, cy);
    }
    r->flags |= UNIT_DEAD;
}

void Unit::DoMove(LOCATION dest) {
    // Porta Unit::DoMove @asm 190538: valida slot vivo + alvo dentro do mapa,
    // monta o CommandPacket tipo 2 (MOVE) com o tile empacotado e despacha pelo
    // objeto de comando do dono.
    UnitRecord* r = rec();
    if (!r || r->id == -1) return;             // [esi+4] == -1 (slot morto)
    if (r->flags & UNIT_DEAD) return;          // flags & 0x20000
    if (g_gameMap.ok()) {
        if (dest.x < 0 || dest.x >= g_gameMap.width())  return;  // 0 <= x < width (54EBC8)
        if (dest.y < 0 || dest.y >= g_gameMap.height()) return;  // 0 <= y < height (54EBD0)
    }

    CommandPacket pkt;
    pkt.type = CMD_MOVE;                        // var_74 = 2
    pkt.unitIds.push_back(m_id);               // lista de unidades (count=1)
    pkt.waypoints.push_back(packLocation(dest.x, dest.y));  // LOCATION empacotado
    processCommandPacket(r->owner & 0x0F, pkt);            // sub_40E210
}

bool createUnit(Unit& out, map_id type, LOCATION loc, int owner,
                map_id cargo, int rotation) {
    if (g_units.empty()) resetUnits();

    // Clampa a posição ao mapa (o original usa clip/limites; aqui o essencial).
    if (g_gameMap.ok()) {
        int w = g_gameMap.width(), h = g_gameMap.height();
        loc.x &= (w - 1);                       // wrap horizontal, como o engine
        if (loc.y < 0) loc.y = 0;
        if (loc.y >= h) loc.y = h - 1;
    }

    // Aloca um slot: reusa um livre (id==-1) ou cresce o pool.
    int slot = -1;
    for (int i = 1; i < static_cast<int>(g_units.size()); ++i) {
        if (g_units[i].id == -1) { slot = i; break; }
    }
    if (slot < 0) {
        slot = static_cast<int>(g_units.size());
        g_units.emplace_back();
    }

    UnitRecord& r = g_units[slot];
    r = UnitRecord{};
    r.type   = type;
    r.id     = slot;
    r.pixelX = loc.x << 5;                       // tile<<5 (como +0x14/+0x18)
    r.pixelY = loc.y << 5;
    r.owner  = static_cast<uint8_t>(owner & 0x0F);
    r.cargo  = static_cast<uint16_t>(cargo);
    r.rotation = static_cast<uint8_t>(rotation & 0xFF);
    // Classe pela faixa do map_id: prédio (0x15..0x3A), veículo (0x01..0x10) ou
    // nenhuma das duas (efeitos/armas, ex. AMBIENT_ANIM 0x72 — sem ocupação).
    r.flags  = mapIdIsBuilding(type) ? UNIT_BUILDING
             : mapIdIsVehicle(type)  ? UNIT_VEHICLE : 0u;

    // Grava a ocupação no grid runtime (bits 16-26), como o engine faz ao
    // posicionar a unidade — é o que sub_4382C0/sub_438390 leem. Veículo = 1 tile;
    // prédio = footprint W×H (building.txt), com a LOCATION no CENTRO (canto
    // superior-esquerdo = loc - (w/2, h/2), como o render/placement do original).
    if (g_gameMap.ok()) {
        int cx = r.pixelX >> 5, cy = r.pixelY >> 5;
        if (mapIdIsBuilding(type)) {
            int w, h; buildingFootprint(type, w, h);
            int tlx = cx - (w >> 1), tly = cy - (h >> 1);
            for (int dy = 0; dy < h; ++dy)
                for (int dx = 0; dx < w; ++dx)
                    g_gameMap.setUnitOccupancy(tlx + dx, tly + dy, slot);
        } else if (mapIdIsVehicle(type)) {
            g_gameMap.setUnitOccupancy(cx, cy, slot);
        }
    }

    out.SetId(slot);
    return true;
}

} // namespace op2
