#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <set>
#include "gfx/renderer.h"
#include "shell/shell_menu.h"
#include "core/config.h"
#include "res/res_manager.h"
#include "game/game_map.h"
#include "game/terrain_renderer.h"
#include "game/game_rng.h"
#include "game/player.h"
#include "game/unit.h"
#include "game/tethys_game.h"
#include "game/pathfinder.h"
#include "game/command.h"
#include "gfx/artfile.h"
#include "app/tapp.h"
#include "app/game_state.h"

// --unittest: exercita o pool de unidades + o wrapper Unit.
static int runUnitTest() {
    op2::resetUnits();
    op2::Unit b, v;
    op2::TethysGame::CreateUnit(b, op2::kFirstBuilding, op2::LOCATION{10, 10}, 0);
    op2::TethysGame::CreateUnit(v, 1, op2::LOCATION{12, 10}, 1);
    auto show = [](const char* nm, op2::Unit& u) {
        op2::LOCATION l = u.Location();
        std::printf("%s id=%d live=%d type=%d owner=%d loc=(%d,%d) bld=%d veh=%d\n",
                    nm, u.id(), u.IsLive(), u.GetType(), u.OwnerID(), l.x, l.y,
                    u.IsBuilding(), u.IsVehicle());
    };
    show("building", b);
    show("vehicle ", v);
    v.DoDeath();   // marca morta (flag 0x20000); slot continua ocupado (fiel)
    std::printf("apos DoDeath: vehicle live=%d (morta, mas slot nao e liberado)\n", v.IsLive());
    op2::Unit v2;
    op2::TethysGame::CreateUnit(v2, 2, op2::LOCATION{5, 5}, 0);
    std::printf("novo veiculo id=%d type=%d (novo slot, pois morta != removida)\n",
                v2.id(), v2.GetType());
    return 0;
}

// --playertest: exercita a API de _Player (wrapper de índice + array global).
static int runPlayerTest() {
    op2::resetPlayers(4);
    op2::Player[0].GoEden();     op2::Player[0].SetOre(5000); op2::Player[0].SetWorkers(16);
    op2::Player[1].GoPlymouth(); op2::Player[1].SetOre(1234); op2::Player[1].GoAI();
    op2::Player[0].SetSolarSat(99);  // deve saturar em 15
    for (int i = 0; i < 2; ++i) {
        const op2::_Player& p = op2::Player[i];
        std::printf("Player %d: %s %s  Ore=%d Workers=%d SolarSat=%d\n",
                    i, p.IsEden() ? "EDEN" : "PLYMOUTH", p.IsHuman() ? "HUMAN" : "AI",
                    p.Ore(), p.Workers(), p.SolarSat());
    }
    std::printf("P0.Ore=%d  P1.Ore=%d (independentes)  P0.SolarSat(clamp)=%d\n",
                op2::Player[0].Ore(), op2::Player[1].Ore(), op2::Player[0].SolarSat());
    return 0;
}

// --rngtest <seedHex> <count>: imprime a sequência do RNG (next cru e getRand),
// para validar o port contra uma transcrição independente do x86.
static int runRngTest(const char* seedStr, const char* countStr) {
    uint32_t seed = static_cast<uint32_t>(strtoul(seedStr, nullptr, 0));
    int count = atoi(countStr);
    op2::GameRNG rng;
    rng.setSeed(seed);
    std::printf("seed=0x%08X  s0=0x%08X s1=0x%08X\n", seed, rng.s0(), rng.s1());
    op2::GameRNG r2; r2.setSeed(seed);
    for (int i = 0; i < count; ++i) {
        uint32_t raw = rng.next();
        int rnd = r2.getRand(100);
        std::printf("%3d  next=0x%08X  getRand(100)=%d\n", i, raw, rnd);
    }
    return 0;
}

// Modo utilitário (sem SDL): extrai um recurso via ResManager e grava em disco.
// Usado para validar o leitor de VOL/LZHUF contra o decoder Python (byte a byte).
//   outpost2_modern.exe --extract <nome_no_vol> <arquivo_saida>
static int runExtract(const std::string& name, const std::string& outPath) {
    if (!op2::g_ResManager.init(".")) return 2;
    auto stream = op2::g_ResManager.openStream(name);
    if (!stream) {
        std::cerr << "[Extract] recurso nao encontrado: " << name << std::endl;
        return 3;
    }
    std::ofstream out(outPath, std::ios::binary);
    if (!out) {
        std::cerr << "[Extract] nao foi possivel criar: " << outPath << std::endl;
        return 4;
    }
    out.write(reinterpret_cast<const char*>(stream->data()),
              static_cast<std::streamsize>(stream->size()));
    std::cout << "[Extract] " << name << " -> " << outPath
              << " (" << stream->size() << " bytes)" << std::endl;
    return 0;
}

