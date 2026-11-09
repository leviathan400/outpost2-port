#include "shell_menu.h"
#include "font.h"
#include "../gfx/dialog_skin.h"
#include "../res/res_manager.h"
#include "../core/config.h"
#include "../game/tethys_game.h"
#include "../audio/sound.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <algorithm>
#include <vector>

namespace op2 {

// A lista suspensa de um combo (ver comboRect e companhia, mais abaixo).
namespace {
constexpr int kComboItemH = 15;    // altura de um item da lista
constexpr int kComboMaxVis = 8;    // itens visíveis antes de rolar
// A barra de rolagem: largura do cursor e quanto andam as teclas (a seta anda
// um degrau, Page um salto, Home/End vao as pontas).
constexpr int kSliderKnobW = 11;
constexpr int kSliderStep = 5;
constexpr int kSliderPage = 20;
}

// ---- Geometria das telas (fiel aos diálogos do OP2SHRES.DLL onde aplicável) ----

// MANUAL (OUTPOST2.HLP). Não há diálogo do original para copiar: no jogo de
// 1997 quem desenhava esta janela era o WinHelp do Windows, que não existe
// mais. A lista de tópicos à esquerda e o texto à direita seguem o mesmo
// esqueleto das outras telas do shell.
namespace {
    constexpr int kHpX = 20, kHpY = 28, kHpW = 600, kHpH = 424;
    constexpr int kHpListX = kHpX + 14, kHpListY = kHpY + 40;
    constexpr int kHpListW = 210, kHpListH = 322;
    int kHpSbW = 14;                      // fork: 17 com a pele OP2 (applyMenuStyle)
    constexpr int kHpRowH = 14;
    constexpr int kHpVisible = kHpListH / kHpRowH;
    constexpr int kHpTextX = kHpListX + kHpListW + 14;
    constexpr int kHpTextY = kHpListY, kHpTextH = kHpListH;
    constexpr int kHpTextW = kHpX + kHpW - 14 - kHpTextX;
}

// MAIN MENU (Dialog 137).
// Fork: tamanho, posição e botões MEDIDOS numa captura do menu original
// (192x348): o fundo "blueprint" da captura bate pixel a pixel com o bitmap 225
// do OP2SHRES no deslocamento (224,54), que é onde a janela fica na tela de
// 640x480. Botões de 146x26 (uma célula da tira 236) em x = 21 (contorno em 23), com os topos
// em 38, 71, 103, 136, 168, 201, 233, 266, 298 (passo alternando 33/32).
namespace {
    // Valores do estilo do porte (painel à direita, revelando a nave
    // Conestoga); applyMenuStyle troca pelos medidos quando MenuSkin=1.
    int kMenuDlgW = 190, kMenuDlgH = 320;
    int kMenuDlgX = 640 - 190 - 10, kMenuDlgY = 146;
    int kMenuBtnX = kMenuDlgX + 27;
    int kMenuBtnW = 135, kMenuBtnH = 23;
    int kMenuBtnTop[9] = {30, 62, 94, 126, 158, 190, 222, 254, 286};
}

// TUTORIALS (Dialog 269): listbox com scrollbar + START + BACK TO MAIN MENU.
namespace {
    constexpr int kTutDlgX = 110, kTutDlgY = 96;
    constexpr int kTutDlgW = 420, kTutDlgH = 288;
    constexpr int kTutListX = kTutDlgX + 14, kTutListY = kTutDlgY + 34;
    constexpr int kTutListW = kTutDlgW - 28, kTutListH = 200;
    int kTutRowH = 22;                    // fork: 14 com a pele OP2 (applyMenuStyle)
    int kTutSbW = 14;                     // fork: 17 com a pele OP2
    int kTutVisible = kTutListH / 22;     // fork: (kTutListH - 6) / 14 com a pele OP2
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
    int kCgRowH = 22;                     // fork: 14 com a pele OP2 (applyMenuStyle)
    constexpr int kCgBtnY = kCgY + 258;

    // Dialog 224 "ENTER YOUR NAME" (224x100 DLU → x·1,5 / y·1,6), centrado.
    // Dialog 155 "MULTIPLAYER GAME STATISTICS": 298x82 DLU. A largura do
    // template (298 DLU = 447 px) basta para a MS Sans Serif 8 do original mas
    // NAO para a nossa fonte: "Enemy Structures Destroyed:" mais o numero
    // passa da borda e os valores caem por cima dos rotulos. A caixa vai a 560
    // e as colunas de numero sao calculadas pela maior largura de rotulo.
    constexpr int kMsW = 560, kMsH = 82 * 8 / 5;
    constexpr int kMsX = (640 - kMsW) / 2, kMsY = (480 - kMsH) / 2;
    constexpr int kEnW = 336, kEnH = 160;
    constexpr int kEnX = (640 - kEnW) / 2, kEnY = (480 - kEnH) / 2;
    constexpr int kEnEditX = kEnX + 150, kEnEditY = kEnY + 22 + 78;   // Edit 1093 (100,49)
    constexpr int kEnEditW = 175, kEnEditH = 26;
    // Dialog 223 "COLONY GAME SCORES" (376x170 DLU), centrado.
    constexpr int kScW = 564, kScH = 272;
    constexpr int kScX = (640 - kScW) / 2, kScY = (480 - kScH) / 2;
    constexpr int kScListX = kScX + 10, kScListY = kScY + 22 + 11;    // ListBox 1092 (7,7)
    constexpr int kScListW = 543, kScListH = 200;
    constexpr int kScBtnY = kScY + 22 + 215;                          // botões (149)
    constexpr int kScRowH = 18;
}

// Campanha (OP2Shell.dll): MISSION MENU (Dialog 219, 124x126 DLU), STORY TITLE
// (Dialog 216, 402x234 DLU, com o RICHEDIT "StoryText" criado em (20,25)
// 565x295 px) e CHAPTERS (Dialog 215, 300x130 DLU). DLU → px: x·1,5 e y·1,6.
namespace {
    constexpr int kMmX = 640 - 190 - 10, kMmY = 146, kMmW = 190, kMmH = 215;
    constexpr int kRvX = 17, kRvY = 40, kRvW = 606, kRvH = 384;
    constexpr int kRvTextX = kRvX + 20, kRvTextY = kRvY + 32, kRvTextW = 565, kRvTextH = 295;
    constexpr int kRvBtnY = kRvTextY + kRvTextH + 20;
    constexpr int kChX = 95, kChY = 120, kChW = 450, kChH = 235;
    constexpr int kChListX = kChX + 10, kChListY = kChY + 30, kChListW = 429, kChListH = 158;
    int kChRowH = 18;                     // fork: 14 com a pele OP2 (applyMenuStyle)
}

// NEW CAMPAIGN / PREFERENCES / ABOUT
namespace {
    constexpr int kNcX = 180, kNcY = 130, kNcW = 280, kNcH = 220;
    // +28 de altura em relacao ao Dialog 142: a linha do modo de tela, que e
    // acrescimo do porte (como ja eram o combo da IA e a barra do volume).
    constexpr int kPrX = 140, kPrY = 59, kPrW = 360, kPrH = 390;
    constexpr int kAbX = 140, kAbY = 100, kAbW = 360, kAbH = 300;
}

// Fork: [Game] MenuSkin escolhe o estilo dos menus. 0 = o do porte (os valores
// acima); 1 = a pele do original, com o menu principal e as listas medidos.
namespace {
void applyMenuStyle(bool op2) {
    if (op2) {
        // MAIN MENU medido numa captura do original (192x348): o fundo
        // "blueprint" bate pixel a pixel com o bitmap 225 no deslocamento
        // (224,54); botões de 146x26 (tira 236) com o contorno em x = 23 e os
        // topos em 38, 71, 103... (passo alternando 33/32).
        static const int tops[9] = {38, 71, 103, 136, 168, 201, 233, 266, 298};
        kMenuDlgW = 192; kMenuDlgH = 348; kMenuDlgX = 224; kMenuDlgY = 54;
        kMenuBtnX = kMenuDlgX + 21; kMenuBtnW = 146; kMenuBtnH = 26;
        for (int i = 0; i < 9; ++i) kMenuBtnTop[i] = tops[i];
        // Listas com linhas de 14 px e a barra de rolagem padrão do Windows.
        kTutRowH = 14; kTutSbW = 17; kTutVisible = (kTutListH - 6) / kTutRowH;
        kCgRowH = 14; kChRowH = 14; kHpSbW = 17;
    } else {
        kMenuDlgW = 190; kMenuDlgH = 320; kMenuDlgX = 640 - 190 - 10; kMenuDlgY = 146;
        kMenuBtnX = kMenuDlgX + 27; kMenuBtnW = 135; kMenuBtnH = 23;
        for (int i = 0; i < 9; ++i) kMenuBtnTop[i] = 30 + i * 32;
        kTutRowH = 22; kTutSbW = 14; kTutVisible = kTutListH / kTutRowH;
        kCgRowH = 22; kChRowH = 18; kHpSbW = 14;
    }
}
} // namespace

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

    // Fork: a pele dos diálogos do original (blueprint 225, botões 235/236),
    // ligada por [Game] MenuSkin=1. Sem ela, o estilo do próprio porte.
    {
        Config cfg("OUTPOST2.INI");
        GameSettings gs;
        if (cfg.load()) gs.loadFrom(cfg);
        const bool loaded = DialogSkin::load(reader);
        DialogSkin::setEnabled(gs.menuSkin != 0);
        applyMenuStyle(DialogSkin::ok());
        std::cout << "[ShellMenu] menus: "
                  << (DialogSkin::ok() ? "OP2 skin" : loaded ? "port style (MenuSkin=0)"
                                                             : "port style (skin not loaded)")
                  << std::endl;
    }

    if (reader.getDialog(137, m_mainMenuDlg)) {
        std::cout << "[ShellMenu] Loaded Dialog 137: " << m_mainMenuDlg.title
                  << " with " << m_mainMenuDlg.controls.size() << " controls." << std::endl;
    }

