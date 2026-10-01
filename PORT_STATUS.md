# Outpost 2: Divided Destiny — Port Moderno (C++/SDL2)

> Documentação do estado atual do port. Reconstruída por engenharia reversa a
> partir dos arquivos existentes na pasta (a IA anterior não deixou registro
> escrito). Última análise: **2026-09-29**.

---

## 1. Visão geral

Este é um **port em andamento** do RTS *Outpost 2: Divided Destiny* (Sierra
On-Line / Dynamix, 1997) para plataformas modernas usando **C++17 + SDL2**.

O jogo original é um executável Win32 de 32 bits (`OUTPOST2.EXE`, 1.1 MB,
compilado com Visual C++, roda a 640×480 em 16-bit color). O port ainda **não
reimplementa o jogo em si** — o que existe hoje é a **fundação técnica** e uma
**reconstrução funcional do menu principal (shell)**, lendo os recursos de arte
e diálogos diretamente das DLLs originais de 1997.

A estratégia adotada pela IA anterior é clara pelos artefatos:
1. **Disassembly completo** do `OUTPOST2.EXE` via IDA Pro (Hex-Rays) → base de
   referência 1:1 para reimplementação.
2. **Leitura direta dos recursos PE** (`.rsrc`) das DLLs originais em vez de
   re-exportar assets — o port lê bitmaps e templates de diálogo de
   `OP2SHRES.DLL` em tempo de execução.
3. **Renderização própria** num framebuffer 640×480 nativo, escalado via SDL2
   para telas modernas.

---

## 2. Estrutura de pastas

```
Outpost 2 - Divided Destiny/
├── src/                     # ← CÓDIGO DO PORT MODERNO (C++17)
│   ├── main.cpp             #   Entry point, loop SDL2, modo --screenshot
│   ├── gfx/
│   │   ├── renderer.cpp/.h  #   Framebuffer 640x480 + SDL2, primitivas UI
│   │   ├── bitmap.cpp/.h    #   Decoder DIB/BMP + RLE8 (recursos RT_BITMAP)
│   │   └── font.cpp/.h      #   Fonte bitmap embutida (desenho de texto)
│   ├── res/
│   │   └── pe_resource.cpp/.h  # Leitor de recursos de PE (DLL/EXE Win32)
│   ├── core/
│   │   └── config.cpp/.h    #   Leitor/gravador do OUTPOST2.INI (= CConfig)
│   └── shell/
│       └── shell_menu.cpp/.h   # Máquina de estados do menu principal
│
├── Outpost2_src/            # ← DISASSEMBLY DE REFERÊNCIA (IDA Pro)
│   ├── OUTPOST2.asm         #   13.7 MB — assembly x86 completo do EXE
│   ├── OUTPOST2.lst         #   13.7 MB — listing com bytes + assembly
│   └── OUTPOST2.amp.map     #   1 MB — mapa de símbolos (30.123 entradas)
│
├── tools/                   # ← SCRIPTS DE ANÁLISE (Python)
│   ├── extract_menu.py      #   Extrai templates de diálogo das DLLs → JSON
│   ├── dump_dialogs.py      #   Dump de diálogos
│   ├── analyze_shell.py     #   Rastreia refs a IDs de recurso no assembly
│   ├── extract_bmps.py      #   Extrai bitmaps das DLLs → PNG/BMP
│   ├── extract_btns.py      #   Extrai bitmaps de botões
│   ├── inspect_vol.py       #   Inspeciona arquivos .VOL (containers de assets)
│   ├── parse_dlg.py         #   Parser de DLGTEMPLATE
│   └── ... (bmp_*.png/bmp gerados como amostras)
│
├── third_party/
│   └── SDL2-2.30.9/         #   SDL2 pré-compilado (include + lib x86/x64)
│
├── build/                   # ← BUILD CMake/MSVC (Visual Studio, Win32)
│   └── Release/outpost2_modern.exe  (64 KB — build funcional)
│
├── CMakeLists.txt           # ← Build system do port
├── menu_dialog.json         # Template de diálogo extraído (dump auxiliar)
├── menu_screen.png/.bmp     # Screenshots do menu renderizado pelo port
├── outpost2_modern.exe      # Cópia do executável do port na raiz
│
└── [ARQUIVOS ORIGINAIS DE 1997 — o jogo instalado]
    ├── OUTPOST2.EXE         #   Executável original (alvo do disassembly)
    ├── OUTPOST2.EXE.i64     #   Database do IDA Pro (17 MB)
    ├── OP2SHRES.DLL         #   Recursos do shell/menu (2.4 MB) ← LIDO PELO PORT
    ├── OP2SHELL.DLL         #   Código do shell/menu
    ├── OUT2RES.DLL          #   Recursos do jogo (451 KB)
    ├── *.VOL                #   Containers de assets (MAPS, SOUND, VOICES, etc.)
    ├── *.DLL                #   Missões/campanhas (E##, P##, T##, M*, etc.)
    ├── *.ANI                #   Cursores animados
    └── OP2_ART.BMP          #   Spritesheet de arte (7.5 MB)
```

