// As telas de multijogador. No original elas NAO vem do shell: o botao
// MULTIPLAYER (1005 do Dialog 137) chama TApp::DoNetGame (sub_487C40) no
// executavel, que abre os dialogos do OUT2RES.DLL nesta ordem:
//
//   259 "PICK CONNECTION METHOD"  sub_411E30 -> escolhe o transporte
//       (INTERNET/SIGS 6, MODEM 4, IPX 5, TCP/IP 3, SERIAL 7, CANCEL 2); a
//       tabela unk_4DF370 liga cada botao ao objeto do transporte. Aqui a tela
//       so oferece o TCP/IP: os outros quatro meios nao existem mais fora do
//       jogo (ver o comentario do caso MpConnection em shell_menu.cpp).
//   260 "NET GAME OPTIONS"        sub_497610 -> CREATE (1074) ou JOIN (1075)
//  2012 "NEW NETWORK GAME"        sub_40DEC0 -> nome, senha, maximo de
//       jogadores (combo 1015) e cenario (combo 1039); chama sub_491BE0.
//  2010 "FIND SESSION"            -> broadcast local (radio 1005) ou um
//       endereco IP (radio 1007 + edit 1008)
//   195 "PICK SESSION"            -> a lista do que a descoberta trouxe
//  2013 "JOIN A MULTIPLAYER GAME" -> o nome do jogador
//   196 "GAME SETTINGS"           sub_45EF90 -> o lobby da partida
//
// No GAME SETTINGS cada linha de jogador tem 6 controles, cujos ids estao na
// tabela dword_4E2FA0 (base 40000 + 5*i, e o EJECT em 1046+i):
//   +0 40000+5i Static   nome
//   +1 40001+5i ComboBox cor       -> playerWord bits 5-7
//   +2 40003+5i Button   Eden      -> playerWord bit 2
//   +3 40002+5i ComboBox Resources -> playerWord bits 3-4 (High/Med/Low)
//   +4 40004+5i Button   Ready?
//   +5 1046+i   Button   EJECT
//
// NAO HA selecao de IA nem de dificuldade nesta tela: sub_4893A0 faz GoHuman
// para CADA lugar em multijogador, e sub_45EF90 grava os bits 3-4 com o valor 1
// de saida (o "Med" do combo Resources, que e o mesmo campo que
// _Player::Difficulty le na campanha como "Normal").
//
// As opcoes globais vao nos flags do GameStartInfo (sub_460240 / loc_46105E):
//   bit 0  (0x1)      checkbox 1077 "Disasters"
//   bit 1  (0x2)      checkbox 1079 "Day and Night"
//   bit 2  (0x4)      checkbox 1078 "Morale Steady"
//   bits 6-8 (0x1C0)  maximo de jogadores
//   bits 17-20        combo 1018 "Initial Vehicles"

#include "shell_menu.h"

#include <algorithm>
#include <functional>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <cstdio>
#include <iostream>

#include <SDL.h>

#include "../audio/sound.h"
#include "../gfx/font.h"
#include "../res/pe_resource.h"