// Grava um buffer ARGB (w x h, top-down) como BMP 32-bit.
static bool writeBmp32(const std::string& path, const uint32_t* px, int w, int h) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    uint8_t hdr[54] = {
        'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0,
        40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 32,0,
        0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0
    };
    uint32_t fileSize = 54 + static_cast<uint32_t>(w) * h * 4;
    memcpy(&hdr[2], &fileSize, 4);
    memcpy(&hdr[18], &w, 4);
    int negH = -h;  // top-down
    memcpy(&hdr[22], &negH, 4);
    fwrite(hdr, 1, 54, f);
    fwrite(px, 4, static_cast<size_t>(w) * h, f);
    fclose(f);
    return true;
}

// Modo utilitário (sem SDL): renderiza um mapa inteiro com terreno para um BMP.
//   outpost2_modern.exe --rendermap <nome.map> <saida.bmp>
static int runRenderMap(const std::string& mapName, const std::string& outPath) {
    if (!op2::g_ResManager.init(".")) return 2;
    auto mapStream = op2::g_ResManager.openStream(mapName);
    if (!mapStream) { std::cerr << "[RenderMap] mapa nao encontrado: " << mapName << std::endl; return 3; }
    if (!op2::g_gameMap.load(mapStream->data(), mapStream->size())) {
        std::cerr << "[RenderMap] .map invalido: " << mapName << std::endl; return 4;
    }
    op2::TerrainRenderer terrain;
    if (!terrain.init(op2::g_gameMap, op2::g_ResManager)) {
        std::cerr << "[RenderMap] falha ao carregar tilesets" << std::endl; return 5;
    }
    int W = terrain.mapPixelWidth();
    int H = terrain.mapPixelHeight();
    std::vector<uint32_t> buf(static_cast<size_t>(W) * H, 0xFF000000u);
    terrain.render(buf.data(), W, H, 0, 0);
    if (!writeBmp32(outPath, buf.data(), W, H)) {
        std::cerr << "[RenderMap] nao foi possivel gravar: " << outPath << std::endl; return 6;
    }
    std::cout << "[RenderMap] " << mapName << " -> " << outPath
              << " (" << W << "x" << H << ", "
              << op2::g_gameMap.width() << "x" << op2::g_gameMap.height() << " tiles, "
              << op2::g_gameMap.tilesets().size() << " tilesets)" << std::endl;
    return 0;
}

// Visualizador de terreno em-engine: abre a janela e rola o mapa com as setas.
//   outpost2_modern.exe --game <nome.map>
static int runGameView(const std::string& mapName) {
    if (!op2::g_ResManager.init(".")) return 2;
    auto mapStream = op2::g_ResManager.openStream(mapName);
    if (!mapStream || !op2::g_gameMap.load(mapStream->data(), mapStream->size())) {
        std::cerr << "[GameView] mapa invalido: " << mapName << std::endl; return 3;
    }
    op2::TerrainRenderer terrain;
    if (!terrain.init(op2::g_gameMap, op2::g_ResManager)) {
        std::cerr << "[GameView] falha ao carregar tilesets" << std::endl; return 4;
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init falhou: " << SDL_GetError() << std::endl; return 5;
    }
    op2::Renderer renderer;
    if (!renderer.init("Outpost 2 - Terreno (setas p/ rolar)", 1280, 960, false)) {
        SDL_Quit(); return 6;
    }

    const int VW = op2::Renderer::NATIVE_WIDTH;
    const int VH = op2::Renderer::NATIVE_HEIGHT;
    int maxX = terrain.mapPixelWidth() - VW;
    int maxY = terrain.mapPixelHeight() - VH;
    if (maxY < 0) maxY = 0;
    // começa no clipRect (área jogável), como o jogo
    int camX = op2::g_gameMap.clipRect().x1 * 32;
    int camY = op2::g_gameMap.clipRect().y1 * 32;

    bool running = true;
    uint32_t last = SDL_GetTicks();
    while (running) {
        uint32_t now = SDL_GetTicks();
        float dt = (now - last) / 1000.0f;
        last = now;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;
        }
        const Uint8* ks = SDL_GetKeyboardState(nullptr);
        int speed = static_cast<int>(600 * dt);
        if (speed < 1) speed = 1;
        if (ks[SDL_SCANCODE_LEFT])  camX -= speed;
        if (ks[SDL_SCANCODE_RIGHT]) camX += speed;
        if (ks[SDL_SCANCODE_UP])    camY -= speed;
        if (ks[SDL_SCANCODE_DOWN])  camY += speed;
        if (camY < 0) camY = 0;
        if (camY > maxY) camY = maxY;
        // X faz wrap no engine; deixamos rolar livremente.

        renderer.clear(0xFF000000);
        terrain.render(renderer.getFramebuffer(), VW, VH, camX, camY);
        renderer.present();

        uint32_t frame = SDL_GetTicks() - now;
        if (frame < 16) SDL_Delay(16 - frame);
    }
    renderer.shutdown();
    SDL_Quit();
    return 0;
}

