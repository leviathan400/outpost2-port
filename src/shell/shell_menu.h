#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "../gfx/renderer.h"
#include "../gfx/bitmap.h"
#include "../res/pe_resource.h"
#include "../game/start_info.h"
#include "../net/net_lobby.h"
#include "../net/net_game.h"
#include "rtf_text.h"
#include "scores.h"

namespace op2 {

enum class MenuState {
    Splash,        // tela de abertura com o logo (antes do menu)
    MainMenu,
    NewCampaign,
    ColonyGames,
    Tutorials,
    Preferences,
    About,
    QuitConfirm,
    MissionMenu,   // Dialog 219 (mensagem 0x73b da janela do shell)
    RtfView,       // Dialog 216 + RICHEDIT: briefing 0x73d, objetivos 0x73c, história 0x737
    Chapters,      // Dialog 215 (0x735): capítulos do ch_title.txt
    EnterName,     // Dialog 224 (0x73e): nome do vencedor de um jogo de colônia
    Scores,        // Dialog 223: COLONY GAME SCORES (botão SCORES do Dialog 222)
    // --- multijogador: as telas vêm do OUT2RES.DLL, não do shell, porque no
    // original o botão MULTIPLAYER chama TApp::DoNetGame no executável.
    MpConnection,  // Dialog 259 "PICK CONNECTION METHOD" (sub_411E30)
    MpNetOptions,  // Dialog 260 "NET GAME OPTIONS" CREATE/JOIN (sub_497610)
    MpNewGame,     // Dialog 2012 "NEW NETWORK GAME" (criar, sub_40DEC0)
    MpFindSession, // Dialog 2010 "FIND SESSION" broadcast ou endereço
    MpPickSession, // Dialog 195 "PICK SESSION" (a lista que a descoberta trouxe)
    MpJoinName,    // Dialog 2013 "JOIN A MULTIPLAYER GAME" (nome)
    MpGameSettings // Dialog 196 "GAME SETTINGS" (o lobby da partida)
};

enum class CtrlKind { Button, Radio, Check, List, Combo, Edit };

// Controle genérico de UI (botão, radio, checkbox ou listbox). Cada tela monta
// sua lista de controles; hover (mouse) e foco (teclado, Tab/Shift-Tab) são
// destacados. Ativação segue o padrão Windows: clique/Espaço no focado, Enter no
// botão default, Espaço alterna checkbox, setas movem radio/listbox.
struct Control {
    int id = 0;
    CtrlKind kind = CtrlKind::Button;
    std::string text;
    int x = 0, y = 0, w = 0, h = 0;
    bool disabled = false;
    bool isDefault = false;   // botão default do diálogo (aciona com Enter)
    bool hovered = false;
    bool pressed = false;
    // Vínculos de estado:
    int*  radioTarget = nullptr;  int radioValue = 0;   // Radio: seta *target=value
    bool* checkTarget = nullptr;                          // Check: alterna *target
    int*  listSel = nullptr;      int listCount = 0;      // List: índice selecionado
    // Combo: as opções e o índice escolhido (um clique passa para a seguinte,
    // como um ciclo — o original abre a lista suspensa do Windows).
    const std::vector<std::string>* comboItems = nullptr;
    int*  comboSel = nullptr;
    std::string* editTarget = nullptr;   // Edit: o texto que recebe digitação
    size_t editMax = 0;                  // limite de caracteres (0 = sem limite)
};

// As opcoes dos combos do GAME SETTINGS, de Format[] no executavel: as cores
// vem dos indices 0x28B..0x290 (tabela unk_4D5470) e os recursos de
// 0x1CF..0x1D1 (unk_4D5460). Definidas em shell_menu_mp.cpp.
extern const std::vector<std::string> kColorNames;
extern const std::vector<std::string> kResourceNames;
extern const std::vector<std::string> kMaxPlayerNames;
extern const std::vector<std::string> kVehicleNames;

class ShellMenu {
public:
    ShellMenu();
    ~ShellMenu();

    bool init(const std::string& resDllPath = "OP2SHRES.DLL");

    void handleMouseMove(int nativeX, int nativeY);
    void handleMouseDown(int nativeX, int nativeY, uint8_t button);
    void handleMouseUp(int nativeX, int nativeY, uint8_t button);
    void handleKeyDown(int keyCode);
    void handleTextInput(const char* text);   // Edit 1093 do "ENTER YOUR NAME"
    void handleMouseWheel(int delta);

    void update(float dt);
    void render(Renderer& renderer);

    bool shouldQuit() const { return m_quitRequested; }

    // O menu pediu para iniciar uma missão? Se sim, devolve o nome do .map (do
    // export MapName da DLL da missão) e limpa o pedido. Consumido pelo ShellState
    // para o TApp trocar para o jogo (ActivateGame). Ver [[op2-engine-architecture]].
    bool consumeLaunch(std::string& outMap);
    // LOAD GAME (botão 1007) pediu o diálogo de slots?
    bool consumeLoadRequest() { const bool r = m_loadRequested; m_loadRequested = false; return r; }
    // GameStartInfo do último pedido (campanha); false se o pedido não tem.
    bool launchStartInfo(GameStartInfo& out) const { if (m_launchHasGsi) out = m_launchGsi; return m_launchHasGsi; }
    // Volta do jogo (CreateShell + mensagem 0x73a): motivo do fim e resultados.
    void afterGame(int reason, const GameStartInfo* results);