---

## 3. O que já está implementado (o port moderno)

### 3.1. `main.cpp` — Entry point
- Inicializa SDL2 (vídeo/áudio/timer).
- Cria janela **1280×960** (2× do nativo 640×480) — não-fullscreen.
- Instancia `Renderer` + `ShellMenu` (carregando `OP2SHRES.DLL`).
- **Loop principal ~60 FPS** com delta-time: eventos de mouse/teclado →
  `ShellMenu`, depois `update()` + `render()` + `present()`.
- Mapeia coordenadas de tela → coordenadas nativas 640×480 (`screenToNative`).
- **Modo `--screenshot <arquivo>`**: renderiza um frame do menu e salva como BMP
  32-bit (usado para gerar `menu_screen.bmp`).

### 3.2. `gfx/renderer.*` — Renderizador
- Framebuffer software **640×480 em ARGB** (`std::vector<uint32_t>`), enviado a
  uma `SDL_Texture` e escalado para a janela mantendo aspect ratio (viewport
  com barras).
- Primitivas: `clear`, `fillRect`, `drawRect`, `drawBevelRect` (bordas 3D
  chanfradas), `blitBitmap` (+ color-key), `blitBitmapSub`.
- **Widgets no estilo Outpost 2**: `drawDialogFrame` (moldura metálica escura
  com título) e `drawButton` (estados hover/pressed/disabled).

### 3.3. `gfx/bitmap.*` — Decoder de imagens
- `loadFromDIB`: interpreta cabeçalho `BITMAPINFOHEADER` de recursos `RT_BITMAP`.
- **Suporta 8-bit indexado + paleta e descompressão RLE8** (`decompressRLE8`),
  que é o formato dos backgrounds do menu original.
- Converte tudo para ARGB 32-bit interno.

### 3.4. `gfx/font.*` — Texto
- Fonte **bitmap embutida no código** (não usa a fonte do sistema), com desenho
  de texto, texto centralizado, sombra, e medição de largura/altura.

### 3.5. `res/pe_resource.*` — Leitor de recursos PE
- **Parser completo de PE de 32 bits** (DOS header → NT headers → seções →
  árvore `.rsrc`), com conversão RVA→offset.
- `getBitmapDIB(id)` → recurso `RT_BITMAP` (tipo 2).
- `getDialog(id)` → faz parsing de `DLGTEMPLATE` (controles, classes atômicas
  Button/Edit/Static/ListBox/etc., estilos, texto). **Este é o coração da
  fidelidade**: o layout do menu vem dos dados reais do jogo, não é chutado.

### 3.6. `core/config.*` — Configurações (OUTPOST2.INI) ✅ (2026-09-30)
Porta a classe **`CConfig`** do jogo original (que era um wrapper sobre
`GetPrivateProfileInt/String` + `WritePrivateProfileString` do Windows), agora
cross-platform. Mesma semântica confirmada no disassembly: seção/chave
case-insensitive, `getInt` lê os dígitos iniciais e cai no default, `setInt`
aceita `"Seção\\Chave"`. `GameSettings` espelha as chaves reais do `[Game]`
(Music, MusicVol, Sound, SoundVol, GameSpeed, ScrollRate, Overlay, Ambient,
StationaryShadows, MobileShadows, IntroAVI, ShowMovies, SpiffyDraw, Demo, Name)
com os defaults do INI. **Validado**: lê o `OUTPOST2.INI` real, round-trip
preserva todas as seções (`[GAME_WINDOW]`, `[ColonyGameScores]`, `[DESKTOP]`…) e
o formato; já carregado no `main.cpp` no startup.

