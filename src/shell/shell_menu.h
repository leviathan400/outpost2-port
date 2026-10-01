#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "../gfx/renderer.h"
#include "../gfx/bitmap.h"
#include "../res/pe_resource.h"

namespace op2 {

enum class MenuState {
    Splash,        // tela de abertura com o logo (antes do menu)
    MainMenu,
    NewCampaign,
    ColonyGames,
    Tutorials,
    Preferences,
    About,
    QuitConfirm
};

enum class CtrlKind { Button, Radio, Check, List };

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
};

class ShellMenu {
public:
    ShellMenu();
    ~ShellMenu();

    bool init(const std::string& resDllPath = "OP2SHRES.DLL");

    void handleMouseMove(int nativeX, int nativeY);
    void handleMouseDown(int nativeX, int nativeY, uint8_t button);
    void handleMouseUp(int nativeX, int nativeY, uint8_t button);
    void handleKeyDown(int keyCode);
    void handleMouseWheel(int delta);

    void update(float dt);
    void render(Renderer& renderer);

    bool shouldQuit() const { return m_quitRequested; }

    // O menu pediu para iniciar uma missão? Se sim, devolve o nome do .map (do
    // export MapName da DLL da missão) e limpa o pedido. Consumido pelo ShellState
    // para o TApp trocar para o jogo (ActivateGame). Ver [[op2-engine-architecture]].
    bool consumeLaunch(std::string& outMap);

    // Abre diretamente uma sub-tela (usado pelo modo --screenshot para captura).
    void showTutorials() { setState(MenuState::Tutorials); }
    void showMainMenu() { setState(MenuState::MainMenu); }
    void showNewCampaign() { setState(MenuState::NewCampaign); }
    void showPreferences() { setState(MenuState::Preferences); }
    void showColonyGames() { setState(MenuState::ColonyGames); }
    void showAbout() { setState(MenuState::About); }

private:
    MenuState m_state = MenuState::Splash;
    bool m_quitRequested = false;
    float m_splashTimer = 0.0f;     // tempo na tela de splash

    // Pedido de início de missão (START/OK): nome do .map resolvido via MapName.
    bool m_launchRequested = false;
    std::string m_launchMap;

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

    std::vector<std::string> m_colonyGameList;
    std::vector<std::string> m_tutorialList;

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

    void renderSplash(Renderer& renderer);
    void renderMainMenu(Renderer& renderer);
    void renderNewCampaignDialog(Renderer& renderer);
    void renderAboutDialog(Renderer& renderer);
    void renderPreferencesDialog(Renderer& renderer);
    void renderColonyGamesDialog(Renderer& renderer);
    void renderTutorialsDialog(Renderer& renderer);
};

} // namespace op2
