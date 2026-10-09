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
#include "game/movement.h"
#include "game/structure.h"
#include "game/weapon.h"
#include "game/trigger.h"
#include "game/disaster.h"
#include "game/commands.h"
#include "game/messages.h"
#include "game/research.h"
#include "game/building_sheet.h"
#include "game/colony.h"
#include "game/structure_anim.h"
#include "game/production.h"
#include "game/docking.h"
#include "game/construction.h"
#include "game/research_lab.h"
#include "game/research.h"
#include "gfx/artfile.h"
#include "app/tapp.h"
#include "app/game_state.h"

// --unittest: exercita o pool de unidades + o wrapper Unit.
static int runUnitTest() {
    op2::resetUnits();
    op2::Unit b, v;
    op2::TethysGame::CreateUnit(b, op2::mapCommandCenter, op2::LOCATION{10, 10}, 0);
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
    op2::g_art.load();   // sprites (como TApp::init)
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

// --structtest <map> <prefix>: conduz o 1º prédio pelos estados (construção,
// dano 1/2, reparo, destruição + entulho) e salva quadros prefix_<fase>.bmp.
static int runStructTest(const std::string& mapName, const std::string& prefix) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { std::cerr << "SDL_Init: " << SDL_GetError() << std::endl; return 1; }
    op2::g_ResManager.init(".");
    op2::g_art.load();
    op2::Renderer renderer;
    if (!renderer.init("OP2 structtest", 1280, 960, false)) { SDL_Quit(); return 1; }
    op2::GameState gs;
    if (!gs.load(mapName)) { renderer.shutdown(); SDL_Quit(); return 2; }
    gs.onActivate();
    int bid = -1;
    for (int i = 1; i < (int)op2::g_units.size(); ++i)
        if (op2::g_units[i].id != -1 && (op2::g_units[i].flags & op2::UNIT_BUILDING)) { bid = i; break; }
    if (bid < 0) { std::cerr << "sem predio" << std::endl; return 3; }
    auto snap = [&](const char* tag) {
        const op2::UnitRecord& r = op2::g_units[bid];
        char path[256];
        std::snprintf(path, sizeof(path), "%s_%s.bmp", prefix.c_str(), tag);
        writeBmp32(path, renderer.getFramebuffer(),
                   op2::Renderer::NATIVE_WIDTH, op2::Renderer::NATIVE_HEIGHT);
        std::printf("[Struct] %-10s tick=%d id=%d estado=%u sub=%u dano=%d/%d anim=%d frame=%d acao=0x%X\n",
                    tag, op2::TethysGame::tick(), r.id, (r.state34 >> 16) & 0xFFFF,
                    r.state34 & 0xFFFF, r.damage, op2::structureMaxHP(r),
                    r.id == -1 ? -1 : op2::structureCurAnim(r),
                    r.id == -1 ? -1 : op2::structureFrame(r), r.action);
    };
    auto run = [&](int ticks) { for (int t = 0; t < ticks; ++t) gs.onIdle(renderer, 0.05f); };  // 1 tick (GameSpeed 5)
    run(2); snap("intacto");
    op2::structureStartBuild(bid, 0);
    const int dur = op2::g_units[bid].buildTime;
    std::printf("[Struct] construcao: duracao=%d ticks\n", dur);
    run(dur / 4); snap("const25");
    run(dur / 2); snap("const75");
    run(dur / 4 + 12); snap("pronto");
    const int hp = op2::structureMaxHP(op2::g_units[bid]);
    op2::g_units[bid].damage = static_cast<int16_t>(hp * 35 / 100); run(2); snap("dano35");
    op2::g_units[bid].damage = static_cast<int16_t>(hp * 65 / 100); run(2); snap("dano65");
    op2::g_units[bid].damage = 0; run(2); snap("reparado");
    // Animacao ambiente: predio operando (sub_483810) -> a cada 100 ticks
    // sub_4835C0 pode por o estado 5+ (frame = contador ate o fim).
    op2::structureTurnOn(op2::g_units[bid]);
    {
        int shots = 0;
        for (int t = 0; t < 1200 && shots < 3; ++t) {
            run(1);
            const op2::UnitRecord& r = op2::g_units[bid];
            int st = (r.state34 >> 16) & 0xFFFF, sub = r.state34 & 0xFFFF;
            if (st >= 5 && sub == 3) {
                char tag[16]; std::snprintf(tag, sizeof(tag), "amb%d", shots);
                snap(tag); ++shots;
            }
        }
        if (!shots) std::printf("[Struct] nenhuma animacao ambiente em 1200 ticks\n");
    }
    op2::Unit(bid).DoDeath();
    run(4); snap("morte04");
    run(8); snap("morte12");
    run(14); snap("morte26");
    run(40); snap("entulho");
    renderer.shutdown();
    SDL_Quit();
    return 0;
}

// --paneshot <map> <out.bmp>: Tokamak ao lado do CC da cena, selecionado, e
// a tela com o quadro de informação (painel direito) p/ comparar com o original.
static int runPaneShot(const std::string& mapName, const std::string& outPath, int extraLight) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    op2::g_ResManager.init(".");
    op2::g_art.load();
    op2::Renderer renderer;
    if (!renderer.init("OP2 paneshot", 1280, 960, false)) { SDL_Quit(); return 1; }
    op2::GameState gs;
    if (!gs.load(mapName)) { renderer.shutdown(); SDL_Quit(); return 2; }
    gs.onActivate();
    int cc = -1;
    for (int i = 1; i < (int)op2::g_units.size(); ++i)
        if (op2::g_units[i].id != -1 && op2::g_units[i].type == op2::mapCommandCenter) { cc = i; break; }
    int sel = cc;
    if (cc > 0) {
        op2::Unit tok;
        op2::LOCATION l{(op2::g_units[cc].pixelX >> 5) + 3, op2::g_units[cc].pixelY >> 5};
        if (op2::TethysGame::CreateUnit(tok, op2::mapTokamak, l, op2::g_units[cc].owner & 0x0F)) sel = tok.id();
    }
    for (int k = 0; k < extraLight; ++k) op2::TethysGame::advanceDaylight(op2::g_gameMap.internalWidth());
    gs.setSelection({sel});
    gs.setMouse(200, 150);
    for (int i = 0; i < 40; ++i) gs.onIdle(renderer, 0.05f);
    bool ok = writeBmp32(outPath, renderer.getFramebuffer(),
                         op2::Renderer::NATIVE_WIDTH, op2::Renderer::NATIVE_HEIGHT);
    std::printf("[PaneShot] %s\n", ok ? outPath.c_str() : "falha");
    renderer.shutdown(); SDL_Quit();
    return ok ? 0 : 1;
}

// --cmdshot <map> <out.bmp> <map_id> [qtd] [carga]: cria `qtd` unidades do tipo
// ao lado do CC, seleciona-as e grava o painel com a botoeira (mouse no botão 1).
static int runCmdShot(const std::string& mapName, const std::string& outPath, int type, int count, int cargo) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    op2::g_ResManager.init(".");
    op2::g_art.load();
    op2::Renderer renderer;
    if (!renderer.init("OP2 cmdshot", 1280, 960, false)) { SDL_Quit(); return 1; }
    op2::GameState gs;
    if (!gs.load(mapName)) { renderer.shutdown(); SDL_Quit(); return 2; }
    gs.onActivate();
    int cc = -1;
    for (int i = 1; i < (int)op2::g_units.size(); ++i)
        if (op2::g_units[i].id != -1 && op2::g_units[i].type == op2::mapCommandCenter) { cc = i; break; }
    if (cc < 0) return 3;
    std::vector<int> sel;
    for (int k = 0; k < count; ++k) {
        op2::Unit u;
        op2::LOCATION l{(op2::g_units[cc].pixelX >> 5) + 3 + k, (op2::g_units[cc].pixelY >> 5) + 3};
        if (op2::TethysGame::CreateUnit(u, static_cast<op2::map_id>(type), l, op2::g_units[cc].owner & 0x0F, cargo))
            sel.push_back(u.id());
    }
    gs.setSelection(sel);
    gs.setMouse(op2::GameState::kColX + 28 + 22, op2::GameState::kPaneTop + 0x3C + 57 + 22);
    for (int i = 0; i < 10; ++i) gs.onIdle(renderer, 0.05f);
    bool ok = writeBmp32(outPath, renderer.getFramebuffer(),
                         op2::Renderer::NATIVE_WIDTH, op2::Renderer::NATIVE_HEIGHT);
    std::printf("[CmdShot] tipo 0x%02X x%d -> %s\n", type, (int)sel.size(), ok ? outPath.c_str() : "falha");
    renderer.shutdown(); SDL_Quit();
    return ok ? 0 : 1;
}

