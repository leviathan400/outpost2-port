#include "game_state.h"

#include <algorithm>
#include <iostream>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "tapp.h"
#include "info_pane.h"
#include "../game/production.h"
#include "../game/construction.h"
#include "../game/mining.h"
#include "../game/earthwork.h"
#include "../gfx/renderer.h"
#include "../gfx/font.h"
#include "../gfx/ui_font.h"
#include "../res/res_manager.h"
#include "../core/config.h"
#include "../audio/sound.h"
#include "../game/tethys_game.h"
#include "../game/player.h"
#include "../game/unit.h"
#include "../game/command.h"
#include "../game/pathfinder.h"
#include "../game/movement.h"
#include "../game/vehicle_anim.h"
#include "../game/structure_anim.h"
#include "../game/missile.h"
#include "../game/salvage.h"
#include "../audio/music.h"
#include "../game/structure.h"
#include "../game/building_sheet.h"
#include "../game/colony.h"
#include "../game/messages.h"
#include "../game/weapon.h"
#include "../game/trigger.h"
#include "../game/research.h"
#include "../game/disaster.h"
#include "../game/commands.h"
#include "../game/scenario.h"
#include "../game/ai_group.h"
#include "../game/start_info.h"
#include "../game/save_game.h"
#include "../game/save_stream.h"
#include "../mission/mission.h"
#include "../gfx/artfile.h"
#include "../gfx/art_blit.h"

namespace op2 {

GameState::GameState() = default;
GameState::~GameState() = default;

void GameState::loadSettings() {
    // Cadência do tick (sub_489160 relê "GameSpeed" do INI ao iniciar a partida;
    // sub_49BF80: intervalo = 1000 / clamp(GameSpeed*4, 4, 40) ms).
    Config config("OUTPOST2.INI");
    GameSettings gs;
    if (config.load()) gs.loadFrom(config);
    int speed = gs.gameSpeed;
    if (speed > 10) speed = 10;
    if (speed < 1) speed = 1;
    int tps = speed * 4;
    if (tps < 4) tps = 4;
    if (tps > 40) tps = 40;
    m_tickPeriod = 1.0f / static_cast<float>(tps);
    setGameSpeedValue(speed * 4);                 // TethysGame +0x50 (GameStartInfo +0xAE)
    setAmbientAnimations(gs.ambient != 0);   // "Ambient" → dword_575628
    m_stationaryShadows = gs.stationaryShadows != 0;   // dword_575634
    m_mobileShadows = gs.mobileShadows != 0;           // dword_575638
    m_overlay = gs.overlay != 0;                       // "Overlay" → dword_57562C
    m_frameSkip = gs.frameSkip;                        // Value (FrameSkip)
}

bool GameState::loadMap(const std::string& mapName) {
    m_mapName = mapName;
    auto stream = g_ResManager.openStream(mapName);
    if (!stream || !g_gameMap.load(stream->data(), stream->size())) {
        std::cerr << "[GameState] mapa invalido: " << mapName << std::endl;
        return false;
    }
    if (!m_terrain.init(g_gameMap, g_ResManager)) {
        std::cerr << "[GameState] falha ao carregar tilesets de " << mapName << std::endl;
        return false;
    }
    const int VW = Renderer::NATIVE_WIDTH, VH = Renderer::NATIVE_HEIGHT;
    m_maxX = m_terrain.mapPixelWidth() - viewportW();
    m_maxY = m_terrain.mapPixelHeight() - viewportH();
    if (m_maxY < 0) m_maxY = 0;
    // começa na área jogável (clipRect), como o jogo
    m_camX = g_gameMap.clipRect().x1 * 32;
    m_camY = g_gameMap.clipRect().y1 * 32;
    clampCamera();
    return true;
}

// Estado da interface no jogo salvo (o lugar de sub_4083B0): câmera e a
// GameStartInfo (TethysGame+0xA4 no original).
std::vector<uint8_t> GameState::uiStateSave() {
    SaveStream s(false);
    s.io(m_camX); s.io(m_camY);
    GameStartInfo& g = m_gsi;
    s.io(g.flags); s.ioStr(g.dll); s.io(g.playerWord); s.io(g.endCode); s.io(g.missionNumber);
    s.io(g.food); s.io(g.ore); s.io(g.rareOre); s.io(g.workers); s.io(g.scientists); s.io(g.kids);
    s.io(g.spaceWord); s.io(g.localPlayer); s.io(g.endTick); s.io(g.playerBlock);
    s.io(g.techCount); s.io(g.techLevel); s.ioVec(g.techIdx); s.ioVec(g.techMask);
    return s.buffer();
}
bool GameState::uiStateLoad(const std::vector<uint8_t>& blob) {
    SaveStream s(true);
    s.setBuffer(blob);
    s.io(m_camX); s.io(m_camY);
    GameStartInfo& g = m_gsi;
    s.io(g.flags); s.ioStr(g.dll); s.io(g.playerWord); s.io(g.endCode); s.io(g.missionNumber);
    s.io(g.food); s.io(g.ore); s.io(g.rareOre); s.io(g.workers); s.io(g.scientists); s.io(g.kids);
    s.io(g.spaceWord); s.io(g.localPlayer); s.io(g.endTick); s.io(g.playerBlock);
    s.io(g.techCount); s.io(g.techLevel); s.ioVec(g.techIdx); s.ioVec(g.techMask);
    clampCamera();
    return s.ok();
}

bool GameState::saveToFile(const std::string& path, const std::string& description) {
    if (!m_mission) return false;
    return saveGameWrite(path, description, uiStateSave());          // sub_48A120
}

bool GameState::loadSavedGame(const std::string& path) {
    SaveHeader h;
    if (!saveReadHeader(path, h)) {
        std::cerr << "[Load] " << path << " nao e um jogo salvo desta versao" << std::endl;
        return false;
    }
    std::cout << "[Load] " << path << ": '" << h.description << "' " << h.dll << " tick " << h.tick << std::endl;
    return loadMission(h.dll, nullptr, &path);
}

bool GameState::loadMission(const std::string& dllName, const GameStartInfo* gsi, const std::string* savePath) {
    loadSettings();
    if (m_mission) { setCurrentMission(nullptr); m_mission.reset(); }
    resetTriggers();
    auto mission = std::make_unique<MissionScript>();
    std::string err;
    if (!mission->load(dllName, err)) {          // sub_402A10 ("Could not load script file")
        std::cerr << "[GameState] nao carregou " << dllName << ": " << err << std::endl;
        return false;
    }
    const AIModDesc* d = mission->desc;
    std::cout << "[Mission] " << dllName << ": '" << mission->levelDesc << "' tipo=" << d->missionType
              << " jogadores=" << d->numPlayers << " tech=" << d->maxTechLevel
              << " mapa=" << mission->mapName << " arvore=" << mission->techtreeName << std::endl;
    if (!loadMap(mission->mapName)) return false;
    m_mission = std::move(mission);
    setCurrentMission(m_mission.get());
    closeTabPanes();
    m_tab = 5;                                               // sub_4893A0: unk_565AC8 no início
    m_lastSel.clear();

    TethysGame::reset(/*seed*/ 0x12345678u, d->numPlayers, /*localPlayer*/ 0);
    m_tickAccum = 0.0f;
    resetPlayers(d->numPlayers);
    // GameStartInfo do shell p/ jogo single player: o jogador 0 é o humano, os
    // demais computador ([+0x28] isHuman).
    // (o registro vem pronto do GameStartInfo: só o campo, sem GoAI/GoHuman).
    for (int p = 0; p < d->numPlayers; ++p) g_playerData[p].isHuman = (p == 0) ? 1 : 0;
    // sub_4893A0: a estrutura vai para TethysGame+0xA4; dificuldade de cada
    // jogador = bits 3-4 da palavra dele (+0x30).
    m_gsi = gsi ? *gsi : GameStartInfo{};
    m_gsi.dll = dllName;
    m_endPhase = 0;
    if (gsi)
        for (int p = 0; p < d->numPlayers && p < 6; ++p) g_playerData[p].difficulty = gsi->difficulty(p);
    // sub_4893A0: planilhas relidas (sub_445070) e árvore da missão (sub_4733B0).
    reloadSheets();
    loadMoraleSheet();
    loadMinesSheet();                       // MINES.TXT (sub_44AF00, no TApp::Init)
    loadTechTree(m_mission->techtreeName, d->maxTechLevel);
    musicEndGame();                         // sub_489FB0 → sub_4506C0 (partida anterior)
    musicStartGame(d->missionType, false);  // sub_4507B0 (antes do InitProc)
    resetMessages();
    resetUnits();
    lavaReset();                                 // sub_4322A0 (início da partida)
    TethysGame::SetDaylightMoves(0);
    TethysGame::SetDaylightEverywhere(0);
    // Jogo salvo (sub_402CB0 & cia.): no lugar do InitProc, o estado do arquivo.
    if (savePath) {
        std::vector<uint8_t> extra;
        if (!saveGameRead(*savePath, extra) || !uiStateLoad(extra)) {
            std::cerr << "[Load] falhou: " << *savePath << std::endl;
            setCurrentMission(nullptr);
            m_mission.reset();
            resetTriggers();
            return false;
        }
        std::cout << "[Load] ok: tick " << TethysGame::tick() << ", unidades " << (g_units.size() - 1) << std::endl;
        return true;
    }
    // Campanha: sobras da missão anterior (sub_4898F0), antes do InitProc.
    if (gsi && gsi->campaign()) startInfoApplyCampaign(m_gsi);
    // sub_402E10: grupo padrão de cada computador (sub_402E70) e InitProc.
    aiDefaultGroupsStart();
    const int ok = m_mission->initProc ? m_mission->initProc() : 1;
    researchStartGame();                         // sub_472C30 (depois do InitProc)
    std::cout << "[Mission] InitProc -> " << ok << "; unidades=" << (g_units.size() - 1) << std::endl;
    int cx, cy;
    if (takeCenterViewRequest(cx, cy)) {           // CenterViewOn
        m_camX = cx - viewportW() / 2;
        m_camY = cy - viewportH() / 2;
        clampCamera();
    }
    return ok != 0;
}

bool GameState::load(const std::string& mapName) {
    loadSettings();
    if (m_mission) { setCurrentMission(nullptr); m_mission.reset(); }
    resetTriggers();
    if (!loadMap(mapName)) return false;

    // Inicia o núcleo de simulação (relógio zerado + RNG semeado). O seed virá da
    // missão/netgame nos próximos slices; por ora um valor fixo p/ determinismo.
    TethysGame::reset(/*seed*/ 0x12345678u, /*numPlayers*/ 2, /*localPlayer*/ 0);
    m_tickAccum = 0.0f;

    // Semeia o jogador local como uma missão faria no InitProc (via Set*). A
    // colônia/recursos reais virão da DLL da missão no próximo slice.
    resetPlayers(TethysGame::noPlayers());
    _Player& p = Player[TethysGame::localPlayer()];
    bool plymouth = (mapName.size() >= 3 &&
                     (mapName[0] == 'p' || mapName[0] == 'P') &&
                     (mapName[1] == 'l' || mapName[1] == 'L'));
    if (plymouth) p.GoPlymouth(); else p.GoEden();
    p.GoHuman();
    p.SetOre(5000);
    p.SetRareOre(0);
    p.SetFoodStored(3000);
    p.SetWorkers(16);
    p.SetScientists(8);
    p.SetKids(4);

    // Cria algumas unidades iniciais (como o InitProc/CreateUnit de uma missão),
    // centradas na área jogável — prova o pool de unidades + CreateUnit + render.
    // Planilhas de unidades (sub_445070): HP, Build Points, entulho, explosao...
    loadBuildingSheet();
    loadVehicleSheet();
    loadMinesSheet();
    loadWeaponSheet();
    loadSpaceSheet();
    loadMoraleSheet();
    resetMessages();
    resetUnits();
    lavaReset();                                 // sub_4322A0 (início da partida)
    // Dia/noite como as missões de colônia configuram (SetDaylightMoves(1) /
    // SetDaylightEverywhere(0)); as missões reais definem isso no InitProc.
    TethysGame::SetDaylightMoves(1);
    TethysGame::SetDaylightEverywhere(0);
    // Centro do demo: meio do mapa (o centro do clipRect da eden01 cai na emenda
    // do wrap horizontal, o que espalha a base pelas duas bordas da tela).
    int cx = (g_gameMap.width() / 2) & (g_gameMap.width() - 1);
    int cy = g_gameMap.height() / 2;
    int owner = TethysGame::localPlayer();
    // Posiciona cada unidade na célula passável mais próxima do ponto desejado
    // (evita montanhas/cânions). As posições reais virão da missão depois.
    auto place = [&](map_id type, int dx, int dy, map_id cargo = kMapIdNone) {
        Unit u;
        // Célula livre (passável E desocupada) p/ não sobrepor footprint/unidades.
        LOCATION loc = g_gameMap.findFreeNear(LOCATION{cx + dx, cy + dy});
        TethysGame::CreateUnit(u, type, loc, owner, cargo);
    };
    // Tanques armados (cargo = arma, map_id): Eden usa Laser / Rail Gun.
    place(mapCommandCenter, 0, 0);
    place(mapConVec, 2, 0);
    place(mapTiger, -2, 1, 0x3F);       // Rail Gun Tiger
    place(mapScout, 1, -2);
    place(mapRoboMiner, -3, -2);
    place(mapLynx, 3, 2, 0x3D);         // Laser Lynx

    // Inimigo Plymouth (player 1) p/ ver a facção (sprites verdes) + distinção.
    if (TethysGame::noPlayers() >= 2) {
        Player[1].GoPlymouth(); Player[1].GoAI();
        auto placeE = [&](map_id type, int dx, int dy, map_id cargo = kMapIdNone) {
            Unit u;
            LOCATION loc = g_gameMap.findFreeNear(LOCATION{(cx + dx) & (g_gameMap.width()-1), cy + dy});
            TethysGame::CreateUnit(u, type, loc, 1, cargo);
        };
        // Plymouth: RPG / Microwave.
        placeE(mapTiger, 8, 0, 0x40);   // RPG Tiger
        placeE(mapLynx, 9, 2, 0x3E);    // Microwave Lynx
        placeE(mapScout, 8, -2);
    }

    // Centraliza a câmera na base inicial (como CenterViewOn do jogo).
    m_camX = cx * 32 - viewportW() / 2;
    m_camY = cy * 32 - viewportH() / 2;
    clampCamera();

    std::cout << "[GameState] mapa " << mapName << " carregado ("
              << g_gameMap.width() << "x" << g_gameMap.height() << " tiles)" << std::endl;
    return true;
}

void GameState::clampCamera() {
    if (m_camY < 0) m_camY = 0;
    if (m_camY > m_maxY) m_camY = m_maxY;
    // X faz wrap no engine; deixamos rolar livremente (terrain faz o wrap).
}

void GameState::onActivate() {
    std::cout << "[GameState] ativo: " << m_mapName << std::endl;
}

// sub_49C830: códigos 10/11 (demo) saem direto; os demais esperam 2 s
// (Sleep(2000) com o cursor de espera) e, numa missão de campanha cumprida,
// abrem a caixa "Success" com "Save Game" / "Continue" (sub_41D5E0, Dialog 262).
void GameState::beginGameEnd() {
    const int code = gameEndCode();
    if (code == 10 || code == 11 || !gTApp.hasShell()) { finishGameEnd(); return; }
    m_endPhase = 1;
    m_endWaitStart = SDL_GetTicks();
}

void GameState::finishGameEnd() {
    const int code = gameEndCode();
    startInfoCollectResults(m_gsi, m_mission ? m_mission->desc : nullptr, code);   // sub_489A30
    if (code == 3 && m_gsi.campaign()) ++m_gsi.missionNumber;                    // +0x16A
    const GameStartInfo results = m_gsi;
    m_endPhase = 0;
    setCurrentMission(nullptr);
    m_mission.reset();
    resetTriggers();
    gTApp.activateShell(code, &results);                     // ActivateShell(motivo, resultados)
}

// Dialog 262 (OUT2RES.DLL, 186x94 DLU): texto + dois botões (25,57) e (102,58).
static constexpr int kSdW = 279, kSdH = 153;
static constexpr int kSdX = (Renderer::NATIVE_WIDTH - kSdW) / 2, kSdY = (Renderer::NATIVE_HEIGHT - kSdH) / 2;

int GameState::successButtonAt(int x, int y) const {
    if (y >= kSdY + 93 && y < kSdY + 93 + 23) {
        if (x >= kSdX + 37 && x < kSdX + 37 + 75) return 1;
        if (x >= kSdX + 153 && x < kSdX + 153 + 75) return 2;
    }
    return 0;
}

void GameState::drawSuccessDialog(Renderer& renderer) {
    renderer.drawDialogFrame(kSdX, kSdY, kSdW, kSdH, "");
    Font::drawTextCentered(renderer.getFramebuffer(), Renderer::NATIVE_WIDTH, Renderer::NATIVE_HEIGHT, "Success",
                           kSdX + kSdW / 2, kSdY + 45, 0xFFFFFFFF, true);
    renderer.drawButton(kSdX + 37, kSdY + 93, 75, 23, "Save Game", successButtonAt(m_mouseNX, m_mouseNY) == 1, false);
    renderer.drawButton(kSdX + 153, kSdY + 93, 75, 23, "Continue", successButtonAt(m_mouseNX, m_mouseNY) == 2, false);
}

void GameState::onIdle(Renderer& renderer, float dt) {
    if (!m_pendingLoad.empty()) {                            // TFrame::PostDelayedLoadMessage
        const std::string path = m_pendingLoad;
        m_pendingLoad.clear();
        if (!loadSavedGame(path)) { gTApp.activateShell(); return; }
    }
    if (m_endPhase == 1 && SDL_GetTicks() - m_endWaitStart >= 2000) {
        const int code = gameEndCode();
        // sub_402820: DescBlock[0] > 0 = missão de campanha (e dword_56DEF8 = 0).
        if (code == 3 && m_mission && m_mission->desc && m_mission->desc->missionType > 0) m_endPhase = 2;
        else { finishGameEnd(); return; }
    }
    const Uint8* ks = SDL_GetKeyboardState(nullptr);
    int speed = static_cast<int>(600 * dt);
    if (speed < 1) speed = 1;
    if (ks[SDL_SCANCODE_LEFT])  m_camX -= speed;
    if (ks[SDL_SCANCODE_RIGHT]) m_camX += speed;
    if (ks[SDL_SCANCODE_UP])    m_camY -= speed;
    if (ks[SDL_SCANCODE_DOWN])  m_camY += speed;
    // sub_407A10: a rolagem soma o tremor acumulado no desenho anterior e zera.
    m_camX += m_shakeX; m_camY += m_shakeY;
    m_shakeX = m_shakeY = 0;
    clampCamera();

    // Avança o relógio de simulação na cadência do GameSpeed (m_tickPeriod).
    const float kTickPeriod = m_tickPeriod;
    m_tickAccum += dt;
    if (m_endPhase || m_saveDlg.active()) m_tickAccum = 0.0f;   // encerrada / diálogo modal: sem turnos
    while (m_tickAccum >= kTickPeriod) {
        TethysGame::advanceTick();
        TethysGame::advanceDaylight(g_gameMap.internalWidth());   // início do turno (sub_489DD0)
        lavaTick();                              // sub_49DC20 da lava (micróbio: pendente)
        m_lastRand = TethysGame::getRand(100);  // 1 consumo determinístico por tick
        tickColony();                            // economia de cada jogador (sub_471F30)
        missionTriggerTick(currentMission());    // triggers, AIProc, vitória/derrota (sub_403110)
        tickMovement();                          // move as unidades pela rota do pathfinder
        tickUnits();                             // predios + efeitos (sub_43AC30)
        if ((TethysGame::tick() & 3) == 2) {     // sub_47EA60: listener = centro da vista
            soundSetListener(m_camX + viewportW() / 2, m_camY + viewportH() / 2);
            soundUpdate();                       // sub_47F250: fila de vozes
        }
        m_tickAccum -= kTickPeriod;
        // Fim de jogo (TethysGame +0x98): chegado o tick, sub_49C830.
        if (gameEndCode() && TethysGame::tick() >= gameEndTick()) {
            beginGameEnd();
            if (!m_endPhase) return;
            break;
        }
    }
    // Minimapa segurado (sub_4587B0 → sub_41F9E0): relê o cursor e centra a vista.
    if (m_miniDrag) {
        if (!(SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_LEFT))) {
            m_miniDrag = false;
        } else {
            int px, py;
            if (miniToWorld(m_mouseNX, m_mouseNY, px, py)) requestCenterView(px, py);
        }
    }
    {   // CenterViewOn pedido pelo script.
        int cx, cy;
        if (takeCenterViewRequest(cx, cy)) {
            m_camX = cx - viewportW() / 2;
            m_camY = cy - viewportH() / 2;
            clampCamera();
        }
    }

