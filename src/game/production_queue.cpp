#include "production_queue.h"

#include <map>

#include "commands.h"
#include "player.h"
#include "production.h"
#include "tethys_game.h"
#include "unit.h"

namespace op2 {

namespace {

struct Fila {
    int type = 0;                     // o tipo da fabrica (indice reaproveitado = outra unidade)
    std::vector<uint32_t> items;      // na ordem dos cliques; items[0] e o proximo
    bool pending = false;             // pacote 9 enviado, esperando a fabrica comecar
    uint32_t sendTick = 0;
    uint32_t nextTry = 0;
};

bool s_on = false;
std::map<int, Fila> s_filas;

bool isFactoryType(int t) { return t == 0x2D || t == 0x2E || t == 0x2F || t == 0x30 || t == 0x37; }

// A fabrica ainda e a mesma, viva e do jogador local?
bool factoryValid(int id, int type) {
    if (id <= 0 || id >= static_cast<int>(g_units.size())) return false;
    const UnitRecord& f = g_units[id];
    if (f.id == -1 || (f.flags & UNIT_DEAD) || !isFactoryType(f.type)) return false;
    if (type && f.type != type) return false;
    return (f.owner & 0x0F) == TethysGame::localPlayer();
}

void addCost(int factoryId, uint32_t item, int& common, int& rare, int& food) {
    int c = 0, r = 0;
    produceCost(factoryId, item, c, r);
    common += c; rare += r;
    if ((item & 0xFFFF) == 0x66) food += 10000;          // Food Cargo
}

const std::vector<uint32_t> kVazia;

} // namespace

void setProductionQueue(bool on) {
    s_on = on;
    if (!on) s_filas.clear();                            // desligar devolve o jogo ao original
}
bool productionQueueOn() { return s_on; }

void productionQueueCommitted(const std::vector<std::pair<int, uint32_t>>& draft,
                              int& common, int& rare, int& food) {
    common = rare = food = 0;
    for (const auto& kv : s_filas) {
        if (!factoryValid(kv.first, kv.second.type)) continue;
        for (uint32_t item : kv.second.items) addCost(kv.first, item, common, rare, food);
    }
    for (const auto& d : draft) addCost(d.first, d.second, common, rare, food);
}

bool productionQueueCanAdd(int factoryId, const std::vector<uint32_t>& draft, uint32_t item) {
    if (!factoryValid(factoryId, 0)) return false;
    std::vector<std::pair<int, uint32_t>> d;
    d.reserve(draft.size() + 1);
    for (uint32_t x : draft) d.emplace_back(factoryId, x);
    d.emplace_back(factoryId, item);
    int common, rare, food;
    productionQueueCommitted(d, common, rare, food);
    const PlayerData& pd = g_playerData[TethysGame::localPlayer()];
    return common <= pd.ore && rare <= pd.rareOre && food <= pd.foodStored;
}

void productionQueueAppend(int factoryId, const std::vector<uint32_t>& items) {
    if (!s_on || items.empty() || !factoryValid(factoryId, 0)) return;
    Fila& q = s_filas[factoryId];
    if (q.type != g_units[factoryId].type) q = Fila{};
    q.type = g_units[factoryId].type;
    q.items.insert(q.items.end(), items.begin(), items.end());
    q.nextTry = 0;                                       // tenta ja no proximo tique
}

const std::vector<uint32_t>& productionQueueOf(int factoryId) {
    auto it = s_filas.find(factoryId);
    if (it == s_filas.end() || !factoryValid(factoryId, it->second.type)) return kVazia;
    return it->second.items;
}

void productionQueueClear(int factoryId) { s_filas.erase(factoryId); }
void productionQueueReset() { s_filas.clear(); }

void productionQueueTick() {
    if (!s_on) return;
    const uint32_t t = TethysGame::tick();
    for (auto it = s_filas.begin(); it != s_filas.end();) {
        const int id = it->first;
        Fila& q = it->second;
        if (!factoryValid(id, q.type) || q.items.empty()) { it = s_filas.erase(it); continue; }
        UnitRecord& f = g_units[id];
        if (q.pending) {
            if (f.action == 9) {                         // comecou: sai da fila
                q.items.erase(q.items.begin());
                q.pending = false;
                if (q.items.empty()) { it = s_filas.erase(it); continue; }
                ++it; continue;
            }
            // Na rede o pacote leva kTurnLead turnos; passado bem disso sem a
            // fabrica comecar, foi recusado (minerio gasto, operarios...).
            if (t - q.sendTick < 24) { ++it; continue; }
            q.pending = false;
            q.nextTry = t + 32;
        }
        if (f.action != 0 || t < q.nextTry) { ++it; continue; }
        // As mesmas condicoes do botao BUILD (sub_456000) e do BUILD IT
        // (sub_468430): operando, baia livre, limite de unidades, minerio.
        const uint32_t item = q.items.front();
        if (!uiCommandAvailable(UC_BUILD, std::vector<int>{id}) || !produceAffordable(id, item)) {
            q.nextTry = t + 8;                           // espera sem pesar a cada tique
            ++it; continue;
        }
        sendProduce(id, item);
        q.pending = true;
        q.sendTick = t;
        if (f.action == 9) {                             // um jogador so: aplicado na hora
            q.items.erase(q.items.begin());
            q.pending = false;
            if (q.items.empty()) { it = s_filas.erase(it); continue; }
        }
        ++it;
    }
}

} // namespace op2