// --cmdtest <map>: Lynx anda por "Mover" (mira) e recebe "Parar" no caminho.
static int runCmdTest(const std::string& mapName) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    op2::g_ResManager.init(".");
    op2::Renderer renderer;
    if (!renderer.init("OP2 cmdtest", 1280, 960, false)) { SDL_Quit(); return 1; }
    op2::GameState gs;
    if (!gs.load(mapName)) { renderer.shutdown(); SDL_Quit(); return 2; }
    int cc = -1;
    for (int i = 1; i < (int)op2::g_units.size(); ++i)
        if (op2::g_units[i].id != -1 && op2::g_units[i].type == op2::mapCommandCenter) { cc = i; break; }
    if (cc < 0) return 3;
    op2::Unit u;
    const int sx = (op2::g_units[cc].pixelX >> 5) + 3, sy = (op2::g_units[cc].pixelY >> 5) + 4;
    op2::TethysGame::CreateUnit(u, op2::mapLynx, op2::LOCATION{sx, sy}, op2::g_units[cc].owner & 0x0F, 0x3D);
    const std::vector<int> sel{u.id()};
    std::printf("[CmdTest] Mover: %d\n", (int)op2::uiCommandClick(op2::UC_MOVE, sel));
    op2::uiCommandTarget(op2::UC_MOVE, sel, sx + 12, sy);
    for (int t = 0; t < 120; ++t) {
        if (t == 60) std::printf("[CmdTest] Parar: %d\n", (int)op2::uiCommandClick(op2::UC_STOP, sel));
        gs.onIdle(renderer, 0.2501f / 4.0f);
        const op2::UnitRecord& r = op2::g_units[u.id()];
        if (t % 10 == 9 || (t > 58 && t < 72))
            std::printf("[CmdTest] t=%3d tile=(%d,%d) px=%d acao=%d f20=%d sub=%d passo=%d\n", t, r.pixelX >> 5, r.pixelY >> 5,
                        r.pixelX, r.action, r.field20, r.field22, r.stepTicks);
    }
    // Espalhar: 3 Lynx lado a lado; Ficar parado: o 1º anda 6 tiles e segura.
    std::vector<int> grp;
    for (int k = 0; k < 3; ++k) {
        op2::Unit v;
        if (op2::TethysGame::CreateUnit(v, op2::mapLynx, op2::LOCATION{sx + k, sy + 3}, op2::g_units[cc].owner & 0x0F, 0x3D))
            grp.push_back(v.id());
    }
    std::printf("[CmdTest] Espalhar: %d\n", (int)op2::uiCommandClick(op2::UC_SCATTER, grp));
    for (int id : grp) {
        const op2::UnitRecord& r = op2::g_units[id];
        std::printf("[CmdTest]   %d de (%d,%d) -> (%d,%d)\n", id, r.pixelX >> 5, r.pixelY >> 5,
                    op2::unpackLocX(r.order.waypoints.back()), op2::unpackLocY(r.order.waypoints.back()));
    }
    for (int t = 0; t < 150; ++t) gs.onIdle(renderer, 0.2501f / 4.0f);
    const std::vector<int> one{grp[0]};
    std::printf("[CmdTest] Ficar parado: %d\n", (int)op2::uiCommandClick(op2::UC_STAND_GROUND, one));
    op2::uiCommandTarget(op2::UC_STAND_GROUND, one, (op2::g_units[grp[0]].pixelX >> 5) - 6, op2::g_units[grp[0]].pixelY >> 5);
    for (int t = 0; t < 300; ++t) gs.onIdle(renderer, 0.2501f / 4.0f);
    const op2::UnitRecord& g0 = op2::g_units[grp[0]];
    std::printf("[CmdTest] apos: tile=(%d,%d) acao=0x%X f20=%d sub=%d\n", g0.pixelX >> 5, g0.pixelY >> 5, g0.action, g0.field20, g0.field22);
    // Atacar: Lynx nosso contra um Scout do jogador 1 a 12 tiles.
    {
        op2::Unit me2, foe;
        op2::resetUnits();                                   // cena limpa: só os dois
        const int ax = sx, ay = sy - 4;
        op2::TethysGame::CreateUnit(me2, op2::mapLynx, op2::LOCATION{ax, ay}, op2::g_units[cc].owner & 0x0F, 0x3D);
        op2::TethysGame::CreateUnit(foe, op2::mapScout, op2::LOCATION{ax + 12, ay}, 1, 0);
        op2::g_units[me2.id()].flags &= ~op2::UNIT_AUTO_TARGET;   // so a ordem (sem alvo automatico)
        const std::vector<int> s1{me2.id()};
        std::printf("[CmdTest] Atacar: %d\n", (int)op2::uiCommandClick(op2::UC_ATTACK, s1));
        op2::uiCommandTarget(op2::UC_ATTACK, s1, ax + 12, ay);
        for (int t = 0; t < 600; ++t) {
            gs.onIdle(renderer, 0.2501f / 4.0f);
            const op2::UnitRecord& a = op2::g_units[me2.id()];
            const op2::UnitRecord& f = op2::g_units[foe.id()];
            if (t % 40 == 0 || (f.flags & op2::UNIT_DEAD))
                std::printf("[CmdTest] t=%3d lynx=(%d,%d) acao=0x%X sub=%d ordem=%08X arma=%08X | scout dano=%d%s\n", t,
                            a.pixelX >> 5, a.pixelY >> 5, a.action, a.field22, a.orderTarget(), a.target(), f.damage,
                            (f.flags & op2::UNIT_DEAD) ? " MORTO" : "");
            if (f.flags & op2::UNIT_DEAD) break;
        }
    }
    // Guardar: Lynx nosso guarda a ConVec; um Lynx do jogador 1 chega perto dela.
    {
        op2::resetUnits();
        const int gx = sx, gy = sy - 4;
        op2::Unit cv, gd, en;
        const int me = op2::TethysGame::localPlayer();
        op2::TethysGame::CreateUnit(cv, op2::mapConVec, op2::LOCATION{gx, gy}, me, 0);
        op2::TethysGame::CreateUnit(gd, op2::mapLynx, op2::LOCATION{gx - 8, gy}, me, 0x3D);
        const std::vector<int> s1{gd.id()};
        std::printf("[CmdTest] Guardar: %d\n", (int)op2::uiCommandClick(op2::UC_GUARD, s1));
        op2::uiCommandTarget(op2::UC_GUARD, s1, gx, gy);
        for (int t = 0; t < 900; ++t) {
            if (t == 200) op2::TethysGame::CreateUnit(en, op2::mapLynx, op2::LOCATION{gx + 3, gy}, 1, 0x3D);
            gs.onIdle(renderer, 0.2501f / 4.0f);
            const op2::UnitRecord& a = op2::g_units[gd.id()];
            const op2::UnitRecord& c = op2::g_units[cv.id()];
            if (t % 25 == 0 && t < 520) {
                const op2::UnitRecord* e = en.id() > 0 ? &op2::g_units[en.id()] : nullptr;
                std::printf("[CmdTest] t=%3d guarda id=%d (%d,%d) vivo=%d acao=0x%X sub=%d arma=%08X dano=%d | inimigo (%d,%d) arma=%08X dano=%d\n",
                            t, a.id, a.pixelX >> 5, a.pixelY >> 5, !(a.flags & op2::UNIT_DEAD), a.action, a.field22,
                            a.target(), a.damage, e ? e->pixelX >> 5 : -1, e ? e->pixelY >> 5 : -1,
                            e ? e->target() : 0, e ? e->damage : -1);
            }
        }
    }
    // Patrulhar: Lynx vai ao ponto inicial e fica indo e voltando até o final.
    {
        op2::resetUnits();
        const int px = sx, py = sy - 4;
        op2::Unit ly;
        op2::TethysGame::CreateUnit(ly, op2::mapLynx, op2::LOCATION{px, py}, op2::TethysGame::localPlayer(), 0x3D);
        const std::vector<int> s1{ly.id()};
        // Ponto final: 1ª célula enterável a partir de px+8 (indo p/ oeste).
        op2::pfSetContext(ly.id());
        int ex = px + 8;
        while (ex > px && !op2::pfCanEnter(ex, py)) --ex;
        const int r0 = (int)op2::uiCommandClick(op2::UC_PATROL, s1);
        std::printf("[CmdTest] Patrulhar: %d dica=\"%s\"\n", r0, op2::uiCommandHint().c_str());
        const int r1 = (int)op2::uiCommandTarget(op2::UC_PATROL, s1, px - 6, py);
        std::printf("[CmdTest] 1o ponto: %d dica=\"%s\"\n", r1, op2::uiCommandHint().c_str());
        const int r2 = (int)op2::uiCommandTarget(op2::UC_PATROL, s1, ex, py);
        const op2::UnitRecord& a = op2::g_units[ly.id()];
        std::printf("[CmdTest] 2o ponto: %d acao=0x%X pontos=%d inicio=%d | rota (%d,%d) <-> (%d,%d)\n", r2, a.action,
                    a.order.count, a.patrolStart, px - 6, py, ex, py);
        for (int t = 0; t < 1200; ++t) {
            gs.onIdle(renderer, 0.2501f / 4.0f);
            if (t % 40 == 0)
                std::printf("[CmdTest] t=%4d lynx=(%d,%d) acao=0x%X sub=%d wp=%d/%d\n", t, a.pixelX >> 5, a.pixelY >> 5,
                            a.action, a.field22, a.order.curWaypoint, a.order.countMinus1);
        }
    }
    renderer.shutdown(); SDL_Quit();
    return 0;
}

