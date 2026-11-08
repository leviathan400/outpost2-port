#include "../game/player.h"
#include "shell_state.h"

#include <iostream>
#include <algorithm>
#include <vector>
#include <string>

#include "tapp.h"
#include "../gfx/renderer.h"
#include "../game/tethys_game.h"
#include "../net/net_game.h"
#include "../net/net_lobby.h"

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

    // Fork: o shell continua em 640x480 escalado (o jogo muda o quadro para o
    // tamanho da janela; ao voltar ao menu, o quadro volta a 640x480).
    renderer.setLogicalSize(Renderer::NATIVE_WIDTH, Renderer::NATIVE_HEIGHT);
    renderer.clear(0xFF000000);
    m_menu.render(renderer);
    if (m_menu.consumeLoadRequest()) m_loadDlg.open(SaveSlotsDialog::kLoad, "");
    m_loadDlg.render(renderer);
    renderer.present();

    // o lobby pediu a partida em REDE? O socket e o par ja vem negociados do
    // net_lobby (pedido 0x32 / aceitacao 0x26 / confirmacao 0x14), entao a
    // partida comeca direto no lockstep, sem refazer aperto de mao.
    std::string netDll;
    if (m_menu.consumeNetLaunch(netDll)) {
        std::string err;
        // A topologia e a mesma da camada de mensagens do lobby: estrela com o
        // host no meio. O host fala com cada lugar ocupado e REPASSA os turnos
        // dos clientes aos demais; o cliente fala so com o host.
        const bool host = lobbyHostActive();
        std::vector<NetPeer> peers;
        if (host) {
            for (int n = 1; n < 8; ++n) {
                const LobbyPlayer* lp = lobbyHostPlayer(n);
                if (lp && lp->state == 2) peers.push_back(NetPeer{lp->addr, lp->port});
            }
        } else {
            uint32_t a = 0; uint16_t pt = 0;
            if (lobbyPeer(a, pt)) peers.push_back(NetPeer{a, pt});
        }
        if (peers.empty()) {
            std::cerr << "[Shell] o lobby nao sabe onde estao os outros" << std::endl;
            return;
        }
        const int me = host ? 0 : m_menu.mpLocalPlayer();
        const int players = std::max(2, m_menu.mpPlayerCount());
        if (!netStartJoined(lobbyTakeGameSocket(), peers.data(), int(peers.size()), host,
                            me, players, err)) {
            std::cerr << "[Shell] a rede nao subiu: " << err << std::endl;
            return;
        }
        if (lobbyHostActive()) lobbyHostStop();
        GameStartInfo gsi;
        const bool hasGsi = m_menu.launchStartInfo(gsi);
        TethysGame::setLocalPlayer(me);
        // Os nomes do lobby viram os nomes do jogo (sub_490BA0), que e o que a
        // linha de conversa e a tela de aliancas mostram.
        for (int i = 0; i < players && i < kMaxPlayers; ++i) setPlayerName(i, m_menu.mpName(i));
        if (!gTApp.activateMission(netDll, hasGsi ? &gsi : nullptr))
            std::cerr << "[Shell] nao foi possivel iniciar a missao em rede ("
                      << netDll << ")" << std::endl;
        return;
    }

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