    // Jogos de colônia: o shell original varre "c*.dll" e lê o export LevelDesc.
    m_colonyGameList.clear();
    m_colonyGameDlls.clear();
    {
        std::error_code ec;
        std::vector<std::string> files;
        for (const auto& de : std::filesystem::directory_iterator(".", ec)) {
            if (ec) break;
            const std::string n = de.path().filename().string();
            if (n.size() < 5) continue;
            const char c0 = static_cast<char>(std::tolower(static_cast<unsigned char>(n[0])));
            std::string ext = n.substr(n.size() - 4);
            for (char& ch : ext) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if (c0 == 'c' && ext == ".dll") files.push_back(n);
        }
        std::sort(files.begin(), files.end());
        for (const std::string& n : files) {
            PeResourceReader mission;
            if (!mission.load(n)) continue;
            const std::string desc = mission.getExportString("LevelDesc");
            if (desc.empty()) continue;                  // só missões de verdade
            m_colonyGameList.push_back(desc);
            m_colonyGameDlls.push_back(n);
        }
    }
    std::cout << "[ShellMenu] " << m_colonyGameList.size()
              << " jogos de colonia lidos das c*.dll." << std::endl;

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

Control& ShellMenu::addSlider(int id, int* tgt, int lo, int hi,
                              int x, int y, int w, int h) {
    Control c;
    c.kind = CtrlKind::Slider;
    c.id = id;
    c.x = x; c.y = y; c.w = w; c.h = h;
    c.sliderTarget = tgt; c.sliderMin = lo; c.sliderMax = hi;
    m_controls.push_back(c);
    return m_controls.back();
}

// A faixa util desconta a largura do cursor, para que as pontas da barra sejam
// exatamente o minimo e o maximo.
int ShellMenu::sliderKnobX(const Control& c) const {
    const int span = c.sliderMax - c.sliderMin;
    const int track = c.w - kSliderKnobW;
    if (span <= 0 || track <= 0) return c.x;
    int v = c.sliderTarget ? *c.sliderTarget : c.sliderMin;
    if (v < c.sliderMin) v = c.sliderMin;
    if (v > c.sliderMax) v = c.sliderMax;
    return c.x + (v - c.sliderMin) * track / span;
}

int ShellMenu::sliderValueAt(const Control& c, int px) const {
    const int span = c.sliderMax - c.sliderMin;
    const int track = c.w - kSliderKnobW;
    if (span <= 0 || track <= 0) return c.sliderMin;
    int t = px - c.x - kSliderKnobW / 2;        // o cursor segue o seu centro
    if (t < 0) t = 0;
    if (t > track) t = track;
    return c.sliderMin + (t * span + track / 2) / track;
}

void ShellMenu::sliderSet(Control& c, int value) {
    if (!c.sliderTarget) return;
    if (value < c.sliderMin) value = c.sliderMin;
    if (value > c.sliderMax) value = c.sliderMax;
    if (*c.sliderTarget == value) return;
    *c.sliderTarget = value;
    // O volume mestre vale na hora, senao nao haveria como ouvir o que se
    // escolheu (so o ACCEPT grava — ver setState e savePrefs).
    if (c.id == 1201) soundSetMasterVolume(value);
}

void ShellMenu::setState(MenuState s) {
    // Sair do PREFERENCES sem ACCEPT devolve o som ao volume gravado.
    if (m_state == MenuState::Preferences && s != MenuState::Preferences)
        soundSetMasterVolume(m_prefAudioVolumeIni);
    m_dragSlider = -1;
    m_state = s;
    m_openComboId = 0;         // trocar de tela fecha qualquer lista solta
    m_comboHover = -1;
    m_comboScroll = 0;
    if (s == MenuState::Splash) m_splashTimer = 0.0f;
    rebuildControls();
}

void ShellMenu::rebuildControls() {
    // 30% de transparencia no menu principal; o resto opaco.
    m_uiOpacity = (m_state == MenuState::MainMenu) ? m_menuAlpha : 255;
    m_controls.clear();
    m_focusIndex = -1;
    m_openCombo = -1;          // reencontrado pelo id no fim, se ainda existir

    switch (m_state) {
    case MenuState::MainMenu: {
        // '&' marca a letra sublinhada (como no template do diálogo original).
        struct Def { int id; const char* label; bool dis; } defs[] = {
            {1014, "&TUTORIALS", false}, {1000, "&NEW CAMPAIGN", false},
            {1006, "&COLONY GAMES", false}, {1005, "&MULTIPLAYER", false},
            {1007, "&LOAD GAME", false}, {1011, "&GAME PREFERENCES", false},
            {1012, "&HELP", false}, {1010, "&ABOUT OUTPOST 2", false},
            {1013, "&QUIT", false}
        };
        for (int i = 0; i < 9; ++i)
            addButton(defs[i].id, defs[i].label, kMenuBtnX,
                      kMenuDlgY + kMenuBtnTop[i], kMenuBtnW, kMenuBtnH, false, defs[i].dis);
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
    // Dialog 224 "ENTER YOUR NAME" (224x100 DLU): Edit 1093 (100,49,117,16) e
    // o botão &OK (60,79,103,14).
    case MenuState::EnterName: {
        addButton(1, "OK", kEnX + 90, kEnY + 22 + 79 * kEnH / 100, 154, 24, true);
        break;
    }
    // Dialog 155: so o &OK (104,61,90,14).
    case MenuState::MpStats: {
        addButton(1, "OK", kMsX + (kMsW - 135) / 2, kMsY + 22 + 61 * 8 / 5, 135, 24, true);
        break;
    }
    // Dialog 223 "COLONY GAME SCORES" (376x170): ListBox 1092 (7,7,362,133),
    // OK (207,149,103,14) e CLEAR SCORES (59,149,103,14).
    case MenuState::Scores: {
        if (m_clearConfirm) {                              // "Delete All Scores?"
            addButton(1, "YES", kScX + kScW / 2 - 170, kScBtnY, 154, 24);
            addButton(2, "NO", kScX + kScW / 2 + 16, kScBtnY, 154, 24, true);
            break;
        }
        addButton(2, "CLEAR SCORES", kScX + 88, kScBtnY, 154, 24);
        addButton(1, "OK", kScX + 310, kScBtnY, 154, 24, true);
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
    case MenuState::MissionMenu: {
        // Dialog 219: botões 1091/1090/1005/1095 e 1094 (LEAVE CAMPAIGN).
        // O template manda 90 DLU de largura (= 135 px na conversão do shell),
        // que bastam para a MS Sans Serif 8 do original mas NÃO para a nossa
        // fonte de 8 px fixos: "SPECIFIC OBJECTIVES" pede 152 px e vazava do
        // botão. Alargamos para 160 e recentramos no quadro (190 − 160) / 2.
        const int bw = 160, x = kMmX + (kMmW - bw) / 2, y0 = kMmY + 30;
        addButton(1091, "MISSION BRIEFING", x, y0, bw, 23);
        addButton(1090, "SPECIFIC OBJECTIVES", x, y0 + 32, bw, 23);
        addButton(1005, "STORY", x, y0 + 64, bw, 23);
        addButton(1095, "START MISSION", x, y0 + 96, bw, 23, true);
        addButton(1094, "LEAVE CAMPAIGN", x, y0 + 144, bw, 23);
        break;
    }
    case MenuState::RtfView: {
        // Botões 1082 (rótulo trocado conforme a tela) e 1089.
        const char* b1 = m_rtfKind == 0 ? "OBJECTIVES" : m_rtfKind == 1 ? "BRIEFING" : "SELECT CHAPTER";
        const char* b2 = (m_rtfKind == 2 && m_gsi.missionNumber == 12) ? "MAIN MENU" : "MISSION MENU";
        addButton(1082, b1, kRvX + 158, kRvBtnY, 135, 23);
        addButton(1089, b2, kRvX + 308, kRvBtnY, 135, 23, true);
        break;
    }
    case MenuState::Chapters: {
        Control lc; lc.kind = CtrlKind::List; lc.id = 1081;
        lc.x = kChListX; lc.y = kChListY; lc.w = kChListW; lc.h = kChListH;
        lc.listSel = &m_selChapter; lc.listCount = static_cast<int>(m_chapters.size());
        m_controls.push_back(lc);
        addButton(1080, "VIEW STORY", kChX + 78, kChListY + kChListH + 12, 135, 23, true);
        addButton(1089, m_gsi.missionNumber == 12 ? "MAIN MENU" : "MISSION MENU", kChX + 237,
                  kChListY + kChListH + 12, 135, 23);
        break;
    }
    case MenuState::Preferences: {
        loadPrefs();
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
        // Velocidades: os botoes LESS/MORE do Dialog 142, com os ids do
        // original (1095/1099 jogo, 1097/1101 rolagem). O rotulo e o valor
        // saem no render.
        addButton(1095, "LESS", kPrX + 188, kPrY + 156, 56, 20);
        addButton(1099, "MORE", kPrX + 250, kPrY + 156, 56, 20);
        addButton(1097, "LESS", kPrX + 188, kPrY + 182, 56, 20);
        addButton(1101, "MORE", kPrX + 250, kPrY + 182, 56, 20);
        // Nivel da IA: acrescimo deste porte. O rotulo e desenhado no render.
        addCombo(1200, &kAiLevelNames, &m_prefAiLevel,
                 kPrX + 130, kPrY + 214, 110, 22);
        // Volume mestre: idem. O original tem LESS/MORE por canal (botoes 1096
        // e 1100 do Dialog 142); aqui e uma barra de 0 a 100 por cento.
        addSlider(1201, &m_prefAudioVolume, 0, 100,
                  kPrX + 24, kPrY + 290, kPrW - 48, 16);
        // Modo de tela: idem, so deste porte. Par de radio, como os de colonia
        // e dificuldade do NEW CAMPAIGN -- o clique escolhe a opcao em que se
        // clicou. O rotulo "Screen Mode:" sai no render.
        {
            Control c;
            c.kind = CtrlKind::Radio; c.id = 1202; c.text = "Full Screen";
            c.x = kPrX + 130; c.y = kPrY + 318; c.w = 100; c.h = 14;
            c.radioTarget = &m_prefFullscreen; c.radioValue = 1;
            m_controls.push_back(c);
            c.id = 1203; c.text = "Window"; c.y = kPrY + 336; c.radioValue = 0;
            m_controls.push_back(c);
        }
        m_prefSyncScreen = true;        // o render acerta com a janela de verdade
        addButton(1, "ACCEPT", kPrX + (kPrW - 100) / 2, kPrY + 356, 100, 26, true);
        break;
    }
    case MenuState::About:
        addButton(1, "&OK", kAbX + (kAbW - 100) / 2, kAbY + 258, 100, 26, true);
        break;
    case MenuState::Help: {
        Control lc; lc.kind = CtrlKind::List; lc.id = 1090;
        lc.x = kHpListX; lc.y = kHpListY; lc.w = kHpListW - kHpSbW; lc.h = kHpListH;
        lc.listSel = &m_helpTopic;
        lc.listCount = m_help.ok() ? (int)m_help.topics().size() : 0;
        m_controls.push_back(lc);
        addButton(2, "BACK TO MAIN MENU", kHpX + (kHpW - 180) / 2,
                  kHpY + kHpH - 40, 180, 26, true);
        break;
    }
    // --- multijogador -------------------------------------------------------
    // A geometria sai dos templates do OUT2RES.DLL, em unidades de diálogo:
    // cada controle fica onde o original o põe, e o DlgBox faz a conversão.
    case MenuState::MpNetOptions: {
        const DlgBox d = dlgBox(124, 66);               // Dialog 260
        addButton(1074, "CREATE", d.x(37), d.y(4),  d.w(50), d.h(14), true);
        addButton(1075, "JOIN",   d.x(37), d.y(24), d.w(50), d.h(14));
        addButton(2,    "CANCEL", d.x(37), d.y(44), d.w(50), d.h(14));
        break;
    }
    case MenuState::MpNewGame: {
        const DlgBox d = dlgBox(284, 122);              // Dialog 2012
        addEdit(1044, &m_mpPlayerName, 11, d.x(76), d.y(8),  d.w(93), d.h(14));
        addEdit(1037, &m_mpPassword,   11, d.x(76), d.y(26), d.w(93), d.h(14));
        addCombo(1015, &kMaxPlayerNames,  &m_mpMaxPlayers, d.x(76), d.y(44), d.w(32), d.h(14));
        addCombo(1039, &m_mpScenarioList, &m_mpScenario,   d.x(8),  d.y(79), d.w(268), d.h(14));
        addButton(1, "OK",     d.x(187), d.y(8),  d.w(90), d.h(14), true);
        addButton(2, "CANCEL", d.x(187), d.y(26), d.w(90), d.h(14));
        break;
    }
    case MenuState::MpFindSession: {
        const DlgBox d = dlgBox(232, 98);               // Dialog 2010
        // O quadradinho do radio fica em x=11 e o rótulo em x=25; como o nosso
        // desenha "( ) " junto, o controle começa no quadradinho e o rótulo cai
        // quase no mesmo lugar.
        addRadio(1005, "Broadcasting on the local network.", d.x(11), d.y(22),
                 d.w(214), &m_mpFindMode, 0);
        addRadio(1007, "Looking at the IP address:", d.x(11), d.y(41),
                 d.w(214), &m_mpFindMode, 1);
        addEdit(1008, &m_mpFindAddr, 15, d.x(41), d.y(51), d.w(91), d.h(14));
        addButton(1, "OK",     d.x(30),  d.y(76), d.w(72), d.h(14), true);
        addButton(2, "CANCEL", d.x(130), d.y(76), d.w(72), d.h(14));
        break;
    }
    case MenuState::MpPickSession: {
        const DlgBox d = dlgBox(362, 126);              // Dialog 195
        Control lc; lc.kind = CtrlKind::List; lc.id = 1000;   // SysListView32
        lc.x = d.x(7); lc.y = d.y(9); lc.w = d.w(348); lc.h = d.h(64);
        lc.listSel = &m_mpSelSession; lc.listCount = (int)m_mpSessions.size();
        m_controls.push_back(lc);
        addButton(3, "REFRESH", d.x(148), d.y(104), d.w(64), d.h(14));
        addButton(2, "CANCEL",  d.x(220), d.y(104), d.w(64), d.h(14));
        addButton(1, "OK",      d.x(291), d.y(104), d.w(64), d.h(14), true);
        break;
    }
    case MenuState::MpJoinName: {
        const DlgBox d = dlgBox(226, 57);               // Dialog 2013
        addEdit(1035, &m_mpPlayerName, 11, d.x(67), d.y(15), d.w(132), d.h(14));
        addButton(1, "OK",     d.x(12),  d.y(36), d.w(90), d.h(14), true);
        addButton(2, "CANCEL", d.x(119), d.y(36), d.w(90), d.h(14));
        break;
    }
    case MenuState::MpGameSettings: {
        const DlgBox d = dlgBoxFull(316, 226);          // Dialog 196
        // O combo do cenario so existe para quem hospeda (Static 1075 para os
        // outros, que recebem a escolha do host).
        if (m_mpIsHost)
            addCombo(40032, &m_mpScenarioList, &m_mpScenario,
                     d.x(51), d.y(2), d.w(251), d.h(14));
        // As seis linhas de jogador: a tabela dword_4E2FA0 dá os ids
        // (40000+5i a 40004+5i) e o template a posição de cada coluna.
        for (int i = 0; i < 6; ++i) {
            const int rowC = 38 + 15 * i;    // combos e EJECT
            const int rowK = 39 + 15 * i;    // caixas de marcar
            const bool mine = (i == (netActive() ? netLocalPlayer() : (m_mpIsHost ? 0 : 1)));
            const bool live = i < m_mpPlayerCount;
            addCombo(40001 + 5 * i, &kColorNames, &m_mpColor[i],
                     d.x(96), d.y(rowC), d.w(46), d.h(14))
                .disabled = !(live && (mine || m_mpIsHost));
            addCheck(40003 + 5 * i, "", d.x(154), d.y(rowK), d.w(20), &m_mpEden[i])
                .disabled = !(live && (mine || m_mpIsHost));
            addCombo(40002 + 5 * i, &kResourceNames, &m_mpResources[i],
                     d.x(176), d.y(rowC), d.w(46), d.h(14))
                .disabled = !(live && (mine || m_mpIsHost));
            addCheck(40004 + 5 * i, "", d.x(231), d.y(rowK), d.w(20), &m_mpReady[i])
                .disabled = !(live && mine);
            addButton(1046 + i, "EJECT", d.x(252), d.y(rowC), d.w(52), d.h(14), false,
                      !(live && m_mpIsHost && i != 0));
        }
        // As opcoes globais (flags do GameStartInfo): só o host as muda. O
        // template as alinha supondo a fonte proporcional do Windows; como a
        // nossa é de largura fixa, a linha é remontada medindo cada rótulo,
        // a partir do mesmo x em que o original começa.
        {
            int cx = d.x(9);
            const int gap = 10, box = SkinText::width("[X] ");
            auto opt = [&](int id, const char* label, bool* tgt) {
                const int wTotal = box + SkinText::width(label);
                addCheck(id, label, cx, d.y(135), wTotal, tgt).disabled = !m_mpIsHost;
                cx += wTotal + gap;
            };
            opt(1077, "Disasters",     &m_mpDisasters);
            opt(1078, "Morale Steady", &m_mpMoraleSteady);
            opt(1079, "Day and Night", &m_mpDayNight);
            addCombo(1018, &kVehicleNames, &m_mpInitialVehicles,
                     cx, d.y(132), d.w(31), d.h(14)).disabled = !m_mpIsHost;
        }
        addEdit(40030, &m_mpChatInput, 60, d.x(36), d.y(189), d.w(272), d.h(14));
        addButton(1045, "START",  d.x(60),  d.y(207), d.w(68), d.h(14), true, !m_mpIsHost);
        addButton(1090, "CANCEL", d.x(183), d.y(207), d.w(68), d.h(14));
        break;
    }
    default:
        break;
    }

    // Se havia uma lista solta, ela continua aberta no controle de mesmo id —
    // a mesa se reconstrói sozinha quando alguém entra, e não é hora de fechar
    // a lista na cara de quem estava escolhendo.
    if (m_openComboId != 0) {
        for (int i = 0; i < (int)m_controls.size(); ++i)
            if (m_controls[i].id == m_openComboId &&
                m_controls[i].kind == CtrlKind::Combo && !m_controls[i].disabled) {
                m_openCombo = i;
                break;
            }
        if (m_openCombo < 0) { m_openComboId = 0; m_comboHover = -1; m_comboScroll = 0; }
        else { m_focusIndex = m_openCombo; return; }
    }

    // Foco inicial: nas telas de multijogador, o botão default do diálogo
    // (como o Windows faz); nas demais, o primeiro controle habilitado.
    if (m_state >= MenuState::MpNetOptions) {
        for (int i = 0; i < (int)m_controls.size(); ++i)
            if (m_controls[i].isDefault && !m_controls[i].disabled) { m_focusIndex = i; break; }
    }
    if (m_focusIndex < 0)
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
    // CBS_DROPDOWNLIST: solta a lista; a escolha sai em comboClose.
    case CtrlKind::Combo:
        comboOpen(index);
        return;
    case CtrlKind::Edit:   break;   // recebe texto por handleTextInput
    case CtrlKind::Slider: break;  // o valor muda no arrasto, nao no clique
    }
    // No lobby, mexer num controle avisa o outro lado.
    if (m_state == MenuState::MpGameSettings && c.kind == CtrlKind::Check)
        mpControlChanged(c.id);
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
        case 1007: m_loadRequested = true; break;            // LOAD GAME (TFileDialog)
        case 1005: showMultiplayer(); break;                 // TApp::DoNetGame
        case 1012: openHelp(); break;                        // TApp::GetHelp
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
            // O shell entrega a DLL da missão ao jogo (StartSingleGame → sub_4893A0).
            if (!map.empty()) { m_launchMap = dll; m_launchRequested = true; m_launchHasGsi = false; }
        } else if (id == 2) setState(MenuState::MainMenu);
        break;
    case MenuState::ColonyGames:
        if (id == 10) {                                  // PLAY: lança a c*.dll escolhida
            const int i = m_selectedColonyGame;
            if (i >= 0 && i < (int)m_colonyGameDlls.size()) {
                m_lastColonyDesc = m_colonyGameList[i];  // guardado p/ o placar
                m_launchMap = m_colonyGameDlls[i];
                m_launchRequested = true;
                m_launchHasGsi = false;
                std::cout << "[Colony] PLAY: " << m_launchMap << std::endl;
            }
        } else if (id == 11) openScores();               // SCORES (Dialog 223)
        else if (id == 12) setState(MenuState::MainMenu);
        break;
    case MenuState::MpStats:
        if (id == 1) setState(MenuState::MainMenu);
        break;
    case MenuState::EnterName:
        if (id == 1) {                                   // OK: grava a entrada e mostra o placar
            ScoreEntry e;
            e.game       = m_lastColonyDesc;
            e.difficulty = scoreDifficultyName(m_gsi.playerWord[0]);
            e.time       = m_scoreMarks;
            e.name       = m_scoreName;
            e.date       = scoreToday();
            scoresAdd(e);
            openScores();
        }
        break;
    case MenuState::Scores:
        if (m_clearConfirm) {                            // "Delete All Scores?"
            if (id == 1) { scoresClear(); m_scores.clear(); m_scoreScroll = 0; }
            m_clearConfirm = false;
            rebuildControls();
        } else if (id == 2) {                            // CLEAR SCORES → confirmação
            m_clearConfirm = true;
            rebuildControls();
        } else if (id == 1) {
            setState(MenuState::MainMenu);
        }
        break;
    case MenuState::NewCampaign:
        if (id == 1) {   // OK (0x13002fd5): zera a estrutura, campanha, colônia e dificuldade
            m_gsi = GameStartInfo{};
            m_gsi.missionNumber = 0;                         // +0x16A
            m_gsi.flags |= 8;
            uint32_t w = 0;
            if (m_campaignColony == 0) w |= 4;               // Eden; Plymouth: cor 1 (bit 5)
            else w = (w & ~0xE4u) | 0x20u;
            if (m_campaignDifficulty == 2) w = (w & ~8u) | 0x10u;
            else if (m_campaignDifficulty == 0) w &= ~0x18u;
            else w = (w & ~0x10u) | 8u;
            m_gsi.playerWord[0] = w;
            std::cout << "[Campaign] " << colonyName() << " dif " << m_campaignDifficulty << std::endl;
            setState(MenuState::MissionMenu);                // EndDialog(0x73b)
        } else if (id == 2) setState(MenuState::MainMenu);
        break;
    case MenuState::MissionMenu:
        switch (id) {
        case 1090: openRtf(1); break;                        // 0x73c
        case 1091: openRtf(0); break;                        // 0x73d
        case 1005: openChapters(); break;                    // 0x735
        case 1095: {                                         // 0x733: StartSingleGame
            char dll[16];
            std::snprintf(dll, sizeof(dll), m_gsi.eden() ? "E%02d.DLL" : "P%02d.DLL", m_gsi.missionNumber + 1);
            m_gsi.dll = dll;                                 // sub_4893A0
            if (!missionMapName(dll).empty()) {
                m_launchMap = dll;
                m_launchRequested = true;
                m_launchHasGsi = true;
                m_launchGsi = m_gsi;
            }
            break;
        }
        case 1094: setState(MenuState::MainMenu); break;     // 0x720
        default: break;
        }
        break;
    case MenuState::RtfView:
        if (id == 1082) {
            if (m_rtfKind == 0) openRtf(1);                  // briefing → objetivos (0x73c)
            else if (m_rtfKind == 1) openRtf(0);             // objetivos → briefing (0x73d)
            else openChapters();                             // história → capítulos (0x735)
        } else if (id == 1089) {
            if (m_rtfKind == 2 && m_gsi.missionNumber == 12) setState(MenuState::MainMenu);
            else setState(MenuState::MissionMenu);
        }
        break;
    case MenuState::Chapters:
        if (id == 1080) {                                    // VIEW STORY: item data = capítulo
            if (m_selChapter >= 0 && m_selChapter < static_cast<int>(m_chapters.size())) {
                m_storyChapter = m_selChapter + 1;
                openRtf(2);                                  // 0x737
            }
        } else if (id == 1089) {
            if (m_gsi.missionNumber == 12) setState(MenuState::MainMenu);
            else setState(MenuState::MissionMenu);
        }
        break;
    case MenuState::Preferences:
        // LESS/MORE das velocidades. As faixas sao as do original: o jogo anda
        // de 1 a 10 (sub_45F0D0/sub_466100) e a rolagem de 1 a 6 (sub_49ACA0),
        // e o botao simplesmente nao passa do limite, como la.
        if (id == 1095 || id == 1099) {
            m_prefGameSpeed += (id == 1099) ? 1 : -1;
            if (m_prefGameSpeed < 1) m_prefGameSpeed = 1;
            if (m_prefGameSpeed > 10) m_prefGameSpeed = 10;
            break;
        }
        if (id == 1097 || id == 1101) {
            m_prefScrollRate += (id == 1101) ? 1 : -1;
            if (m_prefScrollRate < 1) m_prefScrollRate = 1;
            if (m_prefScrollRate > 6) m_prefScrollRate = 6;
            break;
        }
        if (id == 1) { savePrefs(); setState(MenuState::MainMenu); }
        break;
    case MenuState::About:
        if (id == 1) setState(MenuState::MainMenu);
        break;
    case MenuState::Help:
        if (id == 2) setState(MenuState::MainMenu);
        break;

    // --- multijogador -------------------------------------------------------
    case MenuState::MpNetOptions:
        if (id == 1074) {                       // CREATE
            m_mpIsHost = true;
            setState(MenuState::MpNewGame);
        } else if (id == 1075) {                // JOIN
            m_mpIsHost = false;
            setState(MenuState::MpFindSession);
        } else if (id == 2) {
            // Sem o 259 no caminho, o CANCEL daqui volta ao menu principal.
            setState(MenuState::MainMenu);
        }
        break;
    case MenuState::MpNewGame:
        if (id == 1) {
            m_mpSessionName = m_mpPlayerName.empty() ? "OUTPOST 2" : m_mpPlayerName;
            if (m_mpScenarioList.empty()) {
                m_mpStatus = "Nenhum cenario M*.DLL encontrado";
                break;
            }
            if (mpStartHost()) setState(MenuState::MpGameSettings);
        } else if (id == 2) {
            setState(MenuState::MpNetOptions);
        }
        break;
    case MenuState::MpFindSession:
        if (id == 1) {
            if (m_mpFindMode == 1 && m_mpFindAddr.empty()) {
                m_mpStatus = "Digite o endereco do host";
                break;
            }
            mpFindSessions();
            setState(MenuState::MpPickSession);
        } else if (id == 2) {
            setState(MenuState::MpNetOptions);
        }
        break;
    case MenuState::MpPickSession:
        if (id == 3) {                          // REFRESH
            mpFindSessions();
            rebuildControls();
        } else if (id == 1) {
            if (m_mpSessions.empty()) { m_mpStatus = "Nenhuma sessao para entrar"; break; }
            setState(MenuState::MpJoinName);
        } else if (id == 2) {
            setState(MenuState::MpFindSession);
        }
        break;
    case MenuState::MpJoinName:
        if (id == 1) {
            if (mpJoinSelected()) setState(MenuState::MpGameSettings);
        } else if (id == 2) {
            setState(MenuState::MpPickSession);
        }
        break;
    case MenuState::MpGameSettings:
        if (id == 1045) {                       // START
            if (m_mpScenario < 0 || m_mpScenario >= (int)m_mpScenarioDlls.size()) {
                m_mpStatus = "Escolha um cenario";
                break;
            }
            if (m_mpPlayerCount < 2) {
                m_mpStatus = "Ninguem mais na mesa";
                break;
            }
            // Avisa os outros antes de sair da tela, com repeticao porque o
            // aviso e UDP e nao tem confirmacao.
            for (int k = 0; k < 5; ++k) lobbyMsgSend(kLobbyMsgStart, nullptr, 0);
            mpSyncGsi();
            m_launchGsi = m_mpGsi;        // o que launchStartInfo entrega
            m_launchHasGsi = true;
            m_netLaunchDll = m_mpScenarioDlls[m_mpScenario];
            m_netLaunch = true;
        } else if (id == 1090) {                // CANCEL
            lobbyHostStop();
            m_mpPlayerCount = 1;
            setState(MenuState::MainMenu);
        } else if (id >= 1046 && id <= 1051) {  // EJECT
            const int slot = id - 1046;
            if (!m_mpIsHost || slot == 0) break;
            for (int k = 0; k < 5; ++k) lobbyMsgSend(kLobbyMsgEject, nullptr, 0);
            lobbyHostEject(slot);
            m_mpNames[slot].clear();
            m_mpReady[slot] = false;
            m_mpPlayerCount = lobbyHostActiveCount();
            mpSendChat("Jogador " + std::to_string(slot + 1) + " removido");
            rebuildControls();
        }
        break;
    default: break;
    }
}

