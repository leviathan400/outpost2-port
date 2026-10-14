#include "docking.h"
#include "salvage.h"
#include "garage.h"
#include "../audio/sound.h"

#include "building_sheet.h"
#include "game_map.h"
#include "player.h"
#include "tethys_game.h"
#include "unit.h"
#include "vehicle_anim.h"
#include "mining.h"
#include "../gfx/artfile.h"

namespace op2 {

static bool valid(int id) { return id > 0 && id < static_cast<int>(g_units.size()) && g_units[id].id != -1; }
static int ownerOf(const UnitRecord& r) { return r.owner & 0x0F; }
// vt[0x74] (aceita comando) — a mesma aproximação de commands.cpp.
static bool commandable(const UnitRecord& r) {
    return r.id != -1 && !((r.flags | (r.flags << 2)) & 0x80000u) && r.field22 != 0x0A;
}
static bool isEdenBuilt(const UnitRecord& r) {          // [+0x44] & 0x1000 (prédio de Eden)
    const int o = ownerOf(r);
    return o < kMaxPlayers && g_playerData[o].isEden != 0;
}

bool dockCell(int buildingId, int& x, int& y) {          // sub_482F80
    if (!valid(buildingId)) return false;
    const UnitRecord& b = g_units[buildingId];
    const StructSheetDesc* d = structSheet(b.type);
    if (!d || !(d->flags24C & 0x1C)) return false;       // colunas Docking All/Truck/CV
    const int dock = isEdenBuilt(b) ? d->dockLocE : d->dockLocP;
    // pixelX/Y do port = canto do tile central (o original guarda o centro).
    const int cx = b.pixelX >> 5, cy = b.pixelY >> 5;
    x = (cx - (d->sizeX >> 1) + dock - 1) & (g_gameMap.width() - 1);
    y = cy - (d->sizeY >> 1) + d->sizeY - 1;
    return true;
}

int dockedVehicle(int buildingId) {                       // sub_483010
    int x, y;
    if (!dockCell(buildingId, x, y)) return 0;
    const int v = g_gameMap.cellOccupant(x, y);
    if (!valid(v)) return 0;
    const UnitRecord& u = g_units[v];
    if (!(u.flags & UNIT_VEHICLE) || (u.flags & UNIT_DEAD)) return 0;
    if (u.stepTicks > 0) return 0;                       // sub_44D530: no meio de um passo
    if (u.type == mapSpider || u.type == mapScorpion) return 0;
    const uint32_t f = structSheet(g_units[buildingId].type)->flags24C;
    if (f & 0x04) return v;                              // Docking All
    if ((f & 0x10) && u.type == mapConVec) return v;     // Docking CV
    // Docking Truck: Cargo Truck + sub_44DA80(prédio, 1).
    if (u.type == mapCargoTruck && vehicleCanDock(v, buildingId, true)) return v;
    return 0;
}

bool bayTransferAvailable(int buildingId, int bay) {      // sub_456BC0 (fábricas)
    if (!valid(buildingId)) return false;
    const UnitRecord& b = g_units[buildingId];
    if (b.type == 0x29) return garageBayAvailable(buildingId, bay);   // ramo da garagem
    const int v = dockedVehicle(buildingId);
    if (!v) return false;
    const UnitRecord& u = g_units[v];
    if (!commandable(u) || (u.flags & 0x40000u) || u.field22 == 0x0D) return false;
    if (bay < 0 || bay >= 6) return false;
    int bayType = b.bayType[bay];
    if (!bayType) {
        // sub_415730: todas as baias ocupadas (contando a produção em curso)?
        const StructSheetDesc* d = structSheet(b.type);
        const int bays = d ? d->player[(b.owner >> 4) & 0x0F].storageBays : 0;
        int used = b.action == 9 ? 1 : 0;
        for (int i = 0; i < bays && i < 6; ++i) if (b.bayType[i]) ++used;
        if (used == bays) return false;
        if (u.cargo == 0) return false;                  // baia vazia e veículo vazio
    }
    return static_cast<int16_t>(u.cargo) != static_cast<int16_t>(static_cast<int8_t>(bayType));
}

bool processBayTransfer(int owner, int buildingId, int bay) {   // sub_40E210 case 10
    if (!valid(buildingId)) return false;
    UnitRecord& b = g_units[buildingId];
    if (ownerOf(b) != (owner & 0x0F)) return false;
    // (Humano: anel anti-duplicata [jogador+0xBD8] — rede, não portado.)
    if (b.flags & UNIT_DEAD) return false;
    if (b.type == 0x29) { garageBay(buildingId, bay); return true; }   // vt[0x104] sub_41C5F0
    const int v = dockedVehicle(buildingId);
    if (!v || !commandable(g_units[v]) || (g_units[v].flags & 0x40000u)) {
        if (b.type != 0x37) return false;
    }
    // vt[0x108]: Spaceport sub_47F470 (flag 0, imediato) / fábricas sub_415880.
    if (b.type == 0x37) return spaceportBayTransfer(owner, buildingId, bay);
    if (!v) return false;
    UnitRecord& u = g_units[v];
    u.dockTick = static_cast<int16_t>(TethysGame::tick());   // [+0x5E]
    u.flags &= ~0x1000000u;                               // 1ª fase
    u.dockBay = static_cast<int16_t>(bay);                // [+0x5C]
    u.field22 = 0x0D;
    // [+0x3C] = nº de frames da animação parada do veículo (vt[0x80](0) → sub_404C70).
    const bool eden = ownerOf(u) < kMaxPlayers && g_playerData[ownerOf(u)].isEden;
    const int anim = vehicleAnimIndex(u.type, u.rotation, eden);
    u.field3C = (g_art.ok() && anim >= 0 && anim < g_art.numAnimations())
                    ? static_cast<int>(g_art.animation(anim).frames.size()) : 1;
    if (u.field3C <= 0) u.field3C = 1;
    soundPlayAt(unitOX(u), unitOY(u), 0x3F);
    return true;
}

void sendBayTransfer(int buildingId, int bay) {           // sub_456D60
    processBayTransfer(TethysGame::localPlayer(), buildingId, bay);
}

// sub_411050: acha o prédio cuja doca é a célula do veículo e troca carga ↔ baia.
static void exchangeWithBay(UnitRecord& u) {
    const int vx = u.pixelX >> 5, vy = u.pixelY >> 5;
    for (int i = 1; i < static_cast<int>(g_units.size()); ++i) {
        UnitRecord& b = g_units[i];
        if (b.id == -1 || !(b.flags & UNIT_BUILDING) || ownerOf(b) != ownerOf(u)) continue;
        int x, y;
        if (!dockCell(i, x, y) || x != vx || y != vy) continue;
        const int bay = u.dockBay;
        if (bay < 0 || bay >= 6) return;
        const uint8_t oldCargo = static_cast<uint8_t>(u.cargo), oldWeapon = u.kitWeapon;
        const bool vehBit = (u.flags & 0x200000u) != 0;
        const uint8_t mask = static_cast<uint8_t>(1u << bay);
        const uint8_t al = b.bayBits & mask;
        u.cargo = static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(b.bayType[bay])));
        // Fiel ao original: o bit 0x200000 recebe (al << 21) & 0x200000 (só a baia 0 o liga).
        u.flags = (u.flags & ~0x200000u) | ((static_cast<uint32_t>(al) << 21) & 0x200000u);
        u.kitWeapon = b.bayCargo[bay];
        if (!vehBit) b.bayBits = 0;                       // [+0x67] &= (mask == 0)
        b.bayType[bay] = oldCargo;
        b.bayCargo[bay] = oldWeapon;
        return;
    }
}

