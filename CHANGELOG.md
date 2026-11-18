# Changelog

Mudanças do porte, agrupadas por data — a mais recente primeiro. O detalhe
técnico de cada uma está no [PORT_STATUS.BR.md](PORT_STATUS.BR.md).

*Changes to the port, grouped by date — most recent first. The technical
details of each one are in [PORT_STATUS.BR.md](PORT_STATUS.BR.md)
(Portuguese).*

---

## 2026-11-18

### Português

**Corrigido**

- **A mina não trava mais depois de um descarte na doca.** Um caminhão que
  descartava a carga (botão DUMP) atracado na mina ficava preso na doca, e a
  mina não carregava mais nenhum outro caminhão.
- **A missão 2 do Eden agora termina.** As pesquisas pedidas nos objetivos
  ("Reconstruct … database") não saíam da lista, mesmo concluídas no
  laboratório, e a missão nunca acabava. O gatilho de pesquisa disparava no
  primeiro instante da missão, antes de qualquer pesquisa, e era desligado em
  seguida. Agora só dispara quando a tecnologia é de fato pesquisada, como no
  original — vale para todas as missões com objetivo de pesquisa.
- **A praga (Blight) aparece.** Ela se espalhava, mas não era desenhada, então
  parecia não existir nem crescer. Agora aparece como no original: um xadrez
  roxo sobre o terreno infectado, com a opção *Display Computer Overlay* ligada.
  As bordas usam o arquivo `VIRMASK.RAW` do jogo; sem ele, saem mais quadradas.
- **Os veículos de trabalho mostram a animação de trabalho**, como no original:
  o Earthworker assentando tubo ou muro, o ConVec construindo ou reparando, a
  Spider e o veículo de reparo reparando, e o Robo-Dozer terraplanando. Antes
  eles ficavam com a animação de andar girando, parados no lugar.
- **O tubo ligado à doca de um prédio.** Um tubo que terminava na doca (a casa
  onde o caminhão atraca) às vezes ligava o prédio e às vezes não, conforme a
  ordem dos prédios — o tubo parecia pronto e o prédio continuava desligado.
  Agora é sempre como no original: **a doca não liga**; o tubo tem de encostar
  numa parte do prédio que não seja a doca.

**Para quem acompanha o código:** seções 5.34 a 5.36 do PORT_STATUS; os modos de
teste `--tubetest`, `--e02test` e `--dumptest`; o gerador `tools/extract_anim_tables.py` extrai os quatro
conjuntos de animação dos veículos.

### English

**Fixed**

- **A mine no longer jams after a dump at its dock.** A truck that dumped its
  cargo (DUMP button) while docked at a mine got stuck on the dock, and the mine
  loaded no other truck.
- **Eden mission 2 can now be won.** The research asked for in the objectives
  ("Reconstruct … database") never left the list, even when finished in the
  lab, and the mission never ended. The research trigger fired at the very
  first moment of the mission, before any research, and was then switched off.
  It now fires only once the technology is actually researched, as in the
  original — this applies to every mission with a research objective.
- **The Blight shows up.** It spread but was never drawn, so it seemed not to
  exist or grow. It now looks as in the original: a purple checkerboard over
  infected terrain, with the *Display Computer Overlay* option on. The edges use
  the game's `VIRMASK.RAW` file; without it they come out squarer.
- **Work vehicles show their work animation**, as in the original: the
  Earthworker laying tube or wall, the ConVec building or repairing, the Spider
  and the repair vehicle repairing, and the Robo-Dozer bulldozing. They used to
  spin their driving animation while standing still.
- **A tube connected at a building's dock.** A tube ending at the dock (the
  tile where trucks dock) sometimes connected the building and sometimes did
  not, depending on the order of the buildings — the tube looked finished and
  the building stayed off. It now always matches the original: **the dock does
  not connect**; the tube has to touch a part of the building that is not the
  dock.

