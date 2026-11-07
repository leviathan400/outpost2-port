# Outpost 2: Divided Destiny — Port Moderno (C++17 + SDL2)

Reimplementação de *Outpost 2: Divided Destiny* (Sierra On-Line / Dynamix, 1997)
em **C++17 + SDL2**, reconstruída por engenharia reversa a partir do
disassembly do executável original.

**O jogo roda de ponta a ponta**: as campanhas das duas facções, os jogos de
colônia, os 14 tutoriais e partidas multijogador em rede para até 6 jogadores.
As 94 DLLs de missão originais são carregadas e **executadas de verdade**, por
um carregador PE próprio com pontes de ABI para os 222 símbolos que elas
importam do `OUTPOST2.EXE`.

> **Diretiva do projeto:** todo mecanismo é extraído do **código original**
> (disassembly e binários) — nada é adivinhado. Onde o original tem um defeito
> identificado, o porte **corrige em vez de reproduzir**.

Veja [`PORT_STATUS.md`](PORT_STATUS.md) para o estado detalhado: o que está
pronto, o que falta, e o mecanismo original por trás de cada parte.

---

## ⚠️ Nada do jogo de 1997 está aqui

Este repositório contém **apenas o código do porte**. Os arquivos do jogo
(`OUTPOST2.EXE`, as `*.DLL`, os `*.VOL`, `OP2_ART.BMP`, os `*.ANI`,
`OUTPOST2.INI`, `OUTPOST2.HLP`…) e o disassembly são material comercial
protegido e **não** são versionados nem redistribuídos.

O porte **lê** esses arquivos de onde eles já estão e não modifica nenhum. Para
jogar é preciso ter uma cópia legítima do jogo instalada.

---

## Requisitos

| | | |
|---|---|---|
| Sistema | **Windows** 10 ou 11 | o porte usa a API do Windows (carregador PE, Winsock, GDI) |
| Arquitetura | **Win32 / x86 — obrigatório** | nem preferência nem padrão: veja o porquê abaixo |
| Compilador | MSVC com C++17 (Visual Studio **2019 ou mais novo**) | o código usa `__fastcall` e o ABI da Microsoft; MinGW/Clang não foram testados |
| CMake | **3.15 ou mais novo** | testado também com o CMake 4.x |
| SDL2 | pacote de desenvolvimento **2.30.9 para VC** | a única dependência externa do executável |
| Jogo original | **obrigatório**, instalado | os assets de 1997 são lidos do disco |
| Python 3 | opcional | só para os scripts de `tools/` |

Não há vcpkg, Conan nem submódulos. `ws2_32` e `ole32` (rede) e `gdi32`
(fontes) são resolvidos pelo próprio MSVC — nada a instalar por eles.

### Por que o build x86 é requisito

O porte carrega as DLLs de missão originais e **executa o código de 32 bits
delas dentro do próprio processo** (`src/mission/pe_loader.cpp`), com pontes de
API na convenção `__fastcall` do x86 (`src/mission/op2_api.cpp`). Em um
processo de 64 bits isso é impossível: um build x64 **compila** e só falha ao
abrir a primeira missão. Para a armadilha não chegar até lá, o `CMakeLists.txt`
para na configuração se a arquitetura não for 32 bits.

---

## Instalando as dependências

### 1. Compilador (MSVC)

Instale o **Visual Studio Community** (gratuito, 2022 ou mais novo) —
<https://visualstudio.microsoft.com/downloads/> — e, no instalador, marque o
workload **"Desenvolvimento para desktop com C++"** (*Desktop development with
C++*). Ele já traz tudo o que o porte precisa: o compilador MSVC, o Windows
SDK e o **suporte a x86**, que é o alvo usado aqui.

Quem não quer a IDE pode instalar só as ferramentas de linha de comando
(*Build Tools for Visual Studio*, mesma página). Pelo winget:

```powershell
winget install --id Microsoft.VisualStudio.2022.BuildTools --override "--quiet --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

### 2. CMake

O workload de C++ do Visual Studio inclui um CMake. Para ter o comando `cmake`
disponível em qualquer terminal, instale a versão avulsa:

```powershell
winget install --id Kitware.CMake
```

Ou baixe de <https://cmake.org/download/> marcando *"Add CMake to the system
PATH"*. Confira com `cmake --version` (precisa ser 3.15+).

### 3. SDL2

Baixe **`SDL2-devel-2.30.9-VC.zip`** — o pacote *de desenvolvimento* para
Visual C++, não o de runtime — em
<https://github.com/libsdl-org/SDL/releases/tag/release-2.30.9>.

Extraia dentro do repositório de modo que a árvore fique **exatamente** assim:

```
third_party/
  SDL2-2.30.9/
    include/          SDL.h, SDL_render.h, ...
    lib/
      x86/            SDL2.lib, SDL2main.lib, SDL2.dll   <- as que o build usa
      x64/            (vem no pacote; não é usada)
