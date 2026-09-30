#pragma once

namespace op2 {

// Coordenada de célula do mapa (engine: struct LOCATION, passada por valor como
// dois int em GameMap::GetTile/GetCellType etc.).
struct LOCATION {
    int x = 0;
    int y = 0;
};

// Retângulo de células (engine: struct MAP_RECT). x1,y1 inclusivo; x2,y2 exclusivo
// no uso do clipRect do mapa.
struct MAP_RECT {
    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    int width()  const { return x2 - x1; }
    int height() const { return y2 - y1; }
};

} // namespace op2
