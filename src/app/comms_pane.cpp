#include "comms_pane.h"

#include <cstdio>
#include <string>

#include "../game/command_apply.h"
#include "../game/messages.h"
#include "../game/player.h"
#include "../game/tethys_game.h"
#include "../net/net_game.h"
#include "../gfx/art_blit.h"
#include "../gfx/artfile.h"
#include "../gfx/ui_font.h"

namespace op2 {

// Item i do provedor: a i-ésima mensagem mais nova (sub_466630).
static const GameMessage& itemMessage(int i) { return messageAt(messageCount() - 1 - i); }

void CommsPane::open() {                                    // sub_466920 (modo 0)
    m_open = true;
    m_mode = MESSAGES;
    m_list.reset();
    m_seen = messageSerial();
}

void CommsPane::setMode(Mode m) {                           // [+0x1C4]
    m_mode = m;
    if (m == MESSAGES) { m_list.reset(); m_seen = messageSerial(); }
}

// Os outros jogadores HUMANOS ([player+0x28] != 0), na ordem do numero do
// jogador. Eu fico de fora das duas listas.
std::vector<int> CommsPane::chatPlayers() const {
    std::vector<int> out;
    const int me = TethysGame::localPlayer();
    const int n = TethysGame::noPlayers();
    for (int i = 0; i < n && i < kMaxPlayers; ++i)
        if (i != me && g_playerData[i].isHuman) out.push_back(i);
    return out;
}

// --- a tela do modo 1 -------------------------------------------------------
// As duas listas ficam no MESMO x e so mudam de altura; cada jogador ocupa
// 0x0F px. Os numeros sao os de sub_466A40 e sub_4670A0, contados do canto do
// retangulo de cliente da janela do quadro.
namespace {
constexpr int kChatCheckY  = 0x46;   // 1a caixa  ("Send Chat Messages To:")
constexpr int kChatNameY   = 0x44;   // 1o nome
constexpr int kAllyTitleY  = 0x91;   // "Players You're Allied With:"
constexpr int kAllyCheckY  = 0xA0;   // 1a caixa da alianca
constexpr int kAllyNameY   = 0x9E;   // 1o nome
constexpr int kNoteY       = 0xF0;   // a nota do "A-"
constexpr int kRowH        = 0x0F;
constexpr int kCheckX      = 0x0A;
constexpr int kNameX       = 0x1C;
constexpr int kCheckArt    = 0x743;  // a caixa (12x12), quadro 1 = marcada
constexpr int kCheckSize   = 12;

// sub_416650 quebra o texto na largura do retangulo; o drawMarkupText daqui so
// quebra no '\n', entao a conta de palavras fica neste canto.
std::string quebra(const std::string& t, int largura) {
    std::string out, linha, palavra;
    for (size_t k = 0; k <= t.size(); ++k) {
        if (k < t.size() && t[k] != ' ') { palavra += t[k]; continue; }
        const std::string cand = linha.empty() ? palavra : linha + " " + palavra;
        if (!linha.empty() && UiFont::width(cand) > largura) {
            out += linha;
            out += '\n';
            linha = palavra;
        } else {
            linha = cand;
        }
        palavra.clear();
    }
    return out + linha;
}
}  // namespace

void CommsPane::update() {                                  // sub_4668C0
    if (!m_open || m_mode != MESSAGES) return;
    if ((TethysGame::tick() & 3) != 1 || m_seen >= messageSerial()) return;
    m_seen = messageSerial();
    // sub_430F30: a mensagem nova entra no topo; a escolhida desce uma linha.
    if (m_list.sel != -1) m_list.sel = (m_list.sel + 1 < messageCount()) ? m_list.sel + 1 : -1;
}

ListBoxLayout CommsPane::layout(const PaneRect& pane) const {
    return listBoxLayout(pane, 0, 0x19, 0, 0x12);           // sub_459940(dword_5670F4, 1, 0, 0x19, 0, 0x12)
}

std::vector<std::string> CommsPane::items() const {         // sub_466630: "%s" / "<C2>%s"
    std::vector<std::string> out;
    for (int i = 0; i < messageCount(); ++i) {
        const GameMessage& m = itemMessage(i);
        out.push_back((m.x == -2 && m.y == -1) ? m.text : "<C2>" + m.text);
    }
    return out;
}

void CommsPane::draw(uint32_t* fb, int fbW, int fbH, int winX, int winY, const PaneRect& pane, int mx, int my) const {
    if (!m_open) return;
    const ListBoxLayout L = layout(pane);
    if (m_mode == NETSTATS) {                               // sub_467760
        drawMarkupText(fb, fbW, fbH, PaneRect{pane.l + 0xD, pane.t + 2, pane.r - 0xE, L.list.t},
                       "Network Statistics", 0xFF00FF00u, true, kMarkupLine);
        float vel = 0.0f, taxa = 0.0f;
        TethysGame::rates(vel, taxa);
        int ps = 0, bs = 0, pr = 0, br = 0;
        netTraffic(ps, bs, pr, br);
        char buf[512];
        // Fora de rede o original para nas duas primeiras linhas.
        if (netActive())
            std::snprintf(buf, sizeof(buf),
                          "Actual Game Speed:<R>%2.2f\n<L>Actual Update Rate:<R>%2.2f\n"
                          "<L>Packets Sent:<R>%i\n<L>Bytes Sent:<R>%i\n"
                          "<L>Packets Received:<R>%i\n<L>Bytes Received:<R>%i",
                          vel, taxa, ps, bs, pr, br);
        else
            std::snprintf(buf, sizeof(buf),
                          "Actual Game Speed:<R>%2.2f\n<L>Actual Update Rate:<R>%2.2f", vel, taxa);
        drawMarkupText(fb, fbW, fbH,
                       PaneRect{winX + kCheckX, winY + 0x37, pane.r - 0xC, pane.b - 4}, buf,
                       0xFF00FF00u, false, kMarkupLine);
        return;
    }
    if (m_mode == CHAT) {                                   // loc_466B67
        drawMarkupText(fb, fbW, fbH, PaneRect{pane.l + 0xD, pane.t + 2, pane.r - 0xE, L.list.t},
                       "Communications: Chat", 0xFF00FF00u, true, kMarkupLine);
        const int me = TethysGame::localPlayer();
        const uint32_t meu = g_playerData[me].allyMask;      // quem me declarou aliado
        const int dir = pane.r - 0xC;
        auto titulo = [&](const char* t, int y) {
            drawMarkupText(fb, fbW, fbH, PaneRect{winX + kCheckX, winY + y, dir, winY + y + 0xF}, t,
                           0xFF00FF00u);
        };
        auto lista = [&](int yNome, int yCaixa, bool alianca) {
            int yn = winY + yNome, yc = winY + yCaixa;
            for (int i : chatPlayers()) {
                const bool marcada = alianca ? ((g_playerData[i].allyMask >> me) & 1) != 0
                                             : ((chatMask() >> i) & 1) != 0;
                // Quadro 0 e a caixa COM o X (imagem 3942) e o 1 e a vazia.
                if (g_art.ok() && kCheckArt - 0x65 < g_art.numAnimations())
                    blitArtFrame(fb, fbW, fbH, kCheckArt - 0x65, marcada ? 0 : 1,
                                 winX + kCheckX, yc, -1, pane.l, pane.t, pane.r, pane.b);
                const std::string nome = (((meu >> i) & 1) ? "A-" : "") + playerName(i);
                UiFont::draw(fb, fbW, fbH, nome, winX + kNameX, yn,
                             g_art.playerRampColor(g_playerData[i].colorNumber));
                yn += kRowH;
                yc += kRowH;
            }
        };
        titulo("Send Chat Messages To:", 0x37);
        lista(kChatNameY, kChatCheckY, false);
        titulo("Players You're Allied With:", kAllyTitleY);
        lista(kAllyNameY, kAllyCheckY, true);
        drawMarkupText(fb, fbW, fbH,
                       PaneRect{winX + kCheckX, winY + kNoteY, dir, winY + kNoteY + 0x2D},
                       quebra("A- preceding a name indicates that player is allied with you.",
                              dir - (winX + kCheckX)),
                       0xFF00FF00u);
        return;
    }
    drawMarkupText(fb, fbW, fbH, PaneRect{pane.l + 0xD, pane.t + 2, pane.r - 0xE, L.list.t},
                   "Communications: Messages", 0xFF00FF00u, true, kMarkupLine);     // sub_459B60
    // "%i: %s" (sub_439400 = tick/100, "Current Mark") — retângulo da janela do quadro.
    const std::string mark = std::to_string(TethysGame::tick() / 100) + ": Current Mark";
    drawMarkupText(fb, fbW, fbH, PaneRect{winX + 0xA, winY + 0x37, pane.r - 0xC, winY + 0x37 + 0xF},
                   mark, 0xFF00FF00u);
    m_list.draw(fb, fbW, fbH, L, pane, items(), mx, my);
}

bool CommsPane::click(const PaneRect& pane, int x, int y) {
    if (!m_open) return false;
    if (m_mode != MESSAGES) return false;                    // as caixas vao por clickChat
    int item;
    return m_list.click(layout(pane), items(), x, y, item);  // item escolhido: sub_466970
}

// O clique nas caixas (sub_457D20): a de cima so alterna a MINHA mascara de
// destinatarios; a de baixo manda o pacote 0x32, porque a alianca e do jogo
// inteiro e tem de chegar a todas as maquinas.
bool CommsPane::clickChat(int winX, int winY, int x, int y) {
    if (!m_open || m_mode != CHAT) return false;
    if (x < winX + kCheckX || x >= winX + kCheckX + kCheckSize) return false;
    const std::vector<int> ps = chatPlayers();
    for (size_t k = 0; k < ps.size(); ++k) {
        const int yc = winY + kChatCheckY + static_cast<int>(k) * kRowH;
        if (y >= yc && y < yc + kCheckSize) { chatMaskToggle(ps[k]); return true; }
        const int ya = winY + kAllyCheckY + static_cast<int>(k) * kRowH;
        if (y >= ya && y < ya + kCheckSize) { sendAlly(ps[k]); return true; }
    }
    return false;
}

bool CommsPane::jumpEnabled() const {                       // sub_466970: y != −1
    return m_open && m_list.sel >= 0 && m_list.sel < messageCount() && itemMessage(m_list.sel).y != -1;
}

bool CommsPane::jump(int& px, int& py) const {              // sub_466A00 → sub_438EC0
    if (!jumpEnabled()) return false;
    const GameMessage& m = itemMessage(m_list.sel);
    px = m.x; py = m.y;
    return true;
}

} // namespace op2
