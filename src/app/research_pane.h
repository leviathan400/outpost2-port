#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "info_pane.h"

namespace op2 {

// Painel de pesquisa do laboratório — porta unk_569110 (vtable off_4D5A10,
// base de lista off_4D5588). Aberto pelo ASSIGN (sub_456340; dword_5692D4 =
// lab já pesquisando → modo 1 direto).
//   Modo 0 ([+0x1C4] = 0): "<lab>\nAvailable Topics", lista dos tópicos
//     (sub_46C390 → sub_4731F0; nomes = tech+0x28; margens 0/0x14/0/0x12),
//     botões ASSIGN (habilita ao escolher; clique duplo = ASSIGN) / CANCEL.
//   Modo 1: "<lab>\nAssign Scientists"; linhas "Scientists Available /
//     Assigned To This Project / Maximum Project Staff" (sub_46C870) em
//     topo+0x28; LESS/MORE (arte 0x98) em (9, 0x7B) e (9+87, 0x7B) da janela;
//     lista com o texto do tópico (tech+0x30, margens 0/0x64/0/0x12); botões
//     START (sub_457610) e TOPICS (ou STATUS se o lab já pesquisa).
class ResearchPane {
public:
    bool isOpen() const { return m_open; }
    int  mode() const { return m_mode; }
    int  lab() const { return m_lab; }
    bool assignEnabled() const { return m_topic > 0; }
    bool researching() const;
    void open(int labId, int mode);                      // sub_46C210
    void close() { m_open = false; }
    void update(const std::vector<int>& sel);
    void draw(uint32_t* fb, int fbW, int fbH, int winL, int winT, const PaneRect& pane, int mx, int my) const;
    // Clique no painel (lista, setas, LESS/MORE). dblAssign = clique duplo num tópico.
    bool click(int winL, int winT, const PaneRect& pane, int x, int y, bool dbl, bool& dblAssign);
    void less();
    void more();
    void start();                                        // START (pacote 0x16)
    void selectForTest(int topic) { m_topic = topic; }

private:
    std::vector<std::string> teaserLines(int width) const;
    PaneRect lessRect(int winL, int winT) const;
    PaneRect moreRect(int winL, int winT) const;
    bool m_open = false;
    int m_mode = 0;
    int m_lab = 0;
    std::vector<int> m_topics;
    int m_topic = 0;             // [+0xB7C]
    int m_sci = 0;               // [+0xB84]
    int m_sciCur = 0;            // [+0xB88] cientistas já no projeto
    int m_top = 0;
};

// Painel "Assign Students To Class" da University — porta dword_56AF28
// (vtable off_4D5B78): sub_46E020 abre (turma máxima = bloco+0x34 "Production
// Capacity"; alunos = mín(trabalhadores livres, máximo)); sub_46E220 desenha
// "Workers Available / Assigned To Training / Maximum Class Size" em topo+0x28;
// LESS/MORE (sub_46E4B0) em (9, 0x91) e (9+87, 0x91); botões START ("S&tart
// Class", pacote 0x17) e STATUS (sub_46E0D0).
class StudentsPane {
public:
    bool isOpen() const { return m_open; }
    void open(int univId);
    void close() { m_open = false; }
    void update(const std::vector<int>& sel);
    void draw(uint32_t* fb, int fbW, int fbH, int winL, int winT, const PaneRect& pane, int mx, int my) const;
    bool click(int winL, int winT, int x, int y);
    void less() { if (m_n > 1) --m_n; }
    void more();
    void start();
private:
    int maxClass() const;
    bool m_open = false;
    int m_univ = 0;
    int m_n = 0;                 // [+0x2B0]
};

// Confirmação de comando — porta dword_564EA8 (vtable off_4D5310): mensagem
// em vermelho centrada (sub_45C0B0) e os botões "&YES" / "&NO" (sub_45C200).
// Usada pelos STOP de pesquisa/treino (sub_457FC0 / sub_458100).
class ConfirmPane {
public:
    bool isOpen() const { return m_open; }
    void open(int unitId, const std::string& msg) { m_open = true; m_unit = unitId; m_msg = msg; }
    void close() { m_open = false; }
    void update(const std::vector<int>& sel);
    void draw(uint32_t* fb, int fbW, int fbH, const PaneRect& pane) const;
private:
    bool m_open = false;
    int m_unit = 0;
    std::string m_msg;
};

} // namespace op2