### 3.7. `shell/shell_menu.*` — Menu principal (shell)
Máquina de estados (`MenuState`) reconstruindo o menu do jogo:
- Carrega de `OP2SHRES.DLL`: **Bitmap 190** (fundo espaço/planeta 640×480),
  **Bitmap 145** (logo), **Dialog 137** ("MAIN MENU").
- **9 botões** mapeados aos IDs reais do Dialog 137: `1014 TUTORIALS`,
  `1000 NEW CAMPAIGN`, `1006 COLONY GAMES`, `1005 MULTIPLAYER` (desabilitado),
  `1007 LOAD GAME`, `1011 GAME PREFERENCES`, `1012 HELP`,
  `1010 ABOUT OUTPOST 2`, `1013 QUIT`.
- Sub-diálogos **renderizados (placeholder/mock)**: New Campaign (seleção
  Eden/Plymouth + dificuldade), Colony Games (lista), Preferences, About.
- **Janela de Tutoriais ✅ fiel ao original** (Dialog 269 "TUTORIALS" do
  OP2SHRES.DLL): listbox com scrollbar vertical + botões `START` e
  `BACK TO MAIN MENU`. A lista é montada como no jogo: enumera as missões
  `T01.DLL`..`T14.DLL` e lê o **export `LevelDesc`** de cada uma (14 tutoriais).
  Suporta seleção por clique, scroll (roda/​setas) e recorte de texto. O
  `PeResourceReader` ganhou `getExportString()` para ler exports de dados das
  missões (`LevelDesc`, `MapName`, `TechtreeName`). Captura:
  `outpost2_modern.exe --tutorials --screenshot out.bmp` (rodar da raiz do jogo).

---

## 4. Ferramentas de análise (`tools/`)

Scripts Python que usam **`pefile`** + **`capstone`** (disassembler) para
engenharia reversa dos recursos e do código:
- **`extract_menu.py` / `dump_dialogs.py` / `parse_dlg.py`** — extraem e
  interpretam `DLGTEMPLATE` das DLLs (geram JSON como `menu_dialog.json`).
- **`extract_bmps.py` / `extract_btns.py` / `inspect_bmps.py`** — extraem
  bitmaps (fundos, botões) para PNG/BMP de conferência.
- **`analyze_shell.py`** — busca no assembly onde IDs de recurso (ex.: bitmap
  225/190, string "MAIN MENU") são referenciados, para descobrir a lógica.
- **`inspect_vol.py` / `inspect_table.py`** — inspecionam os containers `.VOL`.

> **Dependências Python** (não versionadas): `pip install pefile capstone pillow`.

---

## 4.1. Formato `.VOL` — DECODIFICADO ✅ (2026-09-29)

Container de assets do jogo, conceitualmente igual ao `.WAD` do Doom/Quake:
diretório de entradas + dados concatenados. Usa **chunks com tag** (estilo
RIFF/IFF). Parser funcional em [tools/vol.py](tools/vol.py).

**Layout:**
```
"VOL " <u32 len>            container do diretório (payload = volh+vols+voli)
    "volh" <u32 len=0>      volume header (vazio)
    "vols" <u32 len>        STRING TABLE:  <u32 strLen> + nomes '\0'-separados
    "voli" <u32 len>        INDEX:         (len/14) entradas de 14 bytes
"VBLK" <u32 len> data...    um bloco por arquivo (dados; alinhado a 2 bytes)
"VBLK" <u32 len> data...
...
```
- Todo `u32 len` de chunk tem o bit `0x80000000` como flag "em uso"; tamanho
  real = `len & 0x7FFFFFFF`.
- **IndexEntry (14 bytes):** `u32 nameOffset` (0xFFFFFFFF = slot livre) ·
  `u32 dataBlockOffset` (aponta pro "VBLK") · `u32 fileSize` (tamanho
  **armazenado/comprimido**) · `u8 compressionType` (+0x0C) · `u8 flag` (0x01).

**Compressão** (confirmada no disassembly — `sub_4A0E50` faz switch sobre
`[entry+0x0C]`):

| Tipo | Rotina no EXE | Algoritmo |
|:----:|---------------|-----------|
| 0 | (bloco cru) | sem compressão |
| 1 | `sub_4AFA60` | provável RLE |
| 2 | `sub_434460` | provável LZ |
| 3 | `sub_433F80` | **LZHUF** (LZSS + Huffman adaptativo, Okumura 1989) |

