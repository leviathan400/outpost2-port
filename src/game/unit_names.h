#pragma once

namespace op2 {

// Code_Name de cada descritor (vt[1] = GetName de off_4E1340[map_id]), 1..0x72.
// nullptr se o tipo não tem descritor. Fonte: construtores estáticos do EXE
// (mov [desc], vtable; mov [desc+4], map_id) + structure_anim/vehicle_anim.
const char* unitCodeName(int type);

// sub_445010: map_id pelo Code_Name (varre 1..0x72 na ordem); −1 se não há.
int unitTypeFromCodeName(const char* name);

} // namespace op2
