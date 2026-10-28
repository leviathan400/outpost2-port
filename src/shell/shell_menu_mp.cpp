// As telas de multijogador. No original elas NAO vem do shell: o botao
// MULTIPLAYER (1005 do Dialog 137) chama TApp::DoNetGame (sub_487C40) no
// executavel, que abre os dialogos do OUT2RES.DLL nesta ordem:
//
//   259 "PICK CONNECTION METHOD"  sub_411E30 -> escolhe o transporte
//       (INTERNET/SIGS 6, MODEM 4, IPX 5, TCP/IP 3, SERIAL 7, CANCEL 2); a
//       tabela unk_4DF370 liga cada botao ao objeto do transporte.
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
#include <cctype>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <cstdio>
#include <iostream>

#include <SDL.h>

#include "../gfx/font.h"
#include "../res/pe_resource.h"

namespace op2 {

const std::vector<std::string> kColorNames = {
    "Blue", "Red", "Green", "Yellow", "Cyan", "Magenta"
};
const std::vector<std::string> kResourceNames = { "High", "Med", "Low" };
const std::vector<std::string> kMaxPlayerNames = { "2", "3", "4", "5", "6" };
const std::vector<std::string> kVehicleNames = { "0", "1", "2", "3", "4", "5" };

namespace {

// O diálogo 196 tem 316x226 unidades e o 259 tem 166x122; o nosso quadro é de
// 640x480, então as telas são centralizadas com a mesma proporção de 1 unidade
// = 1 pixel que o resto do shell usa.
int originX(int w) { return (640 - w) / 2; }
int originY(int h) { return (480 - h) / 2; }

} // namespace

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

// Percorre o fluxo das telas chamando o mesmo despacho dos botoes, para que o
// teste exercite o caminho de verdade e nao um atalho: MULTIPLAYER -> TCP/IP
// -> CREATE/JOIN -> (nome/busca) -> GAME SETTINGS -> START.
bool ShellMenu::mpFlowTest(bool host, const std::string& name, std::string& report) {
    auto step = [&](const char* what) { report += what; report += "; "; };

    showMultiplayer();
    if (m_state != MenuState::MpConnection) { report += "nao abriu o 259"; return false; }
    step("259 aberto");

    dispatchButton(3);                       // TCP/IP
    if (m_state != MenuState::MpNetOptions) { report += "nao abriu o 260"; return false; }
    step("260 aberto");

    m_mpPlayerName = name;
    if (host) {
        dispatchButton(1074);                // CREATE
        if (m_state != MenuState::MpNewGame) { report += "nao abriu o 2012"; return false; }
        step("2012 aberto");
        dispatchButton(1);                   // OK -> abre o lobby
        if (m_state != MenuState::MpGameSettings) {
            report += "o host nao chegou ao 196: " + m_mpStatus;
            return false;
        }
        step("196 aberto (host)");
        // Espera o outro lado entrar, bombeando o lobby como a tela faz.
        const uint32_t fim = SDL_GetTicks() + 30000;
        while (SDL_GetTicks() < fim && m_mpPlayerCount < 2) {
            mpPump();
            SDL_Delay(5);
        }
        if (m_mpPlayerCount < 2) { report += "ninguem entrou"; return false; }
        step("o outro lado entrou");
    } else {
        dispatchButton(1075);                // JOIN
        if (m_state != MenuState::MpFindSession) { report += "nao abriu o 2010"; return false; }
        step("2010 aberto");
        dispatchButton(1);                   // OK -> busca por broadcast
        if (m_state != MenuState::MpPickSession) { report += "nao abriu o 195"; return false; }
        step("195 aberto");
        if (m_mpSessions.empty()) { report += "nenhuma sessao encontrada"; return false; }
        step(("achou " + std::to_string(m_mpSessions.size()) + " sessao(oes)").c_str());
        dispatchButton(1);                   // OK -> nome
        if (m_state != MenuState::MpJoinName) { report += "nao abriu o 2013"; return false; }
        step("2013 aberto");
        dispatchButton(1);                   // OK -> entra
        if (m_state != MenuState::MpGameSettings) {
            report += "nao entrou: " + m_mpStatus;
            return false;
        }
        step("196 aberto (cliente)");
    }

    dispatchButton(1045);                    // START
    if (!m_netLaunch) { report += "o START nao pediu a partida: " + m_mpStatus; return false; }
    step("START aceito");
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
    if (!lobbyFind(m_mpPassword, 8, found, err)) {
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

// Enquanto o GAME SETTINGS está no ar o host precisa continuar respondendo às
// consultas e aos pedidos de entrada (os threads sub_48C1F0 / sub_48C300).
void ShellMenu::mpPump() {
    if (!lobbyHostActive()) return;
    lobbyHostPump();
    const int n = lobbyHostActiveCount();
    if (n != m_mpPlayerCount) {
        for (int i = m_mpPlayerCount; i < n && i < 6; ++i)
            if (m_mpNames[i].empty()) m_mpNames[i] = "PLAYER " + std::to_string(i + 1);
        m_mpPlayerCount = n;
        m_mpChat.push_back("Jogadores na mesa: " + std::to_string(n));
        m_mpStatus = (n >= m_mpMaxPlayers + 2) ? "Mesa cheia — pode comecar"
                                               : "Aguardando jogadores...";
    }
}

// ---------------------------------------------------------------------------
// Cada tela escurece o fundo e usa o mesmo quadro de diálogo do resto do shell,
// com o título do recurso correspondente.

// Dialog 259 "PICK CONNECTION METHOD" (166x122 DLU)
void ShellMenu::renderMpConnection(Renderer& renderer) {
    const int ox = (640 - 332) / 2, oy = (480 - 244) / 2;
    renderer.fillRect(0, 0, 640, 480, 0x88000000);
    renderer.drawDialogFrame(ox - 16, oy - 36, 364, 292, "PICK CONNECTION METHOD");
    // Só o TCP/IP está portado; os outros ficam visíveis e desabilitados, como
    // o original os mostra quando o provedor não está instalado.
    Font::drawTextCentered(renderer.getFramebuffer(), 640, 480,
                           "(apenas TCP/IP esta portado)", 320, oy + 236,
                           0xFF8090A8, true);
}

// Dialog 260 "NET GAME OPTIONS" (124x66)
void ShellMenu::renderMpNetOptions(Renderer& renderer) {
    const int ox = (640 - 248) / 2, oy = (480 - 132) / 2;
    renderer.fillRect(0, 0, 640, 480, 0x88000000);
    renderer.drawDialogFrame(ox - 16, oy - 36, 280, 180, "NET GAME OPTIONS");
}

// Dialog 2012 "NEW NETWORK GAME" (284x122)
void ShellMenu::renderMpNewGame(Renderer& renderer) {
    uint32_t* fb = renderer.getFramebuffer();
    const int ox = (640 - 568) / 2, oy = (480 - 244) / 2;
    renderer.fillRect(0, 0, 640, 480, 0x88000000);
    renderer.drawDialogFrame(ox - 16, oy - 36, 600, 296, "NEW NETWORK GAME");
    Font::drawText(fb, 640, 480, "Your Name:",     ox + 16, oy + 20, 0xFFD8E4F8, true);
    Font::drawText(fb, 640, 480, "Password:",      ox + 16, oy + 58, 0xFFD8E4F8, true);
    Font::drawText(fb, 640, 480, "Max Players:",   ox + 16, oy + 96, 0xFFD8E4F8, true);
    Font::drawText(fb, 640, 480, "Scenario Type:", ox + 16, oy + 134, 0xFFD8E4F8, true);
    if (!m_mpStatus.empty())
        Font::drawText(fb, 640, 480, m_mpStatus, ox + 16, oy + 212, 0xFFFFD24A, true);
}

// Dialog 2010 "FIND SESSION" (232x98)
void ShellMenu::renderMpFindSession(Renderer& renderer) {
    uint32_t* fb = renderer.getFramebuffer();
    const int ox = (640 - 464) / 2, oy = (480 - 196) / 2;
    renderer.fillRect(0, 0, 640, 480, 0x88000000);
    renderer.drawDialogFrame(ox - 16, oy - 36, 496, 248, "FIND SESSION");
    Font::drawText(fb, 640, 480, "Look for sessions by:", ox + 14, oy + 14, 0xFFD8E4F8, true);
    if (!m_mpStatus.empty())
        Font::drawText(fb, 640, 480, m_mpStatus, ox + 14, oy + 188, 0xFFFFD24A, true);
}

// Dialog 195 "PICK SESSION" (362x126) — o SysListView32 1000, com as colunas
// que a resposta de descoberta traz (nome, endereço, lugares, ida e volta).
void ShellMenu::renderMpPickSession(Renderer& renderer) {
    uint32_t* fb = renderer.getFramebuffer();
    const int ox = (640 - 600) / 2, oy = (480 - 260) / 2;
    renderer.fillRect(0, 0, 640, 480, 0x88000000);
    renderer.drawDialogFrame(ox - 16, oy - 36, 632, 300, "PICK SESSION");
    renderer.drawRect(ox, oy, 600, 150, 0xFF405068);
    Font::drawText(fb, 640, 480, "Sessao",   ox + 8,   oy + 6, 0xFF8090A8, true);
    Font::drawText(fb, 640, 480, "Endereco", ox + 250, oy + 6, 0xFF8090A8, true);
    Font::drawText(fb, 640, 480, "Jog.",     ox + 420, oy + 6, 0xFF8090A8, true);
    Font::drawText(fb, 640, 480, "ms",       ox + 480, oy + 6, 0xFF8090A8, true);
    for (size_t i = 0; i < m_mpSessions.size() && i < 8; ++i) {
        const LobbySession& ses = m_mpSessions[i];
        const int ly = oy + 24 + int(i) * 15;
        const bool sel = (int)i == m_mpSelSession;
        if (sel) renderer.fillRect(ox + 2, ly - 2, 596, 14, 0xFF2A3A55);
        const uint32_t col = sel ? 0xFFFFFF99 : 0xFFC8D4E8;
        unsigned char ip[4];
        std::memcpy(ip, &ses.addr, 4);
        char addr[32], players[16], ping[16];
        std::snprintf(addr, sizeof(addr), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
        std::snprintf(players, sizeof(players), "%d", ses.maxPlayers);
        std::snprintf(ping, sizeof(ping), "%d", ses.replies ? ses.pingMs / ses.replies : 0);
        Font::drawText(fb, 640, 480, ses.name, ox + 8,   ly, col, true);
        Font::drawText(fb, 640, 480, addr,     ox + 250, ly, col, true);
        Font::drawText(fb, 640, 480, players,  ox + 420, ly, col, true);
        Font::drawText(fb, 640, 480, ping,     ox + 480, ly, col, true);
    }
    if (m_mpSessions.empty())
        Font::drawText(fb, 640, 480, "(nenhuma sessao)", ox + 8, oy + 24, 0xFF8090A8, true);
    if (!m_mpStatus.empty())
        Font::drawText(fb, 640, 480, m_mpStatus, ox, oy + 158, 0xFFFFD24A, true);
}

// Dialog 2013 "JOIN A MULTIPLAYER GAME" (226x57)
void ShellMenu::renderMpJoinName(Renderer& renderer) {
    uint32_t* fb = renderer.getFramebuffer();
    const int ox = (640 - 452) / 2, oy = (480 - 114) / 2;
    renderer.fillRect(0, 0, 640, 480, 0x88000000);
    renderer.drawDialogFrame(ox - 16, oy - 36, 484, 170, "JOIN A MULTIPLAYER GAME");
    Font::drawText(fb, 640, 480, "Your Name:", ox + 16, oy + 30, 0xFFD8E4F8, true);
    if (!m_mpStatus.empty())
        Font::drawText(fb, 640, 480, m_mpStatus, ox + 16, oy + 110, 0xFFFFD24A, true);
}

// Dialog 196 "GAME SETTINGS" (316x226) — o lobby da partida.
void ShellMenu::renderMpGameSettings(Renderer& renderer) {
    uint32_t* fb = renderer.getFramebuffer();
    const int ox = (640 - 632) / 2, oy = (480 - 452) / 2;
    renderer.fillRect(0, 0, 640, 480, 0xCC000000);
    renderer.drawDialogFrame(ox - 4, oy - 20, 640, 476, "GAME SETTINGS");

    // Linha do cenário: combo 40032 para quem hospeda, Static 1075 para os
    // outros (eles recebem a escolha do host).
    Font::drawText(fb, 640, 480, "Scenario:", ox + 8, oy + 8, 0xFFD8E4F8, true);
    if (!m_mpIsHost) {
        const std::string nm = (m_mpScenario >= 0 && m_mpScenario < (int)m_mpScenarioList.size())
                             ? m_mpScenarioList[m_mpScenario] : "Unknown Mission";
        Font::drawText(fb, 640, 480, nm, ox + 100, oy + 8, 0xFFC8D4E8, true);
    }

    // Cabeçalho das colunas, nas posições do diálogo.
    const int rowY = oy + 44, rowH = 30;
    Font::drawText(fb, 640, 480, "Name",      ox + 32,  rowY - 18, 0xFF8090A8, true);
    Font::drawText(fb, 640, 480, "Color",     ox + 200, rowY - 18, 0xFF8090A8, true);
    Font::drawText(fb, 640, 480, "Eden",      ox + 290, rowY - 18, 0xFF8090A8, true);
    Font::drawText(fb, 640, 480, "Resources", ox + 348, rowY - 18, 0xFF8090A8, true);
    Font::drawText(fb, 640, 480, "Ready?",    ox + 442, rowY - 18, 0xFF8090A8, true);
    renderer.fillRect(ox + 16, rowY - 6, 596, 1, 0xFF405068);

    for (int i = 0; i < 6; ++i) {
        const int ly = rowY + i * rowH;
        const bool taken = i < m_mpPlayerCount;
        const uint32_t col = taken ? 0xFFE0ECFF : 0xFF60708A;
        const std::string nm = taken ? (m_mpNames[i].empty() ? "PLAYER" : m_mpNames[i])
                                     : "(vazio)";
        Font::drawText(fb, 640, 480, nm, ox + 32, ly, col, true);
    }

    // Chat: Edit 40031 guarda o registro, 40030 a linha que se digita.
    const int chatY = oy + 300;
    renderer.drawRect(ox + 16, chatY, 600, 72, 0xFF405068);
    int line = 0;
    const int first = std::max(0, (int)m_mpChat.size() - 4);
    for (size_t i = first; i < m_mpChat.size(); ++i, ++line)
        Font::drawText(fb, 640, 480, m_mpChat[i], ox + 22, chatY + 6 + line * 15,
                       0xFFA8B8D0, true);
    Font::drawText(fb, 640, 480, "Chat:", ox + 18, oy + 382, 0xFFD8E4F8, true);
    // Static 1084, à direita do combo 1018.
    Font::drawText(fb, 640, 480, "Initial Vehicles", ox + 462, oy + 270, 0xFFD8E4F8, true);

    if (!m_mpStatus.empty())
        Font::drawTextCentered(fb, 640, 480, m_mpStatus, 320, 466, 0xFFFFD24A, true);
}

} // namespace op2
