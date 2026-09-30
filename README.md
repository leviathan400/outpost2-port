# Outpost 2: Divided Destiny — Port Moderno (C++17 + SDL2)

Port em andamento do RTS **Outpost 2: Divided Destiny** (Sierra On-Line / Dynamix,
1997) para plataformas modernas, em **C++17 + SDL2**, reconstruído por engenharia
reversa a partir do disassembly do executável original.

> **Diretiva do projeto:** todo mecanismo é extraído do **código original**
> (disassembly/binário) — nada é adivinhado. A fidelidade é validada contra o
> jogo original (inclusive os renders oficiais de mapa da wiki da comunidade).

## ⚠️ Arquivos originais não incluídos (direitos autorais)

Este repositório contém **apenas o código do port**. Os arquivos do jogo de 1997
(`OUTPOST2.EXE`, `*.DLL`, `*.VOL`, `OP2_ART.BMP`, `*.ANI`, `OUTPOST2.INI`…) e o
disassembly são material comercial protegido e **não** são versionados. Para
rodar, é preciso ter uma cópia legítima do jogo e colocar o executável do port
na pasta de instalação (junto dos assets originais).

Também não é versionada a `third_party/SDL2-*` (baixe o pacote de desenvolvimento
do SDL2 2.30.9 e ajuste o caminho no `CMakeLists.txt`).

## Estrutura

```
src/
  main.cpp            Entry point, loop SDL2, modos utilitários
  gfx/                Renderer (framebuffer 640x480), bitmap/DIB, fonte, PBMP (tilesets)
  res/                Leitor de PE, VOL + LZHUF, ResManager (gateway de assets)
  game/               GameMap, TerrainRenderer, tipos (LOCATION/MAP_RECT)
  shell/              Menu principal (shell) — máquina de estados
  core/               Config (OUTPOST2.INI)
tools/                Scripts Python de engenharia reversa (VOL, map, PBMP, sprites, sheets)
CMakeLists.txt        Build (CMake + MSVC, C++17, SDL2)
PORT_STATUS.md        Estado detalhado do port e dos formatos decodificados
```

## Build

```powershell
cmake -B build -A Win32          # (ou -A x64)
cmake --build build --config Release
# O exe é copiado para a raiz do jogo (junto dos assets) pelo post-build.
```

## Modos utilitários

```powershell
outpost2_modern.exe                       # menu
outpost2_modern.exe --game eden01.map     # visualiza o terreno (rola com as setas)
outpost2_modern.exe --rendermap eden01.map out.bmp   # render do mapa inteiro
outpost2_modern.exe --extract <nome> <saida>         # extrai um recurso do VOL
```

## Estado atual

- ✅ Leitor de recursos PE (bitmaps + diálogos) e do `OUTPOST2.INI`
- ✅ `ResManager` + `.VOL`/LZHUF (byte-exato)
- ✅ `GameMap` + render de terreno in-engine (fiel ao original: layout em faixas,
  paleta R,G,B, wrap horizontal)
- ✅ Menu principal (shell) com arte original + navegação por teclado
- ⬜ Backbone `TApp`/`UIState`, simulação (`TethysGame`/`Unit`/`_Player`), HUD

Veja `PORT_STATUS.md` para o detalhamento.
