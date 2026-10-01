#include "unit.h"

#include "game_map.h"

namespace op2 {

// Pool global. Índice 0 reservado como "nenhuma unidade" (IsLive trata id 0).
std::vector<UnitRecord> g_units;

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
    if (r) r->flags |= UNIT_DEAD;
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

    out.SetId(slot);
    return true;
}

} // namespace op2
