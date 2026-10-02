#include "unit.h"

#include "game_map.h"
#include "command.h"

namespace op2 {

// Pool global. Índice 0 reservado como "nenhuma unidade" (IsLive trata id 0).
std::vector<UnitRecord> g_units;

// Footprint por map_id (building.txt X-size/Y-size), id começando em 0x15.
// Mesma fonte que popula off_4E1340[type]->[+0x248/+0x24A] no original.
static const struct { int w, h; } kBuildingFootprint[] = {
    { 3, 2 },  // 0x15 COMMAND
    { 2, 2 },  // 0x16 TOKAMAK
    { 2, 2 },  // 0x17 ASE
    { 2, 1 },  // 0x18 GEOTHERMAL
    { 3, 2 },  // 0x19 SOLAR
    { 4, 3 },  // 0x1A FACT_STRUCTURE
    { 1, 1 },  // 0x1B TOWER_GUARD
    { 4, 3 },  // 0x1C SMELTER
    { 1, 2 },  // 0x1D STORAGE_ORE
    { 3, 3 },  // 0x1E LAB_ADV
    { 3, 2 },  // 0x1F LAB_STANDARD
    { 2, 2 },  // 0x20 LAB
    { 4, 3 },  // 0x21 SMELTER_ADV
    { 1, 2 },  // 0x22 STORAGE_RARE
    { 4, 3 },  // 0x23 FACT_VEHICLE
    { 2, 2 },  // 0x24 UNIVERSITY
    { 2, 2 },  // 0x25 NURSERY
    { 3, 2 },  // 0x26 AGRIDOME
    { 2, 2 },  // 0x27 ROBOT_COMMAND
    { 5, 4 },  // 0x28 SPACEPORT
    { 2, 2 },  // 0x29 MED_CENTER
    { 2, 2 },  // 0x2A FACT_ANDROID
    { 3, 2 },  // 0x2B DIRT
    { 3, 2 },  // 0x2C GARAGE
    { 3, 2 },  // 0x2D GORF
    { 3, 3 },  // 0x2E RESIDENCE_E
    { 2, 2 },  // 0x2F RESIDENCE
    { 3, 2 },  // 0x30 RESIDENCE_P
    { 2, 2 },  // 0x31 OBSERVATORY
    { 2, 2 },  // 0x32 METEOR_DEF
    { 2, 2 },  // 0x33 RECREATION
    { 3, 3 },  // 0x34 FACT_LUXURY
    { 2, 2 },  // 0x35 FORUM
    { 2, 2 },  // 0x36 TRADE
    { 1, 1 },  // 0x37 TOWER_LIGHT
    { 2, 1 },  // 0x38 MAGMA_WELL
    { 2, 1 },  // 0x39 MINE
    { 2, 1 },  // 0x3A MINE_ADV
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
    if (!r) return;
    // Libera a ocupação do grid (footprint do prédio ou tile do veículo), limpando
    // só as células que ainda apontam p/ esta unidade (não apaga ocupação alheia).
    if (g_gameMap.ok() && !(r->flags & UNIT_DEAD)) {
        int cx = r->pixelX >> 5, cy = r->pixelY >> 5;
        if (r->flags & UNIT_BUILDING) {
            int w, h; buildingFootprint(r->type, w, h);
            int tlx = cx - (w >> 1), tly = cy - (h >> 1);
            for (int dy = 0; dy < h; ++dy)
                for (int dx = 0; dx < w; ++dx)
                    if (g_gameMap.cellOccupant(tlx + dx, tly + dy) == m_id)
                        g_gameMap.clearUnitOccupancy(tlx + dx, tly + dy);
        } else if (g_gameMap.cellOccupant(cx, cy) == m_id) {
            g_gameMap.clearUnitOccupancy(cx, cy);
        }
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
    r.flags  = mapIdIsBuilding(type) ? UNIT_BUILDING : UNIT_VEHICLE;

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
        } else {
            g_gameMap.setUnitOccupancy(cx, cy, slot);
        }
    }

    out.SetId(slot);
    return true;
}

} // namespace op2