```

Esse caminho é o `SDL2_ROOT` do `CMakeLists.txt`. Se preferir o SDL2 em outro
lugar, mude essa variável lá. A pasta `third_party/` não é versionada — é por
isso que este passo é manual.

No PowerShell, do diretório do repositório:

```powershell
mkdir third_party
Expand-Archive -Path "$HOME/Downloads/SDL2-devel-2.30.9-VC.zip" -DestinationPath third_party
```

### 4. Python, só se for usar os `tools/`

Os 20 scripts de engenharia reversa em `tools/` pedem **Python 3**. A maioria
usa só a biblioteca padrão; nove deles precisam de dois pacotes:

```powershell
pip install pefile capstone
```

`pefile` (ler recursos e exports de PE) é usado por `analyze_shell.py`,
`check_dialog_fmt.py`, `disasm_menu.py`, `dump_dialogs.py`, `extract_bmps.py`,
`extract_menu.py`, `inspect_bmps.py`, `inspect_table.py` e `parse_dlg.py`;
`capstone` (desassemblador) pelos dois primeiros. **Nada disso é necessário
para compilar ou jogar.**

---

## Build

Com as dependências no lugar, do diretório do repositório:

```powershell
cmake -B build -S . -A Win32
cmake --build build --config Release
```

O `-A Win32` não é opcional. Se for omitido em uma máquina de 64 bits, a
configuração para com uma mensagem dizendo o que fazer.

O passo de pós-build copia `outpost2_modern.exe` e `SDL2.dll` para a raiz do
projeto. **O executável tem de rodar no diretório de instalação do jogo** — é
de lá que ele lê os `.VOL`, as DLLs de missão e os recursos originais. O jeito
mais simples é clonar o repositório **dentro da pasta do jogo**: o `.gitignore`
é uma lista branca, feita exatamente para isso, e ignora todo o material
original que convive com os fontes.

### Se der errado

| Sintoma | Causa |
|---|---|
| `Outpost2Port precisa ser gerado para Win32 (x86)` | faltou o `-A Win32` |
| `SDL.h` não encontrado, ou `SDL2.lib` inexistente | o SDL2 não está em `third_party/SDL2-2.30.9/` (confira o nome da pasta) |
| `MSB3073` / *"Permission denied"* ao copiar o exe | o jogo está aberto; feche e refaça o build |
| compila, mas toda missão falha ao abrir | o build saiu x64 — apague `build/` e configure de novo com `-A Win32` |

> Filtrar a saída do build só por `"error C"` **esconde** a falha de cópia do
> pós-build. Filtre por `error|denied`.

---

## Rodando

```powershell
outpost2_modern.exe                      # menu principal
outpost2_modern.exe --game eden01.map    # abre um mapa direto
```

O arquivo `OUTPOST2.INI` do jogo é respeitado. O porte acrescenta cinco chaves
que o original não tem — nível da IA, escuridão da noite, volume, câmera do
demo e tela cheia; estão documentadas no `PORT_STATUS.md`.

Há **93 modos de linha de comando** para testes e capturas, entre eles uma
bateria de regressão (`--savetest` prova o determinismo de salvar/recarregar).
A lista está no `PORT_STATUS.md`.

---

## Estrutura

```
src/
  main.cpp        entrada + os 93 modos de teste/captura
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
```

São 45.200 linhas de C++17. Os formatos de 1997 estão todos decodificados e
documentados no `PORT_STATUS.md`: o container `.VOL` (com o LZHUF), o `.map`,
os tilesets PBMP, os sprites do `op2_art`, as planilhas de dados e os
cursores `.ANI`.

---

## Licença

O código deste porte está sob a licença **MIT** (veja [`LICENSE`](LICENSE)).

A licença cobre **só o código do porte**. Ela não alcança nada de *Outpost 2:
Divided Destiny* — nem o executável, nem as DLLs, nem os `.VOL`, a arte, os
sons, os mapas ou o disassembly usado como referência. Nada disso está neste
repositório, e uma cópia legítima do jogo continua sendo requisito para rodar.

## Créditos

*Outpost 2: Divided Destiny* é de Sierra On-Line / Dynamix. Este é um trabalho
independente de preservação e reimplementação, sem relação com os detentores
dos direitos.

Os projetos **OPUPatch** e **TethysAPI**, da comunidade, foram usados como
referência de documentação — para conferir nomes de classes, offsets e a lista
de defeitos conhecidos do original. **Nenhum código deles foi copiado para
cá**, por serem de licença distinta.

Reverse Enginnering done by Claude Code!
