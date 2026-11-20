[English](PORT_STATUS.EN.md) · **Português (BR)**

# Outpost 2: Divided Destiny — Port Moderno (C++17/SDL2)

> Estado do port. Última revisão: **2026-11-05** (199 commits), feita lendo o
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
| Código do porte | **45.200 linhas** de C++17 em `src/` |
| Simulação (`src/game`, 40 .cpp) | 21.400 linhas |
| UI in-game (`src/app`, 20 .cpp) | 8.100 linhas |
| Shell/menus (`src/shell`) | 3.900 linhas |
| Rede (`src/net`) | 1.400 linhas |
| DLLs de missão que carregam e rodam | **94 de 94** |
| Símbolos da API do EXE implementados | **222 de 222** (os que as 94 DLLs importam) |
| Modos de linha de comando (testes e telas) | 96 |

As duas linhas de missões são a medida honesta de cobertura: toda DLL de
missão do disco carrega, cria as unidades e roda, e nenhuma importa um símbolo
que o porte não tenha. Para refazer a conta: varredura com `--missiontest` em
cada DLL, e a cobertura da API comparando as tabelas de importação das DLLs
com o registro de `src/mission/op2_api.cpp`.

---

## 2. Estrutura

```
src/
  main.cpp        entrada + os 132 modos de teste/captura
  app/            TApp, UIState, GameState (o jogo), ShellState, HUD,
                  as 6 abas do quadro, minimapa, globo, diálogos
  game/           a simulação: unidades, estruturas, movimento, pathfinder,
                  combate, economia, pesquisa, desastres, triggers, IA
  gfx/            framebuffer do tamanho da janela (base 640x480), PBMP, sprites, fontes (bitmap e GDI)
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
| `NightDarkness` | 0..100 | 100 | Quanto da curva de escurecimento noturno se aplica (100 = o original, que leva a noite plena a ~15% do brilho; fora da luz das unidades o terreno quase some — ver 5.41). Até 2026-11-19 o padrão era 75. |
| `AudioVolume` | 0..100 | 50 | Volume mestre sobre a mistura final (sons e música). O original só tem `MusicVol`/`SoundVol`, por canal e em 6 degraus. |
| `DemoCamera` | 0/1 | 0 | No demo de atração a câmera passeia pelos pontos de ação (desastre, combate, unidades). 0 = a câmera parada do original (o padrão desde a 5.45; era 1). |
| `Fullscreen` | 0/1 | 1 | Abre em tela cheia (sem troca de modo de vídeo). Alt+Enter alterna a qualquer momento, e dentro do jogo há o par de rádio **Screen Mode** no fim da lista do `PREFERENCES` (ver 5.8). |
| `MenuSkin` | 0/1 | 0 | Estilo dos menus. 0 = o visual do próprio porte (painéis azul-escuros, menu principal à direita); 1 = a pele de diálogo do OP2 original (fundo "blueprint", molduras vermelhas, texto verde, arte do `OP2SHRES.DLL`). |
| `MapWrap` | 0/1 | 0 | Volta horizontal do mapa. 0 = como no original: a vista para nas bordas esquerda/direita do mapa, e só os mapas "around the world" (512 tiles de largura) dão a volta; 1 = todo mapa dá a volta (o comportamento antigo do porte). |
| `ShowBottomRow` | 0/1 | 0 | Última linha de tiles do mapa. 0 = como no original: a vista para no fundo do clip rect do mapa, e a última linha fica escondida; 1 = a vista pode descer até mostrá-la. |
| `AutoOreRoute` | 0/1 | 0 | Rota de minério num clique (acréscimo deste porte): com caminhões vazios selecionados, o clique da ordem (o esquerdo; o direito com `RightClickOrders`) na doca de uma mina monta o ciclo até a fundição compatível disponível mais próxima (ver 5.23). 0 = só o comando de três cliques, como no original (o padrão desde a 5.45; era 1). Também se troca pelo jogo: a caixa **Auto Ore Route** no `GAME PREFERENCES` do menu e a linha de mesmo nome no `Game Preferences` da aba Options (ver 5.29). |
| `CommandDebug` | 0/1 | 0 | Diagnóstico (acréscimo deste porte): cada comando de prédio recusado — produzir, pesquisar, treinar, o repetido que o anel descarta — vira uma linha no registro de mensagens com o motivo (`not enough workers`, `needs 400 common ore`…). 0 = mudo, como no original. Também se troca pelo jogo: a caixa **Command Debug** no `GAME PREFERENCES` do menu e a linha de mesmo nome no `Game Preferences` da aba Options (ver 5.39). |
| `ProductionQueue` | 0/1 | 0 | Fila de produção (acréscimo deste porte): cada clique num item da lista da fábrica soma +1 ao pedido (o número aparece na frente do item; clique direito tira 1), o `BUILD IT` manda o pedido para a fila e a fábrica produz um item atrás do outro; o clique que o minério não cobre não soma. 0 = um item por ordem, como no original. Também se troca pelo jogo: a caixa **Production Queue** no `GAME PREFERENCES` do menu e a linha de mesmo nome no `Game Preferences` da aba Options (ver 5.40). |
| `RightClickOrders` | 0/1 | 0 | O esquema de mouse dos RTS modernos (veio do fork): o esquerdo só seleciona e o **direito** dá a ordem (o comando padrão da 5.42). 0 = como o original: o **esquerdo** faz tudo — com seleção, executa o comando padrão quando ele aceita o alvo, senão seleciona — e o direito cancela a mira e desseleciona. Também se troca pelo jogo: a caixa **Right-Click Orders** no `GAME PREFERENCES` do menu e a linha de mesmo nome na aba Options (ver 5.44). |
| `ListScrollBars` | 0/1 | 0 | As listas da partida (relatórios, opções, objetivos, pesquisa, produção, veículos) ganham uma barra de rolagem arrastável no lugar das setas de página, e a roda do mouse rola a lista sob o cursor (acréscimo deste porte). 0 = só as setas de página, como no original. Também se troca pelo jogo: a caixa **Wheel & Scroll Bars** no `GAME PREFERENCES` e a linha de mesmo nome na aba Options (ver 5.50). |

Todas, menos `AILevel` dentro da partida, também se trocam pelas telas de
preferências — ver 5.29.

---

## 4. O que FALTA portar

**Nada.** Em 2026-11-06 a última pendência (o item 21) foi fechada. O que
sobrou abaixo é o histórico: cada item com o que era, como foi resolvido e,
nos oito casos em que a "pendência" não existia, por que o comentário mentia.

Duas coisas seguem **registradas como lacuna consciente**, e nenhuma muda o
jogo: o sinalizador `dword_4E0BD0` dos cheats (um único leitor, `sub_431EB0`,
num caminho que não identifiquei — ver 5.6) e a **API de cheat das missões**
sobre o pacote 0x2B, com os oito índices mapeados mas nenhuma das 94 missões
originais a importando.

Revisado item a item em 2026-10-28 e em 2026-11-03/06, conferindo no código.

Isso vale para o que **falta portar**, e não quer dizer que não haja defeito no
que já está: em 2026-11-06, depois do inventário, a aba Options apareceu com
dois (os itens da tela de fim de missão não faziam nada) e foi consertada —
ver 5.7.

**Uma divergência de estrutura segue ABERTA** (achada em 2026-11-06, ver 5.9):
a tela `PREFERENCES` de dentro do jogo tem no original **10 itens** com um
botão de estado por item e uma área de detalhe embaixo da lista, e o porte
mostra 13 linhas com o valor embutido. As dicas dos botões já foram
corrigidas; o resto depende de uma decisão.

### Grande

1. ~~Rede com mais de 2 jogadores~~ — **feito** em 2026-10-30. A topologia é a
   mesma da camada de mensagens do lobby: **estrela com o host no meio**. O
   host fala com cada lugar ocupado e **repassa** os turnos de cada cliente aos
   demais; o cliente fala só com o host, então ninguém precisa alcançar todo
   mundo (NAT, firewall). A máquina de turnos já era de N jogadores — só o
   `sendRaw` estava preso a um par. Medido: 3 e 4 jogadores em lockstep, com
   hash idêntico em todas as máquinas (`--mplan host|join <DLL> <ticks> <N>`).

### Multijogador, peças soltas

Todas resolvidas em **2026-11-03**, e duas delas eram leitura errada minha:

2. ~~Ações da aba Options~~ e ~~TRANSFER~~ — **feitos**. A aba virou as três
   telas do original (menu, `PREFERENCES`, `END MISSION`) com os seis botões de
   `sub_457880`. E o TRANSFER da botoeira **faz** algo: o `nullsub_84` é o
   vt[3], o clique no MAPA, que ele não usa. O vt[1] (`sub_455A80`) abre o
   escolhedor de jogador *"Transfer Vehicle(s) to:"* (`sub_46D230`), e a
   escolha monta o pacote **0x18** com as unidades selecionadas e o destino
   (`sub_455950`); o case 24 troca o dono de cada uma e avisa os dois lados.
   O botão aparece com a trapaça `dword_56DE58` ou com um **Trade Center
   operando** numa partida multijogador.
3. ~~Trade Center~~ — **feito**: não havia "UI de troca de recursos"; o que o
   prédio faz é habilitar o TRANSFER. Ele voltou à lista de produção, porque
   `sub_468120` só deixa o 0x20 passar com `dword_56DC78 != 0` — e esse
   ponteiro é a **camada de rede**, ou seja o prédio só existe em partida em
   rede.
4. ~~Modo Chat da aba Communications~~ — **feito**, e também não era o que o
   nome sugeria: o modo 1 é a tela *"Set Up Allies and Chat Partners"*, com as
   duas listas de jogadores e as caixas de chat (máscara local) e de aliança
   (pacote **0x32**). O botão `&NET STATS` abre a terceira tela
   (`sub_467760`), com os seis campos do original. A conversa em si é a
   **linha do jogo** (`sub_4115F0`): Enter abre, Enter manda o pacote **0x30**,
   Esc desiste.
5. ~~Estatísticas de fim de partida~~ — **feitas**, ver a correção do item 9.

### Miudezas de simulação (todas marcadas no código)

6. ~~Diálogo de confirmação do IDLE quando o prédio guarda metal~~ — **feito**,
   e o original avisa de menos. `sub_455CB0` (o vt[1] do botão IDLE) compara a
   capacidade **deste** prédio com a capacidade **total** do jogador (+0x18
   comum, +0x1C raro) e só pergunta quando são **iguais** — isto é, quando ele
   é o único armazém daquele metal, que é o único caso em que o texto "All
   metal in storage will be lost" é verdade. Com uma fundição (10000) e um
   armazém (5000), desligar a fundição com 15000 guardados perde 10000 **em
   silêncio** (`sub_471F30` trunca). Aqui a conta é o transbordo, então a
   pergunta cobre qualquer perda e o número entra no lugar do "All". Duas
   outras correções: o índice do bloco de stats é o nibble **alto** de `[+0x1D]`
   (`sar al, 4`), não o dono; e o ramo do metal raro do original exige
   `rareOre != 0` enquanto o do comum não, de modo que ele pergunta com o
   estoque vazio — o transbordo cuida dos dois. Teste: `--idletest <DLL>`
   (quatro casos por prédio; E10/E12/P10 cobrem a perda parcial).
7. ~~O painel STORAGE~~ — **não é pendência**: conferido em 2026-10-28, a
   regra do original (`loc_456560`) habilita o comando para 0x19, 0x1A, 0x27,
   0x29, 0x2F e 0x36–0x38, que são exatamente os oito tipos que o painel
   cobre. A mensagem de "ainda não portado" é só uma guarda inalcançável.
8. ~~Ordem "atirar no chão"~~ — **feito, e não era ordem**: o bit 0x2000 de um
   veículo só é ligado em `loc_44B8F0` de `sub_44B650` (o movimento), quando
   uma unidade **do computador** armada de Laser (0x3D) ou Microwave (0x3E)
   esbarra num tubo/parede sem ter alvo — é assim que a IA abre caminho. Não
   existe comando do jogador para isso.
9. ~~Estatísticas de baixas por jogador~~ — **feitas em 2026-11-03**, e a
   conclusão anterior ("nunca lidos") estava **errada**: o `MissionResults`
   **copia sim**. O fim de `sub_489A30` (`loc_489CF3`) leva SETE dwords de
   `[jogador+0x1BC]` para `MissionResults+0xC2 + jogador*28`, e o `OP2Shell`
   lê de lá — os motivos 7, 8 e 9 da mensagem 0x73a mandam todos `0x727`, que
   abre o **Dialog 155 "MULTIPLAYER GAME STATISTICS"**. São seis contadores
   (`+0x1BC` prédios construídos, `+0x1C0` veículos construídos, `+0x1C4`
   prédios perdidos, `+0x1C8` veículos perdidos, `+0x1CC` prédios destruídos,
   `+0x1D0` veículos destruídos); o sétimo dword é a soma dos minérios, que o
   original escreve por cima.
10. ~~Revelação do mapa ao construir~~ — **não existe**: conferido em
    2026-10-29. O trecho de `dword_574C88+0x110` converte o retângulo do prédio
    para a tela e o passa a `sub_46F550`, que é chamada de 36 pontos (entre
    eles o realce de hover) — é a lista de **retângulos sujos** da vista. O
    Outpost 2 não tem névoa de guerra. Este porte repinta a tela inteira a cada
    quadro, então não há o que portar.
11. ~~Pacote 0x1D (tomar posse) para os grupos de IA~~ — **feito** em
    2026-10-29: `orderTakeOver` em `ai_group.cpp` (porte de `sub_42A100`),
    chamado do caso 4 do `classHandler`. A Spider do computador toma veículos
    inimigos **paralisados por EMP** a até 256 px (`sub_427CC0`). Teste:
    `--takeovertest <map>`.
12. ~~Modo 3 do pathfinder~~ — **não é pendência**: conferido em 2026-11-03,
    `sub_438850` está portado em `pfMoveCost` — parede/tubo bloqueia sem
    exceção e o objetivo é ficar ao lado do alvo (quatro vizinhos). O
    comentário em `pathfinder.h` é que dizia "pendente".
13. ~~Fila de waypoints com Shift~~ — **não é pendência**: conferido em
    2026-10-29, a fila `unk_5471B0`/`dword_5471AC` é código morto no
    executável. O contador é o campo +0x3C do objeto do modo de mira, que
    `sub_41B550` zera ao entrar no modo e que ninguém incrementa — as três
    funções que o leem só leem. O jogo publicado manda sempre UM ponto por
    comando, que é o que o porte faz.
14. ~~Dano em paredes nas autodestruições~~ — **não é pendência**: conferido
    em 2026-11-03, `explodeDestruct` chama `damageWall` com
    (raio − d) * (Concussion + Penetration) / raio, que é exatamente a conta
    de `sub_4A4090`. Dois comentários (em `weapon.cpp` e `disaster.h`) é que
    anunciavam a pendência.

### Encontradas no inventário de 2026-11-03

Varredura dos marcadores do próprio código contra o disassembly. Cinco
"pendências" que os comentários anunciavam **não existiam** (a garagem, o
micróbio, o áudio das mensagens, a fase B da economia e a serialização de rede
do `CommandPacket` estão todos portados — os comentários é que tinham ficado
para trás).

**Segunda passada, no mesmo dia**, agora conferindo cada item no código em vez
de ler o comentário que o anunciava: **outras três eram falsas** (as 12, 14 e
17 — modo 3 do pathfinder, dano em paredes nas autodestruições e animações de
ambiente — estavam portadas, com o comentário atrasado). Das seis reais,
16, 18, 19 e 20 foram fechadas no mesmo dia; **sobra a 21**.

16. ~~Veículo guardado em garagem não conta em lugar nenhum~~ — **feito**. Era
    o único item que podia quebrar um objetivo de missão: a garagem foi portada
    em 2026-10-14 (`garageSlot[6]`, `garageCount`), mas seis pontos de contagem
    ignoravam quem estava dentro dela, e no original **todos** tratam o 0x29.
    Os seis, com a regra de cada um lida no disassembly:

    | Ponto | Baias que varre | Regra |
    |---|---|---|
    | `sub_493AF0` (contagem por jogador) | **6, fixo** (`loc_493D0E`) | tipo + filtro de carga de `byte_493FD4` |
    | `sub_494280` (gatilho KIT) | **Storage Bays** | ConVec (tipo 2) carregando o kit |
    | `sub_494E70` (contagem de veículos) | `vt[63]` | soma a contagem à lista |
    | `sub_494600` (gatilho EVAC) | **6, fixo** | transportes 0x0D guardados |
    | `?HasOccupiedBay` | `vt[63]` | devolve a **contagem**, não 0/1 |
    | `?hasVehicle` | **6, fixo** | tipo + carga (sempre a palavra inteira) |

    A inconsistência das baias (uns laços fixam 6, outros vão até a Storage
    Bays) é **do original**, e foi mantida. A regra de carga também difere entre
    as duas metades de `sub_493AF0`: o ramo de **um** jogador aceita os tipos 1,
    2, 5, 6, 7 e 0x0D (tabela `byte_493FD4`) e varre as garagens; o de **todos**
    (`loc_493DE2`) só 1, 2 e 0x0D, e **não** olha garagem nenhuma — também do
    original, conferido, e marcado no código para ninguém "unificar".
    Teste: `--garagecounttest <DLL>`, dez casos; neutralizando a correção, oito
    reprovam e os dois controles negativos continuam passando.
17. ~~Animações de ambiente dos prédios~~ — **não é pendência**: conferido em
    2026-11-03, `structureAmbientTick` é `sub_4835C0` inteiro, com a tabela
    `byte_4E4138` (11 registros) e a variante por tipo (`desc+0x2A6/+0x2A7`),
    o gate de 100 ticks defasado por y e id, e a opção `Ambient` do INI. O
    comentário em `structure.h` é que dizia "ainda não portado".
18. ~~Auto-engajamento: "atacado nos últimos 12 ticks"~~ — **feito**. Era a
    única pendência deste inventário que mudava o que se vê jogando, e o que
    `sub_44DE10` faz não é escolher alvo: é **alertar a vizinhança**. A unidade
    atingida nos últimos 12 ticks passa o próprio agressor para todo veículo
    armado do próprio lado a menos de 96 px (3 tiles) em x e y que esteja
    parado (`[+0x21]` = 0) e ainda sem agressor (`[+0x26]` = 0), datando o
    alerta de 25 ticks atrás — sobram 125 dos 150 ticks da janela de revide.
    Resultado: o esquadrão reage junto em vez de só quem levou o tiro. Cada
    alerta soma uma referência (`inc byte [+0x32]`) no agressor.
    **Só vale para dono humano**: o gate do topo de `sub_44DE10` é
    `dword_56E304[dono]`, que é `isHuman` (`[jogador+0x28]`); a IA tem o
    caminho dela, `ScGroup::HasBeenAttacked`. Teste: `--alerttest <DLL>`, onze
    casos, inclusive a borda exata dos 96 px (o asm compara com `<` estrito) e
    o gate do dono do PC; neutralizando a correção, cinco reprovam e os sete
    controles negativos continuam passando.

    A caixa de busca é por células em vez da lista de unidades ordenada por
    `[+0x18]` que o original percorre nos dois sentidos — o conjunto é o mesmo
    (veículo ocupa uma célula) e o teste de pixel ficou idêntico.
19. ~~`vt[35]` na morte do veículo~~ — **feito**, e não era "descarga por
    classe": extraí as 40 vtables de unidade do asm e em todas, menos
    `off_4D1788`, a entrada 35 é a **mesma** função, `sub_483850` — que põe
    `dword_56E37C[dono] = 1`, o `colonyMarkDirty` do porte. `loc_43D8B3` só a
    chama com o bit 1 de `[+0x44]`, ou seja só em veículo. Uma linha em
    `vehicleDeath`: veículo morto obriga a economia do dono a recalcular.
20. ~~Grupo de IA com prazo vencido~~ — **feito** (`sub_42B360`). A ordem dos
    dois comandos importa e é esta: vencido o prazo, **nos primeiros 400 ticks**
    cada membro leva a autodestruição (0x14) e, **de 400 ticks em diante**, quem
    ainda estiver na lista leva a de sair do mapa (0x2A). Na prática o segundo
    ramo só alcança quem a autodestruição recusa — `unitDoSelfDestruct` exige
    comandável, então um veículo sob EMP sobra para o 0x2A. Quem não é veículo
    (sem o bit 1 de `[+0x44]`) sai do grupo antes, pelo `vt[11]`, e leva a
    ordem do mesmo jeito. O grupo não é apagado aí: cai no passe em que a
    lista chega a zero. Teste: `--deletetest <DLL>`, oito casos cobrindo os
    três ramos; neutralizando a correção, três reprovam.
21. ~~Três miudezas de apresentação~~ — **feito** em 2026-11-06. As três, e o
    que cada uma era de verdade:

    **(a) A plataforma do Spaceport não anunciava.** `sub_47F600` não toca um
    som posicional: chama `sub_47F060`, que é a **fila de vozes do Savant**. A
    SULV (0x69) e o míssil (0x6B) anunciam com a voz **0x80**; a RLV (0x6A), com
    a **0x7F**. E só o **dono** ouve (`cmp eax, dword_56DEE8`).

    **(b) O ícone do painel STORAGE ignorava a arma.** `sub_458EF0` já estava
    portado — é o `drawVehicleIcon` que o quadro da unidade usa, com o corpo na
    cor do dono e a torre da arma por cima. O painel STORAGE é que não o
    chamava: desenhava o quadro 0 do corpo e pronto. Agora chama, e um Laser
    Lynx e um Microwave Lynx aparecem diferentes na caixa da doca.

    **(c) O anel anti-duplicata `[_Player+0xBD8]`** — 16 posições, uma por
    turno. **Dez** comandos passam por ele (os `case` de `sub_40E210` que tocam
    `[ebp+eax*4+0BD8h]`): 0x09 produzir, 0x0A..0x0D as cargas, 0x12 e 0x13
    ociosar e religar, 0x16 pesquisar, 0x17 treinar e 0x19 lançar. Antes de
    aplicar, o original vê se a **mesma unidade** já foi assunto de um desses
    comandos na janela recente; se foi, o comando é **descartado**
    (`loc_40E93C` devolve 1 e o despachante sai sem aplicar). É a proteção
    contra comando repetido — o clique duplo, ou o pacote que chegou duas vezes.
    Vale **só para jogador humano** (`[_Player+0x28]`).

    A janela não é a óbvia, e foi conferida instrução por instrução: com
    `turno = tique >> log2(período)`, ele confere os slots de `turno−4` a
    `turno−2` — três, porque `dword_56DED8` é o mesmo **kTurnLead** do porte —
    e **grava** no slot do `turno`. O turno corrente e o anterior ficam de fora
    da conferência, o que casa com o lockstep: o comando aplicado no turno T foi
    emitido em T−kTurnLead, logo a janela cobre o turno da emissão e os dois
    anteriores. O anel mora no próprio `PlayerData`, como no original, e por
    isso zera em `resetPlayers` e entra no jogo salvo por `playersSerialize`.

    Uma diferença **proposital**: o índice do original sai de um `%` **com
    sinal**, então nos primeiros turnos ele lê alguns dwords *antes* do anel —
    campos vizinhos do jogador. Aqui o índice é normalizado para 0..15; na
    prática dá no mesmo (os slots começam zerados e nada casa), sem ler fora do
    vetor.

    O único dos dez que o porte não guarda é o **0x0D**, que o despachante do
    porte não trata (o enum vai de 0x0C a 0x0E); a entrada está em `usaAnel`
    para quando ele existir.

    Teste: `--item21test <DLL>`, quinze casos. Neutralizando as três peças,
    cinco reprovam — ao menos um por peça — e os dez restantes, controles
    incluídos, continuam passando.

    **Depois (5.39):** o original grava só a chave e nunca limpa o slot, então
    16 turnos depois o registro velho descartava o comando legítimo — a
    pesquisa e a produção que "falhavam ao acaso". O slot agora guarda também
    o turno; mais dois casos no teste (17).

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

## 5.6. Cheats digitados no chat — DECODIFICADOS ✅ (2026-11-06)

Os cheats de *Outpost 2* não são teclas: são **frases escritas na linha de
conversa**. O jogador digita, aperta Enter, e o pacote de chat (0x30) passa pelo
reconhecedor `sub_58642A` — que fica no segmento **DSEG** do `OUTPOST2.EXE`,
escrito à mão em assembly, fora do código gerado pelo compilador.

**As frases não existem no binário.** Só ficam guardados quatro **hashes** de
32 bits (VA 0x5863B8), e os quatro handlers são instalados em tempo de execução
por `sub_58650C` (um `rep movsd` de 4 dwords para 0x5863C8). É por isso que
procurar "Dan" ou "cheat" no executável não acha nada: o que achou o caminho foi
o literal `"Woof"`, que o código monta com imediatos (`mov eax, 666F6F57h`).

### O reconhecedor

`sub_58642A(texto, remetente)` anda no texto em grupos de 4 bytes. Por grupo o
hash é rodado à esquerda por `cl` — o nibble alto do byte baixo do grupo
**anterior** — e XORado com esse grupo anterior, uma vez por byte, parando no
primeiro zero; fechado o grupo, o byte baixo do hash leva um XOR com o 4º byte.
Ao achar o zero devolve na hora, **então os bytes depois do fim da frase nunca
entram na conta**: o hash depende só do texto.

Casando, o efeito é aplicado e o texto do chat é **substituído por "Woof Woof!"**
— que é o que todos leem, e a confirmação de que o cheat pegou.

Dois detalhes que importam:

- O remetente vem de `[pacote+0x0E]`, que `sub_4115F0` preenche com
  `dword_56DEE8` (o jogador local). **O cheat vale para quem digitou.**
- O reconhecedor roda **antes** do teste da máscara de destinatários
  (`loc_40FC96`), logo ele roda em **todas as máquinas**. O efeito é idêntico em
  todas e o lockstep da rede continua de pé — não é um buraco de sincronia.
- **Neste porte eles valem só em JOGO SOLO** (2026-11-09). O original não tem
  porteiro nenhum: a frase funciona em qualquer partida, inclusive em rede, e
  aí um jogador enchendo o próprio estoque estraga a partida dos outros. Como o
  OPUPatch, o reconhecedor agora começa por `cheatsAllowed()` — falso em rede
  (`netActive`) e em cenário multijogador (`multiplayerMission`, o `sub_4027D0`)
  —, e a frase segue como mensagem comum de chat. As duas perguntas não dependem
  de quem pergunta, então todas as máquinas respondem igual e o lockstep segue
  intacto.
- A quinta entrada da tabela é zero e o `jcxz` depois do `repne scasd` a torna
  inalcançável **de propósito**: é o "nenhum casou" do laço, não um bug.

### As quatro frases e o que cada uma faz

| # | Hash | Frase | Efeito, lido do código |
|---|---|---|---|
| 0 | `FF27BC04` | `Dan's dog could make more ore.` | `sub_49E030`: os dois estoques vão para a capacidade (`ore = oreCapacity`, `rareOre = rareCapacity`) |
| 1 | `48D60158` | `Dan's car could hold more people.` | `sub_49E070`: trabalhadores **e** cientistas ganham **25%** (`v + v/4`, divisão truncada) |
| 2 | `AC0FF0E5` | `Dan's bike could backfire more often` | `sub_49E0D0`: a recarga (bloco +0x30) das **16** armas 0x3B..0x4A vira `min(recarga − 2, 20)` |
| 3 | `ABE0CC11` | **desconhecida** | `sub_49E120`: a penetração (bloco +0x2C) das **14** armas 0x3D..0x4A vira `max(pen × 33/32, 500)` |