// --producetest <map> <prefix>: CC + Tokamak + Vehicle Factory + Structure
// Factory encostados; lista de produção (sem e com o atalho "toda tecnologia"),
// painel BUILD (screenshot), pacote 9 e entrega (veículo e kit na baia).
static int runProduceTest(const std::string& mapName, const std::string& prefix) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    op2::g_ResManager.init(".");
    op2::g_art.load();
    op2::Renderer renderer;
    if (!renderer.init("OP2 producetest", 1280, 960, false)) { SDL_Quit(); return 1; }
    op2::GameState gs;
    if (!gs.load(mapName)) { renderer.shutdown(); SDL_Quit(); return 2; }
    gs.onActivate();
    op2::resetUnits();
    const int W = op2::g_gameMap.width(), H = op2::g_gameMap.height();
    int bx = -1, by = -1;
    for (int y = 3; y < H - 10 && bx < 0; ++y)
        for (int x = 3; x < W - 16 && bx < 0; ++x) {
            bool ok = true;
            for (int dy = -1; dy <= 7 && ok; ++dy)
                for (int dx = -2; dx <= 12 && ok; ++dx)
                    if (!op2::g_gameMap.isPassable(op2::LOCATION{x + dx, y + dy})) ok = false;
            if (ok) { bx = x; by = y; }
        }
    if (bx < 0) { std::printf("[Produce] sem area livre\n"); return 3; }
    op2::Unit cc, tok, vf, sf;
    op2::TethysGame::CreateUnit(cc, op2::mapCommandCenter, op2::LOCATION{bx, by}, 0);
    op2::TethysGame::CreateUnit(tok, op2::mapTokamak, op2::LOCATION{bx + 3, by}, 0);
    op2::TethysGame::CreateUnit(vf, op2::mapVehicleFactory, op2::LOCATION{bx + 1, by + 2}, 0);
    op2::TethysGame::CreateUnit(sf, op2::mapStructureFactory, op2::LOCATION{bx + 5, by + 2}, 0);
    op2::PlayerData& pd = op2::g_playerData[0];
    pd.workers = 40; pd.scientists = 10;
    op2::colonyMarkDirty(0);
    for (int t = 0; t < 60; ++t) gs.onIdle(renderer, 0.2501f / 4.0f);
    pd.ore = 5000; pd.rareOre = 1000;            // (a colônia limita pela capacidade a cada recálculo)
    std::printf("[Produce] VF operando=%d SF operando=%d\n", op2::structureOperational(op2::g_units[vf.id()]),
                op2::structureOperational(op2::g_units[sf.id()]));
    std::printf("[Produce] sem pesquisa: VF %d itens, SF %d itens\n", (int)op2::factoryProduceList(vf.id()).size(),
                (int)op2::factoryProduceList(sf.id()).size());
    op2::TethysGame::setCheatFlag(1, 1);                     // dword_56DE5C: toda tecnologia
    for (int f : {vf.id(), sf.id()}) {
        const std::vector<uint32_t> items = op2::factoryProduceList(f);
        std::printf("[Produce] %s (%d itens):\n", op2::unitTitle(op2::g_units[f]).c_str(), (int)items.size());
        for (uint32_t it : items)
            std::printf("    %-34s pagavel=%d  %s\n", op2::produceItemName(it).c_str(),
                        (int)op2::produceAffordable(f, it), op2::produceItemCost(it, 0).c_str());
    }
    // Painel BUILD da Vehicle Factory, com o 2o item selecionado e o mouse no 3o.
    gs.setSelection({vf.id()});
    gs.pressCommand(op2::UC_BUILD);
    const std::vector<uint32_t> vitems = op2::factoryProduceList(vf.id());
    const int listY0 = op2::GameState::kPaneTop + 0x21 + 2;   // topo do quadro + 33 + 2
    gs.setMouse(op2::GameState::kColX + 40, listY0 + 2 * 15 + 5);
    for (int i = 0; i < 4; ++i) gs.onIdle(renderer, 0.05f);
    writeBmp32(prefix + "_list.bmp", renderer.getFramebuffer(), op2::Renderer::NATIVE_WIDTH, op2::Renderer::NATIVE_HEIGHT);
    // Produz o Lynx/Laser (1o combo de chassi) pelo pacote 9.
    uint32_t lynx = 0;
    for (uint32_t it : vitems) if ((it & 0xFFFF) == op2::mapLynx) { lynx = it; break; }
    const int ore0 = pd.ore, rare0 = pd.rareOre;
    op2::sendProduce(vf.id(), lynx);
    const op2::UnitRecord& v = op2::g_units[vf.id()];
    std::printf("[Produce] pacote 9 %s: acao=%d minerio %d->%d raro %d->%d\n", op2::produceItemName(lynx).c_str(),
                v.action, ore0, pd.ore, rare0, pd.rareOre);
    int t = 0;
    for (; t < 3000 && v.action == 9; ++t) {
        gs.onIdle(renderer, 0.2501f / 4.0f);
        if (t == 1) std::printf("[Produce] t=1 sub=%d duracao=%d\n", v.field22, v.produceTotal);
    }
    int made = 0;
    for (int i = 1; i < (int)op2::g_units.size(); ++i) {
        const op2::UnitRecord& r = op2::g_units[i];
        if (r.id != -1 && r.type == op2::mapLynx && !(r.flags & op2::UNIT_DEAD)) {
            made = i;
            std::printf("[Produce] t=%d Lynx id=%d em (%d,%d) arma=0x%X rot=%d (fabrica em %d,%d)\n", t, i,
                        r.pixelX >> 5, r.pixelY >> 5, r.cargo, r.rotation, v.pixelX >> 5, v.pixelY >> 5);
        }
    }
    std::printf("[Produce] mensagem: %s\n", op2::statusBarText().c_str());
    // Kit na Structure Factory: Tokamak.
    pd.ore = 5000; pd.rareOre = 1000;
    op2::sendProduce(sf.id(), 0x3A);
    const op2::UnitRecord& s = op2::g_units[sf.id()];
    for (t = 0; t < 6000 && s.action == 9; ++t) gs.onIdle(renderer, 0.2501f / 4.0f);
    std::printf("[Produce] kit pronto em t=%d: baias %d %d %d %d %d %d | %s\n", t, s.bayType[0], s.bayType[1],
                s.bayType[2], s.bayType[3], s.bayType[4], s.bayType[5], op2::statusBarText().c_str());
    // Doca: ConVec parada na célula da doca da Structure Factory pega o kit da baia 0.
    {
        int dx = 0, dy = 0;
        const bool hasDock = op2::dockCell(sf.id(), dx, dy);
        op2::Unit cv;
        op2::TethysGame::CreateUnit(cv, op2::mapConVec, op2::LOCATION{dx, dy}, 0);
        const op2::UnitRecord& c = op2::g_units[cv.id()];
        std::printf("[Produce] doca=%d (%d,%d) convec em (%d,%d) na doca=%d disponivel=%d\n", hasDock, dx, dy,
                    c.pixelX >> 5, c.pixelY >> 5, op2::dockedVehicle(sf.id()), (int)op2::bayTransferAvailable(sf.id(), 0));
        gs.setSelection({sf.id()});
        gs.pressCommand(op2::UC_STORAGE);
        for (int i = 0; i < 3; ++i) gs.onIdle(renderer, 0.05f);
        writeBmp32(prefix + "_storage.bmp", renderer.getFramebuffer(), op2::Renderer::NATIVE_WIDTH, op2::Renderer::NATIVE_HEIGHT);
        op2::sendBayTransfer(sf.id(), 0);
        std::printf("[Produce] troca: convec sub=0x%X tempo=%d\n", c.field22, c.field3C);
        int tt = 0;
        for (; tt < 400 && c.field22 == 0x0D; ++tt) gs.onIdle(renderer, 0.2501f / 4.0f);
        std::printf("[Produce] apos %d: convec carga=0x%X sub=0x%X | baia0=0x%X\n", tt, c.cargo, c.field22, s.bayType[0]);
        for (int i = 0; i < 3; ++i) gs.onIdle(renderer, 0.05f);
        writeBmp32(prefix + "_storage2.bmp", renderer.getFramebuffer(), op2::Renderer::NATIVE_WIDTH, op2::Renderer::NATIVE_HEIGHT);
        // ConVec constrói o Tokamak (kit) num local livre perto dela.
        int w = 0, h = 0;
        op2::kitFootprint(cv.id(), w, h);
        int px = -1, py = -1;
        for (int r = 4; r < 20 && px < 0; ++r)
            for (int oy = -r; oy <= r && px < 0; ++oy)
                for (int ox = -r; ox <= r && px < 0; ++ox) {
                    const int cxp = (c.pixelX >> 5) + ox, cyp = (c.pixelY >> 5) + oy;
                    int l, t, rr, bb;
                    op2::kitPlacementRect(w, h, cxp, cyp, l, t, rr, bb);
                    bool ok = t >= 0 && bb < op2::g_gameMap.height();
                    for (int yy = t; ok && yy <= bb; ++yy)
                        for (int xx = l; ok && xx <= rr; ++xx)
                            ok = op2::g_gameMap.isPassable(op2::LOCATION{xx, yy}) && !op2::g_gameMap.cellOccupant(xx, yy);
                    if (ok) { px = cxp; py = cyp; }
                }
        std::printf("[Produce] obra: kit %dx%d em (%d,%d)\n", w, h, px, py);
        gs.setSelection({cv.id()});
        gs.pressCommand(op2::UC_BUILD_KIT);
        gs.setCamera(px * 32 - 200, py * 32 - 200 < 0 ? 0 : py * 32 - 200);
        gs.setMouse(op2::GameState::kViewX + 200 + 16, op2::GameState::kViewY + (py * 32 - 200 < 0 ? py * 32 : 200) + 16);
        for (int i = 0; i < 2; ++i) gs.onIdle(renderer, 0.05f);
        writeBmp32(prefix + "_place.bmp", renderer.getFramebuffer(), op2::Renderer::NATIVE_WIDTH, op2::Renderer::NATIVE_HEIGHT);
        op2::sendBuildKit({cv.id()}, px, py);
        std::printf("[Produce] convec acao=%d sub=%d destino retangulo=(%d,%d)-(%d,%d)\n", c.action, c.field22,
                    c.buildL, c.buildT, c.buildR, c.buildB);
        int bld = 0, tb = 0;
        for (; tb < 6000; ++tb) {
            gs.onIdle(renderer, 0.2501f / 4.0f);
            if (!bld)
                for (int i = 1; i < (int)op2::g_units.size(); ++i)
                    if (op2::g_units[i].id != -1 && op2::g_units[i].type == op2::mapTokamak && i != tok.id()) {
                        bld = i;
                        std::printf("[Produce] t=%d Tokamak criado id=%d em (%d,%d) acao=0x%X duracao=%d | convec sub=%d espera=%d\n",
                                    tb, i, op2::g_units[i].pixelX >> 5, op2::g_units[i].pixelY >> 5, op2::g_units[i].action,
                                    op2::g_units[i].buildTime, c.field22, c.field3C);
                    }
            if (bld && op2::g_units[bld].action == 0 && c.action == 0) break;
        }
        std::printf("[Produce] t=%d fim: tokamak acao=0x%X | convec acao=%d sub=%d carga=0x%X | %s\n", tb,
                    bld ? op2::g_units[bld].action : -1, c.action, c.field22, c.cargo, op2::statusBarText().c_str());
    }
    renderer.shutdown(); SDL_Quit();
    return made ? 0 : 4;
}