// Modo de captura (um frame de uma tela do shell), para screenshots.
//   outpost2_modern.exe [--menu|--tutorials|--campaign|--prefs|--colony|--about] --screenshot <out.bmp>
static int runScreenshot(int argc, char* argv[], const std::string& outPath) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { std::cerr << "SDL_Init: " << SDL_GetError() << std::endl; return 1; }
    op2::g_ResManager.init(".");
    op2::Renderer renderer;
    if (!renderer.init("OP2 screenshot", 1280, 960, false)) { SDL_Quit(); return 1; }
    op2::ShellMenu shellMenu;
    shellMenu.init("OP2SHRES.DLL");
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--tutorials") shellMenu.showTutorials();
        else if (a == "--menu") shellMenu.showMainMenu();
        else if (a == "--campaign") shellMenu.showNewCampaign();
        else if (a == "--prefs") shellMenu.showPreferences();
        else if (a == "--colony") shellMenu.showColonyGames();
        else if (a == "--about") shellMenu.showAbout();
    }
    renderer.clear(0xFF000000);
    shellMenu.render(renderer);
    bool ok = writeBmp32(outPath, renderer.getFramebuffer(),
                         op2::Renderer::NATIVE_WIDTH, op2::Renderer::NATIVE_HEIGHT);
    std::cout << "[Screenshot] " << (ok ? "salvo em " : "falha ao salvar ") << outPath << std::endl;
    renderer.shutdown();
    SDL_Quit();
    return ok ? 0 : 1;
}

// --playshot <map> <out.bmp>: carrega o jogo, roda alguns frames e salva o
// framebuffer (terreno + unidades + HUD) para inspeção visual.
static int runPlayShot(const std::string& mapName, const std::string& outPath, bool passOverlay) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { std::cerr << "SDL_Init: " << SDL_GetError() << std::endl; return 1; }
    op2::g_ResManager.init(".");
    op2::Renderer renderer;
    if (!renderer.init("OP2 playshot", 1280, 960, false)) { SDL_Quit(); return 1; }
    op2::GameState gs;
    if (!gs.load(mapName)) { renderer.shutdown(); SDL_Quit(); return 2; }
    gs.onActivate();
    gs.setShowPassability(passOverlay);
    for (int i = 0; i < 5; ++i) gs.onIdle(renderer, 0.1f);   // avança alguns ticks
    bool ok = writeBmp32(outPath, renderer.getFramebuffer(),
                         op2::Renderer::NATIVE_WIDTH, op2::Renderer::NATIVE_HEIGHT);
    std::cout << "[PlayShot] " << (ok ? "salvo em " : "falha ") << outPath << std::endl;
    renderer.shutdown();
    SDL_Quit();
    return ok ? 0 : 1;
}

// --movecap <map> <prefix>: carrega o jogo, manda os veículos do jogador moverem
// e salva frames (prefix_NN.bmp) durante o movimento, para inspeção visual.
static int runMoveCap(const std::string& mapName, const std::string& prefix) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { std::cerr << "SDL_Init: " << SDL_GetError() << std::endl; return 1; }
    op2::g_ResManager.init(".");
    op2::Renderer renderer;
    if (!renderer.init("OP2 movecap", 1280, 960, false)) { SDL_Quit(); return 1; }
    op2::GameState gs;
    if (!gs.load(mapName)) { renderer.shutdown(); SDL_Quit(); return 2; }
    gs.onActivate();

    // Destino ~12 células a leste do 1º veículo; manda todos os veículos p/ lá.
    int mask = op2::g_gameMap.width() - 1;
    int refX = 0, refY = 0, found = 0;
    for (int i = 1; i < (int)op2::g_units.size(); ++i)
        if (op2::g_units[i].id != -1 && (op2::g_units[i].flags & op2::UNIT_VEHICLE)) {
            refX = op2::g_units[i].pixelX >> 5; refY = op2::g_units[i].pixelY >> 5; found = 1; break;
        }
    if (found) {
        op2::LOCATION dst = op2::g_gameMap.findPassableNear(op2::LOCATION{ (refX + 12) & mask, refY + 4 });
        dst.x &= mask;
        for (int i = 1; i < (int)op2::g_units.size(); ++i) {
            op2::UnitRecord& r = op2::g_units[i];
            if (r.id == -1 || !(r.flags & op2::UNIT_VEHICLE)) continue;
            op2::Unit(i).DoMove(dst);
        }
        std::cout << "[MoveCap] destino (" << dst.x << "," << dst.y << ")" << std::endl;
    }

    for (int f = 0; f <= 80; ++f) {
        gs.onIdle(renderer, 0.1f);     // 1 tick + render por frame
        if (f == 0 || f == 20 || f == 40 || f == 60 || f == 80) {
            char path[256];
            std::snprintf(path, sizeof(path), "%s_%02d.bmp", prefix.c_str(), f);
            writeBmp32(path, renderer.getFramebuffer(),
                       op2::Renderer::NATIVE_WIDTH, op2::Renderer::NATIVE_HEIGHT);
            std::cout << "[MoveCap] frame " << f << " -> " << path << std::endl;
        }
    }
    renderer.shutdown();
    SDL_Quit();
    return 0;
}

