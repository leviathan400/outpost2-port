# Outpost 2: Divided Destiny — Port Moderno (C++17/SDL2)

> Estado do port. Última revisão: **2026-10-28** (163 commits), feita lendo o
> código e rodando os testes, não a documentação anterior — que estava de
> 2026-09-29 e dizia que o port "ainda não reimplementa o jogo em si".

---

## 1. O que isto é hoje

Um porte **jogável** de *Outpost 2: Divided Destiny* (Sierra/Dynamix, 1997)
para C++17 + SDL2. O jogo roda: campanha das duas facções, jogos de colônia,
os 14 tutoriais e partidas de multijogador em rede.

O mecanismo **sai do disassembly** (`Outpost2_src/OUTPOST2.asm`, IDA) e dos
recursos das DLLs originais — nada é inventado. Quando o original tem um
defeito identificado, o porte **corrige em vez de reproduzir**.

Os arquivos de 1997 não são modificados nem redistribuídos: o porte os **lê**
(`OP2SHRES.DLL`, `OUT2RES.DLL`, os `.VOL`, `OP2_ART.BMP`, as DLLs de missão).
As missões originais rodam **de verdade**, carregadas por um carregador PE
próprio com pontes de ABI para a API que elas importam do `OUTPOST2.EXE`.

### Números de hoje

| | |
|---|---|
| Código do porte | **41.500 linhas** de C++17 em `src/` |
| Simulação (`src/game`, 38 .cpp) | 20.500 linhas |
| UI in-game (`src/app`, 20 .cpp) | 7.300 linhas |
| Shell/menus (`src/shell`) | 3.300 linhas |
| Rede (`src/net`) | 1.400 linhas |
| DLLs de missão que carregam e rodam | **94 de 94** |
| Símbolos da API do EXE implementados | **222 de 222** (os que as 94 DLLs importam) |
| Modos de teste por linha de comando | 81 |

As duas linhas de missões são a medida honesta de cobertura: toda DLL de
missão do disco carrega, cria as unidades e roda, e nenhuma importa um símbolo
que o porte não tenha. Para refazer a conta: varredura com `--missiontest` em
cada DLL, e a cobertura da API comparando as tabelas de importação das DLLs
com o registro de `src/mission/op2_api.cpp`.

---

## 2. Estrutura

```
src/
  main.cpp        entrada + os 81 modos de teste/captura
  app/            TApp, UIState, GameState (o jogo), ShellState, HUD,
                  as 6 abas do quadro, minimapa, globo, diálogos
  game/           a simulação: unidades, estruturas, movimento, pathfinder,
                  combate, economia, pesquisa, desastres, triggers, IA
  gfx/            framebuffer 640x480, PBMP, sprites, fontes (bitmap e GDI)
  res/            leitor de PE (.rsrc + exports), .VOL + LZHUF, ResManager
  core/           OUTPOST2.INI (CConfig)
  audio/          mixer SDL fiel ao DirectSound do original, música (.clm)
  mission/        carregador PE das DLLs de missão + ponte da API
  net/            lobby (descoberta/entrada) e partida em lockstep
tools/            scripts Python de engenharia reversa (formatos, dumps)
Outpost2_src/     disassembly de referência (NÃO versionado)
```

---

## 3. O que está portado

### Simulação
- **Relógio e RNG** determinísticos, bit-exatos com o original.
- **Unidades**: pool, criação, desenho fiel (sprites `op2_art` com paleta e
  cor de jogador), sombras, nº de série.
- **Movimento e pathfinder** portados da arquitetura real (rotas, filas,
  bloqueios) — nunca um A* inventado.
- **Estruturas**: construção, ligar/desligar, dano, destruição, entulho,
  desmonte, tomada, reparo, animações de ambiente.
- **Economia da colônia** completa: rede de tubos, energia, trabalhadores,
  cientistas, **comida, população e moral** (incluindo a moral forçada pelas
  missões).