O construtor do tipo 3 (`sub_433F80`) preenche `4096-60 = 0xFC4` posições de um
ring buffer com `' '` e inicia `r = 0xFC4` → parâmetros **N=4096, F=60,
THRESHOLD=2** (a assinatura clássica do LZHUF).

**LZHUF implementado** ✅ em [tools/lzhuf.py](tools/lzhuf.py). As tabelas de
decodificação de posição `d_code[256]`/`d_len[256]` foram **extraídas do binário**
(offsets de arquivo `0xE9700` e `0xE9800`) para garantir decode byte-exato — não
reconstruídas de memória. Os 10 `.txt` do `SHEETS.VOL` descomprimem para texto
tab-separado limpo (tabelas de balanceamento: `building.txt`, `vehicles.txt`,
`weapons.txt`, `mines.txt`, `morale.txt`, `space.txt`, `edentek.txt`,
`ply_tek.txt`, `multitek.txt`, `tutortek.txt`).

> Nota: o `.VOL` não armazena o tamanho descomprimido. O engine trata o bloco
> como um *stream* e decodifica sob demanda até os bits acabarem; o parser de
> planilhas para na primeira linha inválida. Nosso decoder reproduz isso fielmente
> — a saída pode ter alguns bytes de "lixo" no fim (bits de padding do último
> byte), que serão ignorados ao parsear as tabelas, tal como no jogo original.

**Conteúdo dos 5 volumes (437 arquivos):**

| Volume | Arquivos | Compressão | Conteúdo |
|--------|:--------:|------------|----------|
| `MAPS.VOL` | 71 | cru | `.map` (mapas) + `.bmp` |
| `SOUND.VOL` | 97 | cru | `.wav` (efeitos) |
| `STORY.VOL` | 124 | cru | `.rtf` (briefings/narrativa) |
| `VOICES.VOL` | 135 | cru | `.wav` (vozes, ex.: `savant*.wav`) |
| `SHEETS.VOL` | 10 | **LZHUF** | `.txt` (tabelas de balanceamento) |

**Status:** ✅ **Formato `.VOL` 100% decodificado** — todos os 437 arquivos dos 5
volumes extraem e validam (crus e LZHUF). Uso: `python tools/vol.py` (scan) ou
`python tools/vol.py extract <VOL> <saida/>`. Parser: [tools/vol.py](tools/vol.py);
decoder LZHUF: [tools/lzhuf.py](tools/lzhuf.py).

---

## 4.2. Formato `.map` — DECODIFICADO ✅ (2026-09-29)

Mapas de terreno do jogo (tag `0x00001011`). Parser em [tools/op2map.py](tools/op2map.py),
validado visualmente (render do grid mostra terreno coerente: mesas, lava, minério).

**Estrutura** (deduzida dos bytes + disassembly, validada em vários mapas):
```
Header (5 x u32 = 20 bytes):
  u32 tag        = 0x00001011
  u32 versionTag = 0
  u32 lgWidth    -> width = 1 << lgWidth   (ex.: 6 -> 64, 7 -> 128)
  u32 height     (em tiles)
  u32 numTileMappings  (ex.: 512 — usado na seção "TILE SET", ainda TODO)

TileData [width*height] (u32 cada) — LAYOUT EM FAIXAS (NÃO row-major):
  idx_u32 = ((x>>5) * height + y) * 32 + (x & 31)    (faixas verticais de 32 col)
  bits 0-4  : cellType  (passabilidade/terreno; 0-31)
  bits 5-15 : tileIndex (índice no pool global; máscara 0xFFE0 >> 5, 11 bits)
  bit  27   : flag por célula (lava/dirty), usado pelas rotinas de paint
  x dá wrap horizontal (widthMask = width-1); height é sempre potência de 2.

clipRect (4 x i32): x1, y1, x2, y2  (área jogável; eden01 = 32,0,95,62)

Tabela de tilesets: repete { u32 nameLen; char name[nameLen]; u32 numTiles }
  enquanto nameLen for plausível. Ex.: well0000..well0012 (13 fontes .bmp;
  soma de numTiles = pool global de tiles, ex.: 2012).

(Depois, em "TILE SET": tile mappings + grupos de terreno/animações — TODO.)
```