// --------------------------------- input ---------------------------------

void ShellMenu::noteActivity() { m_lastActivity = SDL_GetTicks(); }
void ShellMenu::resetIdleClock() { noteActivity(); }

// A mensagem 0x741 do shell (caso 33 da tabela em 0x13009028 do OP2SHELL.DLL).
// O original:
//   - zera o GameStartInfo guardado em 0x13015a34;
//   - le "[Game] Demo" do INI, que NAO e um liga-desliga mas um CONTADOR:
//     escolhe ademo{1,2,3}.dll por `Demo % 3` e regrava Demo+1, de modo que a
//     cada vez entra um demo diferente, inclusive entre execucoes;
//   - liga os bits 0 e 1 das opcoes (desastres e dia/noite) e desliga os bits
//     3 e 4 (campanha e multijogador);
//   - liga o bit 3 de +0x0C, a marca de demo;
//   - poe a velocidade: clamp("[Game] GameSpeed", 1, 10) * 4 — que neste porte
//     ja vem do mesmo lugar, em GameState::loadSettings;
//   - e chama TApp::StartSingleGame.
void ShellMenu::startAttractDemo() {
    Config cfg("OUTPOST2.INI");
    cfg.load();
    int contador = cfg.getInt("Game", "Demo", 0);
    if (contador < 0) contador = 0;
    char dll[32];
    std::snprintf(dll, sizeof(dll), "ademo%d.dll", contador % 3 + 1);
    cfg.setInt("Game", "Demo", contador + 1);
    cfg.save();

    m_launchGsi = GameStartInfo();
    m_launchGsi.flags |= 1u | 2u;          // desastres e ciclo dia/noite
    m_launchGsi.flags &= ~(8u | 0x10u);    // nem campanha nem multijogador
    m_launchGsi.demo = true;               // +0x0C bit 3
    m_launchGsi.dll = dll;
    m_launchHasGsi = true;
    m_launchMap = dll;
    m_launchRequested = true;
    noteActivity();
    std::cout << "[Shell] demo de atracao: " << dll << std::endl;
}

bool ShellMenu::demoTest(std::string& report) {
    Config antes("OUTPOST2.INI");
    antes.load();
    const int gravado = antes.getInt("Game", "Demo", 0);
    showMainMenu();
    std::string vistos;
    for (int k = 0; k < 3; ++k) {
        startAttractDemo();
        std::string dllName;
        if (!consumeLaunch(dllName)) { report += "nao pediu o lancamento"; return false; }
        GameStartInfo gsi;
        if (!launchStartInfo(gsi)) { report += "sem GameStartInfo"; return false; }
        if (!gsi.demo) { report += "sem a marca de demo"; return false; }
        if (gsi.campaign()) { report += "marcou campanha"; return false; }
        if ((gsi.flags & 0x10u) != 0) { report += "marcou multijogador"; return false; }
        if ((gsi.flags & 3u) != 3u) { report += "sem desastres/dia-noite"; return false; }
        vistos += (k ? ", " : "") + dllName;
    }
    if (vistos.find("ademo1.dll") == std::string::npos ||
        vistos.find("ademo2.dll") == std::string::npos ||
        vistos.find("ademo3.dll") == std::string::npos) {
        report += "a rotacao nao passou pelos tres: " + vistos;
        return false;
    }
    report += "rodou " + vistos;
    // Devolve o contador como estava, para o teste nao mexer no INI de verdade.
    Config volta("OUTPOST2.INI");
    volta.load();
    volta.setInt("Game", "Demo", gravado);
    volta.save();
    report += "; contador devolvido a " + std::to_string(gravado);
    return true;
}

