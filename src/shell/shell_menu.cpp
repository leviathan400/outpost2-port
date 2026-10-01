#include "shell_menu.h"
#include "font.h"
#include <iostream>
#include <cstdio>
#include <algorithm>
#include <vector>

namespace op2 {

// ---- Geometria das telas (fiel aos diálogos do OP2SHRES.DLL onde aplicável) ----

// MAIN MENU (Dialog 137): painel à direita, revelando a nave Conestoga.
namespace {
    constexpr int kMenuDlgW = 190, kMenuDlgH = 320;
    constexpr int kMenuDlgX = 640 - kMenuDlgW - 10,  kMenuDlgY = 146;
    constexpr int kMenuBtnX = kMenuDlgX + 27;
    constexpr int kMenuBtnW = 135, kMenuBtnH = 23;
    constexpr int kMenuFirstY = kMenuDlgY + 30;
    constexpr int kMenuStepY = 32;
}

// TUTORIALS (Dialog 269): listbox com scrollbar + START + BACK TO MAIN MENU.
namespace {
    constexpr int kTutDlgX = 110, kTutDlgY = 96;
    constexpr int kTutDlgW = 420, kTutDlgH = 288;
    constexpr int kTutListX = kTutDlgX + 14, kTutListY = kTutDlgY + 34;
    constexpr int kTutListW = kTutDlgW - 28, kTutListH = 200;
    constexpr int kTutRowH = 22;
    constexpr int kTutSbW = 14;
    constexpr int kTutVisible = kTutListH / kTutRowH;
    constexpr int kTutBtnY = kTutDlgY + kTutDlgH - 34;
    constexpr int kTutBtnH = 26;
    constexpr int kTutStartW = 100, kTutBackW = 196;
    constexpr int kTutStartX = kTutDlgX + 20;
    constexpr int kTutBackX  = kTutDlgX + kTutDlgW - kTutBackW - 20;
}

// COLONY GAMES
namespace {
    constexpr int kCgX = 120, kCgY = 90, kCgW = 400, kCgH = 310;
    constexpr int kCgListX = kCgX + 16, kCgListY = kCgY + 36;
    constexpr int kCgListW = kCgW - 32, kCgListH = 200;
    constexpr int kCgRowH = 22;
    constexpr int kCgBtnY = kCgY + 258;
}

// NEW CAMPAIGN / PREFERENCES / ABOUT
namespace {
    constexpr int kNcX = 180, kNcY = 130, kNcW = 280, kNcH = 220;
    constexpr int kPrX = 140, kPrY = 100, kPrW = 360, kPrH = 300;
    constexpr int kAbX = 140, kAbY = 100, kAbW = 360, kAbH = 300;
}

ShellMenu::ShellMenu() = default;
ShellMenu::~ShellMenu() = default;

bool ShellMenu::init(const std::string& resDllPath) {
    PeResourceReader reader;
    if (!reader.load(resDllPath)) {
        std::cerr << "[ShellMenu] Failed to load " << resDllPath << std::endl;
        return false;
    }

    std::vector<uint8_t> bgDib = reader.getBitmapDIB(190);
    if (!bgDib.empty()) {
        m_bgBitmap.loadFromDIB(bgDib.data(), bgDib.size());
        std::cout << "[ShellMenu] Loaded background Bitmap 190 ("
                  << m_bgBitmap.getWidth() << "x" << m_bgBitmap.getHeight() << ")" << std::endl;
    } else {
        std::cerr << "[ShellMenu] Warning: Failed to load Bitmap 190" << std::endl;
    }

    std::vector<uint8_t> logoDib = reader.getBitmapDIB(145);
    if (!logoDib.empty()) {
        m_logoBitmap.loadFromDIB(logoDib.data(), logoDib.size());
        std::cout << "[ShellMenu] Loaded logo Bitmap 145 ("
                  << m_logoBitmap.getWidth() << "x" << m_logoBitmap.getHeight() << ")" << std::endl;
    }

    if (reader.getDialog(137, m_mainMenuDlg)) {
        std::cout << "[ShellMenu] Loaded Dialog 137: " << m_mainMenuDlg.title
                  << " with " << m_mainMenuDlg.controls.size() << " controls." << std::endl;
    }

    m_colonyGameList = {
        "A New Beginning", "Divided Destiny", "Survival on Tethys",
        "Evacuation Protocol", "The Blight Approaches", "Resource Rush"
    };

    // Lista de tutoriais: enumera as missões T##.DLL e lê o export "LevelDesc".
    m_tutorialList.clear();
    for (int i = 1; i <= 14; ++i) {
        char dll[16];
        std::snprintf(dll, sizeof(dll), "T%02d.DLL", i);
        PeResourceReader mission;
        if (mission.load(dll)) {
            std::string desc = mission.getExportString("LevelDesc");
            if (!desc.empty()) m_tutorialList.push_back(desc);
        }
    }
    if (m_tutorialList.empty())
        m_tutorialList = {"Tutorial 1 - Navigating the Map"};
    std::cout << "[ShellMenu] " << m_tutorialList.size()
              << " tutoriais lidos das missões T##.DLL." << std::endl;

    setState(MenuState::Splash);
    return true;
}

// ------------------------- sistema de controles/foco -------------------------

Control& ShellMenu::addButton(int id, const std::string& text, int x, int y,
                              int w, int h, bool isDefault, bool disabled) {
    Control c;
    c.kind = CtrlKind::Button;
    c.id = id; c.text = text;
    c.x = x; c.y = y; c.w = w; c.h = h;
    c.isDefault = isDefault; c.disabled = disabled;
    m_controls.push_back(c);
    return m_controls.back();
}

void ShellMenu::setState(MenuState s) {
    m_state = s;
    if (s == MenuState::Splash) m_splashTimer = 0.0f;
    rebuildControls();
}

void ShellMenu::rebuildControls() {
    m_controls.clear();
    m_focusIndex = -1;

    switch (m_state) {
    case MenuState::MainMenu: {
        struct Def { int id; const char* label; bool dis; } defs[] = {
            {1014, "TUTORIALS", false}, {1000, "NEW CAMPAIGN", false},
            {1006, "COLONY GAMES", false}, {1005, "MULTIPLAYER", true},
            {1007, "LOAD GAME", false}, {1011, "GAME PREFERENCES", false},
            {1012, "HELP", false}, {1010, "ABOUT OUTPOST 2", false},
            {1013, "QUIT", false}
        };
        for (int i = 0; i < 9; ++i)
            addButton(defs[i].id, defs[i].label, kMenuBtnX,
                      kMenuFirstY + i * kMenuStepY, kMenuBtnW, kMenuBtnH, false, defs[i].dis);
        break;
    }
    case MenuState::Tutorials: {
        m_tutorialScroll = 0;
        Control lc; lc.kind = CtrlKind::List; lc.id = 1076;
        lc.x = kTutListX; lc.y = kTutListY; lc.w = kTutListW - kTutSbW; lc.h = kTutListH;
        lc.listSel = &m_selectedTutorial; lc.listCount = (int)m_tutorialList.size();
        m_controls.push_back(lc);
        addButton(1, "START", kTutStartX, kTutBtnY, kTutStartW, kTutBtnH, true);
        addButton(2, "BACK TO MAIN MENU", kTutBackX, kTutBtnY, kTutBackW, kTutBtnH);
        break;
    }
    case MenuState::ColonyGames: {
        Control lc; lc.kind = CtrlKind::List; lc.id = 1;
        lc.x = kCgListX; lc.y = kCgListY; lc.w = kCgListW; lc.h = kCgListH;
        lc.listSel = &m_selectedColonyGame; lc.listCount = (int)m_colonyGameList.size();
        m_controls.push_back(lc);
        addButton(10, "PLAY", kCgX + 30, kCgBtnY, 100, 26, true);
        addButton(11, "SCORES", kCgX + 150, kCgBtnY, 100, 26);
        addButton(12, "BACK", kCgX + 270, kCgBtnY, 100, 26);
        break;
    }
    case MenuState::NewCampaign: {
        auto addRadio = [&](const char* t, int x, int y, int* tgt, int val) {
            Control c; c.kind = CtrlKind::Radio; c.text = t;
            c.x = x; c.y = y; c.w = 110; c.h = 14;
            c.radioTarget = tgt; c.radioValue = val;
            m_controls.push_back(c);
        };
        addRadio("Eden",     kNcX + 24,  kNcY + 56, &m_campaignColony, 0);
        addRadio("Plymouth", kNcX + 24,  kNcY + 76, &m_campaignColony, 1);
        addRadio("Easy",     kNcX + 150, kNcY + 56, &m_campaignDifficulty, 0);
        addRadio("Normal",   kNcX + 150, kNcY + 76, &m_campaignDifficulty, 1);
        addRadio("Hard",     kNcX + 150, kNcY + 96, &m_campaignDifficulty, 2);
        addButton(1, "OK",     kNcX + 35,  kNcY + 165, 95, 26, true);
        addButton(2, "CANCEL", kNcX + 150, kNcY + 165, 95, 26);
        break;
    }
    case MenuState::Preferences: {
        auto addCheck = [&](const char* t, int y, bool* tgt) {
            Control c; c.kind = CtrlKind::Check; c.text = t;
            c.x = kPrX + 24; c.y = y; c.w = 240; c.h = 14;
            c.checkTarget = tgt;
            m_controls.push_back(c);
        };
        int cy = kPrY + 40;
        addCheck("Sound Effects Enabled", cy, &m_prefSound);      cy += 24;
        addCheck("Ambient Music Enabled", cy, &m_prefMusic);      cy += 24;
        addCheck("Ambient Animations",    cy, &m_prefAnimations); cy += 24;
        addCheck("Unit Shadows",          cy, &m_prefUnitShadows);cy += 24;
        addCheck("Structure Shadows",     cy, &m_prefStructShadows);
        addButton(1, "ACCEPT", kPrX + (kPrW - 100) / 2, kPrY + 258, 100, 26, true);
        break;
    }
    case MenuState::About:
        addButton(1, "OK", kAbX + (kAbW - 100) / 2, kAbY + 258, 100, 26, true);
        break;
    default:
        break;
    }

    // Foco inicial: primeiro controle habilitado.
    for (int i = 0; i < (int)m_controls.size(); ++i)
        if (!m_controls[i].disabled) { m_focusIndex = i; break; }
}

int ShellMenu::controlAt(int x, int y) const {
    for (int i = 0; i < (int)m_controls.size(); ++i) {
        const Control& c = m_controls[i];
        if (c.disabled) continue;
        if (x >= c.x && x < c.x + c.w && y >= c.y && y < c.y + c.h) return i;
    }
    return -1;
}

void ShellMenu::moveFocus(int dir) {
    int n = (int)m_controls.size();
    if (n == 0) return;
    int i = (m_focusIndex < 0) ? 0 : m_focusIndex;
    for (int step = 0; step < n; ++step) {
        i = (i + dir + n) % n;
        if (!m_controls[i].disabled) { m_focusIndex = i; return; }
    }
}

void ShellMenu::activateControl(int index) {
    if (index < 0 || index >= (int)m_controls.size()) return;
    Control& c = m_controls[index];
    if (c.disabled) return;
    switch (c.kind) {
    case CtrlKind::Button: dispatchButton(c.id); break;               // pode reconstruir
    case CtrlKind::Check:  if (c.checkTarget) *c.checkTarget = !*c.checkTarget; break;
    case CtrlKind::Radio:  if (c.radioTarget) *c.radioTarget = c.radioValue; break;
    case CtrlKind::List:   break;
    }
}

// Lê o export MapName da DLL de uma missão (o .map que ela usa). "" se falhar.
static std::string missionMapName(const char* dll) {
    PeResourceReader m;
    if (m.load(dll)) {
        std::string mn = m.getExportString("MapName");
        if (!mn.empty()) return mn;
    }
    return "";
}

bool ShellMenu::consumeLaunch(std::string& outMap) {
    if (!m_launchRequested) return false;
    outMap = m_launchMap;
    m_launchRequested = false;
    m_launchMap.clear();
    return true;
}

void ShellMenu::dispatchButton(int id) {
    switch (m_state) {
    case MenuState::MainMenu:
        switch (id) {
        case 1014: setState(MenuState::Tutorials); break;
        case 1000: setState(MenuState::NewCampaign); break;
        case 1006: setState(MenuState::ColonyGames); break;
        case 1011: setState(MenuState::Preferences); break;
        case 1010: setState(MenuState::About); break;
        case 1013: m_quitRequested = true; break;
        default: std::cout << "[Menu] botão " << id << " (ainda sem função)" << std::endl; break;
        }
        break;
    case MenuState::Tutorials:
        if (id == 1) {   // START: resolve o .map da missão de tutorial selecionada
            char dll[16];
            std::snprintf(dll, sizeof(dll), "T%02d.DLL", m_selectedTutorial + 1);
            std::string map = missionMapName(dll);
            std::cout << "[Tutorials] START: " << dll << " -> "
                      << (map.empty() ? "(MapName nao encontrado)" : map) << std::endl;
            if (!map.empty()) { m_launchMap = map; m_launchRequested = true; }
        } else if (id == 2) setState(MenuState::MainMenu);
        break;
    case MenuState::ColonyGames:
        if (id == 12) setState(MenuState::MainMenu);
        else std::cout << "[Colony] botão " << id << std::endl;
        break;
    case MenuState::NewCampaign:
        if (id == 1) {   // OK: inicia a missão 1 da colônia escolhida (E01/P01)
            const char* dll = (m_campaignColony == 0) ? "E01.DLL" : "P01.DLL";
            std::string map = missionMapName(dll);
            std::cout << "[Campaign] " << (m_campaignColony == 0 ? "Eden" : "Plymouth")
                      << " dif " << m_campaignDifficulty << " -> " << dll << " -> "
                      << (map.empty() ? "(MapName nao encontrado)" : map) << std::endl;
            if (!map.empty()) { m_launchMap = map; m_launchRequested = true; }
        } else if (id == 2) setState(MenuState::MainMenu);
        break;
    case MenuState::Preferences:
        if (id == 1) setState(MenuState::MainMenu);
        break;
    case MenuState::About:
        if (id == 1) setState(MenuState::MainMenu);
        break;
    default: break;
    }
}

// --------------------------------- input ---------------------------------

void ShellMenu::handleMouseMove(int x, int y) {
    for (auto& c : m_controls)
        c.hovered = (!c.disabled && x >= c.x && x < c.x + c.w && y >= c.y && y < c.y + c.h);
}

void ShellMenu::handleMouseDown(int x, int y, uint8_t button) {
    if (button != SDL_BUTTON_LEFT) return;
    if (m_state == MenuState::Splash) return;

    // Setas da scrollbar dos tutoriais (não são controles focáveis).
    if (m_state == MenuState::Tutorials) {
        int lx = kTutListX, ly = kTutListY, lw = kTutListW - kTutSbW, lh = kTutListH;
        int sbX = lx + lw;
        if (x >= sbX && x < sbX + kTutSbW) {
            int total = (int)m_tutorialList.size();
            int maxScroll = (total > kTutVisible) ? (total - kTutVisible) : 0;
            if (y >= ly && y < ly + kTutSbW)
                m_tutorialScroll = std::max(0, m_tutorialScroll - 1);
            else if (y >= ly + lh - kTutSbW && y < ly + lh)
                m_tutorialScroll = std::min(maxScroll, m_tutorialScroll + 1);
            return;
        }
    }

    int idx = controlAt(x, y);
    if (idx < 0) return;
    m_focusIndex = idx;
    Control& c = m_controls[idx];
    if (c.kind == CtrlKind::List) {
        if (c.listSel) {   // seleciona o item sob o cursor
            int rowH = (m_state == MenuState::Tutorials) ? kTutRowH : kCgRowH;
            int top  = (m_state == MenuState::Tutorials) ? (c.y + 4) : (c.y + 8);
            int scroll = (m_state == MenuState::Tutorials) ? m_tutorialScroll : 0;
            int row = (y - top) / rowH;
            int sel = scroll + row;
            if (row >= 0 && sel >= 0 && sel < c.listCount) *c.listSel = sel;
        }
    } else {
        c.pressed = true;
    }
}

void ShellMenu::handleMouseUp(int x, int y, uint8_t button) {
    if (button != SDL_BUTTON_LEFT) return;
    int toActivate = -1;
    for (int i = 0; i < (int)m_controls.size(); ++i) {
        Control& c = m_controls[i];
        if (c.pressed) {
            c.pressed = false;
            if (x >= c.x && x < c.x + c.w && y >= c.y && y < c.y + c.h) toActivate = i;
        }
    }
    if (toActivate >= 0) activateControl(toActivate);   // pode reconstruir m_controls
}

void ShellMenu::handleMouseWheel(int delta) {
    if (m_state == MenuState::Tutorials) {
        int total = (int)m_tutorialList.size();
        int maxScroll = (total > kTutVisible) ? (total - kTutVisible) : 0;
        m_tutorialScroll = std::max(0, std::min(maxScroll, m_tutorialScroll - delta));
    }
}

void ShellMenu::handleKeyDown(int keyCode) {
    if (m_state == MenuState::Splash) return;
    SDL_Keymod mod = SDL_GetModState();

    switch (keyCode) {
    case SDLK_TAB:
        moveFocus((mod & KMOD_SHIFT) ? -1 : +1);
        break;

    case SDLK_RETURN:
    case SDLK_KP_ENTER: {
        // Enter -> botão default; senão, o botão focado.
        int def = -1;
        for (int i = 0; i < (int)m_controls.size(); ++i)
            if (m_controls[i].isDefault && !m_controls[i].disabled) { def = i; break; }
        if (def >= 0) activateControl(def);
        else if (m_focusIndex >= 0 && m_controls[m_focusIndex].kind == CtrlKind::Button)
            activateControl(m_focusIndex);
        break;
    }

    case SDLK_SPACE:
        // Espaço -> aciona o controle focado (botão, checkbox ou radio).
        if (m_focusIndex >= 0 && m_controls[m_focusIndex].kind != CtrlKind::List)
            activateControl(m_focusIndex);
        break;

    case SDLK_UP:
    case SDLK_DOWN: {
        if (m_focusIndex < 0) break;
        Control& c = m_controls[m_focusIndex];
        int dir = (keyCode == SDLK_DOWN) ? +1 : -1;
        if (c.kind == CtrlKind::List && c.listSel && c.listCount > 0) {
            int s = std::max(0, std::min(c.listCount - 1, *c.listSel + dir));
            *c.listSel = s;
            if (m_state == MenuState::Tutorials) {   // manter visível
                if (s < m_tutorialScroll) m_tutorialScroll = s;
                else if (s >= m_tutorialScroll + kTutVisible) m_tutorialScroll = s - kTutVisible + 1;
            }
        } else if (c.kind == CtrlKind::Radio && c.radioTarget) {
            // move a seleção dentro do grupo (mesmo radioTarget)
            std::vector<int> vals; std::vector<int> idxs;
            for (int i = 0; i < (int)m_controls.size(); ++i)
                if (m_controls[i].kind == CtrlKind::Radio && m_controls[i].radioTarget == c.radioTarget) {
                    vals.push_back(m_controls[i].radioValue); idxs.push_back(i);
                }
            int pos = 0;
            for (int k = 0; k < (int)vals.size(); ++k) if (vals[k] == *c.radioTarget) { pos = k; break; }
            pos = (pos + dir + (int)vals.size()) % (int)vals.size();
            *c.radioTarget = vals[pos];
            m_focusIndex = idxs[pos];
        }
        break;
    }

    case SDLK_ESCAPE:
        if (m_state != MenuState::MainMenu) setState(MenuState::MainMenu);
        else m_quitRequested = true;
        break;
    }
}

void ShellMenu::update(float dt) {
    if (m_state == MenuState::Splash) {
        m_splashTimer += dt;
        if (m_splashTimer > 2.0f) setState(MenuState::MainMenu);
    }
}

// --------------------------------- render ---------------------------------

void ShellMenu::render(Renderer& renderer) {
    if (m_bgBitmap.getWidth() > 0)
        renderer.blitBitmap(m_bgBitmap, 0, 0);
    else
        renderer.clear(0xFF000029);

    if (m_state == MenuState::Splash) {
        renderSplash(renderer);
        return;
    }

    // Logo no topo do menu, com 60% de transparência (40% opaco).
    if (m_logoBitmap.getWidth() > 0)
        renderer.blitBitmapAlpha(m_logoBitmap, 48, 16, 102, true, 0xFF000000);

    switch (m_state) {
    case MenuState::MainMenu:    renderMainMenu(renderer); break;
    case MenuState::NewCampaign: renderNewCampaignDialog(renderer); break;
    case MenuState::ColonyGames: renderColonyGamesDialog(renderer); break;
    case MenuState::Tutorials:   renderTutorialsDialog(renderer); break;
    case MenuState::Preferences: renderPreferencesDialog(renderer); break;
    case MenuState::About:       renderAboutDialog(renderer); break;
    default: break;
    }

    // Controles (botões/radios/checks) + destaque de hover e foco, por cima.
    drawControls(renderer);
}

void ShellMenu::drawControls(Renderer& renderer) {
    uint32_t* fb = renderer.getFramebuffer();
    for (int i = 0; i < (int)m_controls.size(); ++i) {
        Control& c = m_controls[i];
        bool focused = (i == m_focusIndex);
        bool hl = c.hovered || focused;   // destaque = hover OU foco

        switch (c.kind) {
        case CtrlKind::Button:
            renderer.drawButton(c.x, c.y, c.w, c.h, c.text,
                                c.hovered || focused, c.pressed, c.disabled);
            break;
        case CtrlKind::Radio: {
            bool sel = c.radioTarget && (*c.radioTarget == c.radioValue);
            uint32_t col = c.disabled ? 0xFF60708A
                         : (hl ? 0xFFFFFF99 : (sel ? 0xFFE0ECFF : 0xFFA0B0C8));
            Font::drawText(fb, 640, 480, (sel ? "(o) " : "( ) ") + c.text, c.x, c.y, col, true);
            break;
        }
        case CtrlKind::Check: {
            bool on = c.checkTarget && *c.checkTarget;
            uint32_t col = c.disabled ? 0xFF60708A
                         : (hl ? 0xFFFFFF99 : (on ? 0xFFE0ECFF : 0xFFA0B0C8));
            Font::drawText(fb, 640, 480, (on ? "[X] " : "[ ] ") + c.text, c.x, c.y, col, true);
            break;
        }
        case CtrlKind::List:
            break;   // conteúdo desenhado no render do diálogo
        }

        // Retângulo de foco (amarelo) mostrando qual controle está focado.
        if (focused && !c.disabled)
            renderer.drawRect(c.x - 3, c.y - 3, c.w + 6, c.h + 6, 0xFFFFD24A);
    }
}

void ShellMenu::renderSplash(Renderer& renderer) {
    // Tela de carregamento: logo OPACO centralizado; entra no menu sozinho.
    if (m_logoBitmap.getWidth() > 0) {
        int lw = m_logoBitmap.getWidth();
        int lh = m_logoBitmap.getHeight();
        renderer.blitBitmap(m_logoBitmap, (640 - lw) / 2, (480 - lh) / 2 - 20, true, 0xFF000000);
    }
    Font::drawTextCentered(renderer.getFramebuffer(), 640, 480,
                           "Loading...", 320, 430, 0xFF9CB0C8, true);
}

void ShellMenu::renderMainMenu(Renderer& renderer) {
    renderer.drawDialogFrame(kMenuDlgX, kMenuDlgY, kMenuDlgW, kMenuDlgH, "MAIN MENU");
    // Os botões são desenhados por drawControls().
}

void ShellMenu::renderNewCampaignDialog(Renderer& renderer) {
    renderer.fillRect(0, 0, 640, 480, 0x88000000);
    renderer.drawDialogFrame(kNcX, kNcY, kNcW, kNcH, "NEW CAMPAIGN OPTIONS");
    uint32_t* fb = renderer.getFramebuffer();
    Font::drawText(fb, 640, 480, "Colony:", kNcX + 16, kNcY + 36, 0xFFD8E4F8, true);
    Font::drawText(fb, 640, 480, "Difficulty:", kNcX + 150, kNcY + 36, 0xFFD8E4F8, true);
    // Radios e botões são desenhados por drawControls().
}

void ShellMenu::renderAboutDialog(Renderer& renderer) {
    renderer.fillRect(0, 0, 640, 480, 0x88000000);
    renderer.drawDialogFrame(kAbX, kAbY, kAbW, kAbH, "ABOUT OUTPOST 2...");
    uint32_t* fb = renderer.getFramebuffer();
    Font::drawTextCentered(fb, 640, 480, "Outpost 2: Divided Destiny", kAbX + kAbW/2, kAbY + 36, 0xFFFFFFFF, true);
    Font::drawTextCentered(fb, 640, 480, "Copyright 1997, Sierra On-Line, Inc.", kAbX + kAbW/2, kAbY + 56, 0xFF9CB0C8, true);
    Font::drawTextCentered(fb, 640, 480, "Modern C++ SDL2 Engine Port", kAbX + kAbW/2, kAbY + 76, 0xFF8AE0AA, true);

    int boxX = kAbX + 16, boxY = kAbY + 105, boxW = kAbW - 32, boxH = 140;
    renderer.fillRect(boxX, boxY, boxW, boxH, 0xFF0D121B);
    renderer.drawBevelRect(boxX, boxY, boxW, boxH, 0xFF05080E, 0xFF35445B);
    Font::drawText(fb, 640, 480, "Original Dynamix / Sierra Team:", boxX + 10, boxY + 10, 0xFFFFFF99);
    Font::drawText(fb, 640, 480, "- Lead Design: Jason Pratt", boxX + 10, boxY + 28, 0xFFD0DCE8);
    Font::drawText(fb, 640, 480, "- Lead Programming: Bryan Shannon", boxX + 10, boxY + 44, 0xFFD0DCE8);
    Font::drawText(fb, 640, 480, "- Art Director: Terry Robinson", boxX + 10, boxY + 60, 0xFFD0DCE8);
    Font::drawText(fb, 640, 480, "- Audio: Jan Paul Moorhead", boxX + 10, boxY + 76, 0xFFD0DCE8);
    Font::drawText(fb, 640, 480, "Reconstruction 1:1 via Disassembly", boxX + 10, boxY + 105, 0xFF65C8FF);
    // Botão OK desenhado por drawControls().
}

void ShellMenu::renderPreferencesDialog(Renderer& renderer) {
    renderer.fillRect(0, 0, 640, 480, 0x88000000);
    renderer.drawDialogFrame(kPrX, kPrY, kPrW, kPrH, "PREFERENCES");
    uint32_t* fb = renderer.getFramebuffer();
    // Linhas não-interativas (as checkboxes são desenhadas por drawControls()).
    int y = kPrY + 40 + 5 * 24 + 4;
    Font::drawText(fb, 640, 480, "Game Speed: 5 / 10", kPrX + 24, y, 0xFFD8E4F8, true); y += 24;
    Font::drawText(fb, 640, 480, "Scroll Speed: 4 / 10", kPrX + 24, y, 0xFFD8E4F8, true);
}

void ShellMenu::renderColonyGamesDialog(Renderer& renderer) {
    renderer.fillRect(0, 0, 640, 480, 0x88000000);
    renderer.drawDialogFrame(kCgX, kCgY, kCgW, kCgH, "COLONY GAMES");

    renderer.fillRect(kCgListX, kCgListY, kCgListW, kCgListH, 0xFF0D121B);
    renderer.drawBevelRect(kCgListX, kCgListY, kCgListW, kCgListH, 0xFF05080E, 0xFF35445B);
    for (size_t i = 0; i < m_colonyGameList.size(); ++i) {
        int itemY = kCgListY + 8 + (int)i * kCgRowH;
        if (itemY + 16 > kCgListY + kCgListH) break;
        bool sel = ((int)i == m_selectedColonyGame);
        if (sel) renderer.fillRect(kCgListX + 2, itemY - 2, kCgListW - 4, 18, 0xFF203858);
        Font::drawText(renderer.getFramebuffer(), 640, 480, m_colonyGameList[i],
                       kCgListX + 10, itemY, sel ? 0xFFFFFF99 : 0xFFCCD6E8);
    }
    // Botões PLAY / SCORES / BACK desenhados por drawControls().
}

void ShellMenu::renderTutorialsDialog(Renderer& renderer) {
    renderer.fillRect(0, 0, 640, 480, 0x88000000);
    renderer.drawDialogFrame(kTutDlgX, kTutDlgY, kTutDlgW, kTutDlgH, "TUTORIALS");

    int lx = kTutListX, ly = kTutListY, lw = kTutListW - kTutSbW, lh = kTutListH;
    renderer.fillRect(lx, ly, lw, lh, 0xFF0D121B);
    renderer.drawBevelRect(lx, ly, lw, lh, 0xFF05080E, 0xFF35445B);

    int total = (int)m_tutorialList.size();
    for (int row = 0; row < kTutVisible; ++row) {
        int i = m_tutorialScroll + row;
        if (i >= total) break;
        int itemY = ly + 4 + row * kTutRowH;
        bool sel = (i == m_selectedTutorial);
        if (sel) renderer.fillRect(lx + 2, itemY - 1, lw - 4, kTutRowH - 2, 0xFF264066);
        std::string txt = m_tutorialList[i];
        while (!txt.empty() && Font::getTextWidth(txt) > lw - 16) txt.pop_back();
        Font::drawText(renderer.getFramebuffer(), 640, 480, txt, lx + 8, itemY + 4,
                       sel ? 0xFFFFFF99 : 0xFFCCD6E8);
    }

    // Scrollbar vertical.
    int sbX = lx + lw, sbY = ly, sbH = lh;
    renderer.fillRect(sbX, sbY, kTutSbW, sbH, 0xFF11161F);
    renderer.drawBevelRect(sbX, sbY, kTutSbW, sbH, 0xFF05080E, 0xFF35445B);
    renderer.drawButton(sbX, sbY, kTutSbW, kTutSbW, "^", false, false);
    renderer.drawButton(sbX, sbY + sbH - kTutSbW, kTutSbW, kTutSbW, "v", false, false);
    int trackY = sbY + kTutSbW, trackH = sbH - 2 * kTutSbW;
    int maxScroll = (total > kTutVisible) ? (total - kTutVisible) : 0;
    int thumbH = (total > kTutVisible) ? std::max(16, trackH * kTutVisible / total) : trackH;
    thumbH = std::min(thumbH, trackH);
    int thumbY = trackY + (maxScroll > 0 ? (trackH - thumbH) * m_tutorialScroll / maxScroll : 0);
    renderer.fillRect(sbX + 2, thumbY, kTutSbW - 4, thumbH, 0xFF3A557E);
    renderer.drawBevelRect(sbX + 2, thumbY, kTutSbW - 4, thumbH, 0xFF5A7AA8, 0xFF223144);
    // Botões START / BACK desenhados por drawControls().
}

} // namespace op2