// --researchtest <dll> <prefix>: laboratório do jogador 0 da missão — lista de
// tópicos, painel ASSIGN (modos 0 e 1, screenshots), pacote 0x16 e conclusão.
static int runResearchTest(const std::string& dll, const std::string& prefix) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    op2::g_ResManager.init(".");
    op2::g_art.load();
    op2::Renderer renderer;
    if (!renderer.init("OP2 researchtest", 1280, 960, false)) { SDL_Quit(); return 1; }
    op2::GameState gs;
    if (!gs.loadMission(dll)) { renderer.shutdown(); SDL_Quit(); return 2; }
    gs.onActivate();
    for (int t = 0; t < 40; ++t) gs.onIdle(renderer, 0.2501f / 4.0f);
    int lab = 0;
    for (int i = 1; i < (int)op2::g_units.size() && !lab; ++i) {
        const op2::UnitRecord& r = op2::g_units[i];
        if (r.id != -1 && (r.owner & 0x0F) == 0 && op2::labResearchKind(r.type)) lab = i;
    }
    if (!lab) { std::printf("[Research] sem laboratorio\n"); return 3; }
    const op2::UnitRecord& L = op2::g_units[lab];
    std::printf("[Research] lab id=%d tipo 0x%X operando=%d cientistas livres=%d\n", lab, L.type,
                (int)op2::structureOperational(L), op2::g_colony[0].scientistsLeft);
    const std::vector<int> topics = op2::labTopics(lab);
    for (int t : topics)
        std::printf("    %-40s custo=%d max=%d\n", op2::techAt(t)->name.c_str(), op2::techAt(t)->plymouthCost,
                    op2::techAt(t)->maxScientists);
    if (topics.empty()) return 4;
    gs.setSelection({lab});
    gs.pressCommand(op2::UC_ASSIGN_SCIENTISTS);
    for (int i = 0; i < 2; ++i) gs.onIdle(renderer, 0.05f);
    writeBmp32(prefix + "_topics.bmp", renderer.getFramebuffer(), op2::Renderer::NATIVE_WIDTH, op2::Renderer::NATIVE_HEIGHT);
    gs.researchPaneForTest().open(lab, 0);
    // escolhe o 1o tópico (como o clique na lista) e passa ao modo 1
    gs.researchPaneForTest().selectForTest(topics[0]);
    gs.pressCommand(-5);   // kCmdAssign
    for (int i = 0; i < 2; ++i) gs.onIdle(renderer, 0.05f);
    writeBmp32(prefix + "_assign.bmp", renderer.getFramebuffer(), op2::Renderer::NATIVE_WIDTH, op2::Renderer::NATIVE_HEIGHT);
    gs.researchPaneForTest().more();
    gs.pressCommand(-7);   // kCmdStart
    std::printf("[Research] START %s: lab acao=0x%X cientistas=%d\n", op2::techAt(topics[0])->name.c_str(), L.action,
                (int)L.extraScientists);
    // STOP com confirmação: abre, screenshot, YES → lab parado e sem cientistas; depois recomeça.
    if (L.action != 0x16) op2::sendStartResearch(lab, topics[0], 1);
    for (int i = 0; i < 20; ++i) gs.onIdle(renderer, 0.2501f / 4.0f);
    std::printf("[Research] antes do STOP: lab acao=0x%X sub=0x%X cientistas=%d\n", L.action, L.field22, (int)L.extraScientists);
    gs.setSelection({lab});
    gs.pressCommand(op2::UC_STOP_RESEARCH);
    for (int i = 0; i < 2; ++i) gs.onIdle(renderer, 0.05f);
    writeBmp32(prefix + "_stop.bmp", renderer.getFramebuffer(), op2::Renderer::NATIVE_WIDTH, op2::Renderer::NATIVE_HEIGHT);
    gs.pressCommand(-9);   // kCmdYes
    for (int i = 0; i < 3; ++i) gs.onIdle(renderer, 0.2501f / 4.0f);
    std::printf("[Research] STOP+YES: lab acao=0x%X sub=0x%X topico=%d cientistas=%d\n", L.action, L.field22, L.labTopic,
                (int)L.extraScientists);
    op2::sendStartResearch(lab, topics[0], 5);
    // University: treino de alunos.
    int uni = 0;
    for (int i = 1; i < (int)op2::g_units.size() && !uni; ++i) {
        const op2::UnitRecord& r = op2::g_units[i];
        if (r.id != -1 && (r.owner & 0x0F) == 0 && r.type == 0x26) uni = i;
    }
    if (uni) {
        const op2::UnitRecord& U = op2::g_units[uni];
        gs.setSelection({uni});
        gs.pressCommand(op2::UC_ASSIGN_STUDENTS);
        for (int i = 0; i < 2; ++i) gs.onIdle(renderer, 0.05f);
        writeBmp32(prefix + "_students.bmp", renderer.getFramebuffer(), op2::Renderer::NATIVE_WIDTH, op2::Renderer::NATIVE_HEIGHT);
        const int w0 = op2::g_playerData[0].workers, s0 = op2::g_playerData[0].scientists;
        gs.pressCommand(-11);  // kCmdStartClass
        if (U.action != 0x17) op2::sendTrain(uni, 2);   // sem trabalhadores livres: força uma turma de 2
        std::printf("[Research] University id=%d operando=%d: acao=0x%X alunos=%d tempo=%d\n", uni,
                    (int)op2::structureOperational(U), U.action, U.extraWorkers, U.trainTime);
        int tu = 0;
        for (; tu < 20000 && U.action == 0x17; ++tu) gs.onIdle(renderer, 0.2501f / 4.0f);
        std::printf("[Research] treino fim t=%d: trabalhadores %d->%d cientistas %d->%d | %s\n", tu, w0,
                    op2::g_playerData[0].workers, s0, op2::g_playerData[0].scientists, op2::statusBarText().c_str());
        gs.setSelection({lab});
    }
    int t = 0;
    for (; t < 60000 && (L.action == 0x16); ++t) {
        gs.onIdle(renderer, 0.2501f / 4.0f);
        if (t == 3) std::printf("[Research] t=3 sub=0x%X pontos=%d\n", L.field22, L.field3C);
    }
    std::printf("[Research] t=%d fim: acao=0x%X pesquisado=%d | %s\n", t, L.action,
                (int)op2::researchHas(0, topics[0]), op2::statusBarText().c_str());
    renderer.shutdown(); SDL_Quit();
    return 0;
}

