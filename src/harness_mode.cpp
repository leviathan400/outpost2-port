// --harness: plays a scenario headless, tick by tick, and writes what happened in the
// file formats of the op2-rs test harness (gitlab.com/bufke/op2-rs, harness/), so the
// port can be compared field by field with the original game playing the same recipe.
//
//   outpost2_modern.exe --harness --script S --out DIR --map cm01.map --ticks N
//                       [--seed N] [--techtree multitek.txt] [--trace-every N]
//                       [--shot-every N] [--size WxH] [--show]
//
// The script is the harness's: one `<tick> <verb> <args...>` per line, '#' starts a
// comment; lines at tick T run after the cycle of tick T, as the harness runs them
// (the first cycle is tick 1, so tick 0 lines run there). Coordinates are "file"
// tiles after `coords file` (the port's own tiles) or the original's internal tiles
// otherwise. A unit is `@N`, the Nth spawn, or a unit id.
//
// Outputs in DIR:
//   trace.jsonl   per traced tick: each spawn still alive, with the harness's field
//                 names (px/py/fix/dir/hp/weapon/tur/reload/tgt/lights...) and the RNG
//   harness.log   what ran, spawns, unsupported verbs, the end of the run
//   shot_<tick>.bmp  the game view every --shot-every ticks
//
// The start is the harness's scenario mission (its zScenario.dll), built in: colony
// type, 2 players (0 Eden human, 1 Plymouth computer), tech level 12, daylight
// everywhere. The DLL itself can't run here: it calls Outpost2.exe by address.
#include "harness_mode.h"

#include <SDL.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "app/game_state.h"
#include "game/building_sheet.h"
#include "game/command.h"
#include "game/game_map.h"
#include "game/player.h"
#include "game/scenario.h"
#include "game/tethys_game.h"
#include "game/unit.h"
#include "game/unit_names.h"
#include "gfx/artfile.h"
#include "gfx/renderer.h"
#include "mission/mission.h"
#include "res/res_manager.h"

