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
- API de tienda (Fase 2b): `store_bot_refresh`, `store_bot_find`, `store_bot_buy`, `store_bot_sell`.

### `src/store.c` — API de tienda no-interactiva (Fase 2b)
Funciones nuevas que reusan los helpers `static` locales (`price_item`, `store_will_buy`,
`store_carry`, `store_check_num`, `store_item_increase/optimize`) y el contexto
`st_ptr`/`ot_ptr`/`cur_store_num`; **deben vivir en store.c**. Insertadas antes de
`do_cmd_store`. Nunca abren la UI:
- `store_bot_set(town,store)` (static) — fija el contexto.
- `store_bot_refresh(town,store)` — `store_maint` catch-up (stock actual).
- `store_bot_find(town,store,tval,sval)` — localiza en el stock (sval<0 = cualquiera).
- `store_bot_buy(town,store,stock_idx,amt)` — compra (precio `price_item(...,min_inflate,FALSE)`,
  acota a oro y hueco de mochila).
- `store_bot_sell(town,store,item)` — vende 1 si `store_will_buy` (precio
  `price_item(...,min_inflate,TRUE)`).
- **OJO**: pisar `FEAT_SHOP` auto-abre la UI (cmd1.c:~3778 → `command_new='_'`). El bot
  transacciona desde una **casilla adyacente**, nunca pisando la entrada.

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
- Tiendas/resupply (Fase 2b): defines `AP_WANT_*` (objetivos de stock), estáticos
  `autoplay_shopping`/`autoplay_shop_visited[]`; `autoplay_inv_count`, `autoplay_is_cure`,
  `autoplay_is_staple`, `autoplay_count_cure/food`, `autoplay_keep_item` (qué NO vender),
  `autoplay_shop_sell_junk`, `autoplay_buy_one`/`autoplay_shop_buy_needs`,
  `autoplay_find_shop`/`autoplay_shop_neighbor` (caminar adyacente, no pisar),
  `autoplay_town_step` (tour de tiendas → recall abajo), `autoplay_needs_resupply` (disparo
  en mazmorra → recall arriba). Venta **agresiva**: vende todo lo no-equipable y no-consumible
  de la lista. Navegación: **camina** a cada tienda.
- Split decisión/ejecución + oráculo: descriptor `autoplay_action` (`AP_*`),
  `autoplay_decide()` (rellena acción + `advice` en INGLÉS, sin ejecutar; probes read-only
  `autoplay_can_reach`/`autoplay_can_flee`), `autoplay_perform()` (ejecuta),
  `autoplay_step()` = decide→perform, y `do_cmd_oracle()` (tecla `^N` + menú; dice qué haría
  sin hacerlo). Helpers read-only para el split: `autoplay_light_needs`,
  `autoplay_find_unknown_id`+`autoplay_do_identify`, `autoplay_find_upgrade`,
  `autoplay_flee_from(ty,tx)` (antes tomaba el monstruo).
- **`do_cmd_autoplay()`** — arranca (cancela explore/travel, `autoplaying=1`).
- Oráculo: `externs.h` `do_cmd_oracle`; `dungeon.c` `case KTRL('N')`; `main-gtk2.c` menú
  "Oracle: advice (^N)". Mensajes del oráculo **en inglés**.

## Issues conocidas / por arreglar (reportadas al probar)
1. **Superficie: se quedaba stuck** con enemigos que entran/salen de vista. **Mitigado**:
   en `dun_level==0` se ignora cualquier foe no adyacente (`autoplay_decide`); solo defiende
   si está pegado.
2. **Compras compulsivas / recorría tiendas sin comprar casi nada.** **Mitigado**: cooldown
   `autoplay_no_resupply_until` (turn+3000) tras cada viaje, y se amplían los svals de compra
   (cura CLW/CSW/CCW, varias comidas). **PENDIENTE investigar la causa raíz** del "no compra":
   ¿oro insuficiente?, ¿el stock de la tienda no tiene el item en ese momento?, ¿no alcanza
   las tiendas por pathing en el pueblo? Verificar `store_bot_find/buy` con logging.
3. **No para inmediatamente al pulsar tecla/clic.** **RESUELTO**: intercept en
   `keypress_event_handler`/`button_press_event_handler` (main-gtk2.c) — cualquier tecla/clic
   para el bot al instante y se descarta; y `TERM_XTRA_DELAY` ahora drena TODOS los eventos
   GTK (`DrainEvents`) alrededor del `usleep` (antes solo `inkey` drenaba 1 evento/iter, y un
   clic quedaba detrás de la avalancha de eventos de movimiento → no se procesaba a tiempo).
4. **Stuck en el primer turno de un nivel nuevo** (no había nada "visto" porque la vista aún
   no estaba aplicada). **RESUELTO**: `update_stuff()` al inicio de `autoplay_decide` + fallback
   `AP_DELVE` (empuja hacia suelo real no visto con `travel_walkable_real`).
