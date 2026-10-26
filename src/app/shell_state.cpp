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
    // Volta do fim de uma partida: a tela depende do motivo (0x73a); senão
    // (ESC no jogo), o menu principal.
    if (m_hasReturn) {
        m_hasReturn = false;
        m_menu.afterGame(m_reason, m_hasResults ? &m_results : nullptr);
        return;
    }
    m_menu.showMainMenu();
}

void ShellState::onIdle(Renderer& renderer, float dt) {
    m_menu.update(dt);

    renderer.clear(0xFF000000);
    m_menu.render(renderer);
    if (m_menu.consumeLoadRequest()) m_loadDlg.open(SaveSlotsDialog::kLoad, "");
    m_loadDlg.render(renderer);
    renderer.present();

    // o menu pediu para iniciar uma missão?
    std::string dllName;
    if (m_menu.consumeLaunch(dllName)) {
        GameStartInfo gsi;
        const bool hasGsi = m_menu.launchStartInfo(gsi);
        if (!gTApp.activateMission(dllName, hasGsi ? &gsi : nullptr))
            std::cerr << "[Shell] nao foi possivel iniciar a missao (" << dllName << ")" << std::endl;
        return;
    }
    // o menu pediu para sair?
    if (m_menu.shouldQuit())
        gTApp.requestQuit();
}

void ShellState::handleEvent(Renderer& renderer, const SDL_Event& e) {
    if (m_loadDlg.active()) {
        if (m_loadDlg.handleEvent(renderer, e) == SaveSlotsDialog::kDoLoad &&
            !gTApp.activateSaved(m_loadDlg.path()))
            std::cerr << "[Shell] nao foi possivel carregar " << m_loadDlg.path() << std::endl;
        return;
    }
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
    case SDL_TEXTINPUT:
        m_menu.handleTextInput(e.text.text);
        break;
    default:
        break;
    }
}

} // namespace op2