// --colonytest <map>: base mínima (CC + Tokamak e Agridome encostados + Lab
// isolado) → economia (sub_471F30) → liga o Lab por tubos (cellType 0x1A).
static int runColonyTest(const std::string& mapName) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    op2::g_ResManager.init(".");
    op2::Renderer renderer;
    if (!renderer.init("OP2 colonytest", 1280, 960, false)) { SDL_Quit(); return 1; }
    op2::GameState gs;
    if (!gs.load(mapName)) { renderer.shutdown(); SDL_Quit(); return 2; }
    op2::resetUnits();
    const int W = op2::g_gameMap.width(), H = op2::g_gameMap.height();
    int bx = -1, by = -1;
    for (int y = 3; y < H - 6 && bx < 0; ++y)
        for (int x = 3; x < W - 16 && bx < 0; ++x) {
            bool ok = true;
            for (int dy = -1; dy <= 3 && ok; ++dy)
                for (int dx = -2; dx <= 13 && ok; ++dx)
                    if (!op2::g_gameMap.isPassable(op2::LOCATION{x + dx, y + dy})) ok = false;
            if (ok) { bx = x; by = y; }
        }
    if (bx < 0) { std::printf("[Colony] sem area livre\n"); return 3; }
    op2::Unit cc, tok, agr, lab;
    op2::TethysGame::CreateUnit(cc, op2::mapCommandCenter, op2::LOCATION{bx, by}, 0);
    op2::TethysGame::CreateUnit(tok, op2::mapTokamak, op2::LOCATION{bx + 3, by}, 0);
    op2::TethysGame::CreateUnit(agr, 0x27 /*AGRIDOME*/, op2::LOCATION{bx, by + 2}, 0);
    op2::TethysGame::CreateUnit(lab, 0x31 /*LAB_STANDARD*/, op2::LOCATION{bx + 10, by}, 0);
    auto dump = [&](const char* fase) {
        const op2::ColonyState& c = op2::g_colony[0];
        std::printf("[Colony] %s: energia ger=%d exig=%d sobra=%d  trab sobra=%d cient sobra=%d  operando=%d/%d\n",
                    fase, c.powerGenerated, c.powerRequired, c.powerLeft,
                    c.workersLeft, c.scientistsLeft, c.activeCount, c.structCount);
        const char* nm[] = {"CC", "Tokamak", "Agridome", "Lab"};
        op2::Unit* us[] = {&cc, &tok, &agr, &lab};
        for (int i = 0; i < 4; ++i) {
            const op2::UnitRecord& r = op2::g_units[us[i]->id()];
            std::printf("    %-9s energia=%d trab=%d cient=%d operando=%d\n", nm[i],
                        (r.flags & op2::UNIT_POWERED) != 0, (r.flags & op2::UNIT_WORKERS) != 0,
                        (r.flags & op2::UNIT_SCIENTISTS) != 0, op2::structureOperational(r));
        }
    };
    std::printf("[Colony] ordem da lista (prioridade):");
    for (int id : op2::colonyStructures(0))
        std::printf(" %s", op2::structureCodeName(op2::g_units[id].type));
    std::printf("\n");
    for (int t = 0; t < 40; ++t) { op2::TethysGame::advanceTick(); op2::tickColony(); }
    dump("sem tubo");
    // Tubos ligando o Tokamak (x bx+2..bx+3) ao Lab (x bx+9..bx+11) na linha by.
    for (int x = bx + 4; x <= bx + 8; ++x)
        op2::g_gameMap.setTileAndCellType(x, by, op2::g_gameMap.getTile(op2::LOCATION{x, by}), 0x1A);
    op2::colonyMarkDirty(0);
    for (int t = 0; t < 2; ++t) { op2::TethysGame::advanceTick(); op2::tickColony(); }
    dump("com tubo");
    // Comida/populacao/moral (vez do jogador 0 a cada 256 ticks): 10 ciclos.
    op2::PlayerData& pd = op2::g_playerData[0];
    std::printf("[Colony] antes: comida=%d  trab=%d cient=%d criancas=%d  minerio=%d (cap %d)\n",
                pd.foodStored, pd.workers, pd.scientists, pd.kids, pd.ore, op2::g_colony[0].oreCapacity);
    for (int k = 0; k < 10; ++k) {
        // A vez de cada jogador (comida/moral) vem a cada 8 slots de 32 = 256 ticks.
        for (int t = 0; t < 256; ++t) { op2::TethysGame::advanceTick(); op2::tickColony(); }
        if (k == 4) {   // a partir do 5o ciclo: residencia encostada no Agridome
            op2::Unit res;
            op2::TethysGame::CreateUnit(res, 0x1E /*RESIDENCE*/, op2::LOCATION{bx + 3, by + 2}, 0);
            std::printf("[Colony]   + Residence\n");
        }
        const op2::ColonyState& m = op2::g_colony[0];
        std::printf("[Colony]   ciclo %2d: moral valor=%d nivel=%d tendencia=%+d  eventos=%d lotacao=%d comida=%d deslig=%d\n",
                    k + 1, m.moraleValue, pd.morale, m.moraleTrend, m.moraleEvents, m.crowding, m.foodState, m.disabledPct);
    }
    {   // Moral forcada (fiel, com o XOR do original): Good sozinho vale Great.
        op2::TethysGame::ForceMoraleGood(0);
        for (int t = 0; t < 256; ++t) { op2::TethysGame::advanceTick(); op2::tickColony(); }
        std::printf("[Colony] ForceMoraleGood: valor=%d nivel=%d\n", op2::g_colony[0].moraleValue, pd.morale);
        op2::TethysGame::FreeMoraleLevel(0);
        for (int t = 0; t < 256; ++t) { op2::TethysGame::advanceTick(); op2::tickColony(); }
        std::printf("[Colony] FreeMoraleLevel:  valor=%d nivel=%d\n", op2::g_colony[0].moraleValue, pd.morale);
    }
    const op2::ColonyState& c = op2::g_colony[0];
    std::printf("[Colony] 10 ciclos: comida=%d (produz %d, come %d, saldo %d)  trab=%d cient=%d criancas=%d\n",
                pd.foodStored, c.foodProduced, c.foodEaten, c.foodNet, pd.workers, pd.scientists, pd.kids);
    renderer.shutdown(); SDL_Quit();
    return 0;
}

