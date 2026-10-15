#include "rtf_text.h"

#include <cctype>
#include <cstdlib>

#include "font.h"

namespace op2 {

std::vector<RtfRun> rtfParse(const std::string& s) {
    struct State { int color = 0; bool skip = false; };
    std::vector<State> stack{State{}};
    std::vector<uint32_t> table;               // \colortbl (índice 0 = automática)
    bool inColorTbl = false;
    int r = 0, g = 0, b = 0;
    std::vector<RtfRun> out;
    bool pendingNewline = false;
    auto colorOf = [&](int idx) -> uint32_t {
        if (idx <= 0 || idx >= static_cast<int>(table.size())) return 0xFF000000;
        return table[static_cast<size_t>(idx)];
    };
    auto emit = [&](char c) {
        const State& st = stack.back();
        if (st.skip) return;
        const uint32_t col = colorOf(st.color);
        if (out.empty() || pendingNewline || out.back().color != col) {
            RtfRun run;
            run.color = col;
            run.newline = pendingNewline;
            out.push_back(run);
            pendingNewline = false;
        }
        out.back().text.push_back(c);
    };
    for (size_t i = 0; i < s.size();) {
        const char c = s[i];
        if (c == '{') { stack.push_back(stack.back()); ++i; continue; }
        if (c == '}') {
            if (inColorTbl && stack.size() >= 2) inColorTbl = false;
            if (stack.size() > 1) stack.pop_back();
            ++i;
            continue;
        }
        if (c == '\r' || c == '\n') { ++i; continue; }
        if (c != '\\') {
            if (inColorTbl) {
                if (c == ';') { table.push_back(0xFF000000u | (r << 16) | (g << 8) | b); r = g = b = 0; }
                ++i;
                continue;
            }
            emit(c);
            ++i;
            continue;
        }
        // Palavra de controle.
        ++i;
        if (i >= s.size()) break;
        const char n = s[i];
        if (n == '\\' || n == '{' || n == '}') { emit(n); ++i; continue; }
        if (n == '\'') {
            const int v = static_cast<int>(std::strtol(s.substr(i + 1, 2).c_str(), nullptr, 16));
            emit(static_cast<char>(v));
            i += 3;
            continue;
        }
        if (n == '*') { stack.back().skip = true; ++i; continue; }
        if (!std::isalpha(static_cast<unsigned char>(n))) { ++i; continue; }
        size_t j = i;
        while (j < s.size() && std::isalpha(static_cast<unsigned char>(s[j]))) ++j;
        const std::string word = s.substr(i, j - i);
        bool hasNum = false;
        int num = 0;
        size_t k = j;
        if (k < s.size() && (s[k] == '-' || std::isdigit(static_cast<unsigned char>(s[k])))) {
            hasNum = true;
            num = std::atoi(s.c_str() + k);
            if (s[k] == '-') ++k;
            while (k < s.size() && std::isdigit(static_cast<unsigned char>(s[k]))) ++k;
        }
        if (k < s.size() && s[k] == ' ') ++k;    // o espaço delimitador é engolido
        i = k;
        if (word == "fonttbl" || word == "stylesheet" || word == "info" || word == "pict" ||
            word == "header" || word == "footer") {
            stack.back().skip = true;
        } else if (word == "colortbl") {
            inColorTbl = true;
            stack.back().skip = true;
            r = g = b = 0;
        } else if (inColorTbl && word == "red") r = num;
        else if (inColorTbl && word == "green") g = num;
        else if (inColorTbl && word == "blue") b = num;
        else if (word == "par" || word == "line") {
            if (!stack.back().skip) {
                if (pendingNewline || out.empty()) { RtfRun run; run.newline = true; out.push_back(run); }
                pendingNewline = true;
            }
        } else if (word == "tab") emit(' ');
        else if (word == "cf") stack.back().color = hasNum ? num : 0;
        else if (word == "plain") stack.back().color = 0;
    }
    return out;
}

std::vector<RtfLine> rtfLayout(const std::vector<RtfRun>& runs, int width) {
    std::vector<RtfLine> lines(1);
    int x = 0;
    auto newLine = [&]() { lines.emplace_back(); x = 0; };
    for (const RtfRun& run : runs) {
        if (run.newline && !(lines.size() == 1 && lines[0].pieces.empty() && x == 0)) newLine();
        size_t i = 0;
        const std::string& t = run.text;
        while (i < t.size()) {
            // Palavra (com os espaços que a seguem).
            size_t j = i;
            while (j < t.size() && t[j] != ' ') ++j;
            while (j < t.size() && t[j] == ' ') ++j;
            std::string word = t.substr(i, j - i);
            int w = Font::getTextWidth(word);
            std::string trimmed = word;
            while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
            if (x > 0 && x + Font::getTextWidth(trimmed) > width) newLine();
            if (x == 0) {                       // começo de linha: sem espaços à esquerda
                size_t a = 0;
                while (a < word.size() && word[a] == ' ' && !lines.back().pieces.empty()) ++a;
                word = word.substr(a);
                w = Font::getTextWidth(word);
            }
            RtfLine& L = lines.back();
            if (!L.pieces.empty() && L.pieces.back().color == run.color &&
                L.pieces.back().x + Font::getTextWidth(L.pieces.back().text) == x)
                L.pieces.back().text += word;
            else
                L.pieces.push_back({word, run.color, x});
            x += w;
            i = j;
        }
    }
    return lines;
}

} // namespace op2