    // Seleção: tira unidades que morreram (o slot pode ter sido reusado por
    // um projétil/efeito — sub_439910 reaproveita slots livres).
    m_selected.erase(std::remove_if(m_selected.begin(), m_selected.end(), [](int id) {
        if (id <= 0 || id >= (int)g_units.size()) return true;
        const UnitRecord& r = g_units[id];
        return r.id == -1 || (r.flags & UNIT_DEAD) || !(r.flags & (UNIT_VEHICLE | UNIT_BUILDING));
    }), m_selected.end());

    renderer.clear(0xFF000000);
    m_terrain.render(renderer.getFramebuffer(), Renderer::NATIVE_WIDTH,
                     Renderer::NATIVE_HEIGHT, m_camX, m_camY);

    // Overlay de passabilidade (debug, tecla G): tinge de vermelho as células
    // impassáveis visíveis. Base visual para waypoints/pathfinding.
    if (m_showPassability) {
        uint32_t* fb = renderer.getFramebuffer();
        int t0x = m_camX / 32, t0y = m_camY / 32;
        int t1x = (m_camX + Renderer::NATIVE_WIDTH) / 32;
        int t1y = (m_camY + Renderer::NATIVE_HEIGHT) / 32;
        for (int ty = t0y; ty <= t1y; ++ty) {
            if (ty < 0 || ty >= g_gameMap.height()) continue;
            for (int tx = t0x; tx <= t1x; ++tx) {
                if (g_gameMap.isPassable(LOCATION{tx, ty})) continue;
                int sx = tx * 32 - m_camX, sy = ty * 32 - m_camY;
                for (int yy = 0; yy < 32; ++yy) {
                    int py = sy + yy;
                    if (py < 0 || py >= Renderer::NATIVE_HEIGHT) continue;
                    for (int xx = 0; xx < 32; ++xx) {
                        int px = sx + xx;
                        if (px < 0 || px >= Renderer::NATIVE_WIDTH) continue;
                        uint32_t c = fb[py * Renderer::NATIVE_WIDTH + px];
                        uint32_t r = ((c >> 16) & 0xFF), g = (c >> 8) & 0xFF, b = c & 0xFF;
                        r = r / 2 + 128; g = g / 2; b = b / 2;   // tinge vermelho
                        fb[py * Renderer::NATIVE_WIDTH + px] = 0xFF000000 | (r << 16) | (g << 8) | b;
                    }
                }
            }
        }
    }

    // Unidades: por ora marcadores coloridos por dono (os sprites op2_art entram
    // num slice futuro). Prédio = quadrado maior; veículo = menor.
    static const uint32_t kOwnerColor[7] = {
        0xFF40A0FF, 0xFFFF5040, 0xFF40D070, 0xFFFFD040,
        0xFFC060FF, 0xFF40E0E0, 0xFFFF9030,
    };
    // Overlay de ordem de movimento: linha da unidade até o destino + marcador no
    // tile-alvo. Lê o estado REAL da ordem (order.waypoints, gravado por DoMove ->
    // dispatcher -> bloco de ordem). Prova visível do pipeline de comando (o
    // movimento em si entra quando o pathfinder/integrador estiverem portados).
    {
        uint32_t* fb = renderer.getFramebuffer();
        const int VW = Renderer::NATIVE_WIDTH, VH = Renderer::NATIVE_HEIGHT;
        auto putPx = [&](int x, int y, uint32_t c) {
            if (x >= 0 && x < VW && y >= 0 && y < VH) fb[y * VW + x] = c;
        };
        auto drawLine = [&](int x0, int y0, int x1, int y1, uint32_t c) {
            int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
            int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
            int err = dx + dy;
            for (;;) {
                putPx(x0, y0, c);
                if (x0 == x1 && y0 == y1) break;
                int e2 = 2 * err;
                if (e2 >= dy) { err += dy; x0 += sx; }
                if (e2 <= dx) { err += dx; y0 += sy; }
            }
        };
        for (size_t i = 1; i < g_units.size(); ++i) {
            const UnitRecord& r = g_units[i];
            if (r.id == -1 || (r.flags & UNIT_DEAD)) continue;
            if (!r.hasOrder || (r.action != CMD_MOVE && r.action != 0x26) || r.order.waypoints.empty()) continue;
            uint32_t dest = r.order.waypoints.back();
            int dxt = unpackLocX(dest), dyt = unpackLocY(dest);
            int ux = screenXOf(r.pixelX) + 16;
            int uy = r.pixelY + 16 - m_camY;
            int dsx = screenXOf(dxt * 32) + 16;
            int dsy = dyt * 32 + 16 - m_camY;
            uint32_t col = kOwnerColor[(r.owner & 0x0F) % 7];
            drawLine(ux, uy, dsx, dsy, col);
            // Marcador em X no destino.
            for (int k = -5; k <= 5; ++k) {
                putPx(dsx + k, dsy + k, 0xFFFFFFFF);
                putPx(dsx + k, dsy - k, 0xFFFFFFFF);
            }
        }
    }

    // Blit de um frame de animação do op2_art (g_art) CENTRADO no ponto da unidade
    // na tela. As pictures são posicionadas por (x,y) menos o HOTSPOT da animação
    // (header[5],header[6] = centro do bbox header[1..4]) — como o original faz
    // (`sub eax, dword_50B368[...]` em sub_403E00). Índice 0 = transparente.
    auto drawSprite = [&](int animIdx, int frameIdx, int cx, int cy, int playerColor) {
        return blitArtFrame(renderer.getFramebuffer(), Renderer::NATIVE_WIDTH, Renderer::NATIVE_HEIGHT,
                            animIdx, frameIdx, cx, cy, playerColor,
                            0, 0, viewportW(), viewportH());
    };

    // Colchete de seleção (sub_43E950): guardo o frame do corpo de cada unidade
    // selecionada durante o desenho e traço os cantos depois, por cima de tudo.
    struct SelDeco { int id, cx, cy, anim, frame; };
    std::vector<SelDeco> selDeco;
    auto isSel = [&](int id) {
        for (int s : m_selected) if (s == id) return true;
        return false;
    };
    auto recDeco = [&](int id, int cx, int cy, int anim, int frame) {
        if (isSel(id)) selDeco.push_back({id, cx, cy, anim, frame});
    };

    // Passe de sombra (sub_49E8C0): vt[30] das unidades com 0x10000000 — a
    // poeira do tornado — antes de tudo.
    for (size_t i = 1; i < g_units.size(); ++i) {
        const UnitRecord& r = g_units[i];
        if (r.id != -1 && r.type == mapAmbientAnim && (r.flags & UNIT_EFFECT_HIDDEN)) {   // sub_405080
            int anim, frame;
            if (effectAnimFrame(r, anim, frame, true) && g_art.ok() && anim < g_art.numAnimations())
                drawSprite(anim, frame, screenXOf(r.pixelX) + 16, r.pixelY + 16 - m_camY, -1);
            continue;
        }
        if (r.id == -1 || !isDisaster(r.type)) continue;
        std::vector<WeaponDrawOp> ops;
        disasterDrawOps((int)i, true, ops);
        for (const WeaponDrawOp& op : ops)
            if (op.anim >= 0 && g_art.ok() && op.anim < g_art.numAnimations())
                drawSprite(op.anim, op.frame, screenXOf(op.x), op.y - m_camY, -1);
    }