// --colltest <map>: verifica colisao com terreno impassavel (pathfinder + passo).
static int runCollTest(const std::string& mapName) {
    if (!op2::g_ResManager.init(".")) return 2;
    auto ms = op2::g_ResManager.openStream(mapName);
    if (!ms || !op2::g_gameMap.load(ms->data(), ms->size())) return 3;
    int W = op2::g_gameMap.width(), H = op2::g_gameMap.height(), mask = W - 1;

    // 1) Conta celulas impassaveis e amostra os cellTypes.
    int imp = 0, pass = 0;
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x)
        (op2::g_gameMap.isPassable(op2::LOCATION{x, y}) ? pass : imp)++;
    std::cout << "[COLL] mapa " << W << "x" << H << ": passaveis=" << pass
              << " impassaveis=" << imp << std::endl;

    // 2) pfCanEnter deve ser 0 em TODA celula impassavel e !=0 em passavel.
    op2::resetUnits();
    op2::Unit u; op2::TethysGame::CreateUnit(u, 1, op2::LOCATION{0, 0}, 0);  // veiculo dummy
    op2::pfSetContext(u.id());
    int badBlock = 0, badPass = 0;
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
        bool p = op2::g_gameMap.isPassable(op2::LOCATION{x, y});
        int ce = op2::pfCanEnter(x, y);
        if (p && ce == 0) ++badPass;        // passavel mas canEnter diz nao (ex.: ocupada)
        if (!p && ce != 0) ++badBlock;      // impassavel mas canEnter deixou entrar (ERRO)
    }
    std::cout << "[COLL] pfCanEnter: impassaveis-que-deixaram-entrar=" << badBlock
              << " (deve ser 0)" << std::endl;

    // 3) Acha um par de celulas passaveis com impassavel no caminho reto e roteia.
    //    Procura uma impassavel, pega passaveis a oeste e leste dela na mesma linha.
    int tested = 0, crossings = 0;
    for (int y = 2; y < H - 2 && tested < 5; ++y) {
        for (int x = 2; x < W - 6 && tested < 5; ++x) {
            if (op2::g_gameMap.isPassable(op2::LOCATION{x, y})) continue;      // acha impassavel
            // start a oeste, dest a leste da barreira, ambos passaveis
            int sx = x - 2, ex = x + 3;
            if (sx < 0 || ex >= W) continue;
            if (!op2::g_gameMap.isPassable(op2::LOCATION{sx, y})) continue;
            if (!op2::g_gameMap.isPassable(op2::LOCATION{ex, y})) continue;
            ++tested;
            op2::pfSetTarget(ex, y);
            op2::SearchContext sc; sc.unitId = u.id();
            sc.startX = sx; sc.startY = y; sc.destX = ex; sc.destY = y;
            int dirs[600]; int reached = 0;
            int n = op2::pfSearchRoute(sc, dirs, 600, &reached);
            // re-executa e confere que NENHUMA celula da rota e impassavel
            int cx = sx, cy = y; bool hitImp = false;
            for (int i = 0; i < n; ++i) {
                cx = (cx + op2::kDir8[dirs[i]].dx) & mask;
                cy = cy + op2::kDir8[dirs[i]].dy;
                if (!op2::g_gameMap.isPassable(op2::LOCATION{cx, cy})) { hitImp = true; break; }
            }
            std::cout << "[COLL] barreira em (" << x << "," << y << "): ("
                      << sx << "," << y << ")->(" << ex << "," << y << ") rota=" << n
                      << " reached=" << reached << " pisou-impassavel=" << (hitImp ? "SIM(ERRO)" : "nao")
                      << std::endl;
            if (!hitImp && n > 5) ++crossings;   // rota mais longa que a reta = desviou
        }
    }
    // 4) PRÉDIO como obstáculo: coloca um prédio e roteia um veículo atravessando-o.
    //    Acha uma área passável 6x6 para o prédio + rota.
    int bx = -1, by = -1;
    for (int y = 4; y < H - 4 && bx < 0; ++y)
        for (int x = 6; x < W - 6 && bx < 0; ++x) {
            bool clear = true;
            for (int dy = -2; dy <= 2 && clear; ++dy)
                for (int dx = -3; dx <= 3; ++dx)
                    if (!op2::g_gameMap.isPassable(op2::LOCATION{x + dx, y + dy})) { clear = false; break; }
            if (clear) { bx = x; by = y; }
        }
    if (bx >= 0) {
        op2::Unit bld;
        op2::TethysGame::CreateUnit(bld, op2::kFirstBuilding, op2::LOCATION{bx, by}, 0);  // COMMAND 3x2
        int fw, fh; op2::buildingFootprint(op2::kFirstBuilding, fw, fh);
        int tlx = bx - (fw >> 1), tly = by - (fh >> 1), occOK = 0, occTot = fw * fh;
        for (int dy = 0; dy < fh; ++dy) for (int dx = 0; dx < fw; ++dx)
            if (op2::g_gameMap.cellOccupant(tlx + dx, tly + dy) == bld.id()) ++occOK;
        // veículo atravessando o prédio (oeste -> leste)
        op2::Unit veh; op2::TethysGame::CreateUnit(veh, 1, op2::LOCATION{bx - 4, by}, 0);
        op2::pfSetContext(veh.id());
        op2::pfSetTarget((bx + 4) & mask, by);
        op2::SearchContext sc; sc.unitId = veh.id();
        sc.startX = (bx - 4) & mask; sc.startY = by; sc.destX = (bx + 4) & mask; sc.destY = by;
        int dirs[600]; int reached = 0;
        int n = op2::pfSearchRoute(sc, dirs, 600, &reached);
        int cx = sc.startX, cy = sc.startY; bool hitBld = false;
        for (int i = 0; i < n; ++i) {
            cx = (cx + op2::kDir8[dirs[i]].dx) & mask; cy += op2::kDir8[dirs[i]].dy;
            int occ = op2::g_gameMap.cellOccupant(cx, cy);
            if (occ == bld.id()) { hitBld = true; break; }
        }
        std::cout << "[COLL] predio COMMAND " << fw << "x" << fh << " em (" << bx << "," << by
                  << "): footprint-ocupado=" << occOK << "/" << occTot
                  << "; veiculo atravessou (" << sc.startX << "->" << sc.destX << ",y" << by
                  << ") rota=" << n << " reached=" << reached
                  << " pisou-no-predio=" << (hitBld ? "SIM(ERRO)" : "nao") << std::endl;
    }

    // 5) ESPIRAL: grupo de veiculos -> 1 CommandPacket -> destinos distintos.
    {
        op2::resetUnits();
        // acha area passavel p/ spawnar e p/ o alvo
        int ax = -1, ay = -1;
        for (int y = 6; y < H - 6 && ax < 0; ++y)
            for (int x = 6; x < W - 6 && ax < 0; ++x)
                if (op2::g_gameMap.isPassable(op2::LOCATION{x, y})) { ax = x; ay = y; }
        op2::CommandPacket pkt; pkt.type = op2::CMD_MOVE;
        pkt.waypoints.push_back(op2::packLocation(ax, ay));
        for (int k = 0; k < 6; ++k) {
            op2::Unit v;
            op2::LOCATION sp = op2::g_gameMap.findPassableNear(op2::LOCATION{(ax + 8 + k) & mask, ay});
            op2::TethysGame::CreateUnit(v, 1, sp, 0);
            pkt.unitIds.push_back(v.id());
        }
        op2::processCommandPacket(0, pkt);
        std::cout << "[COLL] espiral: " << pkt.unitIds.size() << " veiculos -> alvo (" << ax << "," << ay << "):";
        std::set<int> cells; int enter = 0;
        for (int id : pkt.unitIds) {
            const op2::UnitRecord& r = op2::g_units[id];
            uint32_t d = r.order.waypoints.empty() ? 0 : r.order.waypoints.back();
            int dx = op2::unpackLocX(d), dy = op2::unpackLocY(d);
            cells.insert(dy * 1000 + dx);
            op2::pfSetContext(id);
            if (op2::pfCanEnter(dx, dy)) ++enter;
            std::cout << " (" << dx << "," << dy << ")";
        }
        std::cout << " | destinos-distintos=" << cells.size() << "/" << pkt.unitIds.size()
                  << " enteraveis=" << enter << "/" << pkt.unitIds.size() << std::endl;
    }

    std::cout << "[COLL] resultado: " << (badBlock == 0 ? "OK terreno bloqueia" : "FALHA")
              << "; desvios validados=" << crossings << std::endl;
    return 0;
}