namespace op2 {

const std::vector<std::string> kColorNames = {
    "Blue", "Red", "Green", "Yellow", "Cyan", "Magenta"
};
const std::vector<std::string> kResourceNames = { "High", "Med", "Low" };
const std::vector<std::string> kMaxPlayerNames = { "2", "3", "4", "5", "6" };
const std::vector<std::string> kVehicleNames = { "0", "1", "2", "3", "4", "5" };
const std::vector<std::string> kAiLevelNames = { "AUTO", "Easy", "Normal", "Hard" };

namespace {

// Altura da barra de título que o drawDialogFrame desenha (3 px de folga, 18
// de faixa, 3 de folga), que o cliente do diálogo tem de pular.
constexpr int kTitleBarH = 24;
constexpr int kFrameEdge = 4;      // a borda dupla de cada lado

} // namespace

DlgBox dlgBox(int cxDlu, int cyDlu) {
    DlgBox b;
    b.fw = cxDlu * b.sxn / b.sxd + kFrameEdge * 2;
    b.fh = cyDlu * b.syn / b.syd + kFrameEdge * 2 + kTitleBarH;
    b.fx = (640 - b.fw) / 2;
    b.fy = (480 - b.fh) / 2;
    b.ox = b.fx + kFrameEdge;
    b.oy = b.fy + kFrameEdge + kTitleBarH;
    return b;
}

DlgBox dlgBoxFull(int cxDlu, int cyDlu) {
    DlgBox b;
    b.sxn = 2; b.sxd = 1;
    b.syn = 2; b.syd = 1;
    b.fx = 0; b.fy = 0; b.fw = 640; b.fh = 480;
    b.ox = (640 - cxDlu * 2) / 2;
    b.oy = kTitleBarH;
    (void)cyDlu;                   // o quadro é a tela; só a origem importa
    return b;
}

// ---------------------------------------------------------------------------
// Os controles que as telas de multijogador usam e as outras ainda nao tinham.

Control& ShellMenu::addCheck(int id, const std::string& text, int x, int y, int w,
                             bool* tgt) {
    Control c;
    c.kind = CtrlKind::Check; c.id = id; c.text = text;
    c.x = x; c.y = y; c.w = w; c.h = 14;
    c.checkTarget = tgt;
    m_controls.push_back(c);
    return m_controls.back();
}

Control& ShellMenu::addRadio(int id, const std::string& text, int x, int y, int w,
                             int* tgt, int value) {
    Control c;
    c.kind = CtrlKind::Radio; c.id = id; c.text = text;
    c.x = x; c.y = y; c.w = w; c.h = 14;
    c.radioTarget = tgt; c.radioValue = value;
    m_controls.push_back(c);
    return m_controls.back();
}

Control& ShellMenu::addCombo(int id, const std::vector<std::string>* items, int* sel,
                             int x, int y, int w, int h) {
    Control c;
    c.kind = CtrlKind::Combo; c.id = id;
    c.x = x; c.y = y; c.w = w; c.h = h;
    c.comboItems = items; c.comboSel = sel;
    m_controls.push_back(c);
    return m_controls.back();
}

Control& ShellMenu::addEdit(int id, std::string* tgt, size_t maxLen,
                            int x, int y, int w, int h) {
    Control c;
    c.kind = CtrlKind::Edit; c.id = id;
    c.x = x; c.y = y; c.w = w; c.h = h;
    c.editTarget = tgt; c.editMax = maxLen;
    m_controls.push_back(c);
    return m_controls.back();
}

void ShellMenu::showMpScreen(int which, bool host, int players) {
    mpLoadScenarios();
    m_mpIsHost = host;
    m_mpPlayerCount = players;
    m_mpStatus.clear();
    for (int i = 0; i < 6; ++i)
        if (m_mpNames[i].empty()) m_mpNames[i] = "PLAYER " + std::to_string(i + 1);
    if (which == 6) {
        m_mpChat.clear();
        m_mpChat.push_back("Partida criada: OUTPOST 2");
        m_mpChat.push_back("PLAYER 2 entrou na mesa");
        m_mpStatus = host ? "Aguardando jogadores..." : "Aguardando o host comecar";
    }
    if (which == 4 && m_mpSessions.empty()) {
        // Uma linha de exemplo, para a tela poder ser conferida sem rede.
        LobbySession s;
        s.addr = 0x0101A8C0;            // 192.168.1.1
        s.name = "OUTPOST 2";
        s.maxPlayers = 2;
        s.pingMs = 17; s.replies = 1;
        m_mpSessions.push_back(s);
    }
    static const MenuState kScreens[] = {
        MenuState::MpConnection, MenuState::MpNetOptions, MenuState::MpNewGame,
        MenuState::MpFindSession, MenuState::MpPickSession, MenuState::MpJoinName,
        MenuState::MpGameSettings
    };
    setState(kScreens[which < 0 ? 0 : (which > 6 ? 6 : which)]);
}

void ShellMenu::debugOpenCombo(int id) {
    for (int i = 0; i < (int)m_controls.size(); ++i)
        if (m_controls[i].id == id && m_controls[i].kind == CtrlKind::Combo) {
            comboOpen(i);
            m_focusIndex = i;
            return;
        }
}

// Exercita a lista suspensa pelo mesmo caminho do usuario: um clique no combo
// a solta, um clique num item escolhe, e com ela fechada as setas trocam a
// opcao. Nada aqui chama comboOpen/comboClose direto.
bool ShellMenu::comboTest(std::string& report) {
    auto step = [&](const std::string& what) { report += what; report += "; "; };

    showMpScreen(6, true, 2);                 // GAME SETTINGS como host
    int idx = -1;
    for (int i = 0; i < (int)m_controls.size(); ++i)
        if (m_controls[i].id == 40001) { idx = i; break; }   // cor do jogador 1
    if (idx < 0) { report += "nao achei o combo de cor"; return false; }
    const Control c = m_controls[idx];
    const int antes = m_mpColor[0];

    // Clique no combo: o botao solta no mouse-up, como no Windows.
    handleMouseDown(c.x + 4, c.y + 4, SDL_BUTTON_LEFT);
    handleMouseUp(c.x + 4, c.y + 4, SDL_BUTTON_LEFT);
    if (m_openCombo < 0) { report += "o clique nao abriu a lista"; return false; }
    step("a lista abriu");

    int lx, ly, lw, lh;
    if (!comboRect(m_controls[m_openCombo], lx, ly, lw, lh)) {
        report += "a lista nao tem caixa"; return false;
    }
    // Escolhe um item diferente do atual.
    const int alvo = (antes + 2) % (int)kColorNames.size();
    const int iy = ly + 2 + (alvo - m_comboScroll) * 15 + 2;
    handleMouseMove(lx + 4, iy);
    if (m_comboHover != alvo) {
        report += "o cursor nao destacou o item " + std::to_string(alvo);
        return false;
    }
    step("o cursor destacou " + kColorNames[alvo]);

    handleMouseDown(lx + 4, iy, SDL_BUTTON_LEFT);
    handleMouseUp(lx + 4, iy, SDL_BUTTON_LEFT);
    if (m_openCombo >= 0) { report += "a lista nao fechou"; return false; }
    if (m_mpColor[0] != alvo) {
        report += "a escolha nao pegou: " + std::to_string(m_mpColor[0]);
        return false;
    }
    step("escolheu " + kColorNames[alvo] + " e fechou");

    // Com a lista fechada, a seta troca a opcao direto (padrao do Windows).
    m_focusIndex = idx;
    handleKeyDown(SDLK_DOWN);
    if (m_mpColor[0] != alvo + 1) {
        report += "a seta nao trocou a opcao"; return false;
    }
    step("a seta trocou para " + kColorNames[m_mpColor[0]]);

    // Esc com a lista aberta desiste sem mudar nada.
    const int mantido = m_mpColor[0];
    handleMouseDown(c.x + 4, c.y + 4, SDL_BUTTON_LEFT);
    handleMouseUp(c.x + 4, c.y + 4, SDL_BUTTON_LEFT);
    if (m_openCombo < 0) { report += "nao reabriu"; return false; }
    handleKeyDown(SDLK_DOWN);
    handleKeyDown(SDLK_ESCAPE);
    if (m_openCombo >= 0) { report += "Esc nao fechou"; return false; }
    if (m_mpColor[0] != mantido) { report += "Esc mudou a opcao"; return false; }
    step("Esc desistiu sem mudar");

    // Um clique fora fecha sem escolher.
    handleMouseDown(c.x + 4, c.y + 4, SDL_BUTTON_LEFT);
    handleMouseUp(c.x + 4, c.y + 4, SDL_BUTTON_LEFT);
    handleMouseDown(4, 4, SDL_BUTTON_LEFT);
    if (m_openCombo >= 0) { report += "o clique fora nao fechou"; return false; }
    if (m_mpColor[0] != mantido) { report += "o clique fora mudou a opcao"; return false; }
    step("o clique fora fechou");

    // Com a lista fechada, a letra salta para a opcao que comeca por ela.
    m_focusIndex = idx;
    handleTextInput("g");
    if (kColorNames[m_mpColor[0]] != "Green") {
        report += "a letra nao saltou: " + kColorNames[m_mpColor[0]];
        return false;
    }
    step("a letra G saltou para Green");

    // E com a lista aberta a letra so move o destaque, sem escolher.
    handleMouseDown(c.x + 4, c.y + 4, SDL_BUTTON_LEFT);
    handleMouseUp(c.x + 4, c.y + 4, SDL_BUTTON_LEFT);
    handleTextInput("m");
    if (m_openCombo < 0) { report += "a letra fechou a lista"; return false; }
    if (kColorNames[m_comboHover] != "Magenta") {
        report += "a letra nao destacou Magenta";
        return false;
    }
    if (kColorNames[m_mpColor[0]] != "Green") {
        report += "a letra escolheu antes do Enter";
        return false;
    }
    handleKeyDown(SDLK_RETURN);
    if (kColorNames[m_mpColor[0]] != "Magenta") {
        report += "o Enter nao confirmou"; return false;
    }
    step("na lista aberta, M destacou e o Enter confirmou");

    // A lista do cenario tem mais itens que a janela: a rolagem tem de andar.
    debugOpenCombo(40032);
    if (m_openCombo >= 0 && (int)m_mpScenarioList.size() > 8) {
        const int antesScroll = m_comboScroll;
        handleMouseWheel(-3);
        if (m_comboScroll == antesScroll) { report += "a roda nao rolou"; return false; }
        step("a roda rolou a lista de cenarios");
    }
    handleKeyDown(SDLK_ESCAPE);
    return true;
}

// --uitest: exercita a barra do volume do PREFERENCES pelo mesmo caminho do
// mouse e do teclado, e confere que o mixer recebeu cada mudanca -- o valor da
// tela nao serve de prova se o som nao acompanha.
bool ShellMenu::sliderTest(std::string& report) {
    auto step = [&](const std::string& what) { report += what; report += "; "; };

    showPreferences();
    int idx = -1;
    for (int i = 0; i < (int)m_controls.size(); ++i)
        if (m_controls[i].id == 1201) { idx = i; break; }
    if (idx < 0) { report += "nao achei a barra do volume"; return false; }
    const Control c = m_controls[idx];

    // Clique no meio da barra: o cursor vai para la e o som acompanha.
    const int meio = c.x + c.w / 2;
    handleMouseDown(meio, c.y + c.h / 2, SDL_BUTTON_LEFT);
    if (m_prefAudioVolume < 48 || m_prefAudioVolume > 52) {
        report += "o clique no meio deu " + std::to_string(m_prefAudioVolume);
        return false;
    }
    if (soundMasterVolume() != m_prefAudioVolume) {
        report += "o mixer ficou em " + std::to_string(soundMasterVolume());
        return false;
    }
    step("o clique no meio deu " + std::to_string(m_prefAudioVolume) + "%");

    // Arrasto: o cursor segue o mouse, inclusive para fora do controle, e as
    // pontas dao exatamente 0 e 100.
    handleMouseMove(c.x + c.w / 4, c.y + c.h / 2);
    const int quarto = m_prefAudioVolume;
    if (quarto < 20 || quarto > 30) {
        report += "o arrasto ao quarto deu " + std::to_string(quarto);
        return false;
    }
    handleMouseMove(c.x + c.w + 500, c.y + 200);
    if (m_prefAudioVolume != 100) { report += "a ponta direita nao deu 100"; return false; }
    handleMouseMove(c.x - 500, c.y + c.h / 2);
    if (m_prefAudioVolume != 0) { report += "a ponta esquerda nao deu 0"; return false; }
    if (soundMasterVolume() != 0) { report += "o mixer nao zerou"; return false; }
    step("o arrasto passou por " + std::to_string(quarto) + "%, 100% e 0%");

    handleMouseUp(c.x - 500, c.y, SDL_BUTTON_LEFT);
    if (m_dragSlider >= 0) { report += "o arrasto nao terminou"; return false; }

    // Teclado: a seta anda um degrau, Page um salto, End vai ao maximo.
    m_focusIndex = idx;
    handleKeyDown(SDLK_RIGHT);
    if (m_prefAudioVolume != 5) {
        report += "a seta deu " + std::to_string(m_prefAudioVolume); return false;
    }
    handleKeyDown(SDLK_PAGEDOWN);
    if (m_prefAudioVolume != 25) {
        report += "o Page deu " + std::to_string(m_prefAudioVolume); return false;
    }
    handleKeyDown(SDLK_END);
    if (m_prefAudioVolume != 100) { report += "o End nao foi ao maximo"; return false; }
    handleKeyDown(SDLK_HOME);
    if (m_prefAudioVolume != 0) { report += "o Home nao foi ao minimo"; return false; }
    step("as teclas andaram 5, 20, End e Home");

    // A roda do mouse sobre a barra tambem anda um degrau.
    handleMouseMove(meio, c.y + c.h / 2);
    handleMouseWheel(+3);
    if (m_prefAudioVolume != 15) {
        report += "a roda deu " + std::to_string(m_prefAudioVolume); return false;
    }
    step("a roda andou 15%");

    // Sair sem ACCEPT devolve o som ao que esta gravado, e o ACCEPT grava.
    const int gravado = m_prefAudioVolumeIni;
    showMainMenu();
    if (soundMasterVolume() != gravado) {
        report += "sair sem ACCEPT deixou o som em " + std::to_string(soundMasterVolume());
        return false;
    }
    step("sair sem ACCEPT devolveu o som a " + std::to_string(gravado) + "%");
    return true;
}

// Percorre o fluxo das telas chamando o mesmo despacho dos botoes, para que o
// teste exercite o caminho de verdade e nao um atalho: MULTIPLAYER -> TCP/IP
// -> CREATE/JOIN -> (nome/busca) -> GAME SETTINGS, e la dentro exercita a
// sincronizacao: o host muda opcoes e o cliente tem de ve-las, o cliente marca
// Ready e o host tem de ve-lo, e os dois conversam pelo chat.
bool ShellMenu::mpFlowTest(bool host, const std::string& name, std::string& report) {
    auto step = [&](const std::string& what) { report += what; report += "; "; };
    // Espera uma condicao bombeando o lobby, como a tela faz a cada quadro.
    auto waitFor = [&](const std::function<bool()>& cond, int ms) {
        const uint32_t fim = SDL_GetTicks() + uint32_t(ms);
        while (SDL_GetTicks() < fim) {
            mpPump();
            if (cond()) return true;
            SDL_Delay(5);
        }
        return cond();
    };

    showMultiplayer();
    if (m_state != MenuState::MpConnection) { report += "nao abriu o 259"; return false; }
    step("259");

    dispatchButton(3);                       // TCP/IP
    if (m_state != MenuState::MpNetOptions) { report += "nao abriu o 260"; return false; }
    step("260");

    m_mpPlayerName = name;
    if (host) {
        dispatchButton(1074);                // CREATE
        if (m_state != MenuState::MpNewGame) { report += "nao abriu o 2012"; return false; }
        step("2012");
        dispatchButton(1);                   // OK -> abre o lobby
        if (m_state != MenuState::MpGameSettings) {
            report += "o host nao chegou ao 196: " + m_mpStatus;
            return false;
        }
        step("196 (host)");
        if (!waitFor([&] { return m_mpPlayerCount >= 2; }, 30000)) {
            report += "ninguem entrou"; return false;
        }
        step("o cliente entrou");

        // O host muda opcoes da mesa; elas tem de chegar ao outro lado.
        m_mpDisasters = true;
        m_mpDayNight = true;
        if (m_mpScenarioList.size() > 1) m_mpScenario = 1;
        mpControlChanged(1077);
        step("host mudou desastres/dia-noite/cenario");

        mpSendChat("HOST: ola");
        if (!waitFor([&] {
                for (const std::string& l : m_mpChat)
                    if (l.rfind("JOIN:", 0) == 0) return true;
                return false;
            }, 15000)) {
            report += "o chat do cliente nao chegou"; return false;
        }
        step("chat do cliente recebido");

        if (!waitFor([&] { return m_mpReady[1]; }, 15000)) {
            report += "o Ready do cliente nao chegou"; return false;
        }
        step("Ready do cliente recebido");
    } else {
        dispatchButton(1075);                // JOIN
        if (m_state != MenuState::MpFindSession) { report += "nao abriu o 2010"; return false; }
        step("2010");
        // Procura pelo endereco (radio 1007), que e o modo dirigido de
        // sub_491860 — assim o teste cobre os dois caminhos de busca.
        m_mpFindMode = 1;
        m_mpFindAddr = "127.0.0.1";
        dispatchButton(1);
        if (m_state != MenuState::MpPickSession) { report += "nao abriu o 195"; return false; }
        if (m_mpSessions.empty()) { report += "nenhuma sessao por endereco"; return false; }
        step("195 por endereco: " + std::to_string(m_mpSessions.size()));
        dispatchButton(1);                   // OK -> nome
        if (m_state != MenuState::MpJoinName) { report += "nao abriu o 2013"; return false; }
        step("2013");
        dispatchButton(1);                   // OK -> entra
        if (m_state != MenuState::MpGameSettings) {
            report += "nao entrou: " + m_mpStatus;
            return false;
        }
        step("196 (cliente)");

        // As opcoes que o host mudou tem de aparecer aqui.
        if (!waitFor([&] { return m_mpDisasters && m_mpDayNight; }, 20000)) {
            report += "as opcoes do host nao chegaram"; return false;
        }
        step("opcoes do host recebidas");

        mpSendChat("JOIN: ola");
        if (!waitFor([&] {
                for (const std::string& l : m_mpChat)
                    if (l.rfind("HOST:", 0) == 0) return true;
                return false;
            }, 15000)) {
            report += "o chat do host nao chegou"; return false;
        }
        step("chat do host recebido");

        // Marca Ready, que e o controle 40004+5i do proprio lugar.
        const int me = 1;
        m_mpReady[me] = true;
        mpControlChanged(40000 + 5 * me + 4);
        step("Ready marcado");
    }

    // Com `ejectTest`, o host remove o visitante em vez de comecar, e o outro
    // lado tem de receber o aviso (o 1046+i do GAME SETTINGS).
    if (m_mpEjectTest) {
        if (host) {
            dispatchButton(1047);            // EJECT do lugar 1
            if (m_mpPlayerCount != 1) {
                report += "a mesa nao voltou a 1 lugar: " +
                          std::to_string(m_mpPlayerCount);
                return false;
            }
            step("EJECT feito, mesa com 1");
        } else {
            if (!waitFor([&] { return m_mpEjected; }, 15000)) {
                report += "o aviso de EJECT nao chegou"; return false;
            }
            step("EJECT recebido");
        }
        return true;
    }

    if (host) {
        dispatchButton(1045);                // START
        if (!m_netLaunch) { report += "o START nao pediu a partida: " + m_mpStatus; return false; }
        step("START aceito");
    } else {
        // O cliente nao aperta START: ele recebe o aviso do host.
        if (!waitFor([&] { return m_netLaunch; }, 20000)) {
            report += "o aviso de START nao chegou"; return false;
        }
        step("START recebido do host");
    }
    return true;
}

void ShellMenu::showMultiplayer() {
    mpLoadScenarios();
    m_mpStatus.clear();
    setState(MenuState::MpConnection);
}

// Os cenários do multijogador são as missões M* (MF/ML/MM/MR/MU), lidas pelo
// export LevelDesc como nas listas de colônia e tutorial.
void ShellMenu::mpLoadScenarios() {
    if (!m_mpScenarioList.empty()) return;
    std::error_code ec;
    std::vector<std::string> files;
    for (const auto& de : std::filesystem::directory_iterator(".", ec)) {
        if (ec) break;
        const std::string n = de.path().filename().string();
        if (n.size() < 5) continue;
        const char c0 = char(std::tolower((unsigned char)n[0]));
        std::string ext = n.substr(n.size() - 4);
        for (char& ch : ext) ch = char(std::tolower((unsigned char)ch));
        if (c0 == 'm' && ext == ".dll") files.push_back(n);
    }
    std::sort(files.begin(), files.end());
    for (const std::string& n : files) {
        PeResourceReader m;
        if (!m.load(n)) continue;
        const std::string desc = m.getExportString("LevelDesc");
        if (desc.empty()) continue;
        m_mpScenarioList.push_back(desc);
        m_mpScenarioDlls.push_back(n);
    }
    std::cout << "[MpMenu] " << m_mpScenarioList.size()
              << " cenarios multijogador lidos das missoes M*.DLL" << std::endl;
}

// Passa o que está nos controles para o GameStartInfo, nos bits do original.
void ShellMenu::mpSyncGsi() {
    uint32_t flags = 0x10;                      // multijogador
    if (m_mpDisasters)    flags |= 0x1;
    if (m_mpDayNight)     flags |= 0x2;
    if (m_mpMoraleSteady) flags |= 0x4;
    flags |= uint32_t((m_mpMaxPlayers + 2) & 7) << 6;
    flags |= uint32_t(m_mpInitialVehicles & 0xF) << 17;
    m_mpGsi.flags = flags;
    for (int i = 0; i < 6; ++i) {
        uint32_t w = 0;
        if (m_mpEden[i]) w |= 0x4;
        w |= uint32_t(m_mpResources[i] & 3) << 3;
        w |= uint32_t(m_mpColor[i] & 7) << 5;
        m_mpGsi.playerWord[i] = w;
    }
    if (m_mpScenario >= 0 && m_mpScenario < (int)m_mpScenarioDlls.size())
        m_mpGsi.dll = m_mpScenarioDlls[m_mpScenario];
    m_mpGsi.missionNumber = -(4 + std::min(4, m_mpMaxPlayers));
    m_mpGsi.localPlayer = netActive() ? netLocalPlayer() : (m_mpIsHost ? 0 : 1);
}

bool ShellMenu::mpStartHost() {
    mpSyncGsi();
    std::string err;
    if (!lobbyHostStart(m_mpSessionName, m_mpMaxPlayers + 2, m_mpPassword, err,
                        m_mpGsi.flags)) {
        m_mpStatus = "Nao foi possivel abrir a partida: " + err;
        return false;
    }
    m_mpIsHost = true;
    m_mpNames[0] = m_mpPlayerName;
    m_mpPlayerCount = 1;
    m_mpStatus = "Aguardando jogadores...";
    m_mpChat.clear();
    m_mpChat.push_back("Partida criada: " + m_mpSessionName);
    return true;
}

bool ShellMenu::mpFindSessions() {
    std::string err;
    std::vector<LobbySession> found;
    // O original manda 8 rajadas com 250 ms entre elas (sub_491710). Aqui a
    // tela fica parada esses dois segundos, como o diálogo de progresso
    // "Finding Local Sessions" do original.
    if (!lobbyFind(m_mpPassword, 8, found, err,
                   m_mpFindMode == 1 ? m_mpFindAddr : std::string())) {
        m_mpStatus = "A busca falhou: " + err;
        return false;
    }
    m_mpSessions = found;
    m_mpSelSession = 0;
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%d sessao(oes) encontrada(s)", (int)found.size());
    m_mpStatus = buf;
    return !found.empty();
}

bool ShellMenu::mpJoinSelected() {
    if (m_mpSelSession < 0 || m_mpSelSession >= (int)m_mpSessions.size()) return false;
    const LobbySession& s = m_mpSessions[m_mpSelSession];
    std::string err;
    int player = -1;
    uint16_t hostPort = 0;
    if (!lobbyJoin(s, m_mpPassword, player, hostPort, err)) {
        m_mpStatus = "Entrada recusada: " + err;
        return false;
    }
    m_mpIsHost = false;
    m_mpStatus = "Entrou como jogador " + std::to_string(player);
    // O host manda os flags da partida no pacote de descoberta (+0x2A), então
    // as opções já aparecem certas antes de a partida começar.
    m_mpDisasters    = (s.settingsFlags & 0x1) != 0;
    m_mpDayNight     = (s.settingsFlags & 0x2) != 0;
    m_mpMoraleSteady = (s.settingsFlags & 0x4) != 0;
    m_mpMaxPlayers   = std::max(0, int((s.settingsFlags >> 6) & 7) - 2);
    m_mpInitialVehicles = int((s.settingsFlags >> 17) & 0xF);
    m_mpPlayerCount = player + 1;
    if (player >= 0 && player < 6) m_mpNames[player] = m_mpPlayerName;
    m_mpChat.clear();
    m_mpChat.push_back("Entrou em: " + s.name);
    return true;
}

// O estado da mesa que o host difunde, e o caminho de volta.
void ShellMenu::mpUiToTable(LobbyTable& t) const {
    t.flags = 0x10;
    if (m_mpDisasters)    t.flags |= 0x1;
    if (m_mpDayNight)     t.flags |= 0x2;
    if (m_mpMoraleSteady) t.flags |= 0x4;
    t.flags |= uint32_t((m_mpMaxPlayers + 2) & 7) << 6;
    t.flags |= uint32_t(m_mpInitialVehicles & 0xF) << 17;
    t.scenario = m_mpScenario;
    for (int i = 0; i < 6; ++i) {
        t.slots[i].name      = m_mpNames[i];
        t.slots[i].color     = uint8_t(m_mpColor[i]);
        t.slots[i].eden      = uint8_t(m_mpEden[i] ? 1 : 0);
        t.slots[i].resources = uint8_t(m_mpResources[i]);
        t.slots[i].ready     = uint8_t(m_mpReady[i] ? 1 : 0);
        t.slots[i].occupied  = uint8_t(i < m_mpPlayerCount ? 1 : 0);
    }
}

void ShellMenu::mpTableToUi(const LobbyTable& t) {
    m_mpDisasters    = (t.flags & 0x1) != 0;
    m_mpDayNight     = (t.flags & 0x2) != 0;
    m_mpMoraleSteady = (t.flags & 0x4) != 0;
    m_mpMaxPlayers   = std::max(0, int((t.flags >> 6) & 7) - 2);
    m_mpInitialVehicles = int((t.flags >> 17) & 0xF);
    m_mpScenario = t.scenario;
    int live = 0;
    const int me = netActive() ? netLocalPlayer() : 1;
    for (int i = 0; i < 6; ++i) {
        // O nosso proprio lugar nao e sobrescrito: o que vale para ele e o que
        // esta na nossa tela, e o host ja o recebeu por kLobbyMsgChange.
        if (i != me) {
            m_mpNames[i]     = t.slots[i].name;
            m_mpColor[i]     = t.slots[i].color;
            m_mpEden[i]      = t.slots[i].eden != 0;
            m_mpResources[i] = t.slots[i].resources;
            m_mpReady[i]     = t.slots[i].ready != 0;
        }
        if (t.slots[i].occupied) ++live;
    }
    m_mpPlayerCount = std::max(1, live);
}

void ShellMenu::mpSendChat(const std::string& line) {
    m_mpChat.push_back(line);
    lobbyMsgSendChat(line);
}

// Um controle do GAME SETTINGS mudou. Em sub_45EF90 quem hospeda manda a mesa
// inteira e os outros avisam so o que mexeram (loc_460FAC).
void ShellMenu::mpControlChanged(int id) {
    const int me = netActive() ? netLocalPlayer() : (m_mpIsHost ? 0 : 1);
    if (m_mpIsHost) {
        LobbyTable t;
        mpUiToTable(t);
        lobbyMsgSendTable(t);
        return;
    }
    // Só os controles do próprio lugar sobem; o resto é visual.
    const int base = 40000 + 5 * me;
    if (id == base + 1)      lobbyMsgSendChange(me, 1, m_mpColor[me]);
    else if (id == base + 3) lobbyMsgSendChange(me, 3, m_mpEden[me] ? 1 : 0);
    else if (id == base + 2) lobbyMsgSendChange(me, 2, m_mpResources[me]);
    else if (id == base + 4) lobbyMsgSendChange(me, 4, m_mpReady[me] ? 1 : 0);
}

// Enquanto o GAME SETTINGS está no ar o host precisa continuar respondendo às
// consultas e aos pedidos de entrada (os threads sub_48C1F0 / sub_48C300) e os
// dois lados trocam o estado da mesa pelo canal da partida.
void ShellMenu::mpPump() {
    if (lobbyHostActive()) {
        lobbyHostPump();
        const int n = lobbyHostActiveCount();
        if (n != m_mpPlayerCount) {
            for (int i = m_mpPlayerCount; i < n && i < 6; ++i)
                if (m_mpNames[i].empty()) m_mpNames[i] = "PLAYER " + std::to_string(i + 1);
            m_mpPlayerCount = n;
            mpSendChat("Jogadores na mesa: " + std::to_string(n));
            m_mpStatus = (n >= m_mpMaxPlayers + 2) ? "Mesa cheia - pode comecar"
                                                   : "Aguardando jogadores...";
            LobbyTable t;
            mpUiToTable(t);
            lobbyMsgSendTable(t);
        }
        // Repete a mesa de tempos em tempos: UDP perde pacote, e quem acabou de
        // entrar precisa de um estado inteiro de qualquer jeito.
        const uint32_t now = SDL_GetTicks();
        if (now - m_mpLastBroadcast > 1000) {
            m_mpLastBroadcast = now;
            if (m_mpPlayerCount > 1) {
                LobbyTable t;
                mpUiToTable(t);
                lobbyMsgSendTable(t);
            }
        }
    }

    int type = 0;
    std::vector<uint8_t> body;
    while (lobbyMsgPoll(type, body)) {
        switch (type) {
        case kLobbyMsgState: {
            LobbyTable t;
            if (!m_mpIsHost && lobbyDecodeTable(body, t)) mpTableToUi(t);
            break;
        }
        case kLobbyMsgChange: {
            if (!m_mpIsHost || body.size() < 12) break;
            const int who = int(body[0] | (body[1] << 8));
            const int ctrl = int(body[4] | (body[5] << 8));
            const int val  = int(body[8] | (body[9] << 8));
            if (who < 0 || who >= 6) break;
            if (ctrl == 1)      m_mpColor[who] = val;
            else if (ctrl == 3) m_mpEden[who] = val != 0;
            else if (ctrl == 2) m_mpResources[who] = val;
            else if (ctrl == 4) m_mpReady[who] = val != 0;
            LobbyTable t;                       // a mesa volta para todos
            mpUiToTable(t);
            lobbyMsgSendTable(t);
            break;
        }
        case kLobbyMsgChat:
            m_mpChat.push_back(std::string(body.begin(), body.end()));
            break;
        case kLobbyMsgEject:
            m_mpEjected = true;
            m_mpStatus = "O host removeu voce da partida";
            break;
        case kLobbyMsgStart:
            if (!m_mpIsHost) {
                mpSyncGsi();
                m_netLaunchDll = (m_mpScenario >= 0 &&
                                  m_mpScenario < (int)m_mpScenarioDlls.size())
                               ? m_mpScenarioDlls[m_mpScenario] : std::string();
                if (!m_netLaunchDll.empty()) {
                    m_launchGsi = m_mpGsi;
                    m_launchHasGsi = true;
                    m_netLaunch = true;
                }
            }
            break;
        default:
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// O desenho de cada tela: os textos fixos (os STATIC do template) e o que os
// controles não cobrem. Cada posição vem das mesmas unidades de diálogo que o
// layout usa, para o rótulo cair ao lado do controle a que pertence.

namespace {

// Encurta um texto ao que cabe em `px` pixels, com reticências.
std::string fitText(const std::string& t, int px) {
    const int cw = Font::getTextWidth("M");
    const int n = px / (cw > 0 ? cw : 8);
    if ((int)t.size() <= n) return t;
    if (n <= 3) return t.substr(0, std::max(0, n));
    return t.substr(0, size_t(n - 3)) + "...";
}

} // namespace

// Dialog 259 "PICK CONNECTION METHOD" (166x122 DLU)
void ShellMenu::renderMpConnection(Renderer& renderer) {
    const DlgBox d = dlgBox(166, 44);
    renderer.dimRect(0, 0, 640, 480, 0x88);
    renderer.drawDialogFrame(d.fx, d.fy, d.fw, d.fh, "PICK CONNECTION METHOD");
    // A nota não é do diálogo original, então fica sob o quadro.
    Font::drawTextCentered(renderer.getFramebuffer(), 640, 480,
                           "(os outros meios do original nao existem mais)",
                           d.fx + d.fw / 2, d.fy + d.fh + 8, 0xFF8090A8, true);
}

// Dialog 260 "NET GAME OPTIONS" (124x66 DLU)
void ShellMenu::renderMpNetOptions(Renderer& renderer) {
    const DlgBox d = dlgBox(124, 66);
    renderer.dimRect(0, 0, 640, 480, 0x88);
    renderer.drawDialogFrame(d.fx, d.fy, d.fw, d.fh, "NET GAME OPTIONS");
}

// Dialog 2012 "NEW NETWORK GAME" (284x122 DLU)
void ShellMenu::renderMpNewGame(Renderer& renderer) {
    uint32_t* fb = renderer.getFramebuffer();
    const DlgBox d = dlgBox(284, 122);
    renderer.dimRect(0, 0, 640, 480, 0x88);
    renderer.drawDialogFrame(d.fx, d.fy, d.fw, d.fh, "NEW NETWORK GAME");
    Font::drawText(fb, 640, 480, "Your Name:",     d.x(10), d.y(10), 0xFFD8E4F8, true);
    Font::drawText(fb, 640, 480, "Password:",      d.x(10), d.y(29), 0xFFD8E4F8, true);
    Font::drawText(fb, 640, 480, "Max Players:",   d.x(10), d.y(47), 0xFFD8E4F8, true);
    Font::drawText(fb, 640, 480, "Scenario Type:", d.x(10), d.y(68), 0xFFD8E4F8, true);
    if (!m_mpStatus.empty())                       // Static 1093
        Font::drawText(fb, 640, 480, fitText(m_mpStatus, d.w(268)),
                       d.x(8), d.y(100), 0xFFFFD24A, true);
}

// Dialog 2010 "FIND SESSION" (232x98 DLU)
void ShellMenu::renderMpFindSession(Renderer& renderer) {
    uint32_t* fb = renderer.getFramebuffer();
    const DlgBox d = dlgBox(232, 98);
    renderer.dimRect(0, 0, 640, 480, 0x88);
    renderer.drawDialogFrame(d.fx, d.fy, d.fw, d.fh, "FIND SESSION");
    Font::drawText(fb, 640, 480, "Look for sessions by:", d.x(7), d.y(7), 0xFFD8E4F8, true);
    if (!m_mpStatus.empty())
        Font::drawText(fb, 640, 480, fitText(m_mpStatus, d.w(218)),
                       d.x(7), d.y(65), 0xFFFFD24A, true);
}

// Dialog 195 "PICK SESSION" (362x126 DLU) — o SysListView32 1000, com as
// colunas que a resposta de descoberta traz (nome, endereço, lugares, ida e
// volta).
void ShellMenu::renderMpPickSession(Renderer& renderer) {
    uint32_t* fb = renderer.getFramebuffer();
    const DlgBox d = dlgBox(362, 126);
    renderer.dimRect(0, 0, 640, 480, 0x88);
    renderer.drawDialogFrame(d.fx, d.fy, d.fw, d.fh, "PICK SESSION");

    const int lx = d.x(7), ly = d.y(9), lw = d.w(348), lh = d.h(64);
    renderer.drawRect(lx, ly, lw, lh, 0xFF405068);
    // As quatro colunas, em frações da largura da lista.
    const int cName = lx + 6, cAddr = lx + lw * 42 / 100,
              cPlay = lx + lw * 70 / 100, cPing = lx + lw * 85 / 100;
    Font::drawText(fb, 640, 480, "Sessao",   cName, ly + 4, 0xFF8090A8, true);
    Font::drawText(fb, 640, 480, "Endereco", cAddr, ly + 4, 0xFF8090A8, true);
    Font::drawText(fb, 640, 480, "Jog.",     cPlay, ly + 4, 0xFF8090A8, true);
    Font::drawText(fb, 640, 480, "ms",       cPing, ly + 4, 0xFF8090A8, true);
    renderer.fillRect(lx + 2, ly + 18, lw - 4, 1, 0xFF405068);

    const int rowH = 15, first = ly + 22;
    const int maxRows = (ly + lh - first) / rowH;
    for (size_t i = 0; i < m_mpSessions.size() && (int)i < maxRows; ++i) {
        const LobbySession& ses = m_mpSessions[i];
        const int ry = first + int(i) * rowH;
        const bool sel = (int)i == m_mpSelSession;
        if (sel) renderer.fillRect(lx + 2, ry - 2, lw - 4, rowH - 1, 0xFF2A3A55);
        const uint32_t col = sel ? 0xFFFFFF99 : 0xFFC8D4E8;
        unsigned char ip[4];
        std::memcpy(ip, &ses.addr, 4);
        char addr[32], players[16], ping[16];
        std::snprintf(addr, sizeof(addr), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
        std::snprintf(players, sizeof(players), "%d", ses.maxPlayers);
        std::snprintf(ping, sizeof(ping), "%d", ses.replies ? ses.pingMs / ses.replies : 0);
        Font::drawText(fb, 640, 480, fitText(ses.name, cAddr - cName - 8), cName, ry, col, true);
        Font::drawText(fb, 640, 480, addr,    cAddr, ry, col, true);
        Font::drawText(fb, 640, 480, players, cPlay, ry, col, true);
        Font::drawText(fb, 640, 480, ping,    cPing, ry, col, true);
    }
    if (m_mpSessions.empty())
        Font::drawText(fb, 640, 480, "(nenhuma sessao)", cName, first, 0xFF8090A8, true);
    if (!m_mpStatus.empty())                       // Static 1093
        Font::drawText(fb, 640, 480, fitText(m_mpStatus, d.w(268)),
                       d.x(7), d.y(88), 0xFFFFD24A, true);
}

// Dialog 2013 "JOIN A MULTIPLAYER GAME" (226x57 DLU)
void ShellMenu::renderMpJoinName(Renderer& renderer) {
    uint32_t* fb = renderer.getFramebuffer();
    const DlgBox d = dlgBox(226, 57);
    renderer.dimRect(0, 0, 640, 480, 0x88);
    renderer.drawDialogFrame(d.fx, d.fy, d.fw, d.fh, "JOIN A MULTIPLAYER GAME");
    Font::drawText(fb, 640, 480, "Your Name:", d.x(8), d.y(17), 0xFFD8E4F8, true);
    if (!m_mpStatus.empty())
        Font::drawTextCentered(fb, 640, 480, fitText(m_mpStatus, d.w(210)),
                               d.fx + d.fw / 2, d.y(3), 0xFFFFD24A, true);
}

// Dialog 196 "GAME SETTINGS" (316x226 DLU) — o lobby da partida. É a única
// tela que ocupa o quadro inteiro; ver dlgBoxFull.
void ShellMenu::renderMpGameSettings(Renderer& renderer) {
    uint32_t* fb = renderer.getFramebuffer();
    const DlgBox d = dlgBoxFull(316, 226);
    renderer.dimRect(0, 0, 640, 480, 0xCC);
    renderer.drawDialogFrame(d.fx, d.fy, d.fw, d.fh, "GAME SETTINGS");

    // O estado da sessão não existe no original; vai na barra de título, à
    // direita, onde não disputa espaço com nada do diálogo.
    if (!m_mpStatus.empty()) {
        const std::string st = fitText(m_mpStatus, 240);
        Font::drawText(fb, 640, 480, st, 630 - Font::getTextWidth(st), d.fy + 6,
                       0xFFFFD24A, true);
    }

    // Static 1080 e, para quem não hospeda, o Static 1075 no lugar do combo.
    Font::drawText(fb, 640, 480, "Scenario:", d.x(8), d.y(4), 0xFFD8E4F8, true);
    if (!m_mpIsHost) {
        const std::string nm = (m_mpScenario >= 0 && m_mpScenario < (int)m_mpScenarioList.size())
                             ? m_mpScenarioList[m_mpScenario] : "Unknown Mission";
        Font::drawText(fb, 640, 480, fitText(nm, d.w(250)), d.x(52), d.y(4),
                       0xFFC8D4E8, true);
    }

    // Cabeçalho das colunas: "Name" e "Resources" são SS_CENTER no template, os
    // outros à esquerda.
    Font::drawTextCentered(fb, 640, 480, "Name",      d.x(54),  d.y(19), 0xFF8090A8, true);
    Font::drawText(fb, 640, 480,         "Color",     d.x(103), d.y(20), 0xFF8090A8, true);
    Font::drawText(fb, 640, 480,         "Eden",      d.x(148), d.y(20), 0xFF8090A8, true);
    Font::drawTextCentered(fb, 640, 480, "Resources", d.x(196), d.y(19), 0xFF8090A8, true);
    Font::drawText(fb, 640, 480,         "Ready?",    d.x(224), d.y(20), 0xFF8090A8, true);
    renderer.fillRect(d.x(8), d.y(31), d.w(296), 1, 0xFF405068);   // o Static de 1 DLU

    // Os nomes (Static 40000+5i, centrados na coluna de 76 DLU).
    for (int i = 0; i < 6; ++i) {
        const bool taken = i < m_mpPlayerCount;
        const uint32_t col = taken ? 0xFFE0ECFF : 0xFF60708A;
        const std::string nm = taken ? (m_mpNames[i].empty() ? "PLAYER" : m_mpNames[i])
                                     : "(vazio)";
        Font::drawTextCentered(fb, 640, 480, fitText(nm, d.w(76)),
                               d.x(54), d.y(39 + 15 * i), col, true);
    }

    // Static 1084, à direita do combo 1018 — segue a linha que o layout mediu.
    for (const Control& c : m_controls)
        if (c.id == 1018) {
            Font::drawText(fb, 640, 480, "Initial Vehicles", c.x + c.w + 10, c.y + 4,
                           m_mpIsHost ? 0xFFD8E4F8 : 0xFF60708A, true);
            break;
        }

    // O registro da conversa (Edit 40031, multilinha) e o rótulo do Edit 40030.
    const int cx = d.x(8), cy = d.y(149), cw = d.w(300), ch = d.h(39);
    renderer.drawRect(cx, cy, cw, ch, 0xFF405068);
    const int lineH = 14, rows = (ch - 8) / lineH;
    const int firstLine = std::max(0, (int)m_mpChat.size() - rows);
    int line = 0;
    for (size_t i = firstLine; i < m_mpChat.size(); ++i, ++line)
        Font::drawText(fb, 640, 480, fitText(m_mpChat[i], cw - 12),
                       cx + 6, cy + 4 + line * lineH, 0xFFA8B8D0, true);
    Font::drawText(fb, 640, 480, "Chat:", d.x(8), d.y(192), 0xFFD8E4F8, true);
}

} // namespace op2