namespace op2 {
namespace {

struct Options {
    std::string script, out = ".", map = "cm01.map", techtree = "multitek.txt";
    int ticks = 3000, seed = 1, traceEvery = 1, shotEvery = 0, width = 1024, height = 768;
    bool show = false;
};

struct Line {
    int tick = 0;
    std::string verb;
    std::vector<std::string> args;
    bool fileCoords = false;
    int number = 0;
};

FILE* g_log = nullptr;

void logf(const char* fmt, ...) {
    if (!g_log) return;
    va_list a;
    va_start(a, fmt);
    std::vfprintf(g_log, fmt, a);
    va_end(a);
    std::fputc('\n', g_log);
    std::fflush(g_log);
}

// ---- The scenario mission ------------------------------------------------------------

AIModDesc g_desc = { -1, 2, 12, 0, nullptr, "Harness scenario", nullptr, 0 };

int scenarioInitProc() {
    Player[0].GoEden();
    Player[0].GoHuman();
    Player[1].GoPlymouth();
    Player[1].GoAI();
    TethysGame::SetDaylightEverywhere(1);
    return 1;
}
void scenarioAIProc() {}

// ---- Script ----------------------------------------------------------------------------

bool loadScript(const std::string& path, std::vector<Line>& out) {
    std::ifstream f(path);
    if (!f) return false;
    std::string text;
    bool fileCoords = false;
    for (int n = 1; std::getline(f, text); ++n) {
        if (const size_t hash = text.find('#'); hash != std::string::npos) text.resize(hash);
        std::istringstream in(text);
        Line l;
        if (!(in >> l.tick >> l.verb)) continue;
        for (std::string a; in >> a;) l.args.push_back(a);
        if (l.verb == "coords") {
            fileCoords = !l.args.empty() && l.args[0] == "file";
            continue;
        }
        l.fileCoords = fileCoords;
        l.number = n;
        out.push_back(l);
    }
    return true;
}

std::vector<int> g_spawned;   // spawn index -> unit id (0 = failed)
std::vector<int> g_spawnType;
std::vector<bool> g_spawnGone;

// Port tile x from a script x.
int tileX(const Line& l, int x) {
    if (l.fileCoords) return x;
    const int off = g_gameMap.internalXOffset();
    return (off > 0 ? x - off : x) & (g_gameMap.width() - 1);
}

int unitArg(const std::string& a) {
    if (!a.empty() && a[0] == '@') {
        const size_t n = std::strtoul(a.c_str() + 1, nullptr, 10);
        return n < g_spawned.size() ? g_spawned[n] : 0;
    }
    return std::atoi(a.c_str());
}

bool alive(int id) {
    if (id <= 0 || id >= static_cast<int>(g_units.size())) return false;
    const UnitRecord& r = g_units[id];
    return r.id != -1 && !(r.flags & UNIT_DEAD);
}

int typeByName(const std::string& name) {
    if (name.empty() || name == "-") return kMapIdNone;
    const int t = unitTypeFromCodeName(name.c_str());
    return t > 0 ? t : kMapIdNone;
}

// A packed waypoint with the range (how near counts as reached) in its top 3 bits.
uint32_t waypoint(int x, int y, int range) {
    return packLocation(x, y) | (static_cast<uint32_t>(range & 7) << 29);
}

void sendPacket(int id, CommandPacket& p) {
    p.unitIds.assign(1, id);
    processCommandPacket(g_units[id].owner & 0x0F, p);
}

void runLine(GameState& gs, const Line& l, int tick) {
    std::string all;
    for (const std::string& a : l.args) all += " " + a;
    logf("script: tick %d: %s%s", tick, l.verb.c_str(), all.c_str());
    auto arg = [&](size_t i) { return i < l.args.size() ? l.args[i] : std::string(); };
    auto num = [&](size_t i) { return std::atoi(arg(i).c_str()); };

    if (l.verb == "spawn") {     // spawn TYPE owner x y [weapon|-] [dir 0-255]
        const int type = typeByName(arg(0)), weapon = typeByName(arg(4));
        const int dir = l.args.size() > 5 ? (num(5) & 0xFF) : 0;
        int id = 0;
        if (type != kMapIdNone) id = scenarioCreateUnit(type, tileX(l, num(2)), num(3), num(1), weapon, dir / 32);
        if (id && dir % 32) g_units[id].rotation = static_cast<uint8_t>(dir);
        // The harness spawns through TethysAPI's Game::CreateUnit, which turns a vehicle's headlights on after the
        // game's CreateUnit (its lightsOn argument defaults to true).
        if (id && (g_units[id].flags & UNIT_VEHICLE)) unitSetLights(id, 1);
        logf("spawn %u: %s -> unit %d", static_cast<unsigned>(g_spawned.size()), arg(0).c_str(), id);
        g_spawned.push_back(id);
        g_spawnType.push_back(id ? g_units[id].type : type);
        g_spawnGone.push_back(id == 0);
        return;
    }
    if (l.verb == "setcell") {   // setcell x1 y1 x2 y2 cellType (inclusive)
        for (int y = num(1); y <= num(3); ++y)
            for (int x = num(0); x <= num(2); ++x) {
                const int px = tileX(l, x);
                const uint32_t c = g_gameMap.rawCell(px, y);
                g_gameMap.setRawCell(px, y, (c & ~0x1Fu) | (static_cast<uint32_t>(num(4)) & 0x1Fu));
            }
        return;
    }
    if (l.verb == "view") {      // centre player 0's view on a tile
        requestCenterView(tileX(l, num(0)) << 5, num(1) << 5);
        return;
    }
    if (l.verb == "shot" || l.verb == "trace" || l.verb == "telemetry") return;   // captures run on their own

    // Orders: the first argument is the unit.
    const int id = unitArg(arg(0));
    if (!alive(id)) {
        logf("script: tick %d line %d: no live unit %s: skipped", tick, l.number, arg(0).c_str());
        return;
    }
    CommandPacket p;
    if (l.verb == "move" || l.verb == "dock" || l.verb == "dockeg") {
        p.type = l.verb == "move" ? CMD_MOVE : l.verb == "dock" ? CMD_DOCK : CMD_DOCK_EG;
        p.waypoints.push_back(waypoint(tileX(l, num(1)), num(2), 0));
        sendPacket(id, p);
    } else if (l.verb == "movewp" || l.verb == "standground" || l.verb == "patrol") {
        // movewp/standground <unit> x,y[,range] ...; patrol <unit> <loop index> x,y[,range] ...
        const bool patrol = l.verb == "patrol";
        p.type = patrol ? CMD_PATROL : l.verb == "standground" ? CMD_STAND_GROUND : CMD_MOVE;
        for (size_t i = patrol ? 2 : 1; i < l.args.size(); ++i) {
            int x = 0, y = 0, r = 0;
            if (std::sscanf(l.args[i].c_str(), "%d,%d,%d", &x, &y, &r) < 2) continue;
            p.waypoints.push_back(waypoint(tileX(l, x), y, r));
        }
        if (patrol) p.patrolStart = num(1);
        if (!p.waypoints.empty() && p.waypoints.size() <= 8) sendPacket(id, p);
    } else if (l.verb == "attack" || l.verb == "guard") {
        p.type = l.verb == "attack" ? CMD_ATTACK : CMD_GUARD;
        p.target = 0xFFFF0000u | static_cast<uint16_t>(unitArg(arg(1)));
        sendPacket(id, p);
    } else if (l.verb == "attackground") {
        p.type = CMD_ATTACK;
        p.target = (static_cast<uint32_t>(static_cast<uint16_t>(num(2))) << 16) | static_cast<uint16_t>(tileX(l, num(1)));
        sendPacket(id, p);
    } else if (l.verb == "lights") {
        unitSetLights(id, num(1) != 0);
    } else if (l.verb == "stop") {
        p.type = CMD_STOP;
        sendPacket(id, p);
    } else if (l.verb == "selfdestruct") {
        unitDoSelfDestruct(id);
    } else {
        logf("script: tick %d line %d: verb %s is not supported here", tick, l.number, l.verb.c_str());
    }
    (void)gs;
}

// ---- Trace -----------------------------------------------------------------------------

int maxHP(const UnitRecord& r) {
    const int p = (r.owner >> 4) & 0x0F;
    if (r.flags & UNIT_VEHICLE) { const VehPlayerStats* v = vehicleSheet(r.type, p); return v ? v->hitPoints : 0; }
    if (r.flags & UNIT_BUILDING) { const StructSheetDesc* d = structSheet(r.type); return d ? d->player[p].hitPoints : 0; }
    return 0;
}

const char* codeName(int type) {
    const char* n = unitCodeName(type);
    return n ? n : "?";
}

void writeUnit(FILE* f, bool& first, int id, int spawn) {
    const UnitRecord& r = g_units[id];
    const int px = unitOX(r), py = unitOY(r);
    // 16.16 position: the step's 16-bit fraction is biased so 0x8000 is "on px"; it reads 0 before the first step.
    // It lives in the order block (+0x38, the original's path context), so it only counts while there is one.
    long long fx = static_cast<long long>(px) << 16, fy = static_cast<long long>(py) << 16;
    if ((r.flags & UNIT_VEHICLE) && r.hasOrder && (r.fracX | r.fracY)) {
        fx += static_cast<int>(r.fracX) - 0x8000;
        fy += static_cast<int>(r.fracY) - 0x8000;
    }
    std::fprintf(f, "%s{\"id\":%d,\"spawn\":%d,\"type\":\"%s\",\"owner\":%d,\"px\":%d,\"py\":%d,\"sx\":%.6f,\"sy\":%.6f,"
                    "\"fix\":[%lld,%lld],\"dir\":%d,\"hp\":%d,\"actionId\":%d",
                 first ? "" : ",", id, spawn, codeName(r.type), r.owner & 0x0F, px, py, fx / 65536.0, fy / 65536.0, fx, fy,
                 r.rotation, maxHP(r) - r.damage, r.action);
    if (r.flags & UNIT_VEHICLE) std::fprintf(f, ",\"lights\":%d", static_cast<int>(r.flags & UNIT_LIGHTS));
    // Armed vehicles and Guard Posts: the weapon, turret direction, reload and target
    // (high word 0x7FFF = none, 0xFFFF = a unit in the low word, else a tile).
    const int w = r.cargo;
    if (w >= 0x3B && w <= 0x4B && ((r.flags & UNIT_VEHICLE) || (r.flags & UNIT_TURRET_BLD))) {
        std::fprintf(f, ",\"weapon\":\"%s\",\"tur\":%d,\"reload\":%d,\"tgt\":", codeName(w), r.turretDir, r.reload);
        const uint16_t hi = static_cast<uint16_t>(r.field4E), lo = static_cast<uint16_t>(r.target4C);
        if (hi == 0x7FFF) std::fputs("null", f);
        else if (hi == 0xFFFF) std::fprintf(f, "%d", static_cast<int>(lo));
        else std::fprintf(f, "[%d,%d]", static_cast<int>(static_cast<int16_t>(lo)), static_cast<int>(static_cast<int16_t>(hi)));
    }
    std::fputc('}', f);
    first = false;
}

void writeTrace(FILE* f, int tick) {
    std::fprintf(f, "{\"tick\":%d,\"units\":[", tick);
    bool first = true;
    for (size_t i = 0; i < g_spawned.size(); ++i) {
        if (g_spawnGone[i]) continue;
        const int id = g_spawned[i];
        if (!alive(id) || g_units[id].type != g_spawnType[i]) { g_spawnGone[i] = true; continue; }
        writeUnit(f, first, id, static_cast<int>(i));
    }
    std::fprintf(f, "],\"rng\":[%u,%u]}\n", TethysGame::rng().s0(), TethysGame::rng().s1());
    std::fflush(f);
}

// ---- Screenshots -----------------------------------------------------------------------

bool writeBmp(const std::string& path, const uint32_t* px, int w, int h) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const uint32_t stride = static_cast<uint32_t>(w) * 3 + 3 & ~3u, image = stride * h;
    uint8_t hdr[54] = { 'B', 'M' };
    auto put32 = [&](int at, uint32_t v) { std::memcpy(hdr + at, &v, 4); };
    put32(2, 54 + image); put32(10, 54); put32(14, 40); put32(18, w); put32(22, h);
    hdr[26] = 1; hdr[28] = 24; put32(34, image);
    std::fwrite(hdr, 1, 54, f);
    std::vector<uint8_t> row(stride, 0);
    for (int y = h - 1; y >= 0; --y) {
        for (int x = 0; x < w; ++x) {
            const uint32_t c = px[y * w + x];
            row[x * 3] = c & 0xFF; row[x * 3 + 1] = (c >> 8) & 0xFF; row[x * 3 + 2] = (c >> 16) & 0xFF;
        }
        std::fwrite(row.data(), 1, stride, f);
    }
    std::fclose(f);
    return true;
}

void takeShot(GameState& gs, Renderer& renderer, const std::string& out, int tick) {
    gs.onIdle(renderer, 0.0f);   // no time passes: draws the frame (and applies a pending view)
    char name[64];
    std::snprintf(name, sizeof(name), "/shot_%07d.bmp", tick);
    writeBmp(out + name, renderer.getFramebuffer(), renderer.getWidth(), renderer.getHeight());
}

bool parse(int argc, char** argv, Options& o) {
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--harness") continue;
        else if (a == "--script") o.script = next();
        else if (a == "--out") o.out = next();
        else if (a == "--map") o.map = next();
        else if (a == "--techtree") o.techtree = next();
        else if (a == "--ticks") o.ticks = std::atoi(next());
        else if (a == "--seed") o.seed = std::atoi(next());
        else if (a == "--trace-every") o.traceEvery = std::atoi(next());
        else if (a == "--shot-every") o.shotEvery = std::atoi(next());
        else if (a == "--show") o.show = true;
        else if (a == "--size") {
            if (std::sscanf(next(), "%dx%d", &o.width, &o.height) != 2) return false;
        } else {
            std::fprintf(stderr, "--harness: unknown option %s\n", a.c_str());
            return false;
        }
    }
    return true;
}

}  // namespace

