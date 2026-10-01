#include "game_state.h"

#include <algorithm>
#include <iostream>

#include <cstdio>

#include "tapp.h"
#include "../gfx/renderer.h"
#include "../gfx/font.h"
#include "../res/res_manager.h"
#include "../game/tethys_game.h"
#include "../game/player.h"
#include "../game/unit.h"

namespace op2 {

bool GameState::load(const std::string& mapName) {
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

    // Inicia o núcleo de simulação (relógio zerado + RNG semeado). O seed virá da
    // missão/netgame nos próximos slices; por ora um valor fixo p/ determinismo.
    TethysGame::reset(/*seed*/ 0x12345678u, /*numPlayers*/ 1, /*localPlayer*/ 0);
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
    resetUnits();
    int cx = (g_gameMap.clipRect().x1 + g_gameMap.clipRect().x2) / 2;
    int cy = (g_gameMap.clipRect().y1 + g_gameMap.clipRect().y2) / 2;
    int owner = TethysGame::localPlayer();
    Unit u;
    TethysGame::CreateUnit(u, /*prédio*/ kFirstBuilding, LOCATION{cx, cy}, owner);
    TethysGame::CreateUnit(u, /*veículo*/ 1, LOCATION{cx + 2, cy}, owner);
    TethysGame::CreateUnit(u, /*veículo*/ 1, LOCATION{cx - 2, cy + 1}, owner);
    TethysGame::CreateUnit(u, /*veículo*/ 2, LOCATION{cx + 1, cy - 2}, owner);

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

    // Avança o relógio de simulação a uma cadência fixa (~10 ticks/s por ora).
    const float kTickPeriod = 0.1f;
    m_tickAccum += dt;
    while (m_tickAccum >= kTickPeriod) {
        TethysGame::advanceTick();
        m_lastRand = TethysGame::getRand(100);  // 1 consumo determinístico por tick
        m_tickAccum -= kTickPeriod;
    }

    renderer.clear(0xFF000000);
    m_terrain.render(renderer.getFramebuffer(), Renderer::NATIVE_WIDTH,
                     Renderer::NATIVE_HEIGHT, m_camX, m_camY);

    // Unidades: por ora marcadores coloridos por dono (os sprites op2_art entram
    // num slice futuro). Prédio = quadrado maior; veículo = menor.
    static const uint32_t kOwnerColor[7] = {
        0xFF40A0FF, 0xFFFF5040, 0xFF40D070, 0xFFFFD040,
        0xFFC060FF, 0xFF40E0E0, 0xFFFF9030,
    };
    for (size_t i = 1; i < g_units.size(); ++i) {
        Unit un(static_cast<int>(i));
        if (!un.IsLive()) continue;
        LOCATION l = un.Location();
        int sx = l.x * 32 - m_camX;
        int sy = l.y * 32 - m_camY;
        uint32_t col = kOwnerColor[un.OwnerID() % 7];
        int sz = un.IsBuilding() ? 26 : 16;
        int off = (32 - sz) / 2;
        renderer.fillRect(sx + off, sy + off, sz, sz, col);
        renderer.drawRect(sx + off, sy + off, sz, sz, 0xFF101010);
    }

    // Overlay: relógio (Tick/Time) + uma amostra do RNG — prova visível de que o
    // núcleo de simulação está rodando determinístico.
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
    std::snprintf(l3, sizeof(l3), "%s   Units %d   (setas: rolar | ESC: menu)",
                  m_mapName.c_str(), liveUnits);
    Font::drawText(fb, VW, VH, l1, 8, 8, 0xFFFFE060, true);
    Font::drawText(fb, VW, VH, l2, 8, 20, 0xFF80E0A0, true);
    Font::drawText(fb, VW, VH, l3, 8, 32, 0xFFB0C4DE, true);

    renderer.present();
}

void GameState::handleEvent(Renderer& renderer, const SDL_Event& e) {
    (void)renderer;
    // ESC volta ao shell (menu), como o jogo (ActivateShell).
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
        gTApp.activateShell();
}

} // namespace op2