// --combattest <map> <prefix>: Laser Lynx + Rail Gun Tiger (jogador 0) contra
// Microwave Lynx + RPG Tiger (jogador 1, IA) a 3 tiles; roda o laço real e
// registra alvos, disparos, dano e mortes; grava quadros <prefix>_tNNN.bmp.
static int runCombatTest(const std::string& mapName, const std::string& prefix) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    op2::g_ResManager.init(".");
    op2::g_art.load();
    op2::Renderer renderer;
    if (!renderer.init("OP2 combattest", 1280, 960, false)) { SDL_Quit(); return 1; }
    op2::GameState gs;
    if (!gs.load(mapName)) { renderer.shutdown(); SDL_Quit(); return 2; }
    gs.onActivate();
    op2::resetUnits();
    const int W = op2::g_gameMap.width(), H = op2::g_gameMap.height();
    for (int y = 0; y < H; ++y)                      // tira a ocupação da cena demo
        for (int x = 0; x < W; ++x) op2::g_gameMap.clearUnitOccupancy(x, y);
    int bx = -1, by = -1;
    for (int y = H / 2; y < H - 6 && bx < 0; ++y)
        for (int x = W / 2; x < W - 8 && bx < 0; ++x) {
            bool ok = true;
            for (int dy = -1; dy <= 3 && ok; ++dy)
                for (int dx = -1; dx <= 5 && ok; ++dx)
                    if (!op2::g_gameMap.isPassable(op2::LOCATION{x + dx, y + dy})) ok = false;
            if (ok) { bx = x; by = y; }
        }
    if (bx < 0) { std::printf("[Combat] sem area livre\n"); return 3; }
    op2::Unit a, b, c, d;
    op2::TethysGame::CreateUnit(a, op2::mapLynx, op2::LOCATION{bx, by}, 0, 0x3D);
    op2::TethysGame::CreateUnit(b, op2::mapTiger, op2::LOCATION{bx, by + 2}, 0, 0x3F);
    op2::TethysGame::CreateUnit(c, op2::mapLynx, op2::LOCATION{bx + 3, by}, 1, 0x3E);
    op2::TethysGame::CreateUnit(d, op2::mapTiger, op2::LOCATION{bx + 3, by + 2}, 1, 0x40);
    const int ids[4] = {a.id(), b.id(), c.id(), d.id()};
    for (int id : ids)
        std::printf("[Combat] %d: %s  HP=%d\n", id, op2::unitTitle(op2::g_units[id]).c_str(),
                    op2::vehicleSheet(op2::g_units[id].type, 0)->hitPoints);
    gs.setMouse(200, 150);
    gs.setCamera(bx * 32 - 200 + 48, by * 32 - 240 + 48);
    gs.setSelection({ids[0], ids[1]});
    int lastDmg[4] = {0, 0, 0, 0}, lastTgt[4] = {0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF};
    int shots = 0;
    for (int t = 1; t <= 600; ++t) {
        op2::TethysGame::advanceTick();
        op2::TethysGame::advanceDaylight(op2::g_gameMap.internalWidth());
        op2::TethysGame::getRand(100);
        op2::tickColony();
        op2::tickMovement();
        const size_t before = op2::g_units.size();
        int projBefore = 0;
        for (size_t i = 1; i < before; ++i)
            if (op2::g_units[i].id != -1 && op2::isWeaponProjectile(op2::g_units[i].type) &&
                op2::g_units[i].field22 == 0x12) ++projBefore;
        op2::tickUnits();
        for (int k = 0; k < 4; ++k) {
            const op2::UnitRecord& r = op2::g_units[ids[k]];
            if (r.id == -1) { if (lastDmg[k] >= 0) std::printf("[Combat] t=%d  %d removido\n", t, ids[k]); lastDmg[k] = -1; continue; }
            const int tg = r.hasTarget() ? r.target4C : 0x7FFF;
            if (tg != lastTgt[k]) {
                std::printf("[Combat] t=%d  %d alvo -> %d (torre %d)\n", t, ids[k], tg == 0x7FFF ? -1 : tg, r.turretDir);
                lastTgt[k] = tg;
            }
            if (r.damage != lastDmg[k] && lastDmg[k] >= 0) {
                std::printf("[Combat] t=%d  %d dano %d -> %d%s\n", t, ids[k], lastDmg[k], r.damage,
                            (r.flags & op2::UNIT_DEAD) ? " (MORTO)" : "");
                lastDmg[k] = r.damage;
            }
        }
        int projNow = 0;
        for (size_t i = 1; i < op2::g_units.size(); ++i)
            if (op2::g_units[i].id != -1 && op2::isWeaponProjectile(op2::g_units[i].type)) ++projNow;
        (void)projBefore;
        static int lastProj = 0;
        if (projNow > lastProj) shots += projNow - lastProj;
        lastProj = projNow;
        if (t == 8 || t == 12 || t == 18 || t == 40 || t == 60 || t == 120 || t == 136 || t == 200 || t == 400) {
            gs.onIdle(renderer, 0.0f);
            char fn[256]; std::snprintf(fn, sizeof(fn), "%s_t%03d.bmp", prefix.c_str(), t);
            writeBmp32(fn, renderer.getFramebuffer(), op2::Renderer::NATIVE_WIDTH, op2::Renderer::NATIVE_HEIGHT);
        }
    }
    std::printf("[Combat] projeteis criados (aprox.): %d\n", shots);
    renderer.shutdown(); SDL_Quit();
    return 0;
}

// --missiontest <DLL> <out.bmp>: carrega a missão pela DLL (loadMission),
// roda ~200 ticks e grava a tela; lista unidades e objetivos.
static int runMissionTest(const std::string& dll, const std::string& outPath, int ticks) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    op2::g_ResManager.init(".");
    op2::g_art.load();
    op2::Renderer renderer;
    if (!renderer.init("OP2 missiontest", 1280, 960, false)) { SDL_Quit(); return 1; }
    op2::GameState gs;
    if (!gs.loadMission(dll)) { std::printf("[MissionTest] falhou\n"); renderer.shutdown(); SDL_Quit(); return 2; }
    gs.onActivate();
    for (size_t i = 1; i < op2::g_units.size(); ++i) {
        const op2::UnitRecord& r = op2::g_units[i];
        if (r.id == -1) continue;
        std::printf("[MissionTest] %2d tipo 0x%02X dono %d tile (%d,%d)\n", r.id, r.type, r.owner & 0x0F,
                    op2::unitOX(r) >> 5, op2::unitOY(r) >> 5);
    }
    std::string lastMsg;
    for (int k = 0; k < ticks; ++k) {
        gs.onIdle(renderer, 0.2501f / 4.0f);
        const std::string m = op2::statusBarText();
        if (m != lastMsg) { std::printf("[MissionTest] t=%d msg: %s\n", op2::TethysGame::tick(), m.c_str()); lastMsg = m; }
    }
    {   int hurt = 0, dead = 0;
        for (size_t i = 1; i < op2::g_units.size(); ++i) {
            const op2::UnitRecord& r = op2::g_units[i];
            if (r.id == -1) continue;
            if (r.flags & op2::UNIT_DEAD) ++dead; else if (r.damage > 0 && (r.flags & op2::UNIT_DAMAGEABLE)) ++hurt;
        }
        std::printf("[MissionTest] unidades danificadas=%d mortas=%d\n", hurt, dead);
    }
    for (const std::string& o : op2::missionObjectives()) std::printf("[MissionTest] objetivo: %s\n", o.c_str());
    {   // pesquisa do jogador 0: nº de tecnologias e alguns blocos melhorados
        int n = 0, up = 0;
        for (int i = 1; i < op2::techCount(); ++i)
            if (op2::techAt(i)->researched & 1u) { ++n; up += static_cast<int>(op2::techAt(i)->upgrades.size()); }
        const op2::VehPlayerStats* cv0 = op2::vehicleSheet(op2::mapConVec, 0);
        const op2::VehPlayerStats* cv1 = op2::vehicleSheet(op2::mapConVec, 1);
        std::printf("[MissionTest] pesquisa j0: %d tecnologias (%d melhorias); ConVec HP j0=%d j1=%d prodRate j0=%d j1=%d\n",
                    n, up, cv0->hitPoints, cv1->hitPoints, cv0->productionRate, cv1->productionRate);
    }
    std::printf("[MissionTest] tick=%d fim=%d\n", op2::TethysGame::tick(), op2::gameEndCode());
    bool ok = writeBmp32(outPath, renderer.getFramebuffer(), op2::Renderer::NATIVE_WIDTH, op2::Renderer::NATIVE_HEIGHT);
    renderer.shutdown(); SDL_Quit();
    return ok ? 0 : 1;
}

