#include "command.h"

#include "command_apply.h"
#include "construction.h"
#include "docking.h"
#include "earthwork.h"
#include "mining.h"
#include "missile.h"
#include "production.h"
#include "repair.h"
#include "research_lab.h"
#include "scenario.h"
#include "unit.h"
#include "weapon.h"
#include "colony.h"

namespace op2 {

// Porta o resto de sub_40E210: o switch pelo tipo do pacote. O caso de
// movimento fica em command.cpp (loc_40E2C0); tudo o mais cai aqui, e cada
// caso so chama o modulo que ja sabia fazer aquilo.
//
// A ordem dos casos segue o enum CommandType do original.
void dispatchCommandPacket(int owner, const CommandPacket& p) {
    switch (p.type) {
        case CMD_DOZE:
            if (!p.unitIds.empty()) applyDoze(owner, p.unitIds[0], p.rectL, p.rectT, p.rectR, p.rectB);
            return;
        case CMD_STOP:          applyStop(owner, p.unitIds); return;
        case CMD_BUILD:
            for (int id : p.unitIds)
                processBuildPacket(owner, id, p.waypoints.empty() ? 0u : p.waypoints[0],
                                   p.rectL, p.rectT, p.rectR, p.rectB, p.arg0);
            return;
        case CMD_BUILD_WALL:
            for (int id : p.unitIds)
                if ((g_units[id].owner & 0x0F) == (owner & 0x0F))
                    earthworkWallOrder(id, p.rectL, p.rectT, p.rectR, p.rectB, p.arg0);
            return;
        case CMD_REMOVE_WALL:   applyRemoveWall(owner, p.unitIds, p.rectL, p.rectT); return;
        case CMD_PRODUCE:
            if (!p.unitIds.empty()) processProducePacket(owner, p.unitIds[0], p.arg0, p.arg1, p.arg2);
            return;
        case CMD_XFER_CARGO:
            if (!p.unitIds.empty()) processBayTransfer(owner, p.unitIds[0], p.arg0);
            return;
        case CMD_LOAD_CARGO:
            if (!p.unitIds.empty()) applyLoadCargo(owner, p.unitIds[0]);
            return;
        case CMD_UNLOAD_CARGO:
            if (!p.unitIds.empty()) applyUnloadCargo(owner, p.unitIds[0]);
            return;
        case CMD_DUMP_CARGO:
            for (int id : p.unitIds)
                if (id > 0 && id < static_cast<int>(g_units.size()) && (g_units[id].owner & 0x0F) == (owner & 0x0F))
                    dumpCargoOrder(id);
            return;
        case CMD_IDLE:
            for (int id : p.unitIds)
                if (id > 0 && id < static_cast<int>(g_units.size()) && (g_units[id].owner & 0x0F) == (owner & 0x0F))
                    structureDoIdle(id);
            return;
        case CMD_UNIDLE:
            for (int id : p.unitIds)
                if (id > 0 && id < static_cast<int>(g_units.size()) && (g_units[id].owner & 0x0F) == (owner & 0x0F))
                    structureDoActivate(id);
            return;
        case CMD_SELF_DESTRUCT: applySelfDestruct(owner, p.unitIds); return;
        case CMD_SCATTER:       applyScatter(owner, p.unitIds); return;
        case CMD_RESEARCH:
            if (!p.unitIds.empty()) processStartResearch(owner, p.unitIds[0], p.arg0, p.arg1);
            return;
        case CMD_TRAIN:
            if (!p.unitIds.empty()) processTrainPacket(owner, p.unitIds[0], p.arg0);
            return;
        case CMD_LAUNCH:
            if (!p.unitIds.empty()) spaceportLaunch(p.unitIds[0], p.arg0, p.arg1);
            return;
        case CMD_REPAIR_OBJ:
        case CMD_REPROGRAM:
        case CMD_DISMANTLE:
            processRepairPacket(owner, p.type, p.unitIds, p.waypoints, p.target);
            return;
        case CMD_SALVAGE:
            applySalvage(owner, p.unitIds, p.rectL, p.rectT, p.rectR, p.rectB);
            return;
        case CMD_LIGHT_TOGGLE:
            for (int id : p.unitIds)
                if (id > 0 && id < static_cast<int>(g_units.size()) && (g_units[id].owner & 0x0F) == (owner & 0x0F))
                    unitSetLights(id, p.arg0);
            return;
        case CMD_ATTACK:
        case CMD_GUARD:
            processAttackPacket(owner, p.type, p.unitIds, p.waypoints, p.target);
            return;
        case CMD_TRANSFER: applyTransfer(owner, p.unitIds, p.arg0); return;
        case CMD_CHAT:  applyChat(owner, p.arg0, p.text); return;
        case CMD_ALLY:  applyAlly(owner, p.arg0); return;
        case CMD_QUIT:  applyQuit(owner, p.arg0); return;
        default: return;                       // Nop e os que o porte nao usa
    }
}

} // namespace op2
