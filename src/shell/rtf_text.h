#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace op2 {

// Texto dos .rtf do STORY.VOL (briefing B_, objetivos O_, história S_) que o
// shell mostra num controle RICHEDIT ("StoryText"). Aqui só o que esses
// arquivos usam: parágrafos (\par), tabulação, cor (\cfN da \colortbl) e
// caracteres \'hh; fontes, tamanhos e itálico ficam na fonte do port.
struct RtfRun {
    std::string text;
    uint32_t color = 0xFF000000;   // ARGB
    bool newline = false;          // \par antes do texto
};

std::vector<RtfRun> rtfParse(const std::string& src);

// Quebra em linhas que cabem em `width` px (medidas com Font::getTextWidth).
struct RtfLine {
    struct Piece { std::string text; uint32_t color; int x; };
    std::vector<Piece> pieces;
};
std::vector<RtfLine> rtfLayout(const std::vector<RtfRun>& runs, int width);

} // namespace op2