void dockTransferTick(int vehicleId) {                    // sub_43DE50 case 13
    UnitRecord& u = g_units[vehicleId];
    // Sons pelo contador [+0x3C] (antes do decremento): 1ª fase 0x51 → garra
    // 0x40, 0x25 → porta 0x3E; 2ª fase 0x4D → porta 0x3E, 0x1C → garra 0x40.
    const bool second = (u.flags & 0x1000000u) != 0;
    if (u.field3C == (second ? 0x4D : 0x51)) soundPlayAt(unitOX(u), unitOY(u), second ? 0x3E : 0x40);
    if (u.field3C == (second ? 0x1C : 0x25)) soundPlayAt(unitOX(u), unitOY(u), second ? 0x40 : 0x3E);
    if (--u.field3C != 0) return;
    if (u.flags & 0x1000000u) {                           // fim da 2ª fase
        u.flags &= ~(0x40000u | 0x80000u);
        u.field23 = u.field22;
        u.field22 = 0;
        // [+0x54]/[+0x56] = 0.
        soundPlayAt(unitOX(u), unitOY(u), 0x3F);
        return;
    }
    // Meio: o caminhão (tipo 1) usa vt[0x98] (minério — não portado); os demais sub_411050.
    if (u.type != mapCargoTruck) exchangeWithBay(u);
    else truckDockExchange(vehicleId);                    // vt[0x98] (sub_406610)
    u.dockTick = static_cast<int16_t>(TethysGame::tick());
    u.flags |= 0x1000000u;
    const bool eden = ownerOf(u) < kMaxPlayers && g_playerData[ownerOf(u)].isEden;
    const int anim = vehicleAnimIndex(u.type, u.rotation, eden);
    u.field3C = (g_art.ok() && anim >= 0 && anim < g_art.numAnimations())
                    ? static_cast<int>(g_art.animation(anim).frames.size()) : 1;
    if (u.field3C <= 0) u.field3C = 1;
    // Caminhão: último frame da animação de doca da 2ª fase (vt[0x80] sub_4062B0).
    if (u.type == mapCargoTruck) u.field3C = truckDockPhaseTicks(u);
}

} // namespace op2