    // Tres passes: ESTRUTURAS primeiro (ficam por baixo), depois veiculos e por
    // fim as unidades-efeito (explosoes) por cima.
    for (int pass = 0; pass < 3; ++pass)
    for (size_t i = 1; i < g_units.size(); ++i) {
        const UnitRecord& r = g_units[i];
        if (r.id == -1 || (r.flags & UNIT_DEAD)) continue;
        const bool isBld = (r.flags & UNIT_BUILDING) != 0;
        const bool isEffect = !(r.flags & (UNIT_BUILDING | UNIT_VEHICLE));
        if (pass != (isBld ? 0 : isEffect ? 2 : 1)) continue;
        // Sombras pela opção do INI da classe (dword_575634 / dword_575638).
        setArtShadows(isBld ? m_stationaryShadows : m_mobileShadows);
        // Centro da unidade na tela = [+0x14]/[+0x18] do original (unitOX/OY:
        // canto do tile + 16; prédio só nas dimensões ímpares — sub_4369E0).
        // Usa a posição INTERPOLADA (não o tile), p/ casar com o deslize.
        int ucx = screenXOf(unitOX(r));       // wrap horizontal do mapa
        int ucy = unitOY(r) - m_camY;

        bool drew = false;
        if ((r.flags & UNIT_VEHICLE) && hasVehicleAnim(r.type)) {
            int ow = r.owner & 0x0F;
            bool eden = (ow >= 0 && ow < kMaxPlayers) ? (g_playerData[ow].isEden != 0) : true;
            int anim = vehicleAnimIndex(r.type, r.rotation, eden);   // resolver fiel (facção)
            if (r.type == mapCargoTruck) {               // vt[0x80] sub_4062B0 (carga/doca)
                const int raw = truckAnimRaw(r, ((r.rotation + 8) & 0xF0) >> 4);
                if (raw) anim = raw - 0x65;
            }
            if (anim >= 0) {
                int nf = g_art.ok() && anim < g_art.numAnimations()
                         ? (int)g_art.animation(anim).frames.size() : 0;
                // Frame — porta sub_44C5C0: fica em 0 (parado) a menos que a
                // unidade esteja em MOVIMENTO ([+0x22]==1) ou em acao 0x1C/0x1D;
                // ai cicla com o relogio: ((clock >> flag) + [unit+0x10]) % nFrames.
                int frame = 0;
                if (isTankChassis(r.type)) {
                    // sub_484A90 (tanques): só em movimento; (id + tick/2) % nF.
                    if (r.field22 == 1 && nf > 0) frame = (((int)i + (int)(TethysGame::tick() >> 1)) % nf + nf) % nf;
                } else if (r.type == mapRoboSurveyor || r.type == mapRoboMiner || r.type == mapGeoCon ||
                           r.type == mapRoboDozer || r.type == mapEvacuationTransport ||
                           r.type == mapRepairVehicle || r.type == mapEarthworker) {
                    // vt[8] sub_44C7B0 (Robo-Surveyor/Miner/Dozer, GeoCon, Evacuation
                    // Transport, Repair, Earthworker; o caminhão em truckFrame — tipos
                    // tirados das vtables): cicla com QUALQUER ação
                    // ([+0x21] != 0) — parado trabalhando também (o bico do
                    // Earthworker, a lâmina do Dozer). Doca (0x0D): conta do início.
                    if (r.field22 == 0x0D && nf > 0) {
                        const int f = (TethysGame::tick() - r.dockTick) & 0xFFFF;
                        frame = f < nf - 1 ? f : nf - 2 > 0 ? nf - 2 : 0;
                    } else if (r.action && nf > 0) {
                        const int t = (int)(TethysGame::tick() >> ((r.flags & 0x10000) ? 1 : 0)) + (int)i;
                        frame = ((t % nf) + nf) % nf;
                    }
                } else {
                    bool animating = (r.field22 == 1) || (r.action == 0x1D) || (r.action == 0x1C);
                    if (animating && nf > 0) {
                        int shift = (r.flags & 0x10000) ? 1 : 0;
                        int t = (TethysGame::tick() >> shift) + (int)i;   // [unit+0x10] = id
                        frame = ((t % nf) + nf) % nf;
                    }
                }
                // Cor do dono: campo +0x30 do registro do jogador (colorNumber),
                // que indexa as rampas de 24 cores de color.bmp (sub_4A8D70).
                if (r.type == mapCargoTruck) frame = truckFrame(r, (int)i);   // vt[8] sub_44C7B0
                int pc = (ow >= 0 && ow < kMaxPlayers) ? g_playerData[ow].colorNumber : 0;
                drew = drawSprite(anim, frame, ucx, ucy, pc);
                if (drew) recDeco((int)i, ucx, ucy, anim, frame);
                // Torre (sub_484CF0): anim da arma p/ a direção da torre [+0x2D],
                // frame 0, no ponto de encaixe do frame atual do chassi.
                int tAnim, tx, ty;
                if (drew && tankTurret(r, anim, frame, tAnim, tx, ty) && tAnim >= 0 &&
                    tAnim < g_art.numAnimations())
                    drawSprite(tAnim, 0, ucx + tx, ucy + ty, pc);
                // Overlays (sub_44C910 / fim de sub_484CF0), fora da doca: espuma
                // 0x2FE (contador [+0x54] < nF-1) / 0x2FD (endurecida, pelo tempo
                // desde [+0x60]), ESG 0x619 e EMP 0x2FF ((id + tick) % nF).
                if (drew && r.field22 != 0x0D && g_art.ok()) {
                    auto lastF = [](int raw) {
                        const int a = raw - 0x65;
                        return a < g_art.numAnimations() ? (int)g_art.animation(a).frames.size() - 1 : 0;
                    };
                    const int tick = (int)TethysGame::tick();
                    if (r.flags & 0x40000u) {
                        const int n = lastF(0x2FE);
                        if (r.foamTicks < n) drawSprite(0x2FE - 0x65, n - r.foamTicks, ucx, ucy, -1);
                        else {
                            const int cap = lastF(0x2FD) - 1;
                            const int f = static_cast<int>((static_cast<unsigned>(tick - r.foamStart) & 0xFFFEu) >> 1);
                            drawSprite(0x2FD - 0x65, f < cap ? f : cap, ucx, ucy, -1);
                        }
                    }
                    if (r.flags & 0x100000u) drawSprite(0x619 - 0x65, (int)((unsigned)((int)i + tick) % (unsigned)(lastF(0x619) + 1)), ucx, ucy, -1);
                    if (r.flags & UNIT_EMP) drawSprite(0x2FF - 0x65, (int)((unsigned)((int)i + tick) % (unsigned)(lastF(0x2FF) + 1)), ucx, ucy, -1);
                }
            }
        }
        if (isBld) {
            // Estrutura -- porta sub_408BE0: anim = tabela[estado] (sub_4835A0/
            // sub_483720), frame = sub_409220. Estado 0 com contador 0 = ultimo
            // frame (predio pronto); construcao, dano (1/2) e destruicao (3) vem
            // da maquina de estados (src/game/structure.cpp).
            int ow = r.owner & 0x0F;
            int anim = structureCurAnim(r);
            int gpAnim;
            const bool guardPost = guardPostSprite(r, gpAnim);     // vt[8] sub_42D250
            if (guardPost) anim = gpAnim;
            if (anim >= 0 && g_art.ok() && anim < g_art.numAnimations()) {
                int pc = (ow >= 0 && ow < kMaxPlayers) ? g_playerData[ow].colorNumber : 0;
                const int bldFrame = guardPost ? 0 : structureFrame(r);
                drew = drawSprite(anim, bldFrame, ucx, ucy, pc);
                if (drew) recDeco((int)i, ucx, ucy, anim, bldFrame);
                // EMP / ESG (0x180000) no prédio (sub_408DB0): grade de 16 px no
                // footprint; cada ponto aparece se rnd(100) > 80 (40 com lado 1),
                // com a RNG semeada por (tick>>5) + id + [+0x18] + 0x11790718.
                if ((r.flags & 0x180000u) && (!guardPost || (r.flags & UNIT_EMP))) {   // GP: só EMP (sub_42D300)
                    const StructSheetDesc* sd = structSheet(r.type);
                    const int sx = sd ? sd->sizeX : 1, sy = sd ? sd->sizeY : 1;
                    const int oraw = (r.flags & UNIT_EMP) ? 0x2FF : 0x619;
                    const int nF = (int)g_art.animation(oraw - 0x65).frames.size();
                    GameRNG rng;
                    rng.setSeed((uint32_t)((int)(TethysGame::tick() >> 5) + (int)i + unitOY(r) + 0x11790718));
                    const int thr = (sx == 1 || sy == 1) ? 0x28 : 0x50;
                    int px = screenXOf(((unitOX(r) & ~0x1F) - (sx / 2) * 32 + 16));
                    const int py0 = (unitOY(r) & ~0x1F) - (sy / 2) * 32 - m_camY + 16;
                    for (int gx = 0; gx < 2 * sx - 1; ++gx, px += 16) {
                        int py = py0;
                        for (int gy = 0; gy < 2 * sy - 1; ++gy, py += 16) {
                            if (rng.getRand(100) <= thr) continue;
                            const int f = (rng.getRand(100) + (int)(TethysGame::tick() >> 1)) % (nF > 0 ? nF : 1);
                            drawSprite(oraw - 0x65, f, px, py, -1);
                        }
                    }
                }
                // Luz de status (fim de sub_408DB0, opção "Overlay"): prédio de
                // um aliado do jogador local, fora de desmonte/0x60000: 0x96 em
                // idle (ação 0x12), 0x94 operando, 0x95 parado; no canto do bbox
                // do sprite + 3; frame = tick % nF.
                const int me = TethysGame::localPlayer();
                if (guardPost) {
                    // sub_42D300: a luz só quando NÃO opera — 0x96 parado (0x12) / 0x95 —
                    // em (x − 3, y − 3).
                    if (m_overlay && ow < kMaxPlayers && (g_playerData[me].allyMask & (1u << ow)) &&
                        !structureOperational(r)) {
                        const int la = (r.action == 0x12 ? 0x96 : 0x95) - 0x65;
                        const int nF = (int)g_art.animation(la).frames.size();
                        drawSprite(la, nF ? TethysGame::tick() % nF : 0, ucx - 3, ucy - 3, -1);
                    }
                } else if (m_overlay && ow < kMaxPlayers && (g_playerData[me].allyMask & (1u << ow)) &&
                    r.action != ACT_DISMANTLE && !(r.flags & 0x60000u)) {
                    const int light = r.action == 0x12 ? 0x96 : structureOperational(r) ? 0x94 : 0x95;
                    const uint32_t* h = g_art.animation(anim).header;
                    const int la = light - 0x65;
                    if (la < g_art.numAnimations()) {
                        const int nF = static_cast<int>(g_art.animation(la).frames.size());
                        drawSprite(la, nF ? TethysGame::tick() % nF : 0,
                                   ucx - static_cast<int>(h[5] - h[1]) + 3, ucy - static_cast<int>(h[6] - h[2]) + 3, -1);
                    }
                }
            }
        }
        if (isEffect && r.type == 0x51) {
            // Beacon de mineração (sub_405310): pesquisado pelo local → anim pelo
            // rendimento (raro 0x73F/0x73E/0x73D, comum 0x73C/0x73B/0x73A); senão
            // 0x740. Frame = tick % nF.
            int raw = 0x740;
            if (beaconSurveyed(static_cast<int>(i), TethysGame::localPlayer())) {
                static const int kRare[3] = {0x73F, 0x73E, 0x73D}, kCommon[3] = {0x73C, 0x73B, 0x73A};
                const int y = r.beaconYield >= 0 && r.beaconYield <= 2 ? r.beaconYield : 0;
                raw = r.beaconRare ? kRare[y] : kCommon[y];
            }
            const int anim = raw - 0x65;
            if (g_art.ok() && anim < g_art.numAnimations()) {
                const int nf = static_cast<int>(g_art.animation(anim).frames.size());
                drawSprite(anim, nf ? TethysGame::tick() % nf : 0, ucx, ucy, -1);
            }
            continue;
        }
        if (isEffect && r.type == 0x53) {
            // Fumarola (vt[6] sub_405840): some sob um Geothermal Plant (0x2C) ou
            // com uma unidade no tile; frame = tick % último frame da anim 0x9F; a
            // cada 100 ticks, gêiser 0x3C (o som sai no desenho → só visível).
            const int tx = (unitOX(r) >> 5) & (g_gameMap.width() - 1), ty = unitOY(r) >> 5;
            const int occ = g_gameMap.cellOccupant(tx, ty);
            if (occ > 0 && occ < static_cast<int>(g_units.size())) {
                const UnitRecord& o = g_units[occ];
                if (o.id != -1 && (o.type == 0x2C ||
                    (((unitOX(o) >> 5) & (g_gameMap.width() - 1)) == tx && (unitOY(o) >> 5) == ty)))
                    continue;
            }
            const int anim = 0x9F - 0x65;
            if (g_art.ok() && anim < g_art.numAnimations()) {
                const int last = static_cast<int>(g_art.animation(anim).frames.size()) - 1;
                drawSprite(anim, last > 0 ? TethysGame::tick() % last : 0, ucx, ucy, -1);
            }
            if (TethysGame::tick() % 100 == 0 && onScreen(ucx, ucy)) drawSound(static_cast<int>(i), unitOX(r), unitOY(r), 0x3C);
            continue;
        }
        if (isEffect && r.type == 0x50 && (r.flags & 0x2000u) && (r.flags & 0x40000u) &&
            r.dsAux > 0 && r.dsAux < static_cast<int>(g_units.size()) && onScreen(ucx, ucy)) {
            // sub_44A9A0: ao desenhar o laser da defesa, 0x47 na defesa e 0x4E no
            // meteoro destruído (0x10000).
            const UnitRecord& d = g_units[r.dsAux];
            drawSound(static_cast<int>(i), unitOX(d), unitOY(d), 0x47);
            if (r.flags & 0x10000u) drawSound(static_cast<int>(i), unitOX(r), unitOY(r), 0x4E);
        }
        if (isEffect && (isLaunchUnit(r.type) || isEmpBlast(r.type))) {
            // Lançamento do Spaceport (sub_4805F0) / anel da EMP_BIG (sub_413BC0).
            std::vector<WeaponDrawOp> ops;
            missileDrawOps((int)i, ops);
            for (const WeaponDrawOp& op : ops) {
                if (op.kind == WeaponDrawOp::SPRITE) {
                    if (op.anim >= 0 && g_art.ok() && op.anim < g_art.numAnimations())
                        drawSprite(op.anim, op.frame, screenXOf(op.x), op.y - m_camY, -1);
                } else {
                    drawBeam(renderer, op);
                    // Ao desenhar o laser: 0x47 na defesa, 0x4E no míssil derrubado.
                    if (onScreen(ucx, ucy)) {
                        const UnitRecord& d = g_units[r.launchDefense];
                        drawSound(static_cast<int>(i), unitOX(d), unitOY(d), 0x47);
                        if (r.flags & 0x10000u) drawSound(static_cast<int>(i), r.pixelX, r.pixelY, 0x4E);
                    }
                }
            }
            continue;
        }
        if (isEffect && isDisaster(r.type)) {
            // Meteoro / raio / tornado (vt[6]); terremoto e erupção não desenham.
            std::vector<WeaponDrawOp> ops;
            disasterDrawOps((int)i, false, ops);
            for (const WeaponDrawOp& op : ops) {
                if (op.kind == WeaponDrawOp::SPRITE) {
                    if (op.anim >= 0 && g_art.ok() && op.anim < g_art.numAnimations())
                        drawSprite(op.anim, op.frame, screenXOf(op.x), op.y - m_camY, -1);
                } else {
                    drawBeam(renderer, op);
                }
            }
            continue;
        }
        if (isEffect && isWeaponProjectile(r.type)) {
            // Projétil/explosão de arma (vt[6]: sub_4A3420 / sub_4A3000 /
            // sub_4A2A00 / sub_4A2D50) em coordenadas de mundo do original.
            std::vector<WeaponDrawOp> ops;
            weaponDrawOps((int)i, ops);
            for (const WeaponDrawOp& op : ops) {
                if (op.kind == WeaponDrawOp::SOUND) {                // som tocado pelo desenho (ácido)
                    if (onScreen(ucx, ucy)) drawSound(static_cast<int>(i), op.x, op.y, op.anim);
                } else if (op.kind == WeaponDrawOp::SPRITE) {
                    if (op.anim >= 0 && g_art.ok() && op.anim < g_art.numAnimations())
                        drawSprite(op.anim, op.frame, screenXOf(op.x), op.y - m_camY, -1);
                } else {
                    drawBeam(renderer, op);
                }
            }
            continue;
        }
        if (isEffect && r.type == 0x54) {
            // Destroço (sub_405A50): 0x7A5 com a cor do jogador 1, se descoberto
            // por um aliado do jogador local.
            int raw, frame;
            if (wreckSprite(static_cast<int>(i), raw, frame) && g_art.ok() && raw - 0x65 < g_art.numAnimations())
                drawSprite(raw - 0x65, frame, ucx, ucy, g_playerData[1].colorNumber);
            continue;
        }
        if (isEffect) {
            // Unidade-efeito AMBIENT_ANIM (sub_405160): sem remap de cor salvo
            // UNIT_EFFECT_COLOR; invisivel antes do atraso.
            int anim, frame;
            if (effectAnimFrame(r, anim, frame) && g_art.ok() && anim < g_art.numAnimations())
                drawSprite(anim, frame, ucx, ucy,
                           (r.flags & UNIT_EFFECT_COLOR) ? r.effectColor : -1);
            continue;   // efeitos nao tem marcador nem barrinha
        }
        if (!drew) {
            // Fallback: marcador colorido (prédios/tipos sem sprite ainda).
            uint32_t col = kOwnerColor[(r.owner & 0x0F) % 7];
            int sz = (r.flags & UNIT_BUILDING) ? 26 : 16;
            renderer.fillRect(ucx - sz / 2, ucy - sz / 2, sz, sz, col);
            renderer.drawRect(ucx - sz / 2, ucy - sz / 2, sz, sz, 0xFF101010);
        }
    }