**For those following the code:** sections 5.34 to 5.36 of PORT_STATUS; the
`--tubetest`, `--e02test` and `--dumptest` test modes; the `tools/extract_anim_tables.py` generator extracts
the four animation sets of the vehicles.

---

## 2026-11-17

### Português

**Novo**

- **Pontos de passagem com Ctrl + clique**, como no original: depois de
  apertar um comando — mover, ficar parado, atacar, guardar, construir,
  reparar, patrulhar, rota de minério —, cada **Ctrl + clique** no mapa marca
  um ponto, desenhado como um quadrado vermelho numerado, e a unidade passa por
  eles, na ordem, antes de ir ao alvo. Até 7 pontos na maioria dos comandos (6
  na patrulha, 5 na rota de minério). Cheio, o último ponto é trocado.
- **Shift na seleção**, como no original: **Shift + clique** num veículo põe
  ele na seleção, ou tira se já estava; o **botão do meio** do mouse faz o
  mesmo. **Shift + caixa** soma os veículos à seleção — a caixa sem Shift
  continua substituindo, como sempre foi neste porte. Um veículo atordoado por
  EMP não entra na seleção, como no original.
- **O bipe ao fechar o jogo**, como no original: ao sair, a janela some, toca
  um bipe e o jogo espera ele acabar. Com o som desligado, fecha na hora — o
  original esperava 3,5 s à toa.
- **As opções do porte se trocam pelo jogo**, sem editar o INI. O `GAME
  PREFERENCES` do menu principal ficou mais largo e ganhou uma coluna *"Port
  extras"* para tudo o que é acréscimo deste porte: **Auto Ore Route** (a rota
  de minério num clique), **Wrap All Maps** (todo mapa dá a volta), **Show
  Bottom Row** (mostra a última linha do mapa), **Demo Camera** (a câmera
  passeia no demo), **Original Menu Skin** (a pele de diálogo do original —
  agora troca na hora, sem reabrir o jogo) e a barra **Night Darkness** (quanto
  as noites escurecem). Desmarcada, cada caixa é o comportamento do original.
  Dentro da partida, a lista *Game Preferences* da aba Options ganhou **Audio
  Volume**, **Night Darkness**, **Wrap All Maps**, **Show Bottom Row** e
  **Auto Ore Route**, que valem no `&APPLY` sem reiniciar a partida.

**Corrigido**

- **Veículos em grupo não ficam mais andando sem parar.** Mandando muitos
  veículos juntos para o mesmo lugar, alguns ficavam para sempre tentando
  chegar. O veículo empurrado agora anda até a casa ao lado como no original
  (e, se ela estiver ocupada, empurra ou espera a sua vez), e quem não acha
  caminho espera um pouco mais a cada tentativa, em vez de tentar a cada
  instante.
- **A última linha do mapa fica escondida**, como no original: a vista para no
  fundo da área jogável do mapa, então a última linha de tiles nunca aparece.
  `ShowBottomRow=1` na seção `[Game]` do `OUTPOST2.INI` deixa a vista descer
  até ela.
- **O piso da Garagem aparece**, nas duas facções: o piso cinza com as setas
  amarelas na garagem de Plymouth, e o chão escuro com luzes na de Eden — antes
  o terreno aparecia por dentro do prédio.

