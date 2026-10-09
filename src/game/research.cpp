#include "research.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>

#include "building_sheet.h"
#include "player.h"
#include "tethys_game.h"
#include "unit_names.h"
#include "../res/res_manager.h"

namespace op2 {

static std::vector<std::unique_ptr<Tech>> s_techs;   // [+4], índice 0 = raiz
static int s_maxTechID = 0;                          // [+8]
static int s_uiCache = -1;                           // [+0x2C]
static int s_topicIndex[0x73] = {};                  // desc+0x1E4 convertido

void clearTechTree() {
    s_techs.clear();
    s_maxTechID = 0;
    for (int& t : s_topicIndex) t = 0;
}
int techCount() { return static_cast<int>(s_techs.size()); }
const Tech* techAt(int i) { return (i >= 0 && i < techCount()) ? s_techs[i].get() : nullptr; }

// sub_472CD0: busca binária pelo ID no vetor ordenado (inclui a raiz).
int techIndex(int id) {
    int lo = 0, hi = techCount() - 1;
    if (hi < 0) return -1;
    while (hi - lo > 1) {
        const int mid = (lo + hi) / 2;
        const int d = s_techs[mid]->id - id;
        if (d > 0) hi = mid;
        else if (d < 0) lo = mid;
        else return mid;
    }
    if (s_techs[lo]->id == id) return lo;
    if (s_techs[hi]->id == id) return hi;
    return -1;
}

int researchTopicIndex(int type) { return (type >= 0 && type < 0x73) ? s_topicIndex[type] : 0; }

// ---- Leitura (sub_48F140 palavra / sub_48F260 texto entre aspas / sub_48F010 inteiro)
namespace {
struct Reader {
    std::string t;
    size_t p = 0;
    int line = 0;
    void skip() {
        for (;;) {
            while (p < t.size() && std::isspace(static_cast<unsigned char>(t[p]))) { if (t[p] == '\n') ++line; ++p; }
            if (p < t.size() && t[p] == ';') { while (p < t.size() && t[p] != '\n') ++p; continue; }
            return;
        }
    }
    bool word(std::string& out) {
        skip();
        if (p >= t.size()) return false;
        out.clear();
        while (p < t.size() && !std::isspace(static_cast<unsigned char>(t[p]))) out += t[p++];
        return !out.empty();
    }
    bool quoted(std::string& out) {
        skip();
        if (p >= t.size() || t[p] != '"') return false;
        ++p;
        out.clear();
        while (p < t.size() && t[p] != '"') { if (t[p] == '\n') ++line; out += t[p++]; }
        if (p >= t.size()) return false;
        ++p;
        return true;
    }
    bool integer(int& v) {
        std::string w;
        if (!word(w)) return false;
        char* e = nullptr;
        v = static_cast<int>(std::strtol(w.c_str(), &e, 10));
        return e != w.c_str();
    }
};

// Tabela de campos @4D5C40 {tipo, nome, offset}.
struct FieldDef { int kind; const char* name; int offset; };
const FieldDef kFields[] = {
    {0, "Hit_Points", 0x00}, {0, "Armor", 0x08}, {1, "Ore", 0x20}, {2, "Test_Function", 0},
    {0, "Production_Capacity", 0x34}, {0, "Move_Speed", 0x24}, {0, "Sight_Range", 0x18},
    {0, "Rate_Of_Fire", 0x30}, {0, "Build_Time", 0x14}, {0, "Power_Required", 0x24},
    {0, "Storage_Capacity", 0x30}, {0, "Storage_Bays", 0x38}, {0, "Production_Rate", 0x2C},
    {0, "Concussion_Damage", 0x28}, {0, "Penetration_Damage", 0x2C}, {0, "Improved", 0x1D},
    {0, "Workers_Required", 0x28}, {0, "Common_Required", 0x0C}, {0, "Rare_Required", 0x10},
    {0, "Turn_Rate", 0x28}, {0, "Build_Points", 0x14},
};
const FieldDef* field(const std::string& n) {
    for (const auto& f : kFields) if (_stricmp(f.name, n.c_str()) == 0) return &f;
    return nullptr;
}

// sub_4739C0: uma entrada BEGIN_TECH .. END_TECH. 0 = fim/erro silencioso, −1 = erro.
int readTech(Reader& r, Tech& t) {
    std::string w;
    if (!r.word(w) || _stricmp(w.c_str(), "BEGIN_TECH") != 0) return 0;
    auto fail = [&]() {
        std::cerr << "[Research] Error reading tech-entry at line " << r.line + 1 << "." << std::endl;
        return -1;
    };
    if (!r.quoted(t.name) || !r.integer(t.id)) return fail();
    t.level = t.id / 1000;
    if (t.level == 0) return fail();
    std::vector<int> req;
    for (;;) {
        if (!r.word(w)) return fail();
        const char* k = w.c_str();
        if (!_stricmp(k, "CATEGORY")) { if (!r.integer(t.category)) return fail(); }
        else if (!_stricmp(k, "DESCRIPTION")) { if (!r.quoted(t.description)) return fail(); }
        else if (!_stricmp(k, "TEASER")) { if (!r.quoted(t.teaser)) return fail(); }
        else if (!_stricmp(k, "IMPROVE_DESC")) { if (!r.quoted(t.improveDesc)) return fail(); }
        else if (!_stricmp(k, "REQUIRES")) { int v; if (!r.integer(v)) return fail(); req.push_back(v); }
        else if (!_stricmp(k, "COST")) { if (!r.integer(t.edenCost)) return fail(); t.plymouthCost = t.edenCost; }
        else if (!_stricmp(k, "EDEN_COST")) { if (!r.integer(t.edenCost)) return fail(); }
        else if (!_stricmp(k, "PLYMOUTH_COST")) { if (!r.integer(t.plymouthCost)) return fail(); }
        else if (!_stricmp(k, "MAX_SCIENTISTS")) { if (!r.integer(t.maxScientists)) return fail(); }
        else if (!_stricmp(k, "LAB")) { if (!r.integer(t.lab)) return fail(); }
        else if (!_stricmp(k, "PLAYER_PROP") || !_stricmp(k, "FUNCTION_RESULT")) {
            const int want = !_stricmp(k, "PLAYER_PROP") ? 1 : 2;
            std::string fn; TechUpgrade u;
            if (!r.word(fn)) return fail();
            const FieldDef* f = field(fn);
            if (!f || f->kind != want || !r.integer(u.value)) return fail();
            u.kind = f->kind; u.offset = f->offset;
            t.upgrades.push_back(u);
        }
        else if (!_stricmp(k, "UNIT_PROP")) {
            std::string un, fn; TechUpgrade u;
            if (!r.word(un)) return fail();
            u.unitType = unitTypeFromCodeName(un.c_str());       // sub_445010
            if (u.unitType == -1 || !r.word(fn)) return fail();
            const FieldDef* f = field(fn);
            if (!f || f->kind != 0 || !r.integer(u.value)) return fail();
            u.kind = 0; u.offset = f->offset;
            if (f->offset == 0x08) u.value = armorValue(u.value);   // Armor: dword_4E1510
            t.upgrades.push_back(u);
        }
        else if (!_stricmp(k, "END_TECH")) break;
        else return fail();
    }
    if (req.empty()) req.push_back(0);                        // depende da raiz
    t.requires = req;                                         // (IDs; viram índices depois)
    return 1;
}
} // namespace

bool loadTechTree(const std::string& fileName, int maxTechLevel) {   // sub_4733B0
    clearTechTree();
    auto stream = g_ResManager.openStream(fileName);
    if (!stream) {
        std::cerr << "[Research] arvore nao encontrada: " << fileName << std::endl;
        return false;
    }
    Reader r;
    r.t.assign(reinterpret_cast<const char*>(stream->data()), stream->size());
    s_maxTechID = maxTechLevel * 1000 + 999;
    std::vector<std::unique_ptr<Tech>> list;
    for (;;) {
        auto t = std::make_unique<Tech>();
        if (readTech(r, *t) != 1) break;
        list.push_back(std::move(t));
    }
    if (list.empty()) return false;
    std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) { return a->id < b->id; });
    auto root = std::make_unique<Tech>();
    root->id = 0;
    root->researched = 0xFFFFFFFFu;
    s_techs.push_back(std::move(root));
    for (auto& t : list) s_techs.push_back(std::move(t));
    // Pré-requisitos: IDs → índices; dependentes.
    for (auto& t : s_techs) {
        for (int& q : t->requires) {
            const int i = techIndex(q);
            if (i == -1) {
                std::cerr << "[Research] pre-requisito " << q << " de " << t->id << " nao existe" << std::endl;
                clearTechTree();
                return false;
            }
            q = i;
        }
    }
    for (int i = 0; i < techCount(); ++i)
        for (int q : s_techs[i]->requires) s_techs[q]->dependents.push_back(i);
    // Research Topic dos descritores (desc+0x1E4) → índice (0 se não existe).
    for (int type = 1; type <= 0x72; ++type) {
        const int id = unitResearchTopic(type);
        const int i = id ? techIndex(id) : -1;
        s_topicIndex[type] = i == -1 ? 0 : i;
    }
    std::cout << "[Research] " << fileName << ": " << (techCount() - 1) << " tecnologias, ID max "
              << s_maxTechID << std::endl;
    return true;
}