    // Colchete + barra de vida (sub_43E950) das unidades selecionadas.
    const int vw = viewportW(), vh = viewportH();
    uint32_t* fb = renderer.getFramebuffer();
    auto put = [&](int x, int y, uint32_t c) {         // 1 px recortado à vista
        if (x >= 0 && x < vw && y >= 0 && y < vh) fb[y * Renderer::NATIVE_WIDTH + x] = c;
    };
    auto hseg = [&](int x, int y, int len, uint32_t c) {   // len<0 = p/ a esquerda
        const int s = len < 0 ? -1 : 1;
        for (int k = 0; k <= (len < 0 ? -len : len); ++k) put(x + k * s, y, c);
    };
    auto vseg = [&](int x, int y, int len, uint32_t c) {
        const int s = len < 0 ? -1 : 1;
        for (int k = 0; k <= (len < 0 ? -len : len); ++k) put(x, y + k * s, c);
    };
    for (const SelDeco& d : selDeco) {
        int ox, oy, w, h;
        if (!artFrameBounds(d.anim, d.frame, ox, oy, w, h)) continue;
        const int l = d.cx + ox, t = d.cy + oy, rt = l + w, bt = t + h;
        // Colchete: 4 cantos em "L" brancos (0x7FFF), cada perna = dimensão/6
        // (idiv 6 do original). Não é um retângulo contínuo.
        const uint32_t WHITE = 0xFFFFFFFFu;            // edi=0x7FFF no caso comum
        const int lx = w / 6, ly = h / 6;
        hseg(l, t, lx, WHITE);     vseg(l, t, ly, WHITE);     // superior-esquerdo
        hseg(rt, t, -lx, WHITE);   vseg(rt, t, ly, WHITE);    // superior-direito
        hseg(l, bt, lx, WHITE);    vseg(l, bt, -ly, WHITE);   // inferior-esquerdo
        hseg(rt, bt, -lx, WHITE);  vseg(rt, bt, -ly, WHITE);  // inferior-direito

        // Barra de vida acima do colchete. maxHP por jogador (planilha), dano
        // em +0x1E; cor por faixa: >=40% verde (0x3E0), >25% amarelo (0x7FE0),
        // <=25% vermelho (0x7C00) piscando com o bit 1 do relógio.
        if (d.id <= 0 || d.id >= (int)g_units.size()) continue;
        const UnitRecord& r = g_units[d.id];
        const int pl = (r.owner >> 4) & 0x0F;
        int maxHP = 0;
        if (r.flags & UNIT_VEHICLE) { const VehPlayerStats* v = vehicleSheet(r.type, pl); maxHP = v ? v->hitPoints : 0; }
        else if (r.flags & UNIT_BUILDING) { const StructSheetDesc* ss = structSheet(r.type); maxHP = ss ? ss->player[pl].hitPoints : 0; }
        if (maxHP <= 0) continue;
        int dmg = r.damage; if (dmg < 0) dmg = 0; if (dmg > maxHP) dmg = maxHP;
        const int hpRest = maxHP - dmg;
        uint32_t barCol;                               // conversão RGB555→888 (31→255)
        bool draw = true;
        if (hpRest * 10 >= maxHP * 4) barCol = 0xFF00FF00u;       // verde  (>= 40%)
        else if (hpRest * 4 > maxHP) barCol = 0xFFFFFF00u;        // amarelo (> 25%)
        else { barCol = 0xFFFF0000u; draw = (TethysGame::tick() & 2) != 0; }  // vermelho piscando
        const int barW = (w > 56) ? 48 : 24;           // var_188: 0x30 se largo, senão 0x18
        const int bx = (l + rt) / 2 - barW / 2;        // centrado na unidade
        const int by = t - 7;                          // var_1B0 = topo do colchete - 7
        // Moldura: laterais brancas, topo e base pretas (6 px de altura).
        vseg(bx, by, 6, WHITE);  vseg(bx + barW + 2, by, 6, WHITE);
        hseg(bx, by, barW + 2, 0xFF000000u);  hseg(bx, by + 6, barW + 2, 0xFF000000u);
        if (draw) {                                    // preenchimento proporcional
            const int fillW = hpRest * barW / maxHP;
            for (int yy = by + 1; yy <= by + 5; ++yy)
                for (int xx = bx + 1; xx <= bx + fillW; ++xx) put(xx, yy, barCol);
        }
    }
    // Modo de posicionamento do kit (sub_451AC0 → sub_41B5B0: retângulo do
    // tamanho da área no cursor). PROVISÓRIO: contorno da área (verde) e do
    // anel de terraplanagem (cinza); o cursor real do original não foi portado.
    if ((m_targetCmd == UC_BUILD_KIT || m_targetCmd == UC_BUILD_MINE) && !m_selected.empty()) {
        int w = 0, h = 0;
        const int mvx = m_mouseNX - kViewX, mvy = m_mouseNY - kViewY;
        if (m_targetCmd == UC_BUILD_MINE) mineFootprint(w, h);      // sub_451FC0: off_4E1394
        else kitFootprint(m_selected[0], w, h);
        if (w && mvx >= 0 && mvx < viewportW() && mvy >= 0 && mvy < viewportH()) {
            int tx = (m_camX + mvx) / 32, ty = (m_camY + mvy) / 32, l, t, r, b;
            if (g_gameMap.ok()) tx &= (g_gameMap.width() - 1);
            kitPlacementRect(w, h, tx, ty, l, t, r, b);
            const int sx = screenXOf((l + 1) * 32), sy = (t + 1) * 32 - m_camY;
            renderer.drawRect(sx - 32, sy - 32, (w + 2) * 32, (h + 2) * 32, 0xFF909090);
            renderer.drawRect(sx, sy, w * 32, h * 32, 0xFF40FF40);
        }
    }
    // Área em arrasto (muro/tubo: reta de até 8 tiles; terraplanar: retângulo).
    // PROVISÓRIO: contorno; o cursor/realce do original não foi portado.
    if (m_areaDrag && m_targetCmd >= 0) {
        int x1, y1, x2, y2;
        areaRect(m_camX + m_mouseNX - kViewX, m_camY + m_mouseNY - kViewY, x1, y1, x2, y2);
        const int sx = screenXOf(x1 * 32), sy = y1 * 32 - m_camY;
        renderer.drawRect(sx, sy, (x2 - x1) * 32, (y2 - y1) * 32, 0xFF40FF40);
    }
    // Caixa de arrasto (multi-seleção).
    if (m_dragging) {
        const int mvx = m_mouseNX - kViewX, mvy = m_mouseNY - kViewY;   // mouse na vista
        int x0 = std::min(m_dragSX, mvx), y0 = std::min(m_dragSY, mvy);
        int w = std::abs(mvx - m_dragSX), h = std::abs(mvy - m_dragSY);
        renderer.drawRect(x0, y0, w, h, 0xFF40FF40);
    }

    // Painel de comando (a "MAINE"): minimapa + info + botões.
    // Fim de jogo (sub_49CAC0): letreiro sobre a vista até o tick de encerramento.
    // (O original usa a fonte grande do quadro de detalhe — aqui a fonte do port.)
    if (gameEndCode()) {
        const int cy = viewportH() / 2;
        Font::drawTextCentered(renderer.getFramebuffer(), Renderer::NATIVE_WIDTH, Renderer::NATIVE_HEIGHT,
                               gameEndText(gameEndCode()), viewportW() / 2, cy, 0xFFFFE070, true);
    }
    // vt[6] do terremoto (sub_413560): tremor p/ o próximo quadro, pelo centro da vista.
    disasterShake(m_camX + (viewportW() / 32) * 16,
                  m_camY + (viewportH() / 32) * 16,
                  m_frameSkip, m_shakeX, m_shakeY);
    // A vista foi desenhada na origem: vai p/ o seu lugar na área cliente
    // (12,22); o resto da tela é a moldura e a coluna direita (renderPanel).
    {
        uint32_t* fb = renderer.getFramebuffer();
        const int VW = Renderer::NATIVE_WIDTH;
        for (int y = kViewH - 1; y >= 0; --y)
            std::memmove(&fb[(y + kViewY) * VW + kViewX], &fb[y * VW], sizeof(uint32_t) * kViewW);
    }
    renderPanel(renderer);

    // Overlay: relógio (Tick/Time) + uma amostra do RNG — prova visível de que o
    // núcleo de simulação está rodando determinístico.
    if (m_showDebugHud) {
    uint32_t* fb = renderer.getFramebuffer();
    const int VW = Renderer::NATIVE_WIDTH, VH = Renderer::NATIVE_HEIGHT;
    const _Player& p = Player[TethysGame::localPlayer()];

    char l1[96], l2[128];
    std::snprintf(l1, sizeof(l1), "Tick %d   Time %d   Rand %d",
                  TethysGame::tick(), TethysGame::time(), m_lastRand);
    std::snprintf(l2, sizeof(l2),
                  "%s   Ore %d  Rare %d  Food %d   W%d S%d K%d",
                  p.IsEden() ? "EDEN" : "PLYMOUTH",
                  p.Ore(), p.RareOre(), p.FoodStored(),
                  p.Workers(), p.Scientists(), p.Kids());
    int liveUnits = 0;
    for (size_t i = 1; i < g_units.size(); ++i) { Unit u((int)i); if (u.IsLive()) ++liveUnits; }
    char l3[128];
    std::snprintf(l3, sizeof(l3), "%s   Units %d   (setas rolar | btn-dir mover | G passab | ESC menu)",
                  m_mapName.c_str(), liveUnits);
    // Economia (sub_471F30): energia gerada/exigida, mão de obra sobrando e
    // prédios operando/total do jogador local.
    const ColonyState& cs = g_colony[TethysGame::localPlayer()];
    char l4[128];
    std::snprintf(l4, sizeof(l4), "Energia %d/%d (sobra %d)   Trab sobra %d  Cient sobra %d   Operando %d/%d",
                  cs.powerGenerated, cs.powerRequired, cs.powerLeft,
                  cs.workersLeft, cs.scientistsLeft, cs.activeCount, cs.structCount);
    Font::drawText(fb, VW, VH, l1, 8, 8, 0xFFFFE060, true);
    Font::drawText(fb, VW, VH, l2, 8, 20, 0xFF80E0A0, true);
    Font::drawText(fb, VW, VH, l3, 8, 32, 0xFFB0C4DE, true);
    Font::drawText(fb, VW, VH, l4, 8, 44, 0xFFE0C080, true);
    }

    if (m_endPhase == 2 && !m_saveDlg.active()) drawSuccessDialog(renderer);
    m_saveDlg.render(renderer);
    renderer.present();
}

int GameState::viewportW() const { return kViewW; }

