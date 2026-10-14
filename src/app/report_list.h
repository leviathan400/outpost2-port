#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "info_pane.h"
#include "produce_pane.h"

namespace op2 {

// Lista rolável dos painéis de relatório (controle da base off_4D5588, desenho
// sub_430400): cada item é texto marcado que pode ter várias linhas ('\n') e é
// quebrado por palavras na largura (sub_416650); as tags do começo da linha
// (<Cn>, <N>, ...) valem p/ as linhas quebradas. A escolha pinta o item inteiro.
// As setas rolam uma página de linhas.
struct ReportLine { int item; std::string text; };
std::vector<ReportLine> reportLines(const std::vector<std::string>& items, int width);

class ReportList {
public:
    int sel = -1;                                        // item escolhido (−1 = nenhum)
    int top = 0;                                         // 1ª linha visível
    void reset() { sel = -1; top = 0; }
    void draw(uint32_t* fb, int fbW, int fbH, const ListBoxLayout& L, const PaneRect& clip,
              const std::vector<std::string>& items, int mx, int my) const;
    // false = fora da lista/setas; item = item clicado (−1 = nenhum).
    bool click(const ListBoxLayout& L, const std::vector<std::string>& items, int x, int y, int& item);
};

} // namespace op2
