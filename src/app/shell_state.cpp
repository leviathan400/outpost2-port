#include "shell_state.h"

#include <iostream>
#include <string>

#include "tapp.h"
#include "../gfx/renderer.h"

namespace op2 {

bool ShellState::init(const std::string& resDll) {
    return m_menu.init(resDll);
}

void ShellState::onActivate() {
    // volta ao menu principal ao reentrar no shell (ex.: ESC no jogo)
    m_menu.showMainMenu();
}

void ShellState::onIdle(Renderer& renderer, float dt) {
    m_menu.update(dt);

    renderer.clear(0xFF000000);
    m_menu.render(renderer);
    renderer.present();

    // o menu pediu para iniciar uma missão?
    std::string mapName;
    if (m_menu.consumeLaunch(mapName)) {
        if (!gTApp.activateGame(mapName))
            std::cerr << "[Shell] nao foi possivel iniciar a missao (" << mapName << ")" << std::endl;
        return;
    }
    // o menu pediu para sair?
    if (m_menu.shouldQuit())
        gTApp.requestQuit();
}

void ShellState::handleEvent(Renderer& renderer, const SDL_Event& e) {
    switch (e.type) {
    case SDL_MOUSEMOTION: {
        int nx, ny;
        renderer.screenToNative(e.motion.x, e.motion.y, nx, ny);
        m_menu.handleMouseMove(nx, ny);
        break;
    }
    case SDL_MOUSEBUTTONDOWN: {
        int nx, ny;
        renderer.screenToNative(e.button.x, e.button.y, nx, ny);
        m_menu.handleMouseDown(nx, ny, e.button.button);
        break;
    }
    case SDL_MOUSEBUTTONUP: {
        int nx, ny;
        renderer.screenToNative(e.button.x, e.button.y, nx, ny);
        m_menu.handleMouseUp(nx, ny, e.button.button);
        break;
    }
    case SDL_MOUSEWHEEL:
        m_menu.handleMouseWheel(e.wheel.y);
        break;
    case SDL_KEYDOWN:
        m_menu.handleKeyDown(e.key.keysym.sym);
        break;
    default:
        break;
    }
}

} // namespace op2