// --quaketest <map>: terremoto de magnitude 3 no CC da cena demo; imprime o
// dano por unidade e o tremor de tela.
static int runQuakeTest(const std::string& mapName) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    op2::g_ResManager.init(".");
    op2::g_art.load();
    op2::Renderer renderer;
    if (!renderer.init("OP2 quaketest", 1280, 960, false)) { SDL_Quit(); return 1; }
    op2::GameState gs;
    if (!gs.load(mapName)) { renderer.shutdown(); SDL_Quit(); return 2; }
    gs.onActivate();
    int cc = 0;
    for (size_t i = 1; i < op2::g_units.size(); ++i)
        if (op2::g_units[i].id != -1 && op2::g_units[i].type == op2::mapCommandCenter) { cc = (int)i; break; }
    if (!cc) return 3;
    op2::setEarthquake(op2::g_units[cc].pixelX >> 5, op2::g_units[cc].pixelY >> 5, 3);
    int maxShake = 0;
    for (int k = 0; k < 1300; ++k) {
        gs.onIdle(renderer, 0.2501f / 4.0f);
        const int s = std::abs(gs.shakeX()) + std::abs(gs.shakeY());
        if (s > maxShake) maxShake = s;
    }
    for (size_t i = 1; i < op2::g_units.size(); ++i) {
        const op2::UnitRecord& r = op2::g_units[i];
        if (r.id == -1 || !(r.flags & op2::UNIT_DAMAGEABLE)) continue;
        std::printf("[QuakeTest] %2d tipo 0x%02X dano=%d%s\n", r.id, r.type, r.damage, (r.flags & op2::UNIT_DEAD) ? " (morto)" : "");
    }
    std::printf("[QuakeTest] tick=%d tremor max=%d msg=%s\n", op2::TethysGame::tick(), maxShake, op2::statusBarText().c_str());
    renderer.shutdown(); SDL_Quit();
    return 0;
}

// --disastertest <map> <meteor|lightning|tornado|eruption> <prefix>: desastre
// perto do CC da cena demo; imprime o dano por unidade, a lava e grava quadros
// <prefix>_NN.bmp com a câmera no desastre.
static int runDisasterTest(const std::string& mapName, const std::string& kind, const std::string& prefix) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    op2::g_ResManager.init(".");
    op2::g_art.load();
    op2::Renderer renderer;
    if (!renderer.init("OP2 disastertest", 1280, 960, false)) { SDL_Quit(); return 1; }
    op2::GameState gs;
    if (!gs.load(mapName)) { renderer.shutdown(); SDL_Quit(); return 2; }
    gs.onActivate();
    int cc = 0;
    for (size_t i = 1; i < op2::g_units.size(); ++i)
        if (op2::g_units[i].id != -1 && op2::g_units[i].type == op2::mapCommandCenter) { cc = (int)i; break; }
    if (!cc) return 3;
    const int cx = op2::g_units[cc].pixelX >> 5, cy = op2::g_units[cc].pixelY >> 5;
    const int W = op2::g_gameMap.width(), H = op2::g_gameMap.height();
    int type = 0;
    if (kind == "meteor")    { type = 0x50; op2::setMeteor(cx, cy, 2); }
    if (kind == "lightning") { type = 0x4C; op2::setLightning((cx - 6 + W) % W, cy, 3, (cx + 12) % W, cy); }
    if (kind == "tornado")   { type = 0x4D; op2::setTornado((cx - 8 + W) % W, cy, 25, (cx + 12) % W, cy, 1); }
    if (kind == "eruption") {
        type = 0x4F;
        for (int y = cy - 3; y <= cy + 12; ++y)
            for (int x = cx + 2; x <= cx + 16; ++x)
                if (y >= 0 && y < H) op2::setLavaPossible(x % W, y, true);
        op2::setEruption((cx + 8) % W, cy + 4, 40);
    }
    if (!type) return 4;
    auto lavaCells = [&]() {
        int n = 0;
        for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) if (op2::g_gameMap.rawCell(x, y) & 0x8000000u) ++n;
        return n;
    };
    std::vector<int> lastDmg(op2::g_units.size(), 0);
    int start = -1, shots = 0, lastX = 0, lastY = 0;
    for (int k = 0; k < 2600; ++k) {
        gs.onIdle(renderer, 0.2501f / 4.0f);
        int did = 0;
        for (size_t i = 1; i < op2::g_units.size(); ++i)
            if (op2::g_units[i].id != -1 && op2::g_units[i].type == type) { did = (int)i; break; }
        if (type == 0x4F && start < 0 && lavaCells() > 0) {        // erupção: a 1ª lava
            start = op2::TethysGame::tick();
            lastX = (((cx + 8) % W) << 5) + 16; lastY = ((cy + 4) << 5) + 16;
            std::printf("[Disaster] lava comecou tick=%d\n", start);
        }
        if (did) {
            const op2::UnitRecord& d = op2::g_units[did];
            lastX = op2::unitOX(d); lastY = op2::unitOY(d);
            if (start < 0 && (d.flags & 0x2000u)) {
                start = op2::TethysGame::tick();
                std::printf("[Disaster] %s comecou tick=%d em (%d,%d) px\n", kind.c_str(), start, lastX, lastY);
            }
        }
        if (lastDmg.size() < op2::g_units.size()) lastDmg.resize(op2::g_units.size(), 0);
        for (size_t i = 1; i < op2::g_units.size(); ++i) {
            const op2::UnitRecord& r = op2::g_units[i];
            if (r.id == -1 || !(r.flags & op2::UNIT_DAMAGEABLE) || r.damage == lastDmg[i]) continue;
            std::printf("[Disaster] t=%d unidade %d tipo 0x%02X dano %d -> %d%s\n", op2::TethysGame::tick(), (int)i,
                        r.type, lastDmg[i], r.damage, (r.flags & op2::UNIT_DEAD) ? " (MORTA)" : "");
            lastDmg[i] = r.damage;
        }
        static int nextSnap = 0;
        if (start >= 0 && nextSnap == 0) nextSnap = start + 4;
        const int every = type == 0x4F ? 120 : type == 0x50 ? 8 : 24;
        if (start >= 0 && shots < 12 && op2::TethysGame::tick() >= nextSnap) {
            nextSnap += every;
            gs.setCamera(lastX - 240, lastY - 220);
            gs.onIdle(renderer, 0.0f);
            char fn[256]; std::snprintf(fn, sizeof(fn), "%s_%02d.bmp", prefix.c_str(), shots++);
            writeBmp32(fn, renderer.getFramebuffer(), op2::Renderer::NATIVE_WIDTH, op2::Renderer::NATIVE_HEIGHT);
        }
        if (start >= 0 && !did && type != 0x4F && op2::TethysGame::tick() > start + 40) break;
    }
    std::printf("[Disaster] fim tick=%d lava=%d msg=%s\n", op2::TethysGame::tick(), lavaCells(),
                op2::statusBarText().c_str());
    renderer.shutdown(); SDL_Quit();
    return 0;
}