    // O START do GAME SETTINGS pediu a partida em rede? Devolve a DLL do
    // cenário; o jogador local e os pares saem do net_lobby.
    bool consumeNetLaunch(std::string& outDll) {
        if (!m_netLaunch) return false;
        m_netLaunch = false;
        outDll = m_netLaunchDll;
        return true;
    }

    // Abre diretamente uma sub-tela (usado pelo modo --screenshot para captura).
    void showTutorials() { setState(MenuState::Tutorials); }
    void showMainMenu() { setState(MenuState::MainMenu); }
    void showNewCampaign() { setState(MenuState::NewCampaign); }
    void showPreferences() { setState(MenuState::Preferences); }
    void showColonyGames() { setState(MenuState::ColonyGames); }
    void showAbout() { setState(MenuState::About); }
    void showScores() { openScores(); }
    // Captura (--screenshot): abre uma tela de multijogador sem passar pela
    // rede. `host` decide quais controles ficam ativos no GAME SETTINGS.
    void showMpScreen(int which, bool host, int players);
    // Teste por linha de comando (--mpui): percorre o fluxo das telas como se
    // os botoes tivessem sido clicados, e devolve o relato de cada passo.
    bool mpFlowTest(bool host, const std::string& name, std::string& report);
    void showEnterName(const char* game) { m_lastColonyDesc = game ? game : ""; m_scoreMarks = 1234; m_scoreName = "COMMANDER"; setState(MenuState::EnterName); }
    // Depuração (--screenshot): campanha na missão n (concluídas), tela 0 menu,
    // 1 briefing, 2 objetivos, 3 capítulos, 4 história (capítulo n+1).
    void debugCampaign(bool eden, int difficulty, int missionNumber, int screen);

private:
    MenuState m_state = MenuState::Splash;
    bool m_quitRequested = false;
    float m_splashTimer = 0.0f;     // tempo na tela de splash

    // Pedido de início de missão (START/OK): nome do .map resolvido via MapName.
    bool m_launchRequested = false;
    std::string m_launchMap;
    bool m_launchHasGsi = false;
    bool m_loadRequested = false;
    GameStartInfo m_launchGsi;
    bool m_netLaunch = false;
    std::string m_netLaunchDll;

    // Campanha: a cópia do shell (0x13015a34) e as telas da campanha.
    GameStartInfo m_gsi;
    int m_rtfKind = 0;             // 0 briefing, 1 objetivos, 2 história
    int m_storyChapter = 1;        // 0x13015e14 (1 = primeiro)
    std::string m_rtfTitle;
    std::vector<RtfLine> m_rtfLines;
    int m_rtfScroll = 0;
    std::vector<std::string> m_chapters;
    int m_selChapter = 0;

    // Original art resources from OP2SHRES.DLL
    Bitmap m_bgBitmap;     // Bitmap 190 (640x480 space/planet art)
    Bitmap m_logoBitmap;   // Bitmap 145 (544x122 Outpost 2 logo)
    PeDialogTemplate m_mainMenuDlg; // Dialog 137 template

    // Controles da tela atual + foco de teclado
    std::vector<Control> m_controls;
    int m_focusIndex = -1;          // controle com foco (Tab); -1 = nenhum

    // Sub-dialog states
    int m_campaignColony = 0;     // 0 = Eden, 1 = Plymouth
    int m_campaignDifficulty = 1; // 0 = Easy, 1 = Normal, 2 = Hard
    int m_selectedColonyGame = 0;
    int m_selectedTutorial = 0;
    int m_tutorialScroll = 0;      // índice do 1º item visível (scroll do listbox)

    // Estado dos checkboxes de Preferences (antes eram fixos)
    bool m_prefSound = true, m_prefMusic = true, m_prefAnimations = true;
    bool m_prefUnitShadows = false, m_prefStructShadows = false;

    std::vector<std::string> m_colonyGameList;    // LevelDesc das c*.dll
    std::vector<std::string> m_colonyGameDlls;    // arquivo correspondente
    std::vector<std::string> m_tutorialList;

    // Placar dos jogos de colônia (Dialogs 223/224).
    std::vector<ScoreEntry> m_scores;
    int         m_scoreScroll = 0;
    std::string m_scoreName;                      // Edit 1093 do "ENTER YOUR NAME"
    bool        m_clearConfirm = false;           // "Delete All Scores?"
    std::string m_lastColonyDesc;                 // jogo de colônia em andamento
    int         m_scoreMarks = 0;                 // marcas (tick/100) ao vencer

