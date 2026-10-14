#include "report_list.h"

#include <algorithm>

#include "../gfx/ui_font.h"

namespace op2 {

static uint32_t rgb555(uint16_t c) {
    const uint32_t r = ((c >> 10) & 31) << 3, g = ((c >> 5) & 31) << 3, b = (c & 31) << 3;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

static int textWidth(const std::string& s) {
    std::string plain;
    bool literal = false;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '<' && !literal) {
            const size_t e = s.find('>', i);
            if (e != std::string::npos) {
                if (e == i + 2 && s[i + 1] == 'N') literal = true;
                i = e;
                continue;
            }
        }
        if (s[i] == '\t') plain += "    "; else plain += s[i];
    }
    return UiFont::width(plain);
}

std::vector<ReportLine> reportLines(const std::vector<std::string>& items, int width) {
    std::vector<ReportLine> out;
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const std::string& it = items[i];
        size_t pos = 0;
        while (true) {
            const size_t nl = it.find('\n', pos);
            const std::string seg = it.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
            // Tags do começo (repetidas nas linhas quebradas).
            size_t p = 0;
            while (p < seg.size() && seg[p] == '<') {
                const size_t e = seg.find('>', p);
                if (e == std::string::npos) break;
                const bool lit = e == p + 2 && seg[p + 1] == 'N';
                p = e + 1;
                if (lit) break;
            }
            const std::string tag = seg.substr(0, p), text = seg.substr(p);
            std::string cur, word;
            for (size_t k = 0; k <= text.size(); ++k) {
                if (k == text.size() || text[k] == ' ') {
                    const std::string cand = cur.empty() ? word : cur + " " + word;
                    if (!cur.empty() && textWidth(cand) > width) { out.push_back({i, tag + cur}); cur = word; }
                    else cur = cand;
                    word.clear();
                } else {
                    word += text[k];
                }
            }
            out.push_back({i, tag + cur});
            if (nl == std::string::npos) break;
            pos = nl + 1;
        }
    }
    return out;
}

void ReportList::draw(uint32_t* fb, int fbW, int fbH, const ListBoxLayout& L, const PaneRect& clip,
                      const std::vector<std::string>& items, int mx, int my) const {
    drawListBoxFrame(fb, fbW, fbH, L);
    auto fill = [&](int x, int y, int w, int h, uint32_t c) {
        for (int yy = std::max(0, y); yy < std::min(fbH, y + h); ++yy)
            for (int xx = std::max(0, x); xx < std::min(fbW, x + w); ++xx) fb[yy * fbW + xx] = c;
    };
    const std::vector<ReportLine> ls = reportLines(items, L.list.r - L.list.l - 8);
    const int n = static_cast<int>(ls.size());
    const int t = std::min(top, std::max(0, n - 1));
    for (int i = 0; i < L.rows && t + i < n; ++i) {
        const ReportLine& ln = ls[t + i];
        const int y = L.list.t + 2 + i * L.lineH;
        if (ln.item == sel) {
            const bool first = t + i == 0 || ls[t + i - 1].item != ln.item;
            const bool last = t + i + 1 >= n || ls[t + i + 1].item != ln.item;
            const int x = L.list.l + 2, w = L.list.r - L.list.l - 4;
            fill(x, y, w, L.lineH, rgb555(0x1CE7));
            fill(x, y, 1, L.lineH, rgb555(0x3DEF));
            fill(x + w - 1, y, 1, L.lineH, rgb555(0x3DEF));
            if (first) fill(x, y, w, 1, rgb555(0x3DEF));
            if (last) fill(x, y + L.lineH - 1, w, 1, rgb555(0x3DEF));
        }
        drawMarkupText(fb, fbW, fbH, PaneRect{L.list.l + 4, y + 1, L.list.r - 4, y + L.lineH}, ln.text, 0xFF00FF00u);
    }
    drawListBoxArrows(fb, fbW, fbH, L, clip, t > 0, t + L.rows < n, mx, my);
}

bool ReportList::click(const ListBoxLayout& L, const std::vector<std::string>& items, int x, int y, int& item) {
    item = -1;
    auto in = [&](const PaneRect& r) { return x >= r.l && x < r.r && y >= r.t && y < r.b; };
    const std::vector<ReportLine> ls = reportLines(items, L.list.r - L.list.l - 8);
    const int n = static_cast<int>(ls.size());
    if (in(L.up)) { top = std::max(0, top - L.rows); return true; }
    if (in(L.down)) { if (top + L.rows < n) top += L.rows; return true; }
    if (!in(L.list)) return false;
    const int i = (y - L.list.t - 2) / L.lineH;
    if (i >= 0 && i < L.rows && top + i < n) { item = ls[top + i].item; sel = item; }
    return true;
}

} // namespace op2
