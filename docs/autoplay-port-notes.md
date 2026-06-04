# Autoplay / autoexplore — notas de portado entre ramas

> Catálogo de **qué fichero y qué función se toca** por cada feature del autoexplore /
> click-to-move / autoplay. Sirve para **replicar los cambios en ramas con base distinta**
> (2.2.2 / 2.3.5 / 2.3.11 / …) cuando un cherry-pick no aplique limpio.
>
> Mantener este doc **al día en cada fase**. Doc de diseño/conceptual aparte: [theborg.md](theborg.md).

## Ramas donde vive

Se desarrolla en **`2.2.2`** y se propaga por `git cherry-pick` a **`main`, `iso-tiles`,
`2.3.5`, `2.3.11`** (NO a `release/v2.2.2`, congelada).

- `cmd1.c` diverge ~271 líneas entre 2.2.2 y 2.3.x, **pero todo el código de
  autoexplore/autoplay es propio de TomeTik** (no vanilla), así que el *3-way merge* del
  cherry-pick cae limpio. Aun así, **tras cherry-pickear a 2.3.x conviene compilar 2.3.11**
  (`./docker/build.sh gtk2`) para confirmar el merge.
- Artefactos que NO se commitean: `src/tome` (binario), `src/w_*.c` (genera tolua),
  `docker/build.log`, `lib/note/PLAYER.nte`, `lib/save/*`. Antes de cambiar de rama,
  `git stash` de lo modificado-no-mío y `git stash pop` al volver.

## Inventario de cambios por fichero (checklist de replicación)

### `src/variable.c` — globales de estado
- `s16b exploring;`  — autoexplore activo (preexistente del autoexplore).
- `s16b autoplaying;` — autoplay activo. **Añadido.**
- (`travelling`, `click_dir` ya existían del travel/click.)

### `src/externs.h` — declaraciones
- `extern s16b autoplaying;`
- `extern void do_cmd_autoplay(void);`
- `extern void autoplay_step(void);`
- `extern void eat_food(int item);`             (extraída de do_cmd_eat_food)
- `extern bool quaff_potion(int tval, int sval, int pval, int pval2);` (pasó a no-static)

### `src/cmd6.c` — primitivas de consumo sin prompt
- `quaff_potion(...)`: **quitar `static`** (para que el bot beba por índice).
- `do_cmd_eat_food()`: **extraer el cuerpo** (todo lo posterior a `get_item`) a una
  función nueva **`void eat_food(int item)`**; `do_cmd_eat_food` queda como wrapper
  (hace el `get_item` y llama a `eat_food(item)`).

### `src/cave.c` — `disturb()`
- **Eliminar el bloque** que hacía `exploring = 0` (el "Cancel auto-explore"). disturb()
  ya no detiene autoexplore/autoplay; solo corta la pierna de viaje (`travel_cancel`).
  Motivo: `disturb()` salta por eventos benignos (abrir puerta, pisar objeto vía
  `py_pickup_floor`) y paraba el bot a cada paso.

### `src/dungeon.c` — bucle/teclas
- `process_command`: añadir `case KTRL('V'): do_cmd_autoplay(); break;` (junto a
  `KTRL('E')` del autoexplore).
- `process_player` (bucle de energía): tras la rama `else if (exploring) explore_step();`
  añadir:
  ```c
  else if (autoplaying) {
      autoplay_step();
      if (autoplaying) { handle_stuff(); Term_fresh(); Term_xtra(TERM_XTRA_DELAY, 150); }
  }
  ```
- Abort por tecla (sección "Handle abort"): incluir `autoplaying` en la condición
  (`if (running || travelling || exploring || autoplaying || command_rep || ...)`), y en el
  handler de `inkey()` cancelar explícitamente: `if (exploring || autoplaying) { exploring=0;
  autoplaying=0; p_ptr->redraw |= PR_STATE; }` (porque disturb ya no lo hace).

### `src/util.c` — `msg_flush()`
- Envolver el bloque "-more-" (pintar + esperar tecla) en
  `if (!(exploring || autoplaying)) { ... }`, dejando el `Term_erase` fuera. Así el bot no
  se bloquea en `-MORE-`.

### `src/object1.c` — `py_pickup_floor()`
- Tras el bloque de oro, saltar basura del bot:
  `if (autoplaying && ((o_ptr->tval == TV_CORPSE) || (o_ptr->tval == TV_SKELETON))) continue;`
  (no recoger cadáveres/esqueletos al pasar).
- Rama de **un objeto** (carry_query / objeto pesado): antes del `get_check("Pick up X?")`,
  `else if (exploring || autoplaying) do_pickup = TRUE;` (auto-modo no pregunta).
- Rama de **varios objetos** (`do_ask`): antes del `get_item`, `if (do_ask && (exploring ||
  autoplaying)) { this_o_idx = floor_o_idx; do_ask = FALSE; }`.

### `src/main-gtk2.c` — menú
- En `main_menu_items[]`, tras la entrada de Auto-explore:
  `{ "/Action/Movement/Autoplay (^V)", NULL, action_event_handler, KTRL('V'), NULL },`