// --artdump <imgIndex> <out.bmp>: carrega op2_art e grava a imagem em BMP.
static int runArtDump(int idx, const std::string& outPath) {
    if (!op2::g_ResManager.init(".")) return 2;
    if (!op2::g_art.load()) { std::cerr << "[ArtDump] falha ao carregar op2_art" << std::endl; return 3; }
    std::cout << "[ArtDump] " << op2::g_art.numImages() << " imagens, "
              << op2::g_art.numAnimations() << " animacoes" << std::endl;
    if (idx < 0 || idx >= op2::g_art.numImages()) { std::cerr << "indice fora da faixa" << std::endl; return 4; }
    const op2::ArtImage& m = op2::g_art.image(idx);
    std::vector<uint32_t> px;
    op2::g_art.decode(idx, px, /*transparent*/ false);
    // fundo rosa p/ enxergar o recorte
    std::vector<uint32_t> rgb(static_cast<size_t>(m.width) * m.height);
    for (size_t k = 0; k < rgb.size(); ++k) rgb[k] = px[k] ? px[k] : 0xFFFF00FFu;
    writeBmp32(outPath, rgb.data(), m.width, m.height);
    std::cout << "[ArtDump] img " << idx << " " << m.width << "x" << m.height
              << " pal=" << m.pal << " type=" << m.type << " -> " << outPath << std::endl;
    return 0;
}

