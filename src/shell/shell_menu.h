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
#include "../res/winhelp.h"
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
    Help,          // o manual do OUTPOST2.HLP (no original, WinHelp)
    // --- multijogador: as telas vêm do OUT2RES.DLL, não do shell, porque no
    // original o botão MULTIPLAYER chama TApp::DoNetGame no executável.
    // O Dialog 259 "PICK CONNECTION METHOD" (sub_411E30) nao existe aqui: dos
    // seis transportes da tabela unk_4DF370 so o TCP/IP (3) sobreviveu ao
    // tempo, entao o MULTIPLAYER abre direto o 260 e o CANCEL dele volta ao
    // menu principal. MpNetOptions e a primeira tela de multijogador, e a
    // ordem deste enum importa: o render usa ">= MpNetOptions".
    MpNetOptions,  // Dialog 260 "NET GAME OPTIONS" CREATE/JOIN (sub_497610)
    MpNewGame,     // Dialog 2012 "NEW NETWORK GAME" (criar, sub_40DEC0)
    MpFindSession, // Dialog 2010 "FIND SESSION" broadcast ou endereço
    MpPickSession, // Dialog 195 "PICK SESSION" (a lista que a descoberta trouxe)
    MpJoinName,    // Dialog 2013 "JOIN A MULTIPLAYER GAME" (nome)
    MpGameSettings // Dialog 196 "GAME SETTINGS" (o lobby da partida)
};

enum class CtrlKind { Button, Radio, Check, List, Combo, Edit, Slider };

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
    // Combo: as opções e o índice escolhido. Um clique abre a lista suspensa
    // (CBS_DROPDOWNLIST do original), que fica por cima dos demais controles
    // até a escolha; com a lista fechada as setas trocam a opção direto, como
    // no Windows.
    const std::vector<std::string>* comboItems = nullptr;
    int*  comboSel = nullptr;
    std::string* editTarget = nullptr;   // Edit: o texto que recebe digitação
    size_t editMax = 0;                  // limite de caracteres (0 = sem limite)
    // Slider: a barra de rolagem horizontal. O cursor arrasta com o mouse e as
    // setas andam de kSliderStep; o valor vive em *sliderTarget.
    int*  sliderTarget = nullptr;
    int   sliderMin = 0, sliderMax = 100;
};

// Geometria de um diálogo do OUT2RES.DLL/OP2SHRES.DLL na tela de 640x480. O
// template dá tudo em unidades de diálogo (DLU), e com MS Sans Serif 8 — a
// fonte de todos eles — 1 DLU vale 1,5 px na horizontal e 1,6 px na vertical,
// a mesma conversão que o resto do shell usa. O quadro acrescenta a borda e a
// barra de título; (ox,oy) é o canto do cliente, de onde as DLU contam.
struct DlgBox {
    int fx = 0, fy = 0, fw = 0, fh = 0;   // o quadro inteiro
    int ox = 0, oy = 0;                   // a origem do cliente
    int sxn = 3, sxd = 2;                 // DLU -> px na horizontal
    int syn = 8, syd = 5;                 // DLU -> px na vertical
    int x(int dlu) const { return ox + dlu * sxn / sxd; }
    int y(int dlu) const { return oy + dlu * syn / syd; }
    int w(int dlu) const { return dlu * sxn / sxd; }
    int h(int dlu) const { return dlu * syn / syd; }
};
// Centraliza na tela um diálogo de cxDlu x cyDlu.
DlgBox dlgBox(int cxDlu, int cyDlu);
// O GAME SETTINGS é a tela mais cheia do jogo, e a nossa fonte de 8x12 é bem
// mais larga que a MS Sans Serif 8 proporcional do original — a linha das
// opções, por exemplo, precisa de ~610 px onde o template reserva 460. Então
// esse diálogo ocupa a tela inteira, a 2 px por DLU, e só ele.
DlgBox dlgBoxFull(int cxDlu, int cyDlu);

