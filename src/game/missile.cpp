#include "missile.h"
#include "../audio/sound.h"

#include <cstdlib>

#include "building_sheet.h"
#include "colony.h"
#include "game_map.h"
#include "messages.h"
#include "player.h"
#include "structure.h"
#include "tethys_game.h"

namespace op2 {

namespace {

constexpr int kEmpBig = 0x4A, kSULV = 0x69, kRLV = 0x6A, kEMPMissile = 0x6B;
constexpr int kActLaunch = 0x1A;
constexpr int kBlastRadius = 200;      // dword_4DF418
constexpr int kBlastPower = 350;       // dword_4DF41C
constexpr int kBlastLife = 0x1E;       // sub_480590: Kill(0x1E)
constexpr int kBlastAnim = 0x2FF;      // sub_413BC0
constexpr int kShotDownAnim = 0x878;   // sub_4802A0 / sub_4805F0

// sub_480830: [linha * 3 + tipo] (tipo: míssil 0, SULV 1, RLV 2); linha pelo
// sub-estado e a flag 0x4000. dword_4E3FE0 (Eden) / dword_4E4008 (Plymouth).
const int kLaunchAnimE[9] = {0, 0x74C, 0x74F, 0, 0, 0x748, 0, 0x750, 0x752};
const int kLaunchAnimP[9] = {0x74B, 0x74E, 0x74D, 0x751, 0, 0x74A, 0x754, 0x753, 0x752};

bool valid(int id) { return id > 0 && id < static_cast<int>(g_units.size()) && g_units[id].id != -1; }
int ownerOf(const UnitRecord& r) { return r.owner & 0x0F; }
int statOwner(const UnitRecord& r) { return (r.owner >> 4) & 0x0F; }

// Distância em x com a volta do mapa (largura INTERNA, dword_54EBC0).
int wrapAbsPx(int d) {
    const int W = g_gameMap.internalWidth() * 32;
    d = std::abs(d);
    return d > W / 2 ? W - d : d;
}
int wrapAbsTile(int d) {
    const int W = g_gameMap.internalWidth();
    if (-W / 2 > d) d += W;
    else if (W / 2 < d) d -= W;
    return std::abs(d);
}
// Ocupante da célula de x INTERNO (fora da faixa do mapa estreito: vazia).
int occupantInternal(int ix, int y) {
    if (y < 0 || y >= g_gameMap.height()) return 0;
    const int px = (ix & (g_gameMap.internalWidth() - 1)) - g_gameMap.internalXOffset();
    if (px < 0 || px >= g_gameMap.width()) return 0;
    return g_gameMap.cellOccupant(px, y);
}

int launchAnimRaw(const UnitRecord& m) {                     // sub_480830
    const int t = m.type == kSULV ? 1 : m.type == kRLV ? 2 : 0;
    int row;
    if (m.field22 == 9) row = (m.flags & 0x4000u) ? 2 : 1;
    else if (m.field22 == 0x0A) row = (m.flags & 0x4000u) ? 2 : 0;
    else return 0;
    if (m.type == kEMPMissile && m.field22 == 9) row = 1;
    const int ow = ownerOf(m);
    const bool eden = ow < kMaxPlayers && g_playerData[ow].isEden;
    return (eden ? kLaunchAnimE : kLaunchAnimP)[row * 3 + t];
}
int lastFrameOr1(int raw) {
    const int n = raw ? artLastFrameRaw(raw) : 0;
    return n > 0 ? n : 1;
}

// vt[0x84] (sub_480590): EMP_BIG no alvo, dona 6 com as estatísticas (nibble
// alto) do míssil; vt[0x94] guarda o míssil como origem; vive 0x1E ticks.
void launchImpact(int id) {
    const int tx = g_units[id].impactX, ty = g_units[id].impactY;
    const uint8_t hi = g_units[id].owner & 0xF0;
    Unit b;
    if (!createUnit(b, static_cast<map_id>(kEmpBig), LOCATION{tx >> 5, ty >> 5}, 6)) return;
    UnitRecord& e = g_units[b.id()];
    e.rawPixel = true;
    e.pixelX = tx; e.pixelY = ty;
    e.owner = static_cast<uint8_t>(6 | hi);
    e.sourceId = id;                                          // sub_4A27E0
    ++g_units[id].refCount;
    killUnit(b.id(), kBlastLife);
}

// sub_4802A0: interceptação na descida (0xC < [+0x3C] < 0x14) e o aviso.
void launchPeriodic(int id) {
    UnitRecord& m = g_units[id];
    if (m.field22 != 9) return;
    if (m.field3C >= 0x14 || m.field3C <= 0x0C) { m.flags &= ~0x40000u; return; }
    const int mx = m.pixelX >> 5, my = m.pixelY >> 5;
    const int off = g_gameMap.internalXOffset();
    for (int p = 0; p < TethysGame::noPlayers() && p < kMaxPlayers; ++p) {
        if (p == ownerOf(g_units[id])) continue;
        const ColonyState& c = g_colony[p];
        if (!c.stat(ST_OBSERVATORY) || !c.stat(ST_METEOR_DEF)) continue;   // +0x10C / +0x110
        int found = 0, seen = 0;
        for (int b : colonyStructures(p)) {                    // +0xC18
            if (c.stat(ST_METEOR_DEF) <= seen) break;
            const UnitRecord& d = g_units[b];
            if (d.type != 0x2B || !structureOperational(d)) continue;
            const int dy = std::abs(my - (unitOY(d) >> 5));
            const int dx = wrapAbsTile(((unitOX(d) >> 5) + off) - (mx + off));
            if ((dx < dy ? dx : dy) / 2 + (dx > dy ? dx : dy) <= 20) { found = b; break; }
            ++seen;
        }
        if (!found) continue;
        // Observatory "Improved" do dono da defesa (off_4E140C, bloco +0x1D).
        const uint8_t* obs = unitStatBlock(0x33, ownerOf(g_units[found]));
        const int chance = (obs && obs[0x1D]) ? 75 : 50;
        UnitRecord& mm = g_units[id];
        if (TethysGame::getRand(100) < chance) {
            mm.flags |= 0x40000u | 0x10000u;
            mm.launchDefense = found;
            killUnit(id, artLastFrameRaw(kShotDownAnim));
        } else {
            mm.flags |= 0x40000u;
            mm.launchDefense = found;
        }
    }
    // "Warning: Incoming Missile": unidade danificável do jogador local a menos
    // de r (raio da EMP_BIG arredondado p/ tiles) em x e até r em y.
    const WeaponSheetDesc* wd = weaponSheet(kEmpBig);
    const int r = (((wd ? wd->radius : 0) + 0x1F) >> 5) << 5;
    const UnitRecord& mm = g_units[id];
    const int px = mm.pixelX, py = mm.pixelY;
    for (int u = unitListFirst(); u != 0; u = unitListNext(u)) {
        if (u == id) continue;
        const UnitRecord& t = g_units[u];
        const int uy = unitOY(t), ux = unitOX(t);
        if (uy < py - r || uy > py + r) continue;
        if (ux <= px - r || ux >= px + r) continue;
        if (!(t.flags & UNIT_DAMAGEABLE) || ownerOf(t) != TethysGame::localPlayer()) continue;
        postMessage(px, py, "Warning: Incoming Missile", 0x6E);
        break;
    }
}

// Raio atual do anel e nº de pontos (sub_413DF0 / sub_413BC0): R = (0x1E −
// [+0x3C]) * 8; pontos = 2^c >= max(628R/1500, 3), c >= 3 se passar de 4.
int blastRing(const UnitRecord& e, int& count, int& step) {
    const int R = (kBlastLife - e.field3C) * 8;
    int n = R * 628 / 1500;
    if (n <= 3) n = 3;
    int c = 2;
    if (n > 4) do { ++c; } while ((1 << c) < n);
    count = 1 << c;
    step = 0x100 >> c;
    return R;
}

// sub_413DF0: EMP nas unidades sob o anel (±1 tile de cada ponto).
void blastPeriodic(int id) {
    int count, step;
    const UnitRecord& e0 = g_units[id];
    const int R = blastRing(e0, count, step);
    const int bx = e0.pixelX, by = e0.pixelY;
    const int off = g_gameMap.internalXOffset() * 32;
    const int src = statOwner(e0);
    for (int k = 0, a = 0; k < count; ++k, a += step) {
        const int tx = ((bx + off) + (dirCos(static_cast<uint8_t>(a)) * R >> 6)) >> 5;
        const int ty = (by + (dirSin(static_cast<uint8_t>(a)) * R >> 6)) >> 5;
        for (int x = tx - 1; x <= tx + 1; ++x) {
            const int y0 = ty - 1 > 0 ? ty - 1 : 0;
            const int y1 = ty + 1 < g_gameMap.height() - 1 ? ty + 1 : g_gameMap.height() - 1;
            for (int y = y0; y <= y1; ++y) {
                const int uid = occupantInternal(x, y);
                if (!valid(uid)) continue;
                const UnitRecord& t = g_units[uid];
                if ((t.flags & UNIT_DEAD) || !(t.flags & UNIT_DAMAGEABLE) || (t.flags & UNIT_EMP)) continue;
                const int sp = statOwner(t);
                int armor = 0;
                if (t.flags & UNIT_BUILDING) {
                    const StructSheetDesc* d = structSheet(t.type);
                    if (!d || !d->player[sp].power) continue;          // bloco +0x24: só quem usa energia
                    armor = d->player[sp].armor;
                } else if (const VehPlayerStats* v = vehicleSheet(t.type, sp)) {
                    armor = v->armor;                                  // bloco +0x08
                }
                if (t.type == 3 || t.type == 4) continue;              // Spider / Scorpion
                const int dy = std::abs(by - unitOY(t));
                const int dx = wrapAbsPx(bx - unitOX(t));
                const int d = (dx < dy ? dx : dy) / 2 + (dx > dy ? dx : dy);
                if (kBlastRadius <= d) continue;
                if (armor > (kBlastRadius * 2 - d) * kBlastPower / kBlastRadius) continue;
                const bool allied = (g_playerData[ownerOf(t)].allyMask >> src) & 1u;
                unitApplyEMP(uid, allied ? 0x20 : 0x60);
            }
        }
    }
}

} // namespace

bool isLaunchUnit(int type) { return type >= kSULV && type <= kEMPMissile; }
bool isEmpBlast(int type) { return type == kEmpBig; }

// sub_4466B0(tipo, px, py, dono, carga): unidade de lançamento no pixel exato,
// alvo +0x34/+0x38, ação 0x1A.
static int createLaunchUnit(int type, int px, int py, int owner, int cargo, int tx, int ty) {
    Unit u;
    if (!createUnit(u, static_cast<map_id>(type), LOCATION{px >> 5, py >> 5}, owner)) return 0;
    UnitRecord& m = g_units[u.id()];
    m.rawPixel = true;
    m.pixelX = px; m.pixelY = py;
    m.cargo = static_cast<uint16_t>(cargo);
    m.impactX = tx; m.impactY = ty;                           // +0x34 / +0x38
    m.action = kActLaunch; m.field20 = 1; m.field3C = 0;
    return u.id();
}

int setEMPMissile(int x, int y, int owner, int tx, int ty) {   // ?SetEMPMissile
    return createLaunchUnit(kEMPMissile, x << 5, y << 5, owner, 0, tx << 5, ty << 5);
}

// Ponto de lançamento do Spaceport: [+0x14]/[+0x18] + offset do frame atual (sub_404DF0).
static void launchPoint(const UnitRecord& sp, int& x, int& y) {
    x = unitOX(sp); y = unitOY(sp);
    const int a = structureCurAnim(sp);
    if (a < 0) return;
    if (const int8_t* off = artFrameOffsets(a + 0x65, structureFrame(sp))) { x += off[0]; y += off[1]; }
}

bool spaceportLaunchAvailable(int sp) {                     // sub_4572F0
    if (!valid(sp)) return false;
    const UnitRecord& s = g_units[sp];
    if (!structureOperational(s) || !s.padCargo) return false;
    if (s.padPayload == 0 && s.padCargo == kSULV) return false;
    if (s.padPayload == 0x67) {
        const PlayerData& d = g_playerData[TethysGame::localPlayer()];
        if (d.workers + d.scientists + d.kids < 200) return false;
    }
    return true;
}

bool spaceportPadSwapAvailable(int sp, int bay) {           // loc_456DD0
    if (!valid(sp) || bay < 0 || bay > 5) return false;
    const UnitRecord& s = g_units[sp];
    if (!s.padCargo || s.padCargo == kEMPMissile) return false;
    const int8_t t = static_cast<int8_t>(s.bayType[bay]);
    if (!t && s.padPayload == 0) return false;
    return s.padPayload != t;
}

bool spaceportPadSwap(int owner, int sp, int bay) {         // case 10 (+0x12 = 1) → sub_47F470
    if (!valid(sp) || bay < 0 || bay > 5) return false;
    UnitRecord& s = g_units[sp];
    if (ownerOf(s) != (owner & 0x0F) || (s.flags & UNIT_DEAD)) return false;
    if (!s.padCargo) return false;
    const int8_t old = static_cast<int8_t>(s.padPayload);
    s.padPayload = static_cast<int8_t>(s.bayType[bay]);
    s.bayType[bay] = static_cast<uint8_t>(old);
    return true;
}

bool spaceportLaunch(int sp, int tx, int ty) {               // case 25
    if (!valid(sp)) return false;
    UnitRecord& s = g_units[sp];
    if (!s.padCargo) return false;
    if (s.padPayload == 0 && s.padCargo == kSULV) return false;   // SULV sem carga
    if (s.padPayload == 0x67) {                                  // Evacuation Module: 200 colonos
        const PlayerData& d = g_playerData[ownerOf(s)];
        if (d.workers + d.scientists + d.kids < 200) return false;
    }
    if (s.flags & UNIT_DEAD) return false;
    if (!structureOperational(s)) return false;
    int x, y;                                                    // sub_424140
    launchPoint(s, x, y);
    const int type = s.padCargo, payload = s.padPayload, owner = ownerOf(s);
    if (!createLaunchUnit(type, x, y, owner, payload, tx, ty)) return true;
    UnitRecord& s2 = g_units[sp];
    s2.padCargo = 0;
    s2.padPayload = 0;
    return true;
}

// vt[0x7C] sub_47FC60: a carga chega em órbita; RLV soma 1 ao contador.
static void launchInOrbit(int id) {
    const UnitRecord& m = g_units[id];
    const int p = ownerOf(m);
    if (p >= kMaxPlayers) return;
    PlayerData& d = g_playerData[p];
    const bool local = p == TethysGame::localPlayer();
    const int cargo = static_cast<int16_t>(m.cargo);
    if (cargo == 0x58) {                                         // EDWARD: bits 9..12
        int n = ((d.spaceBits >> 9) & 0xF) + 1;
        if (n > 15) n = 15;
        d.spaceBits = (d.spaceBits & ~0x1E00u) | (static_cast<uint32_t>(n) << 9);
        if (local) soundVoice(0x7B, -1, -1);
    } else if (cargo == 0x59) {                                  // Solar Satellite
        d.solarSat = d.solarSat + 1 > 15 ? 15 : d.solarSat + 1;
        if (local) soundVoice(0x7C, -1, -1);
    } else if (cargo == 0x67) {
        d.spaceBits |= 1u << (cargo - 0x4D);           // Evacuation Module
        colonyRemoveColonists(p, 200, true);              // sub_471BD0(200, 1)
    } else if (cargo >= 0x5A && cargo <= 0x68) {
        if (local) soundVoice(cargo == 0x68 ? 0xE3 : 0xD6 + (cargo - 0x5A), -1, -1);
        d.spaceBits |= 1u << (cargo - 0x4D);
    }
    if (m.type == kRLV) d.rlvCount = d.rlvCount + 1 > 15 ? 15 : d.rlvCount + 1;
}

// sub_47FB90: o Spaceport aceita o RLV de volta (reserva +0x74; contador − 1).
static bool spaceportReserve(UnitRecord& s) {
    if (!structureOperational(s) || s.padCargo || s.padReserved) return false;
    if (s.produceType == kSULV || s.produceType == kRLV || s.produceType == kEMPMissile) return false;
    s.padReserved = 1;
    PlayerData& d = g_playerData[ownerOf(s)];
    d.rlvCount = (d.rlvCount - 1) & 0xF;
    return true;
}

// vt[0x80]: míssil → alvo (sub_480280); RLV → Spaceport do dono (sub_480150); SULV → não volta.
static bool launchReturn(int id) {
    UnitRecord& m = g_units[id];
    if (m.type == kEMPMissile) { m.pixelX = m.impactX; m.pixelY = m.impactY; return true; }
    if (m.type != kRLV) return false;
    const int p = ownerOf(m);
    if (p >= kMaxPlayers) return false;
    for (int b : colonyStructures(p)) {
        UnitRecord& s = g_units[b];
        if (s.type != 0x37 || !spaceportReserve(s)) continue;
        int x, y;
        launchPoint(s, x, y);
        UnitRecord& r = g_units[id];
        r.pixelX = x; r.pixelY = y;
        return true;
    }
    return false;
}

// vt[0x84]: míssil → EMP_BIG; RLV pousa (sub_4801E0): plataforma = RLV, "RLV Ready".
static void launchLand(int id) {
    UnitRecord& m = g_units[id];
    if (m.type == kEMPMissile) { launchImpact(id); return; }
    if (m.type != kRLV) return;
    const int x = (m.pixelX >> 5) & (g_gameMap.width() - 1), y = m.pixelY >> 5;
    const int occ = g_gameMap.cellOccupant(x, y);
    if (!valid(occ)) return;
    UnitRecord& s = g_units[occ];
    s.padCargo = kRLV;
    s.padReserved = 0;
    if (ownerOf(m) == TethysGame::localPlayer()) postMessage(unitOX(s), unitOY(s), "RLV Ready", 0x7F);
}

void playerCaptureRLV(int p, int from) {                     // ?CaptureRLV
    if (p < 0 || p >= kMaxPlayers || from < 0 || from >= kMaxPlayers) return;
    PlayerData& src = g_playerData[from];
    if (!src.rlvCount) return;
    src.rlvCount = (src.rlvCount - 1) & 0xF;
    PlayerData& dst = g_playerData[p];
    dst.rlvCount = dst.rlvCount + 1 > 15 ? 15 : dst.rlvCount + 1;
    for (int u = unitListFirst(); u; u = unitListNext(u)) {
        UnitRecord& r = g_units[u];
        if (r.type == kRLV && ownerOf(r) == from) {
            r.owner = static_cast<uint8_t>((r.owner & 0xF0) | p);
            return;
        }
    }
}

void missileActionStart(int id) {
    UnitRecord& m = g_units[id];
    m.field22 = 0x0A;
    m.field3C = lastFrameOr1(launchAnimRaw(m));
    soundPlayAt(m.pixelX, m.pixelY, 0x41);
    const bool local = ownerOf(m) == TethysGame::localPlayer();
    if (m.type == kSULV || m.type == kRLV) {
        if (local) soundVoice(m.type == kSULV ? 0x7D : 0x7E, -1, -1);
    } else if (m.type == kEMPMissile) {
        if (local) soundVoice(0x7D, -1, -1);
        else postMessage(m.pixelX, m.pixelY, "Missile Launch Detected", 0x6D);
    }
    m.field20 = 0;
}

void missileActionTick(int id) {
    UnitRecord& m = g_units[id];
    switch (m.field23) {
        case 6:                                                // vt[0x80]
            if (!launchReturn(id)) break;
            g_units[id].field22 = 9;
            g_units[id].field3C = 0x2D;
            break;
        case 9:
            if (m.flags & 0x4000u) {
                m.flags &= ~0x4000u;
                m.field22 = 9;
                m.field3C = lastFrameOr1(launchAnimRaw(m));
            } else {
                launchLand(id);                                // vt[0x84]
                g_units[id].action = ACT_DIE;
            }
            break;
        case 0x0A:
            if (!(m.flags & 0x4000u)) {
                m.field22 = 0x0A;
                m.pixelY -= 0x9B;
                m.field3C = 0x2D;
                m.flags |= 0x4000u;
            } else {
                launchInOrbit(id);                             // vt[0x7C] sub_47FC60
                if (m.type == kSULV) killUnit(id, 1);
                else {
                    m.field22 = SUB_WAIT;
                    m.field3C = TethysGame::cheatFlag(4) ? 0x0C : (m.type == kRLV ? 0x4B0 : 100);
                }
            }
            break;
        default: break;
    }
}

bool missilePeriodicTick(int id) {
    const int t = g_units[id].type;
    if (t == kEMPMissile) { launchPeriodic(id); return true; }   // (SULV/RLV: vt[0x50] nulo)
    if (isLaunchUnit(t)) return true;
    if (isEmpBlast(t)) { blastPeriodic(id); return true; }
    return false;
}

void missileDrawOps(int id, std::vector<WeaponDrawOp>& ops) {
    const UnitRecord& m = g_units[id];
    auto sprite = [&](int raw, int frame, int x, int y) {
        WeaponDrawOp op;
        op.anim = raw - 0x65; op.frame = frame; op.x = x; op.y = y;
        ops.push_back(op);
    };
    const int now = TethysGame::tick();
    if (isEmpBlast(m.type)) {                                  // sub_413BC0
        int count, step;
        const int R = blastRing(m, count, step);
        const int wob = step * 13;
        const int nF = artLastFrameRaw(kBlastAnim) + 1;
        for (int k = 0, a = 0, w = 0; k < count; ++k, a += step, w += wob) {
            const int rr = (dirCos(static_cast<uint8_t>(w & 0xFF)) * R >> 10) + R;
            const int c = dirCos(static_cast<uint8_t>(a));
            sprite(kBlastAnim, nF > 0 ? (now + c) % nF : 0, m.pixelX + (c * rr >> 6),
                   m.pixelY + (dirSin(static_cast<uint8_t>(a)) * rr >> 6));
        }
        return;
    }
    if (!isLaunchUnit(m.type)) return;
    if (m.action != ACT_DIE || m.type != kEMPMissile) {         // sub_480900
        const int raw = launchAnimRaw(m);
        if (raw) {
            const int last = artLastFrameRaw(raw);
            int frame = last - m.field3C, y = m.pixelY;
            if (m.field22 == 0x0A && (m.flags & 0x4000u)) { y += m.field3C * 20 - 900; frame = now % (last + 1); }
            if (m.field22 == 9 && (m.flags & 0x4000u)) { y -= m.field3C * 20; frame = now % (last + 1); }
            sprite(raw, frame, m.pixelX, y);
        }
    } else {                                                    // derrubado
        sprite(kShotDownAnim, artLastFrameRaw(kShotDownAnim) - m.field3C, m.pixelX, m.pixelY - m.field3C * 20);
    }
    if (m.type == kEMPMissile && (m.flags & 0x40000u) && valid(m.launchDefense)) {
        // Laser da defesa (sub_41E8F0, 0x7C0000): 32 px adiante se errou.
        const int lead = (m.flags & 0x10000u) ? 0 : 32;
        const UnitRecord& d = g_units[m.launchDefense];
        WeaponDrawOp op;
        op.kind = WeaponDrawOp::LASER_LINE;
        op.x = unitOX(d); op.y = unitOY(d);
        op.x2 = m.pixelX + lead; op.y2 = m.pixelY - m.field3C * 20 - lead;
        ops.push_back(op);
    }
}

void unitApplyEMP(int id, int ticks) {
    UnitRecord& u = g_units[id];
    if ((u.flags & UNIT_VEHICLE) && (u.flags & UNIT_ARMED)) {   // sub_484C50: larga guarda/ordem/alvo
        auto drop = [](int r) {
            if (r > 0 && r < static_cast<int>(g_units.size()) && g_units[r].refCount) --g_units[r].refCount;
        };
        if (u.guardHi & 0x8000) drop(u.guardLo);
        u.guardHi = 0x7FFF;
        if (u.field4A & 0x8000) drop(u.orderLo);
        u.field4A = 0x7FFF;
        if (u.field4E & 0x8000) drop(u.target4C);
        u.field4E = 0x7FFF;
    }
    if (u.flags & UNIT_VEHICLE) {                               // sub_44B5F0
        u.flags |= UNIT_EMP;
        u.flags &= ~UNIT_LIGHTS;
        if (u.field22 != 1) u.field22 = 0x0A;
        u.empTicks = static_cast<int16_t>(ticks);
        u.action = 0;
        u.field20 = 0;
    } else if (u.flags & UNIT_BUILDING) {                       // sub_4836E0
        u.flags |= UNIT_EMP;
        u.empTicks = static_cast<int16_t>(ticks);
        colonyMarkDirty(ownerOf(u));
    }
}

} // namespace op2