// --animdump <anim> <frame> <out.bmp>: compoe um frame de animacao (pilha de
// pictures do atlas, posicionadas por x,y na ordem slot) e grava em BMP.
static int runAnimDump(int ai, int fi, const std::string& outPath) {
    if (!op2::g_ResManager.init(".")) return 2;
    if (!op2::g_art.load()) return 3;
    if (ai < 0 || ai >= op2::g_art.numAnimations()) { std::cerr << "anim fora da faixa\n"; return 4; }
    const op2::ArtAnimation& an = op2::g_art.animation(ai);
    if (fi < 0 || fi >= (int)an.frames.size()) { std::cerr << "frame fora da faixa\n"; return 5; }
    const op2::ArtFrame& fr = an.frames[fi];
    // bounding box das pictures
    int minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
    for (const auto& p : fr.pics) {
        const op2::ArtImage& m = op2::g_art.image(p.image);
        minX = std::min(minX, (int)p.x); minY = std::min(minY, (int)p.y);
        maxX = std::max(maxX, (int)p.x + (int)m.width); maxY = std::max(maxY, (int)p.y + (int)m.height);
    }
    if (fr.pics.empty()) { minX = minY = 0; maxX = maxY = 1; }
    int W = maxX - minX, H = maxY - minY;
    if (W < 1) W = 1; if (H < 1) H = 1;
    std::vector<uint32_t> canvas((size_t)W * H, 0xFFFF00FFu);   // fundo rosa
    // ordena por slot e blita
    std::vector<op2::ArtPicture> pics = fr.pics;
    std::sort(pics.begin(), pics.end(), [](const op2::ArtPicture& a, const op2::ArtPicture& b){ return a.slot < b.slot; });
    for (const auto& p : pics) {
        const op2::ArtImage& m = op2::g_art.image(p.image);
        std::vector<uint32_t> px; op2::g_art.decode(p.image, px, /*transparent*/ true);
        for (uint32_t y = 0; y < m.height; ++y) for (uint32_t x = 0; x < m.width; ++x) {
            uint32_t c = px[y * m.width + x]; if (!(c >> 24)) continue;   // alpha 0 = pula
            int cx = (p.x - minX) + (int)x, cy = (p.y - minY) + (int)y;
            if (cx >= 0 && cx < W && cy >= 0 && cy < H) canvas[(size_t)cy * W + cx] = c | 0xFF000000u;
        }
    }
    writeBmp32(outPath, canvas.data(), W, H);
    std::cout << "[AnimDump] anim " << ai << " (" << an.frames.size() << " frames, hdr8="
              << an.header[8] << ") frame " << fi << ": " << fr.pics.size() << " pics, "
              << W << "x" << H << " -> " << outPath << std::endl;
    return 0;
}