**Para quem acompanha o código:** seções 5.28 a 5.33 do PORT_STATUS; os modos
de teste `--stacktest` (que veio do PR #2), `--exitbeeptest` e `--waypointtest`; casos novos no `--scrolltest`, no
`--drawordertest`, no `--uitest`, no `--optmenutest` e no `--selecttest`.

### English

**New**

- **Ctrl+click waypoints**, as in the original: after pressing a command —
  move, stand ground, attack, guard, build, repair, patrol, ore route —, each
  **Ctrl+click** on the map marks a point, drawn as a numbered red square, and
  the unit goes through them, in order, before heading to the target. Up to 7
  points for most commands (6 for patrol, 5 for the ore route). When full, the
  last point is replaced.
- **Shift in selection**, as in the original: **Shift+click** on a vehicle
  adds it to the selection, or removes it if it was already there; the
  **middle mouse button** does the same. **Shift+box** adds the vehicles to the
  selection — a box without Shift still replaces it, as it always has in this
  port. A vehicle stunned by EMP is not selected, as in the original.
- **The beep when closing the game**, as in the original: on exit the window
  disappears, a beep plays and the game waits for it to finish. With sound
  off it closes at once — the original waited 3.5 s for nothing.
- **The port's options can be changed in game**, without editing the INI.
  The main menu's `GAME PREFERENCES` is wider and has a new *"Port extras"*
  column for everything this port adds: **Auto Ore Route** (the one-click ore
  route), **Wrap All Maps** (every map wraps around), **Show Bottom Row** (the
  map's last row is shown), **Demo Camera** (the camera tours the demo),
  **Original Menu Skin** (the original dialog skin — it now switches right
  away, without restarting the game) and the **Night Darkness** slider (how
  dark the nights get). Unchecked, each box behaves as the original. During a
  mission, the Options tab's *Game Preferences* list gained **Audio Volume**,
  **Night Darkness**, **Wrap All Maps**, **Show Bottom Row** and **Auto Ore
  Route**, which take effect on `&APPLY` without restarting the mission.

**Fixed**

- **Vehicles in a group no longer keep moving forever.** Sending many vehicles
  to the same place left some of them trying to arrive forever. A pushed
  vehicle now walks to the side tile as in the original (and, if that tile is
  taken, pushes or waits its turn), and a unit that finds no path waits a
  little longer after each try instead of retrying every instant.
- **The map's bottom row is hidden**, as in the original: the view stops at
  the bottom of the map's playable area, so the last tile row is never on
  screen. `ShowBottomRow=1` in the `[Game]` section of `OUTPOST2.INI` lets the
  view scroll down to it.
- **The Garage floor is drawn**, for both factions: the grey pad with yellow
  arrows in the Plymouth garage, and the dark lit floor in the Eden one — the
  terrain used to show through the building.

**For those following the code:** sections 5.28 to 5.33 of PORT_STATUS; the
`--stacktest` (from PR #2), `--exitbeeptest` and `--waypointtest` test modes; new cases in `--scrolltest`,
`--drawordertest`, `--uitest`, `--optmenutest` and `--selecttest`.

---

## 2026-11-10

### Português

**Novo**

- **Rota de minério num clique.** Com caminhões de carga vazios selecionados,
  o **clique direito na doca de uma mina** monta o ciclo inteiro: o caminhão
  carrega, leva até a fundição e volta, sozinho, até receber outra ordem. A
  fundição escolhida é a compatível mais próxima **que esteja operando e com
  espaço**; se nenhuma estiver, fica a mais próxima de todas e o caminhão espera
  nela. Vale para minério comum, minério raro e Magma Well, nas duas raças.
  *Isto é um acréscimo deste porte — não existe no original.* O comando de três
  cliques continua igual, e `AutoOreRoute=0` na seção `[Game]` do
  `OUTPOST2.INI` desliga o atalho.

**Corrigido**

- **Quem vem de frente dá passagem.** Dois veículos que se encontram de frente
  num caminho estreito não ficam mais esperando um pelo outro: o que está entre
  dois passos recebe um desvio para uma casa ao lado, espera ali um instante e
  retoma o caminho, como no original. Antes o porte deixava os dois parados até
  desistirem e procurarem outra rota. De quebra, um defeito do original
  corrigido: se a casa do desvio fica bloqueada, o veículo não faz mais uma
  parada à toa no ponto seguinte do caminho.
- **O veículo parado sai da frente.** Um caminhão — ou qualquer veículo —
  parado na doca, ou no caminho de outro, agora é empurrado para uma casa
  vizinha quando o outro precisa passar, como no original. Antes o porte
  deixava os dois entrarem no mesmo tile. Um veículo ocupado (no meio da própria
  rota) ou um inimigo armado não são empurrados: quem chega espera um pouco e
  procura outro caminho.
- **A carga do caminhão não vaza mais da doca.** Na atracagem, o braço que leva
  o minério para dentro do prédio aparecia descendo por cima do terreno, e os
  funis saíam pelos lados da doca. Agora são cortados na borda do poço, como no
  original — o mesmo vale para o carregamento na mina e para o Magma Well.
- **Veículo atrás de prédio fica atrás do prédio.** O porte desenhava todos os
  prédios e depois todos os veículos, então um veículo passando ao norte de um
  prédio alto aparecia por cima dele. Agora a ordem é a do original: prédios e
  veículos juntos, de cima para baixo no mapa; tiros e explosões por cima; e os
  desastres (raio, tornado, meteoro) por último.
- **Inimigos armados e desarmados no caminho.** A regra de quem pode ser
  empurrado estava invertida para unidades inimigas: o porte deixava o inimigo
  **armado** ser atravessado no planejamento de rotas e bloqueava o
  **desarmado**. Agora é como no original — o armado bloqueia, o desarmado pode
  ser empurrado. Isso também muda, um pouco, como as unidades contornam
  inimigos.

**Para quem acompanha o código:** seções 5.23 a 5.27 do PORT_STATUS; cinco
modos de teste novos — `--autoroutetest`, `--pushtest`, `--artcliptest`,
`--drawordertest` e `--detourtest`.

### English

**New**

- **One-click ore route.** With empty cargo trucks selected, **right-clicking a
  mine's dock** builds the whole cycle: the truck loads, hauls to the smelter
  and comes back on its own until it gets another order. It picks the nearest
  compatible smelter **that is running and has room**; if none is, it takes the
  nearest one anyway and waits there. Works for common ore, rare ore and the
  Magma Well, for both factions. *This is an addition of this port — it is not
  in the original.* The three-click command is unchanged, and `AutoOreRoute=0`
  in the `[Game]` section of `OUTPOST2.INI` turns the shortcut off.

**Fixed**

- **Whoever comes head-on gives way.** Two vehicles meeting head-on in a
  narrow passage no longer just wait for each other: the one between two steps
  gets a detour to a side tile, waits there a moment and carries on, as in the
  original. The port used to leave both standing until they gave up and looked
  for another route. Also, a defect of the original fixed: if the detour tile
  gets blocked, the vehicle no longer makes a pointless stop at the next point
  of its path.
- **A parked vehicle gets out of the way.** A truck — or any vehicle — parked
  on a dock, or in another unit's path, is now pushed to a neighbouring tile
  when the other one needs to pass, as in the original. The port used to let
  both drive onto the same tile. A busy vehicle (mid-route) or an armed enemy is
  not pushed: the one arriving waits a little and finds another way.
- **The truck's load no longer pokes out of the dock.** While docking, the arm
  carrying the ore into the building was drawn sliding down over the terrain,
  and the hoppers stuck out of the sides of the bay. They are now clipped at the
  edge of the pit, as in the original — the same applies to loading at the mine
  and to the Magma Well.
- **A vehicle behind a building stays behind it.** The port drew every building
  and then every vehicle, so a vehicle passing north of a tall building was
  drawn over it. The order is now the original's: buildings and vehicles
  together, top to bottom of the map; shots and explosions on top; and disasters
  (lightning, tornado, meteor) last.
- **Armed and unarmed enemies in the way.** The rule for who can be pushed was
  inverted for enemy units: the port let route planning pass through an
  **armed** enemy and blocked an **unarmed** one. It now matches the original —
  armed blocks, unarmed can be pushed. This also slightly changes how units
  route around enemies.

**For those following the code:** sections 5.23 to 5.27 of PORT_STATUS; five
new test modes — `--autoroutetest`, `--pushtest`, `--artcliptest`,
`--drawordertest` and `--detourtest`.