- **Mineração**: Robo-Miner/GeoCon, `MINES.TXT`, caminhões, docas, smelters.
- **Obras de terreno**: tubos, muros, Robo-Dozer, varredura de região.
- **Combate**: todas as armas de tanque e de torre, EMP/espuma/ESG/ácido/
  Thor/Scorpion, autodestruição, dano em área com armadura, paredes.
- **Pesquisa**: as quatro árvores, laboratórios, cientistas, propagação.
- **Desastres**: meteoro, raio, tornado, terremoto, erupção/lava e a
  **praga (micróbio)**, com as três fases de aviso do original.
- **Triggers**: os 16 `Create*Trigger`, vitória/derrota, objetivos.
- **IA**: grupos (Sc/Fight/Build/Mine), o padrão do computador e o Pinwheel.
- **Salvar/carregar** (`SGAME%d.OP2`), com teste de determinismo.

### Interface
- **Shell**: menu principal, campanha (briefing/objetivos/capítulos do
  `STORY.VOL`), jogos de colônia com placar, tutoriais, preferências, sobre.
- **In-game**: painel direito, quadro da unidade, botoeira 3x3, botões de
  prédio, linha de ajuda, as 6 abas de relatório, minimapa com zoom, visão
  tática e globo, ciclo dia/noite.
- **Controles** no padrão Windows: foco por teclado, combos com lista
  suspensa, barras de rolagem.

### Áudio
Mixer SDL fiel ao do original (volume e pan por tile, fila de vozes), música
`.clm` e **volume mestre** próprio do porte.

### Manual (ajuda) e demo de atração
O botão HELP e o F1 abrem o **OUTPOST2.HLP** — "Outpost 2 Online Manual", 145
tópicos — numa tela do porte, porque o WinHelp que o original chamava não
existe mais no Windows. O arquivo é lido onde está (`src/res/winhelp.*`), não
convertido nem copiado; sem ele, o HELP procura um manual em PDF ao lado do
jogo e o entrega ao leitor do sistema.

Um minuto parado no menu principal dispara o **demo de atração**, como no
original: `ademo{1,2,3}.dll` em rodízio pelo contador `[Game] Demo` do INI
(que é um contador, não um liga-desliga), com desastres e dia/noite ligados.
Qualquer tecla ou clique interrompe. Dois acréscimos deste porte deixam o demo
assistível: a **câmera passeia** pelos pontos de ação (no original ela fica
parada e boa parte da cena acontece fora da tela) e o **inimigo aparece no
escuro** — só no demo; numa partida a cegueira noturna continua valendo.

### Multijogador
Partida em **rede, em lockstep bit-exato**, com lobby sincronizado
(descoberta por broadcast, entrada, chat, EJECT, opções), as telas do
`OUT2RES.DLL` e as 51 missões M*.

### Acréscimos do porte (não existem no original)
Três chaves no `[Game]` do `OUTPOST2.INI`, com controle na tela de
preferências; o original ignora chaves que não conhece.

| Chave | Faixa | Padrão | O que faz |
|---|---|---|---|
| `AILevel` | 0..3 | 0 (AUTO) | Força o nível da IA nas campanhas e colônias. AUTO = o comportamento do original. **Não se aplica em rede**, onde a dificuldade é por jogador e vem do lobby. |
| `NightDarkness` | 0..100 | 75 | Quanto da curva de escurecimento noturno se aplica (100 = o original, que leva a noite plena a ~15% do brilho e faz as unidades, que nunca escurecem, recortarem contra o preto). |
| `AudioVolume` | 0..100 | 50 | Volume mestre sobre a mistura final (sons e música). O original só tem `MusicVol`/`SoundVol`, por canal e em 6 degraus. |
| `DemoCamera` | 0/1 | 1 | No demo de atração a câmera passeia pelos pontos de ação (desastre, combate, unidades). 0 devolve a câmera parada do original. |
| `Fullscreen` | 0/1 | 1 | Abre em tela cheia (sem troca de modo de vídeo; a vista de 640×480 escalada com a proporção mantida). Alt+Enter alterna a qualquer momento. |