// Feixes das armas instantâneas: Bresenham com bloco 2x2 por ponto, como
// sub_41E8F0 (laser: OR 0x7C00 = vermelho máximo em RGB555) e loc_41ECE0
// (microwave: padrão 0xFE1FC3F8 girado pela fase; cor 0xD69FD69E que perde
// 0x30003 a cada ponto aceso e volta ao início a cada falha do padrão).
void GameState::drawBeam(Renderer& renderer, const WeaponDrawOp& op) {
    uint32_t* fb = renderer.getFramebuffer();
    const int VW = Renderer::NATIVE_WIDTH, clipW = viewportW(), clipH = viewportH();
    auto c555 = [](uint32_t w) {
        const uint32_t r = (w >> 10) & 31, g = (w >> 5) & 31, b = w & 31;
        return 0xFF000000u | (((r << 3) | (r >> 2)) << 16) | (((g << 3) | (g >> 2)) << 8) | ((b << 3) | (b >> 2));
    };
    auto to555 = [](uint32_t p) {
        return static_cast<uint16_t>((((p >> 19) & 31) << 10) | (((p >> 11) & 31) << 5) | ((p >> 3) & 31));
    };
    if (op.kind == WeaponDrawOp::BOLT) {                           // sub_48AD70
        weaponDrawBolt(op, m_camX, m_camY, clipW, clipH, [&](int x, int y, uint16_t c) {
            if (x < 0 || x >= clipW || y < 0 || y >= clipH) return;
            fb[y * VW + x] = c555(c);
        });
        return;
    }
    auto put = [&](int x, int y, uint32_t argb, bool orCol) {
        if (x < 0 || x >= clipW || y < 0 || y >= clipH) return;
        uint32_t& px = fb[y * VW + x];
        px = orCol ? c555(to555(px) | op.orColor) : argb;
    };
    int x0 = screenXOf(op.x), y0 = op.y - m_camY;
    int x1 = x0 + (screenXOf(op.x2) - screenXOf(op.x)), y1 = op.y2 - m_camY;
    const bool laser = op.kind == WeaponDrawOp::LASER_LINE;
    uint32_t pat = 0xFE1FC3F8u;
    if (op.phase & 31) pat = (pat << (op.phase & 31)) | (pat >> (32 - (op.phase & 31)));
    const uint32_t start = 0xD69FD69Eu;
    uint32_t col = start;
    const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        if (laser) {
            put(x0, y0, 0, true); put(x0 + 1, y0, 0, true);
            put(x0, y0 + 1, 0, true); put(x0 + 1, y0 + 1, 0, true);
        } else if (pat & 1) {
            const uint32_t lo = c555(col & 0xFFFF), hi = c555(col >> 16);
            put(x0, y0, lo, false); put(x0 + 1, y0, hi, false);
            put(x0, y0 + 1, lo, false); put(x0 + 1, y0 + 1, hi, false);
            col -= 0x30003u;
        } else {
            col = start;
        }
        if (x0 == x1 && y0 == y1) break;
        pat = (pat << 1) | (pat >> 31);
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// sub_48D860, zoom 0: ponto da janela do minimapa → pixel do mundo (tile << 5,
// o canto do tile). Fora do mapa (ou nos botões do topo, que são janelas filhas)
// não converte.
bool GameState::miniToWorld(int nx, int ny, int& px, int& py) const {
    if (!g_gameMap.ok()) return false;
    const int mx = nx - kColX, my = ny - kMiniMapY;
    if (mx < 0 || mx >= kColW || my < 0 || my >= kMiniMapH) return false;
    const int n = g_gameMap.internalXOffset() ? 3 : 4;              // botões (sub_458930)
    if (my < 16 && mx >= kColW / 2 - 0x20 && mx < kColW / 2 - 0x20 + 16 * n) return false;
    const int mw = g_gameMap.width(), mh = g_gameMap.height();
    const int tx = mx * mw / kColW, ty = my * mh / kMiniMapH;
    px = tx << 5;
    py = ty << 5;
    return true;
}

int GameState::screenXOf(int worldPixelX) const {
    int sx = worldPixelX - m_camX;
    if (!g_gameMap.ok()) return sx;
    const int worldW = g_gameMap.width() * 32;       // largura do mapa em pixels
    if (worldW <= 0) return sx;
    sx = ((sx % worldW) + worldW) % worldW;           // [0, worldW)
    if (sx > worldW / 2) sx -= worldW;                // → [-worldW/2, worldW/2)
    return sx;
}

void GameState::selectAt(int nx, int ny, bool additive) {
    // Seleciona a unidade do jogador mais próxima do clique (em coords de tela,
    // então o wrap horizontal já está resolvido), dentro de um raio.
    int me = TethysGame::localPlayer();
    if (!additive) m_selected.clear();
    int best = 0, bestD2 = 20 * 20;   // raio de tolerância
    for (int i = 1; i < static_cast<int>(g_units.size()); ++i) {
        const UnitRecord& r = g_units[i];
        if (r.id == -1 || (r.flags & UNIT_DEAD)) continue;
        if ((r.owner & 0x0F) != me) continue;
        int ucx = screenXOf(unitOX(r)), ucy = unitOY(r) - m_camY;
        int dx = ucx - nx, dy = ucy - ny, d2 = dx * dx + dy * dy;
        if (d2 < bestD2) { bestD2 = d2; best = i; }
    }
    if (best && m_selected.size() < 0x20) {               // sub_41A9E0: teto de 32
        m_selected.push_back(best);
        soundPlay(unitSelectSound(g_units[best].type));   // sub_41AAA0: desc+0x1F0
    }
}

void GameState::selectBox(int x0, int y0, int x1, int y1) {
    // Seleciona os veículos do jogador cujo CENTRO na tela cai na caixa.
    // (coords de tela → o wrap horizontal já está resolvido por screenXOf.)
    int bx0 = std::min(x0, x1), by0 = std::min(y0, y1);
    int bx1 = std::max(x0, x1), by1 = std::max(y0, y1);
    int me = TethysGame::localPlayer();
    m_selected.clear();
    for (int i = 1; i < static_cast<int>(g_units.size()); ++i) {
        const UnitRecord& r = g_units[i];
        if (r.id == -1 || (r.flags & UNIT_DEAD)) continue;
        if ((r.owner & 0x0F) != me || !(r.flags & UNIT_VEHICLE)) continue;
        int ucx = screenXOf(r.pixelX) + 16, ucy = r.pixelY + 16 - m_camY;
        if (ucx >= bx0 && ucx <= bx1 && ucy >= by0 && ucy <= by1) {
            if (m_selected.size() >= 0x20) break;   // sub_41A9E0: a seleção para em 32
            m_selected.push_back(i);
        }
    }
    // sub_41AAA0: 1 unidade → som dela (desc+0x1F0); várias → 0x55.
    if (m_selected.size() == 1) soundPlay(unitSelectSound(g_units[m_selected[0]].type));
    else if (!m_selected.empty()) soundPlay(0x55);
}

void GameState::handleEvent(Renderer& renderer, const SDL_Event& e) {
    if (m_saveDlg.active()) {                                // TFileDialog (modal)
        const SaveSlotsDialog::Result r = m_saveDlg.handleEvent(renderer, e);
        if (r == SaveSlotsDialog::kDoSave) {
            saveToFile(m_saveDlg.path(), m_saveDlg.description());   // TApp::SaveGame
        } else if (r == SaveSlotsDialog::kDoLoad) {
            m_endAfterSave = false;
            m_endPhase = 0;
            if (!loadSavedGame(m_saveDlg.path())) gTApp.activateShell();
            return;
        }
        if (r != SaveSlotsDialog::kNone && m_endAfterSave) { m_endAfterSave = false; finishGameEnd(); }
        return;
    }
    if (m_endPhase) {                                        // diálogo modal / espera
        if (e.type == SDL_MOUSEMOTION) renderer.screenToNative(e.motion.x, e.motion.y, m_mouseNX, m_mouseNY);
        if (m_endPhase != 2) return;
        if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
            int nx, ny;
            renderer.screenToNative(e.button.x, e.button.y, nx, ny);
            const int b = successButtonAt(nx, ny);
            if (b == 1) { m_endAfterSave = true; m_saveDlg.open(SaveSlotsDialog::kSave, m_mission ? m_mission->levelDesc : ""); }
            else if (b == 2) finishGameEnd();
        } else if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_RETURN) {           // IDOK = "Save Game" (OnSaveSlot)
                m_endAfterSave = true;
                m_saveDlg.open(SaveSlotsDialog::kSave, m_mission ? m_mission->levelDesc : "");
            } else if (e.key.keysym.sym == SDLK_ESCAPE) finishGameEnd();
        }
        return;
    }
    if (e.type == SDL_KEYDOWN) {
        // Aceleradores 130 do OUT2RES: Ctrl+S = 40017 (OnSaveSlot), Ctrl+L = 40018.
        if ((SDL_GetModState() & KMOD_CTRL) && e.key.keysym.sym == SDLK_s && m_mission) {
            m_saveDlg.open(SaveSlotsDialog::kSave, m_mission->levelDesc);
            return;
        }
        if ((SDL_GetModState() & KMOD_CTRL) && e.key.keysym.sym == SDLK_l) {
            m_saveDlg.open(SaveSlotsDialog::kLoad, "");
            return;
        }
        if (e.key.keysym.sym == SDLK_ESCAPE) gTApp.activateShell();
        if (e.key.keysym.sym == SDLK_g) m_showPassability = !m_showPassability;
        if (e.key.keysym.sym == SDLK_F3) m_showDebugHud = !m_showDebugHud;
        // DEPURACAO dos estados de predio (ainda nao ha armas/ConVec construindo):
        //   D = +10% do HP max de dano   H = repara (dano 0)
        //   K = Unit::DoDeath (Kill(1))  B = reinicia a construcao (sub_43DE50)
        //   O = poe o predio OPERANDO (sub_483810 — o que a economia faria)
        const SDL_Keycode k = e.key.keysym.sym;
        const bool ctrl = (SDL_GetModState() & KMOD_CTRL) != 0;
        if (ctrl && (k == SDLK_d || k == SDLK_h || k == SDLK_k || k == SDLK_b || k == SDLK_o)) {
            for (int id : std::vector<int>(m_selected)) {
                if (id <= 0 || id >= (int)g_units.size()) continue;
                UnitRecord& r = g_units[id];
                if (r.id == -1 || !(r.flags & UNIT_BUILDING)) continue;
                if (k == SDLK_d) {
                    int hp = structureMaxHP(r);
                    r.damage = static_cast<int16_t>(std::min(hp, r.damage + hp / 10));
                } else if (k == SDLK_h) {
                    r.damage = 0;
                } else if (k == SDLK_k) {
                    Unit(id).DoDeath();
                } else if (k == SDLK_b && r.action != ACT_DIE) {
                    structureStartBuild(id, 0);
                } else if (k == SDLK_o) {
                    structureTurnOn(r);
                }
            }
        }
        // University: L&ESS / &MORE.
        if (!ctrl && m_students.isOpen()) {
            if (k == SDLK_e) m_students.less();
            if (k == SDLK_m) m_students.more();
        }
        // Pesquisa (modo 1): L&ESS / &MORE.
        if (!ctrl && m_research.isOpen() && m_research.mode() == 1) {
            if (k == SDLK_e) m_research.less();
            if (k == SDLK_m) m_research.more();
        }
        // STORAGE: Alt+1..6 = botão da baia (sub_40A900(0x31 + i)).
        // sub_498890: Alt+V (40224) só com Robot Command Center ([jogador+0x1A8]).
        if ((SDL_GetModState() & KMOD_ALT) && k == SDLK_v &&
            g_colony[TethysGame::localPlayer()].stat(ST_ROBOT_GRP)) {
            closePanes(); closeTabPanes(); m_tab = -1; m_vehicles.open();
        }
        // 40200..40205: Alt+C/M/P/F/N/R → Resources no modo 1..6 (fora de missão
        // só de unidades; Alt+R só com a mina de raro pesquisada); 40206: Alt+O.
        if ((SDL_GetModState() & KMOD_ALT) && !reportsDisabled()) {
            const int resMode = k == SDLK_c ? 1 : k == SDLK_m ? 2 : k == SDLK_p ? 3 : k == SDLK_f ? 4 :
                                k == SDLK_n ? 5 : k == SDLK_r ? 6 : 0;
            if (resMode && (resMode != 6 || ResourcesPane::rareAvailable())) openReport(2, resMode);
        }
        if ((SDL_GetModState() & KMOD_ALT) && k == SDLK_o && m_mission) openReport(5, 0);
        if ((SDL_GetModState() & KMOD_ALT) && k >= SDLK_1 && k <= SDLK_6 && m_storage.isOpen())
            m_storage.pressBay(static_cast<int>(k - SDLK_1));
        // Atalhos dos botões de comando (letra após o '&' da dica).
        if (!ctrl && k >= SDLK_a && k <= SDLK_z) {
            const char key = static_cast<char>('A' + (k - SDLK_a));
            bool done = false;
            for (const CmdButton& b : commandButtons())
                if (b.hotkey == key) { clickCommand(b.cmd); done = true; break; }
            if (!done)
                for (const TextButton& b : structureButtons())
                    if (b.hotkey == key) { clickCommand(b.cmd); break; }
        }
    }
    if (e.type == SDL_MOUSEMOTION) {
        renderer.screenToNative(e.motion.x, e.motion.y, m_mouseNX, m_mouseNY);
    }
    if (e.type == SDL_MOUSEBUTTONDOWN) {
        int nx, ny; renderer.screenToNative(e.button.x, e.button.y, nx, ny);
        m_mouseNX = nx; m_mouseNY = ny;
        // Vista do mapa em (kViewX, kViewY); vx/vy = coordenadas dentro dela.
        const int vx = nx - kViewX, vy = ny - kViewY;
        bool inView = vx >= 0 && vx < viewportW() && vy >= 0 && vy < viewportH();
        // Painel de produção: lista e setas (sub_4683F0 / sub_4683B0 / sub_430910-60).
        bool paneHit = false;
        // Minimapa (sub_4583D0 case 513): ponto válido → arrasto da vista.
        if (e.button.button == SDL_BUTTON_LEFT && !inView) {
            int px, py;
            if (miniToWorld(nx, ny, px, py)) {
                m_miniDrag = true;
                requestCenterView(px, py);
                paneHit = true;
            }
        }
        if (e.button.button == SDL_BUTTON_LEFT && !inView && m_produce.isOpen()) {
            bool buildNow = false;
            paneHit = m_produce.click(infoPaneRect(), nx, ny, e.button.clicks >= 2, buildNow);
            if (buildNow) produceBuildIt();
        }
        if (e.button.button == SDL_BUTTON_LEFT && !inView && !paneHit && m_students.isOpen())
            paneHit = m_students.click(kColX, kPaneWinY, nx, ny);   // LESS/MORE: 0x57 lá dentro
        if (e.button.button == SDL_BUTTON_LEFT && !inView && !paneHit && m_research.isOpen()) {
            bool dbl = false;
            paneHit = m_research.click(kColX, kPaneWinY, infoPaneRect(), nx, ny, e.button.clicks >= 2, dbl);
            if (dbl) clickCommand(kCmdAssign);
        }
        if (e.button.button == SDL_BUTTON_LEFT && !inView && !paneHit && m_vehicles.isOpen())
            paneHit = m_vehicles.click(infoPaneRect(), nx, ny);
        if (e.button.button == SDL_BUTTON_LEFT && !inView && !paneHit && m_storage.isOpen())
            paneHit = m_storage.click(kColX, kPaneWinY, nx, ny);
        if (e.button.button == SDL_BUTTON_LEFT && !inView && !paneHit) {
            syncTabToSelection();
            for (int k = 0; k < 6 && !paneHit; ++k) {
                PaneRect r;
                if (tabRect(k, r) && nx >= r.l && nx < r.r && ny >= r.t && ny < r.b) { openTab(k); paneHit = true; }
            }
        }
        if (e.button.button == SDL_BUTTON_LEFT && !inView && !paneHit && m_tab == 3 && m_comms.isOpen())
            paneHit = m_comms.click(infoPaneRect(), nx, ny);
        if (e.button.button == SDL_BUTTON_LEFT && !inView && !paneHit && m_tab == 4 && m_space.isOpen())
            paneHit = m_space.click(infoPaneRect(), nx, ny);
        if (e.button.button == SDL_BUTTON_LEFT && !inView && !paneHit && m_tab == 2 && m_resources.isOpen()) {
            bool dbl = false;
            paneHit = m_resources.click(infoPaneRect(), nx, ny, e.button.clicks >= 2, dbl);
            if (dbl) clickCommand(kCmdDetails);
        }
        if (e.button.button == SDL_BUTTON_LEFT && !inView && !paneHit && m_tab == 1 && m_labs.isOpen()) {
            bool dbl = false;
            paneHit = m_labs.click(infoPaneRect(), nx, ny, e.button.clicks >= 2, dbl);
            if (dbl) clickCommand(kCmdDetails);
        }
        if (e.button.button == SDL_BUTTON_LEFT && !inView && !paneHit && m_tab == 0 && m_factories.isOpen()) {
            bool dbl = false;
            paneHit = m_factories.click(infoPaneRect(), nx, ny, e.button.clicks >= 2, dbl);
            if (dbl) clickCommand(kCmdDetails);
        }
        if (e.button.button == SDL_BUTTON_LEFT && !inView && !paneHit && m_tab == 5 && m_mission)
            paneHit = m_objectives.click(infoPaneRect(), nx, ny);
        // Botão de comando do quadro (IDLE / ACTIVATE → CommandPacket 0x12 / 0x13).
        if (e.button.button == SDL_BUTTON_LEFT && !inView && !paneHit) {
            for (const TextButton& b : structureButtons())
                if (nx >= b.l && nx < b.r && ny >= b.t && ny < b.b) { clickCommand(b.cmd); break; }
        }
        if (e.button.button == SDL_BUTTON_LEFT && !inView) {
            for (const CmdButton& b : commandButtons())
                if (nx >= b.l && nx < b.r && ny >= b.t && ny < b.b) { clickCommand(b.cmd); break; }
        }
        if (e.button.button == SDL_BUTTON_LEFT && inView && m_targetCmd >= 0 && uiCommandIsArea(m_targetCmd)) {
            // Área (sub_41B550 modos 1/4): o arrasto começa aqui e termina ao soltar.
            m_areaDrag = true;
            m_areaSX = m_camX + vx; m_areaSY = m_camY + vy;
        } else if (e.button.button == SDL_BUTTON_LEFT && inView && m_launchFrom) {
            // sub_457480: o clique escolhe o alvo do míssil (pacote 0x19 no ponto).
            const int px = g_gameMap.ok() ? (m_camX + vx) & (g_gameMap.width() * 32 - 1) : m_camX + vx;
            spaceportLaunch(m_launchFrom, px, m_camY + vy);
            m_launchFrom = 0;
            closePanes();
            soundPlay(0x56);
        } else if (e.button.button == SDL_BUTTON_RIGHT && m_launchFrom) {
            m_launchFrom = 0;
        } else if (e.button.button == SDL_BUTTON_LEFT && inView && m_targetCmd >= 0) {
            // Modo de mira: o clique escolhe o alvo (vt[3] do comando).
            int tileX = (m_camX + vx) / 32, tileY = (m_camY + vy) / 32;
            if (g_gameMap.ok()) tileX &= (g_gameMap.width() - 1);
            uiSetCursorPixel(g_gameMap.ok() ? (m_camX + vx) & (g_gameMap.width() * 32 - 1) : m_camX + vx, m_camY + vy);
            if (uiCommandTarget(m_targetCmd, m_selected, tileX, tileY) != UiClick::TARGET)
                m_targetCmd = -1;                            // Patrulhar: 2 cliques
        } else if (e.button.button == SDL_BUTTON_RIGHT && m_targetCmd >= 0) {
            m_targetCmd = -1;                                // cancela a mira
        } else if (e.button.button == SDL_BUTTON_LEFT && inView) {
            m_dragging = true; m_dragSX = vx; m_dragSY = vy;
        } else if (e.button.button == SDL_BUTTON_RIGHT && inView && !m_selected.empty()) {
            // Mover os SELECIONADOS para o tile clicado (pipeline real + espiral).
            int tileX = (m_camX + nx) / 32, tileY = (m_camY + ny) / 32;
            if (g_gameMap.ok()) tileX &= (g_gameMap.width() - 1);
            int me = TethysGame::localPlayer();
            CommandPacket pkt; pkt.type = CMD_MOVE;
            pkt.waypoints.push_back(packLocation(tileX, tileY));
            for (int id : m_selected) {
                Unit u(id);
                if (u.IsLive() && u.OwnerID() == me && u.IsVehicle()) pkt.unitIds.push_back(id);
            }
            if (!pkt.unitIds.empty()) { processCommandPacket(me, pkt); soundPlay(0x56); }
        }
    }
    if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT && m_miniDrag) {
        // case 514: ponto final, depois solta (flags 2, sem 1|4).
        int px, py;
        if (miniToWorld(m_mouseNX, m_mouseNY, px, py)) requestCenterView(px, py);
        m_miniDrag = false;
    }
    if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT && m_areaDrag) {
        m_areaDrag = false;
        int nx, ny; renderer.screenToNative(e.button.x, e.button.y, nx, ny);
        int x1, y1, x2, y2;
        areaRect(m_camX + nx - kViewX, m_camY + ny - kViewY, x1, y1, x2, y2);
        if (m_targetCmd >= 0 && uiCommandArea(m_targetCmd, m_selected, x1, y1, x2, y2) != UiClick::TARGET)
            m_targetCmd = -1;
    }
    if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT && m_dragging) {
        int nx, ny; renderer.screenToNative(e.button.x, e.button.y, nx, ny);
        const int vx = nx - kViewX, vy = ny - kViewY;     // coordenadas na vista
        int dx = std::abs(vx - m_dragSX), dy = std::abs(vy - m_dragSY);
        if (dx < 4 && dy < 4) selectAt(vx, vy, false);   // clique = seleção simples
        else selectBox(m_dragSX, m_dragSY, vx, vy);        // caixa = multi-seleção
        m_dragging = false;
    }
}