### `src/cmd1.c` — el grueso (todo propio de TomeTik)

**Click-to-move sobre terreno real** (el A* del click ya no se para ante lo no visto):
- `travel_walkable_real(y,x)` — walkable por `cave[][].feat` real, sin exigir `seen`.
- Puntero `travel_hook` (política de la pierna activa); `travel_plan(gy,gx,hook)` recibe el
  hook; `travel_step` revalida con `travel_hook`; `travel_to` usa `travel_walkable_real` y
  "encaja" a suelo adyacente si clicas roca; `explore_step`/autoplay usan
  `explore_walkable_hook`.

**Autoexplore robusto** (no se para en puertas/objetos):
- `explore_block` (set por nivel, junto a `explore_bad`) + `explore_walkable_hook`
  (seen-gated + respeta `explore_block`) + `explore_block_cell()`.
- `travel_step`: durante explore, fallo de puerta/terreno → `explore_block_cell`+`travel_clear`
  (no aborta); monstruo en el camino sí para.
- `explore_cell_interest()` (clases EXPLORE_NONE/FRONTIER/LOOT) + `cell_has_item`/`cell_has_gold`;
  `cell_has_item` ignora `TV_GOLD`, `TV_CORPSE`, `TV_SKELETON`. `explore_item_radius`,
  `explore_no_items` (bot con mochila llena).
- `find_nearest_goal()` usa `explore_cell_interest`; `explore_pick_goal()` /
  `explore_arm_next_leg()` (extraídos de `explore_step`, compartidos con autoplay; conservan
  la anti-oscilación de fronteras).

**Autoplay** (sección "Auto-play"), todas `static` salvo las 2 públicas:
- Combate/supervivencia: `autoplay_nearest_enemy`, `autoplay_find_heal`, `autoplay_quaff`,
  `autoplay_find_food`, `autoplay_step_towards(gy,gx,do_pickup)`, `autoplay_flee_from`,
  `autoplay_too_dangerous` (lee `r_info[].level`/`RF1_UNIQUE`).
- Luz: `autoplay_find_torch`, `autoplay_find_oil`, `autoplay_equip_lite`, `autoplay_manage_light`.
- Inventario lleno: `autoplay_pack_full`.
- Descenso: `autoplay_find_downstair`, `autoplay_descend` (usa `do_cmd_go_down` con
  `confirm_stairs` desactivado temporalmente).
- Mundo/recall (Fase 1): `autoplay_in_town`, `autoplay_find_recall_scroll`,
  `autoplay_set_recall_target(dungeon,depth)` (fija `p_ptr->recall_dungeon` + `max_dlv[]`),
  `autoplay_start_recall` (lee WoR o invoca recall directo).
- Inventario (Fase 2a): `autoplay_find_identify_scroll`, `autoplay_id_worthy`,
  `autoplay_identify_step` (object_aware+object_known+IDENT_MENTAL, consume Scroll of
  Identify), `autoplay_slot_autoequippable`, `autoplay_wield`, `autoplay_autoequip_step`
  (compara por `object_value`, solo conocido/sensado y no maldito).
- **`autoplay_step()`** — escalera de prioridades (1 acción/turno):
  curar → combate/huida → (en pueblo) recall a mazmorra → comer → luz → identificar →
  auto-equipar → descansar → explorar → bajar escaleras.
- **`do_cmd_autoplay()`** — arranca (cancela explore/travel, `autoplaying=1`).

## Historial de commits (lógicos)

Cada uno cherry-pickeado a main/iso-tiles/2.3.5/2.3.11:
1. `feat(travel)`: click-to-move cruza lo no explorado; autoexplore se desvía a objetos.
2. `fix(explore)`: autoexplore deja de pararse en cada puerta/objeto; rodea puertas imposibles.
3. `feat(autoplay)`: modo autoplay (^V) borg-lite sobre el autoexplore.
4. `feat(autoplay)`: descansar si seguro, amenaza de monstruos, inventario lleno; no recoger cadáveres.
5. `fix(autoplay)`: no bloquearse en el prompt "Pick up X? (y/n)".
6. `feat(autoplay)`: navegación de mundo fase 1 — recall town↔mazmorra.
7. `feat(autoplay)`: identificar desconocidos y auto-equipar lo mejor.
8. (en curso) no recoger esqueletos; y Fase 2b: comprar/vender/resupply (tocará `store.c`).

## Pendiente
- **Fase 2b**: tiendas (comprar/vender/resupply). Tocará `store.c` (varias funciones son
  `static`: `price_item`, `store_carry`, `store_will_buy`, `store_item_increase/optimize` —
  habrá que exponerlas o añadir una API no-static), + contexto `st_ptr`/`ot_ptr`.
- **Cerebro Lua**: ruta/estrategia ("a qué mazmorra ir") como datos; bindings tolua.
- **Fase 1b**: navegación por wilderness (descubrir mazmorras a pie) — recall cubre casi todo.