5. **[POR ARREGLAR BIEN] Selección de objetivo en oscuridad total.** El núcleo de exploración
   usa **fronteras vistas** (celda vista junto a una no vista); si el `@` **no ve nada**
   (habitación a oscuras / sin fuente de luz), no hay frontera → no elige objetivo → se
   bloquea. Es un **problema lógico del algoritmo de selección de objetivo**. *Workaround*
   actual: equipar antorcha antes del recall (ver abajo) + `AP_DELVE` (terreno real) lo
   mitiga, pero una sala **cerrada con solo puertas secretas** (necesita buscar) o el caso de
   cero luz siguen siendo flojos. Fix propio: que la selección de objetivo no dependa solo de
   "visto" (p.ej. delve siempre como objetivo válido, o buscar puertas secretas).
6. **Bot perseguía a perpetuidad a evasivos / NPCs (fruit bat, Blubbering idiot, Farmer
   Maggot).** **RESUELTO**: give-up por **turnos totales** (`ap_chase_turns > 15` ⇒ se mete en
   `ap_ignore_idx` y explora; el progreso-based reseteaba con la fluctuación de distancia).
   `autoplay_nearest_enemy` también ignora `monfear` (asustados) salvo si están adyacentes.
   **Workaround de equipo:** en `autoplay_town_step`, antes del recall-abajo, se identifica +
   auto-equipa + asegura luz (descender ya pertrechado, no depender del upkeep in-dungeon que
   se gatea con `!enemy`).
7. **NPC amistoso (Farmer Maggot) en la ruta → "stop to avoid hitting" en bucle.** **RESUELTO**:
   A* ahora **rodea** las casillas con monstruo en el movimiento no-combate. Hooks
   `explore_walkable_clear`/`real_walkable_clear` (= hooks normales + casilla con `m_idx` =
   intransitable), usados en explorar/delve/escalera (pathing + `autoplay_can_reach_hook` +
   `blind_goal`). El combate sigue ignorando monstruos (para alcanzar al blanco). *Caso límite:*
   NPC bloqueando pasillo de 1 ancho (sin rodeo) → seguiría flojo; ahí tocaría swap/push-past.

## Puente Lua (Paso B) — política tweakeable

- **`lib/scpt/autoplay.lua`** (NUEVO): `autoplay_config(key)` (knobs: `want_cure/food/id/wor/oil/
  torch`, `min_gold`, `chase_turns`, `resupply_cooldown`) y `autoplay_avoid(m)` (tabla de
  amenaza por nombre: 1=evitar, 0=pelear, ausente=heurística C). Trae `["Farmer Maggot"]=1`.
- **`lib/scpt/init.lua`**: `tome_dofile("autoplay.lua")` al final.
- **`cmd1.c`**: `autoplay_lua_ok()` (sonda única vía `string_exec_lua`, porque `call_lua` peta si
  la función no existe), `autoplay_cfg(key,def)` (lee `autoplay_config`, fallback a def si Lua
  ausente o devuelve <0). `autoplay_too_dangerous` consulta `autoplay_avoid(m)` (`call_lua "(M)"`).
  Los knobs `AP_WANT_*`/chase/min_gold/cooldown leen de `autoplay_cfg`.
- **OJO porting**: los `.lua` viven en `lib/scpt/` (versionado, por rama). `init.lua` puede
  diferir en 2.3.x → vigilar el cherry-pick de esa línea; `autoplay.lua` es fichero nuevo (sin
  conflicto). Editar el `.lua` re-tunea el bot **sin recompilar**.

## Historial de commits (lógicos)

Cada uno cherry-pickeado a main/iso-tiles/2.3.5/2.3.11:
1. `feat(travel)`: click-to-move cruza lo no explorado; autoexplore se desvía a objetos.
2. `fix(explore)`: autoexplore deja de pararse en cada puerta/objeto; rodea puertas imposibles.
3. `feat(autoplay)`: modo autoplay (^V) borg-lite sobre el autoexplore.
4. `feat(autoplay)`: descansar si seguro, amenaza de monstruos, inventario lleno; no recoger cadáveres.
5. `fix(autoplay)`: no bloquearse en el prompt "Pick up X? (y/n)".
6. `feat(autoplay)`: navegación de mundo fase 1 — recall town↔mazmorra.
7. `feat(autoplay)`: identificar desconocidos y auto-equipar lo mejor.
8. `fix(autoplay)`: no recoger esqueletos (TV_SKELETON); `docs`: estas notas.
9. `feat(autoplay)`: Fase 2b — tiendas (vender agresivo / comprar suministros / resupply),
   `store.c` API no-interactiva + flujo de pueblo en cmd1.c.
10. `feat(autoplay)`: split decisión/ejecución + **Oráculo** (^N, consejos en inglés); fixes:
    ignorar foes no adyacentes en superficie, cooldown de resupply, ampliar svals de compra.
11. `fix(autoplay)`: robustez (arranque/vista+delve, WoR-loop+gold-gate, give-up de evasivos/
    Maggot, responsividad tecla+clic con DrainEvents, equip antes del recall).
12. `feat(autoplay)`: **puente Lua** (Paso B) — `autoplay.lua` config+threat table; A* rodea NPCs.

## Pendiente
- **Cerebro Lua**: ruta/estrategia ("a qué mazmorra ir") como datos; bindings tolua. Modelo
  acordado: híbrido (motor C de grindeo+recall + ruta curada en Lua + lectura de quests).
- **Fase 1b**: navegación por wilderness (descubrir mazmorras a pie) — recall cubre casi todo.
- Afinar: amenaza de monstruos (tabla de peligro real), umbrales de resupply/compra,
  selección de mazmorra por nivel del pj.