static int factionCost(const Tech& t, int player) {          // [+0xC + isEden*4]
    return g_playerData[player].isEden ? t.edenCost : t.plymouthCost;
}

static bool requiresMet(const Tech& t, unsigned bit) {
    for (int q : t.requires) if (!(s_techs[q]->researched & bit)) return false;
    return true;
}

static void applyTech(int player, int index) {               // sub_473860
    Tech& t = *s_techs[index];
    const unsigned bit = 1u << player;
    if (t.researched & bit) return;
    t.researched |= bit;
    if (player == TethysGame::localPlayer()) s_uiCache = -1;
    for (const TechUpgrade& u : t.upgrades) {
        if (u.kind == 0) {
            uint8_t* blk = unitStatBlock(u.unitType, player);
            if (!blk) continue;
            int k = 0;                                        // slot em +0x3C (índice ou 1º zero)
            for (;; ++k) {
                int32_t v; std::memcpy(&v, blk + 0x3C + 4 * k, 4);
                if (v == index || v == 0) break;
            }
            std::memcpy(blk + u.offset, &u.value, 4);
            const int32_t idx = index;
            std::memcpy(blk + 0x3C + 4 * k, &idx, 4);
        } else if (u.kind == 1) {
            if (u.offset == 0x20) g_playerData[player].ore = u.value;   // "Ore"
        }
        // kind 2 (FUNCTION_RESULT): "Test_Function" = nullsub_110.
    }
}

