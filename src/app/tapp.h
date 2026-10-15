#pragma once

#include <string>

namespace op2 {

struct GameStartInfo;
class Renderer;
class UIState;
class ShellState;
class GameState;

// Aplicação — porta TApp (asm: WinMain @278474, Init @218884, CreateFrames
// @220028, Run @220113, Activate* @220807/@220964, SetUIState @221046).
// Ver [[op2-engine-architecture]]. Backbone fiel adaptado ao SDL2:
//   Init → CreateFrames → Run → ShutDown.
// A UIState ativa fica em m_cur (equivale a TApp+0x44); Run chama handleEvent +
// onIdle dela a cada frame; Activate* trocam o estado (OnDeactivate/OnActivate).
class TApp {
public:
    bool init();          // SDL + renderer + config + ResManager
    // Cria a GameState (uma vez) e ativa o shell (menu). Se initialMap != "",
    // entra direto no jogo com esse mapa (como o branch de load/newgame do
    // CreateFrames original) — usado pelo modo --play.
    bool createFrames(const std::string& initialMap = "", const std::string& initialMission = "");
    void run();           // loop principal (PeekMessage→OnIdle, aqui via SDL)
    void shutdown();

    // Troca de estado (equivale a ActivateShell/ActivateGame/SetUIState).
    void activateShell();
    // TApp::ActivateShell(GameTermReasons, MissionResults*): o shell recebe o
    // motivo do fim e os resultados (CreateShell) e decide a próxima tela (0x73a).
    void activateShell(int reason, const GameStartInfo* results);
    bool activateGame(const std::string& mapName);
    // Inicia uma missão pela DLL (T##/E##/P##.DLL) e ativa o jogo.
    bool activateMission(const std::string& dllName, const GameStartInfo* gsi = nullptr);
    void setUIState(UIState* next);

    void requestQuit() { m_quit = true; }
    bool hasShell() const { return m_shell != nullptr; }   // falso nos testes por linha de comando
    Renderer& renderer() { return *m_renderer; }

private:
    Renderer* m_renderer = nullptr;
    UIState*  m_cur = nullptr;    // estado ativo (TApp+0x44)
    ShellState* m_shell = nullptr;
    GameState*  m_game = nullptr; // singleton do jogo (dword_574A80)
    bool m_quit = false;
    bool m_sdlReady = false;
};

// Instância global, como o gTApp original.
extern TApp gTApp;

} // namespace op2