// Retângulo do arrasto em tiles (x2/y2 exclusivos): muro/tubo passa pela reta
// de sub_41C410; terraplanar é o retângulo dos dois cantos.
void GameState::areaRect(int ex, int ey, int& x1, int& y1, int& x2, int& y2) const {
    int sx = m_areaSX, sy = m_areaSY;
    if (uiCommandIsLine(m_targetCmd)) wallDragConstrain(sx, sy, ex, ey);
    const int ax = sx >> 5, ay = sy >> 5, bx = ex >> 5, by = ey >> 5;
    x1 = std::min(ax, bx); x2 = std::max(ax, bx) + 1;
    y1 = std::min(ay, by); y2 = std::max(ay, by) + 1;
    if (y1 < 0) y1 = 0;
    if (g_gameMap.ok()) {
        if (y2 > g_gameMap.height()) y2 = g_gameMap.height();
        const int m = g_gameMap.width() - 1;
        const int w = x2 - x1;
        x1 &= m; x2 = x1 + w;
    }
}

// sub_46D540: lista dos 22 (1 unidade) ou dos 11 (várias); cada comando
// disponível vira um botão na posição do descritor, numa grade 3x3 de 46 px:
// x = 28 + 46*col, y = topo do quadro + 0x3C + 57 + 46*lin. Só p/ a botoeira de
// veículo (painel off_4D5B28); prédios usam os botões de texto do quadro.
std::vector<GameState::CmdButton> GameState::commandButtons() const {
    std::vector<CmdButton> out;
    if (m_selected.empty() || m_confirm.isOpen() || m_vehicles.isOpen() || m_tab != -1) return out;   // outro painel no lugar da botoeira
    const UnitRecord& u = g_units[m_selected[0]];
    if (u.id == -1 || !(u.flags & 0x42u) || (u.owner & 0x0F) != TethysGame::localPlayer()) return out;
    const std::vector<int>& list = m_selected.size() == 1 ? uiCommandsSingle() : uiCommandsMulti();
    const int px0 = kColX;                                // janela do quadro
    const int top = kPaneTop + 0x3C + 57;                 // dword_57556C + dword_578F4C + 0x3C
    for (int cmd : list) {
        if (!uiCommandAvailable(cmd, m_selected)) continue;
        const UiCommandDesc d = uiCommandDescribe(cmd, m_selected);
        if (d.slot < 1 || d.slot > 9 || !d.iconRaw) continue;
        const int row = (d.slot - 1) / 3, col = (d.slot - 1) % 3;
        CmdButton b;
        b.cmd = cmd;
        b.l = px0 + 28 + 46 * col; b.t = top + 46 * row;
        b.r = b.l + 44; b.b = b.t + 44;
        b.iconRaw = d.iconRaw;
        b.tip = d.tip;
        b.hotkey = uiCommandHotkey(d);
        out.push_back(b);
    }
    return out;
}

// sub_459F70: com 1 prédio selecionado, os comandos disponíveis da lista de 12
// (até 4) viram botões de texto; sub_458D10 os dispõe a partir de x = 9, de
// baixo p/ cima (altura H = 25, largura W = 87): 1 → largo embaixo; 2 → lado a
// lado; 3 → dois em cima + largo embaixo; 4 → 2x2. PROVISÓRIO: base = fundo do
// quadro (dword_5750EC[+0x40]) e x relativo ao painel.
std::vector<GameState::TextButton> GameState::structureButtons() const {
    std::vector<TextButton> out;
    auto layoutButtons = [&]() {
        const int W = 87, H = 25, x0 = kColX + 9, bottom = kPaneBottom - 2;
        const int n = static_cast<int>(out.size());
        for (int i = 0; i < n; ++i) {
            TextButton& b = out[i];
            const bool twoRows = n >= 3;
            const int row = twoRows ? (i < 2 ? 0 : 1) : 1;
            const int col = (n == 1 || (n == 3 && i == 2)) ? 0 : (i % 2);
            b.wide = n == 1 || (n == 3 && i == 2);
            b.l = x0 + col * W;
            b.t = bottom - (row == 0 ? 2 * H : H);
            b.r = b.l + (b.wide ? 175 : W);
            b.b = b.t + H;
        }
    };
    if (m_vehicles.isOpen()) {                               // sub_46B2D0: &JUMP / &STATUS
        TextButton b{};
        b.cmd = kCmdJump; b.label = "&JUMP"; b.tip = "&Jump to selected vehicle type"; b.hotkey = 'J';
        b.disabled = !m_vehicles.jumpEnabled();
        out.push_back(b);
        b = TextButton{};
        b.cmd = kCmdStatus; b.label = "&STATUS"; b.tip = "Back to Structure &Status"; b.hotkey = 'S';
        out.push_back(b);
        layoutButtons();
        return out;
    }
    if (m_tab == 4) {
        // sub_46CFA0: modo 0 "&STARSHIP" (só com a pesquisa do tipo 0x37); 1 "&SATELLITES".
        TextButton b{};
        b.cmd = kCmdSpaceMode;
        if (m_space.mode() == 0) {
            if (!SpacePane::starshipAvailable()) return out;
            b.label = "&STARSHIP"; b.tip = "Show &Starship Information"; b.hotkey = 'S';
        } else {
            b.label = "&SATELLITES"; b.tip = "Show &Satellite Information"; b.hotkey = 'S';
        }
        out.push_back(b);
        layoutButtons();
        return out;
    }
    if (m_tab == 2) {
        // sub_469490: 0 → "&DETAILS" largo; 1..6 → "S&TRUCTURES" + "&SUMMARY";
        // 7..12 → "&DETAILS" + "&SUMMARY".
        const int md = m_resources.mode();
        TextButton b{};
        if (md == 0 || md >= 7) {
            b.cmd = kCmdDetails; b.label = "&DETAILS"; b.tip = "&Details"; b.hotkey = 'D';
            b.disabled = !m_resources.detailsEnabled();
            out.push_back(b);
        } else {
            b.cmd = kCmdResStructures; b.label = "S&TRUCTURES"; b.tip = "S&tructures"; b.hotkey = 'T';
            out.push_back(b);
        }
        if (md != 0) {
            b = TextButton{};
            b.cmd = kCmdResSummary; b.label = "&SUMMARY"; b.tip = "&Summary"; b.hotkey = 'S';
            out.push_back(b);
        }
        layoutButtons();
        return out;
    }
    if (m_tab == 0) {
        // sub_464BD0: "&DETAILS" largo, habilitado depois de escolher uma fábrica.
        TextButton b{};
        b.cmd = kCmdDetails; b.label = "&DETAILS"; b.tip = "&Details"; b.hotkey = 'D';
        b.disabled = !m_factories.detailsEnabled();
        out.push_back(b);
        layoutButtons();
        return out;
    }
    if (m_tab == 1) {
        // sub_46BB90: modo 0 DETAILS + C&OMPLETED; 1 DETAILS + C&URRENT; 2 C&OMPLETED + C&URRENT.
        TextButton b{};
        if (m_labs.mode() != 2) {
            b.cmd = kCmdDetails; b.label = "&DETAILS"; b.tip = "&Details"; b.hotkey = 'D';
            b.disabled = !m_labs.detailsEnabled();
            out.push_back(b);
        }
        if (m_labs.mode() != 1) {
            b = TextButton{};
            b.cmd = kCmdLabsCompleted; b.label = "C&OMPLETED"; b.tip = "C&ompleted Research"; b.hotkey = 'O';
            out.push_back(b);
        }
        if (m_labs.mode() != 0) {
            b = TextButton{};
            b.cmd = kCmdLabsCurrent; b.label = "C&URRENT"; b.tip = "C&urrent Research"; b.hotkey = 'U';
            out.push_back(b);
        }
        layoutButtons();
        return out;
    }
    if (m_tab == 3) {
        // sub_4670A0 (modo 0, jogo solo): só "&JUMP", largo; habilitado quando a
        // mensagem escolhida tem posição (sub_466970).
        TextButton b{};
        b.cmd = kCmdMsgJump; b.label = "&JUMP"; b.tip = "&Jump to the message's location"; b.hotkey = 'J';
        b.disabled = !m_comms.jumpEnabled();
        out.push_back(b);
        layoutButtons();
        return out;
    }
    if (m_tab == 5 && m_mission) {
        // Aba de objetivos (sub_465830, modo 1): "&GAME OPTIONS" largo.
        TextButton b{};
        b.cmd = kCmdGameOptions; b.label = "&GAME OPTIONS"; b.tip = "Show &Game Options"; b.hotkey = 'G';
        b.wide = true;
        b.l = kColX + 9; b.t = kPaneBottom - 2 - 25; b.r = b.l + 175; b.b = b.t + 25;
        out.push_back(b);
        return out;
    }
    if (m_selected.size() != 1) return out;
    const UnitRecord& u = g_units[m_selected[0]];
    if (u.id == -1 || (u.owner & 0x0F) != TethysGame::localPlayer()) return out;
    // Confirmação (dword_564EA8) vale p/ prédio e veículo (despejo de carga).
    if (!(u.flags & UNIT_BUILDING) && !m_confirm.isOpen()) return out;
    if (m_confirm.isOpen()) {                                // sub_45C200: &YES / &NO
        TextButton b{};
        b.cmd = kCmdYes; b.label = "&YES"; b.tip = "&YES: Confirm command"; b.hotkey = 'Y';
        out.push_back(b);
        b = TextButton{};
        b.cmd = kCmdNo; b.label = "&NO"; b.tip = "&NO: Abort command"; b.hotkey = 'N';
        out.push_back(b);
    }
    if (m_students.isOpen()) {                               // sub_46E0D0: S&TART / &STATUS
        TextButton b{};
        b.cmd = kCmdStartClass; b.label = "S&TART"; b.tip = "S&tart Class"; b.hotkey = 'T';
        out.push_back(b);
        b = TextButton{};
        b.cmd = kCmdStatus; b.label = "&STATUS"; b.tip = "&Status"; b.hotkey = 'S';
        out.push_back(b);
    }
    if (m_research.isOpen()) {
        // sub_46C520: modo 0 = ASSIGN (habilita com tópico) / CANCEL;
        // modo 1 = START / TOPICS (ou STATUS se o lab já pesquisa).
        TextButton b{};
        if (m_research.mode() == 0) {
            b.cmd = kCmdAssign; b.label = "&ASSIGN"; b.tip = "&Assign Scientists"; b.hotkey = 'A';
            b.disabled = !m_research.assignEnabled();
            out.push_back(b);
            b = TextButton{};
            b.cmd = kCmdCancel; b.label = "CA&NCEL"; b.tip = "Ca&ncel"; b.hotkey = 'N';
            out.push_back(b);
        } else {
            b.cmd = kCmdStart; b.label = "ST&ART"; b.tip = "St&art Research"; b.hotkey = 'A';
            out.push_back(b);
            b = TextButton{};
            if (m_research.researching()) { b.cmd = kCmdStatus; b.label = "&STATUS"; b.tip = "&Status"; b.hotkey = 'S'; }
            else { b.cmd = kCmdTopics; b.label = "&TOPICS"; b.tip = "Research &Topics"; b.hotkey = 'T'; }
            out.push_back(b);
        }
    }
    if (m_storage.isOpen() && m_storage.resourceMode()) {
        // sub_463170 (recursos): L&OAD / &UNLOAD (dword_564A68 / dword_564A5C) e &STATUS.
        TextButton b{};
        b.cmd = kCmdLoad; b.label = "L&OAD"; b.tip = "L&oad Truck"; b.hotkey = 'O';
        b.disabled = !m_storage.loadEnabled();
        out.push_back(b);
        b = TextButton{};
        b.cmd = kCmdUnload; b.label = "&UNLOAD"; b.tip = "&Unload Truck"; b.hotkey = 'U';
        b.disabled = !m_storage.unloadEnabled();
        out.push_back(b);
        b = TextButton{};
        b.cmd = kCmdStatus; b.label = "&STATUS"; b.tip = "Back to Structure &Status"; b.hotkey = 'S';
        out.push_back(b);
    } else if (m_storage.isOpen()) {
        // sub_463170: "&STATUS"; no modo plataforma ([+0x6C4]) também "L&AUNCH".
        TextButton b{};
        b.cmd = kCmdStatus; b.label = "&STATUS"; b.tip = "Back to Structure &Status"; b.hotkey = 'S';
        out.push_back(b);
        if (m_storage.launchMode()) {
            b = TextButton{};
            b.cmd = kCmdLaunch; b.label = "L&AUNCH"; b.tip = "L&aunch Vehicle into Orbit"; b.hotkey = 'A';
            b.disabled = !spaceportLaunchAvailable(m_storage.building());
            out.push_back(b);
        }
    }
    if (m_produce.isOpen()) {
        // sub_4682C0: "&BUILD IT" (desabilitado até escolher um item pagável) e
        // "&STATUS" — ambos voltam ao painel de status (dword_564A80).
        TextButton b{};
        b.cmd = kCmdBuildIt; b.label = "&BUILD IT"; b.tip = "&Build selected item"; b.hotkey = 'B';
        b.disabled = !m_produce.buildEnabled();
        out.push_back(b);
        b = TextButton{};
        b.cmd = kCmdStatus; b.label = "&STATUS"; b.tip = "Back to Structure &Status"; b.hotkey = 'S';
        out.push_back(b);
    }
    for (int cmd : uiCommandsStructure()) {
        if (anyPaneOpen()) break;
        if (out.size() == 4) break;
        if (!uiCommandAvailable(cmd, m_selected)) continue;
        const UiCommandDesc d = uiCommandDescribe(cmd, m_selected);
        TextButton b{};
        b.cmd = cmd; b.label = d.label; b.tip = d.tip; b.hotkey = uiCommandHotkey(d);
        out.push_back(b);
    }
    // sub_459F70: o Robot Command Center (0x1F) ganha "&VEHICLES" junto dos
    // botões disponíveis (sem nenhum, não aparece).
    if (!anyPaneOpen() && u.type == 0x1F && !out.empty() && out.size() < 4) {
        TextButton b{};
        b.cmd = kCmdVehicles; b.label = "&VEHICLES"; b.tip = "Show &Vehicle List"; b.hotkey = 'V';
        out.push_back(b);
    }
    const int W = 87, H = 25, x0 = kColX + 9, bottom = kPaneBottom - 2;
    const int n = static_cast<int>(out.size());
    for (int i = 0; i < n; ++i) {
        TextButton& b = out[i];
        const bool twoRows = n >= 3;
        const int row = twoRows ? (i < 2 ? 0 : 1) : 1;           // 0 = de cima
        const int col = (n == 1 || (n == 3 && i == 2)) ? 0 : (i % 2);
        b.wide = n == 1 || (n == 3 && i == 2);
        b.l = x0 + col * W;
        b.t = bottom - (row == 0 ? 2 * H : H);
        b.r = b.l + (b.wide ? 175 : W);
        b.b = b.t + H;
    }
    return out;
}

void GameState::debugVehicleList(int row, bool jump) {
    if (!m_vehicles.isOpen()) clickCommand(kCmdVehicles);
    const PaneRect pane = infoPaneRect();
    const ListBoxLayout L = listBoxLayout(pane, 0, 0x0A, 0, 0x12);
    if (row >= 0) m_vehicles.click(pane, L.list.l + 5, L.list.t + 2 + row * L.lineH + 2);
    if (jump) clickCommand(kCmdJump);
}