static void propagate(int player, int start) {               // sub_472D80
    const unsigned bit = 1u << player;
    std::vector<int> stack{start};
    while (!stack.empty()) {
        const int node = stack.back();
        stack.pop_back();
        for (int d : s_techs[node]->dependents) {
            const Tech& t = *s_techs[d];
            if (t.researched & bit) continue;
            if (factionCost(t, player) != 0) continue;
            if (t.id > s_maxTechID) continue;
            if (!requiresMet(t, bit)) continue;
            applyTech(player, d);
            stack.push_back(d);
        }
    }
}

void researchStartGame() {                                    // sub_472C30
    if (!techCount()) return;
    for (int p = 0; p < 6; ++p) propagate(p, 0);
}

void researchMarkComplete(int player, int index) {           // sub_473830
    if (index < 0 || index >= techCount() || player < 0 || player > 6) return;
    applyTech(player, index);
    propagate(player, index);
}

void researchSetTechLevel(int player, int level) {           // sub_472F70
    if (!techCount() || player < 0 || player > 6) return;
    const int maxID = level * 1000;
    const unsigned bit = 1u << player;
    std::vector<int> stack{0};
    while (!stack.empty()) {
        const int node = stack.back();
        stack.pop_back();
        for (int d : s_techs[node]->dependents) {
            const Tech& t = *s_techs[d];
            if (t.id > maxID) continue;
            if (factionCost(t, player) == -1) continue;
            if (!requiresMet(t, bit)) continue;
            if (!(t.researched & bit)) { applyTech(player, d); propagate(player, d); }
            stack.push_back(d);
        }
    }
}

bool researchHas(int player, int index) {                    // sub_472C50
    if (TethysGame::cheatFlag(1)) return true;
    if (index < 0 || index >= techCount() || player < 0 || player > 6) return false;
    // Basic Lab (0x21) deixa de valer com o Standard Lab (0x31) pesquisado.
    if (s_topicIndex[0x21] == index && (s_techs[s_topicIndex[0x31]]->researched & (1u << player)))
        return false;
    return (s_techs[index]->researched & (1u << player)) != 0;
}

} // namespace op2
