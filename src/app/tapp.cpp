#include "tapp.h"

#include <SDL.h>
#include <iostream>

#include "ui_state.h"
#include "shell_state.h"
#include "game_state.h"
#include "../gfx/renderer.h"
#include "../gfx/artfile.h"
#include "../core/config.h"
#include "../res/res_manager.h"

namespace op2 {

TApp gTApp;

bool TApp::init() {
    // Config do jogo (OUTPOST2.INI), como o executável original.
    Config config("OUTPOST2.INI");
    GameSettings settings;
    if (config.load()) {
        settings.loadFrom(config);
        std::cout << "[Config] OUTPOST2.INI carregado (jogador='" << settings.name
                  << "', GameSpeed=" << settings.gameSpeed << ")" << std::endl;
    } else {
        std::cout << "[Config] OUTPOST2.INI nao encontrado; usando defaults." << std::endl;
    }

    // Gateway de assets (g_ResManager).
    g_ResManager.init(".");

    // Carrega os sprites op2_art (como TApp::Init -> sub_404360("op2_art")).
    if (!g_art.load())
        std::cerr << "[TApp] aviso: op2_art nao carregou (sprites indisponiveis)." << std::endl;
    else
        std::cout << "[TApp] op2_art: " << g_art.numImages() << " imagens, "
                  << g_art.numAnimations() << " animacoes." << std::endl;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init falhou: " << SDL_GetError() << std::endl;
        return false;
    }
    m_sdlReady = true;

    m_renderer = new Renderer();
    if (!m_renderer->init("Outpost 2: Divided Destiny", 1280, 960, false)) {
        std::cerr << "Renderer init falhou." << std::endl;
        return false;
    }
    return true;
}

bool TApp::createFrames(const std::string& initialMap) {
    // Cria a UIState do jogo uma vez (como dword_574A80 em CreateFrames)...
    m_game = new GameState();
    // ...e a UIState do shell.
    m_shell = new ShellState();
    if (!m_shell->init("OP2SHRES.DLL"))
        std::cerr << "Aviso: OP2SHRES.DLL nao carregou direito." << std::endl;

    // Entra direto no jogo se um mapa foi pedido (branch de newgame/load do
    // CreateFrames original); senão, ativa o shell (ActivateShell).
    if (!initialMap.empty() && activateGame(initialMap))
        return true;
    activateShell();
    return true;
}

void TApp::setUIState(UIState* next) {
    if (next == m_cur) return;
    if (m_cur) m_cur->onDeactivate();
    m_cur = next;
    if (m_cur) m_cur->onActivate();
}

void TApp::activateShell() {
    setUIState(m_shell);
}

bool TApp::activateGame(const std::string& mapName) {
    if (!m_game->load(mapName)) return false;   // mapa invalido: fica no shell
    setUIState(m_game);
    return true;
}

void TApp::run() {
    uint32_t last = SDL_GetTicks();
    while (!m_quit) {
        uint32_t now = SDL_GetTicks();
        float dt = (now - last) / 1000.0f;
        last = now;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { m_quit = true; break; }
            if (m_cur) m_cur->handleEvent(*m_renderer, e);
        }
        if (m_quit) break;

        if (m_cur) m_cur->onIdle(*m_renderer, dt);

        uint32_t frame = SDL_GetTicks() - now;
        if (frame < 16) SDL_Delay(16 - frame);
    }
}

void TApp::shutdown() {
    delete m_game;  m_game = nullptr;
    delete m_shell; m_shell = nullptr;
    m_cur = nullptr;
    if (m_renderer) { m_renderer->shutdown(); delete m_renderer; m_renderer = nullptr; }
    if (m_sdlReady) { SDL_Quit(); m_sdlReady = false; }
    std::cout << "[TApp] encerrado." << std::endl;
}

} // namespace op2