// --movelog <map>: manda o 1º veículo andar ~10 células e imprime o
// deslocamento (px) por tick — prova que o deslize é contínuo (sem tick parado).
static int runMoveLog(const std::string& mapName) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    op2::g_ResManager.init(".");
    op2::Renderer renderer;
    if (!renderer.init("OP2 movelog", 1280, 960, false)) { SDL_Quit(); return 1; }
    op2::GameState gs;
    if (!gs.load(mapName)) { renderer.shutdown(); SDL_Quit(); return 2; }
    int vid = -1;
    for (int i = 1; i < (int)op2::g_units.size(); ++i)
        if (op2::g_units[i].id != -1 && (op2::g_units[i].flags & op2::UNIT_VEHICLE)) { vid = i; break; }
    if (vid < 0) return 3;
    op2::UnitRecord& r = op2::g_units[vid];
    int mask = op2::g_gameMap.width() - 1;
    op2::LOCATION dst = op2::g_gameMap.findPassableNear(
        op2::LOCATION{((r.pixelX >> 5) + 10) & mask, r.pixelY >> 5});
    op2::Unit(vid).DoMove(dst);
    std::printf("[MoveLog] veiculo %d tipo %d -> (%d,%d)\n", vid, r.type, dst.x, dst.y);
    int px = r.pixelX, py = r.pixelY;
    std::string line;
    for (int t = 0; t < 400 && op2::g_units[vid].hasOrder; ++t) {
        op2::tickMovement();
        op2::TethysGame::advanceTick();
        const op2::UnitRecord& u = op2::g_units[vid];
        int dx = u.pixelX - px, dy = u.pixelY - py;
        if (dx > 1000) dx -= op2::g_gameMap.width() * 32;
        if (dx < -1000) dx += op2::g_gameMap.width() * 32;
        char b[32]; std::snprintf(b, sizeof(b), "%d,%d r%d ", dx, dy, u.rotation);
        line += b;
        px = u.pixelX; py = u.pixelY;
    }
    std::printf("[MoveLog] dx,dy por tick: %s\n", line.c_str());
    renderer.shutdown(); SDL_Quit();
    return 0;
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
        op2::TethysGame::CreateUnit(bld, op2::mapCommandCenter, op2::LOCATION{bx, by}, 0);  // 3x2
        int fw, fh; op2::buildingFootprint(op2::mapCommandCenter, fw, fh);
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
    std::cout << "   header[0..8] =";
    for (int k = 0; k < 9; ++k) std::cout << " " << (int32_t)an.header[k];
    std::cout << "   bbox(pics)=(" << minX << "," << minY << ")..(" << maxX << "," << maxY << ")"
              << std::endl;
    for (const auto& p : fr.pics) {
        const op2::ArtImage& m = op2::g_art.image(p.image);
        std::cout << "   pic img=" << p.image << " type=0x" << std::hex << m.type
                  << std::dec << " pal=" << m.pal << " (type&1="
                  << (m.type & 1) << " shadow=" << (int)m.isShadow() << ") "
                  << m.width << "x" << m.height
                  << " stride=" << m.stride << " rowBytes=" << m.rowBytes()
                  << " slot=" << (int)p.slot << " @(" << p.x << "," << p.y << ")" << std::endl;
    }
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

// --animlist "i1,i2,..." <out.bmp>: grade com o frame 0 das animacoes da lista.
static int runAnimList(const std::string& list, const std::string& outPath) {
    if (!op2::g_ResManager.init(".")) return 2;
    if (!op2::g_art.load()) return 3;
    std::vector<int> idx; { std::string t; for (char c : list) { if (c==',') { idx.push_back(atoi(t.c_str())); t.clear(); } else t+=c; } if(!t.empty()) idx.push_back(atoi(t.c_str())); }
    const int CELL = 90, COLS = 6;
    int rows = ((int)idx.size() + COLS - 1) / COLS;
    int W = COLS * CELL, H = rows * CELL;
    std::vector<uint32_t> cv((size_t)W * H, 0xFF202020u);
    for (size_t k = 0; k < idx.size(); ++k) {
        int ai = idx[k];
        if (ai < 0 || ai >= op2::g_art.numAnimations()) continue;
        const op2::ArtAnimation& an = op2::g_art.animation(ai);
        if (an.frames.empty()) continue;
        const op2::ArtFrame& fr = an.frames[0];
        int minX=1e9,minY=1e9,maxX=-1e9,maxY=-1e9;
        for (auto&p:fr.pics){const auto&m=op2::g_art.image(p.image);minX=std::min(minX,(int)p.x);minY=std::min(minY,(int)p.y);maxX=std::max(maxX,(int)p.x+(int)m.width);maxY=std::max(maxY,(int)p.y+(int)m.height);}
        if (fr.pics.empty()) continue;
        int bw=maxX-minX,bh=maxY-minY,cx=((int)k%COLS)*CELL,cy=((int)k/COLS)*CELL;
        int ox=cx+(CELL-bw)/2-minX,oy=cy+(CELL-bh)/2-minY;
        std::vector<op2::ArtPicture> pics=fr.pics; std::sort(pics.begin(),pics.end(),[](const op2::ArtPicture&a,const op2::ArtPicture&b){return a.slot<b.slot;});
        for (auto&p:pics){const auto&m=op2::g_art.image(p.image);std::vector<uint32_t> px;op2::g_art.decode(p.image,px,true);
            for(uint32_t y=0;y<m.height;++y)for(uint32_t x=0;x<m.width;++x){uint32_t c=px[y*m.width+x];if(!(c>>24))continue;int X=ox+p.x+(int)x,Y=oy+p.y+(int)y;if(X>=cx&&X<cx+CELL&&Y>=cy&&Y<cy+CELL&&X<W&&Y<H)cv[(size_t)Y*W+X]=c|0xFF000000u;}}
    }
    writeBmp32(outPath, cv.data(), W, H);
    std::cout << "[AnimList] " << idx.size() << " anims (" << COLS << "/linha) -> " << outPath << std::endl;
    return 0;
}

int main(int argc, char* argv[]) {
    // Saída sem buffer: os modos de teste rodam com stdout em pipe e um crash
    // não pode engolir o log.
    std::setvbuf(stdout, nullptr, _IONBF, 0);
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
        if (std::strcmp(argv[i], "--structtest") == 0 && i + 2 < argc)
            return runStructTest(argv[i + 1], argv[i + 2]);
        if (std::strcmp(argv[i], "--paneshot") == 0 && i + 2 < argc)
            return runPaneShot(argv[i + 1], argv[i + 2], i + 3 < argc ? atoi(argv[i + 3]) : 0);
        if (std::strcmp(argv[i], "--cmdshot") == 0 && i + 3 < argc)
            return runCmdShot(argv[i + 1], argv[i + 2], (int)std::strtol(argv[i + 3], nullptr, 0),
                              i + 4 < argc ? atoi(argv[i + 4]) : 1, i + 5 < argc ? (int)std::strtol(argv[i + 5], nullptr, 0) : 0);
        if (std::strcmp(argv[i], "--cmdtest") == 0 && i + 1 < argc)
            return runCmdTest(argv[i + 1]);
        if (std::strcmp(argv[i], "--quaketest") == 0 && i + 1 < argc)
            return runQuakeTest(argv[i + 1]);
        if (std::strcmp(argv[i], "--disastertest") == 0 && i + 3 < argc)
            return runDisasterTest(argv[i + 1], argv[i + 2], argv[i + 3]);
        if (std::strcmp(argv[i], "--missiontest") == 0 && i + 2 < argc)
            return runMissionTest(argv[i + 1], argv[i + 2], i + 3 < argc ? atoi(argv[i + 3]) : 200);
        if (std::strcmp(argv[i], "--combattest") == 0 && i + 2 < argc)
            return runCombatTest(argv[i + 1], argv[i + 2]);
        if (std::strcmp(argv[i], "--producetest") == 0 && i + 2 < argc)
            return runProduceTest(argv[i + 1], argv[i + 2]);
        if (std::strcmp(argv[i], "--researchtest") == 0 && i + 2 < argc)
            return runResearchTest(argv[i + 1], argv[i + 2]);
        if (std::strcmp(argv[i], "--colonytest") == 0 && i + 1 < argc)
            return runColonyTest(argv[i + 1]);
        if (std::strcmp(argv[i], "--movelog") == 0 && i + 1 < argc)
            return runMoveLog(argv[i + 1]);
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
        if (std::strcmp(argv[i], "--animlist") == 0 && i + 2 < argc)
            return runAnimList(argv[i + 1], argv[i + 2]);
    }

    // --play <map>: entra direto no jogo com o mapa (via o backbone TApp).
    // --mission <DLL>: entra direto numa missão (ex.: T01.DLL).
    std::string initialMap, initialMission;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--play") == 0 && i + 1 < argc) initialMap = argv[i + 1];
        if (std::strcmp(argv[i], "--mission") == 0 && i + 1 < argc) initialMission = argv[i + 1];
    }

    std::cout << "========================================" << std::endl;
    std::cout << "  Outpost 2: Divided Destiny Modern Port " << std::endl;
    std::cout << "  Engine Version: 1.0.0 (MSVC + SDL2)   " << std::endl;
    std::cout << "========================================" << std::endl;

    // Backbone TApp (asm): Init -> CreateFrames -> Run -> ShutDown.
    if (!op2::gTApp.init())                   { op2::gTApp.shutdown(); return 1; }
    if (!op2::gTApp.createFrames(initialMap, initialMission)) { op2::gTApp.shutdown(); return 1; }
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