**Conteúdo do `MAPS.VOL`:** 54 mapas `.map` (dimensões 64×64, 128×64, etc.) +
alguns `.bmp` (paletas). Dimensões vêm do header; largura sempre potência de 2.

**Status:** ✅ header, grid de tiles (cellType + tileIndex), clipRect, tabela de
tilesets e render com gráficos — completos. **Mecanismo confirmado no disassembly**:
o acesso em faixa aparece idêntico em `GameMap::GetTile` (@191591) e nas rotinas de
paint `sub_4322C0`/`sub_432560` (`((x>>5)<<lgHeight + y)*128 + (x&31)*4` sobre
`dword_54F01C`). O render do port é **pixel-idêntico ao render original da wiki**
(eden01/eden02). Uso: `python tools/op2map.py eden01.map` ou
`outpost2_modern.exe --rendermap eden01.map out.bmp`.

**Seção "TILE SET"** (após a tabela de tilesets) — *entendida, não necessária para
renderizar*: uma **tabela de tile mappings** (`numTileMappings`, ex. 512, registros
`{u16 tileNumber, u16 tilesetIndex}` + índice) seguida de **grupos de terreno
nomeados** (`BLUE`, `mudbas1`, `mudcrk1`… = tipos de terreno / sequências de
animação de tile, ex. lava). O render de terreno funciona sem ela (o `tileIndex`
já aponta pro pool global); ela serve para animação de tile e tipagem de terreno
(polish de runtime do motor).

---

## 4.3. Tilesets `PBMP` + render de terreno — DECODIFICADO ✅ (2026-09-29)

Os gráficos de terreno (`well0000.bmp`..`well0012.bmp`) ficam **dentro do
`MAPS.VOL`** e usam o formato próprio **`PBMP`** (chunked, IFF-like). Carregados
em runtime via `ResManager::OpenStream("well####.bmp")` (busca em VOLs/soltos).

**Formato PBMP** — parser em [tools/pbmp.py](tools/pbmp.py):
```
"PBMP" u32 size
  "head" u32 len(20)  { u32 ver=2; u32 width; u32 height; u32 bpp=8; u32 ? }
  "PPAL" u32 len      (paleta)
      "head" u32 len(4)   { u32 numPalettes=1 }
      "data" u32 len(1024){ RGBQUAD[256]: b,g,r,a }
  "data" u32 len      { pixels 8-bit, width*height, top-down }
```
Cada tileset é uma coluna de tiles **32×32** empilhados (`height = numTiles*32`).
Também no `MAPS.VOL`: `op2_art.prt` (paleta da arte, chunks `CPAL`/`PPAL`) e
`virmask.raw` (máscara). O `OP2_ART.BMP` (raiz) é um BMP 8-bit 2732×2731.

**Render de terreno** — [tools/render_map.py](tools/render_map.py): junta a
pipeline completa `.VOL → .map → PBMP` e desenha o mapa. O `tileIndex` de cada
célula indexa o **pool global** (concatenação dos tiles de todos os tilesets do
mapa, na ordem listada). ✅ **Validado**: `eden01.map` renderiza como terreno
coerente (lava azul, rocha, vegetação, cânions). Uso:
`python tools/render_map.py eden01.map saida.png`.

**Status:** ✅ pipeline de terreno completa e funcional (decodificar + renderizar
qualquer um dos 54 mapas com gráficos originais). Isto cobre o essencial para o
motor desenhar o mundo.

---

## 4.4. Planilhas de dados `.txt` — DECODIFICADAS ✅ (2026-09-29)

Os 10 `.txt` do `SHEETS.VOL` (LZHUF) são as tabelas de dados/balanceamento do
jogo. Parser em [tools/sheets.py](tools/sheets.py). Dois formatos:

**Tabelas TSV** (1–2 linhas de cabeçalho + linhas separadas por TAB):

| Arquivo | Registros | Conteúdo |
|---------|:---------:|----------|
| `building.txt` | 42 | estruturas (custo, energia, HP, tamanho…) |
| `vehicles.txt` | 17 | veículos (HP, armadura, movimento, arma…) |
| `weapons.txt` | 29 | armas **e desastres** (EARTHQUAKE, METEOR… dão dano) |
| `mines.txt` | 9 | rendimento de minas (High/Med/Low × variante) |
| `morale.txt` | 72 | moral (estados + modificadores por evento; multi-seção) |
| `space.txt` | 23 | objetos da corrida espacial |