bool ShellMenu::helpTest(std::string& report) {
    openHelp();
    if (m_state != MenuState::Help) { report += "nao abriu o manual"; return false; }
    if (!m_help.ok()) { report += "sem manual: " + m_help.error(); return false; }
    const int n = (int)m_help.topics().size();
    report += std::to_string(n) + " topicos";

    helpBuildLines();
    const size_t linhas0 = m_helpLines.size();
    if (linhas0 < 3) { report += "; o primeiro topico saiu com " +
                                 std::to_string(linhas0) + " linhas"; return false; }
    report += "; '" + m_help.topics()[0].title + "' em " + std::to_string(linhas0) + " linhas";

    // A seta desce na lista e o texto tem de ser remontado.
    m_focusIndex = 0;
    handleKeyDown(SDLK_DOWN);
    if (m_helpTopic != 1) { report += "; a seta nao andou na lista"; return false; }
    helpBuildLines();
    if (m_helpLines.empty()) { report += "; o segundo topico saiu vazio"; return false; }
    report += "; a seta foi para '" + m_help.topics()[1].title + "'";

    // A lista rola junto quando a selecao passa do fim da janela.
    for (int k = 0; k < kHpVisible + 3; ++k) handleKeyDown(SDLK_DOWN);
    if (m_helpListScroll <= 0) { report += "; a lista nao rolou"; return false; }
    if (m_helpTopic < m_helpListScroll ||
        m_helpTopic >= m_helpListScroll + kHpVisible) {
        report += "; o item escolhido saiu da janela"; return false;
    }
    report += "; a lista rolou ate " + std::to_string(m_helpListScroll);

    // A roda e o End andam pelo TEXTO, nao pela lista.
    const int topico = m_helpTopic;
    helpBuildLines();
    if ((int)m_helpLines.size() > helpVisibleLines()) {
        handleMouseWheel(-2);
        if (m_helpTextScroll <= 0) { report += "; a roda nao rolou o texto"; return false; }
        handleKeyDown(SDLK_END);
        const int fim = (int)m_helpLines.size() - helpVisibleLines();
        if (m_helpTextScroll != fim) { report += "; o End nao foi ao fim"; return false; }
        handleKeyDown(SDLK_HOME);
        if (m_helpTextScroll != 0) { report += "; o Home nao voltou"; return false; }
        report += "; roda/End/Home andaram no texto";
    }
    if (m_helpTopic != topico) { report += "; o texto mexeu na lista"; return false; }

    dispatchButton(2);
    if (m_state != MenuState::MainMenu) { report += "; o BACK nao voltou"; return false; }
    report += "; o BACK voltou ao menu";
    return true;
}

void ShellMenu::handleMouseMove(int x, int y) {
    // Arrastando uma barra, o cursor é dela até soltar o botão — inclusive
    // fora do controle, como o trackbar do Windows.
    if (m_dragSlider >= 0 && m_dragSlider < (int)m_controls.size()) {
        Control& c = m_controls[m_dragSlider];
        sliderSet(c, sliderValueAt(c, x));
        return;
    }
    // Com uma lista solta, só ela escuta o cursor (o combo do Windows captura
    // o mouse enquanto está aberto).
    if (m_openCombo >= 0) {
        const int item = comboItemAt(x, y);
        if (item >= 0) m_comboHover = item;
        for (auto& c : m_controls) c.hovered = false;
        return;
    }
    for (auto& c : m_controls)
        c.hovered = (!c.disabled && x >= c.x && x < c.x + c.w && y >= c.y && y < c.y + c.h);
}

