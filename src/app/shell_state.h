#pragma once

#include "ui_state.h"
#include "../shell/shell_menu.h"
#include "save_dialog.h"

namespace op2 {

// UIState do shell (menu principal) — no original vem da OP2Shell.dll via
// ActivateShell. Aqui embrulha o nosso ShellMenu já existente. A cada frame
// verifica se o menu pediu para iniciar uma missão (consumeLaunch) e, se sim,
// manda o TApp trocar para o jogo (activateGame).
class ShellState : public UIState {
public:
    bool init(const std::string& resDll = "OP2SHRES.DLL");

    void onActivate() override;
    void onIdle(Renderer& renderer, float dt) override;
    void handleEvent(Renderer& renderer, const SDL_Event& e) override;

    ShellMenu& menu() { return m_menu; }
    // Volta do jogo com motivo e resultados (consumido no próximo onActivate).
    void setReturn(int reason, const GameStartInfo* results) {
        m_hasReturn = true;
        m_reason = reason;
        m_hasResults = results != nullptr;
        if (results) m_results = *results;
    }

private:
    ShellMenu m_menu;
    SaveSlotsDialog m_loadDlg;      // LOAD GAME (Dialog 241)
    bool m_hasReturn = false, m_hasResults = false;
    int m_reason = 0;
    GameStartInfo m_results;
};

} // namespace op2