**Árvores de tecnologia** (blocos `BEGIN_TECH "Nome" <id> … END_TECH`):
`edentek` (135), `ply_tek` (137), `multitek` (137), `tutortek` (95) = **504 techs**.
Propriedades: CATEGORY, REQUIRES, COST, DESCRIPTION, TEASER, MAX_SCIENTISTS,
LAB, UNIT_PROP, EDEN_COST/PLYMOUTH_COST.

**Mecanismo confirmado no disassembly** (diretiva: sempre ir ao código):
o jogo lê linha a linha (512B), para em `0x1A` (Ctrl-Z) ou fim do stream, e
acessa registros **por nome** (não por contagem) — por isso o resíduo de padding
do LZHUF é inofensivo. Campos são separados por TAB; techs usam keywords com
comparação case-insensitive. Uso: `python tools/sheets.py`.

---

## 4.5. Sprites `op2_art.prt` + `OP2_ART.BMP` — DECODIFICADOS ✅ (2026-09-30)

Sistema de sprites do jogo (estruturas, unidades, UI). Parser em
[tools/artfile.py](tools/artfile.py), validado visualmente (estruturas da colônia
renderizam com cores corretas). Carregado no código por `sub_404360("op2_art")`.

- **`OP2_ART.BMP`** (raiz): atlas de pixels 8-bit, 2732×2731, sem compressão.
- **`op2_art.prt`** (dentro do `MAPS.VOL`): metadados. Estrutura:
```
"CPAL" u32 numPalettes(=9)
numPalettes x "PPAL" { "head"{u32=1}; "data"{ RGB[256]: r,g,b,flag } }
u32 numImages(=5390)
ImageMeta[numImages] (20 bytes cada):
   u32 scanLineByteWidth  (largura em bytes, múltiplo de 4)
   u32 pixelDataOffset    (offset nos pixels do OP2_ART.BMP)
   u32 height
   u32 width
   u16 type
   u16 paletteIndex
(depois: definições de animação/grupos de sprites — TODO)
```
As imagens são pixels 8-bit indexados, top-down, **empacotados sequencialmente**
no atlas: `offset[i+1] = offset[i] + scanLineByteWidth[i]*height[i]` (verificado).
A paleta é R,G,B (confirmado no loader: `byte0→vermelho` no empacotamento RGB565).
As **9 paletas são independentes** (não variações de cor de jogador) — cada imagem
declara a sua em `paletteIndex`; **renderizar sempre com a paleta própria da
imagem** (`m.pal`). Cor de jogador no OP2 é remapeamento em runtime, não uma das 9.

**Status:** ✅ **5390 imagens** extraíveis do atlas com paleta correta (estruturas,
unidades e UI renderizam certo). Uso: `python tools/artfile.py`.

**Animações** — parser em [tools/artanim.py](tools/artanim.py), validado por
**consumo exato** (contagens batem com o header) e por render (frames coerentes).
Estrutura (após o array de ImageMeta), confirmada no disassembly (`sub_404360`):
```
u32 numAnimations(2079); u32 numFrames(24185); u32 numPictures(160922); u32 numFrameExtra(6012)
Animation[numAnimations]:
   u32 header[9]  (header[8]=numFrames desta anim; header[3..6] ~ bounding box)
   Frame[numFrames]:
      u8 b1 -> numLayers = b1 & 0x7F ; flag1 = b1 & 0x80
      u8 b2 -> flag2 = b2 & 0x80
      if flag1: u16   ; if flag2: u16   (offset sombra/velocidade)
      Picture[numLayers] (uma "layer" do frame):
         u16 imageIndex ; u8 flag ; u8 slot(ordem) ; i16 x ; i16 y
   u32 extraCount ; ExtraRecord[extraCount] (4 x u32)
```
✅ **2079 animações, 24185 frames, 160922 layers** parseados; cada frame é uma
pilha de imagens do atlas posicionadas por (x,y), desenhadas na ordem `slot` e com
a paleta própria de cada imagem. Uso: `python tools/artanim.py`.

> A tabela final (~12KB, offset 0x178E56) **não é lida pelo loader de arte**: o
> disassembly mostra `sub_404360` liberando o stream e retornando logo após o loop
> de animações (`loc_404C49`). É dado vestigial ou de outro subsistema — registros
> ~8 bytes referenciando as últimas imagens do atlas em posições de grade.