// As opcoes dos combos do GAME SETTINGS, de Format[] no executavel: as cores
// vem dos indices 0x28B..0x290 (tabela unk_4D5470) e os recursos de
// 0x1CF..0x1D1 (unk_4D5460). Definidas em shell_menu_mp.cpp.
extern const std::vector<std::string> kColorNames;
extern const std::vector<std::string> kResourceNames;
extern const std::vector<std::string> kMaxPlayerNames;
extern const std::vector<std::string> kVehicleNames;
// As opcoes do combo de nivel: AUTO deixa valer o que a partida escolheu, e os
// outros tres sao os nomes que o jogo usa (Dialog 140 e off_4E9028).
extern const std::vector<std::string> kAiLevelNames;

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
    void showHelp() { openHelp(); }
    // Teste por linha de comando (--demotest): monta o pedido do demo de
    // atracao como a ociosidade faria, e devolve o relato.
    bool demoTest(std::string& report);
    // Teste por linha de comando (--helptest): abre o manual e percorre a
    // lista e o texto pelo mesmo caminho do teclado e da roda.
    bool helpTest(std::string& report);
    void showColonyGames() { setState(MenuState::ColonyGames); }
    void showAbout() { setState(MenuState::About); }
    void showScores() { openScores(); }
    // Captura (--screenshot): abre uma tela de multijogador sem passar pela
    // rede (0 = 260, 1 = 2012, 2 = 2010, 3 = 195, 4 = 2013, 5 = 196).
    // `host` decide quais controles ficam ativos no GAME SETTINGS.
    void showMpScreen(int which, bool host, int players);
    // Captura (--screenshot): solta a lista do combo de id `id` na tela atual.
    void debugOpenCombo(int id);
    // Teste por linha de comando (--uitest): exercita a lista suspensa pelo
    // mesmo caminho do mouse e do teclado, e devolve o relato de cada passo.
    bool comboTest(std::string& report);
    // Teste por linha de comando (--uitest): exercita a barra do volume pelo
    // mouse (clique, arrasto, pontas) e pelo teclado, e confere que o mixer
    // recebeu cada mudanca.
    bool sliderTest(std::string& report);
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
    // Demo de atracao: o shell do original conta o tempo desde a ultima
    // atividade (GetTickCount - dword_130158DC) e, passado um minuto, manda a
    // mensagem 0x741, que dispara ademo%d.dll. Ver startAttractDemo.
    uint32_t m_lastActivity = 0;
    void noteActivity();            // zera o relogio da ociosidade
    void startAttractDemo();        // a mensagem 0x741 do shell

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
    // Lista suspensa aberta (o combo "solto" do Windows): enquanto houver uma,
    // ela é modal — o cursor e o teclado só falam com ela. O id acompanha a
    // reconstrução dos controles, que acontece sozinha quando alguém entra na
    // mesa, para a lista não fechar na cara de quem estava escolhendo.
    int m_openCombo = -1;           // índice em m_controls; -1 = nenhuma
    int m_openComboId = 0;          // id do controle, para reencontrá-lo
    int m_comboHover = -1;          // item sob o cursor (ou destacado pelo teclado)
    int m_comboScroll = 0;          // 1º item visível, quando a lista não cabe
    // Barra sendo arrastada (índice em m_controls); -1 = nenhuma. Enquanto
    // arrasta, o cursor fala só com ela, mesmo que saia do controle.
    int m_dragSlider = -1;

    // Sub-dialog states
    int m_campaignColony = 0;     // 0 = Eden, 1 = Plymouth
    int m_campaignDifficulty = 1; // 0 = Easy, 1 = Normal, 2 = Hard
    int m_selectedColonyGame = 0;
    int m_selectedTutorial = 0;
    int m_tutorialScroll = 0;      // índice do 1º item visível (scroll do listbox)

    // Estado dos checkboxes de Preferences (antes eram fixos)
    bool m_prefSound = true, m_prefMusic = true, m_prefAnimations = true;
    bool m_prefUnitShadows = false, m_prefStructShadows = false;
    // "AILevel": 0 = AUTO (o nivel que a partida escolher), 1..3 Easy/Normal/
    // Hard. Nao existe no jogo original — ver loadPrefs/savePrefs.
    int  m_prefAiLevel = 0;
    // "AudioVolume" (0..100): volume mestre, tambem acrescimo deste porte. A
    // barra mexe no som na hora, para poder ser ouvida; o que esta gravado
    // fica em m_prefAudioVolumeIni, e sair sem ACCEPT devolve o som a ele.
    int  m_prefAudioVolume = 50;
    int  m_prefAudioVolumeIni = 50;
    void loadPrefs();   // le o [Game] do OUTPOST2.INI para a tela
    void savePrefs();   // grava de volta (botao ACCEPT)

    // --- manual (OUTPOST2.HLP) ---
    WinHelpFile m_help;
    bool m_helpTried = false;        // ja tentamos carregar (nao insiste a cada clique)
    std::string m_helpFallback;      // manual externo aberto, quando nao ha .HLP
    int m_helpTopic = 0;             // topico escolhido na lista
    int m_helpListScroll = 0;        // 1o topico visivel
    int m_helpTextScroll = 0;        // 1a linha visivel do texto
    int m_helpBuiltFor = -1;         // topico de m_helpLines
    std::vector<RtfLine> m_helpLines;
    void openHelp();                 // botao 1012 / F1
    void helpBuildLines();           // monta o texto do topico escolhido
    int  helpVisibleLines() const;
    void renderHelp(Renderer& renderer);

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
    // --- lista suspensa dos combos ---
    void comboOpen(int index);              // abre a lista do controle
    void comboClose(int chosen);            // fecha; >=0 escolhe o item
    bool comboRect(const Control& c, int& x, int& y, int& w, int& h) const;
    int  comboItemAt(int x, int y) const;   // item sob o cursor; -1 fora
    void drawOpenCombo(Renderer& renderer); // desenhada por cima de tudo
    Control& addButton(int id, const std::string& text, int x, int y, int w, int h,
                       bool isDefault = false, bool disabled = false);
    Control& addCheck(int id, const std::string& text, int x, int y, int w, bool* tgt);
    Control& addRadio(int id, const std::string& text, int x, int y, int w,
                      int* tgt, int value);
    Control& addCombo(int id, const std::vector<std::string>* items, int* sel,
                      int x, int y, int w, int h);
    Control& addEdit(int id, std::string* tgt, size_t maxLen, int x, int y, int w, int h);
    Control& addSlider(int id, int* tgt, int lo, int hi, int x, int y, int w, int h);
    // --- barra de rolagem ---
    int  sliderKnobX(const Control& c) const;        // canto esquerdo do cursor
    int  sliderValueAt(const Control& c, int px) const;   // valor sob o cursor
    void sliderSet(Control& c, int value);           // grava, limita e aplica

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
    bool m_mpEjected = false;               // o host nos tirou da mesa
public:
    bool m_mpEjectTest = false;             // --mpui eject: testa o 1046+i
private:
    uint32_t m_mpLastBroadcast = 0;         // quando o host difundiu a mesa

    void showMultiplayer();           // o botão 1005 do menu principal
    void mpLoadScenarios();           // as missões M* que servem de cenário
    void mpSyncGsi();                 // passa os controles para o m_mpGsi
    void mpPump();                    // bombeia o lobby enquanto a tela está no ar
    void mpControlChanged(int id);    // um controle do GAME SETTINGS mudou
    void mpTableToUi(const LobbyTable& t);   // o estado que o host difundiu
    void mpUiToTable(LobbyTable& t) const;   // o estado para difundir
    void mpSendChat(const std::string& line);
    bool mpStartHost();               // CREATE: abre o lobby
    bool mpFindSessions();            // a descoberta por broadcast
    bool mpJoinSelected();            // entra na sessão escolhida
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
