#include "game_options_pane.h"

#include <cstdio>
#include <iostream>

#include "../core/config.h"
#include "../game/scenario.h"
#include "../gfx/font.h"
#include "objectives_pane.h"
#include "produce_pane.h"

namespace op2 {

namespace {
// Espelho do [Game] enquanto o painel está aberto (o original mexe direto nas
// globais e regrava o INI).
GameSettings g_opt;

const char* kDetail[3] = {"Low", "Medium", "High"};

// Limites do original: volumes 0..10, detalhe 0..2 e, conferidos no
// disassembly, GameSpeed 1..10 (sub_45F0D0: GetInt padrao 5, teto 0Ah; o MORE
// de sub_466100 para em 10 e o LESS em 1) e ScrollRate **1..6** (sub_49ACA0:
// GetInt padrao 4, depois clamp 1..6; o MORE para em 6).
void clampAll() {
    auto cl = [](int& v, int lo, int hi) { if (v < lo) v = lo; if (v > hi) v = hi; };
    cl(g_opt.musicVol, 0, 10);
    cl(g_opt.soundVol, 0, 10);
    cl(g_opt.gameSpeed, 1, 10);
    cl(g_opt.scrollRate, 1, 6);
    cl(g_opt.spiffyDraw, 0, 2);
}
}

void GameOptionsPane::open() {
    Config cfg("OUTPOST2.INI");
    if (cfg.load()) g_opt.loadFrom(cfg);
    clampAll();
    m_list.reset();
    m_list.sel = 0;
    m_mode = MENU;                 // o original abre no MENU, nao nas opcoes
    m_open = true;
}

void GameOptionsPane::setMode(Mode m) {
    // Entrar em PREFS relê o INI: o que foi mexido e abandonado sem APPLY não
    // pode sobreviver (é o modelo do original, que só grava no "Apply new
    // values").
    if (m == PREFS) {
        Config cfg("OUTPOST2.INI");
        if (cfg.load()) g_opt.loadFrom(cfg);
        clampAll();
        m_list.reset();
        m_list.sel = 0;
    }
    m_mode = m;
}

void GameOptionsPane::apply() { saveIni(); }

// O Alt+Enter troca a tela sem gravar o INI, entao o valor lido do arquivo pode
// estar velho: ao entrar no PREFERENCES o GameState acerta o espelho com a
// janela de verdade, senao o radio mostraria um modo e a tela seria outra.
void GameOptionsPane::syncScreenMode(bool full) { g_opt.fullscreen = full ? 1 : 0; }
bool GameOptionsPane::wantFullscreen() const { return g_opt.fullscreen != 0; }

bool GameOptionsPane::manualOk() const { return m_help.ok(); }

void GameOptionsPane::openManual() {
    if (!m_helpTried) {
        m_helpTried = true;
        if (!m_help.load("OUTPOST2.HLP"))
            std::cout << "[Help] OUTPOST2.HLP: " << m_help.error() << std::endl;
    }
    m_list.reset();
    m_topic = -1;
    m_mode = MANUAL;
}

// As linhas da tela do manual: em MANUAL, os títulos dos tópicos; em
// MANUALTEXT, os parágrafos do tópico escolhido. As tabelas de atributos viram
// "rótulo  valor", como na tela do shell.
std::vector<std::string> GameOptionsPane::manualItems() const {
    std::vector<std::string> out;
    if (!m_help.ok()) {
        out.push_back("O manual nao pode ser aberto:");
        out.push_back(m_help.error());
        out.push_back("Ponha o OUTPOST2.HLP junto do jogo.");
        return out;
    }
    const std::vector<HelpTopic>& tops = m_help.topics();
    if (m_mode == MANUAL) {
        for (const HelpTopic& t : tops) out.push_back(t.title);
        return out;
    }
    if (m_topic < 0 || m_topic >= static_cast<int>(tops.size())) return out;
    auto apara = [](std::string t) {
        size_t a = 0, b = t.size();
        while (a < b && static_cast<unsigned char>(t[a]) <= ' ') ++a;
        while (b > a && static_cast<unsigned char>(t[b - 1]) <= ' ') --b;
        return t.substr(a, b - a);
    };
    for (const HelpPara& par : tops[m_topic].paras) {
        std::vector<std::string> cel;
        for (const std::string& c : par.cells) {
            const std::string t = apara(c);
            if (!t.empty()) cel.push_back(t);
        }
        if (cel.empty()) continue;
        if (par.table) {
            for (size_t i = 0; i < cel.size(); i += 2) {
                std::string t = cel[i];
                if (i + 1 < cel.size()) t += ":  " + cel[i + 1];
                out.push_back(t);
            }
        } else {
            std::string t;
            for (size_t i = 0; i < cel.size(); ++i) { if (i) t += " "; t += cel[i]; }
            out.push_back(t);
        }
    }
    return out;
}

void GameOptionsPane::saveIni() const {
    Config cfg("OUTPOST2.INI");
    cfg.load();
    g_opt.saveTo(cfg);
    cfg.save();
}

std::vector<std::string> GameOptionsPane::items() const {
    if (m_mode == MANUAL || m_mode == MANUALTEXT) return manualItems();
    auto onOff = [](int v) { return v ? "On" : "Off"; };
    char b[96];
    std::vector<std::string> out;
    auto add = [&](const char* fmt, auto v) { std::snprintf(b, sizeof(b), fmt, v); out.push_back(b); };
    // Os rótulos e o formato "<R>" (valor alinhado à direita) são os do original.
    add("Music\n\tVolume:<R>%i", g_opt.musicVol);
    add("Sound Effects\n\tVolume:<R>%i", g_opt.soundVol);
    add("Game Speed\n\tGame Speed:<R>%i", g_opt.gameSpeed);
    add("Scrolling Speed\n\tScrolling Speed:<R>%i", g_opt.scrollRate);
    add("Graphic Detail\n\tGraphic Detail Level:<R>%s", kDetail[g_opt.spiffyDraw < 0 || g_opt.spiffyDraw > 2 ? 2 : g_opt.spiffyDraw]);
    add("Play Music<R>%s", onOff(g_opt.music));
    add("Play Sound Effects<R>%s", onOff(g_opt.sound));
    add("Display Ambient Animations<R>%s", onOff(g_opt.ambient));
    add("Display Computer Overlay<R>%s", onOff(g_opt.overlay));
    add("Display Structure Shadows<R>%s", onOff(g_opt.stationaryShadows));
    add("Display Vehicle Shadows<R>%s", onOff(g_opt.mobileShadows));
    add("Display Completed Objectives<R>%s", onOff(objectivesShowCompleted() ? 1 : 0));
    add("Skip Frames<R>%s", onOff(g_opt.frameSkip > 1));
    // Extra deste porte: o modo de tela, em duas linhas de radio com o mesmo
    // desenho dos dialogos do shell ("(o) " escolhido, "( ) " nao -- ver
    // shell_menu.cpp). O nome do grupo vai na primeira linha, que e o arranjo
    // de duas linhas que os itens com valor ja usam aqui.
    const bool cheia = g_opt.fullscreen != 0;
    out.push_back(std::string("Screen Mode\n\t") + (cheia ? "(o) " : "( ) ") + "Full Screen");
    out.push_back(std::string("\t") + (cheia ? "( ) " : "(o) ") + "Window");
    return out;
}

ListBoxLayout GameOptionsPane::layout(const PaneRect& pane) const {
    // Nas telas do manual os botoes ficam EMBAIXO (nao ha onde empilha-los em
    // cima, a lista ocupa o painel), entao a lista encurta para caber: um
    // botao em MANUAL, dois em MANUALTEXT, 29 px cada.
    PaneRect r = pane;
    if (m_mode == MANUAL) r.b -= 29;
    else if (m_mode == MANUALTEXT) r.b -= 58;
    return listBoxLayout(r, 0, 0x0A, 0, 0x12);
}

void GameOptionsPane::bump(int i, int dir) {
    switch (i) {
    case OPT_MUSIC_VOL:  g_opt.musicVol  += dir; break;
    case OPT_SOUND_VOL:  g_opt.soundVol  += dir; break;
    case OPT_GAME_SPEED: g_opt.gameSpeed += dir; break;
    case OPT_SCROLL_RATE:g_opt.scrollRate+= dir; break;
    case OPT_DETAIL:     g_opt.spiffyDraw+= dir; break;
    case OPT_MUSIC:      g_opt.music = !g_opt.music; break;
    case OPT_SOUND:      g_opt.sound = !g_opt.sound; break;
    case OPT_AMBIENT:    g_opt.ambient = !g_opt.ambient; break;
    case OPT_OVERLAY:    g_opt.overlay = !g_opt.overlay; break;
    case OPT_STRUCT_SHADOW: g_opt.stationaryShadows = !g_opt.stationaryShadows; break;
    case OPT_VEH_SHADOW:    g_opt.mobileShadows = !g_opt.mobileShadows; break;
    case OPT_DONE_OBJECTIVES: objectivesSetShowCompleted(!objectivesShowCompleted()); return;   // dword_57563C
    case OPT_FRAME_SKIP: g_opt.frameSkip = g_opt.frameSkip > 1 ? 1 : 2; break;
    // Radio, nao alternancia: cada linha escolhe o SEU modo. Quem aplica na
    // janela e o GameState no quadro seguinte -- o painel nao tem o Renderer.
    case OPT_SCREEN_FULL:   g_opt.fullscreen = 1; break;
    case OPT_SCREEN_WINDOW: g_opt.fullscreen = 0; break;
    default: return;
    }
    clampAll();
    // NÃO grava aqui: no original o que se mexe só vale no "&APPLY".
}

// O título da tela em que se está. São cinco, um por modo de sub_465830:
// off_4E8B28 "Game Options", off_4E8B2C (objetivos), off_4E8B30 "Game
// Preferences", off_4E8B34 "Game End: Single Player" e off_4E8B38 "Game End:
// Multiplayer" -- a tela de fim de missão diz de que partida se trata, e não
// "End Mission".
std::string GameOptionsPane::paneTitle() const {
    switch (m_mode) {
    case PREFS:      return "Game Preferences";
    case ENDMISSION: return multiplayerMission() ? "Game End: Multiplayer"
                                                 : "Game End: Single Player";
    case MANUAL:     return "Outpost 2 Online Manual";
    case MANUALTEXT:
        if (m_help.ok() && m_topic >= 0 && m_topic < static_cast<int>(m_help.topics().size()))
            return m_help.topics()[m_topic].title;
        return std::string();
    default:         return "Game Options";
    }
}

void GameOptionsPane::draw(uint32_t* fb, int fbW, int fbH, const PaneRect& pane, int mx, int my) const {
    if (!m_open) return;
    const ListBoxLayout L = layout(pane);
    const std::string titulo = paneTitle();
    drawMarkupText(fb, fbW, fbH, PaneRect{pane.l + 0xD, pane.t + 2, pane.r - 0xE, L.list.t},
                   titulo, 0xFF00FF00u, true, kMarkupLine);
    if (m_mode == PREFS || m_mode == MANUAL || m_mode == MANUALTEXT) {
        m_list.draw(fb, fbW, fbH, L, pane, items(), mx, my);
        return;
    }
    // MENU e ENDMISSION são só botões (montados pelo GameState); aqui sobra a
    // versão no rodapé: o original escreve com aVersion ("Version "); aqui a
    // linha diz "Version 1.0 - beta port", por ser um porte em andamento.
    if (m_mode == MENU)
        drawMarkupText(fb, fbW, fbH, PaneRect{pane.l + 0xD, pane.b - 18, pane.r - 0xE, pane.b},
                       "Version 1.0 - beta port", 0xFF00FF00u, false, kMarkupLine);
}

bool GameOptionsPane::click(const PaneRect& pane, int x, int y, bool& changed, bool& back) {
    changed = false;
    back = false;
    // MENU e ENDMISSION são só botões; as telas com lista respondem ao clique.
    if (!m_open || (m_mode != PREFS && m_mode != MANUAL && m_mode != MANUALTEXT)) return false;
    const ListBoxLayout L = layout(pane);
    int item = -1;
    if (m_mode == MANUAL) {
        if (!m_list.click(L, items(), x, y, item)) return false;
        if (item >= 0 && m_help.ok() && item < static_cast<int>(m_help.topics().size())) {
            m_topic = item;
            m_mode = MANUALTEXT;
            m_list.reset();
        }
        return true;
    }
    if (m_mode == MANUALTEXT) { m_list.click(L, items(), x, y, item); return true; }  // só rola
    if (m_list.click(L, items(), x, y, item)) {
        if (item >= 0 && item < OPT_COUNT) {
            m_list.sel = item;
            if (!valueItem(item)) { bump(item, 0); changed = true; }   // alternância no próprio clique
        }
        return true;
    }
    return false;
}

} // namespace op2
