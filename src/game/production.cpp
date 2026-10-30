#include "production.h"

#include "../net/net_game.h"
#include "../audio/sound.h"

#include <cstdio>
#include <cstring>

#include "building_sheet.h"
#include "colony.h"
#include "messages.h"
#include "scenario.h"
#include "structure.h"
#include "weapon.h"
#include "player.h"
#include "research.h"
#include "tethys_game.h"
#include "unit.h"
#include "ai_group.h"

namespace op2 {

// Tabelas fixas lidas do OUTPOST2.EXE (terminadas em -100). Entrada > 0 = tipo;
// entrada -k = chassi[k-1] combinado com cada arma da lista de armas.
static const int kArachnidItems[]   = {-1, -2, 0x03, 0x70, -100};          // unk_4E33C0
static const int kArachnidChassis[] = {0x04, 0x71};                         // unk_4E33D8
static const int kArachnidWeapons[] = {0x49, -100};                         // unk_4E33E0
static const int kConsumerItems[]   = {0x6C, 0x6D, 0x6E, -100};             // asc_4E3358
static const int kStructItems[] = {                                         // unk_4E3460
    0x27, 0x1C, 0x28, 0x2D, 0x2E, 0x2F, 0x30, 0x1B, 0x29, 0x39, -1, 0x32, 0x21, 0x31, 0x18,
    0x22, 0x1D, 0x2B, 0x23, 0x33, 0x25, 0x1E, 0x35, 0x34, 0x1F, 0x36, 0x38, 0x24, 0x37,
    0x19, 0x1A, 0x3A, 0x20, 0x26, -100};
static const int kStructChassis[] = {0x17};                                 // unk_4E34EC
static const int kStructWeapons[] = {                                       // unk_4E34F0
    0x3B, 0x3C, 0x46, 0x3D, 0x3E, 0x3F, 0x40, 0x43, 0x47, 0x44, 0x48, -100};
static const int kVehicleItems[] = {                                        // unk_4E33E8
    0x01, 0x02, 0x0F, 0x0D, 0x0A, 0x0E, 0x0C, 0x09, 0x08, 0x0B, -1, -2, -3, -100};
static const int kVehicleChassis[] = {0x05, 0x06, 0x07};                    // unk_4E3420
static const int kVehicleWeapons[] = {                                      // unk_4E3430
    0x3B, 0x3C, 0x46, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x47, 0x42, 0x48, -100};
static const int kSpaceportItems[] = {                                      // unk_4E3368
    0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65,
    0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, -100};

static bool validFactory(int id) {
    return id > 0 && id < static_cast<int>(g_units.size()) && g_units[id].id != -1;
}

// Baia vazia (vt[0x104](i) == 0 p/ alguma baia 0..nº de baias do dono).
static bool hasEmptyBay(const UnitRecord& f) {
    const StructSheetDesc* d = structSheet(f.type);
    const int bays = d ? d->player[(f.owner >> 4) & 0x0F].storageBays : 0;
    for (int i = 0; i < bays && i < 6; ++i)
        if (!f.bayType[i]) return true;
    return false;
}

bool factoryCanProduce(int factoryId, int type) {           // sub_4567B0
    if (!validFactory(factoryId)) return false;
    const UnitRecord& f = g_units[factoryId];
    if (!structureOperational(f)) return false;              // vt+0xA0
    if (type == 0) return true;
    const int p = TethysGame::localPlayer();                 // dword_56DEE8
    const uint32_t own = unitOwnerFlags(type);               // desc+0x1F4
    if (g_playerData[p].isEden ? !(own & 0x1000u) : !(own & 0x800u)) return false;
    if (!researchHas(p, researchTopicIndex(type))) return false;   // sub_472C50(desc+0x1E4)
    if (type < 0x58 || type > 0x6B) return true;
    if (type == 0x58 || (type >= 0x5A && type <= 0x63)) {
        if (!hasEmptyBay(f)) return false;
        // EDWARD: sub_490800 (contagem) == 0; módulos: sub_490780 (já lançado) == 0.
        return type == 0x58 ? playerSpaceCount(p, type) == 0 : playerSpaceHas(p, type) == 0;
    }
    if (type >= 0x64 && type <= 0x68) {
        // sub_4907E0: todos os módulos 0x5A..0x63 lançados (bits 13..22).
        if ((g_playerData[p].spaceBits & 0x7FE000u) != 0x7FE000u) return false;
        if (type == 0x67 && (!playerSpaceHas(p, 0x64) || !playerSpaceHas(p, 0x65) || !playerSpaceHas(p, 0x66)))
            return false;                                     // Evacuation Module: as 3 cargas
        if (!hasEmptyBay(f)) return false;
        return playerSpaceHas(p, type) == 0;
    }
    if (type == 0x59) return hasEmptyBay(f);                  // Solar Satellite
    return f.padCargo == 0;                                   // SULV/RLV/míssil: [+0x6E] vazio
}

std::vector<uint32_t> factoryProduceList(int factoryId) {    // sub_468120
    std::vector<uint32_t> out;
    if (!validFactory(factoryId)) return out;
    const int* items = nullptr; const int* chassis = nullptr; const int* weapons = nullptr;
    switch (g_units[factoryId].type) {
        case 0x2D: items = kArachnidItems; chassis = kArachnidChassis; weapons = kArachnidWeapons; break;
        case 0x2E: items = kConsumerItems; break;
        case 0x2F: items = kStructItems; chassis = kStructChassis; weapons = kStructWeapons; break;
        case 0x30: items = kVehicleItems; chassis = kVehicleChassis; weapons = kVehicleWeapons; break;
        case 0x37: items = kSpaceportItems; break;
        default: return out;
    }
    for (const int* e = items; *e != -100; ++e) {
        if (*e > 0) {
            // Trade Center (0x20) só com o objeto dword_56DC78 (criado em
            // sub_40C4E0 e afins; não portado → 0, o item fica de fora).
            if (*e == 0x20) continue;
            if (factoryCanProduce(factoryId, *e)) out.push_back(static_cast<uint32_t>(*e));
            continue;
        }
        const int ch = chassis[-*e - 1];
        if (!factoryCanProduce(factoryId, ch)) continue;
        for (const int* w = weapons; *w != -100; ++w)
            if (factoryCanProduce(factoryId, *w))
                out.push_back(static_cast<uint32_t>(ch) | (static_cast<uint32_t>(*w) << 16));
    }
    return out;
}

static int costOf(int type, int player, int off) {          // bloco + 0x0C / 0x10
    const uint8_t* b = unitStatBlock(type, player);
    return b ? *reinterpret_cast<const int32_t*>(b + off) : 0;
}

bool produceAffordable(int factoryId, uint32_t item) {     // sub_468430
    const int p = TethysGame::localPlayer();
    const int type = static_cast<int>(item & 0xFFFF), weapon = static_cast<int>(item >> 16);
    const PlayerData& pd = g_playerData[p];
    int common = costOf(type, p, 0x0C), rare = costOf(type, p, 0x10);
    if (weapon) { common += costOf(weapon, p, 0x0C); rare += costOf(weapon, p, 0x10); }
    if (rare > pd.rareOre || common > pd.ore) return false;   // dword_56E300 / dword_56E2FC
    if (validFactory(factoryId) && g_units[factoryId].type == 0x37 && type >= 0x69 && type <= 0x6B &&
        (g_units[factoryId].padCargo != 0 || g_units[factoryId].padReserved != 0))
        return false;
    if (type == 0x66 && pd.foodStored < 10000) return false;   // Food Cargo: 10000 de comida
    return true;
}

std::string produceItemName(uint32_t item) {               // sub_4685C0
    const int type = static_cast<int>(item & 0xFFFF), weapon = static_cast<int>(item >> 16);
    char buf[128];
    if (type >= 0x11 && type <= 0x3A) {                      // prédios: nome +0x220
        if (weapon && type != 4) std::snprintf(buf, sizeof(buf), "%s/%s", unitProduceName(type), unitDisplayName(weapon));
        else std::snprintf(buf, sizeof(buf), "%s", unitProduceName(type));
    } else if (weapon && type != 4 && type != 0x71) {
        std::snprintf(buf, sizeof(buf), "%s/%s", unitDisplayName(type), unitDisplayName(weapon));
    } else {
        std::snprintf(buf, sizeof(buf), "%s", unitDisplayName(type));
    }
    return buf;
}

std::string produceItemCost(uint32_t item, int player) {   // sub_4686E0
    const int type = static_cast<int>(item & 0xFFFF), weapon = static_cast<int>(item >> 16);
    int common = costOf(type, player, 0x0C), rare = costOf(type, player, 0x10);
    if (weapon == 0x49 && type == 0x71) {                    // Scorpion 3-Pack: 3 armas
        common += 3 * costOf(weapon, player, 0x0C); rare += 3 * costOf(weapon, player, 0x10);
    } else if (weapon) {
        common += costOf(weapon, player, 0x0C); rare += costOf(weapon, player, 0x10);
    }
    const std::string name = produceItemName(item);
    char buf[160];
    if (item == 0x66) std::snprintf(buf, sizeof(buf), "%s: %i %s, %i %s", name.c_str(), common, "Common", 10000, "Food");
    else if (rare) std::snprintf(buf, sizeof(buf), "%s: %i %s, %i %s", name.c_str(), common, "Common", rare, "Rare");
    else std::snprintf(buf, sizeof(buf), "%s: %i %s", name.c_str(), common, "Common");
    return buf;
}

// ---- Produção ------------------------------------------------------------

static int ownerLo(const UnitRecord& r) { return r.owner & 0x0F; }
static int ownerHi(const UnitRecord& r) { return (r.owner >> 4) & 0x0F; }   // [+0x1D] sar 4

bool processProducePacket(int owner, int factoryId, int type, int weapon, int group) {   // case 9
    if (!validFactory(factoryId)) return false;
    UnitRecord& f = g_units[factoryId];
    if (ownerLo(f) != (owner & 0x0F)) return false;
    // (Humano: o original ignora o pacote se a fábrica já está no anel de
    // comandos recentes [jogador+0xBD8] — proteção da rede, não portada.)
    if (f.action != 0) return false;                         // [+0x21] == 0
    // Custo pelo bloco do criador (nibble alto de +0x1D): desc+0x14/+0x18.
    const int hb = ownerHi(f);
    int common = costOf(type, hb, 0x0C), rare = costOf(type, hb, 0x10);
    if (weapon) { common += costOf(weapon, hb, 0x0C); rare += costOf(weapon, hb, 0x10); }
    if (type == 0x71) { common += 2 * costOf(weapon, hb, 0x0C); rare += 2 * costOf(weapon, hb, 0x10); }
    if (f.flags & 0x20000u) return false;
    if (!structureOperational(f)) return false;              // vt+0xA0
    PlayerData& pd = g_playerData[ownerLo(f)];
    // Sem minério / comida: som 0x58 p/ o jogador local.
    const bool local = ownerLo(f) == TethysGame::localPlayer();
    if (pd.rareOre < rare || pd.ore < common) { if (local) soundPlay(0x58); return false; }
    if (type == 0x66) {                                      // Food Cargo
        if (pd.foodStored < 10000) { if (local) soundPlay(0x58); return false; }
        pd.foodStored -= 10000;
    }
    if (type >= 0x69 && type <= 0x6B && f.padReserved != 0) return false;   // [+0x74]
    f.action = 9;                                            // [+0x21]
    f.field20 = 1;
    f.field3C = 0;
    f.produceType = static_cast<uint8_t>(type);              // [+0x60]
    f.produceGroup = static_cast<int16_t>(group);            // [+0x6C]
    f.kitWeapon = static_cast<uint8_t>(weapon ? weapon : 0); // [+0x4F]
    pd.rareOre -= rare;                                      // dword_56E300
    pd.ore -= common;                                        // dword_56E2FC
    return true;
}

// sub_46E5C0: custo do tipo no bloco do dono (nibble baixo: comum +0x0C, raro
// +0x10) * 50% — 60% com o GORF melhorado (bloco +0x1D do GORF, nibble alto).
void gorfRecycle(int gorf, int type) {
    const UnitRecord& gr = g_units[gorf];
    const int p = ownerLo(gr);
    const uint8_t* b = unitStatBlock(type, p);
    if (!b) return;
    int common, rare;
    std::memcpy(&common, b + 0x0C, 4);
    std::memcpy(&rare, b + 0x10, 4);
    const uint8_t* gb = unitStatBlock(0x39, ownerHi(gr));
    const int pct = (gb && gb[0x1D]) ? 60 : 50;
    g_playerData[p].ore += common * pct / 100;
    g_playerData[p].rareOre += rare * pct / 100;
}

void sendProduce(int factoryId, uint32_t item) {            // sub_456B50: pacote 9
    CommandPacket pkt;
    pkt.type = CMD_PRODUCE;
    pkt.unitIds.push_back(factoryId);
    pkt.arg0 = static_cast<int>(item & 0xFFFF);             // item
    pkt.arg1 = static_cast<int>(item >> 16);                // arma
    pkt.arg2 = -1;                                          // grupo da IA: nenhum
    commandSubmit(TethysGame::localPlayer(), pkt);
}

// vt[0xFC] (sub_4157B0): Production Capacity do prédio (bloco do criador,
// +0x34) com o bônus de fábrica da moral do dono (dword_56465C).
static int productionRate(const UnitRecord& f) {
    const StructSheetDesc* d = structSheet(f.type);
    const int cap = d ? d->player[ownerHi(f)].productionCapacity : 0;
    const int bonus = moraleRow(g_playerData[ownerLo(f)].morale).factBonus;
    return ((cap * bonus + 0x7F) >> 8) + cap;
}

void factoryProduceStart(int factoryId) {                    // case 9 ([+0x20])
    UnitRecord& f = g_units[factoryId];
    f.field22 = 7;
    // (com o atalho de depuração dword_56DE6C a duração é 1.)
    const int bt = costOf(f.produceType, ownerLo(f), 0x14);  // Build Time (bloco do dono)
    const int prod = bt * productionRate(f);
    int t = (prod + ((prod >> 31) & 0x7F)) >> 7;             // cdq; and 7Fh; sar 7
    if (multiplayerMission()) t = (t - (t >> 31)) >> 1;      // sub_4027D0: /2
    f.field3C = t;
    f.produceTotal = t;                                      // [+0x68]
    f.field20 = 0;
}

void factoryProduceCountdown(int factoryId) {                // sub_43DE50 case 7
    UnitRecord& f = g_units[factoryId];
    if (!structureOperational(f)) return;                    // pausa sem energia/gente
    if (--f.field3C == 0) { f.field23 = f.field22; f.field22 = 0; }
}

// vt[0x100]: guarda o produto. Spaceport (sub_47F600): SULV/RLV/míssil vão p/
// a plataforma (+0x6E); o resto (sub_4156B0) p/ a 1ª baia vazia, com a arma.
static bool factoryStoreProduct(UnitRecord& f, int type) {
    if (f.type == 0x37 && type >= 0x69 && type <= 0x6B) {
        f.padCargo = type;            // (sons 0x80/0x7F p/ o jogador local — áudio não portado)
        return true;
    }
    const StructSheetDesc* d = structSheet(f.type);
    const int bays = d ? d->player[ownerHi(f)].storageBays : 0;
    for (int i = 0; i < bays && i < 6; ++i)
        if (!f.bayType[i]) {
            f.bayType[i] = static_cast<uint8_t>(type);       // [+0x61+i]
            f.bayCargo[i] = f.kitWeapon;                     // [+0x48+i] = [+0x4F]
            return true;
        }
    return false;
}

void factoryProduceFinish(int factoryId) {                   // jpt_43B312 case 9
    UnitRecord& f = g_units[factoryId];
    const int me = TethysGame::localPlayer();
    const int fx = f.pixelX >> 5, fy = f.pixelY >> 5;
    char msg[100];
    if (f.type == 0x30 || f.type == 0x2D) {                  // veículos (loc_43C198)
        int n = 1;
        if (f.produceType == 0x71 || f.produceType == 0x70) { n = 3; f.produceType -= 0x6D; }
        for (int i = 0; i < n; ++i) {
            const int type = g_units[factoryId].produceType;
            if (!type) break;
            int x = fx, y = fy;
            findVehicleSpot(type, x, y);                     // sub_4366D0
            Unit u;
            if (!createUnit(u, static_cast<map_id>(type), LOCATION{x, y}, ownerLo(g_units[factoryId]))) continue;   // sub_4466B0
            UnitRecord& fr = g_units[factoryId];                 // (createUnit pode realocar)
            UnitRecord& v = g_units[u.id()];
            const uint8_t dir = dirOfVec(x - fx, y - fy);    // sub_45C4F0
            v.turretDir = dir & 0xF0;                        // [+0x2D]
            v.rotation = dir & 0xE0;                         // [+0x1C]
            v.curDir = static_cast<uint8_t>((dir & 0xE0) >> 5);
            v.flags |= UNIT_LIGHTS;
            if (v.flags & UNIT_ARMED) v.cargo = fr.kitWeapon; // [+0x24] = arma
            fr.action = 0;
            // sub_4030E0: grupo de IA [+0x6C] (estatística [jogador+0x1C0]++ — pendente).
            if (fr.produceGroup != -1) aiTakeUnit(fr.produceGroup, u.id());
            if (ownerLo(fr) == me && i == 0) {
                const int snd = type == 3 ? 0x65 : type == 4 ? 0x66 : 0x62;
                const std::string name = n == 1 ? unitTitle(v) : (type == 3 ? "Spider Pack" : "Scorpion Pack");
                std::snprintf(msg, sizeof(msg), "%s produced", name.c_str());
                postMessage(unitOX(v), unitOY(v), msg, snd);
            }
        }
        g_units[factoryId].produceType = 0;
        return;
    }
    const int type = f.produceType;
    if (f.type == 0x2E) {                                    // Consumer Factory
        moraleConsumerGoods(ownerLo(f), type);               // sub_44FA10
        if (ownerLo(f) == me) {
            std::snprintf(msg, sizeof(msg), "%s produced", unitDisplayName(type));
            postMessage(unitOX(f), unitOY(f), msg, 0x63);
        }
    } else {
        factoryStoreProduct(f, type);                        // vt[0x100](tipo, -1)
        if (ownerLo(f) == me) {
            int snd = 0x64;
            if (type >= 0x69 && type <= 0x6B) {
                std::snprintf(msg, sizeof(msg), "%s finished and on launch pad", unitDisplayName(type));
                snd = 0;
            } else {
                std::snprintf(msg, sizeof(msg), "%s finished and in storage", unitDisplayName(type));
                if (type == 0x58 || type == 0x59) snd = 0xAF;
                else if (type >= 0x5A && type <= 0x68) snd = 0xAE;
            }
            postMessage(unitOX(f), unitOY(f), msg, snd);
        }
    }
    f.produceType = 0;
    f.action = 0;
}

} // namespace op2