---

## 5. Build

**Toolchain:** CMake ≥ 3.15 + MSVC (Visual Studio), C++17, SDL2 2.30.9 (em
`third_party/`). Detecta x86/x64 automaticamente.

```powershell
# A partir da raiz do projeto:
cmake -B build -A Win32          # (ou -A x64)
cmake --build build --config Release
# Saída: build/Release/outpost2_modern.exe
```

O `CMakeLists.txt` copia automaticamente para junto do executável:
`menu_screen.bmp`, `menu_dialog.json`, `SDL2.dll` e **`OP2SHRES.DLL`** (a DLL de
recursos que o port lê em runtime).

**Executar:**
```powershell
./build/Release/outpost2_modern.exe
# ou gerar screenshot do menu:
./build/Release/outpost2_modern.exe --screenshot menu.bmp
```

> ⚠️ O executável precisa do `OP2SHRES.DLL` original ao lado para carregar a
> arte/diálogos do menu.

---

## 6. Estado atual — resumo

| Área | Status |
|------|--------|
| Infra SDL2 (janela, loop, input, scaling) | ✅ Funcional |
| Leitor de recursos PE (bitmaps + diálogos) | ✅ Funcional |
| Decoder DIB / RLE8 | ✅ Funcional |
| Renderer 640×480 + widgets estilo OP2 | ✅ Funcional |
| Menu principal (shell) com arte original | ✅ Funcional |
| Sub-diálogos (Campaign/Colony/Tutorials/Prefs/About) | 🟡 Placeholder/mock |
| Disassembly de referência (IDA) | ✅ Disponível (não convertido) |
| **Motor de jogo (RTS, simulação, mapas, IA, unidades)** | ❌ Não iniciado |
| Leitura de `.VOL`/LZHUF no port (`ResManager`) | ✅ Funcional (byte-exato) |
| `GameMap` + render de terreno no port (in-engine) | ✅ Funcional (pixel-exato) |
| Backbone `TApp`/`UIState` (loop + troca shell↔jogo) | ✅ Funcional |
| Núcleo de simulação `TethysGame` (clock + RNG determinístico) | ✅ Funcional (RNG bit-exato) |
| `_Player` (modelo de jogador/economia + API) | ✅ Funcional (offsets reais) |
| `Unit` (unidades) + update per-tick da economia | ⬜ Próximo passo |
| Áudio / música / vídeos | ❌ Não iniciado |
| Multiplayer / rede | ❌ Não iniciado |

**Em uma frase:** temos um **shell/menu recriado com fidelidade** rodando sobre
uma base SDL2 sólida que lê os assets reais de 1997, mais o **disassembly
completo do EXE** como mapa para o próximo grande passo — que é reimplementar o
motor do jogo em si.

---

## 7. Próximos passos sugeridos

1. **Tornar o menu 100% funcional** — hoje os sub-diálogos são mock; ligá-los aos
   dados reais (listas de missões vêm das DLLs `E##`/`P##`/`T##` e dos `.VOL`).
2. **Parser de `.VOL`** — os containers `MAPS.VOL`, `SOUND.VOL`, `VOICES.VOL`,
   `STORY.VOL`, `SHEETS.VOL` guardam mapas, áudio e tabelas de dados do jogo
   (`inspect_vol.py` já é um começo).
3. **Estruturas de dados do jogo** — usar o `.map` (30k símbolos) + o `.asm` para
   mapear as principais `sub_*` do motor (init, game loop, unidades, economia).
4. **Formato de mapa e renderização de terreno** — o passo que abre caminho para
   a simulação de fato.
5. **Áudio via SDL2_mixer** e reprodução de vídeos.

---

## 8. Notas técnicas úteis

- **Imagebase do EXE original:** `0x400000`. Compilado com Visual C++, timestamp
  Aug 09 1997. PDB original: `C:\Tethys\release\Outpost2.pdb` ("Tethys" era o
  codinome/mundo do jogo).
- **Seções do EXE:** `.text` 842 KB de código, mais `.rdata/.data/.rsrc/.reloc`.
- **`.i64`** é a database do IDA Pro — abra nele para navegar o disassembly com os
  nomes/comentários já atribuídos, em vez de reprocessar o `.asm` cru.
- O port **não modifica** nenhum arquivo original de 1997; apenas os lê.
```