---

## 4. O que FALTA portar

Revisado item a item em 2026-10-28, conferindo no código.

### Grande

1. **Rede com mais de 2 jogadores.** O lockstep (anel de 16 turnos) e o lobby
   já tratam a mesa inteira, mas o transporte é ponto a ponto: a contagem está
   fixa em 2 em `app/shell_state.cpp` e em `main.cpp`. Para 3–6 jogadores são
   precisas N conexões (ou retransmissão pelo host) e roteamento dos turnos por
   jogador. Mapas e GAME SETTINGS já suportam 6.

### Multijogador, peças soltas

2. **TRANSFER** — doar as unidades e abandonar a partida. É o **único** comando
   de veículo que ainda cai em `NOT_PORTED`; falta o diálogo de escolha de
   jogador (`sub_45D020`).
3. **Trade Center** — o prédio existe e conta na estatística, mas não há a UI
   de troca de recursos.
4. **Modo Chat** da aba Communications (`[+0x1C4]` = 1), que só existe em
   multijogador.
5. **Estatísticas de fim de partida** multijogador (motivos 7..9 da mensagem
   0x73e) e as matrizes de baixas por par de jogadores.

### Miudezas de simulação (todas marcadas no código)

6. Diálogo de confirmação do IDLE quando o prédio guarda metal ("All metal in
   storage will be lost...").
7. ~~O painel STORAGE~~ — **não é pendência**: conferido em 2026-10-28, a
   regra do original (`loc_456560`) habilita o comando para 0x19, 0x1A, 0x27,
   0x29, 0x2F e 0x36–0x38, que são exatamente os oito tipos que o painel
   cobre. A mensagem de "ainda não portado" é só uma guarda inalcançável.
8. Ordem "atirar no chão" (bit 0x2000, `sub_4A1BC0`).
9. Estatísticas 0x1C4/0x1CC por jogador (baixas infligidas/sofridas).
10. Revelação do mapa ao construir (`dword_574C88`) e o som de `sub_42F000`.
11. Pacote 0x1D (tomar posse) para os grupos de IA.
12. Modo 3 do pathfinder (`sub_438850`).
13. ~~Fila de waypoints com Shift~~ — **não é pendência**: conferido em
    2026-10-29, a fila `unk_5471B0`/`dword_5471AC` é código morto no
    executável. O contador é o campo +0x3C do objeto do modo de mira, que
    `sub_41B550` zera ao entrar no modo e que ninguém incrementa — as três
    funções que o leem só leem. O jogo publicado manda sempre UM ponto por
    comando, que é o que o porte faz.
14. Dano em paredes nas autodestruições (`sub_4A23F0` no caminho de
    `sub_4A4090`).

### Não aplicável nesta instalação

15. **Filmes (AVI).** O shell original toca `I.AVI`/`IE`/`IP` na abertura,
    `E4.AVI`, `MEF`/`MPF` e as oito variantes de fim de campanha
    (`MEBL`/`MEBW`/`MEUL`/`MEUW`/`MPBL`/`MPBW`/`MPUL`/`MPUW`) e as quatro de fim
    de colônia (`CES`/`CEP`/`CPS`/`CPP`). **Nenhum `.AVI` existe no disco** —
    ficavam no CD. Portar o player não teria o que tocar.

### Decisões deliberadas (não são pendências)

- **A tela 259 "PICK CONNECTION METHOD" não existe aqui.** Dos seis meios da
  tabela `unk_4DF370` só o TCP/IP sobreviveu — o SIGS era o serviço da Sierra,
  o IPX saiu do Windows, modem e serial eram DirectPlay sobre linha discada.
  O MULTIPLAYER abre direto o Dialog 260. Por isso `MpNetOptions` é a primeira
  tela de multijogador, e os testes `m_state >= MpNetOptions` dependem da ordem
  do enum.

---

## 5. Ferramentas de análise (`tools/`)

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

## 5.1. Formato `.VOL` — DECODIFICADO ✅ (2026-09-29)

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

## 5.2. Formato `.map` — DECODIFICADO ✅ (2026-09-29)

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

## 5.3. Tilesets `PBMP` + render de terreno — DECODIFICADO ✅ (2026-09-29)

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

## 5.4. Planilhas de dados `.txt` — DECODIFICADAS ✅ (2026-09-29)

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

## 5.5. Sprites `op2_art.prt` + `OP2_ART.BMP` — DECODIFICADOS ✅ (2026-09-30)

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

---

## 6. Build e execução

**Toolchain:** CMake >= 3.15 + MSVC, C++17, SDL2 2.30.9 (em `third_party/`).
O build é **Win32 (x86)** — o carregador PE das missões executa código de 32
bits das DLLs originais, então x64 não serve.

```powershell
cmake -B build -S . -A Win32
cmake --build build --config Release
```

O passo de pós-build copia `outpost2_modern.exe` e `SDL2.dll` para a raiz do
jogo, que é de onde o executável tem de rodar (ele lê os `.VOL`, as DLLs de
missão e os recursos originais do diretório de trabalho).

> Se a cópia falhar com "Permission denied", o jogo está aberto — feche-o e
> refaça o build. Filtrar a saída do build por `"error C"` **esconde** essa
> falha; filtre por `error|denied`.

### Testes por linha de comando

São 81 modos. Os que cobrem regressão de verdade:

| Modo | O que prova |
|---|---|
| `--savetest <DLL> <ticks> <mais>` | determinismo: salvar, recarregar e continuar tem de dar o mesmo hash |
| `--mplan host\|join <DLL> <ticks>` | lockstep em rede: as duas máquinas fecham com o mesmo hash |
| `--mptest <DLL>` | missão multijogador local |
| `--mpui host\|join` | o fluxo das telas de rede, pelo despacho de verdade dos botões |
| `--uitest` | combos e barras de rolagem, pelo caminho do mouse e do teclado |
| `--missiontest <DLL> <saida> <ticks>` | carrega e roda qualquer missão |
| `--aitest <DLL campanha> <DLL multi>` | o `AILevel` chega ao jogador 0 e o multijogador fica intacto |
| `--helptest [tópico]` | o leitor do `.HLP` e a navegação da tela do manual |
| `--demotest` | o rodízio dos `ademo`, o `GameStartInfo` do demo e a interrupção por tecla |
| `--salvagetest`, `--oretest`, `--earthtest`, `--combattest`, `--researchtest`, `--garagetest`, `--gptest`, `--pinwheeltest` | cada subsistema |
| `--screenshot` + `--menu`/`--prefs`/`--mpsettings`/`--panetest`/... | captura de telas para conferência visual |

---

## 7. Resumo por área

| Área | Estado |
|---|---|
| Infra (SDL2, framebuffer, entrada, escala) | Completo |
| Leitura dos originais (PE, `.VOL`/LZHUF, PBMP, sprites, planilhas) | Completo |
| Terreno e mapa | Completo (render pixel-idêntico ao original) |
| Unidades, movimento, pathfinder | Completo |
| Estruturas e construção | Completo |
| Economia, comida, população, moral | Completo |
| Combate | Completo (falta só o dano em paredes nas autodestruições) |
| Pesquisa | Completo |
| Desastres, lava e praga | Completo |
| Triggers, objetivos, vitória/derrota | Completo |
| IA (grupos, computador padrão, Pinwheel) | Completo |
| Campanha, colônia, tutoriais | Completo |
| Shell e telas | Completo |
| Manual (OUTPOST2.HLP) e demo de atração | Completo |
| HUD, abas, minimapa, globo | Completo |
| Áudio e música | Completo |
| Salvar/carregar | Completo |
| Multijogador em rede | Joga, **mas só a dois**; faltam chat in-game, troca e estatísticas |
| Filmes | Não aplicável (arquivos não existem) |

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
