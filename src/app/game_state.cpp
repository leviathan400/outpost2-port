#include "game_state.h"

#include <algorithm>
#include <iostream>

#include "tapp.h"
#include "../gfx/renderer.h"
#include "../res/res_manager.h"

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

    renderer.clear(0xFF000000);
    m_terrain.render(renderer.getFramebuffer(), Renderer::NATIVE_WIDTH,
                     Renderer::NATIVE_HEIGHT, m_camX, m_camY);
    renderer.present();
}

void GameState::handleEvent(Renderer& renderer, const SDL_Event& e) {
    (void)renderer;
    // ESC volta ao shell (menu), como o jogo (ActivateShell).
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
        gTApp.activateShell();
}

} // namespace op2