// --contactsheet <start> <count> <out.bmp>: grade com o frame 0 de varias animacoes
// (celula 80x80), p/ identificar visualmente unidades/estruturas e seus indices.
static int runContactSheet(int start, int count, const std::string& outPath) {
    if (!op2::g_ResManager.init(".")) return 2;
    if (!op2::g_art.load()) return 3;
    const int CELL = 80, COLS = 8;
    int rows = (count + COLS - 1) / COLS;
    int W = COLS * CELL, H = rows * CELL;
    std::vector<uint32_t> cv((size_t)W * H, 0xFF202020u);
    for (int k = 0; k < count; ++k) {
        int ai = start + k;
        if (ai < 0 || ai >= op2::g_art.numAnimations()) continue;
        const op2::ArtAnimation& an = op2::g_art.animation(ai);
        if (an.frames.empty()) continue;
        const op2::ArtFrame& fr = an.frames[0];
        // bbox
        int minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
        for (const auto& p : fr.pics) {
            const op2::ArtImage& m = op2::g_art.image(p.image);
            minX = std::min(minX, (int)p.x); minY = std::min(minY, (int)p.y);
            maxX = std::max(maxX, (int)p.x + (int)m.width); maxY = std::max(maxY, (int)p.y + (int)m.height);
        }
        if (fr.pics.empty()) continue;
        int bw = maxX - minX, bh = maxY - minY;
        int cellX = (k % COLS) * CELL, cellY = (k / COLS) * CELL;
        int ox = cellX + (CELL - bw) / 2 - minX, oy = cellY + (CELL - bh) / 2 - minY;
        std::vector<op2::ArtPicture> pics = fr.pics;
        std::sort(pics.begin(), pics.end(), [](const op2::ArtPicture& a, const op2::ArtPicture& b){ return a.slot < b.slot; });
        for (const auto& p : pics) {
            const op2::ArtImage& m = op2::g_art.image(p.image);
            std::vector<uint32_t> px; op2::g_art.decode(p.image, px, true);
            for (uint32_t y = 0; y < m.height; ++y) for (uint32_t x = 0; x < m.width; ++x) {
                uint32_t c = px[y * m.width + x]; if (!(c >> 24)) continue;
                int X = ox + p.x + (int)x, Y = oy + p.y + (int)y;
                if (X >= cellX && X < cellX + CELL && Y >= cellY && Y < cellY + CELL && X < W && Y < H)
                    cv[(size_t)Y * W + X] = c | 0xFF000000u;
            }
        }
    }
    writeBmp32(outPath, cv.data(), W, H);
    std::cout << "[Sheet] anims " << start << ".." << (start + count - 1) << " ("
              << COLS << " por linha) -> " << outPath << std::endl;
    return 0;
}

int main(int argc, char* argv[]) {
    // Modos utilitários (sem o loop interativo).
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--extract") == 0 && i + 2 < argc)
            return runExtract(argv[i + 1], argv[i + 2]);
        if (std::strcmp(argv[i], "--rendermap") == 0 && i + 2 < argc)
            return runRenderMap(argv[i + 1], argv[i + 2]);
        if (std::strcmp(argv[i], "--game") == 0 && i + 1 < argc)
            return runGameView(argv[i + 1]);
        if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc)
            return runScreenshot(argc, argv, argv[i + 1]);
        if (std::strcmp(argv[i], "--rngtest") == 0 && i + 2 < argc)
            return runRngTest(argv[i + 1], argv[i + 2]);
        if (std::strcmp(argv[i], "--playertest") == 0)
            return runPlayerTest();
        if (std::strcmp(argv[i], "--unittest") == 0)
            return runUnitTest();
        if (std::strcmp(argv[i], "--playshot") == 0 && i + 2 < argc) {
            bool pass = (i + 3 < argc && std::strcmp(argv[i + 3], "pass") == 0);
            return runPlayShot(argv[i + 1], argv[i + 2], pass);
        }
        if (std::strcmp(argv[i], "--movecap") == 0 && i + 2 < argc)
            return runMoveCap(argv[i + 1], argv[i + 2]);
        if (std::strcmp(argv[i], "--colltest") == 0 && i + 1 < argc)
            return runCollTest(argv[i + 1]);
        if (std::strcmp(argv[i], "--artdump") == 0 && i + 2 < argc)
            return runArtDump(atoi(argv[i + 1]), argv[i + 2]);
        if (std::strcmp(argv[i], "--animdump") == 0 && i + 3 < argc)
            return runAnimDump(atoi(argv[i + 1]), atoi(argv[i + 2]), argv[i + 3]);
        if (std::strcmp(argv[i], "--contactsheet") == 0 && i + 3 < argc)
            return runContactSheet(atoi(argv[i + 1]), atoi(argv[i + 2]), argv[i + 3]);
    }

    // --play <map>: entra direto no jogo com o mapa (via o backbone TApp).
    std::string initialMap;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--play") == 0 && i + 1 < argc) initialMap = argv[i + 1];

    std::cout << "========================================" << std::endl;
    std::cout << "  Outpost 2: Divided Destiny Modern Port " << std::endl;
    std::cout << "  Engine Version: 1.0.0 (MSVC + SDL2)   " << std::endl;
    std::cout << "========================================" << std::endl;

    // Backbone TApp (asm): Init -> CreateFrames -> Run -> ShutDown.
    if (!op2::gTApp.init())                   { op2::gTApp.shutdown(); return 1; }
    if (!op2::gTApp.createFrames(initialMap)) { op2::gTApp.shutdown(); return 1; }
    op2::gTApp.run();
    op2::gTApp.shutdown();
    return 0;
}