    // --- sistema de controles/foco ---
    void setState(MenuState s);     // troca de tela e reconstrói os controles
    void rebuildControls();         // monta m_controls conforme m_state
    int  controlAt(int x, int y) const;   // índice do controle sob o cursor
    void moveFocus(int dir);        // Tab (+1) / Shift-Tab (-1)
    void activateControl(int index);      // aciona clique/Espaço
    void dispatchButton(int id);    // ação de cada botão por (estado, id)
    void drawControls(Renderer& renderer);
    Control& addButton(int id, const std::string& text, int x, int y, int w, int h,
                       bool isDefault = false, bool disabled = false);
    Control& addCheck(int id, const std::string& text, int x, int y, int w, bool* tgt);
    Control& addRadio(int id, const std::string& text, int x, int y, int w,
                      int* tgt, int value);
    Control& addCombo(int id, const std::vector<std::string>* items, int* sel,
                      int x, int y, int w, int h);
    Control& addEdit(int id, std::string* tgt, size_t maxLen, int x, int y, int w, int h);

    void renderSplash(Renderer& renderer);
    void renderMainMenu(Renderer& renderer);
    void renderNewCampaignDialog(Renderer& renderer);
    void renderAboutDialog(Renderer& renderer);
    void renderPreferencesDialog(Renderer& renderer);
    void renderColonyGamesDialog(Renderer& renderer);
    void renderTutorialsDialog(Renderer& renderer);
    void renderMissionMenu(Renderer& renderer);
    void renderRtfView(Renderer& renderer);
    void renderChapters(Renderer& renderer);
    void renderEnterName(Renderer& renderer);
    void renderScores(Renderer& renderer);
    void openScores();

    // --- multijogador -------------------------------------------------------
    // O GameStartInfo que o GAME SETTINGS monta. Os campos seguem os bits do
    // original: flags bit 0 desastres, bit 1 dia/noite, bit 2 moral fixa,
    // bits 6-8 máximo de jogadores, bits 17-20 veículos iniciais; e, por
    // jogador, playerWord bit 2 Eden, bits 3-4 recursos (= o mesmo campo de
    // _Player::Difficulty), bits 5-7 a cor.
    GameStartInfo m_mpGsi;
    int  m_mpConnection = 3;          // o botão do 259 (3 TCP/IP, 4 modem...)
    bool m_mpIsHost = false;
    std::string m_mpPlayerName = "PLAYER";   // Edit 1044 / 1035 "Your Name"
    std::string m_mpPassword;                // Edit 1037 "Password"
    std::string m_mpSessionName = "OUTPOST 2";
    int  m_mpMaxPlayers = 0;          // índice no combo 1015 (0 = 2 jogadores)
    int  m_mpScenario = 0;            // combo 1039 / 40032
    std::vector<std::string> m_mpScenarioList;  // LevelDesc das missões M*
    std::vector<std::string> m_mpScenarioDlls;
    int  m_mpFindMode = 0;            // 0 = broadcast local, 1 = endereço
    std::string m_mpFindAddr;         // Edit 1008
    std::vector<LobbySession> m_mpSessions;  // o que a descoberta trouxe (195)
    int  m_mpSelSession = 0;
    std::string m_mpStatus;           // Static 1093 (mensagem de estado)
    std::string m_mpNames[6];         // Static 40000+5i (nomes dos jogadores)
    int  m_mpResources[6] = {1,1,1,1,1,1};  // combo 40002+5i (0 High,1 Med,2 Low)
    int  m_mpColor[6] = {1,2,3,4,5,0};      // combo 40001+5i
    // sub_45EF90 alterna Eden/Plymouth entre os lugares na entrada do lobby.
    bool m_mpEden[6] = {false, true, false, true, false, true};
    bool m_mpReady[6] = {};                 // checkbox 40004+5i
    std::vector<std::string> m_mpChat;      // Edit 40031 (o registro)
    std::string m_mpChatInput;              // Edit 40030 (o que se digita)
    bool m_mpDisasters = false, m_mpDayNight = false, m_mpMoraleSteady = false;
    int  m_mpInitialVehicles = 0;           // combo 1018
    int  m_mpPlayerCount = 1;               // quantos lugares ocupados

    void showMultiplayer();           // o botão 1005 do menu principal
    void mpLoadScenarios();           // as missões M* que servem de cenário
    void mpSyncGsi();                 // passa os controles para o m_mpGsi
    void mpPump();                    // bombeia o lobby enquanto a tela está no ar
    bool mpStartHost();               // CREATE: abre o lobby
    bool mpFindSessions();            // a descoberta por broadcast
    bool mpJoinSelected();            // entra na sessão escolhida
    void renderMpConnection(Renderer& renderer);
    void renderMpNetOptions(Renderer& renderer);
    void renderMpNewGame(Renderer& renderer);
    void renderMpFindSession(Renderer& renderer);
    void renderMpPickSession(Renderer& renderer);
    void renderMpJoinName(Renderer& renderer);
    void renderMpGameSettings(Renderer& renderer);
    int  scoreVisibleLines() const;
    void openRtf(int kind);
    void openChapters();
    std::string colonyName() const;
    int rtfVisibleLines() const;
};

} // namespace op2