**A pontuação faz parte do hash.** As duas primeiras exigem o ponto final; a
terceira **não pode** ter ponto. Errar a pontuação não faz nada — a frase vai
para o chat como conversa comum.

As duas últimas escrevem no mesmo bloco de 0x44 bytes que as melhorias de
pesquisa usam, então entram no jogo salvo e `reloadSheets()` as desfaz no começo
da partida seguinte. Repetir o cheat empilha o efeito.

As duas também põem `dword_4E0BD0 = 1`. Esse sinalizador tem **um** leitor no
original — `sub_431EB0`, um caminho de criação de unidade tipo 4 onde ele limpa
a flag 0x400 (danificável) da unidade criada. Não identifiquei essa classe, e
por isso o porte **não** tem o sinalizador: ele não teria leitor nenhum, e
inventar um consumidor seria pior que registrar a lacuna aqui.

### A quarta frase, e o extra do porte

A frase do índice 3 nunca foi publicada e **não dá para recuperá-la do hash**:
tentei 66 mil candidatas no molde `Dan's <algo> could <verbo> <algo>` e nenhuma
casou. O hash `ABE0CC11` fica na tabela do porte: se alguém descobrir a frase,
ela funciona, sem mudar uma linha.

Para que o quarto efeito tenha uma frase utilizável, o porte acrescenta **uma
entrada extra que o original não tem**, apontando para o mesmo handler:

| Hash | Frase | |
|---|---|---|
| `9AC1C27E` | `Dan's gun could die more people.` | **extra do porte** |

### O que NÃO existe (alegações conferidas e descartadas)

Listas de cheats que circulam na internet trazem coisas que o código desmente.
Conferido em 2026-11-06:

- **`Dan's dog could make more ore` sem o ponto** não "adiciona sete
  trabalhadores e três cientistas": o hash dela é `86352271`, que **não está na
  tabela**. Ela não faz nada.
- **`Ctrl+F4` não aumenta a população.** Os únicos escritores de trabalhadores
  (`dword_56E370`) e cientistas (`dword_56E374`) no executável inteiro são o
  handler de `GameOpt` (`sub_4229F0`), a API exportada
  `_Player::SetWorkers`/`SetScientists`, e o cheat 1. Não há atalho de teclado.
- A descrição "aumenta a capacidade do carro do Dan" para o cheat 1 é leitura
  literal da piada: o que ele faz é somar 25% à população.

### A linha de conversa abre em jogo solo (consertado em 2026-11-06)

Achado logo depois, conferindo o caminho até aqui: o porte só abria a linha de
conversa com o Enter **em partida multijogador**, e com isso os cheats ficavam
inalcançáveis justamente onde servem. O filtro do original (`sub_4115F0`) não
testa o tipo de jogo: trata o `WM_CHAR` 0x0D com a linha fechada e abre, ponto.
O porteiro saiu.

O `--chattest` **devolvia 0 sempre**, mesmo imprimindo `FALHOU` — foi por isso
que a regressão parecia limpa com a linha morta em jogo solo. Agora devolve o
código certo e tem um caso novo: digitar `Dan's dog could make more ore.` na
linha, em jogo solo, e ver o estoque encher — o caminho inteiro, da tecla ao
`sub_58642A`, e não só o hash.

### A outra via: a API de cheat das missões (ainda não portada)

Separado das frases, o `OUTPOST2.EXE` exporta uma API de cheat para as DLLs de
missão, por cima do pacote **0x2B** (`GameOpt`) via
`TethysGame::sIssueOptPacket(indice, valor)`. Os oito primeiros índices caem no
caso padrão de `sub_4229F0` e viram dwords no objeto `dword_56DE58`:

| Índice | Variável | Função exportada | O que faz |
|---|---|---|---|
| 0 | `56DE58` | `SetCheatUnlimitedResources` | não desconta recursos |
| 1 | `56DE5C` | `SetCheatProduceAll` | produção ignora a pesquisa (`sub_472C50`) |
| 2 | `56DE60` | — | overlay de depuração ("Srpls") |
| 3 | `56DE64` | — | dano × 4 no cálculo de `sub_433...` |
| 4 | `56DE68` | `SetCheatFastUnits` | pula o custo de terreno do movimento |
| 5 | `56DE6C` | `SetCheatFastProduction` | timer de produção = 1 tique |
| 6 | `56DE70` | — | não identificado |
| 7 | `56DE74` | — | desliga `sub_43A330` em `sub_44CD10` |

**Nenhuma das 94 DLLs de missão originais importa essas funções** — é por isso
que elas nunca fizeram falta para a conta de "222 de 222 símbolos". Os outros
índices do mesmo pacote (morál forçada, luz do dia) **estão** portados, em
`colonyForceMorale` e `TethysGame::SetDaylight*`. Portar as cinco de cheat é
trabalho em aberto, útil só para quem escrever missões novas.

Teste: `--cheattest <DLL>`, dezenove casos — os quatro hashes contra os valores
gravados no executável, o efeito de cada cheat (inclusive o teto de 20, o piso
de 500 e a EMP ficando de fora da faixa) e cinco controles negativos de
pontuação.

---

## 5.7. A aba Options (`GAME OPTIONS`) — CONSERTADA ✅ (2026-11-06)

Relatado pelo usuário: *"todos os menus do game options: Restart Mission, End
Mission, Exit Game, etc estão sem função"*. Estava certo, e eram **dois
defeitos independentes** — mais três divergências de fidelidade que apareceram
ao conferir `sub_465830` e `sub_457880` no disassembly.

### Defeito 1: `L&ESS` e `&MORE` com o id de outro comando

Os três ids `kCmdOptLess`, `kCmdOptMore` e `kCmdOptBack` valiam −20, −21 e −22
— exatamente os de `kCmdLabsCurrent`, `kCmdResStructures` e `kCmdResSummary`.
Em `clickCommand` os tratadores dos relatórios vêm **antes** e saem com
`return` incondicional (`if (cmd == kCmdResStructures) { if (m_tab == 2) ...;
return; }`), então na aba Options o clique morria ali. Os dois botões do
`PREFERENCES` não faziam nada. Agora são −70, −71 e −72.

As alternâncias ("Play Music", "Display Vehicle Shadows", ...) sempre
funcionaram, porque mudam no clique da **lista**, não nesses botões — foi por
isso que o defeito passou batido.

### Defeito 2: a confirmação se fechava sozinha

Cada item da tela de fim de missão abre a confirmação `dword_564EA8` (os casos
3 a 7 de `sub_457880`), e é o `&YES` que executa o caso 8..0xC
correspondente. No porte ela abria e **morria no quadro seguinte**, por três
motivos somados:

- `ConfirmPane::update` fechava a confirmação sempre que a seleção não fosse
  exatamente a unidade que a abriu. Em `sub_45C1A0` esse auto-fecho tem uma
  exceção que faltava: `cmp eax, offset unk_565AC8` — **se quem a abriu foi o
  painel de Game Options, ela não depende de seleção nenhuma**. Sem nada
  selecionado (o caso normal nessa tela), ela fechava no quadro seguinte.
- os botões `&YES`/`&NO` (`sub_45C200`) nunca eram montados, porque o ramo da
  aba 5 devolvia os botões da própria tela e saía antes;
- e o desenho mostrava o painel de opções em vez da mensagem (`sub_45C0B0`).

No original a confirmação **toma** o painel (`sub_45D020` troca o painel do
contentor) e o `&NO` devolve o painel que a abriu. É assim agora: a tela de fim
reaparece inteira ao desistir.

### Divergência A: a tela de fim de missão é UMA DE DUAS

`sub_465830` tem cinco modos, e o botão `E&ND MISSION` escolhe entre o 3 e o 4
pelo mesmo teste de `sub_4027D0`. O porte juntava os quatro itens numa tela só:

| | jogo solo (modo 3) | multijogador (modo 4) |
|---|---|---|
| itens | `RESTART MISSIO&N` (caso 8), `&BACK TO MAIN MENU` (9), `E&XIT OUTPOST 2` (0xA) | `SELF-&DESTRUCT` (0xB), `&TRANSFER` (0xC) |
| título | `Game End: Single Player` | `Game End: Multiplayer` |