// (código antigo abaixo removido: o loop interativo agora vive em TApp::run)
#if 0
int main_old(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {}

    std::cout << "========================================" << std::endl;
    std::cout << "  Outpost 2: Divided Destiny Modern Port " << std::endl;
    std::cout << "  Engine Version: 1.0.0 (MSVC + SDL2)   " << std::endl;
    std::cout << "========================================" << std::endl;

    // Carrega as configurações do jogo (OUTPOST2.INI), como o executável original.
    op2::Config config("OUTPOST2.INI");
    op2::GameSettings settings;
    if (config.load()) {
        settings.loadFrom(config);
        std::cout << "[Config] OUTPOST2.INI carregado (jogador='" << settings.name
                  << "', GameSpeed=" << settings.gameSpeed
                  << ", ScrollRate=" << settings.scrollRate << ")" << std::endl;
    } else {
        std::cout << "[Config] OUTPOST2.INI não encontrado; usando defaults." << std::endl;
    }

    // Gateway de assets (VOLs + arquivos soltos), como o g_ResManager original.
    op2::g_ResManager.init(".");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        std::cerr << "Failed to initialize SDL2: " << SDL_GetError() << std::endl;
        return 1;
    }

    op2::Renderer renderer;
    // 640x480 native, scaled up to 960x720 (1.5x) or 1280x960 (2x) window for crisp modern displays
    if (!renderer.init("Outpost 2: Divided Destiny", 1280, 960, false)) {
        std::cerr << "Failed to initialize Renderer!" << std::endl;
        SDL_Quit();
        return 1;
    }

    op2::ShellMenu shellMenu;
    if (!shellMenu.init("OP2SHRES.DLL")) {
        std::cerr << "Warning: OP2SHRES.DLL resources could not be loaded directly." << std::endl;
    }

    // Check for --screenshot mode
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--tutorials") {
            shellMenu.showTutorials();
        }
        if (std::string(argv[i]) == "--menu") {
            shellMenu.showMainMenu();
        }
        if (std::string(argv[i]) == "--campaign") shellMenu.showNewCampaign();
        if (std::string(argv[i]) == "--prefs")    shellMenu.showPreferences();
        if (std::string(argv[i]) == "--colony")   shellMenu.showColonyGames();
        if (std::string(argv[i]) == "--about")    shellMenu.showAbout();
        if (std::string(argv[i]) == "--screenshot" && i + 1 < argc) {
            std::string outPath = argv[i + 1];
            renderer.clear(0xFF000000);
            shellMenu.render(renderer);
            
            // Save framebuffer as 32-bit BMP
            FILE* f = fopen(outPath.c_str(), "wb");
            if (f) {
                int w = op2::Renderer::NATIVE_WIDTH;
                int h = op2::Renderer::NATIVE_HEIGHT;
                uint8_t bmpHdr[54] = {
                    'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0,
                    40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 32,0,
                    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0
                };
                uint32_t fileSize = 54 + w * h * 4;
                memcpy(&bmpHdr[2], &fileSize, 4);
                memcpy(&bmpHdr[18], &w, 4);
                int negH = -h; // top-down
                memcpy(&bmpHdr[22], &negH, 4);
                fwrite(bmpHdr, 1, 54, f);
                fwrite(renderer.getFramebuffer(), 4, w * h, f);
                fclose(f);
                std::cout << "[Screenshot] Saved screenshot to: " << outPath << std::endl;
            }
            renderer.shutdown();
            SDL_Quit();
            return 0;
        }
    }

    bool running = true;
    uint32_t lastTicks = SDL_GetTicks();

    while (running && !shellMenu.shouldQuit()) {
        uint32_t currentTicks = SDL_GetTicks();
        float dt = (currentTicks - lastTicks) / 1000.0f;
        lastTicks = currentTicks;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;

                case SDL_MOUSEMOTION: {
                    int nx, ny;
                    renderer.screenToNative(event.motion.x, event.motion.y, nx, ny);
                    shellMenu.handleMouseMove(nx, ny);
                    break;
                }

                case SDL_MOUSEBUTTONDOWN: {
                    int nx, ny;
                    renderer.screenToNative(event.button.x, event.button.y, nx, ny);
                    shellMenu.handleMouseDown(nx, ny, event.button.button);
                    break;
                }

                case SDL_MOUSEBUTTONUP: {
                    int nx, ny;
                    renderer.screenToNative(event.button.x, event.button.y, nx, ny);
                    shellMenu.handleMouseUp(nx, ny, event.button.button);
                    break;
                }

                case SDL_MOUSEWHEEL:
                    shellMenu.handleMouseWheel(event.wheel.y);
                    break;

                case SDL_KEYDOWN:
                    shellMenu.handleKeyDown(event.key.keysym.sym);
                    break;

                default:
                    break;
            }
        }

        shellMenu.update(dt);

        renderer.clear(0xFF000000);
        shellMenu.render(renderer);
        renderer.present();

        // Target ~60 FPS cap
        uint32_t frameElapsed = SDL_GetTicks() - currentTicks;
        if (frameElapsed < 16) {
            SDL_Delay(16 - frameElapsed);
        }
    }

    std::cout << "[Main] Exiting cleanly." << std::endl;
    renderer.shutdown();
    SDL_Quit();
    return 0;
}
#endif // 0
