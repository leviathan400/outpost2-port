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

| | |
|---|---|
| Jogo original | **obrigatório**, instalado (os assets são lidos do disco) |
| Compilador | MSVC com C++17 (Visual Studio 2019 ou mais novo) |
| Arquitetura | **Win32 (x86) obrigatório** — veja abaixo |
| SDL2 | pacote de desenvolvimento **2.30.9** para VC |

### O build x86 não é preferência, é requisito

O porte executa o código de 32 bits das DLLs de missão originais dentro do
próprio processo (`src/mission/pe_loader.cpp`), e as pontes de API usam a
convenção `__fastcall` do x86 (`src/mission/op2_api.cpp`). Em um processo de 64
bits isso é impossível. Um build x64 compila e depois falha ao abrir qualquer
missão — por isso o CMake agora recusa gerar fora de Win32.

### SDL2

Baixe `SDL2-devel-2.30.9-VC.zip` em
<https://github.com/libsdl-org/SDL/releases/tag/release-2.30.9> e extraia de
modo que o caminho fique:

```
third_party/SDL2-2.30.9/
  include/
  lib/x86/
```

É o caminho que o `CMakeLists.txt` espera (`SDL2_ROOT`). A pasta
`third_party/` não é versionada.

---

## Build

```powershell
cmake -B build -S . -A Win32
cmake --build build --config Release
```

O passo de pós-build copia `outpost2_modern.exe` e `SDL2.dll` para a raiz do
projeto. **O executável tem de rodar no diretório de instalação do jogo** — é
de lá que ele lê os `.VOL`, as DLLs de missão e os recursos originais. O jeito
mais simples é clonar o repositório dentro da pasta do jogo: o `.gitignore` é
uma lista branca, feita exatamente para isso, e ignora todo o material original
que convive com os fontes.

> Se a cópia do pós-build falhar com *"Permission denied"*, o jogo está aberto.
> Feche e refaça. Filtrar a saída do build só por `"error C"` **esconde** essa
> falha; filtre por `error|denied`.

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

## Créditos

*Outpost 2: Divided Destiny* é de Sierra On-Line / Dynamix. Este é um trabalho
independente de preservação e reimplementação, sem relação com os detentores
dos direitos.

Os projetos **OPUPatch** e **TethysAPI**, da comunidade, foram usados como
referência de documentação — para conferir nomes de classes, offsets e a lista
de defeitos conhecidos do original. **Nenhum código deles foi copiado para
cá**, por serem de licença distinta.