Ou seja: **não se reinicia nem se volta ao menu numa partida em rede**, e **não
existe autodestruição em jogo solo**. O `&TRANSFER` ("&Transfer
vehicles/destroy structures") faltava por inteiro; ele nasce desabilitado a
menos que `sub_4909F0` ache outro jogador **humano** com o meu bit na máscara
de aliança — é para ele que os veículos vão.

### Divergência B: o título não é "End Mission"

São cinco, um por modo (`off_4E8B28`..`off_4E8B38`): "Game Options", "Mission
Objectives (Alt+O)", "Game Preferences", "Game End: Single Player" e "Game End:
Multiplayer". O porte escrevia "End Mission", que não existe no executável.

### Divergência C: o `BACK TO MAIN MENU` manda pacote

O original não encerra na mão: os casos 9, 0xB e 0xC chamam `sub_48A5D0`, que
monta o **pacote 0x31** com o método (`GoAI`, `SelfDestruct`, `Transfer`) e um
atraso — **4 na autodestruição e 2 nos outros dois**, detalhe que o porte
tratava como constante. E `applyQuit` tinha um `method != QUIT_GO_AI` a mais:
em `loc_40FD54` quem saiu encerra em **qualquer** método, com o código 6 em
jogo solo e 9 em rede.

### O teste

`--optmenutest <DLL>`: 50 casos em jogo solo e 52 em multijogador (roda com
`E04.DLL` e `MF2_01.DLL`). Cobre os dois botões do `PREFERENCES` separadamente,
a composição e o título das duas telas de fim, a confirmação sobrevivendo ao
quadro seguinte, a troca dos botões por `&YES`/`&NO`, o `&NO` devolvendo a
tela, o `&YES` do `E&XIT` pedindo o fim do programa, o `sub_4909F0` nos três
estados e o caminho inteiro do clique até o pacote 0x31, medindo o atraso.

Neutralizando cada peça, uma por vez: ids de volta **2** falhas (uma por
botão), auto-fecho da confirmação **7/6**, sem `&YES`/`&NO` **6/5**, tela de
fim sem o par solo/rede **0/10** (o esperado: em jogo solo a lista antiga já
era a certa), título **1/1**, `GoAI` sem encerrar **4/3** e atraso fixo
**0/1**.

---

## 5.8. Extra do porte: o modo de tela no `PREFERENCES` (2026-11-06)

Pedido no fórum do outpost2.net depois da publicação: **modo janela escolhido
dentro do jogo**. A janela já abria dos dois jeitos (`[Game] Fullscreen` e
Alt+Enter), mas só pelo arquivo — não havia como escolher jogando.

No fim da lista do `PREFERENCES` entrou um **par de rádio**, que o original não
tem:

```
Screen Mode
  ( ) Full Screen
  (o) Window
```

São **duas linhas**, e não uma alternância como as outras oito opções de
liga/desliga, para o grupo se comportar como os rádios dos diálogos do shell: o
clique escolhe a linha em que se clicou, em vez de inverter o que estava
(clicar duas vezes na mesma linha não faz nada). O desenho `"(o) "`/`"( ) "` é o
mesmo de `shell_menu.cpp`, e o arranjo de duas linhas (nome do grupo em cima,
valor indentado embaixo) é o que os itens com valor já usam nesta lista. As
linhas ficam abaixo da dobra: uma página da seta de rolagem chega lá.

Três detalhes que fazem a coisa funcionar de verdade:

- **Só vale no `&APPLY`**, como todo o resto desta tela — e o APPLY grava
  `[Game] Fullscreen` no INI junto com o resto, porque `saveIni` manda o
  espelho inteiro para o arquivo.
- **O espelho é acertado com a janela de verdade ao entrar no `PREFERENCES`**
  (`GameState::enterPrefs`): o Alt+Enter troca a tela sem gravar nada no INI,
  então o valor do arquivo pode estar velho e o rádio mostraria um modo
  enquanto a tela é outra.
- `clickCommand` recebe só o id do comando, **sem o `Renderer`**: o clique e o
  APPLY deixam o pedido marcado e quem resolve é o `onIdle`, que tem
  (`GameState::applyScreenMode`). Por isso `loadSettings` **não** aplica o
  `Fullscreen` — se aplicasse, qualquer `loadSettings` jogaria a janela de volta
  para o valor (velho) do arquivo.

O outro pedido do fórum — **ver mais mapa de uma vez** — foi feito logo depois:
o quadro do jogo passou a ter o tamanho da janela em vez de esticar 640×480, com
o HUD se recolocando (`relayout`). O shell segue em 640×480 escalado, que era a
saída prevista: é arte de tamanho fixo.

### A lista não dava para rolar (consertado junto)

O rádio não aparecia para quem jogava, e a culpa não era dele: a lista do
`PREFERENCES` ia até o **pé do painel**, e os três botões são desenhados por
cima. Com isso as **setas de rolagem nasciam debaixo dos botões** — invisíveis
e, pior, o clique vai ao painel **antes** dos botões (`handleEvent`), então a
lista ainda comia o clique do `L&ESS`. Nada depois da primeira página era
alcançável: o `Skip Frames` já não era, antes de existir rádio nenhum.

A lista agora recua (`layout()`, como já fazia nas telas do manual) e as setas
ficam acima dos botões. Quatro casos novos, dois deles medindo o que o jogador
sente: a seta não cai debaixo de botão nenhum, e o painel não come o clique do
`L&ESS`. Neutralizando o recuo: **2 falhas**.

**A mesma geometria vale para qualquer painel com 3 ou mais botões**: o
`layoutButtons` põe a primeira fileira em `kPaneBottom-2-50` (416..441), e as
setas caem em ~414..434. O `Labs`, com cinco botões, é o caso óbvio. Não
varrido ainda.

### O teste

Os 12 casos novos do `--optmenutest` passam pelo `onIdle` de verdade (não por um
atalho): o rádio nascer no modo da **janela** e não no do INI, o texto das duas
linhas com a bolinha no lugar certo, o clique escolhendo em vez de inverter, o
clique **não** mexendo na tela, o `&APPLY` mexendo, e o INI ficando com 1 e com
0 — com o arquivo posto no valor **oposto** antes de cada medida, senão o caso
passa de graça. O INI do usuário é guardado em bytes e devolvido no fim.

Neutralizando cada peça: sem `applyScreenMode` no `onIdle` **4** falhas, sem
acertar o espelho **3**, APPLY que não aplica **1**, rádio virando alternância
**5**, INI que não guarda o `Fullscreen` **2** — com a base em 0 antes e depois.

---

## 5.9. As dicas do `PREFERENCES` são por item — e a tela toda é outra (2026-11-06)

Investigando a dica genérica dos botões `L&ESS`/`&MORE`, o `sub_465DA0` levou a
uma **tabela de 7 dwords por item** em `0x4E3118`, indexada a partir de **1**
(`[esi+250h]` é o item escolhido). Cada campo é um índice no vetor `Format`
(`0x4E8xxx`, entrada 0 = `" ENGLISH"`):

| campo | o que é |
|---|---|
| 0 | dica do controle de estado — `"Toggle music"` |
| 1 | rótulo do controle de estado — `"Play Music"` |
| 2 | id do comando (0x0E, 0x0F, 0x11…) |
| 3 / 4 | dica e rótulo do botão da direita — `"Turn music volume up (&More)"` / `"&MORE"` |
| 5 / 6 | dica e rótulo do botão da esquerda — `"Turn music volume down (L&ess)"` / `"L&ESS"` |

Resolvida, a tabela dá a tela inteira:

| item | controle de estado | L&ESS/&MORE |
|---|---|---|
| 1 | `Play Music` (cmd 0x0E) | volume da música |
| 2 | `Play Sound Effects` (0x0F) | volume dos efeitos |
| 3 | — | `Game Speed` |
| 4 | — | `Scrolling Speed` |
| 5 | `Display Computer Overlay` (0x11) | — |
| 6 | `Display Ambient Animations` (0x10) | — |
| 7 | `Display Structure Shadows` (0x12) | — |
| 8 | `Display Vehicle Shadows` (0x13) | — |
| 9 | `Display Completed Objectives` (0x14) | — |
| 10 | `Skip Frames` (0x15) | — |

### O que foi corrigido

As **dicas por item**. O porte dizia `"Turn this option down (L&ess)"` para
qualquer item; agora cada um diz o seu (`GameOptionsPane::tipLess/tipMore`).
Dez casos novos no `--optmenutest`; voltando à dica genérica, **10 falhas**.

### O que ficou ABERTO (decisão, não esquecimento)

A tela do original **não é a que o porte mostra**:

- são **10 itens**, não 13. Música e som são UM item cada, com a alternância
  **e** o volume juntos; o porte parte cada um em duas linhas;
- a lista mostra só o **nome curto** do item (`"Music"`, `"Sound Effects"`,
  `"Game Speed"`…, que é o que o `sub_4663A0` devolve), e não o valor;
- embaixo da lista há uma **área de detalhe** do item escolhido
  (`sub_465DA0`): em `[ebx+18h]+0x1E` o controle de estado com o nome do item
  (`"Play Music"`) e a marca de ligado, em `+0x14` abaixo o valor
  (`"Volume:<R>%i"`) e em `+0x46` os dois botões;
- **o botão de alternância existe** — era o que faltava achar. Não há string
  `"TOGGLE"` no executável porque o rótulo dele é **o nome do próprio item**; a
  dica é que é `"Toggle music"`. No porte a alternância é o clique na linha;
- `Graphic Detail Level` **não tem registro** nesta tabela: a linha existe no
  porte, mas no original o item não está na lista de dentro do jogo (as duas
  dicas dele existem no executável e foram usadas assim mesmo).

Refazer a tela é trabalho de uma sessão e mexe numa coisa que hoje funciona,
então fica para quando for decidido.

---

## 5.10. A lista de defeitos do OPUPatch, conferida no porte (2026-11-08)

O **OPUPatch** é o patch de comunidade para o `OUTPOST2.EXE` original, e o
changelog dele é a melhor lista pública de defeitos do jogo de 1997. Serve aqui
como **documentação**, igual ao disassembly: nenhuma linha de código veio de lá
(projeto separado, outra licença).

Pela diretiva do porte — *achou defeito, conserta, não reproduz* —, cada item
dessa lista é um lugar para conferir. Conferidos um a um no nosso código:

| Defeito que eles corrigem | No nosso porte |
|---|---|
| Meteor Defense derruba míssil de **aliado** | **reproduzia** — `missile.cpp` pulava só o dono. **Corrigido** |
| Muro falha num 3×3 em volta de muro em obra, Blight ou lava | **reproduzia** — `wallNeighborOk`. **Corrigido** |
| Alerta de aliança dispara no início da partida | **não se aplica** aqui — ver 5.11 |
| Alerta de transferência com IA/Gaia | reproduzia. **Corrigido** |
| "Power Levels: Optimal" / "Storage Needed" repetindo | reproduzia. **Corrigido** |
| Limite de unidades | o porte não tinha limite nenhum. **Posto em 2048**, como eles |

Não conferidos ainda, e por isso não afirmados: EMP que não pousa para Eden,
Meteor Defense ignorando míssil em cima dela, perma-walls quando o Earthworker
morre, destroços virando entulho, travamento ao transferir para Gaia, carga
parcial do caminhão, `ForceMorale*` na 2ª chamada, `ScGroup::TakeUnit`,
`DoInfect` com mais de um jogador.

Fora do alcance por escolha, não por esquecimento: os cheats continuam valendo
em multijogador (eles desligam; o original permite) e as mudanças de equilíbrio
da lista deles (Light Tower avisando inimigo, EDWARD revelando balizas para
aliados, animação das balizas por variante).

### 1. A defesa e o míssil do aliado

`sub_4802A0` varre os jogadores e pula **só o dono do míssil**:
`if (p == ownerOf(...)) continue`. Numa partida aliada, a Meteor Defense do
amigo abatia o míssil que ia no inimigo. O porte agora também pula quem é
aliado, com a conta que o resto do combate usa (`candidateOk` em `weapon.cpp`:
o bit de quem atira na máscara de quem leva, `sub_490C50`).

Teste novo **`--mdtest <mapa>`**: Observatory + Meteor Defense do jogador 1,
míssil do jogador 0 descendo ao lado. O observável é a flag `0x40000`, que o
original põe em **qualquer** engajamento — assim o caso não depende do dado de
50/75% da Observatory. Quatro casos, dois deles de controle: a defesa está
operacional (senão o resto não mede nada) e **contra inimigo ela engaja**.
Neutralizando a correção: 1 falha.

### 2. O muro e os oito vizinhos

`wallCellOk` testa as 8 células em volta (`unk_4E0C10`) com `sub_438D70`, que
reprova a célula com qualquer bit de `0xA8000000`. Lendo o que cada bit é no
nosso mapa, sobram exatamente três casos:

| bit | o que é | por que não deve impedir |
|---|---|---|
| 31 | a **reserva** de um muro em obra (posta em `sub_4235E0`, tirada ao assentar) | dois Earthworkers lado a lado se travam — e o próprio veículo se barrava, porque a célula que ele acabou de reservar é vizinha da próxima |
| 29 | a **praga** (`disaster.cpp` marca o tile infectado) | não se fecha uma linha de muro perto da praga |
| 27 | a **lava** | idem perto de lava |

Muro pronto sai antes (cellType 0x17–0x19) e tubo pronto é 0x1A, sem bit;
prédio vizinho continua reprovando, pela linha do Guard Post. Cinco casos novos
no `--walltest`, com os dois controles (vizinho limpo passa, prédio vizinho
reprova). Voltando ao teste do original: **3 falhas**, uma por bit.

### 3. O teto de unidades

O porte **não tinha teto**: o pool é um `std::vector` que crescia sem limite.
E não é detalhe: a ocupação da célula do mapa tem **11 bits**
(`CELL_OCCUPANCY = 0x07FF0000`), então o maior id que cabe lá é **2047** —
passar disso não daria "cheio", daria ocupação truncada, que é silenciosa e bem
pior. O `createUnit` agora recusa em `kMaxLiveUnits = 2048`, o mesmo número do
OPUPatch (que dobra o 1024 do original). Caso novo no `--unittest`: enche até
recusar e confere que parou no id 2047.

---

## 5.11. Os avisos do Savant (2026-11-08)

Os três que tinham ficado abertos na 5.10. Um deles **não existia**, e no lugar
dele apareceu um defeito nosso, bem pior.

### O alerta de aliança: não se aplica

Eu tinha anotado "reproduz" olhando só o `applyAlly`, que de fato posta o aviso
sem olhar o relógio. Seguindo o caminho até o começo da partida, não é por ali
que as alianças iniciais passam: o `P_AllyWith` da ponte da API (o que as
missões chamam no `InitProc`) mexe na máscara **direto, sem mensagem nenhuma**,
e no porte **não há mais nenhum** lugar que escreva `allyMask` — só o reset e o
pacote 0x32, que é a aliança pedida no jogo, e essa deve avisar mesmo.

Fica o registro de método: a anotação da primeira passada estava errada porque
foi feita pelo ponto de chegada, sem seguir quem chama.

### A transferência de/para jogador de PC

O escolhedor da UI só lista **humanos** (`m_pickPlayer` pula `!isHuman`), então
o caminho que importa é o dos scripts: `U_DoTransfer` → pacote 0x18 →
`applyTransfer`. Missão transfere unidade de e para jogador de PC o tempo todo,
e cada uma virava um "Transfer completed to …" na cara de quem joga. Agora o
aviso só sai quando o **outro lado é humano** — entre humanos ele continua,
porque ali é o único jeito de saber que chegou alguma coisa.

### Energia e estoque: só a mudança avisa

`Power Levels: Optimal` e os dois `Storage Needed` saíam a **cada** recálculo da
colônia. Agora valem por mudança de estado; os avisos de **falta**
(`Shortage`/`Marginal`) seguem insistindo, que é o que se espera deles. A
memória disso mora num `static` do `colony.cpp`, fora do `ColonyState`: é
apresentação e só do jogador local, então **não entra no jogo salvo nem na
simulação** — o lockstep fica intacto.

### O defeito que o teste achou: o relógio do filtro

Montando o caso, o controle "o aviso TEM de sair quando o estoque enche" dava
**zero**. A causa não era a correção:

```c
static bool cooldownAllow(uint32_t& lastT, uint32_t span, uint32_t now) {
    if (lastT + span >= now) return false;      // <- aqui
```

`lastT == 0` quer dizer "nunca avisou". No original isso se resolve sozinho,
porque o relógio é o `timeGetTime` — tempo desde o **boot do Windows**, sempre
enorme —, então `0 + 120000 >= now` é falso. O porte trocou por `SDL_GetTicks`,
que começa em **zero com o processo**: a conta virava verdadeira e **barrava os
avisos de armazenamento e de prédio desligado nos dois primeiros minutos de
cada partida**, em silêncio. Na mesma função, o `now - 0x61A8` da janela de 25 s
estoura por baixo pelo mesmo motivo; passou a somar do outro lado.

Não é defeito do original: é do porte, nascido da troca de relógio. O
`--colonytest` pega: neutralizando só essa linha, o controle volta a dar zero.

### Os testes

- `--transfertest <DLL>`: transferência para jogador de PC **cala** e para
  humano **avisa**, pelo `applyTransfer` de verdade.
- `--colonytest <mapa>`: três medidas para o estoque (vazio → nada; enche →
  avisa; segue cheio → não repete) e três para a energia (desliga o gerador →
  falta avisa; religa → "Optimal" sai; parado → não repete). Entre as medidas
  o teste chama `resetMessages()`, que zera o filtro de tempo real: **sem isso
  o caso passaria de graça**, porque o filtro sozinho já barra o repetido por
  dois minutos — e o que eu mudei foi a colônia, não o filtro.

Neutralizando, uma peça por vez: transferência **1**, estoque **1**, energia
**1**, relógio do filtro **1** — cada uma com a sua assinatura, e a base em 0
antes e depois.

---

## 5.12. Mais três da lista do OPUPatch (2026-11-09)

Dos nove que faltavam conferir, três se confirmaram e foram corrigidos; dois
**não se aplicam** ao porte e um é decisão de jogo, não conserto.

### `ForceMorale*` só pegava na segunda chamada

`sub_4229F0` (casos 21..25) faz `mascara ^= bit` nas **cinco** máscaras de nível
e depois liga o bit na escolhida. Como o bit começa apagado, a primeira chamada
o **liga nos cinco** — e quem lê (`sub_469730`) devolve o **primeiro** nível cuja
máscara tem o bit, isto é `Great`, qualquer que tenha sido o pedido. Só a
segunda chamada acertava, porque aí o XOR apagava os cinco antes do OR.

Trocado por apagar o bit em todas e ligar na escolhida, que é o que o XOR
queria dizer. O `--colonytest` agora exige que a **primeira** `ForceMoraleGood`
já dê o nível 1; com o XOR de volta ela dá 0 (`Great`).

### O "perma-wall": a reserva que ficava para sempre

Ao começar um muro, o Earthworker marca o **bit 31** da célula (`sub_4235E0`) e
só o tira ao assentar. Se ele morre no meio, ninguém tira: fica um bloqueio
**invisível** na célula, para sempre. Era o único caminho no porte que escrevia
esse bit sem par.

Entrou `earthworkAbort(id)`, chamada no `killUnit`. Ela só larga a célula se a
reserva ainda estiver lá **e** nada tiver sido assentado — se a célula já virou
muro, o bit 31 é do muro e não pode sair. O teste tem esse controle.

### O muro assentava por cima de quem estivesse no tile

Na hora de assentar, o original só cancela e devolve o metal quando é **tubo**;
o muro era posto por cima de outra unidade. Agora os dois cancelam. O teste do
muro não pode ser o `wallCellOk` — a própria reserva do bit 31 já o reprovaria
—, então o que vale é se **outra** unidade ocupa a célula.

### Os testes

Três casos novos no `--walltest`, pelos pontos de entrada de verdade
(`killUnit` e `wallBuildTick`): a reserva sai na morte, o **muro pronto
sobrevive** (controle) e o tile ocupado cancela, devolve o metal e larga a
reserva. Neutralizando: sem o `earthworkAbort` **1** falha; com o cancelamento
só do tubo, **1**. No `--colonytest`, a moral forçada dá **1**.

### Os que NÃO se aplicam

- **Míssil que não pousa para Eden**: as duas entradas Eden do míssil EMP na
  tabela de animação (`dword_4E3FE0`) são **0**, e é daí que vem o defeito
  deles. O porte já atravessa isso: `lastFrameOr1` devolve 1 quadro quando a
  animação é 0, então a contagem desce e o míssil pousa.
- **Overflow em `ScGroup::TakeUnit`**: o `aiTakeUnit` já tem a guarda dos 32 com
  `std::vector`, e manda a unidade para fora do mapa quando o grupo está cheio,
  que é o que o original quer fazer.

### Decisão, não conserto

**Carga parcial do caminhão.** Hoje o `L&OAD` só aparece com estoque suficiente
para encher o caminhão (`truckCap(v) <= pd.ore`). O OPUPatch deixa carregar
parcial e, com isso, muda como os objetivos de missão contam a carga. É
mudança de jogo: fica para quando for decidido.

---

## 5.13. Os últimos da lista do OPUPatch (2026-11-09)

Com estes, **os nove que faltavam conferir estão fechados**. Nenhum ficou por
afirmar sem abrir o mecanismo.

### `DoInfect` não fazia nada com mais de um jogador

```asm
mov     eax, dword_56DEC0
cmp     eax, 1
jg      loc_476C3F          ; mais de um jogador -> sai
```

O porte transcreveu isso fielmente. Como **quase toda missão de campanha tem
dois ou três jogadores**, o `DoInfect` das missões era um `return` disfarçado —
a praga nunca infectava prédio nenhum por chamada de script. Nada no corpo da
função depende de haver só um jogador: a guarda saiu.

Caso novo no `--colonytest`, com o número de jogadores como **controle**: sem
ele o caso não mediria nada. Repondo a guarda: 1 falha.

### O destroço virava entulho

Na coleta (ação 0x0F), o original testa o **entulho primeiro**:

```c
if (rubbleTile(tile, false))      { kind = 6; ... }   // entulho comum
else if (rubbleTile(tile, true))  { kind = 7; ... }   // entulho raro
else if (wreckAtTile(u))          { kind = 9; ... }   // destroço
```

Um destroço posto num tile de entulho era carregado **como entulho** — e pior,
o tile era terraplanado em seguida (`kind != 9`), levando o destroço junto.
Destroço é peça única de missão e entulho tem em todo canto: o destroço passou
a vir antes.

Caso novo no `--salvagetest`: destroço sobre entulho dá carga tipo 9 e o tile
**continua** 0x16; o controle (só entulho) segue dando 6. Com a ordem do
original: carga 6 e o tile terraplanado para 0x15.

> O `--salvagetest` **devolvia 0 sempre**. Imprimia o `FALHOU` e passava — a
> mesma armadilha que o `--chattest` já tinha pregado. Agora reprova.

### Os dois que não se aplicam, agora com teste

- **Míssil mirado em cima de uma Meteor Defense**: no original as defesas
  vizinhas o ignoravam. Aqui a distância é medida da posição do **míssil**,
  então em cima da defesa dá zero e ela engaja. Caso novo no `--mdtest`.
- **Transferir unidade para a Gaia** (jogador 6): trava o original. Aqui o
  caminho é o mesmo dos outros jogadores e os vetores têm `kMaxPlayers = 7`
  posições, então 6 é índice válido. Caso novo no `--transfertest`, que
  confere o dono **e** que a simulação segue depois.

### O placar da lista

| | |
|---|---|
| Corrigidos | míssil de aliado, muro no 3×3, perma-wall, muro sobre unidade, `ForceMorale*`, `DoInfect`, destroço virando entulho, avisos de transferência e de energia/estoque |
| Não se aplicam ao porte | EMP para Eden, míssil em cima da defesa, `ScGroup::TakeUnit`, transferir para a Gaia, alerta de aliança no início |
| Achado nosso, não deles | o relógio do filtro de mensagens (5.11) e a ausência de teto de unidades (5.10) |
| Decisão de jogo | carga parcial do caminhão (feita, 5.14) e cheats desligados em rede/multijogador (feito, 5.16); mudanças de equilíbrio ficam de fora |

---

## 5.14. Carga parcial do caminhão (2026-11-09)

A última da lista do OPUPatch, e a única que era **decisão de jogo** e não
conserto. Decidida: fazer como o patch.

### Carregar o que houver

O original só deixa carregar o caminhão **cheio**: o `L&OAD` do painel STORAGE
pede `truckCap(v) <= estoque`, e o `sub_423F70` repete a conta. Com 900 de
metal no depósito e um caminhão de 1000, não havia como tirar aquilo de lá.

Agora o botão acende com qualquer estoque e o pedido é `min(capacidade,
estoque)`. Três lugares, porque o caminho tem três conferências:

| onde | o que mudou |
|---|---|
| `storageLoadAvailable` | acende com `estoque > 0` |
| `applyLoadCargo` | pede `min(cap, estoque)` |
| `truckDockExchange` | se o estoque caiu entre a ordem e a atracação, leva o que sobrou em vez de **zerar** a carga |

A comida, no Agridome, seguia o mesmo caminho e recebeu o mesmo tratamento.

### O objetivo passa a valer pela quantidade

Com carga parcial, **contar caminhões não serve mais**: cinco caminhões podem
somar 4900. Como no patch, o gatilho de contagem para caminhões com filtro de
carga passa a valer pela quantidade — o `count` que a missão pede vale em
caminhões **cheios**:

```
contagem = soma das cargas / capacidade do caminhão
```

Cinco caminhões com 4900 dão **4**, e o objetivo não fecha; completando os 100
que faltam, dá 5 e fecha. É exatamente o exemplo do changelog deles. Vale nos
dois contadores (`countForPlayer`, que também olha as garagens, e
`countAccumulate`); sem filtro de carga (`cargo == -1`) nada muda, continua
contando caminhões.

### Os testes

- `--oretest`: depósito com meio caminhão de metal — o `L&OAD` acende, o
  caminhão sai com exatamente aquilo e o estoque vai a zero.
- `--garagecounttest`: cinco caminhões somando 4900 **não** disparam o
  objetivo de "5 caminhões de metal comum"; completando para 5000, dispara.
  O caso fica por **último** no teste: cinco caminhões a mais mudariam a
  contagem do `VehicleCountTrigger`, que vem antes.

Neutralizando: `L&OAD` exigindo caminhão cheio **1** falha; objetivo voltando a
contar caminhões **1**.

> O gatilho só nasce com **missão** carregada — `createFuncRef` precisa
> resolver o nome da função no script. Num teste que abre só o mapa ele devolve
> `0xFF` e o `triggerCheck` responde `false` para sempre: foi por isso que o
> caso do objetivo mudou de casa.

---

## 5.15. O relógio do tique, e o HUD que sorteava (2026-11-09)

Duas divergências de **simulação** do porte, achadas comparando-o com o
original cena a cena num banco de provas (`--harness`, que lê um roteiro, roda
a partida sem janela e grava `trace.jsonl` com os mesmos campos do original).

### O tique subia cedo demais

O original levanta o contador **no fim** do `ProcessGameCycle`, depois do laço
de unidades:

```asm
call    sub_43AD30          ; 0x48A01B  o laço das unidades
...
inc     [GameImpl+0x84]     ; 0x48A020..0x48A02D  só agora o tique sobe
```

Ou seja: economia, gatilhos, pacotes de comando e unidades **todos veem o tique
anterior**. O porte subia primeiro, então cada porta do tipo `(id ^ tick) & 7`
abria um tique adiantada — a varredura das torres ociosas ficava fora de fase e,
num duelo, o outro tanque procurava alvo antes. O `advanceTick` foi para depois
do laço, e o `netApplyTurn` passou a receber `tique + 1` para o número do turno
não mudar.

### O HUD de depuração sorteava um número por tique

O campo "Rand" do HUD chamava `getRand(100)` **a cada tique**. O original não
sorteia nada ali — e cada sorteio desloca a sequência inteira, então todo
número tirado depois (dano de arma, por exemplo) saía diferente do original. O
HUD agora **lê o estado** do gerador em vez de sortear.

Com as duas, os oito duelos do banco batem com o original em todos os campos
rastreados e no RNG, tique a tique; no `line_fast` a única diferença que resta é
o tique em que o estado da rota é liberado (731 aqui, 732 lá).

> Nenhum `--savetest` pegaria isto: ele compara o porte **com ele mesmo**, e as
> duas divergências são coerentes consigo. Só a comparação com o jogo de 1997
> mostra.

### No mesmo lote

- **As plataformas de doca** das minas, do Magma Well, dos tanques e do
  Agridome passaram a sair do passe de "camada de chão" do original (slot 30 da
  vtable do prédio), com a animação certa por tipo e por dano — no lugar da
  plataforma posta à mão em 2026-11-07, que não mostrava dano.
- **Os respiradouros de magma** (tipo `0x52`) nunca eram desenhados; entram no
  mesmo passe (animação `0x2A2`), e somem quando há um Magma Well em cima.
- **A escala de noite dos sprites** usava a rampa inteira do original enquanto o
  terreno segue a chave `NightDarkness` — o respiradouro saía mais escuro que o
  chão em volta. Agora os dois usam a mesma escala.
- **O minimapa abre no zoom 0** (encaixe) a cada mapa carregado, como o
  `MiniMap::InitializeForMap`; antes abria no 1 e um mapa pequeno virava um
  quadradinho no meio da caixa.
- **Clique direito numa doca sempre leva o veículo até lá.** No original, com
  um veículo que não pode usá-la (um caminhão carregado numa mina, que só
  aceita vazios), o clique era ignorado e o caminhão não saía do lugar.
- O `--dockclicktest` ganhou o tipo de prédio como argumento (`15`/`16`/`2A`
  para as minas, `19`/`1A` para os tanques, `27` para o Agridome — o padrão).
  Vale lembrar que ele sai com **3** quando a missão escolhida não tem aquele
  prédio: é premissa, não falha.

---

## 5.16. Cheats só em jogo solo (2026-11-09)

O original não tem porteiro: a frase digitada no chat funciona em **qualquer**
partida, e como o reconhecedor roda no pacote 0x30 em todas as máquinas, numa
partida em rede um jogador enche o próprio estoque e estraga a dos outros. (A
`CanAllowCheats` existe no executável, mas é só exportada para as DLLs de
missão — o reconhecedor não a consulta.)

Como o OPUPatch, e por decisão do usuário, **neste porte eles valem só em jogo
solo**. O `cheatCheckChat` começa por:

```c
bool cheatsAllowed() { return !netActive() && !multiplayerMission(); }
```

— rede **ou** cenário multijogador desliga. Nenhuma das duas perguntas depende
de quem está perguntando, então todas as máquinas respondem igual e **o
lockstep não corre risco**; a frase simplesmente segue como mensagem comum de
chat, sem virar "Woof Woof!".

O porteiro mora no **reconhecedor**, e não na linha de conversa: assim vale
para qualquer caminho que chegue ao pacote de chat, não só para o que foi
digitado.

### Os testes

- `--cheattest <DLL>`: em jogo solo, os efeitos de sempre; num cenário
  multijogador, as quatro frases **não** são reconhecidas, o texto **não** vira
  "Woof Woof!" e nem o estoque nem os trabalhadores se mexem.
- `--chattest <DLL>`: o caminho inteiro, da tecla ao efeito — enche o estoque
  em jogo solo e não faz nada em multijogador.

Tirando o porteiro, o multijogador reprova (**4** falhas no `--cheattest` e
**1** no `--chattest`) e o jogo solo **continua passando**, que é a assinatura
certa: a mudança só alcança a rede.

---

## 5.17. A rolagem da vista: borda do mouse e setas (2026-11-09)

O porte rolava a vista só pelas setas, a 600 px por segundo tirados do nada. O
original tem a coisa inteira em **`sub_407A10`**, e é ela que está portada agora
— **a borda da janela e as setas pedem a mesma coisa**, pelo mesmo caminho.

### O passo

```
passo = (FrameSkip * ScrollRate * 20) / (GameSpeed * 4)     ; mínimo 1
```

`Value` é o **FrameSkip** e `dword_56DEA8` é o **GameSpeed × 4** — os dois
conferidos no disassembly (`[esi+388h] = dword_56DEA8 / 4` é o que a tela
`PREFERENCES` mostra como *Game Speed*). O `ScrollRate` é a chave do INI que o
porte **lia e não usava**: agora é ela que manda, e a opção *Scrolling Speed*
(1..6) do `PREFERENCES` finalmente faz efeito.

### A aceleração

Cada eixo tem a sua velocidade (`[+0x14C]` em X, `[+0x148]` em Y):

| situação | o que acontece |
|---|---|
| alguém pede o lado | a velocidade anda `passo` por quadro e **satura em `passo × 32`** |
| pede o lado contrário | antes de somar, ela **salta `passo × 4`** na direção do zero — freia rápido em vez de virar de uma vez |
| ninguém pede | volta para zero de `passo × 4` por quadro, e a vista **ainda desliza** enquanto isso |

Detalhe fiel: o original usa **um único** sinalizador de "houve pedido" para os
dois eixos, então pedir um eixo segura a desaceleração do outro.

### A borda

É a da **janela** com 2 px de margem (`GetWindowRect` menos 2), não a da vista
do mapa: no original o jogo ocupava a tela e era o canto do monitor que rolava.
A esquerda ganha da direita e o cima ganha do baixo — o original testa um e só
olha o outro se o primeiro não pediu. As setas entram pelo mesmo teste
(`GetAsyncKeyState`), então valem junto com o mouse.

**O minimapa acompanha sozinho**: ele é redesenhado a partir da câmera a cada
quadro, e o retângulo da vista anda junto.

### Duas diferenças conscientes

- Só rola com o ponteiro **dentro** da janela e com ela em primeiro plano. O
  original usa `GetCursorPos`, que rola até com a janela atrás de outra.
- Não rola no demo de atração, onde a câmera já é de quem passeia.

### O teste

**`--scrolltest <mapa>`**, 24 casos, com a conta e a regra da borda separadas da
leitura do SDL para poderem ser medidas sem mouse: a margem de 2 px (x=2 rola,
x=3 não), os quatro sentidos, o canto pedindo dois eixos, ponteiro fora da
janela, as setas, a esquerda ganhando da seta direita; e do lado da conta, o
passo da fórmula, a saturação em `passo × 32`, o salto de `passo × 4` ao trocar
de lado, o deslize depois de soltar e a parada. O último caso compara os
**pixels do minimapa** antes e depois de rolar — é assim que se prova que ele
acompanha.

Neutralizando: margem de 2 px **1** falha, setas **1**, saturação **1**, câmera
ignorando a velocidade **2**.

> Duas premissas de teste que custaram rodada: a câmera posta em 2000/3000 num
> mapa de 2048 px fica presa no `clampCamera` e não anda; e a velocidade
> **sobra do caso anterior**, então o caso do deslize precisa deixar a vista
> parar antes de começar.

---

## 5.18. A área de clique das unidades (2026-11-09)

Para marcar um prédio era preciso acertar um alvo pequeno no meio dele. A causa
estava à vista no código: o clique **e** o realce usavam um **raio de 20 px do
centro**, igual para tudo —

```c
int best = 0, bestD2 = 20 * 20;   // raio de tolerância
```

— então num Command Center, de **3×2 tiles** (96×64 px), só o miolo respondia.
Medido: com o raio, **2 dos 6 tiles** marcavam o prédio, e o canto não marcava
nunca.

### A caixa certa

Agora cada unidade tem a sua caixa (`GameState::unitHitBox`):

| | área que responde |
|---|---|
| prédio | a **pegada inteira** — as mesmas `w × h` células que ele marca no mapa ao nascer (`createUnit` preenche a ocupação de `(cx − w/2, cy − h/2)` por `w × h`) |
| veículo | 48 px em volta do centro, da ordem do sprite (o tile tem 32) |

Quando duas caixas se sobrepõem — um caminhão parado sobre a doca, por exemplo
— ganha a unidade cujo **centro** está mais perto do clique. O realce (`hover`)
passou pelo mesmo caminho, então o que se destaca é o que vai ser marcado.

### O teste

**`--selecttest <mapa>`**: um Command Center num canto livre, e o clique no
**centro de cada tile da pegada** tem de marcá-lo — mais o canto do prédio, que
era o caso que nunca funcionava. Dois controles negativos (um tile fora da
pegada e um ponto longe do veículo) e o realce vendo o prédio pelo canto.

Voltando ao raio de 20 px: **4 falhas**, com os números do defeito à mostra
(`clicar em qualquer tile do predio marca 2 (esperado 6)`).

### Um teste que dependia do INI do usuário

No caminho, o `--optmenutest` começou a reprovar em dois casos — e não por causa
desta mudança: o `GameSpeed` do INI estava em **10**, o teto, e o `&MORE` não
tinha para onde subir. O caso media o INI de quem roda, não o porte. Agora ele
leva o valor ao piso e sobe um antes de medir, então os dois botões têm folga
venha o INI como vier.

---

## 5.19. Os cursores de comando (2026-11-09)

Os 17 `.ANI` da pasta do jogo estavam decodificados desde setembro e **nunca
tinham sido usados**: o porte jogava com a seta do sistema em tudo. Agora eles
estão no lugar.

### De onde vem o "qual cursor"

Do `sub_488900`, que carrega a tabela de **`0x4E9840`**: as três primeiras
entradas são cursores do Windows (`0x7F02` WAIT, `0x7F03`, `0x7F00` ARROW) e o
resto são os arquivos, achados pelo `ResManager::GetFilePath` e com a seta como
recuo quando faltam. A ordem da tabela é o que diz qual cursor é de qual
comando:

| 3 | 5 / 6 | 7 / 8 | 9 | 11 / 12 | 13 / 14 | 15 / 16 | 17 / 18 | 19 | 22 | 24 |
|---|---|---|---|---|---|---|---|---|---|---|
| destruct | move / no_move | attack / noattack | bulldoze | build / nobuild | dock / nodock | repair / norepair | reprog / no_repro | guard | salvage | waypoint |

Quase todos vêm em **par**: o "pode" e o "não pode". No original a escolha é um
`cmp eax,1 / adc` sobre o teste de validade — por exemplo o `sub_4515xx` pega
13 (`dock`) ou 14 (`nodock`) conforme o veículo poder usar aquela doca. Aqui o
teste é o **mesmo que o comando já usa**: `uiCommandCellOk` nos comandos de
área, a unidade sob o ponteiro nos que miram unidade, e o `vehicleCanDock` na
doca.

### O leitor

Os `.ANI` são cursores animados do Windows, formato público RIFF/ACON: `anih`
(cadência), `rate` (por passo), `seq ` (ordem) e uma LIST `fram` de chunks
`icon`, cada um um `.CUR` inteiro (ICONDIR + BITMAPINFOHEADER + máscara XOR +
máscara AND). `src/res/ani_cursor.cpp` lê isso e devolve os quadros em ARGB,
com o ponto quente. Nada aqui é engenharia reversa — o que veio do disassembly
foi só a tabela acima.

Os 17 lidos: todos 32×32, de 2 a 8 quadros, pontos quentes onde se espera
(ataque no centro `(16,16)`, bulldoze no canto `(1,27)`, doca em `(13,9)`), e
cinco deles (ataque, reparo, waypoint e os "no") usam o `seq ` para repetir
quadros fora de ordem.

### Os testes

- **`--anitest`**: lê os 17 e exige de cada um tamanho e ponto quente sãos,
  pixel **opaco** *e* pixel **transparente** em cada quadro, e todo passo
  caindo no quadro que o `seq ` do arquivo manda.
- **`--cursortest <mapa>`**: o cursor que aparece em cada situação — mover em
  chão livre e em chão fechado, atacar sobre inimigo e sobre o vazio, reparar o
  meu machucado e o meu inteiro, reprogramar, terraplanar, guardar, patrulhar,
  autodestruir e erguer muro; desde a 5.42, também o comando padrão (com
  seleção e sem mira) e o clique direito.

Neutralizando: o par do ataque virando só "attack" **1** falha; a máscara AND
do `.CUR` ignorada **17** (todo cursor vira um quadrado sólido); o `seq `
ignorado **5** (os cinco arquivos que têm mais passos que quadros).

> As duas últimas só passaram a ser pegas depois de endurecer o `--anitest`: a
> primeira versão só exigia "um pixel opaco por quadro", e as duas mutações
> passavam. O `frameOfStep` tranca em zero o índice inválido, então conferir
> que o quadro *existe* não vê um `seq` ignorado — é preciso comparar com a
> sequência do arquivo.

### O que ficou de fora

O cursor de espera (`0x7F02`) nas trocas de tela e o `0x7F03`; e o `salvage`,
que já tem comando de área mas cujo cursor o porte ainda não distingue do
`bulldoze` em todas as situações.

---

## 5.20. O ESC, e o relógio do demo de atração (2026-11-09)

Duas coisas que o porte fazia e o jogo não faz.

### O ESC largava a missão na hora

`handleEvent` tinha uma linha só: `if (e.key.keysym.sym == SDLK_ESCAPE)
gTApp.activateShell();`. Qualquer toque no ESC — com uma mira de comando em
curso, com o diálogo de salvar aberto, no meio de uma campanha de duas horas —
jogava direto no menu principal, **sem perguntar** e sem passar pelo caminho de
saída do jogo (o pacote `0x31`, que é quem encerra a partida de quem sai).

Agora o ESC é uma **cadeia de desistências**, da mais interna para a mais
externa, e cada uma sai com `return`:

| ordem | o que está no ar | o que o ESC faz |
|---|---|---|
| 1 | o diálogo de slots (Ctrl+S / Ctrl+L) | fecha |
| 2 | uma confirmação `&YES`/`&NO` | fecha — é um "não" |
| 3 | o escolhedor de jogador (transferir) | cancela |
| 4 | uma mira de comando (mover, atacar...) | desiste da mira |
| 5 | nada | **pergunta** se é para sair |

A pergunta não é inventada: é a tela de fim de missão do próprio jogo. O ESC
leva para a aba **Options**, abre o `ENDMISSION` e põe em cima a confirmação
com a frase do executável — *"Are you sure you want to abort the mission?"* —
ligada ao mesmo comando do `&BACK TO MAIN MENU`. Um `&YES` ali sai pelo caminho
de sempre; um `&NO` (ou outro ESC) volta para a partida.

Em **multijogador** não existe "voltar ao menu": a tela de fim de lá só tem
`SELF-&DESTRUCT` e `&TRANSFER`. Então ali o ESC abre a tela e **não** pergunta
nada.

A confirmação nasce com o `fromOptions` ligado (o `[+0x15C] == unk_565AC8` do
original): sem ele, ela morre sozinha no quadro seguinte por falta de seleção —
era exatamente o defeito da seção 5.7.

### O demo de atração entrava assim que o menu voltava

`ShellMenu::update` chama o demo quando `SDL_GetTicks() - m_lastActivity >
60000`. O `m_lastActivity` guardava o instante da **última mexida no menu** — e
entrar numa missão não mexe no menu. Depois de um minuto jogando, voltar ao
menu (pelo ESC ou pelo fim da partida) caía **direto** no demo, antes mesmo de
dar para ler a tela.

`ShellState::onActivate` agora começa com `m_menu.resetIdleClock()`: toda vez
que o menu volta, o relógio recomeça do zero. O `setState` não zerava nada, e
voltar de uma partida nem troca de estado (o menu já estava em `MainMenu`), por
isso não havia nenhum outro ponto em que isso acontecesse por acaso.

### Os testes

- **`--optmenutest <DLL>`**: sete casos novos. O ESC entra pela **tecla de
  verdade** (um `SDL_KEYDOWN` pelo `handleEvent`), não chamando a função por
  dentro — sem isso, desligar o ESC no tratador passava despercebido. Mede: a
  aba vira 5, o título é o da tela de fim certa para o tipo de partida, a
  pergunta é a frase do executável, ela sobrevive ao quadro seguinte com
  `&YES`/`&NO`, o segundo ESC a fecha, e com mira em curso o ESC só desiste da
  mira. Em multijogador, o caso espelhado: abre a tela e não pergunta.
- **`--demotest`**: monta um `ShellState` de verdade e mede os dois lados —
  com o relógio velho e **sem** passar pelo `onActivate` o demo entra (senão o
  caso não mediria nada); passando pelo `onActivate`, não entra.

Neutralizando, uma peça de cada vez (solo / rede / demo): o ESC voltando a
largar a partida **19 / 15 / 0**; o ESC abrindo a tela sem perguntar **4 / 0**
(em rede é o comportamento certo, então zero); a mira não sendo desfeita
**7 / 1**; a pergunta no ar deixando de ser um "não" **8 / 0**; a confirmação
nascendo sem o `fromOptions` **4 / 0**; e o `resetIdleClock` fora do
`onActivate` **0 / 0 / 1**.

---

## 5.21. O letreiro de fim de missão (2026-11-09)

"MISSION ACCOMPLISHED" e "MISSION FAILED" saíam na **fontinha de depuração do
porte**, amarelos e pequenos. No original são grandes e **vermelhos**.

### De onde vem

O texto é escolhido pelo `sub_49CAC0` (a tabela de códigos que o porte já
tinha) e entregue ao controle **`TFrame+0x1F4`**, um filho transparente
(`WS_EX_TRANSPARENT`) que serve de letreiro — é o mesmo que escreve
*"Game Paused. Press CTRL+P to Resume"*. Quem desenha são duas rotinas:

**`sub_4084D0`** cria a fonte uma única vez e a guarda em `[+0x130]`:

| campo | valor |
|---|---|
| `lfHeight` | **0x30** (48) — positivo: é a altura da **célula** |
| `lfWeight` | **0x2BC** (700, negrito) |
| `lfFaceName` | `Arial` |
| `lfCharSet` / `lfOutPrecision` / `lfPitchAndFamily` | 1 / 7 / 0x52 |

Os mesmos três últimos das outras faces do jogo, que o porte já reproduz.

**`sub_4085C0`** desenha. `SetBkMode(TRANSPARENT)`, um `DrawText` com
`DT_CENTER|DT_WORDBREAK|DT_CALCRECT` para **medir** a altura, a centragem
vertical pelo que ele mediu — e então **cinco passadas**:

1. `SetTextColor(0)` — preto — e o texto nos quatro cantos diagonais de **±2
   px** (o retângulo anda `+2,+2`, depois `+2,−2`, `−2,−2`, `−2,+2`). É o
   contorno.
2. `SetTextColor(0xFF)` — o `COLORREF` `0x0000FF`, ou seja **vermelho puro** —
   e o texto de volta no lugar certo.

O porte faz exatamente isso: uma sexta face no `UiFont` (`kBanner`, Arial
negrito de célula 48) e as cinco passadas sobre a vista.

### O teste

`--bannertest <DLL> [saida.bmp]` mede o letreiro **no quadro desenhado**, e por
**diferença**: desenha um quadro sem o letreiro, guarda, põe o código de fim e
desenha outro. Só os pixels que mudaram são o letreiro — nem o terreno nem o
painel entram na conta. Com `dt = 0` nenhum turno avança, e o primeiro caso
confere justamente isso (dois quadros parados são idênticos), senão a diferença
não mediria nada.

Para os códigos 3 e 4: há pixels vermelhos; a caixa deles tem **30 px ou mais
de altura** (a fonte do porte dava 10); a caixa preta é a vermelha **dilatada em
exatamente 2 px** de cada lado, que é o que as quatro passadas produzem; não
sobrou nenhum pixel do amarelo antigo; e a caixa está centrada na vista. O
último caso compara os dois códigos: textos diferentes dão larguras diferentes.

Neutralizando: o amarelo de volta **13** falhas; sem o contorno **2**; a face
dos títulos no lugar da do letreiro **2**; o contorno a 3 px **2**; o letreiro
fora do centro **2**; e a face do letreiro criada com corpo 16 **2**.

> O tamanho do quadro só vale **depois** do primeiro `onIdle` — é o
> `fitToWindow` que o acerta. Medindo antes, a varredura usava 640×480 num
> quadro de 1280×960 e não via nada: os quatorze casos reprovaram com o
> letreiro desenhado corretamente na tela.

### O que ficou de fora

O mesmo controle escreve *"Game Paused. Press CTRL+P to Resume"*, e o porte
ainda não tem a pausa do `Ctrl+P`. Quando tiver, o letreiro já está pronto.

---

## 5.22. A pausa do Ctrl+P (2026-11-09)

O porte não tinha pausa: a tecla não fazia nada e o letreiro *"Game Paused.
Press CTRL+P to Resume"* — que é o **mesmo controle** da seção anterior — nunca
aparecia.

### A tecla

Despejando a tabela de aceleradores **130** do `OUT2RES.DLL` (RIFF de recursos,
`RT_ACCELERATOR`), o comando da pausa é o **40001**, e ele tem **duas** teclas:

| tecla | comando |
|---|---|
| `CTRL+P` | 40001 |
| `PAUSE` (VK 0x13) | 40001 |

Os dois estão ligados. (Na mesma tabela: `CTRL+S` = 40017 e `CTRL+L` = 40018,
que o porte já tratava.)

### O que o comando faz (`sub_49891C`)

1. **Partida em rede** (`dword_56DC78`, o objeto da camada de rede): não pausa.
   Só o aviso na barra de estado — *"Cannot Pause in multiplayer games"*, pelo
   `sub_49BF10`, que é a barra direta, sem entrar no log nem tocar som. No
   porte a mesma regra vale também para uma missão multijogador rodando
   sozinha, que no original não existe.
2. **Antes do primeiro tique** (`dword_56DEDC == 0`): a tecla não faz nada.
3. Senão, alterna: `sub_49BFE0(paused ? 0 : 1)`.
4. E, conforme o contador ficou em 1 ou em 0, põe ou tira o texto do letreiro.

### A pausa em si (`sub_49BFE0`)

É um **contador** — `TFrame+0x1E0` —, porque o jogo também se pausa sozinho
noutras situações; só a subida de 0 para 1 e a descida de volta a 0 é que
agem. Na subida:

| | |
|---|---|
| `sub_450A90` | para a **música**: mata o `timeKillEvent` do sequenciador |
| `sub_47E6F0` | **suspende o som**: solta os canais que estão tocando (é outro contador, `[+0x91C]`) |
| `TApp::OnPauseGame(1)` | liga `TApp+0x178` (`dword_56DDA0`, o flag global) e **larga a captura do mouse** |

Na descida, o caminho inverso — e o original ainda devolve os campos de tempo
do quadro (`[+0xC0] = [+0xBC]`, `[+0xC4]` e `[+0xCC]` zerados) para o jogo não
disparar em câmera rápida ao voltar. Aqui isso já está feito: o grampo do
`onIdle` zera o acumulador do tique **a cada quadro** enquanto a pausa está de
pé, e o último quadro antes de soltar é um deles.

> Essa linha chegou a existir no `pauseGame`, repetindo o original. Uma
> mutação mostrou que ela era **código morto** — tirá-la não reprovava nada,
> porque o grampo do quadro já faz o serviço. Saiu, com o comentário dizendo
> onde o reset mora.

### Uma correção de caminho

Os aceleradores liam os modificadores com `SDL_GetModState()`, que responde
pelas teclas de **agora**, não pelas da batida. Agora saem do próprio evento
(`e.key.keysym.mod`), que é o que um acelerador quer dizer — e é também o que
torna a tecla testável sem um teclado de verdade: com o `GetModState`, um
`SDL_KEYDOWN` montado à mão nunca tinha Ctrl.

### O teste

`--pausetest <DLL> [saida.bmp]`: 23 casos em jogo solo e 9 em multijogador. A
tecla entra pelo `handleEvent`, como a de verdade. Mede: antes do primeiro
tique não pausa; o relógio anda antes e **para** na pausa (com `dt` generoso,
senão o caso não mediria nada); o arrasto que estava no ar é solto; o som é
suspenso uma vez; o letreiro aparece, grande, com contorno, e é **o texto da
pausa** e não o do fim (a largura entrega); uma segunda pausa **empilha** sem
suspender o som de novo, e uma despausa só não solta o jogo; o Ctrl+P de volta
solta, sem disparar os tiques; e, pausado **e** com fim de missão, quem aparece
é o letreiro do fim. Em rede: não pausa, não há letreiro, o relógio continua, e
o aviso sai na barra.

Neutralizando, uma peça de cada vez (solo / rede): a tecla sem efeito
**12 / 1**; os turnos continuando **1 / 0**; sem letreiro **4 / 0**; pausando
também em rede **0 / 5**; sem suspender o som **2 / 0**; o contador sem
proteger a subida **2 / 0** e sem segurar a descida **6 / 0**; a captura não
solta **1 / 0**; o letreiro da pausa ganhando do fim de missão **1 / 0**; e a
tecla valendo antes do primeiro tique **18 / 0**.

---

## 5.23. Rota de minério automática (2026-11-10) — **acréscimo deste porte**

Isto **não existe no original**. É o sexto extra do porte, ao lado de
`AILevel`, `NightDarkness`, `AudioVolume`, `DemoCamera` e `Fullscreen`, e como
eles mora numa chave do `OUTPOST2.INI`:

```ini
[Game]
AutoOreRoute=1      ; 1 (padrão) = o atalho; 0 = só o comando de três cliques
```

### O que faz

Com **caminhões de carga vazios** selecionados, o **clique direito na doca de
uma mina** monta a rota inteira: escolhe a fundição e manda o ciclo. Vale para
as três fontes de mineral e para as duas raças. O comando original de três
cliques (`S&et an ore-hauling route`) continua ali, inteiro, com a chave
ligada ou desligada.

### Por que custou pouco

O clique direito numa doca **já mandava o caminhão atracar**: esse caminho
passa pelo `uiCommandTarget(UC_MOVE, …)`, que reconhece a doca e troca o
"mover" pelo pacote de atracagem. Faltava só, ali, mandar o pacote **`0x27`**
em vez do de doca. `dockCell`, o casamento mina↔fundição do `oreRouteTarget` e
o `sendOreRoute` já estavam prontos.

E nada muda no motor: a rota montada é **o mesmo pacote `0x27`** dos três
cliques, com os mesmos três pontos (mina, fundição, mina). Lockstep de rede,
jogo salvo e determinismo ficam como estavam.

### A escolha da fundição — `nearestSmelterFor`

É a peça nova de verdade. Varre os prédios **do mesmo dono** e filtra pelo
tipo que casa com a mina — o casamento vem **antes** da distância:

| Mina | Fundição |
|---|---|
| `0x15` comum | `0x36` `SMELTER` |
| `0x16` rara | `0x38` `SMELTER_ADV` |
| `0x2A` Magma Well | `0x38` `SMELTER_ADV` |

Entre as compatíveis, duas camadas:

1. a mais próxima **disponível no momento** — operando **e** com espaço no
   estoque. É exatamente a condição do `truckDockStatus` para o caminhão
   **não** ficar esperando na doca (`structureOperational(f) &&
   smelterCanAccept(f, 1)`), então o atalho escolhe onde ele vai conseguir
   descarregar;
2. se nenhuma estiver disponível, a mais próxima de todas: a rota ainda é
   montada e o caminhão espera nela, como no comando de três cliques.

A distância é medida **entre as docas** (é por onde o caminhão entra), com o
wrap em X.

> **Minério bruto só descarrega em fundição.** O `vehicleCanDock` diz que o
> Storage Tanks (`0x19`/`0x1A`) aceita só **metal já fundido** (cargas 4 e 5);
> minério bruto (2 e 3) só entra no `0x36`/`0x38`. Por isso "o depósito que
> aceita o minério" é, no jogo, a fundição.

### As duas raças

`SMELTER` e `SMELTER_ADV` têm `Owner = B` na planilha: são os mesmos prédios
para Eden e Plymouth. O que muda é a **doca** — a planilha traz duas colunas,
`dockLocE` e `dockLocP`, e na fundição de minério raro elas são **diferentes**
(`4` e `1`). Quem resolve é o `dockCell`, que já escolhe a coluna pela raça do
dono.

### Quando o atalho desiste

Sem caminhão de carga vazio na seleção, ou com o clique fora da **doca** de uma
mina do jogador, ou sem nenhuma fundição compatível viva, o atalho devolve
`false` e o clique volta a ser o de sempre — o caminhão vai, atraca e carrega.

### O teste — `--autoroutetest <mapa>`

36 casos, em dois cenários.

**Eden, minério comum.** Uma mina e quatro fundições: uma comum perto (ligada
ao CC), uma de minério **raro** ainda mais perto, uma comum longe e uma do
**vizinho** mais perto que todas. Mede: a escolhida é a comum perto; a
**disponível** ganha da mais próxima parada; com **duas** disponíveis ganha a
mais próxima; sem nenhuma disponível — inclusive com o estoque cheio — fica a
mais próxima de todas; a do vizinho é pulada, **também no recuo**. O clique
direito entra pelo `handleEvent`, com um `SDL_MOUSEBUTTONDOWN` de verdade
posicionado pelo `nativeToScreen`; fora da doca (no **outro** tile da mina,
que é 2×1) ele não monta rota. E o ciclo roda 4000 tiques: **10 cargas, 4754
de minério no estoque**, caminhão visto cheio e vazio — é o que separa
"atracou e carregou" de "está em ciclo".

**Plymouth, minério raro e Magma Well.** Beacon raro → mina `0x16`;
respiradouro → Magma Well `0x2A`; os dois vão para a fundição de raro, mesmo
com uma comum mais perto; a doca da fundição de raro **muda com a raça**; e o
ciclo do raro roda: **4 cargas, 0 → 1020** de minério raro.

Neutralizando, uma peça de cada vez: tipo ignorado **15**; a mais longe na
camada das disponíveis **1**; o clique direito sem o desvio **9**; a chave do
INI ignorada **1**; caminhão cheio aceito **1**; fundição morta contando **1**;
o dono ignorado **1**; qualquer tile da mina **1**; a disponibilidade ignorada
**1**; "disponível" sem o teste de espaço **1**; sem o de operando **1**; sem o
recuo **3**.

### O que o teste ensinou (e nenhum era defeito do código)

- **Região reaproveitada.** O segundo cenário caía na mesma região do primeiro,
  e a doca da fundição de Plymouth (coluna 1) ficava sobre uma célula que o
  primeiro tinha marcado. O caminhão parava colado na doca. Agora a busca da
  área começa abaixo do primeiro cenário.
- **Fome.** O segundo cenário roda depois de milhares de tiques do primeiro,
  sem Agridome: os trabalhadores morriam, a fundição parava, e o caminhão
  esperava na doca — comportamento **certo**, só que medindo a economia e não
  a rota. O teste alimenta a colônia.
- **Estoque no teto.** Nos mapas de escaramuça o jogador começa com o estoque
  cheio — de novo o caso em que o caminhão espera. O teste zera o estoque.
- **Uma bateria que mentia.** Cada rodada leva perto de 4 minutos; com o
  limite em 280 s, o `timeout` matava o processo antes da lista de casos, e o
  `grep -c FALHOU` contava **zero** — lido como "a mutação escapou". A
  mutação do clique direito parecia sobreviver e, rodada à mão, derrubava 9
  casos. A bateria agora acusa **"sem resultado"** quando a lista não sai.

---

## 5.24. O veículo parado sai da frente (2026-11-10)

Pedido: *um caminhão parado na doca impede o outro de atracar — ele tem de
sair*. Isto **é do original** e tinha ficado de fora do porte.

### O defeito de fundo

No original, o passo do veículo (`sub_44B650`) **nunca entra numa célula
ocupada**: `test ebx, 0AFFF0000h` — só anda se não houver ocupante nem parede.
O porte usava ali o `pfCanEnter`, que é um predicado do **planejador** ("dá
para planejar uma rota passando por aqui"). Com um veículo empurrável na
célula, o planejador diz "dá" — e o passo **entrava**, sobrescrevendo a
ocupação do mapa: dois veículos no mesmo tile, e a ocupação apontando só para
um deles.

O comentário antigo em `movement.cpp` dizia isso com todas as letras: *"o
original desvia (sub_44BD60/sub_44BBA0); aqui a rota é recalculada"*.

### O que o original faz (`sub_44B650`)

Com uma unidade na célula de destino:

| | |
|---|---|
| célula com parede **e** ocupante | desiste (`loc_44B998`) |
| ocupante que **não** pode ser empurrado | desiste |
| ocupante empurrável | empurra (`sub_44BBA0`) e **espera** |

A espera é um passo de **deslocamento zero** (`[st+0x58] = [st+0x5C] = 0`):
a rota fica, e no tique seguinte a unidade tenta a mesma direção. Um contador
(`[st+0x6C]`) limita a **20** tentativas; depois desiste e replaneja. O
contador zera quando um passo **com** deslocamento termina (`loc_44C0F1`) e
quando a rota é replanejada (`loc_44C1A0`).

### Quem pode ser empurrado (`sub_44BD60`)

Já estava portado, como `pfOccupantPassable`: veículo, vivo, sem EMP, **aliado
ou desarmado**, e fora destas ações — rota de minério `0x27`, `0x28`, indo
atracar `3`, andando `1`, `0x26`, `0x1F`, `0x0F`. Um caminhão **parado** é
empurrado; um caminhão **no meio da própria rota**, esperando na doca, não é —
quem chega espera e, em 20, replaneja.

**Estava invertido** no porte o teste da arma para não-aliados:

```
call sub_490C50 / test eax,eax / jnz segue   ; aliado
test [+44h],10h / jz segue                   ; desarmado
xor eax,eax / retn                           ; armado: bloqueia
```

Inimigo **armado** bloqueia; inimigo **desarmado** pode ser empurrado. O porte
fazia o contrário. Como o mesmo predicado alimenta o planejador
(`pfCanEnter`/`pfMoveCost`), a correção também muda como as unidades planejam
rotas em volta de inimigos.

### O empurrão (`sub_44BBA0`)

Entre as **7** vizinhas do bloqueador — todas menos a que volta para quem
empurra, `(dir + 4) & 7` —, a de menor custo de terreno (`sub_438390`, com o
contexto do **bloqueador**). A direção do **próximo trecho de quem empurra**
(`routeDirs[idx+1]` dele — o `edi` ali é quem empurra, não o empurrado) leva
**1,5×**, para o bloqueador não ser mandado para dentro do caminho que vem a
seguir. O parado recebe a ordem de ir para lá (`vt[0x7C]` = `sub_44C350`) e
`[+0x22] = 1`, incondicionalmente.

**Corrigido em relação ao original:** o custo de célula bloqueada é
`0x7FFFFFFF`, e `custo * 3 / 2` em 32 bits dá a volta para ~`0x3FFFFFFE` — uma
célula **intransitável** na direção penalizada podia ganhar a escolha. Aqui
bloqueada continua bloqueada.

**Trava:** no original, `vt[0x7C]` chama o planejador (`sub_4401F0`) e o
empurrado anda no **próprio** tique — ninguém empurra de dentro de um
empurrão. O porte usava `unitStepToward`, que dá o passo na hora, e uma trava
para quem está sendo empurrado não empurrar. **Substituído na 5.30**: o
empurrado recebe a rota, como no original, e a trava saiu.

### O bloqueador em movimento

O ramo do bloqueador **andando**, vindo de frente — o desvio inserido na ordem
dele (`sub_44BFD0`) — ficou para a seção **5.27**, portado no mesmo dia.

### O teste — `--pushtest <mapa>`

18 casos, e uma **invariante vigiada a cada tique**: dois veículos em repouso
nunca no mesmo tile, e a ocupação do mapa sob cada um aponta para ele.

1. **O caso do pedido.** Um caminhão parado na doca de uma fundição; outro
   mandado atracar ali pelo clique direito. A ocupação da doca deixa o parado
   no tique **157** e passa ao que chegou no **158** — estritamente depois —,
   após **6** tentativas; e o contador sai zerado depois que ele entra andando.
2. **A regra da arma:** inimigo armado não é empurrado; desarmado é.
3. **Quem não pode ser empurrado não é invadido:** com um inimigo armado na
   doca, o que chega nem entra, nem empurra, nem fica esperando.
4. **Encurralado:** um caminhão empurrável na doca com **todas** as saídas
   ocupadas por outros parados. Ele não sai; quem chega sobe até o limite,
   desiste, replaneja e recomeça — e o contador nunca passa dele.

Neutralizando, uma peça de cada vez: voltar a entrar em célula ocupada **6**
falhas; o empurrão sem efeito **3**; sem o limite de 20 **1**; o teste da arma
invertido de novo **5**; sem a trava **2**; o contador sem zerar na rota nova
**1**; sem zerar no passo com deslocamento **1**.

> Duas armadilhas do teste, e nenhuma era defeito: (1) medir quem saiu antes
> pelo tile do **pixel** — a ocupação muda no começo do passo e os dois
> deslizam juntos, então qual troca de tile primeiro depende da direção; o
> certo é medir a **ocupação** da doca. (2) O primeiro cerco não cercava: a
> doca da fundição é a coluna 4 de 4, a borda direita, então leste e nordeste
> também são livres. O teste agora ocupa todas as vizinhas transitáveis.

---

## 5.25. O recorte por pedaço dos sprites — a carga que "entra" na doca (2026-11-10)

Pedido: *quando o caminhão atraca para depositar, a parte que desce para dentro
da construção aparece; ela deveria ficar oculta.*

### O que se via

Capturando a atracagem quadro a quadro: o caminhão abre o poço da doca (um
quadrado escuro do tamanho de um tile, que **faz parte do sprite do próprio
caminhão**), e o braço que leva a carga desce **atravessando a borda de baixo do
poço**, pintado por cima do terreno. Nos funis laterais, o mesmo: saíam pelos
lados do poço.

### O mecanismo (`sub_403E00`)

Não era ordem de desenho nem a arte. É um recurso do formato `op2_art.prt`:
**recorte por pedaço**. No blitter, para cada pedaço (imagem) de um quadro:

- os bits **5..9** do `type` da imagem são um índice `k`;
- se `k != 0`, o pedaço só é desenhado dentro do **retângulo `k` da animação**
  (`unk_50B374[anim*7 + k]`, 16 bytes, um RECT), relativo ao hotspot e
  **intersectado** com o recorte da tela — que é salvo antes e restaurado
  depois.

Na doca (animações 497 e 486, por exemplo), o recorte 2 é **(12,5)–(41,38)**:
o interior do poço. O braço (`img 254`, 12×33, que desce até y=63) e os funis
(`img 229/230`) têm `k = 2`. No original, tudo o que sai do poço é cortado.

O porte **já lia** esses retângulos (`ArtAnimation::clips`) e os usava no teste
de clique (`pixelMiss`, `sub_404E30`) — o desenho é que os ignorava. Agora o
`blitArtFrame` aplica o recorte de cada pedaço, com a mesma conta e o mesmo
índice do teste de clique.

### Onde mais isso vale

**48 animações** usam o recorte, e nenhuma aponta para um retângulo que não
existe: as docas do caminhão nas duas raças e nas duas fases (476–515), o
caminhão carregando na mina (458/459), mais uma família de doca (355–358) e o
**Magma Well** (1335/1336), cuja peça que desce no respiradouro também é
cortada. Conferido renderizando cada família com e sem o recorte: só some o que
sai do retângulo.

### O teste — `--artcliptest`

10 casos, cada um contra a **composição crua** da mesma animação (todos os
pedaços inteiros) como controle:

1. **Doca, fase 1, quadro 110:** cru, o braço passa 208 pixels abaixo do poço;
   desenhado, **zero** — e, dentro do retângulo de recorte, o desenho é
   **idêntico** ao cru, pixel a pixel (nada sumiu do que devia aparecer).
2. **Doca, fase 2, quadro 60:** cru, o funil sai 114 pixels pela lateral;
   desenhado, zero.
3. **Uma animação sem pedaço marcado sai idêntica à crua** em todos os
   quadros — o recorte não corta nada à toa.
4. Todo `k` aponta para um retângulo da própria animação.

Neutralizando: sem o recorte **2** falhas; o índice deslocado (`clips[k]`)
**1**; o retângulo sem descontar o hotspot **2**; só o corte vertical **1**; o
recorte aplicado também aos pedaços não marcados **1**.

> Uma armadilha de diagnóstico: o `--animdump` decodifica as imagens **direto**,
> sem passar pelo blitter, e o meu primeiro despejo parava no quadro 40 — a
> animação tem 120. Por isso a primeira leitura dos quadros sugeria que a arte
> não passava da borda. Foi a captura **no jogo**, com o caminhão atracado,
> que mostrou o braço vazando.

### Achado no caminho: a ordem de desenho (portada na 5.26)

O original desenha prédios e veículos **numa passada só**, na ordem da lista de
unidades, que `sub_43E7E0` mantém **ordenada por Y** (`[+0x18]`): é um
*painter's algorithm*, e um veículo que passa **atrás** (ao norte) de um prédio
alto é coberto por ele. O porte desenha **todos os prédios e depois todos os
veículos** (`GameState::onIdle`, os "três passes"), então o veículo sempre sai
por cima. Não era isso que mostrava o braço da doca — o caminhão atracado tem Y
maior que o centro do prédio e sai por cima nas duas ordens —, e mudar a ordem
mexe no visual do jogo inteiro; ficou para decisão, e foi portada em seguida
(seção 5.26).

---

## 5.26. A ordem de desenho do original (2026-11-10)

O porte desenhava **todos os prédios, depois todos os veículos, depois os
efeitos**, percorrendo as unidades **por índice**. Um veículo passando atrás
(ao norte) de um prédio alto saía sempre **por cima** dele.

### O que o original faz

`sub_49EA90` monta, a cada quadro, duas listas com as unidades visíveis — **na
ordem da lista por Y** (`dword_54EC0C`, que `sub_43E7E0` mantém ordenada por
`[+0x18]` no fim de cada laço de tique) — e `sub_49E8C0` desenha:

| leva | quem | como |
|---|---|---|
| 1 | **lista A**: prédios **e** veículos | juntos, de cima para baixo no mapa |
| 2 | **lista B** sem `0x200` | armas e projéteis, explosões de prédio, animação de ambiente, beacons, fumarolas, respiradouros |
| 3 | **lista B** com `0x200` | os desastres: raio `0x4C`, tornado `0x4D`, terremoto `0x4E`, erupção `0x4F`, meteoro `0x50` |

A lista B são as unidades com `[+0x44] & 0x140`. O porte não modelava esses
bits; a classificação veio do binário, sem chute:

- o `0x100` não vem da classe, vem do **descritor do tipo**: a criação
  genérica (`sub_4466B0`) faz `unidade[+0x44] |= desc[+0x1F4]`, e o `init`
  (`vt[4]`) de 31 vtables de descritor liga esse bit. Lendo o nome de cada uma
  (o `vt[1]` do descritor devolve o *code name*), **têm** `0x100`: todas as
  armas (`LASER`, `CANNON`, `ACID`, `EMP_BIG`...), `BLD_EXPL_SML/MED/BIG`,
  `AMBIENT_ANIM`, `BEACON`, `FUMAROLE`, `MAGMA_VENT`, `SHUTTLE`, `WRECK_FLAG` e
  os desastres; **não têm**: todos os prédios e todos os veículos. O míssil do
  Spaceport liga o bit no construtor da própria classe (`sub_4807F0`);
- o `0x40` só é ligado por uma classe que **não desenha nada**
  (`off_4D5D48`, slot 6 = `nullsub_65`) — irrelevante para a ordem;
- o `0x200` é ligado pela base dos desastres (`off_4D73A0`, `sub_4A7E70`) e
  nenhuma derivada o desliga: terremoto `sub_413510`, raio `sub_432DF0`,
  meteoro `sub_44A1F0`, erupção `sub_4A8120`, tornado `sub_48F3D0`.

No porte isso vira: lista A = `UNIT_BUILDING | UNIT_VEHICLE`; lista B = o
resto; última leva = tipos `0x4C..0x50`. E o laço passa a percorrer a lista
por Y (`unitListFirst/unitListNext`), que o porte já mantinha igual à do
original para o tique — o desenho é que não a usava. As decorações de cada
unidade (torre, espuma, EMP, luzes) são do `vt[0x18]` dela e vêm junto.

O sorteio que existe dentro do laço (fumaça de prédio danificado) usa um
gerador **local**, semeado por unidade — mudar a ordem não mexe na simulação.

### O teste — `--drawordertest <mapa>`

Cada caso renderiza a mesma cena quatro vezes (as duas unidades, só uma, só a
outra, nenhuma) e olha os pixels em que **as duas** desenham: ali, o quadro
completo tem de ser igual ao de quem ficou **na frente**.

1. **Veículo ao norte** de um Command Center: o **prédio** na frente em 433 dos
   443 pixels de sobreposição, o veículo em **nenhum** — era o defeito.
2. **Efeito da lista B ao norte** do prédio: o efeito na frente, apesar do Y
   menor.
3. **Desastre contra efeito:** um tornado, com o efeito de Y **maior** e criado
   **depois** — nem a ordem por Y nem a de criação dariam a resposta certa — e o
   tornado fica na frente nos 1042 pixels.
4. **Caminhão na doca** de uma fundição (dentro da linha de baixo do prédio):
   o caminhão na frente — a doca continua como estava.

Neutralizando, uma peça de cada vez: a ordem antiga inteira **4** falhas; a
lista por Y mas com prédios antes de veículos **2**; sem a leva do `0x200`
**2**; a lista A percorrida por índice **2**; tudo junto na lista por Y, sem
lista B, **4**.

> Duas premissas que o teste ensinou: o sprite de um prédio **não desce** abaixo
> do próprio footprint (um veículo na linha logo abaixo nem encosta nele — por
> isso o caso "ao sul" é a doca); e meteoro, raio e tornado só **desenham**
> depois de alguns tiques ativos, então o caso deixa o jogo andar até o desastre
> aparecer e só então congela.

## 5.27. Quem vem de frente dá passagem — o desvio (2026-11-10)

Pedido: portar o caso que a 5.24 deixou de fora — o bloqueador **andando**.

### O que o original faz (`sub_44BBA0`, ramo `[+0x22] == 1`)

Quando quem empurra acha na célula da frente uma unidade que também está
andando, ela só é desviada se estiver:

- **entre dois passos** — `[st+0x58]`, `[st+0x5C]` e `[st+0x60]` (as duas
  velocidades e o passo de giro) zerados; no meio de um passo, nada;
- **vindo de frente** — o próximo trecho da rota dela (`[st+0x7C + idx]`) é a
  direção oposta, `(dir + 4) & 7`, rumo a quem empurra; ou não há mais trecho.

A casa escolhida é a mesma do empurrão do parado (as 7 vizinhas, com a
penalidade de 1,5× — ver 5.24). Só que, em vez de uma ordem nova, ela recebe um
**desvio** na ordem que já tem: `sub_44BFD0(casaX*32, casaY*32, 0)`. Qualquer
outro sub-estado: nada.

### O desvio na ordem (`sub_44BFD0`)

| passo | |
|---|---|
| bit 2 de `[ordem+0x34]` ligado | sai: já há um desvio pendente |
| liga o bit 2 | |
| sem pontos (`count == 0`) | o alvo `[+0x50]/[+0x54]` vira o 1º ponto |
| índice atual `[ordem+8] == −1` | sai (o bit fica) |
| recua o índice e grava a casa ali | o ponto em que ia vira o seguinte |
| `sub_44C2F0` | replaneja |

Com o índice em 0 — a ordem comum, de um ponto só — o recuo leva a **−1** e a
escrita cai em `[ordem+0x10]`, logo antes do vetor de pontos (`[ordem+0x14]`).
Não é estouro: é uma vaga de propósito, e o teste do −1 impede de passar dela.
`sub_44BEC0` (avanço) e `sub_44C2F0` (objetivo) leem
`[ordem + idx*4 + 0x14]`, então alcançam a vaga também. No porte, a vaga é
`OrderBlock::detourSlot`, e `wpAt` lê o ponto `i` com ela.

O caso sem pontos é o de quem foi **empurrado** e anda a rota curta sem ordem
(5.30). Ele fica de fora: o laço sem ordem do porte não tem lista de pontos.
Esse empurrado não recebe desvio; segue a rota e, se travar, desiste em 20
esperas, como qualquer passo.

### A chegada ao desvio (`loc_44C1D2`, em `sub_44C0B0`)

Ao alcançar um ponto, depois de `sub_44BEC0` (avança) e `sub_44C2F0`: se o
bit 2 está ligado **e** houve avanço, apaga o bit e fica **32 tiques** parado —
um passo sem deslocamento (`[st+0x64] = 0x20`, velocidades e giro zerados) —,
dando tempo a quem empurrou para passar. Depois retoma o ponto em que ia.

### Corrigido em relação ao original

Se o desvio fica inalcançável (alguém para ou levanta um muro na casa dele), o
ponto é pulado (`loc_44C16D`: `inc [ordem+8]`) — mas o bit 2 **ficava
ligado**. A unidade fazia a parada de 32 tiques no ponto **seguinte**, que
nada tem a ver com o desvio, e não aceitava outro desvio até lá. Enquanto o bit
está ligado o desvio é sempre o ponto atual, então o pulo é sempre o dele:
aqui o bit sai junto.

### O teste — `--detourtest <mapa>`

Dois caminhões, cada um com ordem de mover dada pelo clique direito, um contra
o outro através de um **corredor** de 13 casas (quatro linhas de muro de cada
lado e um nicho de uma casa embaixo, no meio). A cada tique, a invariante da
5.24: dois em repouso nunca no mesmo tile, e a ocupação sob cada um aponta
para ele.

1. **O encontro.** Os dois ficam juntos no corredor por 305 tiques; um deles
   recebe **um** desvio, na vaga −1, para o **nicho**; chega lá, fica 32 tiques
   parado na casa do desvio, e os dois chegam ao destino (t=525 e t=474).
2. **A tabela de decisão**, com o estado do bloqueador montado à mão e o
   empurrão chamado direto: entre passos e de frente → desvia; sem trecho
   restante → desvia; com velocidade, girando, indo para o norte, indo para o
   leste, com desvio já pendente, ou noutro sub-estado → nada muda. Quando
   desvia: a casa é vizinha e não é a de quem empurra, o ponto antigo fica
   intacto, a rota é refeita e o bloqueador não ganha passo nenhum.
3. **O desvio inalcançável:** um muro na casa do desvio. O ponto é pulado e o
   bit sai junto, em 26 tiques, sem parada; sem a correção, o bit só saía na
   chegada ao destino final, 120 tiques depois.

Neutralizando, uma peça de cada vez: sem chamar o desvio **5** falhas; sem a parada de 32 **1**; sem o teste de "entre passos" **2**; sem o teste de "de frente" **2**; a vaga −1 ignorada (lendo sempre o ponto 0) **1**; o bit 2 nunca apagado **1**; o índice sem recuar **6**; sem replanejar ao inserir **2**; o andando empurrado como se estivesse parado **5**; o desvio pulado deixando o bit ligado, como no original, **1**. Sobrevive uma: a parada sem exigir o avanço — com o bit ligado, chegar a um ponto intermediário sem avançar não acontece, porque o desvio é sempre o ponto atual e a chegada a ele sempre avança.

> Três armadilhas do teste, e nenhuma era defeito: (1) em campo aberto não há
> encontro — um veículo andando custa 4× no planejador e os dois se contornam;
> (2) com partida e destino na mesma linha, a rota de cada um passava pela casa
> onde o outro estava parado, e os dois davam a volta por fora do corredor —
> agora partem e chegam em diagonal; (3) o encontro só passa pelo caminho
> feliz (lá o bloqueador está sempre entre passos e de frente): as mutações que
> tiravam esses dois testes sobreviviam a ele, e por isso existe a tabela.

## 5.28. A última linha do mapa e o piso da Garage (2026-11-17)

Duas contribuições de fora, recebidas como *pull request* e conferidas no
binário antes de entrar.

### A vista para no fundo do clip rect

`sub_402E70` copia o clip rect do mapa (`dword_54EBD4..E0`: x1, y1, x2, y2) para
o retângulo-limite da vista em `[vista+0x344]`, com x1/y1 ≥ 0 e y2 limitado a
`altura − 1` — o y2 é um índice de tile **inclusivo**. Nos mapas do jogo
`y2 = altura − 2`, então a última linha de tiles nunca aparece. O porte deixava
a câmera descer até o fim do mapa. Agora o fundo da vista é `(y2 + 1) * 32`
(`GameState::viewMaxY`); a chave nova `[Game] ShowBottomRow=1` devolve o
comportamento antigo.

Teste: dois casos no `--scrolltest` — a câmera saturada embaixo mostra até
2016 px no eden02 (o fundo do clip rect; o mapa tem 2048) e, com
`ShowBottomRow=1`, até 2048.

### O piso da Garage

A camada de chão da Garage (`loc_41CF24..41CF5A`) desenha um piso sob o prédio
inteiro, uma animação por estado: estado 0/1/2 → `0x85A`/`0x85C`/`0x85F` com o
bit `0x1000` da unidade (a mesma tabela do corpo do prédio, `isEdenTable`), e
senão `0x214`/`0x216`/`0x218`; nos outros estados, nada. O porte não o
desenhava: faltava o piso cinza com as setas amarelas da garagem de Plymouth, e
o terreno aparecia por dentro da de Eden. Entra no passe da camada de chão, com
os pisos do Agridome, dos tanques e das minas.

Teste: um caso no `--drawordertest` para cada facção, que conta quantos pixels
do footprint ficam iguais ao terreno sem a garagem — 4–5% com o piso (as
bordas recortadas do sprite), 38–40% sem ele. O `--missiontest` passou a
listar o estado e a animação de cada prédio.

### O terceiro, que não entrou

O terceiro *pull request* portava o empurrão e o desvio de quem está no
caminho (`sub_44BBA0`/`sub_44BFD0`) — o mesmo que as seções 5.24 e 5.27, feito
em paralelo a partir de um commit anterior a elas. A versão deste repositório é
a que fica: tem a regra da arma corrigida, a vaga −1 do original, a parada de
32 tiques e a conta de 1,5× sem estouro. A única correção dele que faltava aqui
(o bit do desvio pulado) já tinha entrado na 5.27.

## 5.29. As chaves do porte nas telas de preferências (2026-11-17) — **acréscimo deste porte**

Pedido: poder trocar pelo jogo, sem editar o INI, as chaves que são só deste
porte — primeiro a `AutoOreRoute` (5.23), depois todas as que ainda não
estavam em tela nenhuma.

| Chave | `GAME PREFERENCES` (menu principal) | `Game Preferences` (aba Options, na partida) |
|---|---|---|
| `Fullscreen` | rádio *Screen Mode* (já existia, 5.8) | rádio *Screen Mode* (já existia) |
| `AILevel` | combo *AI Level* (já existia) | — o nível é decidido ao começar a missão |
| `AudioVolume` | barra *Audio Volume* (já existia) | **novo:** *Audio Volume*, `L&ESS`/`&MORE` de 5 em 5% |
| `AutoOreRoute` | **novo:** caixa *Auto Ore Route* (1204) | **novo:** *Auto Ore Route* `On`/`Off` |
| `MapWrap` | **novo:** caixa *Wrap All Maps* (1205) | **novo:** *Wrap All Maps* `On`/`Off` |
| `ShowBottomRow` | **novo:** caixa *Show Bottom Row* (1206) | **novo:** *Show Bottom Row* `On`/`Off` |
| `DemoCamera` | **novo:** caixa *Demo Camera* (1207) | — só vale no demo de atração do menu |
| `MenuSkin` | **novo:** caixa *Original Menu Skin* (1208) | — só vale nos menus |
| `NightDarkness` | **novo:** barra *Night Darkness* (1209, 0..100%) | **novo:** *Night Darkness*, `L&ESS`/`&MORE` de 5 em 5% |
| `CommandDebug` | **novo (5.39):** caixa *Command Debug* (1210) | **novo (5.39):** *Command Debug* `On`/`Off` |
| `ProductionQueue` | **novo (5.40):** caixa *Production Queue* (1211) | **novo (5.40):** *Production Queue* `On`/`Off` |
| `RightClickOrders` | **novo (5.44):** caixa *Right-Click Orders* (1212) | **novo (5.44):** *Right-Click Orders* `On`/`Off` |
| `ListScrollBars` | **novo (5.50):** caixa *Wheel & Scroll Bars* (1213) | **novo (5.50):** *Wheel & Scroll Bars* `On`/`Off` |

**No menu principal**, o diálogo ficou **160 px mais largo** (360 → 520;
`kPrX` 140 → 60) e ganhou uma **coluna à direita**, *"Port extras:"*, que
começa depois dos botões `LESS`/`MORE` das velocidades e termina com a nota
*"(not in the original)"*. A altura não tinha para onde crescer em 480 px.
Desmarcada, cada caixa é o comportamento do original — menos `AutoOreRoute` e
`DemoCamera`, que nascem ligadas. Tudo só vale no `ACCEPT`, como o resto da
tela. A **pele dos menus** passou a trocar já no `ACCEPT`
(`ShellMenu::applyMenuSkin`, o mesmo caminho da abertura do jogo, e o
`setState` seguinte remonta o menu com as medidas da pele); antes só valia
reabrindo o jogo.

**Dentro da partida**, as linhas novas vêm depois do *Screen Mode*. As de
porcentagem são itens com valor (`valueItem`), com as dicas próprias nos
botões. O `&APPLY` regrava o INI e o `loadSettings` relê tudo: volume,
escuridão, volta do mapa, última linha e rota de minério mudam na hora, sem
reiniciar a partida.

Testes, ambos pelo caminho do jogador e devolvendo o INI byte a byte:

- `--uitest` (o `prefsTest` do shell): cada caixa está à direita da coluna das
  caixas do original; com o INI no valor **oposto**, nasce com ele, o clique a
  inverte, sair sem `ACCEPT` não grava, e o `ACCEPT` grava — nos dois
  sentidos. A pele liga e desliga no `ACCEPT`. A barra da noite nasce com o
  INI (100) e o clique no meio grava ~50.
- `--optmenutest`: as linhas nascem com o INI; o `&MORE` anda de 5 em 5, o
  volume para em 100 e em 0; nada vale antes do `&APPLY`; o `&APPLY` grava as
  quatro chaves e o jogo passa a usá-las (`nightDarkness()`,
  `soundMasterVolume()`, a volta e a última linha).

## 5.30. O empurrado anda pela rota, e quem não acha rota espera (2026-11-17)

O teste de estresse que veio no PR #2 (`--stacktest`, agora do projeto) manda
**todos** os veículos do jogador, em grupo, 8 tiles a leste, de volta e de
novo. Nunca havia dois no mesmo tile, mas sobravam 1 a 3 veículos **andando
para sempre**: com 3000 tiques por perna, nenhuma perna terminava nos mapas
E05 (25 veículos), P05 (26) e P08. O diagnóstico mostrou dois estados presos,
e os dois eram simplificações do porte.

### 1. O empurrado ganha uma rota, não um passo (`vt[0x7C]` = `sub_44C350`)

No original, o empurrão do parado (`sub_44BBA0`) chama `vt[0x7C]` com a casa
escolhida e põe `[+0x22] = 1`. O `sub_44C350` **não dá passo nenhum**: encaixa
a posição no centro do tile, põe o alvo em `[st+0x50]/[st+0x54]` e o objetivo
em `[st+0xCC]/[st+0xD0]`, zera o passo, as velocidades e a rota, e chama o
planejador (`sub_4401F0`). Quem anda é o laço da unidade (`sub_44C0B0`), **nos
tiques dela** — com a lista de pontos vazia, na casa do alvo é a chegada de
`loc_44C1C0`. E, como anda pelo laço comum, o empurrado pode por sua vez
empurrar o seguinte, ou esperar.

O porte dava **um passo único na hora** (`unitStepToward`) e, por isso,
precisava de uma trava contra empurrão dentro de empurrão. Numa multidão a
vizinha escolhida costuma estar ocupada: o passo era recusado, o empurrado
ficava com `[+0x22] = 1` sem sair do lugar, e quem empurrava tentava de novo a
cada tique. Agora:

- `unitPathTo` é o `sub_44C350`: guarda o alvo (`UnitRecord::pathGoalX/Y`),
  zera passo e rota e planeja, sem passo;
- o `stepUnit` tem o laço **sem ordem**: sub-estado 1, sem ordem e com alvo →
  segue a rota (replaneja quando ela acaba), empurrando e esperando como
  qualquer passo; na casa do alvo ou sem rota, sai do sub-estado
  (`loc_44C1C0` / `loc_44C20A` sem o bit 0); passo recusado, desiste;
- a trava `s_empurrando` saiu;
- o "sair de cima" do salvage (`sub_44BE40`) usa o mesmo `vt[0x7C]`, e o
  `[+0x22] = 1` é incondicional, como lá.

### 2. Sem rota para o último ponto: espera crescente (`loc_44C20A`)

Sem rota até o **último** ponto, o original não desiste se a ordem está ativa
(bit 0 de `[ordem+0x34]`): fica `[st+0x70]` tiques parado (um passo sem
deslocamento) e aumenta a espera da vez seguinte — `espera + GetRand(espera) +
2`, com teto de `0x53` (`loc_44C227`). A espera volta a 0 quando um ponto é
alcançado (`loc_44C114`). Sem o bit 0, só sai do sub-estado de movimento. O
porte replanejava **a cada tique**, sem recuo, zerava o sub-estado e não
consumia o sorteio do gerador. Agora é o do original (`OrderBlock::retryWait`,
no bloco que a ordem nova zera).

### O teste — `--stacktest <DLL>`

Além da invariante de ocupação a cada tique, passa a falhar se, ao fim de
alguma perna, **sobrar veículo andando**, e imprime o estado de quem sobrou.
Agora todas as pernas terminam: E05 em 201/452/703 tiques, P05 em
201/502/653, e também E10 e P08 — sem nenhum tique com dois veículos no mesmo
tile.

O `--pushtest` mudou junto, porque duas premissas dele vinham do passo único:

- **a doca**: o empurrado anda no próprio tique e libera a célula no início do
  passo; quem chega, se vier depois na volta das unidades, entra no **mesmo**
  tique (157/157, antes 157/158). O caso agora exige "não antes", e a
  invariante continua garantindo que nunca há dois no mesmo tile;
- **o encurralado** virou dois: com o cerco de **caminhões**, o preso agora sai
  empurrando um vizinho — a cadeia do original — e quem chega atraca (t=53);
  com o cerco de **paredes**, ninguém sai, e quem chega vai a 20, desiste e
  recomeça (o caso que testa o limite). 21 casos.

E o `--detourtest` ganhou o **alvo murado**: um caminhão mandado a um tile
cercado de paredes não larga a ordem, e a espera cresce em 7 tentativas até o
teto de 83. Sem o recuo, o caso falha (espera 0, tentativa a cada tique).

### O que ficou de fora

O **desvio** (`sub_44BFD0`) num empurrado que está andando a rota **sem
ordem**: o original faz do alvo `[+0x50]/[+0x54]` o 1º ponto e então recua; o
laço sem ordem do porte não tem lista de pontos. Esse empurrado segue a rota
curta dele e, se travar, desiste em 20 esperas.

## 5.31. O bipe da saída, e um ramo do marcador que nunca roda (2026-11-17)

Duas pendências pequenas que estavam anotadas — e as duas anotações estavam
erradas.

### O bipe da saída (`IWnd::_WndProc`)

A anotação dizia `sub_431000`, que é uma cópia de texto. O lugar certo é o
procedimento de janela estático `IWnd::_WndProc`: no `WM_NCDESTROY` (`0x82`)
da **primeira** janela `IWnd` destruída na sessão (`dword_4E0BB8`, escrito só
ali), ele toca o som **0x5C** — o `beep5.wav`, pelo `sub_47F020`, global — e
faz `Sleep(3500)`, para o som terminar antes de o áudio ser desligado. Os
diálogos (`IDlgWnd`) passam pelo procedimento de diálogo, não por este; a
janela que chega aqui de forma garantida é a **moldura**, destruída pelo
`TFrame::ShutDown` ao sair do programa.

No porte: `soundExitBeep()`, chamado pelo `TApp::shutdown` depois de esconder
a janela (no original ela já sumiu nessa hora) e antes de desligar o vídeo e o
som. Uma vez por sessão.

**Corrigido em relação ao original:** ele espera os 3,5 s **mesmo sem som**
(`Sound=0`, ou sem dispositivo de áudio) — uma espera à toa ao fechar o jogo.
Aqui só espera se o bipe de fato começou.

Teste `--exitbeeptest`: com som, toca e espera 3,5 s (medido: 3501 ms); a
segunda chamada não faz nada; com `Sound=0`, não toca e não espera.

### O ramo `loc_43EE7C` do marcador de seleção — código morto

Anotado como "barra verde ainda não portada". Lido de perto, o `sub_43E950`
(colchete e barra de vida sobre a unidade) vai para `loc_43EE7C` quando
`arg_0 & 0xB == 0`, e ali a primeira coisa é `test al, 1 / jz loc_43F223` —
que sai, porque o bit 0 já é zero para ter chegado ali. O que viria depois era
o **nome** da unidade escrito sobre ela (`vt[0x3C]`, com o nome da arma em
`"%s %s"` quando `[+0x44] & 0x90`), e não uma barra. E o `loc_43F223` em
seguida desenha os **pontos da ordem** só com `arg_0 & 0x10` e o
`dword_56DE70` ligado pelo comando **40122** do menu de depuração. Os três
chamadores passam `arg_0` igual a 1 (o *hover*) ou 2 (a seleção): nenhum dos
dois ramos roda no executável. Não há o que portar.

## 5.32. Shift e o botão do meio na seleção (2026-11-17)

A pendência dizia que a regra do Shift/Ctrl estava num "objeto enumerador"
ainda não extraído. Ela está no procedimento de mensagens do filtro de seleção,
o **`sub_41AAA0`**, que é quem escreve a lista (`dword_5471D0`, a contagem, e
`dword_5471D4`, os ids). O `wParam` do mouse traz `MK_SHIFT` (4) e
`MK_CONTROL` (8).

### O que o original faz

| ação | efeito |
|---|---|
| clique numa unidade, sem Shift | limpa e marca **só ela** — menos a que está no sub-estado `0x0A` (EMP), que deixa a seleção vazia; o som de seleção (`desc+0x1F0`) só toca se ela **não** era a única já marcada |
| **Shift + clique** | **alterna** o veículo: se está na seleção, sai; senão entra (até 32), sem som. Nada acontece se a seleção atual é um **prédio**, se o clicado não é veículo, ou se ele está nos sub-estados `0x0A`/`0x0D` |
| **botão do meio** | é o esquerdo com Shift (casos 519/520 do `switch`: `or arg_8, 4`) — também na caixa |
| Shift + clique com um comando armado | vai para a **seleção** (o `test al, 4` pula o enumerador do comando); a mira continua armada |
| a caixa (`WM_LBUTTONUP`) | **soma** os veículos do jogador à seleção atual (até 32), só a limpando antes se ela for um único prédio; os do sub-estado `0x0A` ficam de fora; **não checa repetidos** |
| **Ctrl + clique** | **não é seleção**: com um comando armado, acrescenta um **ponto de passagem** à rota (`[esi+3C]`/`[esi+40h]`, com o som `0x59`), desenhado em vermelho pelo `sub_41B620` |
| ESC (`WM_CHAR 0x1B`) / clique direito (`WM_RBUTTONDOWN`) | sem comando armado, **limpam** a seleção |

### O que entrou no porte

- Shift + clique alternando, com as quatro exceções acima, e o botão do meio
  como Shift + clique — inclusive com um comando armado;
- no clique simples, os dois filtros que faltavam: quem está em `0x0A` não é
  marcado, e o som só toca quando a unidade marcada muda;
- na caixa, quem está em `0x0A` fica de fora.

**Duas decisões deste porte**, porque o modelo do original não fecha com o do
porte:

- **A caixa sem Shift continua substituindo** a seleção, e **com Shift (ou com
  o botão do meio) ela soma**, como a do original. No original toda caixa
  soma, e quem limpa é o ESC ou o clique direito — mas aqui o clique direito
  é "mover" e o clique no chão limpa, então uma caixa que sempre somasse não
  teria como recomeçar a seleção pelo mouse;
- **corrigido**: ao somar, um veículo já marcado não entra de novo na lista
  (o original não checava, e ele aparecia duas vezes).

### O Ctrl

Os **pontos de passagem com Ctrl + clique** são outro recurso (a rota de vários
pontos de um comando armado, com o desenho dela), não seleção — portados na
**5.33**.

### O teste — `--selecttest <mapa>`

Dezessete casos novos, pelo mouse de verdade (`GameState::debugMouse` manda o
evento ao `handleEvent`, com o Shift pelo `SDL_SetModState`): o Shift soma e
tira, o botão do meio faz o mesmo, prédio não entra e não aceita soma, o
clique simples troca, a caixa simples substitui e a com Shift (ou com o meio)
soma sem repetir, o Shift com a mira armada mexe na seleção e a mira fica, e o
`0x0A` fica de fora do clique, do Shift e da caixa. Neutralizando a checagem de
repetidos, 2 falhas; tirando o botão do meio, 1.

## 5.33. Pontos de passagem com Ctrl + clique (2026-11-17)

O Ctrl, no original, **não é de seleção**: com um comando armado, Ctrl +
clique marca **pontos de passagem** que a unidade percorre antes do alvo.

### O mecanismo (do disassembly)

- **A lista mora na mira**, o objeto `dword_547170` (um `SubFilter`): em
  `[+0x34]` o **limite**, em `[+0x3C]` a contagem e, a partir de `[+0x40]`, os
  pontos (x e y em pixels do mundo, em *words*).
- **Armar um comando** — `sub_41B550(estado, comando, limite, …)`, chamado no
  `vt[1]` de cada um — põe o estado da mira (`[+0x24]`: 1/4 área, 2
  posicionamento, 3 ponto), o limite, e **zera a contagem**. O porte chamava
  esse limite de "modo"; os comentários da tabela de comandos foram
  corrigidos.
- **Ctrl + clique** (`sub_41AAA0`, `WM_LBUTTONDOWN` com `MK_CONTROL`, testado
  antes de tudo): com `[+0x34] != 0`, grava o ponto e toca o som `0x59`; com a
  lista cheia, **troca o último**. Com o limite 0 — ou sem comando armado —,
  nada: nem ponto, nem seleção, nem alvo.
- **Os limites** (o 3º argumento do `sub_41B550`): **7** para mover, ficar
  parado, guardar, reparar/tomar/desmontar e as obras (kit e mina); atacar
  também, mas **0** quando o único selecionado é um prédio armado (bit `0x80`,
  que não anda); **6** na patrulha e **5** na rota de minério, que **rearmam**
  a cada clique com o que sobra (`7 − usados`, `6 − usados`); **0** em muros,
  tubos e terraplenagem.
- **No pacote** (`sub_451420`): cada ponto vira um LOCATION de pixel com a
  folga de chegada **2** nos bits 29..31 (`dl = 2`: chegar a até 2 tiles),
  **antes** do alvo. Mover/doca e ficar parado acrescentam o alvo por último
  (folga 0); a obra acrescenta o local (o `inc word ptr [ebx]` do
  `sub_451B30`); ataque, guarda e reparo levam só os intermediários — o alvo
  deles vai em outro campo.
- **Patrulha e rota de minério** (`sub_454540` / `sub_453FA0`) copiam os
  pontos do Ctrl para a lista delas **antes** de cada ponto clicado; o índice
  de recomeço da patrulha e os índices da mina e da fundição apontam para o
  ponto **clicado**.
- **O desenho** (`sub_41B620`, `loc_41B74D`): em cada ponto, um quadrado
  **cheio** de 17×17 px vermelho (`0x7C00`, `sub_41F3E0`) e, dentro, o número
  dele em branco (`'0' + [+0x38] + i` — 1, 2, 3…), na fonte comum. E com Ctrl
  apertado numa mira de posicionamento (estado 2), o retângulo da obra **não**
  é desenhado — ali o clique é ponto, não obra.

### No porte

`uiWaypointsArm` / `uiWaypointAdd` / `uiWaypoints` (em `commands`), o limite
de cada comando no `uiCommandClick`, os pontos em cada pacote, o Ctrl no
clique do `GameState` e o desenho. A ordem de obra (`setBuildOrder`) passou a
receber a lista inteira, com o local por último. O pacote leva os pontos na
rede sem mudança (a serialização já levava a lista de qualquer tipo).

**Ficou de fora:** remover muro. O original aceita 7 pontos ali, mas no porte o
Earthworker vai até o muro sem lista de pontos (`applyRemoveWall`); lá o
limite fica 0.

### O teste — `--waypointtest <mapa> [bmp]`

31 casos, pelo botão do comando e pelo mouse de verdade (Ctrl no
`SDL_SetModState`): sem mira, Ctrl + clique não faz nada; mover arma com limite
7, acrescenta pontos sem disparar, desenha o quadrado vermelho com o número
branco, para em 7 trocando o último, zera ao rearmar; o alvo dispara a ordem
com os 2 pontos (folga 2) e o alvo (folga 0), e o caminhão passa a até 2 tiles
de cada ponto, na ordem, antes de chegar; o muro tem limite 0 e o Ctrl + clique
não o dispara; a patrulha arma com 6, reabre com 4 depois do ponto inicial, e
manda 5 pontos recomeçando no índice 2; o kit arma com 7 e a ordem da obra
leva o ponto antes do local. Com o `bmp`, salva o quadro com os pontos.

> Uma armadilha do próprio porte, pega olhando a imagem e que o teste agora
> cobre: o `UiFont::draw` recebe a largura como **passo da linha**; passando a
> largura da vista, o número saía espalhado fora do quadrado.

## 5.34. O Earthworker: a animação de trabalho e o tubo que não ligava (2026-11-18)

Pedido: *o veículo que constrói os tubos não está funcionando direito, e a
animação dele está estranha.* Eram dois defeitos do porte, e nenhum dos dois
era só do Earthworker.

### 1. Os quatro conjuntos de animação dos veículos (`sub_44C4F0`)

O porte só conhecia **uma** tabela de 16 direções por facção. O descritor do
veículo tem **quatro**, e o `sub_44C4F0` (`vt[0x80]`) escolhe uma pelo estado:

| conjunto | quando | Eden / Plymouth |
|---|---|---|
| 0 | parado | `+0x24C` / `+0x34C` |
| 1 | andando — sub-estado 1 com velocidade ou giro (`[st+0x58]`/`[st+0x5C]`/`[st+0x60]`) | `+0x28C` / `+0x38C` |
| 2 | **trabalhando** — a ação é a de `desc+0x44C`, fora do sub-estado 1 | `+0x2CC` / `+0x3CC` |
| 3 | a ação é a de `desc+0x450`, fora do sub-estado 1 | `+0x30C` / `+0x40C` |

Uma entrada vazia cai na do conjunto 0. O construtor-base (`sub_44B400`) põe
`0x35` — uma ação que não existe — nos dois campos, e só quem tem animação de
trabalho os define. Saíram do binário:

- **Earthworker**: ação **7** (muro/tubo) → 1804–1811 (Eden) / 1812–1819
  (Plymouth). O porte mostrava os quadros de **andar** girando com o veículo
  parado — era a "animação estranha";
- **ConVec**: ação **6** (construindo) e **0x1C** (reparando);
- **Spider** e **veículo de reparo**: ação **0x1C** (reparando);
- **Robo-Dozer**: ação **1** (terraplanando).

Nenhum deles mostrava a animação de trabalho no porte. O
`tools/extract_anim_tables.py --emit` passou a extrair os quatro conjuntos e
as duas ações (`vehicleAnimSet` / `vehicleAnimIndexSet`), e o desenho usa o
conjunto pelo estado. A função antiga (`vehicleAnimIndex`) ficou como o
conjunto 0, para quem só precisa do corpo parado.

### 2. A doca na rede de tubos (`sub_42F000` / `sub_42EC40`)

Um tubo que terminava na **doca** de um prédio (a casa livre para o caminhão
atracar, sem ocupante) às vezes ligava e às vezes não. A busca do porte
começava pelo **retângulo inteiro** do prédio, com a doca: saindo do prédio,
ela achava o tubo pela doca; vindo pelo tubo, não achava o prédio — e o
resultado dependia da ordem dos prédios na lista.

No original, a busca de um prédio parte do **centro** dele (`[+14h]/[+18h]`)
e se espalha pelas células que ele **ocupa** (`sub_42EC40`: prédio sem nó de
rede entra; prédio com nó registra o encontro; sem ocupante, só passa tubo).
A doca fica de fora nos dois sentidos: **tubo encostado só na doca não liga o
prédio** — sempre. O porte passou a fazer igual.

### O que estava certo

O percurso do Earthworker (a ponta mais próxima da reta, ir para o **lado** de
cada casa, assentar, seguir) já estava fiel: com minério, as retas saem
inteiras, nos dois sentidos. Quando falta minério, ele larga a ordem **sem
aviso** — e o original faz o mesmo (`loc_43BCCD`: ação 0, sub-estado 0). E
**usina não precisa de tubo**: o Tokamak opera solto, como no jogo.

### O teste — `--tubetest <mapa> [prefixo]`

Um CC com armazéns encostados, dois Agridomes a ligar (um abaixo, um à
direita), um Tokamak solto e o Earthworker partindo de longe. Retas pelo
comando de verdade: a vertical liga o Agridome de baixo; a horizontal termina
na **doca** do da direita e não liga; um tubo de uma casa encostado numa célula
ocupada liga; e uma reta da direita para a esquerda sai inteira. Casa a casa,
o tique em que ficou pronta. Enquanto assenta, a animação é sempre a de
trabalho (2283 tiques, todos no conjunto 2). Com o prefixo, salva o quadro
com o veículo trabalhando. 10 casos.

## 5.35. Missão 2 do Eden: a praga que não aparecia e a pesquisa que não contava (2026-11-18)

Pedido: *na missão 2 do Eden a infecção do Blight não aparece nem aumenta, e
os itens de pesquisa são feitos mas não saem dos objetivos — a missão não
termina.* Dois defeitos do porte.

### 1. O gatilho de pesquisa disparava no primeiro tique (`sub_492700`)

A E02 cria cinco `CreateResearchTrigger` (tecnologias 2701 a 2705, uma por
banco de dados a reconstruir) e uma condição de vitória para cada. A máscara
do gatilho nasce `(1 << jogador) ^ 0x3F` (`sub_4926D0`), e o porte devolvia
"disparou" quando ela não era zero — **sempre**. O gatilho disparava no
primeiro tique, a função da missão o desligava, e a pesquisa concluída depois
nunca ligava o bit do jogador: os cinco objetivos ficavam pendentes e não havia
vitória.

No original, para um jogador específico, ele dispara **só** quando o jogador
tem a tecnologia (`loc_49275B`: liga o bit e devolve 1; senão 0). Só no modo
"qualquer jogador" (`−1`) vale "máscara ≠ 0". Isso afetava todo gatilho de
pesquisa de todas as missões (a T03 também usa).

### 2. A praga não era desenhada (`sub_46FF60` / `sub_4A88D0`)

A praga **se espalhava**: a E02 a planta perto do tique 13200 (`SetVirusUL` em
7 cantos na borda de baixo e `SetMicrobeSpreadSpeed(170)`), e ela passa de
2900 células logo depois. Mas o porte **nunca a desenhava** — por isso parecia
não existir nem crescer.

No original, ela é parte do **overlay do computador**: com a opção *Display
Computer Overlay* (`dword_57562C`) ligada, o laço dos tiles do terreno
(`sub_46FF60`) monta, no tile tocado pela praga (`0x20000000`), a máscara dos
quatro cantos com vírus (`0x40000000`: `(x,y)` = 1, `(x+1,y)` = 2, `(x,y+1)` =
4, `(x+1,y+1)` = 8), e o desenho do tile (`sub_4A88D0`) a passa às rotinas de
pixel (em `sub_5865EB`, que o IDA deixou como bytes crus — desmontados com o
capstone a partir do `OUTPOST2.EXE`):

- máscara `0xF`, tile inteiro (`0x5866C3`): um **xadrez** na cor `0x3018` — um
  roxo, em RGB555 — no tile todo;
- máscara parcial (modo 1, `0x58665B`): o mesmo xadrez só onde o **molde** liga
  o bit;
- a paridade (`dword_586390`) é `x ^ y` do destino e vira a cada pixel e a cada
  linha: o pixel fica roxo quando `(X + Y)` de tela é **par**. A cor vai direto,
  sem a paleta da escuridão.

O molde (16 padrões × 32 linhas, `unk_57ABAC` = `dword_57AB98 + 0x14`, membro do
objeto do micróbio) vem do arquivo **`VIRMASK.RAW`**, lido pelo `sub_49E180`:
cada palavra em big-endian e invertida. No original, sem o arquivo o jogo nem
abre (`TApp::Init`). O porte o lê dos volumes ou da pasta do jogo; **sem ele**,
usa um molde de reserva em que cada quarto do tile acompanha o seu canto
(acréscimo do porte — as bordas ficam mais quadradas; o miolo não usa molde).

### O teste — `--e02test [DLL] [bmp]`

Carrega a E02, mostra os gatilhos de pesquisa e de vitória e os objetivos,
roda até a praga se espalhar, conclui as cinco pesquisas pelo caminho do
laboratório (`researchMarkComplete`) e confere: a praga é plantada e passa de
100 células; com o Overlay, a vista sobre ela tem pixels roxos; sem o Overlay,
nenhum; e os cinco objetivos de pesquisa ficam cumpridos. Com o `bmp`, salva o
quadro com a praga.

## 5.36. O botão DUMP, e a doca da mina que travava depois dele (2026-11-18)

Pedido: *o botão de descartar a carga dos transportes não funciona — o que está
carregado não é descartado.*

### O botão, conferido

O botão **DUMP** ("D&ump vehicle's cargo") existe no original e funciona como
o porte já fazia: só fica ativo com **um** veículo selecionado que tenha carga —
caminhão ou ConVec, que descarta o **kit** (`loc_455710`); pergunta antes
(*"Are you sure you want to dump this cargo?"*, `sub_4557B0(0)`); o **&YES**
manda o pacote `0x0E`, que põe a ação 14 (`sub_40E210`, caso 14, depois do
`vt[0x74]`); a ação toca o som `0x45`, espera a animação `0x7BC` e zera a carga
(`jpt_43AD56` / `jpt_43B312`, caso 14). O teste novo descarta pelo **mouse de
verdade** — o botão da botoeira e o &YES — em todos os estados: parado, com os
nove tipos de carga; o ConVec com kit; andando; na rota de minério, carregado;
atracado na fundição; atracado na mina. A carga some em todos.

### O defeito: depois do descarte na doca, a mina travava

O caminhão que descarta **atracado na mina** termina a ação 14 com a ação 0,
mas guarda a ordem da rota de minério. O próximo caminhão chega, o empurra
(5.24), e o empurrado recebe a rota curta para sair (`vt[0x7C]`, 5.30) — só
que o laço "sem ordem" do porte só rodava para quem **não tem** ordem. Com a
ordem velha, o laço comum o via "parado", zerava o sub-estado, vinha outro
empurrão, e assim para sempre: ele nunca saía da doca e a mina não carregava
mais ninguém. Agora quem tem ordem mas nenhuma ação de movimento segue a rota
curta como os outros empurrados.

### O teste — `--dumptest <mapa>`

21 casos: os estados acima, o descarte com cada uma das 6 abas do painel aberta,
e depois do descarte na doca da mina, outro caminhão ainda carrega ali (antes:
3000 tiques sem carregar; agora, 113). `DUMP_W`/`DUMP_H` mudam o tamanho da
janela (passa em 800×600, 1280×960, 1366×768 e 1920×1080); `DUMP_GRID` e
`DUMP_SHOT` salvam a botoeira e a pergunta. E `--dumpmission <DLL>` mostra a
botoeira de cada caminhão/ConVec de uma missão de verdade e clica no DUMP: na
E02 eles começam **vazios**, e sem carga o botão nem aparece (como no original).

## 5.37. O ConVec: o kit no quadro e a doca da Structure Factory (2026-11-18)

Pedido: *ao guardar ou pegar um kit (Agridome, Smelter...) na Structure
Factory, não há a animação da doca abrindo e de parte do ConVec descendo; e o
quadro do ConVec não diz que kit ele leva — só dá para saber que leva algo
porque aparece o botão de construir.*

### O texto do quadro de veículo (`sub_46D880`)

O quadro do porte desenhava o nome, o ícone, a blindagem e os pontos de vida de
um veículo e parava ali: a parte de texto só existia para prédios. O original
monta, para **um** veículo selecionado, um texto marcado no mesmo retângulo das
seções dos prédios (`13..W-14`, abaixo do cabeçalho):

- duas linhas de **melhoria** do tipo — o texto da tecnologia em `+0x3C`/`+0x40`
  do bloco do dono das estatísticas; a segunda some se repetir a primeira;
- e, pelo tipo:
  - **ConVec**: `Cargo` e, à direita, o **nome do kit** (`desc+0x1F8`); no kit
    do Guard Post (`0x17`), o nome da **arma** dele (`[+0x4F]`); sem kit,
    `(empty)`;
  - **caminhão**: `Cargo` e o nome da carga (`sub_406210`) com `:quantidade` —
    sem a quantidade quando ela é 0 ou a carga é entulho, espacial ou destroço;
  - **tanques** (Scorpion, Lynx, Panther, Tiger): mais duas linhas, as
    melhorias da **arma**;
  - **Evacuation Transport**: `Cargo` e `Colonists` ou `(empty)`.

Os nomes de carga do caminhão ganharam os dois que faltavam: `Wreckage` (9) e
`Gene Bank` (10).

### A doca do ConVec (`sub_410E00` / `sub_410E50`)

A vtable do ConVec (`off_4CF990`) tem duas entradas próprias para isso:

- **`vt[0x80]` (`sub_410E00`)**, a animação: no sub-estado `0x0D` (na doca),
  `0x1C8` (Eden) ou `0x1CA` (Plymouth) na ida — a porta da fábrica abre, o kit
  desce para o fosso — e `+1` na volta (bit `0x1000000`); fora da doca, a
  animação comum (`sub_44C4F0`).
- **`vt[0x20]` (`sub_410E50`)**, o quadro: na doca, conta desde `[+0x5E]` e
  para no último; fora dela, parado sem ação ou na ação `0x1E` sem andar, e
  ciclando com qualquer outra ação.

O porte já punha o ConVec no sub-estado `0x0D` e trocava o kit, mas desenhava a
animação comum e media as duas fases por ela — poucos tiques, em vez dos ~120
de cada fase da doca, e por isso os sons da garra e da porta (no contador em
`0x51`/`0x25` e `0x4D`/`0x1C`) nunca tocavam. Agora `[+0x3C]` de cada fase é
`vt[0x80](0)` → `sub_404C70` (quadros − 1) como no original (`sub_415880` e o
`case 13` de `sub_43DE50`), com o atalho de 4 tiques da trapaça
`dword_56DE68` só na segunda fase, e o desenho e o teste de clique usam a
animação da doca.

### Os testes

- `--vehpanetest <mapa> [tela.bmp]` — 9 casos: ConVec vazio, com Agridome, com
  Tokamak e com Guard Post (a arma); caminhão com comida (com a quantidade), com
  entulho (sem ela) e vazio; Evacuation Transport; Lynx (sem linha de carga).
- `--convecdocktest <mapa> [prefixo]` — 40 casos, nas duas facções e nos dois
  sentidos (guardar o kit na baia e pegá-lo de volta), um tique por vez: entra
  na doca, cada fase dura a animação inteira (119 e 126 tiques), desenha a ida e
  a volta, o quadro chega ao fim de cada uma, sai da doca e o kit trocou de
  lugar. Com `<prefixo>`, grava quadros da animação.

## 5.38. O minimapa: a praga, as cores dos jogadores, os beacons e os destroços (2026-11-18)

Pedido: *o micróbio não aparece no minimapa — no original aparece, em escala, o
surgimento e o crescimento dele.*

### A praga no minimapa (`sub_48D1E0`)

O original não desenha a praga no minimapa a cada quadro: o `sub_48D1E0(x, y,
0x301B)` grava a cor (roxo, RGB555) **direto na superfície** do minimapa
(`[+0x10A0]`) e na textura do globo (`[+0x10A4]`), e ela fica lá. Só o micróbio
chama essa função — tornado, raio, meteoro e terremoto não marcam o minimapa
(o minimapa percorre a lista de efeitos, mas dela só marca os beacons e os
destroços — ver abaixo). Os dois chamadores:

- o **espalhamento** (`sub_49DC90`): cada canto que pega o vírus marca a sua
  célula — é assim que a mancha cresce no minimapa junto com a praga. No mapa de
  largura 512, só na metade visível (deslocamento < x ≤ deslocamento + W/2);
- a **repintura** (`sub_49E2D0`, `vt[8]` do micróbio): toda célula com os
  **quatro** cantos infectados. Roda depois da carga do mapa e do jogo salvo
  (`sub_435CC0`) e depois de **qualquer** botão do minimapa (`sub_458B40`).

A superfície só é refeita (`sub_437250`) na carga e na vista tática; o zoom e o
globo não a refazem, só repintam por cima. O porte já tinha o lugar anotado no
espalhamento, mas nada pintava. Como ele refaz o terreno do minimapa a cada
quadro, a superfície da praga virou uma camada à parte, por célula (x interno),
que o minimapa e o globo leem.

**Correção:** no original o roxo ficava no minimapa até a superfície ser refeita
mesmo depois de o vírus sumir (a lava apaga os cantos, `sub_49E680`, e o
`SetVirusUL(x, y, 0)` também) — o mapa mostrava praga onde não havia mais. Aqui o
canto limpo leva a marca junto.

### As cores dos jogadores (`sub_490C00`)

O porte pintava as unidades com uma tabela fixa de 7 cores em tons pastel. O
original monta a tabela `[+0x1060]` (8 entradas) com `sub_490C00` para cada
jogador: a entrada 4 da rampa da cor dele (`word_57C420`, a luz 0 — o
`playerRampColor(cor, 4)` do porte) com cada canal **saturado**: acima de 16
vira 31, senão 0. Sai azul, vermelho, verde, amarelo… puros. As entradas sem
jogador ficam `0x7FFF` (branco).

### Os beacons e os destroços (a lista `dword_5737D0`)

Depois dos jogadores, o `sub_48CCD0` percorre a lista de efeitos e marca, na
cor `[+0x106C]` (a 7ª entrada — branca):

- o **mining beacon** (`0x51`) ainda sem mina (`[+0x66]`); o de minério raro
  (`vt[0x98]`) só para quem já pesquisou a tecnologia da Rare Ore Mine
  (`desc 0x16 +0x1E4`);
- o **destroço** (`0x54`) que o jogador já conhece (`vt[0x9C]`).

O porte não marcava nenhum dos dois.

### Os testes

- `--e02test` ganhou 9 casos (13 no total): a praga aparece no minimapa
  (1432 células em 8000 tiques) e a mancha cresce a cada 1000 tiques; toda
  marca tem vírus no canto; os pixels `0x301B` estão na tela; a superfície
  refeita fica sem roxo e a repintura traz de volta exatamente as células de
  quatro cantos; o canto limpo leva a marca; e a praga segue marcando depois.
- `--minimaptest <DLL> [tela.bmp]` — 11 casos: as cores saturadas, o branco de
  quem não é jogador, os beacons (comum, raro antes e depois da pesquisa, com
  mina), o destroço antes e depois de conhecido, e os pixels na tela. Passa
  na E02 (Eden, azul) e na P04 (Plymouth, vermelho).

## 5.39. Pesquisa e produção que falhavam "ao acaso": o anel anti-duplicata, e o diagnóstico `CommandDebug` (2026-11-18)

Relato, jogando a missão 3 do Eden: *mandar pesquisar no laboratório às vezes
só pega na 3ª ou 4ª tentativa; o mesmo com a produção na fábrica (Lynx, Scout,
Robo-Miner); o Medical Center levou umas 10 tentativas. É aleatório.*

### A causa: o anel guardava a chave sem o turno

O anel anti-duplicata `[_Player+0xBD8]` (item 21c da seção 4, `sub_40E210`)
descarta o comando de produzir, pesquisar, treinar, lançar, das cargas e de
ociosar/religar quando a **mesma unidade** já foi assunto de um desses comandos
nos turnos `T−4` a `T−2`. O original grava no slot `T % 16` só a chave — o
`[unidade+0x10]`, que é o **índice** da unidade (`sub_439910` o põe na
alocação, nos dois ramos) — e **nunca limpa** o slot: não há outra escrita em
`+0xBD8` no executável, e o anel de pacotes de turno (`+0x4C8`, 16 slots de
`0x71` bytes) termina exatamente em `+0xBD8`, sem sobrepor.

Dezesseis turnos (64 tiques) depois, o slot volta à janela com a chave velha.
Quem manda sempre nos mesmos prédios — a fábrica, o laboratório — vai enchendo
os 16 slots com as chaves deles, e daí em diante quase todo comando novo para
um desses prédios casa com algum registro antigo e é **descartado em
silêncio**. Medido no teste abaixo, com a missão 3 rodando e um pedido de
fábrica e um de laboratório a cada 37 tiques: **297 de 400** pedidos
descartados. O clique só pega quando a janela calha de cair em slots com chave
de outra unidade — daí o "na 3ª, 4ª, 10ª tentativa".

**Correção:** o slot guarda a chave **e o turno** em que foi gravado
(`marcaDoAnel`: 19 bits de turno, 12 de chave), e cada slot da janela só casa
com o turno que ele representa. A proteção continua de pé — o mesmo comando
repetido 2 turnos depois ainda é descartado —, mas o registro velho não casa
mais. O anel continua `int[16]`, então o jogo salvo não muda de formato (um
salvo antigo traz chaves sem turno, que simplesmente não casam), e as duas
máquinas da partida em rede fazem a mesma conta.

Com isso, os comentários que diziam que o anel "não foi portado" em
`processProducePacket` e `processStartResearch` foram corrigidos: ele filtra
antes, no despachante.

### O diagnóstico: `[Game] CommandDebug` (acréscimo deste porte)

Pedido junto: *quando não conseguir, mostrar uma mensagem dizendo o que
falta.* O original recusa em silêncio — só a falta de minério ou de comida toca
o som `0x58`. Com `CommandDebug=1` no `OUTPOST2.INI`, cada recusa de um comando
do jogador **local** vira uma linha no registro de mensagens (sem som, na
posição do prédio, então o clique na mensagem leva até ele) e no stdout:

| Onde | Mensagem |
|---|---|
| fábrica já produzindo / universidade já treinando | `already producing` / `already training` |
| prédio ocupado com outra ordem | `busy` |
| obra / desmonte / ociosado pelo jogador | `still under construction` / `being dismantled` / `idled` |
| dano acima de 3/4 | `too damaged` |
| EMP e afins (`0xC0000`) | `disabled (EMP)` |
| sem energia (ligado ou não ao CC pelos tubos) | `not enough power` / `no tube link to CC` |
| sem trabalhadores / cientistas | `not enough workers` / `not enough scientists` |
| minério | `needs N common ore` / `needs N rare ore` |
| Food Cargo | `needs 10000 food` |
| plataforma do Spaceport ocupada | `launch pad occupied` |
| tópico já pesquisado | `topic already researched` |
| o anel descartou | `repeated command ignored` |

O laboratório e a universidade que recebem a ordem parados (sem energia,
trabalhadores ou cientistas) não recusam no original: devolvem 1 e não começam
nada. O diagnóstico avisa esse caso também. O motivo sai de
`structureDownReason`, que segue as mesmas condições de `structureOperational`.
Só exibe: não toca na simulação. Padrão `0` (mudo, como no original). Como as
outras chaves do porte, também se troca pelo jogo: a caixa **Command Debug**
(1210) na coluna *Port extras* do `GAME PREFERENCES` do menu (a barra da noite
e a nota desceram 22 px para abrir lugar) e a linha *Command Debug* `On`/`Off`
no `Game Preferences` da aba Options, que vale já no `&APPLY`.

### Os testes

- `--cmddebugtest <DLL>` — 20 casos na própria missão (E03, Eden; P03,
  Plymouth): cada motivo forçado um a um na Structure Factory e no laboratório,
  a pesquisa que não começa sem cientistas, o repetido que o anel descarta, a
  chave desligada (nada no registro) e ligada (uma linha); e a missão rodando
  com 400 pedidos espaçados — **todos aceitos**. Com o anel antigo, 297
  descartados.
- `--item21test` ganhou 2 casos (17 no total): o registro de 16 turnos atrás
  não descarta mais, e o repetido 2 turnos depois continua descartado.
- `--optmenutest` confere a linha nova (nasce `Off`, o clique a liga, nada vale
  antes do `&APPLY`, que grava `CommandDebug=1` e liga o diagnóstico na hora);
  o `--uitest` passa a caixa 1210 pelo mesmo ciclo das outras da coluna.

Mutações: o anel sem o turno (pega nos dois testes), o anel mudo, o
laboratório parado mudo, o motivo dos trabalhadores apagado e a chave ignorada —
6 de 6 pegas.

## 5.40. Fila de produção: vários itens num pedido só (`ProductionQueue`, acréscimo deste porte) (2026-11-19)

Pedido: *é um saco mandar construir um, esperar e mandar o próximo. Como em
Dark Colony, cada clique numa unidade soma +1, dá para pedir várias coisas em
fila, e o clique que o minério não cobre não soma — mas com uma opção no Game
Preferences, mantendo o jeito do original quando desligada.*

**O original não tem fila.** A fábrica produz um item por ordem: escolhe na
lista (`sub_4683F0`), `BUILD IT` (`sub_467D50` → pacote 9), e o painel volta ao
status até o item ficar pronto (`sub_467FF0` fecha a lista com a ação 9). Com
`ProductionQueue=0` (o padrão) tudo continua exatamente assim.

### Com a opção ligada

- **O pedido.** Cada clique num item da lista soma +1 (o duplo conta dois); o
  número aparece em amarelo **na frente** do item, numa coluna fixa, e o
  segundo título vira *"Click +1, Right-click -1"*. O clique direito tira a
  **última** unidade pedida daquele item. Itens diferentes entram na ordem dos
  cliques. Sair pelo `STATUS` sem confirmar descarta o pedido.
- **A conferência.** O clique só soma se o minério comum e o raro (e a comida
  da Food Cargo) cobrem o pedido **mais o que já está nas filas de todas as
  fábricas** do jogador — o minério é um só. Senão não soma e toca o `0x58`, o
  mesmo som de "sem recursos" do original.
- **`BUILD IT`** (embaixo da lista, onde sempre esteve) manda o pedido para a
  fila da fábrica. **`ST&OP`**, ao lado dele e também no quadro de status, para
  o item atual e cancela o resto da fila.
- **A produção.** A cada tique, a fábrica parada recebe o próximo item da fila
  pelo **mesmo pacote 9** do `BUILD IT`, desde que valham as regras do botão
  `BUILD` (`sub_456000`: operando, baia livre na Structure Factory, limite de
  unidades) e do `BUILD IT` (`sub_468430`: minério, plataforma do Spaceport).
  Falta alguma coisa? O item espera na fila até dar. O minério só sai quando o
  item **começa**, como no original.
- **Produzindo**, o `BUILD` continua disponível e a lista fica aberta, para
  somar mais itens. Acima dos botões aparece o item em produção com a
  porcentagem e a mesma barra do quadro de status (`0x3E0` sobre `0x1E0`), e à
  direita quantos ainda esperam (`+N`); parada, *"Waiting: <próximo>"*. No
  quadro de status da fábrica, abaixo da barra de progresso, *"Queued"* e
  *"Next"*.

### Por que não muda a simulação

A fila mora só do lado do jogador local (`src/game/production_queue.cpp`): a
simulação continua recebendo um pacote 9 por item, igual ao do clique. A
partida em rede segue no lockstep (o outro lado vê só os pacotes, e cada
jogador pode ter a opção como quiser), o anel anti-duplicata não estranha
(entre dois itens passa a produção inteira) e o jogo salvo não muda de formato.
O preço disso: a fila **não é gravada** no jogo salvo — ao carregar, as fábricas
voltam sem fila. Na rede o pacote leva `kTurnLead` turnos; a fila espera a
fábrica começar e, passados 24 tiques sem isso, tenta de novo.

Desligar a opção no meio da partida apaga as filas na hora.

### Telas

A caixa **Production Queue** (1211) na coluna *Port extras* do `GAME
PREFERENCES` do menu (as caixas da coluna passaram a 20 px uma da outra, para
a nota *"(not in the original)"* não encostar no valor do volume) e a linha
*Production Queue* `On`/`Off` no `Game Preferences` da aba Options, que vale já
no `&APPLY`.

### Os testes

- `--prodqueuetest <DLL> [tela.bmp]` — 30 casos na Structure Factory da missão
  (E03 e P03): desligada, o clique só seleciona, o duplo pede o `BUILD IT`, o
  `BUILD` some produzindo e a lista fecha; ligada, +1 por clique na ordem, o
  direito tira o último, o clique sem minério não soma (nem com o minério
  comprometido pela fila), o `BUILD IT` enche a fila, a fábrica pega o 1º e o
  minério só sai aí, a lista fica aberta produzindo com o "2x" e a barra, três
  itens produzidos em seguida **na ordem dos cliques**, o item sem minério
  espera e começa quando o minério chega, o `ST&OP` para o atual e o resto, e
  desligar apaga as filas.
- `--optmenutest` e `--uitest` passam a linha e a caixa novas pelo mesmo ciclo
  das outras chaves do porte.

Mutações: o tique da fila desligado, a fila sem comprometer o minério, a lista
fechando produzindo, o direito tirando o primeiro, o `ST&OP` sem limpar, o
`BUILD` fechado produzindo, desligar sem apagar, o clique sem conferir o
minério e o clique sem pedido — 9 de 9 pegas.

## 5.41. A luz das unidades na noite, e os sprites que escurecem (2026-11-19)

Relato, comparando com um vídeo do original (Eden, missão 4): *por onde os
nossos veículos passam o escuro se abre, e onde eles saem o escuro volta — no
porte é diferente.* Os prints do original mostram, à noite, um **círculo de
luz** em volta de cada prédio e de cada veículo, com a borda pontilhada, e os
prédios fora da luz **apagados**.

### O mapa de luz (`sub_49EA90` → `sub_43A200`)

A vista do original guarda um mapa de bits, um bit por tile (`[vista+0x810]`).
A cada quadro, sem luz em toda parte (`dword_56DEA4`), `sub_49EA90` o zera e
chama o `vt[7]` de cada unidade visível. Nos veículos e prédios esse é
`sub_43A200`: com a luz acesa (`[+0x44]` bit 0 — os faróis dos veículos; os
prédios ligam junto com a energia), marca um **círculo** cujo raio é o
**alcance de visão** da ficha (bloco do dono `+0x18`, o "Sight range" das
planilhas):

```
para dy = -r .. r:   t = dy > 0 ? dy - 1 : dy
                     w = isqrt(r*r - t*t)          (byte_4E0C68: isqrt de 0 a 81)
                     linha cy + dy, colunas cx - w .. cx + w + 1
```

`sub_46FD50`/`sub_46FE60` comparam com o quadro anterior e só redesenham os
tiles que mudaram — é por isso que a luz acompanha o veículo.

### O terreno (`sub_46FF60` → `sub_4A88D0`)

Cada tile lê os bits dos **quatro cantos** — `(x,y)` = 1, `(x+1,y)` = 2,
`(x,y+1)` = 4, `(x+1,y+1)` = 8 (daí o `+1` à direita e o `dy − 1` embaixo no
círculo) — e `sub_4A88D0` o desenha: `0xF` com a paleta **base** (sem
escuridão), `0` com a paleta da coluna, e o resto pixel a pixel pelo molde do
`VIRMASK.RAW` — o mesmo da praga —, no **modo 2** da rotina de pixels
(`0x586746`, código que o IDA deixou como bytes crus e que foi desmontado à
parte): bit ligado = paleta clara, desligado = escura. É a borda pontilhada.

### Os sprites escurecem (correção de uma leitura antiga)

O porte não escurecia nenhuma unidade. A anotação antiga dizia que, "dos 84
pontos que chamam `sub_403E00`, só dois passam a coluna de luz" — mas aquela
conta só viu os `push 0FFFFFFFFh` literais. Os que passam **registrador**
passam a coluna: prédios (`sub_408DB0`), veículos (`sub_44C910`), tanques e
torres (`sub_484CF0`), o chão da garagem (`sub_401970`) e a fumarola
(`sub_405840`). A coluna é a do tile da unidade; os do jogador local e dos
**aliados** dele (`dword_56E2E0`) somam `0x200`, o que lê a **segunda curva**
(`+0x25C`, a mesma noite 9 níveis mais clara). `sub_470810` desenha o sprite
com o mesmo mapa de luz: dentro da luz sai claro, fora sai escuro, e na borda
mistura pelo molde. Continuam sem escurecer, como lá, os efeitos
(`sub_405160`), o beacon, o destroço, o brilho do EMP/ESG e a luz de status
dos prédios.

### No porte

`src/game/light_map.cpp`: o mapa de luz (um byte por tile, com uma coluna e
uma linha a mais, como a vista alargada do original), remontado a cada quadro
para as unidades que podem tocar a vista; a máscara dos cantos; o pixel claro
pelo molde; e os moldes do `VIRMASK.RAW`, que saíram do `game_state`. O
`TerrainRenderer` escolhe a paleta (ou mistura) por tile; o `blitArtFrame`
recebe um gancho (`setArtLight`) que deixa claro o pixel dentro da luz quando
há escuridão. A escuridão de cada sprite segue a coluna, com a curva dos
aliados. A opção `NightDarkness` continua valendo para tudo (a 100 é o
original).

**O padrão da `NightDarkness` passou de 75 para 100** (pedido do usuário, no
mesmo dia): com a luz das unidades, o terreno fica visível onde importa — em
volta da base e dos veículos —, e a noite mais clara do porte deixou de ser
necessária. Quem já tem a chave no INI continua com o valor gravado.

### O teste

`--lighttest <DLL> [tela.bmp]` — 20 casos (E04 e P04): o raio é o alcance de
visão; o tile do veículo e os `r` tiles para os lados claros; na coluna
`cx+r+1` só os cantos da esquerda (o `+1`); embaixo, o `dy − 1`; longe,
escuro; a borda segue o molde pixel a pixel; farol apagado não ilumina; com
luz em toda parte, nada; a curva dos aliados mais clara; o terreno na luz bem
mais claro que fora; o Scout no escuro escurecido, o inimigo mais escuro que
o do jogador, e o Scout na luz **idêntico**, pixel a pixel, ao de dia.

Mutações: o círculo que não marca, sem o `+1`, sem o `dy − 1`, o terreno e o
sprite ignorando a luz, sem a curva dos aliados, o farol ignorado, a luz em
toda parte ignorada e a borda sem molde — 9 de 9 pegas.

## 5.42. O comando padrão: o cursor de ataque (e os outros) sem comando armado (2026-11-19)

Relato: *no original, quando se manda atacar, tem um cursor específico — e os
demais cursores de ação também.*

Os 17 cursores `.ANI` já estavam portados (`src/app/cursors.cpp`), mas o
porte só os mostrava com um comando **armado** no painel. O original tem um
**comando composto**, o comando padrão (`dword_5648A0`, vtable `off_4D4E00`):
com unidades selecionadas e nada armado, `sub_4552C0` junta, **nesta ordem**,
os comandos de `off_4E2C00` disponíveis para a seleção e arma o estado 3:

1. **atacar** (`dword_56484C`) — o `vt[8]` (`sub_453260` → `sub_452E00`) aceita
   qualquer unidade sob o ponteiro cujo dono **não** tem o jogador local como
   aliado; com um Guard Post selecionado, só dentro do alcance (bloco `+0x18`,
   distância em tiles `max + min/2`, com a volta do mapa). Cursor: `attack`;
2. **reprogramar** (`dword_564988`, disponível só com **um Spider** sozinho —
   `loc_454BA0`) — `sub_454E70`: veículo não aliado, que não seja Spider nem
   Scorpion e esteja **paralisado pelo EMP** (`[+0x44] & 0x80000`). Cursor:
   `reprog`;
3. **mover** (`dword_56498C`) — `sub_451640`: sobre um **prédio** do jogador
   local, só a doca dele; nela, o veículo que **não** atraca ali (`sub_44DA80`
   = 0) é aceito como mover, e o que atraca só com a doca **livre** (célula
   `0x7FF0000` = 0) — a doca ocupada fica para o botão MOVE, que continua
   empurrando o parado (5.30); fora de prédio, aceita se não houver unidade do
   jogador local sob o ponteiro. Cursor: o do `vt[4]` — `move`/`no_move`, ou
   `dock`/`nodock` na doca.

Sob o ponteiro, `sub_455390` pergunta o `vt[8]` de cada um e o **primeiro** que
aceitar dá o cursor (`vt[9]`); nenhum aceitando, a seta. O clique
(`sub_455350`) executa esse mesmo comando.

**No porte:** `uiDefaultCommand` (`src/game/commands.cpp`) faz a escolha, o
cursor sem comando armado passa a segui-la, e o clique da ordem — o
**esquerdo**, como no original, ou o **direito** com `RightClickOrders` (5.44)
— executa o mesmo comando: sobre o
inimigo **ataca** (antes só movia até ele), com um Spider sobre um inimigo
paralisado **reprograma**, sobre uma unidade sua **não faz nada** (a seta
avisa), e no chão ou na doca move/atraca. O atalho da rota de minério
(`AutoOreRoute`) continua vindo antes.

O cursor pergunta a **cada quadro**, com a seleção que estiver na tela; o
`uiDefaultCommand` só considera ids vivos e dentro da tabela — uma seleção
velha (unidades recriadas, jogo carregado) derrubava o `uiCommandAvailable`
(achado pelo `--autoroutetest`, que recria as unidades entre os cenários).

**Teste:** o `--cursortest <mapa>` ganhou 11 casos (25 no total): selecionado e
sem mira, o cursor sobre o inimigo, no chão livre, no chão fechado e sobre uma
unidade sua; o Spider sobre o inimigo sem e com EMP; o clique direito sobre
a unidade sua, sobre o inimigo (ação `0x24`) e no chão (ação 2); e a seleção
velha (id fora da tabela, unidade morta) dando a seta. O `--pushtest` passa a
mandar o caminhão para a doca ocupada pelo botão MOVE. Mutações: o padrão sem
atacar, o direito só movendo, o Spider sem exigir o EMP, o mover sobre unidade
sua, o cursor sem o padrão e a seleção sem filtro — 6 de 6 pegas.

## 5.43. Os caminhões da rota de minério que travavam na doca (2026-11-19)

Relato: *com vários caminhões transportando o minério, às vezes o que está
descarregando fica preso na plataforma, todos ficam esperando e ele não
consegue sair. Isso tem de ter um tratamento.*

O `--truckjamtest` (abaixo) reproduziu: com 10 caminhões o ciclo parava de vez
depois de umas 14 entregas. Eram **três** defeitos, todos herdados do original.

### 1. O caminhão da rota é sempre intransponível (`sub_44BD60`)

A função que diz se um veículo pode ser contornado ou empurrado trata **todo**
veículo com a ação `0x27` (a rota de minério) como ocupado de vez. Os
carregados que esperam a doca da fundição cercam o que acabou de descarregar:
ele não acha rota para sair (o planejador vê as vizinhas bloqueadas), eles não
entram enquanto ele não sai. **Correção:** o caminhão da rota que está
**parado entre passos** (sub-estado 0 ou 1, sem velocidade nem giro) e é aliado
de quem passa deixa de bloquear — o planejador o atravessa (custo ×4, como
quem anda) e o passo o manda sair da frente (`sub_44BBA0`). Para o que está no
sub-estado 0, o empurrão do original (`vt[0x7C]`, uma rota sem ordem) não vale
— o laço segue a ordem da rota —, então ele recebe o **desvio** na ordem, como
quem anda.

### 2. O desvio estragava a rota (`sub_44BFD0`)

O empurrão de quem está andando insere um desvio na ordem: o índice do ponto
atual recua um e o desvio é gravado **por cima do ponto já visitado** — que o
original nunca devolve. Numa ordem de ida, tanto faz. Na rota de minério, que
repete os pontos, o ponto sobrescrito podia ser o da doca da mina ou da
fundição: na volta seguinte o caminhão terminava o trecho fora da doca, o
`truckDockStatus` dava 0 e `sub_44DFB0` **abandonava a rota** — o caminhão
parava carregado no meio do caminho (com 20 caminhões, 11 saíam da rota). A
patrulha (`0x28`), que também repete pontos, tinha o mesmo defeito.
**Correção:** o ponto sobrescrito é guardado (`detourIdx`/`detourOld` no
`OrderBlock`) e devolvido quando o desvio termina — na chegada (`loc_44C1D2`)
ou quando ele é pulado por falta de rota.

### 3. A multidão: a troca de lugar

Com 20 caminhões ou mais, o bloco em volta da doca fica tão denso que o
empurrado não tem casa livre para onde ir. **Correção (acréscimo do porte):**
dois caminhões aliados da rota, o da frente parado entre passos e com o
próximo passo vindo **de frente** para a célula de quem anda — o que sai da
doca e o que quer entrar —, **trocam de lugar** no mesmo tique, cruzando um
pelo outro (`setupStep`). Os dois viram na hora para a direção do passo, para
nenhum ficar só girando com a célula do outro já tomada.

### O teste

`--truckjamtest <mapa> [n] [tiques] [tela.bmp]` — uma colônia com fundição e
mina, `n` caminhões (padrão 10) na rota pelo atalho, `tiques` (padrão 20000)
de simulação: nenhuma janela de 2000 tiques sem entrega, ninguém parado numa
doca por 1500 tiques, ninguém saindo da rota. Antes: 10 caminhões paravam em
14 entregas, 20 em 3. Agora, com 3, 10, 14, 20 e 25 caminhões, cerca de 6
entregas por janela (o limite da doca) e ninguém preso mais de ~500 tiques —
inclusive em 60 mil tiques. Mutações (com 10 e 20 caminhões): o caminhão
esperando bloqueando, o parado sem desvio, o desvio sem devolver o ponto e sem
a troca de lugar — 4 de 4 pegas.

---

## 5.44. O mouse do original: o esquerdo faz tudo (`RightClickOrders`, opção do porte) (2026-11-19)

Pergunta do usuário: *como é a movimentação no original, com o botão direito
ou o esquerdo?* No original, **o esquerdo faz tudo**. O porte usava o esquema
dos RTS modernos que veio do fork (o esquerdo só seleciona, o direito dá a
ordem). Agora o padrão é o do original, e o esquema do fork virou opção.

**O original** (`sub_41AAA0`, o tratador de mouse da vista):

- **`WM_LBUTTONDOWN`** com um comando armado — estado 3, o que inclui o
  **comando padrão** composto da 5.42 quando há seleção e nada foi escolhido
  no painel: pergunta o `vt[4]` do comando no ponto; aceitando, guarda o ponto
  e executa (`vt[3]`). Não aceitando, um comando escolhido no painel não faz
  nada; o padrão (`[esi+30h]`) cai na **seleção** da unidade sob o ponteiro.
  Com `MK_SHIFT` o comando é pulado e o clique vai direto para a seleção (já
  portado, 5.32);
- **`WM_RBUTTONDOWN`** (caso 516): cancela o comando armado (`vt[6]`) e **zera
  a seleção** (`dword_5471D0 = 0`), tudo no mesmo clique.

Ou seja: com unidades selecionadas, o clique esquerdo no chão move, no inimigo
ataca, na doca atraca e numa unidade sua a seleciona; o direito desseleciona.
A **caixa** de seleção só começa no caminho da seleção quando não há unidade
sob o clique (`loc_41B174`: guarda o ponto e faz o SetCapture) — com unidades
selecionadas o comando padrão aceita o chão antes, e o arrasto vira ordem de
mover. Para traçar uma caixa nova, desseleciona-se antes com o direito.

**No porte:**

- `GameState::defaultCommandClick` executa o comando padrão no ponto (a rota de
  minério num clique, `AutoOreRoute`, vem antes) e diz se algum aceitou;
- **`RightClickOrders=0`** (o padrão, o original): o esquerdo sem Ctrl/Shift e
  sem mira chama o `defaultCommandClick`; se nenhum aceitar, segue para a
  seleção de sempre (clique ou caixa). O direito desseleciona; com mira, a
  cancela **e** desseleciona;
- **`RightClickOrders=1`** (o esquema do fork): o esquerdo só seleciona, o
  direito executa o comando padrão e, com mira, só a cancela.

A chave tem caixa no `GAME PREFERENCES` (*Right-Click Orders*, 1212 — as oito
caixas da coluna dos extras passaram a 18 px, de +62 a +188, para caber antes
do rótulo da noite) e a linha *Right-Click Orders* na aba Options; vale na
hora, no `&APPLY`.

**Teste:** o `--cursortest` ganhou 8 casos (33 no total): com a opção, o
esquerdo no chão não dá ordem; sem ela, o esquerdo no inimigo ataca, no chão
move (e a seleção fica), numa unidade sua a seleciona sem dar ordem à
anterior, o direito desseleciona, o direito com mira cancela e desseleciona,
e o esquerdo sem seleção não faz nada. O `--selecttest` roda os casos da
caixa com a opção ligada e ganhou 4 do original: com seleção, o arrasto no
chão não faz caixa e manda mover; o direito desseleciona; e aí a caixa marca.
O `--optmenutest` confere a linha, o
clique, o `&APPLY` gravando `RightClickOrders=1` e valendo na hora; o
`--uitest` confere a caixa 1212. O `debugRightClickTile` dos testes passou a
usar o botão da ordem do esquema em vigor, então os testes antigos de ordem
valem nos dois. Mutações: o direito com mira sem desselecionar, o esquerdo
sem o comando padrão, o direito sem desselecionar e a opção não lida no
`&APPLY` — 4 de 4 pegas.

## 5.45. Todas as opções do porte nascem desligadas: `AutoOreRoute` e `DemoCamera` em 0 (2026-11-19)

Regra do usuário: *o padrão é sempre o comportamento do original* — toda
evolução do porte é uma opção do `GAME PREFERENCES` que se liga. Das doze
chaves extras, duas nasciam ligadas: `AutoOreRoute` (a rota de minério num
clique, 5.23) e `DemoCamera` (a câmera que passeia no demo de atração). As
duas passam a valer **0** quando o `OUTPOST2.INI` não as traz
(`src/core/config.h` e os membros do `GameState`); quem já gravou um valor nas
preferências continua com ele. O `--autoroutetest`, cujo assunto é a rota num
clique, liga a opção explicitamente.

## 5.46. O cursor da rota de minério de três cliques (2026-11-19)

Relato: *com o Auto Ore Route desligado, o botão dos 3 passos para a mina
(o das duas setas, de ir e voltar) não funciona — o cursor não muda.*

O botão arma o comando (`S&et an ore-hauling route`, ícone `0x486`, vtable
`off_4D4D38`) e os três cliques já funcionavam. Mas o `cursorForHover` não
tinha o caso dele e mostrava a **seta**, então o botão parecia não armar. E um
clique fora da doca é recusado com o som `0x58`, sem aviso visual. No original,
o `vt[4]` do comando (`sub_454290`), além de aceitar ou recusar o tile, grava o
cursor no 3º argumento:

- alvo aceito na etapa → `dword_56DDFC` = `dword_56DDC8[13]`, o cursor **dock**;
- recusado → `dword_56DE00` = `[14]`, o **nodock**.

As etapas são as de `oreRouteTarget`: 1º a **doca da mina** (ou do Magma
Well), 2º a **doca da fundição** compatível, 3º a doca da mina de novo.

**No porte:** `uiOreRouteTargetOk` (`src/game/commands.cpp`) pergunta o mesmo
`oreRouteTarget` na etapa atual, e o `cursorForHover` mostra dock/nodock.

**Teste:** o `--autoroutetest` ganhou 12 casos (48 no total). Com a rota num
clique desligada, ele aperta o botão de verdade na botoeira do caminhão vazio,
confere que a mira armou e confere o cursor em cada etapa: dock na doca certa,
nodock na outra doca e no chão. Depois faz os três cliques e confere a rota
montada (ação `0x27`, fundição clicada) e a mira desarmada. Mutação: sem o
caso do cursor, 6 falhas.

## 5.47. A transmissão de dados da E04: a unidade mandada para cima de um prédio nunca chegava (2026-11-19)

Relato: *na missão 04 do Eden, o Scout chega perto do Advanced Laboratory e não
faz a transmissão de dados — algum gatilho não funciona.*

**O gatilho estava certo.** A E04 cria três `SpecialTarget` (um por
laboratório do jogador 1, alvo com a flag `0x2000000`, fonte do tipo `0x0B`,
o Scout). No original, a chegada de um veículo ao fim da ordem chama o
`sub_44D5B0`, que olha as 4 casas vizinhas: com um alvo especial, a unidade
entra na ação `0x10`, transmite 60 tiques e o `sub_44D7D0` → `sub_495170`
marca o disparo. Com o Scout parado numa casa encostada no laboratório, o
porte fazia exatamente isso.

**O defeito estava no movimento.** O natural é clicar **em cima** do
laboratório. A ordem leva então à casa do prédio, que é ocupada: a rota
termina na casa mais perto, encostada nele, e dali o planejador não acha rota
para a casa ocupada. O `sub_44C0B0` cai em `loc_44C20A`: com a ordem ativa
(bit 0 de `[ordem+0x34]`) a unidade **não desiste**. Ela espera
`[st+0x70]` tiques e tenta de novo, com a espera crescendo até `0x53`. Isso
foi feito para quem está no caminho por um instante, mas um prédio não sai: o
Scout ficava "andando" (ação 2, sub-estado 1, velocidade zero) para sempre.
Como ele nunca chegava, o `sub_44D5B0` nunca rodava, e o objetivo ficava
impossível.

**Corrigido:** no último ponto da ordem, sem rota e com a unidade **encostada
(8 vizinhas) no prédio** que ocupa a casa do alvo, é a chegada
(`touchesTargetBuilding`, `src/game/movement.cpp`). Vale para qualquer ordem de
mover que mire um prédio. A transmissão continua exigindo um dos 4 lados, como
no original. Nos testes, a rota terminou sempre no meio de um lado.

**Teste:** `--e04test [DLL]` (novo) carrega a E04 de verdade. Ele faz o papel
dos Lynx: elimina as unidades armadas do inimigo e as torres, e repete isso
enquanto os Scouts andam, por causa dos reforços. Depois:

- manda um Scout para a casa encostada no laboratório 3;
- manda os outros dois clicando **em cima** dos laboratórios 2 e 1;
- confere a chegada, a ação `0x10`, o disparo de cada `SpecialTarget`, os três
  objetivos cumpridos e o fim `MISSION ACCOMPLISHED`.

Sem a correção, o Scout do clique em cima fica parado ao lado do laboratório
sem transmitir.

### A rotina do original, comparada com a do porte

| Peça | Original | Porte |
|---|---|---|
| Objetivo do modo 0 | `sub_438390`: a casa do objetivo tem custo negativo, mas a casa com unidade que não é veículo (`test al, 2`) vale `0x7FFFFFFF` antes disso — a casa de um prédio **nunca** é alcançada | `pfMoveCost`: igual |
| Ponto final do MOVE | `sub_451280` → pacote 2 com o ponto clicado, sem troca, num prédio fora da doca; `sub_451420` com folga de chegada **0** no alvo (só os intermediários têm folga) | igual |
| Sem rota para o último ponto | `sub_44C0B0`, `loc_44C16D` → `loc_44C20A`: com o bit 0 de `[ordem+0x34]`, espera `[st+0x70]` tiques e sorteia a próxima (até `0x53`), com o sub-estado 1 | igual até a 5.47; agora, encostada no prédio do alvo, é a chegada |
| Quem apaga o bit 0 | só o caso 8 (Earthworker, `loc_43AED7`) e o 40 (patrulha, `loc_43D6FB`) — o mover nunca | igual |
| Chegada | `jpt_43B312` casos 2-4, só com `[+0x22] == 0`: flag `0x40` → `sub_44D5B0` → `sub_406C20` (caminhão vazio) → `sub_484460` (tipo 8) | `unitArrived`: igual |
| `sub_44D5B0` | recusa o caminhão (1) e o transporte de evacuação (`0x0D`) **carregados**; 4 vizinhas (x−1, x+1, y−1, y+1) com a flag `0x2000000` → ação `0x10` | testava a flag `0x40` no lugar da carga — **corrigido** |
| Ação `0x10` | início (`jpt_43AD56` caso 16): sub-estado 6, `[+0x3C] = 0x3C`; fim (`jpt_43B312` caso 16): `sub_44D7D0`, ação 0 | igual |
| `sub_44D7D0` | nas 4 vizinhas, o 1º gatilho da lista `dword_4EA084` com aquele alvo e o tipo da fonte → `sub_495170` | igual |
| `sub_495170` | grava a fonte (`[+0x34]`) e `[+0x3C] = 1`; gatilho de **uma vez** (`[+0x1C] == 1`): o alvo perde a flag `0x2000000` | não tirava a flag — **corrigido** |

Conclusão: o porte reproduzia o original fielmente, inclusive o defeito, que
também existe lá — no original o Scout mandado para cima do laboratório
também fica tentando chegar para sempre. A missão só se completa clicando no
chão encostado. A correção da 5.47 é um desvio consciente (regra do projeto:
defeito do original se corrige). As outras duas diferenças achadas na
comparação eram do porte e foram alinhadas ao original. O `--e04test` ganhou o
caso "o alvo de gatilho de uma vez deixa de ser especial" (7 casos).

## 5.48. A tela de briefing da campanha: a imagem da missão ao lado do texto (2026-11-20)

Relato: *no original, a tela de briefing das missões traz o texto numa caixa
à esquerda e uma imagem à direita; a nossa não faz isso.*

O porte usava, nas três telas de texto da campanha (briefing, objetivos e
história), o RICHEDIT "StoryText" do template do Dialog 216, com 565 de
largura. O **briefing** tem procedimento próprio (OP2SHELL `0x13005DF0`):

- **`WM_INITDIALOG`:** cria **outro** RICHEDIT com
  `CreateWindowExA(WS_EX_CLIENTEDGE, "RICHEDIT", "StoryText", 0x5030C804, 20,
  25, 230, 295, …, id 0x43F)`, uma caixa de **230** de largura. Depois carrega a
  imagem da missão com `LoadImageA` do DLL de recursos (OP2SHRES). O id sai de
  uma tabela de 24 montada na pilha (`0x190`..`0x1A7`); o índice é a missão
  (0-based, `0x13015B9E`) **+ 12** quando o jogador não é Eden (bit 2 da
  palavra dele). São 12 imagens do Eden e 12 do Plymouth, de 320×240 (a
  `0x19A` tem 322×242);
- **`WM_PAINT`:** `BitBlt(hdc, 260, 50, largura, altura, memDC, 0, 0,
  SRCCOPY)`, ou seja, a imagem desenhada em (260, 50) da área cliente, à
  direita da caixa;
- título `"%s Mission %i: Briefing"` e texto `B_%s%02i.rtf`, como já era.

Objetivos e história continuam com a caixa larga do template, sem imagem.

**No porte:** `openRtf(0)` carrega o bitmap `0x190 + (Eden ? 0 : 12) +
missão` do OP2SHRES, e o `renderRtfView` o desenha em (kRvX + 260,
kRvY + 7 + 50) com a caixa de texto de 230 (`rtfTextW`). A quebra das linhas,
a barra de rolagem e a roda do mouse seguem essa largura.

**Teste:** `--brieftest` (novo) abre o briefing das 24 missões e confere:

- a imagem de cada uma, comparada pixel a pixel com o recurso `0x190 +
  missão` (Eden) ou `0x19C + missão` (Plymouth), lido por conta própria do
  DLL;
- o tamanho de 320×240;
- a caixa de texto de 230;
- nos objetivos, a caixa larga e nenhuma imagem.

Mutação: sem o `+ 12` do Plymouth, as 12 missões dele acusam a imagem
trocada.

## 5.49. A rolagem da caixa de texto pelo mouse: a roda e a barra (2026-11-20)

Relato: *a barra de rolagem da área de texto do briefing não funciona pelo
mouse; rolar com a roda e com a barra.*

- **A roda:** o código já rolava 3 linhas por degrau, mas lia só o
  `wheel.y` do SDL. Rodas de alta resolução e touchpads mandam `y = 0` em cada
  evento e o movimento só no fracionário `preciseY`, e aí a roda não fazia
  nada. `handleMouseWheelPrecise` acumula o `preciseY` (com o sentido
  invertido de `SDL_MOUSEWHEEL_FLIPPED`) e entrega degraus inteiros. Vale para
  todo o shell (`ShellState` e o laço do `main`).
- **A barra:** era só desenho. O estilo `0x5030C804` do RICHEDIT tem
  `WS_VSCROLL`, então agora a barra funciona como a do Windows:
  - as setas rolam 1 linha;
  - a trilha rola 1 página e para quando o polegar chega ao cursor;
  - seta e trilha repetem com o botão seguro (400 ms, depois a cada 50 ms);
  - o polegar se arrasta, também além das pontas.

  A barra com setas é a mesma nas duas peles (a do porte tinha uma trilha fina
  sem setas), e o texto quebra **antes** dela (8 de margem + 17 da barra). Vale
  no briefing, nos objetivos e na história.

**Teste:** o `--brieftest` ganhou 9 casos (15 no total), no briefing do
Eden 5:

- a caixa tem barra;
- meio degrau de roda ainda não rola, e o degrau inteiro rola 3 linhas;
- a roda invertida rola ao contrário;
- a seta de baixo desce 1 linha e a de cima sobe 1;
- a trilha desce uma página;
- arrastar o polegar além do fim vai ao fim, e além do topo volta ao início.

## 5.50. Barra de rolagem e roda em todas as listas com texto (2026-11-20)

Pedido do usuário: *em todos os campos de texto do tipo lista, as funções da
barra de rolagem e a rolagem pela roda do mouse.*

**Nos menus (shell), é fidelidade.** No original, essas listas são controles
do Windows (LISTBOX, RICHEDIT, SysListView32), que já trazem a barra que se
arrasta e a roda. O porte tinha só desenho, ou setas que andavam 1 linha.

Um mecanismo comum (`ScrollTarget`, `scrollTargets()` em
`src/shell/shell_menu.cpp`) atende todas: cada tela informa a barra, a área da
lista, a variável de rolagem, o total e as linhas visíveis. O desenho
(`drawSkinScrollBar`, nas duas peles) e o mouse usam a mesma geometria. Valem:

- as setas (1 linha) e a trilha (1 página, parando no cursor), as duas
  repetindo com o botão seguro;
- o arrasto do polegar;
- a roda na lista **sob o cursor**, 3 linhas por degrau.

As listas atendidas:

- **tutoriais;**
- **manual:** a lista de tópicos e também o **texto**, que antes só mostrava a
  porcentagem e agora quebra antes da barra;
- **placar;**
- **capítulos:** são 13 na missão 12 e cabem 8 linhas, então os últimos
  ficavam **cortados**, sem como rolar. Agora rolam, com o clique e o teclado
  contando a rolagem;
- **briefing, objetivos e história** (5.49), agora pelo mecanismo comum.

As listas que sempre cabem (jogos de colônia, sessões de rede) não mostram
barra, como no Windows.

**Na partida, é extra do porte** (chave `ListScrollBars`, padrão **0**). As
listas da partida são o controle próprio do original (`sub_459940`), que só
tem as duas setas de **página**. Com a chave ligada:

- a lista ganha uma barra de 11 px na margem de 14 px que o `listBoxLayout`
  já deixa à direita dela, então o texto não muda;
- as setas de página saem;
- a roda rola a lista sob o cursor.

O mecanismo é um **registro**: cada lista se registra ao se desenhar
(`drawListBoxScroll`, no lugar do `drawListBoxArrows`), e a GameState trata
clique, arrasto e roda pelo registro do último quadro, antes dos painéis.
Assim valem todas as listas sem mexer no clique de cada painel:

- relatórios por ReportList: Factories, Labs, Resources, Comms, Space e
  Options;
- objetivos, pesquisa (as duas listas), produção e lista de veículos.

Desligada, `drawListBoxScroll` desenha as setas do original como antes e o
mouse não muda nada.

A chave tem caixa no `GAME PREFERENCES` (*List Scroll Bars*, 1213; as nove
caixas da coluna passaram a 16 px, de +62 a +190) e a linha *List Scroll
Bars* na aba Options.

**Testes:**

- `--listscrolltest` (novo, 25 casos): tutoriais, manual (lista e texto) e
  capítulos. Em cada um, a roda sob o cursor rola só aquela lista, a seta desce
  1, e o polegar arrastado vai ao fim e volta ao topo. Nos capítulos, o clique
  no item visível conta a rolagem;
- `--panescrolltest [DLL] [tela.bmp]` (novo, 13 casos), na lista do Game
  Preferences, a 640×480:
  - desligada: a roda e a margem não agem, e a seta de página desce uma
    página;
  - ligada: meio degrau de roda não rola, o degrau inteiro rola 3 linhas, a
    seta da barra rola 1, a trilha uma página, o polegar vai às duas pontas, e
    a seta de página oculta não age.

## 5.51. A rolagem vertical do mapa pela borda e a roda no mapa (2026-11-20)

Relato: *a rolagem para baixo e para cima no mapa está ruim — muito lenta ou
não anda; para a esquerda e para a direita está normal.* E o pedido: *a roda do
mouse rolando o mapa para cima e para baixo, com a opção de rolagem do mouse.*

**O defeito.** A lógica da rolagem é simétrica e funcionava (o novo
`--missionscrolltest` anda 80 px em 20 quadros para os quatro lados numa
missão). O problema era a **entrada**. O porte só rolava com o cursor **dentro**
da janela (`SDL_WINDOW_MOUSE_FOCUS`), lido em coordenadas da janela:

- numa janela maximizada, as bordas esquerda e direita são as da tela, o cursor
  para ali e continua dentro, então a rolagem horizontal funcionava;
- em cima fica a barra de título do Windows e embaixo a barra de tarefas. Ao
  empurrar o cursor, ele **saía** da janela, o jogo perdia o foco do mouse e a
  rolagem parava. Só uma faixa de 3 px rolava, e a rolagem vertical parecia
  muito lenta ou parada.

**O original** (`sub_407A10`) faz `GetWindowRect` + `GetCursorPos`, os dois
em coordenadas de **tela**, com o retângulo encolhido 2 px de cada lado: rola
para a esquerda com `cursor.x <= left`, para a direita com `cursor.x >=
right − 1`, e o mesmo em y. O cursor **além** da borda, fora da janela,
continua rolando.

**Corrigido:** `scrollView` lê a posição **global** do cursor
(`SDL_GetGlobalMouseState`) relativa à área cliente (`SDL_GetWindowPosition`),
e as comparações de `scrollWanted` já aceitam posições além da borda. Fica só a
exigência da janela em primeiro plano (`SDL_WINDOW_INPUT_FOCUS`), acréscimo do
porte: janela atrás de outra não rola.

**A roda no mapa** (pedido; mesma chave `ListScrollBars`, cujo rótulo virou
*Wheel & Scroll Bars* no `GAME PREFERENCES` e na aba Options):

- sobre a **vista**, a roda rola o mapa **2 tiles (64 px) por degrau**, para
  cima e para baixo;
- o arrasto lateral do touchpad (`preciseX`) rola para os lados;
- sobre uma lista, a roda continua rolando a lista.

Desligada, a roda não faz nada no mapa, como no original.

**Testes:**

- `--scrolltest`: ganhou o cursor acima, abaixo e à esquerda da janela (rola),
  e a janela sem foco (não rola);
- `--missionscrolltest [DLL]` (novo): a rolagem quadro a quadro numa missão
  real, para os quatro lados;
- `--panescrolltest`: ganhou 4 casos (17 no total). Desligada, a roda sobre o
  mapa não o rola. Ligada, desce 2 tiles e sobe de volta, e sobre a lista rola
  a lista e não o mapa.

## 5.52. Os objetivos da E05 e o "caminhão de carga vazio" (2026-11-20)

Pedido do usuário: *testar a missão 05 do Eden e ver se todos os objetivos são
concluídos normalmente.*

A E05 tem **22 objetivos** (o novo `--objdump <DLL>` lista cada um com o
gatilho por trás):

- RESOURCE ≥ para 40 crianças, 30 trabalhadores e 20 cientistas. A colônia
  começa com 28/22/15, dentro dos transportes, então são metas de
  **crescimento**;
- RESEARCH: Robot-Assist Mechanic (5102) e Geothermal Power (5115);
- EVAC: transportes para toda a população;
- COUNT: 3 caminhões **vazios**, 5 ConVecs, Earthworker, Robo-Surveyor, 2
  Scouts, 4 Lynx, 2 Panthers com Rail Gun (`0x3F`), o satélite (tipo 88 =
  EDWARD, pela contagem espacial) e as cargas nos caminhões (5000 de metal
  comum, 2000 de raro, 3000 de comida);
- KIT: Agridome, Command Center, Smelter, Structure Factory e Tokamak
  guardados.

**Defeito achado (do porte):** "Have 3 or more empty Cargo Trucks" **nunca se
cumpria**. Desde a 5.14 (a carga parcial), o COUNT de caminhões com filtro de
carga conta pela quantidade: a soma das cargas daquele tipo dividida pela
capacidade. A carga **0** é o caminhão **vazio**, cuja quantidade é sempre 0,
então a conta dava 0 caminhões. **Corrigido** em `countForPlayer` e
`countAccumulate`: a conta pela quantidade vale só para carga de verdade (tipo
> 0); vazio conta caminhões, como o `sub_493AF0` original.

**Teste:** `--e05test [DLL]` (novo) cumpre cada objetivo pelo estado legítimo
do jogo:

- a população nas metas, simulando o crescimento, com transportes para todos;
- as pesquisas pelo caminho do laboratório (`researchMarkComplete`);
- os veículos e os caminhões com as cargas;
- os kits nas baias de uma Structure Factory;
- o satélite **lançado** de verdade: espaçoporto com SULV + EDWARD,
  `spaceportLaunch`, em órbita em 30 tiques.

Com tudo isso, os 22 objetivos se cumprem e a missão termina em MISSION
ACCOMPLISHED. Mutação: sem a correção, o objetivo dos caminhões vazios fica
pendente e a missão não termina.

## 5.53. Auditoria: o que cada pesquisa libera (2026-11-20)

Pergunta do usuário: *todas as pesquisas dos laboratórios estão abrindo as
novas funções — por exemplo a reciclagem: primeiro a pesquisa do laboratório,
depois as funções dos veículos sobre ela?*

**O mecanismo.** Cada prédio e veículo traz na planilha a pesquisa que o libera
(`desc+0x1E4`, convertida em índice da árvore ao carregar). Essa pesquisa é
consultada (`researchHas`, o `sub_472C50`):

- na produção das fábricas (`factoryCanProduce`, `sub_4567B0`);
- nos comandos dos veículos (muros, minas raras);
- na lista de veículos, no espaçoporto, no relatório de recursos e nos
  desastres.

Ao concluir, cada pesquisa aplica os seus `UNIT_PROP` no bloco do jogador
(`sub_473860`).

**A reciclagem** é uma cadeia:

1. **Metals Reclamation (03302)** libera o kit do **GORF** na Structure
   Factory;
2. com um GORF **operando**, o caminhão de carga vazio ganha o comando
   **SALVAGE** (`sub_453A20`), e a desmontagem de prédios (ConVec) e a
   reciclagem de kits devolvem metal;
3. **Recycler Postprocessing (05306)** liga o `Improved` do GORF: o retorno
   passa de **50%** para **60%** do custo (`sub_46E5C0`).

**Teste:** `--techunlocktest [DLL]` (novo), em duas partes.

- **A, a reciclagem de ponta a ponta, numa colônia montada no teste:**
  - o kit do GORF não sai antes da pesquisa e sai depois;
  - o caminhão vazio não tem o SALVAGE sem GORF e ganha com ele;
  - a reciclagem de um Tokamak devolve 550 (50%), e 660 (60%) depois do
    Recycler Postprocessing.
- **B, a árvore inteira**, pesquisa por pesquisa:
  - nenhum tipo da colônia exige uma pesquisa que não existe na árvore;
  - nada é liberado **antes** da pesquisa;
  - tudo é liberado **depois**;
  - todos os `UNIT_PROP` estão aplicados.

Rodado em E01/P01 (118 e 120 pesquisas, quase a árvore inteira), E05/P05 e
E12/P12: **nenhuma falha**. A única "exceção" é regra do original, conferida
no asm: o `sub_472C50` devolve 0 para o Basic Lab (0x21) quando o Standard Lab
(0x31) já foi pesquisado, porque o laboratório básico fica obsoleto. O teste
confere essa regra num caso próprio.

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

São 109 modos. Os que cobrem regressão de verdade:

| Modo | O que prova |
|---|---|
| `--savetest <DLL> <ticks> <mais>` | determinismo: salvar, recarregar e continuar tem de dar o mesmo hash |
| `--mplan host\|join <DLL> <ticks>` | lockstep em rede: as duas máquinas fecham com o mesmo hash |
| `--mptest <DLL>` | missão multijogador local |
| `--mpui host\|join` | o fluxo das telas de rede, pelo despacho de verdade dos botões |
| `--uitest` | combos e barras de rolagem, pelo caminho do mouse e do teclado |
| `--chattest <DLL> [tela]` | a linha de conversa: mandar, desistir, a máscara de destinatários e um cheat digitado |
| `--transfertest <DLL>` | o TRANSFER: escolhedor, desistência e a troca de dono |
| `--idletest <DLL>` | o aviso de perder metal ao desligar armazém/fundição, nos dois lados da fronteira |
| `--garagecounttest <DLL>` | os seis pontos em que um veículo guardado em garagem tem de contar |
| `--alerttest <DLL>` | a unidade atacada alerta os vizinhos armados do próprio lado (`sub_44DE10`) |
| `--deletetest <DLL>` | grupo de IA com prazo vencido: autodestruição e, 400 ticks depois, sair do mapa |
| `--cheattest <DLL>` | os cheats do chat: os quatro hashes do executável e o efeito de cada um |
| `--item21test <DLL>` | as vozes da plataforma, a arma no ícone do STORAGE e o anel anti-duplicata (e o registro velho, 5.39) |
| `--optmenutest <DLL>` | a aba Options: os botões do PREFERENCES, o rádio do modo de tela e a tela de fim de missão inteira |
| `--bannertest <DLL> [bmp]` | o letreiro de fim de missão: cor, tamanho, contorno e centragem, medidos no quadro desenhado |
| `--pausetest <DLL> [bmp]` | a pausa do `Ctrl+P`: o relógio para, o som suspende, o letreiro aparece, e em rede nada disso |
| `--autoroutetest <mapa>` | a rota de minério automática: a fundição disponível mais próxima, as três minas, as duas raças, e o ciclo rodando |
| `--pushtest <mapa>` | o veículo parado sai da frente: a doca, a regra da arma, o encurralado, e a invariante de ocupação a cada tique |
| `--artcliptest` | o recorte por pedaço dos sprites: a carga da doca cortada na borda do poço, contra a composição crua |
| `--drawordertest <mapa>` | a ordem de desenho: veículo atrás de prédio, efeitos por cima, desastres por último, a doca |
| `--detourtest <mapa>` | quem vem de frente dá passagem: o encontro no corredor, a tabela de decisão do `sub_44BBA0` e o desvio inalcançável |
| `--exitbeeptest` | o bipe da saída: toca o `beep5.wav` e espera 3,5 s uma vez por sessão; sem som, não espera |
| `--dumptest <mapa>` | o botão DUMP pelo mouse em todos os estados do caminhão, e a mina que segue carregando depois do descarte na doca |
| `--dumpmission <DLL>` | a botoeira de cada caminhão/ConVec de uma missão e o clique no DUMP |
| `--vehpanetest <mapa> [tela.bmp]` | o texto do quadro de veículo: o kit do ConVec, a carga do caminhão, os colonos |
| `--convecdocktest <mapa> [prefixo]` | a doca do ConVec na Structure Factory: a animação, a duração das fases e a troca do kit |
| `--minimaptest <DLL> [tela.bmp]` | o minimapa além do terreno: as cores dos jogadores, os beacons e os destroços |
| `--cmddebugtest <DLL>` | o anel anti-duplicata sem registro velho e cada motivo do diagnóstico `CommandDebug` (5.39) |
| `--prodqueuetest <DLL> [tela.bmp]` | a fila de produção do porte: o pedido, a conferência do minério, a ordem, o ST&OP e o original com a opção desligada (5.40) |
| `--lighttest <DLL> [tela.bmp]` | a luz das unidades na noite: o círculo de `sub_43A200`, os cantos e o molde, o terreno e os sprites (5.41) |
| `--truckjamtest <mapa> [n] [tiques] [tela.bmp]` | n caminhões na mesma rota de minério: o ciclo não para, ninguém preso na doca, ninguém sai da rota (5.43) |
| `--e02test [DLL] [bmp]` | missão 2 do Eden: a praga se espalha e é desenhada com o Overlay, e os objetivos de pesquisa são cumpridos; e a praga no minimapa |
| `--e04test [DLL]` | A transmissão de dados da E04 (`SpecialTarget`): o Scout encostado e o clique em cima de cada laboratório, os três objetivos e a vitória (5.47) |
| `--brieftest` | A tela de briefing da campanha: a imagem de cada uma das 24 missões (OP2SHRES `0x190`+missão, +12 no Plymouth) e a caixa de texto de 230 (5.48) |
| `--listscrolltest` | A barra de rolagem e a roda das listas do shell: tutoriais, manual (lista e texto), capítulos (5.50) |
| `--panescrolltest [DLL] [tela.bmp]` | A barra e a roda nas listas da partida, com a opção `ListScrollBars` desligada e ligada (5.50) |
| `--missionscrolltest [DLL]` | A rolagem da vista numa missão, quadro a quadro, para os quatro lados (5.51) |
| `--e05test [DLL]` | Os 22 objetivos da E05 cumpridos pelo estado de jogo (inclusive o satélite lançado) e o MISSION ACCOMPLISHED (5.52) |
| `--objdump <DLL> [tiques]` | Lista os objetivos da missão com o gatilho por trás de cada um (tipo e campos) e a população (5.52) |
| `--techunlocktest [DLL]` | O que cada pesquisa libera: a cadeia da reciclagem (GORF, SALVAGE, 50%/60%) e a árvore inteira (liberação antes/depois, `UNIT_PROP`) (5.53) |
| `--tubetest <mapa> [prefixo]` | o Earthworker ligando prédios com tubo: retas inteiras, a doca que não liga, a animação de trabalho |
| `--waypointtest <mapa> [bmp]` | pontos de passagem com Ctrl + clique: limites por comando, o pacote com os pontos antes do alvo, a unidade passando por eles, patrulha e obra, e o desenho |
| `--selecttest <mapa>` | a área de clique das unidades, e a seleção com Shift e com o botão do meio (soma, tira, caixa que soma, as exceções) |
| `--stacktest <DLL> [bmp]` | estresse do empurrão: todos os veículos do jogador indo e voltando em grupo; nunca dois no mesmo tile, e ninguém sobra andando ao fim de cada perna |
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
| Combate | Completo |
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
| Multijogador em rede | Completo: até 6 jogadores, conversa, aliança, troca e estatísticas |
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