// sub_45CB60/sub_45CBA0: trocar a seleção (por outro que não o próprio quadro —
// inclusive JUMP/DETAILS dos relatórios) marca [+0x484]; no quadro seguinte
// sub_45CC30 põe o painel da unidade — ou nenhum, sem seleção.
void GameState::syncTabToSelection() {
    if (m_selected == m_lastSel) return;
    m_lastSel = m_selected;
    m_tab = -1;
    closeTabPanes();
    closePanes();                                            // o painel da unidade substitui o atual
}

// sub_457760 (DETAILS): unidade viva → centraliza a vista nela (sub_408120) e a
// seleciona (sub_41A8E0 + sub_41A710) — a troca de seleção põe o painel dela.
void GameState::reportDetails(int unit) {
    if (unit <= 0 || unit >= static_cast<int>(g_units.size())) return;
    const UnitRecord& r = g_units[unit];
    if (r.id == -1 || (r.flags & UNIT_DEAD)) return;
    requestCenterView(unitOX(r), unitOY(r));
    m_selected = {unit};
}

// Retângulo da aba k (sub_45C940): encostadas a partir de x = 1 no topo da
// janela do quadro, cada uma da largura da sua arte (sub_404D30).
bool GameState::tabRect(int k, PaneRect& r) const {
    static const int kTabArt[6] = {0x8E, 0x91, 0x90, 0x8F, 0x92, 0x8D};   // dword_4D5390
    if (!g_art.ok() || k < 0 || k > 5) return false;
    int x = 1;
    for (int i = 0; i <= k; ++i) {
        const int anim = kTabArt[i] - 0x65;
        if (anim >= g_art.numAnimations()) return false;
        const ArtAnimation& an = g_art.animation(anim);
        const int w = static_cast<int>(an.header[3]) - static_cast<int>(an.header[1]);
        const int h = static_cast<int>(an.header[4]) - static_cast<int>(an.header[2]);
        if (i == k) { r = PaneRect{kColX + x, kPaneWinY, kColX + x + w, kPaneWinY + h}; return true; }
        x += w;
    }
    return false;
}

// sub_45C620: clicar na aba limpa a seleção (sub_41A8E0, com o quadro como
// origem: sem [+0x484]), volta o painel ao modo 0 (vt[5]) e o põe no quadro
// (sub_45D020, aba acesa). PROVISÓRIO: só as abas já portadas respondem.
bool GameState::reportsDisabled() const {
    return m_mission && m_mission->desc && m_mission->desc->boolUnitMission;
}

void GameState::openReport(int k, int mode) {
    closePanes();
    closeTabPanes();
    m_targetCmd = -1;
    m_tab = k;
    m_lastSel = m_selected;
    if (k == 0) m_factories.open();
    if (k == 1) m_labs.open(mode);
    if (k == 2) m_resources.open(mode);
    if (k == 3) m_comms.open();
    if (k == 4) m_space.open(mode);
}

void GameState::openTab(int k) {
    if (k == 5 ? !m_mission : k == 4 ? false : (k < 0 || k > 3 || reportsDisabled())) return;
    m_targetCmd = -1;
    m_selected.clear();
    m_lastSel.clear();
    closePanes();
    closeTabPanes();
    m_tab = k;
    if (k == 0) m_factories.open();
    if (k == 1) m_labs.open(0);
    if (k == 2) m_resources.open(0);
    if (k == 3) m_comms.open();
    if (k == 4) m_space.open(0);
    soundPlay(0x57);
}

void GameState::debugTab(int k, int row) {
    PaneRect r;
    if (tabRect(k, r)) openTab(k);
    if (k == 3 && row >= 0) m_comms.select(row);
    if (k == 0 && row >= 0) m_factories.select(row);
    if (k == 1 && row >= 0) m_labs.select(row);
    if (k == 2 && row >= 0) m_resources.select(row);
}

PaneRect GameState::infoPaneRect() const {
    return PaneRect{kColX, kPaneTop, kColX + kColW, kPaneBottom};
}

// BUILD IT (sub_467D50): item escolhido → sub_456B50 (pacote 9); volta ao status.
void GameState::produceBuildIt() {
    if (!m_produce.isOpen() || !m_produce.buildEnabled() || !m_produce.selected()) return;
    sendProduce(m_produce.factory(), m_produce.selected());
    m_produce.close();
}

// Som tocado no desenho (vt[6]) do original: uma vez por tick por unidade/som,
// já que aqui há vários quadros por tick.
void GameState::drawSound(int unitId, int x, int y, int id) {
    const uint32_t t = TethysGame::tick();
    if (t != m_drawSoundTick) { m_drawSoundTick = t; m_drawSounds.clear(); }
    const long long key = (static_cast<long long>(unitId) << 16) | id;
    if (std::find(m_drawSounds.begin(), m_drawSounds.end(), key) != m_drawSounds.end()) return;
    m_drawSounds.push_back(key);
    soundPlayAt(x, y, id);
}

bool GameState::onScreen(int sx, int sy) const {
    return sx > -64 && sx < viewportW() + 64 && sy > -64 && sy < viewportH() + 64;
}

void GameState::clickCommand(int cmd) {                  // vt[1] do comando
    soundPlay(0x57);                                      // sub_40A960: botão solto dentro
    if (cmd == kCmdBuildIt) { produceBuildIt(); return; }
    if (cmd == kCmdLaunch) {                                  // sub_4573C0
        const int sp = m_storage.building();
        if (!spaceportLaunchAvailable(sp)) return;
        if (g_units[sp].padCargo == 0x6B) {                   // míssil: mira (sub_41B550 modo 3)
            m_launchFrom = sp;
            m_targetCmd = -1;
            return;
        }
        spaceportLaunch(sp, 0, 0);                            // pacote 0x19
        closePanes();
        soundPlay(0x56);
        return;
    }
    if (cmd == kCmdLoad) { m_storage.load(); return; }       // pacote 0x0B
    if (cmd == kCmdUnload) { m_storage.unload(); return; }   // pacote 0x0C
    if (cmd == kCmdStatus || cmd == kCmdCancel) { closePanes(); return; }
    if (cmd == kCmdVehicles) { closePanes(); m_vehicles.open(); return; }   // dword_567D80
    if (cmd == kCmdDetails) {
        if (m_tab == 0 && m_factories.detailsEnabled()) reportDetails(m_factories.unit());
        if (m_tab == 2 && m_resources.detailsEnabled()) {
            if (m_resources.mode() == 0) m_resources.open(m_resources.detailsMode());   // botão de modo
            else reportDetails(m_resources.unit());                                     // off_4E2D20
        }
        if (m_tab == 1 && m_labs.detailsEnabled()) {
            if (m_labs.mode() == 0) reportDetails(m_labs.lab());    // sub_4576D0
            else m_labs.open(2);                                    // botão de modo (sub_45C460)
        }
        return;
    }
    if (cmd == kCmdLabsCompleted) { if (m_tab == 1) m_labs.open(1); return; }
    if (cmd == kCmdLabsCurrent) { if (m_tab == 1) m_labs.open(0); return; }
    if (cmd == kCmdResStructures) { if (m_tab == 2) m_resources.open(m_resources.mode() + 6); return; }
    if (cmd == kCmdResSummary) { if (m_tab == 2) m_resources.open(0); return; }
    if (cmd == kCmdSpaceMode) { if (m_tab == 4) m_space.open(m_space.mode() == 0 ? 1 : 0); return; }
    if (cmd == kCmdMsgJump) {                               // sub_466A00 → sub_438EC0 → sub_408120
        int px, py;
        if (m_comms.jump(px, py)) requestCenterView(px, py);
        return;
    }
    if (cmd == kCmdJump) {                                  // sub_46B3D0
        int px, py;
        const int v = m_vehicles.jump(px, py);
        if (v) { requestCenterView(px, py); m_selected = {v}; }
        return;
    }
    if (cmd == kCmdAssign) {                                // botão volta ao painel no modo 1
        if (m_research.isOpen() && m_research.assignEnabled()) m_research.open(m_research.lab(), 1);
        return;
    }
    if (cmd == kCmdTopics) { if (m_research.isOpen()) m_research.open(m_research.lab(), 0); return; }
    if (cmd == kCmdStart) { m_research.start(); return; }
    if (cmd == kCmdStartClass) { m_students.start(); return; }
    if (cmd == kCmdYes) {                                   // confirma o comando guardado
        const int c = m_confirm.command();
        m_confirm.close();
        if (c == UC_DUMP_CARGO) { if (!m_selected.empty()) sendDumpCargo(m_selected[0]); }   // pacote 0x0E
        else uiCommandClick(UC_STOP, m_selected);           // pacote 5 (parar)
        return;
    }
    if (cmd == UC_DUMP_CARGO && m_selected.size() == 1) {   // sub_4557B0(0): pergunta antes
        closePanes();
        m_confirm.open(m_selected[0], "Are you sure you want to dump this cargo?", UC_DUMP_CARGO);
        return;
    }
    if (cmd == kCmdNo) { m_confirm.close(); return; }
    if (cmd == UC_ASSIGN_STUDENTS && m_selected.size() == 1) {      // sub_4564E0 → dword_56AF28
        closePanes();
        m_students.open(m_selected[0]);
        return;
    }
    if ((cmd == UC_STOP_RESEARCH || cmd == UC_STOP_TRAINING) && m_selected.size() == 1) {   // sub_457FC0 / sub_458100
        closePanes();
        m_confirm.open(m_selected[0], cmd == UC_STOP_RESEARCH
            ? "Stopping loses all research already done on this topic. Confirm your order to stop?"
            : "Stopping loses all training already done with these workers. Confirm your order to stop?");
        return;
    }
    if (cmd == UC_ASSIGN_SCIENTISTS && m_selected.size() == 1) {   // sub_456340 → unk_569110
        closePanes();
        m_research.open(m_selected[0], g_units[m_selected[0]].action == 0x16 ? 1 : 0);
        return;
    }
    if (cmd == UC_LAUNCH_PAD && m_selected.size() == 1) {   // sub_456730 → painel [+0x6C4] = 1
        closePanes();
        m_storage.open(m_selected[0], true);
        return;
    }
    if (cmd == UC_STORAGE && m_selected.size() == 1) {      // loc_456640 → painel dword_5650F0
        if (StoragePane::supports(g_units[m_selected[0]].type)) {
            closePanes();
            m_storage.open(m_selected[0]);
        } else {
            postMessage(-1, -1, "STORAGE deste predio ainda nao portado", 0);
        }
        return;
    }
    if (cmd == kCmdGameOptions) { postMessage(-1, -1, "Game Options ainda nao portado", 0); return; }
    if (cmd == UC_BUILD) {                                // sub_456250 → painel de produção
        if (m_selected.size() == 1) { closePanes(); m_produce.open(m_selected[0]); }
        m_targetCmd = -1;
        return;
    }
    switch (uiCommandClick(cmd, m_selected)) {
        case UiClick::TARGET: m_targetCmd = cmd; break;
        case UiClick::DONE: m_targetCmd = -1; break;
        case UiClick::NOT_PORTED:
            postMessage(-1, -1, "Comando ainda nao portado", 0);
            break;
    }
}

// Nome curto por map_id de veículo (p/ o painel).
// Nomes de exibicao das unidades, do MANUAL do jogo (seccao "Unit Reference",
// Vehicles). O Code_Name interno do vehicles.txt NAO e o nome mostrado ao
// jogador; a coluna "Unit Name" da planilha e so um indice de letra (a..q).
// A correspondencia foi confirmada pelos dados do proprio vehicles.txt:
//   MOBILE_HOME  Cargo=25  -> Evacuation Transport ("carry up to 25 Colonists")
//   BIG_TANK     Cargo=2   -> Tiger (chassi pesado, torres DUPLAS, tracked)
//   WALL_TRUCK   Track=T   -> Earthworker (tracked; constroi Tubes/Walls)
//   REPAIR_TRUCK Owner=E   -> Repair Vehicle ("available only to Eden")
//   GEO_METRO    Owner=E   -> GeoCon ("available only to Eden")
//   SCORPION     Track=L,P -> Scorpion (legged, Plymouth)
static const char* vehName(int type) {
    switch (type) {                          // map_id REAL (off_4E1340 = SDK)
        case mapCargoTruck:          return "Cargo Truck";          // BIG_TRUCK
        case mapConVec:              return "ConVec";               // CON_TRUCK
        case mapSpider:              return "Spider";
        case mapScorpion:            return "Scorpion";
        case mapLynx:                return "Lynx";                 // LIGHT_TANK (chassi leve)
        case mapPanther:             return "Panther";              // MED_TANK   (chassi medio)
        case mapTiger:               return "Tiger";                // BIG_TANK   (chassi pesado)
        case mapRoboSurveyor:        return "Robo-Surveyor";        // SURVEYOR
        case mapRoboMiner:           return "Robo-Miner";           // MINER
        case mapGeoCon:              return "GeoCon";               // GEO_METRO
        case mapScout:               return "Scout";
        case mapRoboDozer:           return "Robo-Dozer";           // BULL_DOZER
        case mapEvacuationTransport: return "Evacuation Transport"; // MOBILE_HOME
        case mapRepairVehicle:       return "Repair Vehicle";       // REPAIR_TRUCK
        case mapEarthworker:         return "Earthworker";          // WALL_TRUCK
        default: return "Unidade";
    }
}

// Lynx/Panther/Tiger sao CHASSIS: o nome exibido e composto com a torre
// ("Laser Lynx #1-01", "EMP Lynx" -- Tutorial 4 do manual). So esses tres tem
// "Weapon enabled = Y" no vehicles.txt. Enquanto as torres nao estao portadas,
// mostramos so o chassi.
static bool vehIsCombatChassis(int type) {
    return type == mapLynx || type == mapPanther || type == mapTiger;
}

// Numero de serie mostrado pelo jogo: "#<jogador>-<NN>", contado por DONO e por
// TIPO (o manual mostra "Agridome #1-02", "Cargo Truck #1-03", "Lynx #1-01").
static int vehSerial(const UnitRecord& r, size_t idx) {
    int n = 1;
    for (size_t k = 1; k < idx && k < g_units.size(); ++k) {
        const UnitRecord& o = g_units[k];
        if (o.id == -1) continue;
        if (o.owner == r.owner && o.type == r.type) ++n;
    }
    return n;
}