void ShellMenu::handleMouseDown(int x, int y, uint8_t button) {
    if (button != SDL_BUTTON_LEFT) return;
    noteActivity();
    if (m_state == MenuState::Splash) return;

    // Lista solta aberta: o clique é dela. Dentro escolhe, fora fecha — e é
    // consumido, como no Windows.
    if (m_openCombo >= 0) {
        comboClose(comboItemAt(x, y));
        return;
    }

    // Setas da barra de rolagem da lista do manual.
    if (m_state == MenuState::Help && m_help.ok()) {
        const int sbX = kHpListX + kHpListW - kHpSbW;
        if (x >= sbX && x < sbX + kHpSbW && y >= kHpListY && y < kHpListY + kHpListH) {
            const int total = (int)m_help.topics().size();
            const int maxScroll = (total > kHpVisible) ? (total - kHpVisible) : 0;
            if (y < kHpListY + kHpSbW) m_helpListScroll = std::max(0, m_helpListScroll - 1);
            else if (y >= kHpListY + kHpListH - kHpSbW)
                m_helpListScroll = std::min(maxScroll, m_helpListScroll + 1);
            return;
        }
    }

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
    if (c.kind == CtrlKind::Slider) {
        // Um clique na barra leva o cursor para onde se clicou e começa o
        // arrasto; o som acompanha na hora.
        sliderSet(c, sliderValueAt(c, x));
        m_dragSlider = idx;
        c.pressed = true;
    } else if (c.kind == CtrlKind::List) {
        if (c.listSel) {   // seleciona o item sob o cursor
            int rowH = (m_state == MenuState::Tutorials) ? kTutRowH
                     : m_state == MenuState::Chapters ? kChRowH
                     : m_state == MenuState::Help ? kHpRowH
                     : m_state == MenuState::MpPickSession ? 15 : kCgRowH;
            int top  = (m_state == MenuState::Tutorials) ? (c.y + 4)
                     : m_state == MenuState::Help ? (c.y + 1)
                     : m_state == MenuState::MpPickSession ? (c.y + 22) : (c.y + 8);
            int scroll = (m_state == MenuState::Tutorials) ? m_tutorialScroll
                       : m_state == MenuState::Help ? m_helpListScroll : 0;
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
    if (m_dragSlider >= 0) {
        // Soltar a barra toca o clique da interface (som 0x57), que é o que se
        // ouve no volume recém-escolhido.
        if (m_dragSlider < (int)m_controls.size()) {
            m_controls[m_dragSlider].pressed = false;
            if (m_controls[m_dragSlider].id == 1201) soundPlay(0x57);
        }
        m_dragSlider = -1;
        return;
    }
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
    noteActivity();
    if (m_openCombo >= 0 && m_openCombo < (int)m_controls.size()) {
        const Control& c = m_controls[m_openCombo];
        int x, y, w, h;
        if (comboRect(c, x, y, w, h)) {
            const int n = (int)c.comboItems->size();
            const int vis = (h - 4) / kComboItemH;
            m_comboScroll -= delta;
            if (m_comboScroll > n - vis) m_comboScroll = n - vis;
            if (m_comboScroll < 0) m_comboScroll = 0;
        }
        return;
    }
    // A roda sobre a barra (ou com ela focada) anda um degrau, como no Windows.
    for (int i = 0; i < (int)m_controls.size(); ++i) {
        Control& c = m_controls[i];
        if (c.kind != CtrlKind::Slider || c.disabled) continue;
        if (!c.hovered && i != m_focusIndex) continue;
        if (!c.sliderTarget) continue;
        sliderSet(c, *c.sliderTarget + delta * kSliderStep);
        return;
    }
    // No manual, a roda rola o TEXTO (é o que se está lendo).
    if (m_state == MenuState::Help) {
        const int maxTxt = std::max(0, (int)m_helpLines.size() - helpVisibleLines());
        m_helpTextScroll = std::max(0, std::min(maxTxt, m_helpTextScroll - delta * 3));
        return;
    }
    if (m_state == MenuState::RtfView) {
        const int maxScroll = std::max(0, static_cast<int>(m_rtfLines.size()) - rtfVisibleLines());
        m_rtfScroll = std::max(0, std::min(maxScroll, m_rtfScroll - delta * 3));
        return;
    }
    if (m_state == MenuState::Tutorials) {
        int total = (int)m_tutorialList.size();
        int maxScroll = (total > kTutVisible) ? (total - kTutVisible) : 0;
        m_tutorialScroll = std::max(0, std::min(maxScroll, m_tutorialScroll - delta));
    }
}

void ShellMenu::handleTextInput(const char* text) {
    if (!text) return;
    // Num combo — solto ou fechado — a letra salta para o item seguinte que
    // comece por ela, como no Windows. Procura em ciclo a partir do atual.
    const int combo = (m_openCombo >= 0) ? m_openCombo : m_focusIndex;
    if (combo >= 0 && combo < (int)m_controls.size() &&
        m_controls[combo].kind == CtrlKind::Combo && !m_controls[combo].disabled) {
        Control& c = m_controls[combo];
        if (c.comboItems && !c.comboItems->empty() && text[0] >= 0x20) {
            const char want = char(std::tolower((unsigned char)text[0]));
            const int n = (int)c.comboItems->size();
            const int from = (m_openCombo >= 0) ? m_comboHover
                           : (c.comboSel ? *c.comboSel : 0);
            for (int k = 1; k <= n; ++k) {
                const int item = (from + k % n + n) % n;
                const std::string& t = (*c.comboItems)[item];
                if (t.empty()) continue;
                if (char(std::tolower((unsigned char)t[0])) != want) continue;
                if (m_openCombo >= 0) {
                    m_comboHover = item;
                    int x, y, w, h;
                    const int vis = comboRect(c, x, y, w, h) ? (h - 4) / kComboItemH : 1;
                    if (item < m_comboScroll) m_comboScroll = item;
                    else if (item >= m_comboScroll + vis) m_comboScroll = item - vis + 1;
                } else if (c.comboSel && *c.comboSel != item) {
                    *c.comboSel = item;
                    if (m_state == MenuState::MpGameSettings) mpControlChanged(c.id);
                }
                break;
            }
            return;
        }
    }
    // Qualquer Edit com foco recebe o texto (os campos de nome, senha, endereço
    // e a linha de chat do GAME SETTINGS).
    if (m_focusIndex >= 0 && m_focusIndex < (int)m_controls.size()) {
        Control& c = m_controls[m_focusIndex];
        if (c.kind == CtrlKind::Edit && c.editTarget && !c.disabled) {
            for (const char* p = text; *p; ++p)
                if ((c.editMax == 0 || c.editTarget->size() < c.editMax) &&
                    static_cast<unsigned char>(*p) >= 0x20)
                    *c.editTarget += *p;
            return;
        }
    }
    if (m_state != MenuState::EnterName) return;
    for (const char* p = text; *p; ++p)
        if (m_scoreName.size() < 20 && static_cast<unsigned char>(*p) >= 0x20) m_scoreName += *p;
}

void ShellMenu::handleKeyDown(int keyCode) {
    if (m_state == MenuState::Splash) return;
    noteActivity();
    SDL_Keymod mod = SDL_GetModState();

    // Com a lista solta no ar ela é modal, como o combo do Windows: as setas
    // andam pelos itens, Enter/Espaço escolhem, Esc e Tab desistem.
    if (m_openCombo >= 0 && m_openCombo < (int)m_controls.size()) {
        const Control& c = m_controls[m_openCombo];
        const int n = c.comboItems ? (int)c.comboItems->size() : 0;
        int x, y, w, h, vis = 1;
        if (comboRect(c, x, y, w, h)) vis = (h - 4) / kComboItemH;
        switch (keyCode) {
        case SDLK_UP:
        case SDLK_DOWN:
        case SDLK_PAGEUP:
        case SDLK_PAGEDOWN:
        case SDLK_HOME:
        case SDLK_END: {
            int sel = m_comboHover;
            if (keyCode == SDLK_UP)        sel -= 1;
            else if (keyCode == SDLK_DOWN) sel += 1;
            else if (keyCode == SDLK_PAGEUP)   sel -= vis;
            else if (keyCode == SDLK_PAGEDOWN) sel += vis;
            else if (keyCode == SDLK_HOME) sel = 0;
            else                           sel = n - 1;
            if (sel < 0) sel = 0;
            if (sel > n - 1) sel = n - 1;
            m_comboHover = sel;
            if (sel < m_comboScroll) m_comboScroll = sel;
            else if (sel >= m_comboScroll + vis) m_comboScroll = sel - vis + 1;
            return;
        }
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE:
            comboClose(m_comboHover);
            return;
        default:
            // As letras e os números saltam para o item: quem trata é o
            // handleTextInput, que recebe o caractere de verdade.
            if ((keyCode >= SDLK_a && keyCode <= SDLK_z) ||
                (keyCode >= SDLK_0 && keyCode <= SDLK_9)) return;
            comboClose(-1);     // qualquer outra tecla desiste da escolha
            return;
        }
    }

    if (keyCode == SDLK_BACKSPACE) {
        if (m_focusIndex >= 0 && m_focusIndex < (int)m_controls.size()) {
            Control& c = m_controls[m_focusIndex];
            if (c.kind == CtrlKind::Edit && c.editTarget && !c.disabled) {
                if (!c.editTarget->empty()) c.editTarget->pop_back();
                return;
            }
        }
        if (m_state == MenuState::EnterName) {
            if (!m_scoreName.empty()) m_scoreName.pop_back();
            return;
        }
    }
    // Enter na linha de chat do GAME SETTINGS manda a linha (Edit 40030).
    if ((keyCode == SDLK_RETURN || keyCode == SDLK_KP_ENTER) &&
        m_state == MenuState::MpGameSettings &&
        m_focusIndex >= 0 && m_focusIndex < (int)m_controls.size() &&
        m_controls[m_focusIndex].kind == CtrlKind::Edit &&
        m_controls[m_focusIndex].id == 40030) {
        if (!m_mpChatInput.empty()) {
            const int me = netActive() ? netLocalPlayer() : (m_mpIsHost ? 0 : 1);
            const std::string who = m_mpNames[me].empty() ? "EU" : m_mpNames[me];
            mpSendChat(who + ": " + m_mpChatInput);
            m_mpChatInput.clear();
        }
        return;
    }

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
    case SDLK_DOWN:
    // Esquerda/direita andam o mesmo que cima/baixo: é o que o Windows faz
    // numa barra, num combo fechado e num grupo de radios.
    case SDLK_LEFT:
    case SDLK_RIGHT: {
        if (m_focusIndex < 0) break;
        Control& c = m_controls[m_focusIndex];
        int dir = (keyCode == SDLK_DOWN || keyCode == SDLK_RIGHT) ? +1 : -1;
        if (c.kind == CtrlKind::Slider && c.sliderTarget && !c.disabled) {
            sliderSet(c, *c.sliderTarget + dir * kSliderStep);
            if (c.id == 1201) soundPlay(0x57);
        } else if (c.kind == CtrlKind::List && c.listSel && c.listCount > 0) {
            int s = std::max(0, std::min(c.listCount - 1, *c.listSel + dir));
            *c.listSel = s;
            if (m_state == MenuState::Tutorials) {   // manter visível
                if (s < m_tutorialScroll) m_tutorialScroll = s;
                else if (s >= m_tutorialScroll + kTutVisible) m_tutorialScroll = s - kTutVisible + 1;
            } else if (m_state == MenuState::Help) {
                if (s < m_helpListScroll) m_helpListScroll = s;
                else if (s >= m_helpListScroll + kHpVisible) m_helpListScroll = s - kHpVisible + 1;
            }
        } else if (c.kind == CtrlKind::Combo && c.comboSel && c.comboItems &&
                   !c.comboItems->empty() && !c.disabled) {
            // Combo fechado: a seta troca a opção sem soltar a lista.
            const int n = (int)c.comboItems->size();
            int sel = *c.comboSel + dir;
            if (sel < 0) sel = 0;
            if (sel > n - 1) sel = n - 1;
            if (sel != *c.comboSel) {
                *c.comboSel = sel;
                if (m_state == MenuState::MpGameSettings) mpControlChanged(c.id);
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

    case SDLK_F1:                       // WM_HELP -> TApp::GetHelp
        if (m_state != MenuState::Help) openHelp();
        break;

    case SDLK_PAGEUP:
    case SDLK_PAGEDOWN:
    case SDLK_HOME:
    case SDLK_END:
        // No manual, Page/Home/End andam pelo TEXTO.
        if (m_state == MenuState::Help) {
            const int vis = helpVisibleLines();
            const int maxTxt = std::max(0, (int)m_helpLines.size() - vis);
            if (keyCode == SDLK_PAGEUP)        m_helpTextScroll -= vis - 1;
            else if (keyCode == SDLK_PAGEDOWN) m_helpTextScroll += vis - 1;
            else if (keyCode == SDLK_HOME)     m_helpTextScroll = 0;
            else                               m_helpTextScroll = maxTxt;
            m_helpTextScroll = std::max(0, std::min(maxTxt, m_helpTextScroll));
            break;
        }
        if (m_focusIndex >= 0 && m_focusIndex < (int)m_controls.size() &&
            m_controls[m_focusIndex].kind == CtrlKind::Slider) {
            Control& c = m_controls[m_focusIndex];
            if (c.sliderTarget && !c.disabled) {
                int v = *c.sliderTarget;
                if (keyCode == SDLK_PAGEUP)        v -= kSliderPage;
                else if (keyCode == SDLK_PAGEDOWN) v += kSliderPage;
                else if (keyCode == SDLK_HOME)     v = c.sliderMin;
                else                               v = c.sliderMax;
                sliderSet(c, v);
                if (c.id == 1201) soundPlay(0x57);
            }
            break;
        }
        if (m_state == MenuState::RtfView)
            handleMouseWheel(keyCode == SDLK_PAGEUP ? rtfVisibleLines() / 3 : -(rtfVisibleLines() / 3));
        break;

    case SDLK_ESCAPE:
        if (m_state == MenuState::RtfView || m_state == MenuState::Chapters) setState(MenuState::MissionMenu);
        else if (m_state != MenuState::MainMenu) setState(MenuState::MainMenu);
        else m_quitRequested = true;
        break;
    }
}

void ShellMenu::update(float dt) {
    // Um minuto parado no menu principal chama o demo de atracao (0xEA60 ms no
    // OP2SHELL.DLL). So no menu principal: com um dialogo aberto o original
    // nem chega a essa conta.
    if (m_state == MenuState::MainMenu && !m_launchRequested) {
        if (m_lastActivity == 0) noteActivity();
        else if (SDL_GetTicks() - m_lastActivity > 60000u) startAttractDemo();
    } else {
        noteActivity();
    }
    // Enquanto o GAME SETTINGS esta no ar, o host continua respondendo as
    // consultas de descoberta e aos pedidos de entrada.
    if (m_state == MenuState::MpGameSettings) {
        const int before = m_mpPlayerCount;
        mpPump();
        if (m_mpPlayerCount != before) rebuildControls();
        if (m_mpEjected) {               // o host nos tirou da mesa
            m_mpEjected = false;
            lobbyHostStop();
            m_mpPlayerCount = 1;
            setState(MenuState::MainMenu);
        }
    }
    if (m_state == MenuState::Splash) {
        m_splashTimer += dt;
        if (m_splashTimer > 2.0f) setState(MenuState::MainMenu);
    }
}

// --------------------------------- render ---------------------------------

void ShellMenu::render(Renderer& renderer) {
    // O par de rádio do modo de tela: o Alt+Enter troca a janela sem gravar o
    // INI, então ao abrir a tela o rádio é acertado com a janela DE VERDADE; e
    // o ACCEPT, que não tem Renderer na mão, deixa o pedido marcado para aqui.
    if (m_prefSyncScreen) {
        m_prefSyncScreen = false;
        m_prefFullscreen = renderer.fullscreen() ? 1 : 0;
    }
    if (m_prefApplyScreen) {
        m_prefApplyScreen = false;
        if (renderer.fullscreen() != (m_prefFullscreen != 0))
            renderer.setFullscreen(m_prefFullscreen != 0);
    }
    if (m_bgBitmap.getWidth() > 0)
        renderer.blitBitmap(m_bgBitmap, 0, 0);
    else
        renderer.clear(0xFF000029);

    if (m_state == MenuState::Splash) {
        renderSplash(renderer);
        return;
    }

    // Logo no topo do menu, bem translúcido (50/255 = 20% opaco), para a arte
    // da nave e as estrelas passarem por trás da placa dele. A placa é cinza,
    // não preta, então a chave de cor (preto) não a recorta — quem a dissolve
    // é a opacidade. As telas de multijogador ocupam o quadro
    // todo, então ficam sem ele.
    const bool mpScreen = m_state >= MenuState::MpNetOptions;
    if (m_logoBitmap.getWidth() > 0 && !mpScreen)
        renderer.blitBitmapAlpha(m_logoBitmap, 48, 16, m_logoAlpha, true, 0xFF000000);

    switch (m_state) {
    case MenuState::MainMenu:    renderMainMenu(renderer); break;
    case MenuState::NewCampaign: renderNewCampaignDialog(renderer); break;
    case MenuState::ColonyGames: renderColonyGamesDialog(renderer); break;
    case MenuState::EnterName: renderEnterName(renderer); break;
    case MenuState::MpStats: renderMpStats(renderer); break;
    case MenuState::Scores: renderScores(renderer); break;
    case MenuState::Tutorials:   renderTutorialsDialog(renderer); break;
    case MenuState::Help:        renderHelp(renderer); break;
    case MenuState::Preferences: renderPreferencesDialog(renderer); break;
    case MenuState::About:       renderAboutDialog(renderer); break;
    case MenuState::MissionMenu: renderMissionMenu(renderer); break;
    case MenuState::RtfView:     renderRtfView(renderer); break;
    case MenuState::Chapters:    renderChapters(renderer); break;
    case MenuState::MpNetOptions:   renderMpNetOptions(renderer); break;
    case MenuState::MpNewGame:      renderMpNewGame(renderer); break;
    case MenuState::MpFindSession:  renderMpFindSession(renderer); break;
    case MenuState::MpPickSession:  renderMpPickSession(renderer); break;
    case MenuState::MpJoinName:     renderMpJoinName(renderer); break;
    case MenuState::MpGameSettings: renderMpGameSettings(renderer); break;
    default: break;
    }

    // Controles (botões/radios/checks) + destaque de hover e foco, por cima.
    drawControls(renderer);
}

// ---------------------------------------------------------------------------
// A lista suspensa dos combos. O original usa CBS_DROPDOWNLIST, o combo do
// Windows que não aceita digitação: um clique solta a lista por cima de tudo,
// o item escolhido fecha, e com a lista fechada as setas trocam a opção direto.

bool ShellMenu::comboRect(const Control& c, int& x, int& y, int& w, int& h) const {
    if (c.kind != CtrlKind::Combo || !c.comboItems || c.comboItems->empty())
        return false;
    const int n = (int)c.comboItems->size();
    const int vis = (n < kComboMaxVis) ? n : kComboMaxVis;
    w = c.w;
    h = vis * kComboItemH + 4;
    x = c.x;
    y = c.y + c.h;                       // o Windows solta a lista para baixo
    if (y + h > 478) y = c.y - h;        // sem espaço: abre para cima
    if (y < 2) y = 2;
    return true;
}

int ShellMenu::comboItemAt(int px, int py) const {
    if (m_openCombo < 0 || m_openCombo >= (int)m_controls.size()) return -1;
    const Control& c = m_controls[m_openCombo];
    int x, y, w, h;
    if (!comboRect(c, x, y, w, h)) return -1;
    if (px < x || px >= x + w || py < y + 2 || py >= y + h - 2) return -1;
    const int idx = m_comboScroll + (py - y - 2) / kComboItemH;
    return (idx >= 0 && idx < (int)c.comboItems->size()) ? idx : -1;
}

void ShellMenu::comboOpen(int index) {
    if (index < 0 || index >= (int)m_controls.size()) return;
    const Control& c = m_controls[index];
    if (c.disabled || !c.comboItems || c.comboItems->empty()) return;
    m_openCombo = index;
    m_openComboId = c.id;
    m_comboHover = (c.comboSel && *c.comboSel >= 0 &&
                    *c.comboSel < (int)c.comboItems->size()) ? *c.comboSel : 0;
    // Abre já mostrando a opção corrente.
    const int n = (int)c.comboItems->size();
    const int vis = (n < kComboMaxVis) ? n : kComboMaxVis;
    m_comboScroll = m_comboHover - vis / 2;
    if (m_comboScroll > n - vis) m_comboScroll = n - vis;
    if (m_comboScroll < 0) m_comboScroll = 0;
}

void ShellMenu::comboClose(int chosen) {
    const int index = m_openCombo;
    m_openCombo = -1;
    m_openComboId = 0;
    m_comboHover = -1;
    m_comboScroll = 0;
    if (index < 0 || index >= (int)m_controls.size() || chosen < 0) return;
    Control& c = m_controls[index];
    if (!c.comboSel || !c.comboItems || chosen >= (int)c.comboItems->size()) return;
    if (*c.comboSel == chosen) return;
    *c.comboSel = chosen;
    // CBN_SELCHANGE: no lobby, mexer num controle avisa o outro lado.
    if (m_state == MenuState::MpGameSettings) mpControlChanged(c.id);
}

void ShellMenu::drawOpenCombo(Renderer& renderer) {
    if (m_openCombo < 0 || m_openCombo >= (int)m_controls.size()) return;
    const Control& c = m_controls[m_openCombo];
    int x, y, w, h;
    if (!comboRect(c, x, y, w, h)) return;
    uint32_t* fb = renderer.getFramebuffer();
    const int n = (int)c.comboItems->size();
    const int vis = (h - 4) / kComboItemH;

    const bool skin = DialogSkin::ok();
    if (skin) {
        renderer.fillRect(x, y, w, h, DialogSkin::kBoxBg);
        renderer.drawRect(x, y, w, h, DialogSkin::kBoxEdge);
    } else {
        renderer.fillRect(x, y, w, h, 0xFF101620);
        renderer.drawRect(x, y, w, h, 0xFFFFD24A);
    }
    // Fork: com a pele, barra de rolagem à direita quando a lista não cabe.
    const int sbW = (skin && n > vis) ? 16 : 0;
    for (int k = 0; k < vis; ++k) {
        const int item = m_comboScroll + k;
        if (item < 0 || item >= n) break;
        const int iy = y + 2 + k * kComboItemH;
        const bool hot = (item == m_comboHover);
        if (hot) renderer.fillRect(x + 2, iy, w - 4 - sbW, kComboItemH,
                                   skin ? DialogSkin::kSelBg : 0xFF2A3A55);
        if (hot && skin)
            SkinText::drawRaw(fb, 640, 480, (*c.comboItems)[item], x + 5, iy + 2, DialogSkin::kSelText);
        else
            SkinText::draw(fb, 640, 480, (*c.comboItems)[item], x + 5, iy + 2,
                           hot ? 0xFFFFFF99 : 0xFFC8D4E8, true);
    }
    if (sbW) {
        renderer.drawSkinScrollBar(x + w - 1 - sbW, y + 1, sbW, h - 2, m_comboScroll, vis, n);
        return;
    }
    // As setinhas de rolagem, quando a lista é maior que a janela.
    if (n > vis) {
        if (m_comboScroll > 0)
            SkinText::draw(fb, 640, 480, "^", x + w - 10, y + 2, 0xFF8090A8, true);
        if (m_comboScroll + vis < n)
            SkinText::draw(fb, 640, 480, "v", x + w - 10, y + h - 14, 0xFF8090A8, true);
    }
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
                                c.hovered || focused, c.pressed, c.disabled, m_uiOpacity);
            break;
        case CtrlKind::Radio: {
            bool sel = c.radioTarget && (*c.radioTarget == c.radioValue);
            if (DialogSkin::ok()) {             // Fork: glifo + texto verde
                renderer.drawSkinRadio(c.x, c.y, sel, c.disabled);
                SkinText::drawRaw(fb, 640, 480, c.text, c.x + 18, c.y,
                                  c.disabled ? DialogSkin::kTextOff : DialogSkin::kText);
                break;
            }
            uint32_t col = c.disabled ? 0xFF60708A
                         : (hl ? 0xFFFFFF99 : (sel ? 0xFFE0ECFF : 0xFFA0B0C8));
            SkinText::draw(fb, 640, 480, (sel ? "(o) " : "( ) ") + c.text, c.x, c.y, col, true);
            break;
        }
        case CtrlKind::Check: {
            bool on = c.checkTarget && *c.checkTarget;
            if (DialogSkin::ok()) {             // Fork: glifo + texto verde
                renderer.drawSkinCheck(c.x, c.y, on, c.disabled);
                SkinText::drawRaw(fb, 640, 480, c.text, c.x + 18, c.y,
                                  c.disabled ? DialogSkin::kTextOff : DialogSkin::kText);
                break;
            }
            uint32_t col = c.disabled ? 0xFF60708A
                         : (hl ? 0xFFFFFF99 : (on ? 0xFFE0ECFF : 0xFFA0B0C8));
            SkinText::draw(fb, 640, 480, (on ? "[X] " : "[ ] ") + c.text, c.x, c.y, col, true);
            break;
        }
        case CtrlKind::List:
            break;   // conteúdo desenhado no render do diálogo
        case CtrlKind::Combo: {
            const std::string txt = (c.comboItems && c.comboSel &&
                                     *c.comboSel >= 0 &&
                                     *c.comboSel < (int)c.comboItems->size())
                                  ? (*c.comboItems)[*c.comboSel] : std::string();
            if (DialogSkin::ok()) {
                // Fork: caixa preta com borda clara, botão de seta à direita.
                const int ab = std::min(17, c.h - 4);
                renderer.drawSkinBox(c.x, c.y, c.w, c.h);
                renderer.drawSkinArrowButton(c.x + c.w - 2 - ab, c.y + 2, ab, c.h - 4, false);
                SkinText::drawRaw(fb, 640, 480, txt, c.x + 4, c.y + (c.h - SkinText::height()) / 2 + 1,
                                  c.disabled ? DialogSkin::kTextOff : DialogSkin::kText);
                break;
            }
            renderer.drawRect(c.x, c.y, c.w, c.h, c.disabled ? 0xFF2A3545 : 0xFF405068);
            SkinText::draw(fb, 640, 480, txt, c.x + 4, c.y + (c.h - 10) / 2,
                           c.disabled ? 0xFF60708A : (hl ? 0xFFFFFF99 : 0xFFC8D4E8), true);
            // a marca da lista suspensa, no canto direito
            SkinText::draw(fb, 640, 480, (i == m_openCombo) ? "^" : "v",
                           c.x + c.w - 10, c.y + (c.h - 10) / 2,
                           c.disabled ? 0xFF60708A : 0xFF8090A8, true);
            break;
        }
        case CtrlKind::Slider: {
            const int v = c.sliderTarget ? *c.sliderTarget : c.sliderMin;
            const int span = c.sliderMax - c.sliderMin;
            const int kx = sliderKnobX(c);
            const int ty = c.y + c.h / 2 - 2;     // a calha, no meio da altura
            const bool skin = DialogSkin::ok();
            renderer.fillRect(c.x, ty, c.w, 4, skin ? DialogSkin::kBoxBg : 0xFF10161F);
            renderer.drawRect(c.x, ty, c.w, 4, skin ? DialogSkin::kBoxEdgeDk
                                                    : (c.disabled ? 0xFF2A3545 : 0xFF405068));
            // a parte já percorrida, até o cursor
            const int fill = kx - c.x + kSliderKnobW / 2;
            if (fill > 0 && span > 0)
                renderer.fillRect(c.x + 1, ty + 1, fill, 2,
                                  skin ? (c.disabled ? DialogSkin::kTextOff : 0xFF00B000)
                                       : (c.disabled ? 0xFF48566C : 0xFF6C8CC0));
            // os riscos das pontas e do meio, para dar escala à barra
            for (int k = 0; k <= 4; ++k) {
                const int mx = c.x + kSliderKnobW / 2 + k * (c.w - kSliderKnobW) / 4;
                renderer.fillRect(mx, c.y + c.h - 3, 1, 3, skin ? DialogSkin::kBoxEdgeDk : 0xFF48566C);
            }
            renderer.drawButton(kx, c.y, kSliderKnobW, c.h, "", hl, c.pressed, c.disabled);
            (void)v;
            break;
        }
        case CtrlKind::Edit: {
            const bool active = focused && !c.disabled;
            std::string txt = c.editTarget ? *c.editTarget : std::string();
            if (active) txt += "_";
            if (DialogSkin::ok()) {             // Fork: caixa preta, texto verde
                renderer.drawSkinBox(c.x, c.y, c.w, c.h);
                SkinText::drawRaw(fb, 640, 480, txt, c.x + 4, c.y + (c.h - SkinText::height()) / 2 + 1,
                                  c.disabled ? DialogSkin::kTextOff : DialogSkin::kText);
                break;
            }
            renderer.drawRect(c.x, c.y, c.w, c.h, active ? 0xFFFFD24A : 0xFF405068);
            SkinText::draw(fb, 640, 480, txt, c.x + 4, c.y + (c.h - 10) / 2,
                           c.disabled ? 0xFF60708A : 0xFFE0ECFF, true);
            break;
        }
        }

        // Retângulo de foco (amarelo) mostrando qual controle está focado.
        // Fork: com a pele do original o botão focado já tem o contorno
        // vermelho vivo (estado 2 da tira), sem retângulo extra.
        const bool skinnedButton = DialogSkin::ok();   // Fork: nenhum controle usa o retângulo amarelo
        if (focused && !c.disabled && !skinnedButton)
            renderer.drawRect(c.x - 3, c.y - 3, c.w + 6, c.h + 6, 0xFFFFD24A);
    }
    drawOpenCombo(renderer);   // a lista solta fica por cima de todos
}

void ShellMenu::renderSplash(Renderer& renderer) {
    // Tela de carregamento: logo OPACO centralizado; entra no menu sozinho.
    if (m_logoBitmap.getWidth() > 0) {
        int lw = m_logoBitmap.getWidth();
        int lh = m_logoBitmap.getHeight();
        renderer.blitBitmap(m_logoBitmap, (640 - lw) / 2, (480 - lh) / 2 - 20, true, 0xFF000000);
    }
    SkinText::drawCentered(renderer.getFramebuffer(), 640, 480,
                           "Loading...", 320, 430, 0xFF9CB0C8, true);
}

void ShellMenu::renderMainMenu(Renderer& renderer) {
    renderer.drawDialogFrame(kMenuDlgX, kMenuDlgY, kMenuDlgW, kMenuDlgH, "MAIN MENU",
                             m_uiOpacity);
    // Os botões são desenhados por drawControls().
}

void ShellMenu::renderNewCampaignDialog(Renderer& renderer) {
    renderer.dimRect(0, 0, 640, 480, 0x88);
    renderer.drawDialogFrame(kNcX, kNcY, kNcW, kNcH, "NEW CAMPAIGN OPTIONS");
    uint32_t* fb = renderer.getFramebuffer();
    SkinText::draw(fb, 640, 480, "Colony:", kNcX + 16, kNcY + 36, 0xFFD8E4F8, true);
    SkinText::draw(fb, 640, 480, "Difficulty:", kNcX + 150, kNcY + 36, 0xFFD8E4F8, true);
    // Radios e botões são desenhados por drawControls().
}

void ShellMenu::renderAboutDialog(Renderer& renderer) {
    renderer.dimRect(0, 0, 640, 480, 0x88);
    renderer.drawDialogFrame(kAbX, kAbY, kAbW, kAbH, "ABOUT OUTPOST 2...");
    uint32_t* fb = renderer.getFramebuffer();
    SkinText::drawCentered(fb, 640, 480, "Outpost 2: Divided Destiny", kAbX + kAbW/2, kAbY + 36, 0xFFFFFFFF, true);
    SkinText::drawCentered(fb, 640, 480, "Copyright 1997, Sierra On-Line, Inc.", kAbX + kAbW/2, kAbY + 56, 0xFF9CB0C8, true);
    SkinText::drawCentered(fb, 640, 480, "Modern C++ SDL2 Engine Port", kAbX + kAbW/2, kAbY + 76, 0xFF8AE0AA, true);

    int boxX = kAbX + 16, boxY = kAbY + 105, boxW = kAbW - 32, boxH = 140;
    renderer.drawSkinBox(boxX, boxY, boxW, boxH);
    SkinText::draw(fb, 640, 480, "Original Dynamix / Sierra Team:", boxX + 10, boxY + 10, 0xFFFFFF99);
    SkinText::draw(fb, 640, 480, "- Lead Design: Jason Pratt", boxX + 10, boxY + 28, 0xFFD0DCE8);
    SkinText::draw(fb, 640, 480, "- Lead Programming: Bryan Shannon", boxX + 10, boxY + 44, 0xFFD0DCE8);
    SkinText::draw(fb, 640, 480, "- Art Director: Terry Robinson", boxX + 10, boxY + 60, 0xFFD0DCE8);
    SkinText::draw(fb, 640, 480, "- Audio: Jan Paul Moorhead", boxX + 10, boxY + 76, 0xFFD0DCE8);
    SkinText::draw(fb, 640, 480, "Reconstruction 1:1 via Disassembly", boxX + 10, boxY + 105, 0xFF65C8FF);
    // Botão OK desenhado por drawControls().
}

// O PREFERENCES mexe no [Game] do OUTPOST2.INI, como o original faz. Fora o
// AILevel, que e acrescimo deste porte, as chaves sao as mesmas.
void ShellMenu::loadPrefs() {
    Config cfg("OUTPOST2.INI");
    GameSettings gs;
    if (cfg.load()) gs.loadFrom(cfg);
    m_prefSound        = gs.sound != 0;
    m_prefMusic        = gs.music != 0;
    m_prefAnimations   = gs.ambient != 0;
    m_prefUnitShadows  = gs.mobileShadows != 0;
    m_prefStructShadows = gs.stationaryShadows != 0;
    m_prefGameSpeed = gs.gameSpeed < 1 ? 1 : (gs.gameSpeed > 10 ? 10 : gs.gameSpeed);
    m_prefScrollRate = gs.scrollRate < 1 ? 1 : (gs.scrollRate > 6 ? 6 : gs.scrollRate);
    m_prefAiLevel = (gs.aiLevel >= 0 && gs.aiLevel <= 3) ? gs.aiLevel : 0;
    m_prefAudioVolume = gs.audioVolume < 0 ? 0 : (gs.audioVolume > 100 ? 100 : gs.audioVolume);
    m_prefAudioVolumeIni = m_prefAudioVolume;
    m_prefFullscreen = gs.fullscreen ? 1 : 0;
}

void ShellMenu::savePrefs() {
    Config cfg("OUTPOST2.INI");
    GameSettings gs;
    if (cfg.load()) gs.loadFrom(cfg);     // preserva o que a tela nao mostra
    gs.sound             = m_prefSound ? 1 : 0;
    gs.music             = m_prefMusic ? 1 : 0;
    gs.ambient           = m_prefAnimations ? 1 : 0;
    gs.mobileShadows     = m_prefUnitShadows ? 1 : 0;
    gs.stationaryShadows = m_prefStructShadows ? 1 : 0;
    gs.gameSpeed         = m_prefGameSpeed;
    gs.scrollRate        = m_prefScrollRate;
    gs.aiLevel           = m_prefAiLevel;
    gs.audioVolume       = m_prefAudioVolume;
    gs.fullscreen        = m_prefFullscreen;
    m_prefApplyScreen    = true;                 // o render troca a janela
    m_prefAudioVolumeIni = m_prefAudioVolume;    // o ACCEPT é que torna isso o som gravado
    gs.saveTo(cfg);
    cfg.save();
}

void ShellMenu::renderPreferencesDialog(Renderer& renderer) {
    renderer.dimRect(0, 0, 640, 480, 0x88);
    renderer.drawDialogFrame(kPrX, kPrY, kPrW, kPrH, "PREFERENCES");
    uint32_t* fb = renderer.getFramebuffer();
    // Velocidades: o rótulo e o valor; quem muda são os botões LESS/MORE.
    // O valor aparece como "n / máximo" para o limite ficar à vista, já que
    // os dois têm faixas diferentes (10 e 6, como no original).
    {
        char buf[32];
        const struct { const char* rotulo; int valor, maximo, y; } linhas[] = {
            { "Game Speed:",   m_prefGameSpeed,  10, kPrY + 160 },
            { "Scroll Speed:", m_prefScrollRate,  6, kPrY + 186 },
        };
        for (const auto& l : linhas) {
            SkinText::draw(fb, 640, 480, l.rotulo, kPrX + 24, l.y, 0xFFD8E4F8, true);
            std::snprintf(buf, sizeof(buf), "%d / %d", l.valor, l.maximo);
            // alinhado a direita, encostando nos botoes
            SkinText::draw(fb, 640, 480, buf, kPrX + 178 - SkinText::width(buf),
                           l.y, 0xFFFFD24A, true);
        }
    }
    // Nível da IA (só deste porte): o rótulo do combo 1200 e a explicação do
    // AUTO, que é como o jogo original se comporta.
    SkinText::draw(fb, 640, 480, "AI Level:", kPrX + 24, kPrY + 219, 0xFFD8E4F8, true);
    SkinText::draw(fb, 640, 480,
                   m_prefAiLevel == 0 ? "(o nivel que a partida escolher)"
                                      : "(vale em campanha e colonia)",
                   kPrX + 24, kPrY + 246, 0xFF8090A8, true);
    // Volume do áudio (idem): o rótulo à esquerda e o valor na mesma linha,
    // como o original põe o static 1103 ao lado do "Volume:".
    SkinText::draw(fb, 640, 480, "Audio Volume:", kPrX + 24, kPrY + 270, 0xFFD8E4F8, true);
    char pct[16];
    std::snprintf(pct, sizeof(pct), "%d%%", m_prefAudioVolume);
    SkinText::draw(fb, 640, 480, pct, kPrX + kPrW - 24 - SkinText::width(pct),
                   kPrY + 270, 0xFFFFD24A, true);
    // Modo de tela (idem): o rótulo do par de rádio 1202/1203.
    SkinText::draw(fb, 640, 480, "Screen Mode:", kPrX + 24, kPrY + 320, 0xFFD8E4F8, true);
}

// O botão HELP (1012) e a tecla F1 — no original, WM_HELP -> TApp::GetHelp,
// que chamava o WinHelp com HELP_INDEX quando estava no shell.
void ShellMenu::openHelp() {
    if (!m_helpTried) {
        m_helpTried = true;
        if (m_help.load("OUTPOST2.HLP")) {
            std::cout << "[Help] " << m_help.manualTitle() << ": "
                      << m_help.topics().size() << " topicos." << std::endl;
        } else {
            std::cout << "[Help] OUTPOST2.HLP: " << m_help.error() << std::endl;
            m_helpFallback = helpOpenExternalManual();
            if (!m_helpFallback.empty())
                std::cout << "[Help] abri o manual externo: " << m_helpFallback << std::endl;
        }
    }
    // Sem o .HLP mas com um PDF ao lado, quem mostra o manual é o leitor do
    // sistema — não há tela nossa para abrir.
    if (!m_help.ok() && !m_helpFallback.empty()) return;
    m_helpTopic = 0;
    m_helpListScroll = 0;
    m_helpTextScroll = 0;
    m_helpBuiltFor = -1;
    setState(MenuState::Help);
}

int ShellMenu::helpVisibleLines() const { return kHpTextH / (SkinText::height() + 2); }

void ShellMenu::helpBuildLines() {
    m_helpBuiltFor = m_helpTopic;
    m_helpLines.clear();
    m_helpTextScroll = 0;
    if (!m_help.ok()) return;
    const std::vector<HelpTopic>& tops = m_help.topics();
    if (m_helpTopic < 0 || m_helpTopic >= (int)tops.size()) return;

    auto apara = [](std::string t) {
        size_t a = 0, b = t.size();
        while (a < b && (unsigned char)t[a] <= ' ') ++a;
        while (b > a && (unsigned char)t[b - 1] <= ' ') --b;
        return t.substr(a, b - a);
    };

    std::vector<RtfRun> runs;
    bool primeiro = true;
    // Uma linha do texto. `bloco` abre um parágrafo novo (linha em branco
    // antes); as linhas seguidas de uma mesma tabela não abrem.
    auto linha = [&](const std::string& txt, uint32_t cor, bool bloco) {
        if (!primeiro && bloco) { RtfRun b; b.newline = true; runs.push_back(b); }
        RtfRun r; r.text = txt; r.color = cor; r.newline = !primeiro;
        runs.push_back(r);
        primeiro = false;
    };

    for (const HelpPara& par : tops[m_helpTopic].paras) {
        // Os pedaços de um link vêm terminados em NUL e VÁRIOS SÃO VAZIOS —
        // são as marcas de formatação entre os textos. Só os com conteúdo
        // valem como células.
        std::vector<std::string> cel;
        for (const std::string& c : par.cells) {
            const std::string t = apara(c);
            if (!t.empty()) cel.push_back(t);
        }
        if (cel.empty()) continue;
        if (par.table) {
            // As tabelas do manual são pares (rótulo, valor), às vezes dois
            // pares no mesmo link. Cada par vira uma linha, com o valor numa
            // coluna enquanto o rótulo couber nela.
            bool abre = true;
            for (size_t i = 0; i < cel.size(); i += 2) {
                std::string t = cel[i];
                if (i + 1 < cel.size()) {
                    if (t.size() < 26) t += std::string(26 - t.size(), ' ');
                    else t += "  ";
                    t += cel[i + 1];
                }
                linha(t, 0xFFB8C8E0, abre);
                abre = false;
            }
        } else {
            std::string t;
            for (size_t i = 0; i < cel.size(); ++i) {
                if (i) t += " ";
                t += cel[i];
            }
            linha(t, primeiro ? 0xFFFFD24A : 0xFFD8E4F8, true);
        }
    }
    m_helpLines = rtfLayout(runs, kHpTextW);
}

void ShellMenu::renderHelp(Renderer& renderer) {
    renderer.dimRect(0, 0, 640, 480, 0x88);
    renderer.drawDialogFrame(kHpX, kHpY, kHpW, kHpH,
                             m_help.ok() ? m_help.manualTitle() : std::string("ONLINE MANUAL"));
    uint32_t* fb = renderer.getFramebuffer();

    if (!m_help.ok()) {
        SkinText::draw(fb, 640, 480, "O manual nao pode ser aberto:", kHpX + 24, kHpY + 60,
                       0xFFD8E4F8, true);
        SkinText::draw(fb, 640, 480, m_help.error(), kHpX + 24, kHpY + 82, 0xFFFF9999, true);
        SkinText::draw(fb, 640, 480, "Ponha o OUTPOST2.HLP (ou um manual em PDF) junto do jogo.",
                       kHpX + 24, kHpY + 110, 0xFF8090A8, true);
        return;
    }
    if (m_helpBuiltFor != m_helpTopic) helpBuildLines();

    // ---- lista de tópicos ----
    const std::vector<HelpTopic>& tops = m_help.topics();
    const int total = (int)tops.size();
    const int maxScroll = (total > kHpVisible) ? (total - kHpVisible) : 0;
    if (m_helpListScroll > maxScroll) m_helpListScroll = maxScroll;
    if (DialogSkin::ok()) {
        // Fork: listbox do original -- caixa preta, seleção azul, barra padrão.
        renderer.drawSkinBox(kHpListX, kHpListY, kHpListW, kHpListH);
        const int textW = kHpListW - kHpSbW - 8;
        for (int k = 0; k < kHpVisible; ++k) {
            const int i = m_helpListScroll + k;
            if (i < 0 || i >= total) break;
            const int y = kHpListY + 1 + k * kHpRowH;
            const bool sel = (i == m_helpTopic);
            if (sel) renderer.fillRect(kHpListX + 2, y, kHpListW - kHpSbW - 4, kHpRowH, DialogSkin::kSelBg);
            std::string t = tops[i].title;
            if (SkinText::width(t) > textW) {
                while (!t.empty() && SkinText::width(t + "...") > textW) t.pop_back();
                t += "...";
            }
            SkinText::drawRaw(fb, 640, 480, t, kHpListX + 4, y + 1,
                              sel ? DialogSkin::kSelText : DialogSkin::kText);
        }
        renderer.drawSkinScrollBar(kHpListX + kHpListW - 2 - kHpSbW, kHpListY + 2, kHpSbW,
                                   kHpListH - 4, m_helpListScroll, kHpVisible, total);
    } else {
    renderer.drawRect(kHpListX, kHpListY, kHpListW, kHpListH, 0xFF405068);
    for (int k = 0; k < kHpVisible; ++k) {
        const int i = m_helpListScroll + k;
        if (i < 0 || i >= total) break;
        const int y = kHpListY + 2 + k * kHpRowH;
        const bool sel = (i == m_helpTopic);
        if (sel) renderer.fillRect(kHpListX + 2, y - 1, kHpListW - kHpSbW - 4, kHpRowH, 0xFF2A3A55);
        std::string t = tops[i].title;
        const int cabe = (kHpListW - kHpSbW - 12) / 8;
        if ((int)t.size() > cabe && cabe > 2) t = t.substr(0, cabe - 2) + "..";
        SkinText::draw(fb, 640, 480, t, kHpListX + 6, y, sel ? 0xFFFFFF99 : 0xFFC8D4E8, true);
    }
    }
    // barra de rolagem da lista: as duas setas e o cursor
    const int sbX = kHpListX + kHpListW - kHpSbW;
    if (!DialogSkin::ok()) {
    renderer.drawRect(sbX, kHpListY, kHpSbW, kHpListH, 0xFF405068);
    SkinText::drawCentered(fb, 640, 480, "^", sbX + kHpSbW / 2, kHpListY + 2, 0xFF8090A8, true);
    SkinText::drawCentered(fb, 640, 480, "v", sbX + kHpSbW / 2, kHpListY + kHpListH - 14,
                           0xFF8090A8, true);
    if (maxScroll > 0) {
        const int faixa = kHpListH - 2 * kHpSbW - 8;
        const int cy = kHpListY + kHpSbW + 4 + faixa * m_helpListScroll / maxScroll;
        renderer.fillRect(sbX + 3, cy, kHpSbW - 6, 10, 0xFF6C8CC0);
    }
    }

    // ---- texto do tópico ----
    const int lineH = SkinText::height() + 2;
    const int vis = helpVisibleLines();
    const int maxTxt = (int)m_helpLines.size() > vis ? (int)m_helpLines.size() - vis : 0;
    if (m_helpTextScroll > maxTxt) m_helpTextScroll = maxTxt;
    if (m_helpTextScroll < 0) m_helpTextScroll = 0;
    for (int k = 0; k < vis; ++k) {
        const int i = m_helpTextScroll + k;
        if (i < 0 || i >= (int)m_helpLines.size()) break;
        const int y = kHpTextY + k * lineH;
        for (const RtfLine::Piece& pc : m_helpLines[i].pieces)
            SkinText::draw(fb, 640, 480, pc.text, kHpTextX + pc.x, y, pc.color, true);
    }
    if (maxTxt > 0) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%d%%", 100 * m_helpTextScroll / maxTxt);
        SkinText::draw(fb, 640, 480, buf, kHpX + kHpW - 50, kHpY + kHpH - 34, 0xFF8090A8, true);
    }
}

