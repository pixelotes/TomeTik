# Autoexplore en TomeTik — investigación y plan

> Notas de investigación (2026-05-31) sobre cómo implementar **autoexplore** en TomeTik,
> con un repaso a cómo lo resuelven Brogue, DCSS y los Angband-likes, y cómo encaja con
> la infraestructura que ya existe en este repo.

## TL;DR conceptual

Autoexplore es sorprendentemente simple en su núcleo:

> **Mientras queden celdas no exploradas y nada interesante interrumpa: muévete un paso
> hacia la celda no explorada más cercana. Repite.**

Toda la complejidad está en (a) cómo calculas "la más cercana" de forma eficiente y
(b) las reglas de **cuándo parar**. El movimiento paso a paso ya está resuelto en el
sistema de `running` clásico de Angband/ToME.

---

## Cómo lo hacen los juegos de referencia

Hay **dos familias** de implementación, y la diferencia es puramente del algoritmo de
pathfinding subyacente.

### 1. Brogue → "Dijkstra maps" (BFS multi-fuente)

Es la técnica canónica, inventada precisamente para Brogue
([RogueBasin: The Incredible Power of Dijkstra Maps](https://www.roguebasin.com/index.php/The_Incredible_Power_of_Dijkstra_Maps)).
La idea:

- Creas una matriz de costes del tamaño del mapa.
- Pones a **0** todas las celdas-objetivo (en autoexplore: **todas las celdas no
  exploradas**), y a "infinito" el resto.
- Haces un **flood-fill / relajación**: cada celda toma `min(vecinos)+1` hasta que el
  mapa se estabiliza. Resultado: cada celda contiene la distancia al objetivo más cercano.
- El jugador simplemente **"rueda cuesta abajo"** — mira sus 8 vecinos y se mueve al de
  menor valor. Un paso por turno.

Lo elegante: con **un solo cálculo** sabes la ruta óptima desde *cualquier* punto, y al
ser multi-fuente (todas las celdas no exploradas a la vez) te da gratis "la más cercana"
sin tener que elegirla explícitamente. El mismo mapa sirve para IA de monstruos,
pathfind-to-cursor, generación de mazmorras, etc.

### 2. DCSS / Angband → "flow" + run

Dungeon Crawl ([CrawlWiki: Autoexplore](http://crawl.chaosforge.org/Autoexplore))
adaptó su código `travel` de NetHack, y autoexplore es literalmente *"un pequeño riff
sobre travel"*. La descripción de un dev lo resume perfecto:

> *"Auto-explore simplemente se mueve a la celda no explorada más cercana. Si haces una
> función que encuentre eso y se lo pases a la función run, ya tienes autoexplore."*

Angband usa lo mismo conceptualmente con su sistema de **"flow"** (`cave-map.c`): cada
celda guarda un valor de "ruido" = distancia BFS desde un origen, y el comando
`do_cmd_pathfind(target)` calcula ese flow y luego va rodando cuesta abajo hacia el
target. Es el mismo Dijkstra map, solo que con un único objetivo en vez de multi-fuente.

**Conclusión:** Brogue y Angband hacen *exactamente lo mismo* (BFS/Dijkstra map + roll
downhill). La única diferencia con autoexplore es: ¿el objetivo es **una** celda (travel)
o **todas las no exploradas** (explore)?

### El detalle que todos comparten: cuándo PARAR

Esto es lo que realmente hace que autoexplore se sienta bien (y donde está el 80% del
trabajo de pulido). DCSS para cuando:

- Aparece un monstruo en vista.
- Hay un objeto en el suelo (lo recoge o se detiene).
- Encuentra escaleras / features inusuales (altares, tiendas, puertas).
- No quedan celdas alcanzables → "Done exploring." / "Partly explored, can't reach the rest."

Limitación conocida de DCSS, útil saberla: el autoexplore "ingenuo a la celda más
cercana" deja esquinas de salas y finales de pasillo sin ver hasta el final, obligando a
una segunda vuelta. No es bloqueante, pero es la queja #1.

---

## Cómo encaja en TomeTik (lo bueno: ya tienes casi todo)

El commit `ba5da09` (A*) y la infraestructura clásica de ToME aportan las tres piezas:

| Pieza necesaria | Qué tienes ya | Dónde |
|---|---|---|
| **Pathfinding** | A* con callback de walkability | `pathfind.h:71`, `astar_find_path_cb()` |
| **Saber qué está explorado** | flag `CAVE_MARK` por celda | `defines.h:2119` — `!(cave[y][x].info & CAVE_MARK)` = no explorado |
| **Auto-mover paso a paso + parar** | sistema de `running` + `disturb()` | `run_step()` `cmd1.c:4618`, `disturb()` `cave.c:5157` |
| **Ejecutar un paso** | `move_player_aux()` | `cmd1.c:3280` |
| **Bucle de turno** | `process_player()` revisa `running` | `dungeon.c:4697` |

**Lo único que no existe:** ninguna feature de travel/goto. Campo virgen. El A* recién
añadido **aún no tiene ningún caller** en el código.

### Decisión de arquitectura que ahorra dolor

El A* actual encuentra ruta **a un objetivo concreto `(gy, gx)`**. Para autoexplore hay
dos caminos:

**Opción A — Reusar el A* (mínimo código nuevo):**

1. Escanea el mapa, encuentra todas las celdas no exploradas adyacentes a celdas
   conocidas y caminables (la "frontera").
2. Elige la frontera más cercana (heurística rápida: distancia recta, o un A* a la más
   cercana por distancia).
3. `astar_find_path_cb()` hasta ella → guardas el `path_result`.
4. Avanza un waypoint por turno (en el espíritu de `run_step`).
5. Al llegar (o si se revela algo nuevo / interrumpe), recalcula.

Sencillo, pero "elegir la frontera más cercana" con A* repetidos puede ser O(N) llamadas.
Para un mapa de ToME es perfectamente aceptable.

**Opción B — Dijkstra map multi-fuente (estilo Brogue, "lo correcto"):**

- Un solo BFS desde *todas* las celdas-frontera a la vez te da el campo de distancias
  completo. El jugador rueda cuesta abajo. No eliges objetivo, emerge solo, y es
  robustísimo. Son ~40 líneas de C con una cola.
- Bonus: ese mismo `cost`-map ya **existe parcialmente** en `cave_type` (campo
  `byte cost` para MONSTER_FLOW, ver `types.h:707`) — Angband reusa justo eso.

**Recomendación:** empieza con **A** (reusas el A* recién hecho, ves resultados en una
tarde), y si el "deja esquinas sin explorar" molesta, migra el selector de objetivo a un
Dijkstra map (**B**). El loop de movimiento y parada es idéntico en ambos.

### Esqueleto de integración

```
do_cmd_explore()                    // nuevo, bindeado a una tecla (p.ej. 'o' como DCSS)
  └─ explore_step()                 // análogo a run_step()
       ├─ si no hay path: find_nearest_unexplored() → A* o Dijkstra map
       ├─ if (!path) → "Done exploring." ; return
       ├─ chequear interrupciones (monstruo visible, objeto, feature) → disturb()
       └─ move_player_aux(dir_al_siguiente_waypoint, ...)
```

En `process_player()` (`dungeon.c:4697`), justo donde ya hace `if (running) run_step(0)`,
añades `else if (exploring) explore_step()`. El sistema de energía y el bucle ya están
montados.

---

## Los puntos peliagudos (donde se va el tiempo de verdad)

1. **¿Qué cuenta como "explorable"?** No toda celda sin `CAVE_MARK` es alcanzable (otro
   lado de una pared). La clave es buscar **fronteras**: celda no marcada que sea
   *adyacente a una celda caminable y conocida*. Si no, el A* falla y hay que detectarlo.
2. **Walkability ≠ "es suelo".** Puertas cerradas, trampas conocidas, lava, agua
   profunda... Define bien el hook de caminabilidad (probablemente quieras *abrir puertas*
   automáticamente como DCSS, o pararte ante ellas).
3. **Recálculo:** cada paso revela celdas nuevas → la ruta puede quedar obsoleta. Lo
   barato es recalcular cada N pasos o cuando se memoriza una celda nueva relevante. Un
   Dijkstra map se recalcula entero y barato; con A* recalculas solo al llegar al objetivo
   o al perderlo.
4. **Condiciones de parada** (lo más importante para el "feel"): reusa la lógica de
   `run_test()` (`cmd1.c:4240`) que ya detecta monstruos/objetos/obstáculos — autoexplore
   es básicamente `run` con un selector de dirección distinto.
5. **Loops infinitos:** el bug clásico de DCSS
   ([tracker #7523](https://crawl.develz.org/mantis/print_bug_page.php?bug_id=7523)). Si
   una celda es "explorable" pero inalcanzable, puedes oscilar para siempre. Necesitas un
   set de "intentadas e inalcanzables" o un límite de pasos.

---

## Por dónde empezar (concreto)

1. **Lee `run_step` / `run_test` / `run_init`** en `cmd1.c:4146-4667` de cabo a rabo.
   Autoexplore es un primo de esto; entender el modelo de "estado persistente + un paso
   por turno + interrupción" es el 70% del diseño.
2. **Escribe `find_nearest_unexplored()`** usando `astar_find_path_cb` — es el primer
   hito tangible y prueba que el A* funciona contra `cave[][]` real (que aún no tiene
   ningún caller).
3. **Conéctalo al bucle** en `process_player()` y bindea una tecla.

---

## Referencias

- [RogueBasin — The Incredible Power of Dijkstra Maps](https://www.roguebasin.com/index.php/The_Incredible_Power_of_Dijkstra_Maps)
- [CrawlWiki — Autoexplore](http://crawl.chaosforge.org/Autoexplore)
- [DCSS tracker #7523 — Endless autoexplore loop](https://crawl.develz.org/mantis/print_bug_page.php?bug_id=7523)
- [Angband Manual — How It Works](https://angband.readthedocs.io/en/latest/hacking/how-it-works.html)
- [angband/src/cmd-cave.c (`do_cmd_pathfind`)](https://github.com/angband/angband/blob/master/src/cmd-cave.c)
