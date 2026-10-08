#include "game_state.h"

#include <algorithm>
#include <iostream>

#include <cstdio>
#include <cstdlib>

#include "tapp.h"
#include "info_pane.h"
#include "../gfx/renderer.h"
#include "../gfx/font.h"
#include "../res/res_manager.h"
#include "../core/config.h"
#include "../game/tethys_game.h"
#include "../game/player.h"
#include "../game/unit.h"
#include "../game/command.h"
#include "../game/pathfinder.h"
#include "../game/movement.h"
#include "../game/vehicle_anim.h"
#include "../game/structure_anim.h"
#include "../game/structure.h"
#include "../game/building_sheet.h"
#include "../game/colony.h"
#include "../game/messages.h"
#include "../game/weapon.h"
#include "../game/trigger.h"
#include "../game/scenario.h"
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
    m_maxX = m_terrain.mapPixelWidth() - VW;
    m_maxY = m_terrain.mapPixelHeight() - VH;
    if (m_maxY < 0) m_maxY = 0;
    // começa na área jogável (clipRect), como o jogo
    m_camX = g_gameMap.clipRect().x1 * 32;
    m_camY = g_gameMap.clipRect().y1 * 32;
    clampCamera();
    return true;
}

bool GameState::loadMission(const std::string& dllName) {
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

    TethysGame::reset(/*seed*/ 0x12345678u, d->numPlayers, /*localPlayer*/ 0);
    m_tickAccum = 0.0f;
    resetPlayers(d->numPlayers);
    // GameStartInfo do shell p/ jogo single player: o jogador 0 é o humano, os
    // demais computador ([+0x28] isHuman).
    for (int p = 0; p < d->numPlayers; ++p) { if (p == 0) Player[p].GoHuman(); else Player[p].GoAI(); }
    loadBuildingSheet();
    loadVehicleSheet();
    loadWeaponSheet();
    loadMoraleSheet();
    resetMessages();
    resetUnits();
    TethysGame::SetDaylightMoves(0);
    TethysGame::SetDaylightEverywhere(0);
    // sub_402E10: (IA dos jogadores de computador, sub_402E70 — pendente) e InitProc.
    const int ok = m_mission->initProc ? m_mission->initProc() : 1;
    std::cout << "[Mission] InitProc -> " << ok << "; unidades=" << (g_units.size() - 1) << std::endl;
    int cx, cy;
    if (takeCenterViewRequest(cx, cy)) {           // CenterViewOn
        m_camX = cx - viewportW() / 2;
        m_camY = cy - (Renderer::NATIVE_HEIGHT - kStatusBarH) / 2;
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
    loadWeaponSheet();
    loadMoraleSheet();
    resetMessages();
    resetUnits();
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
    m_camX = cx * 32 - Renderer::NATIVE_WIDTH / 2;
    m_camY = cy * 32 - Renderer::NATIVE_HEIGHT / 2;
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

void GameState::onIdle(Renderer& renderer, float dt) {
    const Uint8* ks = SDL_GetKeyboardState(nullptr);
    int speed = static_cast<int>(600 * dt);
    if (speed < 1) speed = 1;
    if (ks[SDL_SCANCODE_LEFT])  m_camX -= speed;
    if (ks[SDL_SCANCODE_RIGHT]) m_camX += speed;
    if (ks[SDL_SCANCODE_UP])    m_camY -= speed;
    if (ks[SDL_SCANCODE_DOWN])  m_camY += speed;
    clampCamera();

    // Avança o relógio de simulação na cadência do GameSpeed (m_tickPeriod).
    const float kTickPeriod = m_tickPeriod;
    m_tickAccum += dt;
    while (m_tickAccum >= kTickPeriod) {
        TethysGame::advanceTick();
        TethysGame::advanceDaylight(g_gameMap.internalWidth());   // início do turno (sub_489DD0)
        m_lastRand = TethysGame::getRand(100);  // 1 consumo determinístico por tick
        tickColony();                            // economia de cada jogador (sub_471F30)
        missionTriggerTick(currentMission());    // triggers, AIProc, vitória/derrota (sub_403110)
        tickMovement();                          // move as unidades pela rota do pathfinder
        tickUnits();                             // predios + efeitos (sub_43AC30)
        m_tickAccum -= kTickPeriod;
        // Fim de jogo (TethysGame +0x98): chegado o tick, a partida acaba e
        // volta ao shell.
        if (gameEndCode() && TethysGame::tick() >= gameEndTick()) {
            setCurrentMission(nullptr);
            m_mission.reset();
            resetTriggers();
            gTApp.activateShell();
            return;
        }
    }
    {   // CenterViewOn pedido pelo script.
        int cx, cy;
        if (takeCenterViewRequest(cx, cy)) {
            m_camX = cx - viewportW() / 2;
            m_camY = cy - (Renderer::NATIVE_HEIGHT - kStatusBarH) / 2;
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
            if (!r.hasOrder || r.action != CMD_MOVE || r.order.waypoints.empty()) continue;
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
                            0, 0, viewportW(), Renderer::NATIVE_HEIGHT - kStatusBarH);
    };

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
        // Centro da unidade na tela (pixelX/Y é o canto do tile; +16 = centro).
        // Usa a posição INTERPOLADA (não o tile), p/ casar com o deslize.
        int ucx = screenXOf(r.pixelX) + 16;   // wrap horizontal do mapa
        int ucy = r.pixelY + 16 - m_camY;

        bool drew = false;
        if ((r.flags & UNIT_VEHICLE) && hasVehicleAnim(r.type)) {
            int ow = r.owner & 0x0F;
            bool eden = (ow >= 0 && ow < kMaxPlayers) ? (g_playerData[ow].isEden != 0) : true;
            int anim = vehicleAnimIndex(r.type, r.rotation, eden);   // resolver fiel (facção)
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
                int pc = (ow >= 0 && ow < kMaxPlayers) ? g_playerData[ow].colorNumber : 0;
                drew = drawSprite(anim, frame, ucx, ucy, pc);
                // Torre (sub_484CF0): anim da arma p/ a direção da torre [+0x2D],
                // frame 0, no ponto de encaixe do frame atual do chassi.
                int tAnim, tx, ty;
                if (drew && tankTurret(r, anim, frame, tAnim, tx, ty) && tAnim >= 0 &&
                    tAnim < g_art.numAnimations())
                    drawSprite(tAnim, 0, ucx + tx, ucy + ty, pc);
            }
        }
        if (isBld) {
            // Estrutura -- porta sub_408BE0: anim = tabela[estado] (sub_4835A0/
            // sub_483720), frame = sub_409220. Estado 0 com contador 0 = ultimo
            // frame (predio pronto); construcao, dano (1/2) e destruicao (3) vem
            // da maquina de estados (src/game/structure.cpp).
            int ow = r.owner & 0x0F;
            int anim = structureCurAnim(r);
            if (anim >= 0 && g_art.ok() && anim < g_art.numAnimations()) {
                int pc = (ow >= 0 && ow < kMaxPlayers) ? g_playerData[ow].colorNumber : 0;
                drew = drawSprite(anim, structureFrame(r), ucx, ucy, pc);
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
        if (isEffect && isWeaponProjectile(r.type)) {
            // Projétil/explosão de arma (vt[6]: sub_4A3420 / sub_4A3000 /
            // sub_4A2A00 / sub_4A2D50) em coordenadas de mundo do original.
            std::vector<WeaponDrawOp> ops;
            weaponDrawOps((int)i, ops);
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
        // Acento de cor do dono (barrinha acima da unidade) — distingue jogador.
        if (drew)
            renderer.fillRect(ucx - 6, ucy - 18, 12, 2, kOwnerColor[(r.owner & 0x0F) % 7]);
    }

    // Caixas de seleção ao redor das unidades selecionadas.
    for (int id : m_selected) {
        if (id <= 0 || id >= (int)g_units.size()) continue;
        const UnitRecord& r = g_units[id];
        if (r.id == -1 || (r.flags & UNIT_DEAD)) continue;
        // Centrada na posição INTERPOLADA (casa com o sprite durante o deslize).
        int ucx = screenXOf(r.pixelX) + 16, ucy = r.pixelY + 16 - m_camY;
        if (ucx < -16 || ucx > viewportW() + 16 || ucy < -16 || ucy > Renderer::NATIVE_HEIGHT + 16)
            continue;
        renderer.drawRect(ucx - 15, ucy - 15, 30, 30, 0xFF40FF40);   // verde (selecionado)
    }
    // Caixa de arrasto (multi-seleção).
    if (m_dragging) {
        int x0 = std::min(m_dragSX, m_mouseNX), y0 = std::min(m_dragSY, m_mouseNY);
        int w = std::abs(m_mouseNX - m_dragSX), h = std::abs(m_mouseNY - m_dragSY);
        renderer.drawRect(x0, y0, w, h, 0xFF40FF40);
    }

    // Painel de comando (a "MAINE"): minimapa + info + botões.
    // Fim de jogo (sub_49CAC0): letreiro sobre a vista até o tick de encerramento.
    // (O original usa a fonte grande do quadro de detalhe — aqui a fonte do port.)
    if (gameEndCode()) {
        const int cy = (Renderer::NATIVE_HEIGHT - kStatusBarH) / 2;
        Font::drawTextCentered(renderer.getFramebuffer(), Renderer::NATIVE_WIDTH, Renderer::NATIVE_HEIGHT,
                               gameEndText(gameEndCode()), viewportW() / 2, cy, 0xFFFFE070, true);
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

    renderer.present();
}

int GameState::viewportW() const { return Renderer::NATIVE_WIDTH - kPanelW; }

// Feixes das armas instantâneas: Bresenham com bloco 2x2 por ponto, como
// sub_41E8F0 (laser: OR 0x7C00 = vermelho máximo em RGB555) e loc_41ECE0
// (microwave: padrão 0xFE1FC3F8 girado pela fase; cor 0xD69FD69E que perde
// 0x30003 a cada ponto aceso e volta ao início a cada falha do padrão).
void GameState::drawBeam(Renderer& renderer, const WeaponDrawOp& op) {
    uint32_t* fb = renderer.getFramebuffer();
    const int VW = Renderer::NATIVE_WIDTH, clipW = viewportW(), clipH = Renderer::NATIVE_HEIGHT - kStatusBarH;
    auto c555 = [](uint32_t w) {
        const uint32_t r = (w >> 10) & 31, g = (w >> 5) & 31, b = w & 31;
        return 0xFF000000u | (((r << 3) | (r >> 2)) << 16) | (((g << 3) | (g >> 2)) << 8) | ((b << 3) | (b >> 2));
    };
    auto put = [&](int x, int y, uint32_t argb, bool orRed) {
        if (x < 0 || x >= clipW || y < 0 || y >= clipH) return;
        uint32_t& px = fb[y * VW + x];
        px = orRed ? (px | 0x00FF0000u) : argb;
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
        int ucx = screenXOf(r.pixelX) + 16, ucy = r.pixelY + 16 - m_camY;
        int dx = ucx - nx, dy = ucy - ny, d2 = dx * dx + dy * dy;
        if (d2 < bestD2) { bestD2 = d2; best = i; }
    }
    if (best) m_selected.push_back(best);
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
        if (ucx >= bx0 && ucx <= bx1 && ucy >= by0 && ucy <= by1)
            m_selected.push_back(i);
    }
}

void GameState::handleEvent(Renderer& renderer, const SDL_Event& e) {
    if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_ESCAPE) gTApp.activateShell();
        if (e.key.keysym.sym == SDLK_g) m_showPassability = !m_showPassability;
        if (e.key.keysym.sym == SDLK_F3) m_showDebugHud = !m_showDebugHud;
        // DEPURACAO dos estados de predio (ainda nao ha armas/ConVec construindo):
        //   D = +10% do HP max de dano   H = repara (dano 0)
        //   K = Unit::DoDeath (Kill(1))  B = reinicia a construcao (sub_43DE50)
        //   O = poe o predio OPERANDO (sub_483810 — o que a economia faria)
        const SDL_Keycode k = e.key.keysym.sym;
        if (k == SDLK_d || k == SDLK_h || k == SDLK_k || k == SDLK_b || k == SDLK_o) {
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
        if (e.key.keysym.sym == SDLK_s) {   // Stop: cancela a ordem dos selecionados
            for (int id : m_selected) {
                if (id > 0 && id < (int)g_units.size()) {
                    UnitRecord& r = g_units[id];
                    r.field20 = 0; r.action = 0; r.hasOrder = false;
                    r.order = OrderBlock{}; r.routeDirs.clear();   // o passo em curso termina
                }
            }
        }
    }
    if (e.type == SDL_MOUSEMOTION) {
        renderer.screenToNative(e.motion.x, e.motion.y, m_mouseNX, m_mouseNY);
    }
    if (e.type == SDL_MOUSEBUTTONDOWN) {
        int nx, ny; renderer.screenToNative(e.button.x, e.button.y, nx, ny);
        m_mouseNX = nx; m_mouseNY = ny;
        bool inView = nx < viewportW() && ny < Renderer::NATIVE_HEIGHT - kStatusBarH;
        // Botão de comando do quadro (IDLE / ACTIVATE → CommandPacket 0x12 / 0x13).
        if (e.button.button == SDL_BUTTON_LEFT && !inView && m_selected.size() == 1) {
            const PaneRect pane{viewportW() + 4, kMiniMapH + kTabsH,
                                Renderer::NATIVE_WIDTH - 4, Renderer::NATIVE_HEIGHT - 2};
            PaneButton b;
            if (infoPaneButton(pane, m_selected[0], TethysGame::localPlayer(), b) &&
                nx >= b.rc.l && nx < b.rc.r && ny >= b.rc.t && ny < b.rc.b) {
                if (b.command == 0x12) structureDoIdle(m_selected[0]);
                else structureDoActivate(m_selected[0]);
            }
        }
        if (e.button.button == SDL_BUTTON_LEFT && inView) {
            m_dragging = true; m_dragSX = nx; m_dragSY = ny;
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
            if (!pkt.unitIds.empty()) processCommandPacket(me, pkt);
        }
    }
    if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT && m_dragging) {
        int nx, ny; renderer.screenToNative(e.button.x, e.button.y, nx, ny);
        int dx = std::abs(nx - m_dragSX), dy = std::abs(ny - m_dragSY);
        if (dx < 4 && dy < 4) selectAt(nx, ny, false);   // clique = seleção simples
        else selectBox(m_dragSX, m_dragSY, nx, ny);        // caixa = multi-seleção
        m_dragging = false;
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
void GameState::drawObjectives(uint32_t* fb, int fbW, int fbH, int l, int t, int r, int b) {
    int y = t + 4;
    const int maxW = r - l - 8;
    for (const std::string& line : missionObjectives()) {
        // "<Cn>" + texto; quebra por palavras na largura do painel.
        const std::string tag = line.substr(0, 4), text = line.substr(4);
        std::string cur, word;
        auto flush = [&]() {
            if (cur.empty() || y >= b) return;
            y += drawMarkupText(fb, fbW, fbH, PaneRect{l + 4, y, r - 4, b}, tag + cur, 0xFF30D030);
            cur.clear();
        };
        for (size_t i = 0; i <= text.size(); ++i) {
            if (i == text.size() || text[i] == ' ') {
                const std::string cand = cur.empty() ? word : cur + " " + word;
                if (!cur.empty() && Font::getTextWidth(cand) > maxW) { flush(); cur = word; }
                else cur = cand;
                word.clear();
            } else {
                word += text[i];
            }
        }
        flush();
        y += 4;
    }
}

void GameState::renderPanel(Renderer& renderer) {
    const int VW = Renderer::NATIVE_WIDTH, VH = Renderer::NATIVE_HEIGHT;
    const int px0 = viewportW();                 // início do painel (x)
    uint32_t* fb = renderer.getFramebuffer();
    const int me = TethysGame::localPlayer();

    // Fundo do painel + bordas (moldura clara/escura, como o original).
    renderer.fillRect(px0, 0, kPanelW, VH, 0xFF000000);
    for (int y = 0; y < VH; ++y) {
        fb[y * VW + px0] = 0xFFDCDCE0;
        fb[y * VW + px0 + 1] = 0xFF5C5C5F;
        fb[y * VW + px0 + 3] = 0xFFB9B9BB;
    }

    // --- Minimapa (y 0..kMiniMapH) ---
    // Cores reais do terreno (média de cada tile, sub_48B320) escurecidas por
    // coluna pela faixa de luz (sub_48EC70 + tabelas de sub_586193), como o
    // original. Prédios de todos os jogadores; veículos inimigos só na luz.
    const int mmX0 = px0 + 6, mmY0 = 2, mmW = kPanelW - 12, mmH = kMiniMapH - 4;
    int mw = g_gameMap.ok() ? g_gameMap.width() : 64;
    int mh = g_gameMap.ok() ? g_gameMap.height() : 64;
    int sc = std::max(1, std::min(mmW / mw, mmH / mh));
    int mmW2 = mw * sc, mmH2 = mh * sc;
    int mmX = mmX0 + (mmW - mmW2) / 2, mmY = mmY0 + (mmH - mmH2) / 2;
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
                renderer.fillRect(mmX + tx * sc, mmY + ty * sc, sc, sc,
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
        int mx = mmX + (r.pixelX >> 5) * sc, my = mmY + (r.pixelY >> 5) * sc;
        renderer.fillRect(mx, my, sc, sc, kOC[ow % 7]);
    }
    {
        int vx = mmX + (m_camX / 32) * sc, vy = mmY + (m_camY / 32) * sc;
        int vw = (viewportW() / 32) * sc, vh = ((VH - kStatusBarH) / 32) * sc;
        renderer.drawRect(vx, vy, vw, vh, 0xFFFFFFFF);
    }
    for (int x = px0; x < VW; ++x) fb[(kMiniMapH - 1) * VW + x] = 0xFFFFFFFC;

    // --- Linha de abas (6 botões; os ícones originais entram com a UI de comandos) ---
    {
        const int ty = kMiniMapH + 4, th = kTabsH - 10, tw = 32, gap = 2;
        const int total = 6 * tw + 5 * gap;
        const int tx0 = px0 + (kPanelW - total) / 2;
        for (int k = 0; k < 6; ++k) {
            int bx = tx0 + k * (tw + gap);
            renderer.fillRect(bx, ty, tw, th, 0xFF3A3A3D);
            renderer.drawRect(bx, ty, tw, th, 0xFFC2C2C6);
            char n[4]; std::snprintf(n, sizeof(n), "%d", k + 1);
            Font::drawTextCentered(fb, VW, VH, n, bx + tw / 2, ty + th / 2 - 4, 0xFF9A9A9E, false);
        }
        for (int x = px0; x < VW; ++x) fb[(kMiniMapH + kTabsH - 1) * VW + x] = 0xFFDCDCE0;
    }

    // --- Quadro de informação da seleção (classe de painel @off_4D5220) ---
    const PaneRect pane{px0 + 4, kMiniMapH + kTabsH, VW - 4, VH - 2};
    renderer.fillRect(pane.l, pane.t, pane.r - pane.l, pane.b - pane.t, 0xFF000A00);
    if (!m_selected.empty())
        drawUnitInfoPane(fb, VW, VH, pane, m_selected[0], static_cast<int>(m_selected.size()), me);
    else if (m_mission)
        drawObjectives(fb, VW, VH, pane.l, pane.t, pane.r, pane.b);
    if (m_selected.size() == 1) {
        PaneButton b;
        if (infoPaneButton(pane, m_selected[0], me, b)) {
            renderer.fillRect(b.rc.l, b.rc.t, b.rc.r - b.rc.l, b.rc.b - b.rc.t, 0xFF0D0F09);
            renderer.drawRect(b.rc.l, b.rc.t, b.rc.r - b.rc.l, b.rc.b - b.rc.t, 0xFF70281F);
            Font::drawTextCentered(fb, VW, VH, b.label, (b.rc.l + b.rc.r) / 2,
                                   b.rc.t + (b.rc.b - b.rc.t) / 2 - 4, 0xFFE04030, false);
        }
    }

    // --- Barra de status sob a visão do mapa ---
    {
        const int sy = VH - kStatusBarH, sw = viewportW();
        renderer.fillRect(0, sy, sw, kStatusBarH, 0xFF141416);
        for (int x = 0; x < sw; ++x) { fb[sy * VW + x] = 0xFFFFFFFF; fb[(sy + 1) * VW + x] = 0xFF87868A; }
        // Coordenadas do tile sob o mouse. PROVISÓRIO: exibidas 1-based no
        // sistema do port — o deslocamento exato que o original aplica (o engine
        // guarda mapas pequenos com x+32) ainda não foi rastreado.
        if (m_mouseNX < viewportW() && m_mouseNY < sy) {
            int tx = (m_camX + m_mouseNX) / 32, ty = (m_camY + m_mouseNY) / 32;
            if (g_gameMap.ok()) tx &= (g_gameMap.width() - 1);
            char c[32]; std::snprintf(c, sizeof(c), "%d,%d", tx + 1, ty + 1);
            Font::drawText(fb, VW, VH, c, 4, sy + 2, 0xFFE0E0E0, false);
        }
        // Última mensagem (sub_49BF10: maiúsculas).
        Font::drawText(fb, VW, VH, statusBarText(), 4, sy + 13, 0xFFE8E8E8, false);
    }
}

} // namespace op2
