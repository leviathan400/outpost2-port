#pragma once

namespace op2 {

// Driver de movimento por tick. Para cada unidade com ordem de movimento ativa,
// consome a rota do pathfinder REAL (pfSearchRoute = sub_43F5B0) e desliza a
// unidade pelas células exatas dela (ocupação do grid + rotação). O algoritmo de
// BUSCA é 1:1 com o original; este executor substitui, por ora, o integrador
// byte-exato sub_44B650 (que depende do modelo completo do objeto Unit/descritor,
// ainda a mapear) — mas NÃO substitui o pathfinder: a rota é a do original.
void tickMovement();
void moveDockArrive(int id);          // ações 3/4 no destino (giro + doca)

} // namespace op2