void ShellMenu::renderColonyGamesDialog(Renderer& renderer) {
    renderer.dimRect(0, 0, 640, 480, 0x88);
    renderer.drawDialogFrame(kCgX, kCgY, kCgW, kCgH, "COLONY GAMES");

    renderer.drawSkinBox(kCgListX, kCgListY, kCgListW, kCgListH);
    for (size_t i = 0; i < m_colonyGameList.size(); ++i) {
        int itemY = kCgListY + 8 + (int)i * kCgRowH;
        if (itemY + 16 > kCgListY + kCgListH) break;
        bool sel = ((int)i == m_selectedColonyGame);
        if (DialogSkin::ok()) {     // Fork: seleção azul, texto branco / verde
            if (sel) renderer.fillRect(kCgListX + 2, itemY, kCgListW - 4, kCgRowH, DialogSkin::kSelBg);
            SkinText::drawRaw(renderer.getFramebuffer(), 640, 480, m_colonyGameList[i],
                              kCgListX + 4, itemY + 1, sel ? DialogSkin::kSelText : DialogSkin::kText);
            continue;
        }
        if (sel) renderer.fillRect(kCgListX + 2, itemY - 2, kCgListW - 4, 18, 0xFF203858);
        SkinText::draw(renderer.getFramebuffer(), 640, 480, m_colonyGameList[i],
                       kCgListX + 10, itemY, sel ? 0xFFFFFF99 : 0xFFCCD6E8);
    }
    // Botões PLAY / SCORES / BACK desenhados por drawControls().
}