// Lista de objetivos do painel (modo dword_565C8C = 1, sub_4663A0): uma linha
// por VictoryCondition, "<C0>%s" se cumprida (verde) e "<C2>%s" se não (vermelho).
void GameState::renderPanel(Renderer& renderer) {
    const int VW = Renderer::NATIVE_WIDTH, VH = Renderer::NATIVE_HEIGHT;
    const int px0 = kColX;                       // coluna direita (x)
    uint32_t* fb = renderer.getFramebuffer();
    const int me = TethysGame::localPlayer();

    // Moldura da janela (sub_499E80) e fundo da área cliente (sub_499BC0).
    if (!m_frame.ok()) m_frame.load();
    renderer.fillRect(kViewX + kViewW, kClientY, VW - kViewX - kViewW, VH - kClientY, 0xFF000000);
    renderer.fillRect(kClientX, kStatusY, VW - kClientX, VH - kStatusY, 0xFF000000);
    renderer.fillRect(0, 0, VW, kClientY, 0xFF000000);
    renderer.fillRect(0, 0, kClientX, VH, 0xFF000000);
    m_frame.drawNonClient(fb, VW, VH, VW, VH, true);
    m_frame.drawClient(fb, VW, VH, kClientX, kClientY, kClientW, kClientH, kViewW, kViewH);

    // --- Minimapa (janela 192x128 em vista+8, 0) ---
    // Cores reais do terreno (média de cada tile, sub_48B320) escurecidas por
    // coluna pela faixa de luz (sub_48EC70 + tabelas de sub_586193), como o
    // original. Prédios de todos os jogadores; veículos inimigos só na luz.
    const int mmX0 = kColX, mmY0 = kMiniMapY, mmW = kColW, mmH = kMiniMapH;
    int mw = g_gameMap.ok() ? g_gameMap.width() : 64;
    int mh = g_gameMap.ok() ? g_gameMap.height() : 64;
    // Zoom 0 (sub_48E7F0): escalas independentes [+0]/[+4] = janela / mapa em
    // 16.16 — o mapa estica p/ ocupar a janela inteira. PROVISÓRIO: os zooms
    // 1+ (botões +/−) e o ajuste vertical dos mapas largos não foram portados.
    const int mmX = mmX0, mmY = mmY0;
    auto mapX = [&](int tx) { return mmX + tx * mmW / mw; };      // tile → pixel do minimapa
    auto mapY = [&](int ty) { return mmY + ty * mmH / mh; };
    renderer.fillRect(mmX0, mmY0, mmW, mmH, 0xFF000000);
    const bool shade = g_gameMap.ok() && !TethysGame::daylightEverywhere();
    // Brilho 0..31 da coluna do port (x interna = x + deslocamento).
    auto brightness = [&](int tx) {
        return 0x1F - g_gameMap.lightDarkness(tx + g_gameMap.internalXOffset() - TethysGame::daylightPos());
    };
    // Escurecimento de um canal de 5 bits (tabelas byte_585000/585400/585C00).
    auto shade5 = [](int c5, int b) {
        if (c5 == 31) return 31;
        const int m = std::min(b + 8, 33);
        return (c5 * m) >> 5;
    };
    if (g_gameMap.ok()) {
        for (int ty = 0; ty < mh; ++ty)
            for (int tx = 0; tx < mw; ++tx) {
                uint16_t c = m_terrain.minimapColor(g_gameMap.getTile(LOCATION{tx, ty}));
                int r5 = (c >> 10) & 31, g5 = (c >> 5) & 31, b5 = c & 31;
                if (shade) {
                    const int b = brightness(tx);
                    r5 = shade5(r5, b); g5 = shade5(g5, b); b5 = shade5(b5, b);
                }
                renderer.fillRect(mapX(tx), mapY(ty), mapX(tx + 1) - mapX(tx), mapY(ty + 1) - mapY(ty),
                                  0xFF000000u | (uint32_t(r5 << 3) << 16) | (uint32_t(g5 << 3) << 8) | uint32_t(b5 << 3));
            }
    }
    static const uint32_t kOC[7] = { 0xFF4060FF,0xFFFF5040,0xFF40D070,0xFFFFD040,
                                     0xFFC060FF,0xFF40E0E0,0xFFFF9030 };
    for (int i = 1; i < (int)g_units.size(); ++i) {
        const UnitRecord& r = g_units[i];
        if (r.id == -1 || (r.flags & UNIT_DEAD) || !(r.flags & (UNIT_VEHICLE | UNIT_BUILDING))) continue;
        const int ow = r.owner & 0x0F;
        if ((r.flags & UNIT_VEHICLE) && ow != me && shade) {
            // sub_48CCD0: veículo de quem não é aliado só aparece com a escuridão
            // da coluna < 16 (ou com os faróis acesos, flag 1).
            const bool ally = (g_playerData[me].allyMask >> ow) & 1;
            if (!ally && !(r.flags & 1) &&
                g_gameMap.lightDarkness((r.pixelX >> 5) + g_gameMap.internalXOffset() - TethysGame::daylightPos()) >= 16)
                continue;
        }
        const int tx = r.pixelX >> 5, ty = r.pixelY >> 5;
        renderer.fillRect(mapX(tx), mapY(ty), std::max(1, mapX(tx + 1) - mapX(tx)),
                          std::max(1, mapY(ty + 1) - mapY(ty)), kOC[ow % 7]);
    }
    {
        const int ctx = ((m_camX / 32) % mw + mw) % mw, cty = m_camY / 32;
        const int vx = mapX(ctx), vy = mapY(cty);
        renderer.drawRect(vx, vy, mapX(ctx + viewportW() / 32) - vx, mapY(cty + viewportH() / 32) - vy, 0xFFFFFFFF);
    }
    // Botões do minimapa (sub_458930, posição [+0x310] = 0: topo, centrados):
    // x = W/2 − 32 e +16 por botão. Arte crua {0x9D Zoom In, 0x9C Zoom Out,
    // 0x9E Toggle Tactical View, 0x9A Toggle Globe View (só sem dword_54EBF0)}.
    // PROVISÓRIO: só o desenho (frame 0) — zoom/visões ainda não portados.
    if (g_art.ok()) {
        static const int kMiniBtn[4] = {0x9D, 0x9C, 0x9E, 0x9A};
        const int n = g_gameMap.ok() && g_gameMap.internalXOffset() ? 3 : 4;
        for (int k = 0; k < n; ++k) {
            const int anim = kMiniBtn[k] - 0x65;
            if (anim >= g_art.numAnimations()) continue;
            blitArtFrame(fb, VW, VH, anim, 0, mmX0 + mmW / 2 - 0x20 + 16 * k, mmY0, -1,
                         mmX0, mmY0, mmX0 + mmW, mmY0 + mmH);
        }
    }

    // --- Janela do quadro (dword_5750D8): abas no topo + painel atual ---
    // sub_45C940: 6 abas encostadas, x = 1 + soma das larguras (sub_404D30 =
    // retângulo do header), y = 0, arte da tabela dword_4D5390 {0x8E, 0x91,
    // 0x90, 0x8F, 0x92, 0x8D} (crua; frames 0 normal / 1 atual / 2 desabilitada).
    // O conteúdo começa abaixo delas (sub_45CB00). Fundo (sub_45CCF0): preto +
    // emblema da facção centrado (anim crua 0x99 Eden / 0x540 Plymouth).
    const PaneRect pane = infoPaneRect();
    syncTabToSelection();
    {
        const int winT = kPaneWinY;                         // topo da janela = topo das abas
        const int winL = pane.l, winW = pane.r - pane.l, winH = pane.b - winT;
        renderer.fillRect(winL, winT, winW, winH, 0xFF000000);
        if (g_art.ok()) {
            const bool eden = g_playerData[me < kMaxPlayers ? me : 0].isEden != 0;
            const int emb = (eden ? 0x99 : 0x540) - 0x65;
            if (emb < g_art.numAnimations()) {
                const ArtAnimation& an = g_art.animation(emb);
                const int w = static_cast<int>(an.header[3]) - static_cast<int>(an.header[1]);
                const int h = static_cast<int>(an.header[4]) - static_cast<int>(an.header[2]);
                int ey = (winH >> 1) - (h >> 1);
                if (ey < 0x6E) ey = 0x6E;
                const int ex = (winW >> 1) - (w >> 1) - 1;
                blitArtFrame(fb, VW, VH, emb, 0, winL + ex, winT + ey, -1, winL, winT, pane.r, pane.b);
            }
            static const int kTabArt[6] = {0x8E, 0x91, 0x90, 0x8F, 0x92, 0x8D};   // dword_4D5390
            // Aba atual (sub_40A790 com 0x40) = frame 1; desabilitada = frame 2
            // (sub_45C770: abas 0..3 em missão só de unidades).
            const int current = m_tab;
            int x = 1;
            for (int k = 0; k < 6; ++k) {
                const int anim = kTabArt[k] - 0x65;
                if (anim >= g_art.numAnimations()) continue;
                const ArtAnimation& an = g_art.animation(anim);
                // Botão (sub_40AEE0): sub_403E00 com o hotspot no canto do retângulo.
                const int fr = (k <= 3 && reportsDisabled()) ? 2 : k == current ? 1 : 0;
                blitArtFrame(fb, VW, VH, anim, fr, winL + x, winT, -1,
                             kColX, kPaneWinY, kColX + kColW, pane.t);
                x += static_cast<int>(an.header[3]) - static_cast<int>(an.header[1]);
            }
        }
    }

    // --- Quadro de informação da seleção (classe de painel @off_4D5220) ---
    m_produce.update(m_selected);
    m_storage.update(m_selected);
    m_research.update(m_selected);
    m_students.update(m_selected);
    m_confirm.update(m_selected);
    m_vehicles.update();
    m_comms.update();
    m_factories.update();
    m_labs.update();
    m_resources.update();
    m_space.update();
    if (m_tab == 5 && m_mission)
        m_objectives.draw(fb, VW, VH, pane, m_mouseNX, m_mouseNY);
    else if (m_tab == 2 && m_resources.isOpen())
        m_resources.draw(fb, VW, VH, pane, m_mouseNX, m_mouseNY);
    else if (m_tab == 4 && m_space.isOpen())
        m_space.draw(fb, VW, VH, pane, m_mouseNX, m_mouseNY);
    else if (m_tab == 3 && m_comms.isOpen())
        m_comms.draw(fb, VW, VH, kColX, kPaneWinY, pane, m_mouseNX, m_mouseNY);
    else if (m_tab == 0 && m_factories.isOpen())
        m_factories.draw(fb, VW, VH, pane, m_mouseNX, m_mouseNY);
    else if (m_tab == 1 && m_labs.isOpen())
        m_labs.draw(fb, VW, VH, pane, m_mouseNX, m_mouseNY);
    else if (m_vehicles.isOpen())
        m_vehicles.draw(fb, VW, VH, pane, m_mouseNX, m_mouseNY);
    else if (m_confirm.isOpen())
        m_confirm.draw(fb, VW, VH, pane);
    else if (m_students.isOpen())
        m_students.draw(fb, VW, VH, kColX, kPaneWinY, pane, m_mouseNX, m_mouseNY);
    else if (m_research.isOpen())
        m_research.draw(fb, VW, VH, kColX, kPaneWinY, pane, m_mouseNX, m_mouseNY);
    else if (m_produce.isOpen())
        m_produce.draw(fb, VW, VH, pane, m_mouseNX, m_mouseNY);
    else if (m_storage.isOpen()) {
        // A caixa da doca fica 0x33 acima da fileira de botões mais alta.
        int topBtn = kPaneBottom - 2 - 25;
        for (const TextButton& b : structureButtons()) topBtn = std::min(topBtn, b.t);
        m_storage.draw(fb, VW, VH, kColX, kPaneWinY, pane, topBtn, m_mouseNX, m_mouseNY);
    }
    else if (!m_selected.empty())
        drawUnitInfoPane(fb, VW, VH, pane, m_selected[0], static_cast<int>(m_selected.size()), me);
    // Botoeira do veículo (sub_46D540): ícones verdes do op2_art (anim crua − 0x65).
    {
        const std::vector<CmdButton> btns = commandButtons();
        const CmdButton* hover = nullptr;
        for (const CmdButton& b : btns) {
            const int anim = b.iconRaw - 0x65;
            if (g_art.ok() && anim >= 0 && anim < g_art.numAnimations()) {
                // Botão (sub_40AEE0): sub_403E00 com o hotspot no canto do retângulo.
                blitArtFrame(fb, VW, VH, anim, 0, b.l, b.t, -1, px0, 0, VW, VH);
            }
            const bool over = m_mouseNX >= b.l && m_mouseNX < b.r && m_mouseNY >= b.t && m_mouseNY < b.b;
            if (over) hover = &b;
            if (over || b.cmd == m_targetCmd) renderer.drawRect(b.l - 1, b.t - 1, b.r - b.l + 2, b.b - b.t + 2,
                                                               b.cmd == m_targetCmd ? 0xFFFFFF60 : 0xFFE0E0E0);
        }
        // Dica (sem o '&'). PROVISÓRIO: caixa junto ao cursor — o desenho do
        // gerenciador de dicas do original (dword_5750D8) ainda não foi portado.
        if (hover) {
            std::string t;
            for (char ch : hover->tip) if (ch != '&') t += ch;
            const int w = UiFont::width(t) + 8, h = 14;
            int tx = std::min(m_mouseNX + 4, VW - w - 2), ty = m_mouseNY - h - 4;
            renderer.fillRect(tx, ty, w, h, 0xFF101810);
            renderer.drawRect(tx, ty, w, h, 0xFF40C040);
            UiFont::draw(fb, VW, VH, t, tx + 4, ty + 2, 0xFFE0FFE0, false);
        }
    }
    // Botões de texto do prédio: arte 0x98 (87x25) / 0x97 larga (175x25), frame 0
    // normal e 1 aceso (PROVISÓRIO: aceso com o mouse em cima; cor do texto a confirmar).
    for (const TextButton& b : structureButtons()) {
        const bool over = m_mouseNX >= b.l && m_mouseNX < b.r && m_mouseNY >= b.t && m_mouseNY < b.b;
        const int anim = b.wide ? 0x97 - 0x65 : 0x98 - 0x65;
        if (g_art.ok() && anim < g_art.numAnimations()) {
            // Botão (sub_40AEE0): sub_403E00 com o hotspot no canto do retângulo.
            blitArtFrame(fb, VW, VH, anim, over ? 1 : 0, b.l, b.t, -1, px0, 0, VW, VH);
        }
        std::string t;
        for (char ch : b.label) if (ch != '&') t += ch;
        // Desabilitado (BUILD IT sem item pagável): texto apagado (PROVISÓRIO).
        UiFont::drawCentered(fb, VW, VH, t, (b.l + b.r) / 2, UiFont::centeredY(b.t, b.b),
                               b.disabled ? 0xFF603020 : over ? 0xFFFF7050 : 0xFFE04030, false);
    }

    // --- Barra de status sob a vista (sub_4998D0 / sub_499A30) ---
    // Retângulos na área cliente: linha 1 em y = (H & ~31) + 6, linha 2 12 px
    // abaixo; contadores de minério em x = vista - 0x51 + 8 (ícone 211 comum /
    // 210 raro + número em branco); texto à esquerda até vista - 0x51.
    {
        const int y1 = kClientY + kViewH + 6, y2 = y1 + 12;
        const int oreX = kClientX + kViewW - 0x51 + 8;
        const PlayerData& pd = g_playerData[me < kMaxPlayers ? me : 0];
        char num[16];
        m_frame.drawPiece(fb, VW, VH, 23, oreX, y1 + 2);                 // 211 (comum)
        std::snprintf(num, sizeof(num), "%d", pd.ore);
        UiFont::draw(fb, VW, VH, num, oreX + m_frame.width(23) + 2, y1, 0xFFFFFFFF, false);
        // [+0xA8]: linha do minério raro. PROVISÓRIO: mostrada quando há raro.
        if (pd.rareOre > 0) {
            m_frame.drawPiece(fb, VW, VH, 24, oreX, y2 + 2);             // 210 (raro)
            std::snprintf(num, sizeof(num), "%d", pd.rareOre);
            UiFont::draw(fb, VW, VH, num, oreX + m_frame.width(24) + 2, y2, 0xFFFFFFFF, false);
        }
        // Texto (linha 2): coordenadas do tile sob o mouse e a última mensagem
        // (sub_49BF10) ou a ajuda do modo de mira (unk_575570). PROVISÓRIO: a
        // composição exata dos dois objetos de texto ainda não foi portada.
        int tx0 = kClientX + 2;
        const int mvx = m_mouseNX - kViewX, mvy = m_mouseNY - kViewY;
        if (mvx >= 0 && mvx < viewportW() && mvy >= 0 && mvy < viewportH()) {
            int tx = (m_camX + mvx) / 32, ty = (m_camY + mvy) / 32;
            if (g_gameMap.ok()) tx &= (g_gameMap.width() - 1);
            char c[32]; std::snprintf(c, sizeof(c), "%d,%d", tx + 1, ty + 1);
            UiFont::draw(fb, VW, VH, c, tx0, y1, 0xFFE0E0E0, false);
        }
        const std::string& msg = (m_targetCmd >= 0 && !uiCommandHint().empty()) ? uiCommandHint() : statusBarText();
        std::string clipped = msg;
        while (!clipped.empty() && tx0 + UiFont::width(clipped) > oreX - 8) clipped.pop_back();
        UiFont::draw(fb, VW, VH, clipped, tx0, y2, 0xFFE8E8E8, false);
    }
}

} // namespace op2