int runHarness(int argc, char** argv) {
    Options o;
    if (!parse(argc, argv, o)) return 2;
    g_log = std::fopen((o.out + "/harness.log").c_str(), "w");
    if (!g_log) { std::fprintf(stderr, "--harness: can't write %s/harness.log\n", o.out.c_str()); return 2; }
    logf("harness: port, map=%s ticks=%d seed=%d trace=%d shot=%d", o.map.c_str(), o.ticks, o.seed, o.traceEvery,
         o.shotEvery);
    std::vector<Line> script;
    if (!o.script.empty() && !loadScript(o.script, script)) { logf("FATAL: can't read script %s", o.script.c_str()); return 2; }
    logf("script: %u commands from %s", static_cast<unsigned>(script.size()), o.script.c_str());

    if (SDL_Init(SDL_INIT_VIDEO) != 0) { logf("FATAL: SDL_Init: %s", SDL_GetError()); return 1; }
    g_ResManager.init(".");
    g_art.load();
    Renderer renderer;
    if (!renderer.init("OP2 harness", o.width, o.height, false)) { logf("FATAL: no window"); SDL_Quit(); return 1; }
    if (!o.show)
        if (SDL_Window* w = SDL_GetWindowFromID(1)) SDL_HideWindow(w);

    auto mission = std::make_unique<MissionScript>();
    mission->dllName = "harness scenario";
    mission->mapName = o.map;
    mission->levelDesc = g_desc.levelDesc;
    mission->techtreeName = o.techtree;
    g_desc.mapName = mission->mapName.c_str();
    g_desc.techtreeName = mission->techtreeName.c_str();
    mission->desc = &g_desc;
    mission->initProc = &scenarioInitProc;
    mission->aiProc = &scenarioAIProc;
    GameState gs;
    gs.setSeed(static_cast<uint32_t>(o.seed));
    if (!gs.startMission(std::move(mission))) {
        logf("FATAL: the scenario didn't start (map %s)", o.map.c_str());
        renderer.shutdown(); SDL_Quit();
        return 2;
    }
    logf("start: map %dx%d, x offset %d, players %d, rng [%u,%u]", g_gameMap.width(), g_gameMap.height(),
         g_gameMap.internalXOffset(), TethysGame::noPlayers(), TethysGame::rng().s0(), TethysGame::rng().s1());

    FILE* trace = o.traceEvery > 0 ? std::fopen((o.out + "/trace.jsonl").c_str(), "w") : nullptr;
    size_t pos = 0;
    int tick = 0, nextShot = 5;
    const char* why = "tick limit";
    const Uint32 t0 = SDL_GetTicks();
    for (;;) {
        const bool ended = gs.simTick();
        tick = TethysGame::tick();
        while (pos < script.size() && script[pos].tick <= tick) runLine(gs, script[pos++], tick);
        if (trace && tick % o.traceEvery == 0) writeTrace(trace, tick);
        if (o.shotEvery > 0 && tick >= nextShot) {
            takeShot(gs, renderer, o.out, tick);
            nextShot = (tick / o.shotEvery + 1) * o.shotEvery;
        }
        if (ended) { why = "mission ended"; break; }
        if (o.ticks > 0 && tick >= o.ticks) break;
    }
    const double secs = (SDL_GetTicks() - t0) / 1000.0;
    if (o.shotEvery > 0 && tick % o.shotEvery) takeShot(gs, renderer, o.out, tick);
    logf("quit at tick %d: %s (%.2fs, %.0f ticks/s)", tick, why, secs, tick / (secs > 0 ? secs : 1));
    if (trace) std::fclose(trace);
    std::fclose(g_log);
    renderer.shutdown();
    SDL_Quit();
    return 0;
}

}  // namespace op2