void ShellMenu::renderTutorialsDialog(Renderer& renderer) {
    renderer.dimRect(0, 0, 640, 480, 0x88);
    renderer.drawDialogFrame(kTutDlgX, kTutDlgY, kTutDlgW, kTutDlgH, "TUTORIALS");

    int lx = kTutListX, ly = kTutListY, lw = kTutListW - kTutSbW, lh = kTutListH;
    int total = (int)m_tutorialList.size();
    if (DialogSkin::ok()) {
        // Fork: como o listbox do original -- a borda envolve a lista E a
        // barra de rolagem; linha selecionada em azul com texto branco.
        renderer.drawSkinBox(lx, ly, kTutListW, lh);
        uint32_t* fb = renderer.getFramebuffer();
        for (int row = 0; row < kTutVisible; ++row) {
            int i = m_tutorialScroll + row;
            if (i >= total) break;
            int itemY = ly + 4 + row * kTutRowH;
            bool sel = (i == m_selectedTutorial);
            std::string txt = m_tutorialList[i];
            while (!txt.empty() && SkinText::width(txt) > lw - 10) txt.pop_back();
            if (sel) renderer.fillRect(lx + 2, itemY, lw - 2, kTutRowH, DialogSkin::kSelBg);
            SkinText::drawRaw(fb, 640, 480, txt, lx + 4, itemY + 1,
                              sel ? DialogSkin::kSelText : DialogSkin::kText);
        }
        renderer.drawSkinScrollBar(lx + lw - 2, ly + 2, kTutSbW, lh - 4,
                                   m_tutorialScroll, kTutVisible, total);
        return;
    }

    renderer.fillRect(lx, ly, lw, lh, 0xFF0D121B);
    renderer.drawBevelRect(lx, ly, lw, lh, 0xFF05080E, 0xFF35445B);

    for (int row = 0; row < kTutVisible; ++row) {
        int i = m_tutorialScroll + row;
        if (i >= total) break;
        int itemY = ly + 4 + row * kTutRowH;
        bool sel = (i == m_selectedTutorial);
        if (sel) renderer.fillRect(lx + 2, itemY - 1, lw - 4, kTutRowH - 2, 0xFF264066);
        std::string txt = m_tutorialList[i];
        while (!txt.empty() && SkinText::width(txt) > lw - 16) txt.pop_back();
        SkinText::draw(renderer.getFramebuffer(), 640, 480, txt, lx + 8, itemY + 4,
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

// ------------------------------- campanha ---------------------------------

std::string ShellMenu::colonyName() const { return m_gsi.eden() ? "Eden" : "Plymouth"; }

void ShellMenu::debugCampaign(bool eden, int difficulty, int missionNumber, int screen) {
    m_gsi = GameStartInfo{};
    m_gsi.flags = 8;
    m_gsi.playerWord[0] = (eden ? 4u : 0x20u) | (static_cast<uint32_t>(difficulty) << 3);
    m_gsi.missionNumber = missionNumber;
    switch (screen) {
        case 1: openRtf(0); break;
        case 2: openRtf(1); break;
        case 3: openChapters(); break;
        case 4: m_storyChapter = missionNumber + 1; openRtf(2); break;
        default: setState(MenuState::MissionMenu); break;
    }
}

int ShellMenu::rtfVisibleLines() const { return kRvTextH / (SkinText::height() + 2); }

// Mensagem 0x73a da janela do shell (motivo − 1 → tabela 0x130090b4). Os filmes
// (opção Game/ShowMovies, mensagem 0x72b) não existem aqui: vai direto ao passo
// seguinte ([+0x540]).
void ShellMenu::afterGame(int reason, const GameStartInfo* results) {
    // Voltando de uma partida (inclusive do proprio demo), o relogio da
    // ociosidade recomeca — senao o demo seguinte entraria na mesma hora.
    noteActivity();
    if (results) m_gsi = *results;
    if (results && m_gsi.campaign() && reason == 3) {        // 0x13008b3e
        if (m_gsi.missionNumber == 12) openChapters();       // fim da campanha: 0x735
        else setState(MenuState::MissionMenu);               // 0x73b
        return;
    }
    if (results && m_gsi.campaign() && reason == 4) {        // 0x13008c61
        setState(MenuState::MissionMenu);
        return;
    }
    // 7, 8 e 9 vao todos para a mesma tela (0x13008CE0/CF8/D10 mandam 0x727, que
    // abre o Dialog 155): as estatisticas da partida multijogador.
    if (reason >= 7 && reason <= 9) {
        setState(MenuState::MpStats);
        return;
    }
    // Fora da campanha, vitória (3) num jogo de colônia → 0x73e ENTER YOUR NAME.
    if (reason == 3 && !m_lastColonyDesc.empty()) {
        m_scoreMarks = static_cast<int>(TethysGame::tick()) / 100;   // [0x13015bbf]/100
        m_scoreName.clear();
        setState(MenuState::EnterName);
        return;
    }
    setState(MenuState::MainMenu);
}

void ShellMenu::openScores() {
    m_scores = scoresLoad();
    m_scoreScroll = 0;
    m_clearConfirm = false;
    setState(MenuState::Scores);
}

int ShellMenu::scoreVisibleLines() const { return (kScListH - 12) / kScRowH; }

// Dialog 224 (0x73e): parabeniza e pede o nome; o Edit 1093 recebe o texto.
void ShellMenu::renderEnterName(Renderer& renderer) {
    renderer.dimRect(0, 0, 640, 480, 0x88);
    renderer.drawDialogFrame(kEnX, kEnY, kEnW, kEnH, "ENTER YOUR NAME");
    uint32_t* fb = renderer.getFramebuffer();
    const int tx = kEnX + 10, ty = kEnY + 22 + 11;
    SkinText::draw(fb, 640, 480, "Congratulations on completing this", tx, ty, 0xFFCCD6E8);
    SkinText::draw(fb, 640, 480, "colony game! Please enter your", tx, ty + 16, 0xFFCCD6E8);
    SkinText::draw(fb, 640, 480, "name below.", tx, ty + 32, 0xFFCCD6E8);
    SkinText::draw(fb, 640, 480, "Your Name:", tx, kEnEditY + 6, 0xFFCCD6E8);
    renderer.drawSkinBox(kEnEditX, kEnEditY, kEnEditW, kEnEditH);
    std::string t = m_scoreName + ((SDL_GetTicks() / 400) & 1 ? "_" : "");
    while (!t.empty() && SkinText::width(t) > kEnEditW - 8) t.erase(0, 1);
    SkinText::draw(fb, 640, 480, t, kEnEditX + 4, kEnEditY + 7, 0xFFFFFFFF);
}

// Dialog 155 (motivos 7..9 da mensagem 0x73a): os seis contadores do jogador
// local, que o jogo deixou no MissionResults+0xC2 + jogador*28. A coluna da
// esquerda e o que ele fez e perdeu; a da direita, o que ele destruiu.
void ShellMenu::renderMpStats(Renderer& renderer) {
    renderer.dimRect(0, 0, 640, 480, 0x88);
    renderer.drawDialogFrame(kMsX, kMsY, kMsW, kMsH, "MULTIPLAYER GAME STATISTICS");
    uint32_t* fb = renderer.getFramebuffer();
    const int p = (m_gsi.localPlayer >= 0 && m_gsi.localPlayer < 6) ? m_gsi.localPlayer : 0;
    const int* v = m_gsi.stats[p];
    // (rotulo, coluna 0/1, linha, valor) -- os mesmos Statics do dialogo, nas
    // mesmas posicoes relativas: quatro linhas a esquerda, duas a direita.
    struct Linha { const char* rot; int col, lin, valor; };
    const Linha linhas[] = {
        {"Units Built:",                0, 0, v[1]},
        {"Structures Built:",           0, 1, v[0]},
        {"Units Lost:",                 0, 2, v[3]},
        {"Structures Lost:",            0, 3, v[2]},
        {"Enemy Units Destroyed:",      1, 2, v[5]},
        {"Enemy Structures Destroyed:", 1, 3, v[4]},
    };
    // A coluna do numero fica depois do rotulo mais largo da sua coluna.
    int fim[2] = {0, 0};
    for (const Linha& l : linhas) {
        const int w = SkinText::width(l.rot);
        if (w > fim[l.col]) fim[l.col] = w;
    }
    // A coluna da direita so comeca depois do NUMERO da esquerda, senao os dois
    // se encostam quando o valor tem dois digitos.
    int larguraVal = 0;
    for (const Linha& l : linhas)
        if (l.col == 0) {
            const int w = SkinText::width(std::to_string(l.valor));
            if (w > larguraVal) larguraVal = w;
        }
    const int colX0 = kMsX + 10;
    const int valX0 = colX0 + fim[0] + 10;
    const int colX[2] = {colX0, valX0 + larguraVal + 24};
    const int valX[2] = {valX0, colX[1] + fim[1] + 10};
    for (const Linha& l : linhas) {
        const int y = kMsY + 22 + 8 + l.lin * 21;
        SkinText::draw(fb, 640, 480, l.rot, colX[l.col], y, 0xFFCCD6E8);
        SkinText::draw(fb, 640, 480, std::to_string(l.valor), valX[l.col], y, 0xFFFFFFFF);
    }
}

// Dialog 223: a listbox com uma linha por entrada (scoreLine).
void ShellMenu::renderScores(Renderer& renderer) {
    renderer.dimRect(0, 0, 640, 480, 0x88);
    renderer.drawDialogFrame(kScX, kScY, kScW, kScH, "COLONY GAME SCORES");
    uint32_t* fb = renderer.getFramebuffer();
    renderer.drawSkinBox(kScListX, kScListY, kScListW, kScListH);
    const int vis = scoreVisibleLines();
    for (int k = 0; k < vis; ++k) {
        const int i = m_scoreScroll + k;
        if (i < 0 || i >= (int)m_scores.size()) break;
        std::string line = scoreLine(m_scores[i]);
        while (!line.empty() && SkinText::width(line) > kScListW - 16) line.pop_back();
        SkinText::draw(fb, 640, 480, line, kScListX + 8, kScListY + 6 + k * kScRowH, 0xFFCCD6E8);
    }
    if (m_scores.empty())
        SkinText::draw(fb, 640, 480, "(no scores yet)", kScListX + 8, kScListY + 6, 0xFF708090);
    if (m_clearConfirm) {                                 // caixa de confirmação
        const int cw = 420, ch = 96, cx = (640 - cw) / 2, cy = kScY + 60;
        renderer.drawDialogFrame(cx, cy, cw, ch, "Delete All Scores?");
        SkinText::draw(fb, 640, 480, "Are you sure you want to clear", cx + 12, cy + 34, 0xFFCCD6E8);
        SkinText::draw(fb, 640, 480, "all of the scores?", cx + 12, cy + 50, 0xFFCCD6E8);
    }
}

// Dialog 216 com o RICHEDIT: B_%s%02i.rtf (briefing), O_%s%02i_%c.rtf
// (objetivos; e/m/h pela dificuldade) e S_%s%02i.rtf (capítulo).
void ShellMenu::openRtf(int kind) {
    m_rtfKind = kind;
    const char* col = m_gsi.eden() ? "e" : "p";
    const int n = m_gsi.missionNumber + 1;
    char file[32], title[64];
    if (kind == 0) {
        std::snprintf(file, sizeof(file), "B_%s%02i.rtf", col, n);
        std::snprintf(title, sizeof(title), "%s Mission %i: Briefing", colonyName().c_str(), n);
    } else if (kind == 1) {
        const uint32_t d = m_gsi.playerWord[0] & 0x18;
        const char c = d == 0 ? 'e' : d == 0x10 ? 'h' : 'm';
        std::snprintf(file, sizeof(file), "O_%s%02i_%c.rtf", col, n, c);
        std::snprintf(title, sizeof(title), "%s Mission %i: Objectives", colonyName().c_str(), n);
    } else {
        std::snprintf(file, sizeof(file), "S_%s%02i.rtf", col, m_storyChapter);
        std::snprintf(title, sizeof(title), "%s Part %i", colonyName().c_str(), m_storyChapter);
    }
    m_rtfTitle = title;
    m_rtfLines.clear();
    m_rtfScroll = 0;
    if (auto s = g_ResManager.openStream(file)) {
        const std::vector<uint8_t>& b = s->bytes();
        m_rtfLines = rtfLayout(rtfParse(std::string(b.begin(), b.end())), kRvTextW - 8);
    } else {
        std::cerr << "[Campaign] nao achou " << file << std::endl;
    }
    setState(MenuState::RtfView);
}

// Dialog 215: linhas do ch_title.txt (Eden a partir da 0, Plymouth da 13) até
// o capítulo da missão atual (missões concluídas + 1).
void ShellMenu::openChapters() {
    m_chapters.clear();
    if (auto s = g_ResManager.openStream("ch_title.txt")) {
        const std::vector<uint8_t>& b = s->bytes();
        std::string all(b.begin(), b.end());
        std::vector<std::string> lines;
        size_t p = 0;
        while (p <= all.size()) {
            size_t e = all.find('\n', p);
            if (e == std::string::npos) e = all.size();
            std::string l = all.substr(p, e - p);
            while (!l.empty() && (l.back() == '\r' || l.back() == '\n')) l.pop_back();
            lines.push_back(l);
            p = e + 1;
        }
        const int off = m_gsi.eden() ? 0 : 13;
        for (int k = off; k <= m_gsi.missionNumber + off && k < static_cast<int>(lines.size()); ++k)
            m_chapters.push_back(lines[static_cast<size_t>(k)]);
    } else {
        std::cerr << "[Campaign] Could not open story file." << std::endl;
    }
    m_selChapter = 0;
    setState(MenuState::Chapters);
}

void ShellMenu::renderMissionMenu(Renderer& renderer) {
    char title[64];
    std::snprintf(title, sizeof(title), "%s Mission %i Menu", colonyName().c_str(), m_gsi.missionNumber + 1);
    renderer.drawDialogFrame(kMmX, kMmY, kMmW, kMmH, title);
}

void ShellMenu::renderRtfView(Renderer& renderer) {
    renderer.dimRect(0, 0, 640, 480, 0x88);
    renderer.drawDialogFrame(kRvX, kRvY, kRvW, kRvH, m_rtfTitle);
    if (DialogSkin::ok())   // Fork: borda clara de 2 px, como a lista do original
        renderer.drawSkinBox(kRvTextX - 2, kRvTextY - 2, kRvTextW + 4, kRvTextH + 4);
    renderer.fillRect(kRvTextX, kRvTextY, kRvTextW, kRvTextH, 0xFF000000);   // EM_SETBKGNDCOLOR(0)
    if (!DialogSkin::ok())
        renderer.drawBevelRect(kRvTextX - 1, kRvTextY - 1, kRvTextW + 2, kRvTextH + 2, 0xFF05080E, 0xFF35445B);
    uint32_t* fb = renderer.getFramebuffer();
    const int lh = SkinText::height() + 2, vis = rtfVisibleLines();
    for (int i = 0; i < vis; ++i) {
        const int li = m_rtfScroll + i;
        if (li >= static_cast<int>(m_rtfLines.size())) break;
        for (const RtfLine::Piece& pc : m_rtfLines[static_cast<size_t>(li)].pieces)
            SkinText::draw(fb, 640, 480, pc.text, kRvTextX + 4 + pc.x, kRvTextY + 4 + i * lh, pc.color);
    }
    // Barra de rolagem (só a posição).
    const int total = static_cast<int>(m_rtfLines.size());
    if (total > vis && DialogSkin::ok()) {
        renderer.drawSkinScrollBar(kRvTextX + kRvTextW - 17, kRvTextY, 17, kRvTextH, m_rtfScroll, vis, total);
    } else if (total > vis) {
        const int trackX = kRvTextX + kRvTextW - 10, trackH = kRvTextH - 4;
        const int thumbH = std::max(16, trackH * vis / total);
        const int maxScroll = total - vis;
        const int thumbY = kRvTextY + 2 + (trackH - thumbH) * m_rtfScroll / maxScroll;
        renderer.fillRect(trackX, kRvTextY + 2, 8, trackH, 0xFF11161F);
        renderer.fillRect(trackX + 1, thumbY, 6, thumbH, 0xFF3A557E);
    }
}

void ShellMenu::renderChapters(Renderer& renderer) {
    renderer.dimRect(0, 0, 640, 480, 0x88);
    renderer.drawDialogFrame(kChX, kChY, kChW, kChH, "CHAPTERS");
    renderer.drawSkinBox(kChListX, kChListY, kChListW, kChListH);
    for (size_t i = 0; i < m_chapters.size(); ++i) {
        const int y = kChListY + 8 + static_cast<int>(i) * kChRowH;
        if (y + 14 > kChListY + kChListH) break;
        const bool sel = static_cast<int>(i) == m_selChapter;
        if (DialogSkin::ok()) {     // Fork: seleção azul, texto branco / verde
            if (sel) renderer.fillRect(kChListX + 2, y, kChListW - 4, kChRowH, DialogSkin::kSelBg);
            SkinText::drawRaw(renderer.getFramebuffer(), 640, 480, m_chapters[i], kChListX + 4, y + 1,
                              sel ? DialogSkin::kSelText : DialogSkin::kText);
            continue;
        }
        if (sel) renderer.fillRect(kChListX + 2, y - 2, kChListW - 4, kChRowH - 2, 0xFF203858);
        SkinText::draw(renderer.getFramebuffer(), 640, 480, m_chapters[i], kChListX + 10, y,
                       sel ? 0xFFFFFF99 : 0xFFCCD6E8);
    }
}

} // namespace op2
