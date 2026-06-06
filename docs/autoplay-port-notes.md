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
2. **Compras compulsivas / recorría tiendas sin comprar casi nada.** **RESUELTO (D v1)**:
   la causa de "compra poco" era que SOLO compraba consumibles (cura/comida/ID/recall/luz);
   no tocaba armas ni armaduras. `autoplay_buy_best_gear` (cmd1.c) ahora compra, tras los
   consumibles, la mayor mejora de equipo del stock (`object_value` > la del slot, no maldita,
   asequible dejando reserva `min_gold`) vía `store_bot_price`+`store_bot_buy`; el pase de
   equipar la viste. Cooldown `autoplay_no_resupply_until` (turn+3000) sigue evitando el
   yo-yo. *Pendiente v2*: stock/pathing edge-cases, black market, vender mejor.
2b. **Persecuciones tontas (fruit bats, etc.).** **RESUELTO**: `autoplay_low_value_foe` =
   erráticos (`RF1_RAND_25|50`, imposibles de acorralar) o inofensivos (sin blows y sin
   spells). `autoplay_nearest_enemy` los salta si no son adyacentes; `autoplay_nearest_ranged`
   los salta siempre (no malgastar munición en un murciélago que rebota).
3. **No para inmediatamente al pulsar tecla/clic.** **RESUELTO**: intercept en
   `keypress_event_handler`/`button_press_event_handler` (main-gtk2.c) — cualquier tecla/clic
   para el bot al instante y se descarta; y `TERM_XTRA_DELAY` ahora drena TODOS los eventos
   GTK (`DrainEvents`) alrededor del `usleep` (antes solo `inkey` drenaba 1 evento/iter, y un
   clic quedaba detrás de la avalancha de eventos de movimiento → no se procesaba a tiempo).
4. **Stuck en el primer turno de un nivel nuevo** (no había nada "visto" porque la vista aún
   no estaba aplicada). **RESUELTO**: `update_stuff()` al inicio de `autoplay_decide` + fallback
   `AP_DELVE` (empuja hacia suelo real no visto con `travel_walkable_real`).
5b. **Antorcha agotada → se quedaba parado, incluso con antorchas nuevas en la mochila.**
   **RESUELTO (causa raíz: faltaba el `case AP_LIGHT` en `autoplay_perform`).** Al partir
   decisión/ejecución (Paso B/Oráculo) se quedó fuera el caso de la luz: `autoplay_decide`
   elegía `AP_LIGHT` con prioridad (paso 5, antes de explorar) y detectaba bien las antorchas
   (`autoplay_find_torch` — verificado en vivo con gdb que devolvía el slot correcto), pero
   `autoplay_perform` caía en `default: break;` → no equipaba **ni gastaba turno** →
   `process_player` repetía la misma decisión en su `while (energy>=100)` = **bucle infinito**
   (el "se queda parado"). Fix: `case AP_LIGHT: (void)autoplay_manage_light(); break;`.
   Diagnóstico clave: el **Oráculo/mensaje de stall** decía "Tend your light source", que
   apuntaba a ejecución, no a detección.
   - **Guarda anti-stall** (`autoplay_step`): se pone `energy_use=0` antes de `perform`; si tras
     ejecutar sigue a 0 (acción no-op) y no es `AP_REST` (que delega al subsistema de descanso),
     se **para limpio nombrando la acción** en vez de colgarse. Convierte cualquier futuro no-op
     en diagnóstico en vez de cuelgue.
   - **Red de seguridad de luz**: `autoplay_no_spare_light()` / `autoplay_light_dark()` + **paso
     5b**: sin luz y sin repuesto/aceite → `AP_RECALL` al pueblo (o `AP_WAIT` si recall pendiente)
     ANTES de explorar; y `autoplay_needs_resupply` dispara viaje cuando la luz baja
     (`timeout<500`) sin repuesto, **antes del gold-gate** (antorchas baratas, ir a oscuras es
     letal). El `else` de `autoplay_shop_buy_needs` ya compra antorchas aunque no haya luz puesta.

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
   `blind_goal`) **y en el paseo por el pueblo** (`autoplay_town_step`/`autoplay_shop_neighbor`,
   que era el que faltaba — en Bree seguía atravesando a Maggot). El combate sigue ignorando
   monstruos (para alcanzar al blanco). *Caso límite:* NPC bloqueando pasillo de 1 ancho (sin
   rodeo) → seguiría flojo; ahí tocaría swap/push-past.
8. **Nivel sin salida → "Nothing to do".** **RESUELTO (scumming)**: tras agotar explorar/delve/
   bajar, el bot va a una escalera ARRIBA conocida y sube (el nivel se regenera; `AP_ASCEND`/
   `autoplay_find_upstair`/`autoplay_ascend`); sin escaleras → recall al pueblo (`AP_RECALL` +
   `AP_WAIT` mientras cuenta atrás) y re-baja a un nivel fresco.

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

## Ruta / estrategia (Paso C) — a qué mazmorra ir

- **`lib/scpt/autoplay.lua`**: tabla `autoplay_route` = lista ordenada de
  `{dungeon, plev, depth, name}` (índices de `lib/edit/d_info.txt`). Es **dato puro,
  tweakeable**: reordenar/borrar/re-gatear sin recompilar. Espina dorsal = los 4
  dungeons `PRINCIPAL` (Barrow-Downs→Mirkwood→Mordor→Angband, escalera de
  profundidad 1..127); el resto son raids opcionales de jefe (`FINAL_GUARDIAN`) y
  botín (`FINAL_ARTIFACT`/`FINAL_OBJECT`), intercalados por nivel. `plev` es un gate
  de **supervivencia** (más estricto que el `min_plev` "puede entrar" de d_info):
  suele fijarse cerca de la profundidad del fondo, para que el bot grindee antes de
  un boss profundo. Accesores Lua: `autoplay_route_at(i)` → `dungeon,plev,depth`
  (devuelve `dungeon=-1` pasado el final = centinela; **Lua 4.0 no tiene
  `table.getn`**), `autoplay_route_name(i)` → etiqueta.
- **`cmd1.c`** `autoplay_objective(int *dungeon,int *depth,char *name)`: recorre la
  ruta y coge la **primera** entrada no terminada (`max_dlv[dungeon] < depth`) y para
  la que hay nivel (`p_ptr->lev >= plev`). Si Lua ausente o lista agotada →
  **fallback**: el dungeon `PRINCIPAL` más profundo que puede entrar (`min_plev`) y no
  ha tocado fondo. `depth` = donde lo dejó (`max_dlv`), clamp a `[mindepth,target]`.
  Usa `DF1_PRINCIPAL` (0x1), campos `mindepth/maxdepth/min_plev/flags1`, `max_d_idx`,
  `dungeon_type` (dungeon actual), `d_name + d_info[i].name` (nombre fallback).
- **Integración** en `cmd1.c`:
  - `autoplay_town_step` (recall tras comprar): el destino del recall = `autoplay_objective`,
    no el último dungeon. Mensaje "recalling to X (Ln)".
  - Cola del dead-end de `autoplay_decide` (paso "11b", antes del scum): si el nivel
    está agotado y el objetivo es **otro** dungeon (éste limpio / fuera de plan) →
    `AP_RECALL` al pueblo (o `AP_WAIT` si recall ya pendiente) en vez de hacer scum
    eterno. Si el objetivo sigue siendo este dungeon → cae al scum (regenera para
    hallar bajada).
- **Completitud**: se aproxima por `max_dlv[dungeon] >= depth` (tocó fondo ≈ hecho).
  NO comprueba que el `FINAL_GUARDIAN` esté muerto (mejora futura: `r_info[guard].max_num==0`).
- **OJO porting**: `autoplay.lua` es nuevo (sin conflicto). `call_lua` soporta múltiples
  retornos (`ret="ddd"`, cada arg `s32b*`). En LP64+L64 `s32b==int`. Probar con
  `autoplay_lua_ok()` antes (peta si falta la global).

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
13. `feat(autoplay)`: **ruta/estrategia** (Paso C) — tabla `autoplay_route` en Lua (espina
    principal + raids de quest por nivel); `autoplay_objective` dirige el recall y el salto
    entre mazmorras al limpiar una.
14. `fix(autoplay)`: **fijar escalera objetivo** (no oscilar entre equidistantes) — `autoplay_pick_stair`
    se compromete con una y elige por alcanzabilidad A*, no línea recta.
15. `fix(autoplay)`: **equipar/repostar luz** — faltaba el `case AP_LIGHT` en `autoplay_perform`
    (regresión del split); + guarda anti-stall + red de seguridad de luz (recall si sin repuesto).

## Pendiente
- **(B) Capacidades de supervivencia — HECHO** (`d56be5e`): **detección** (vara/rod/scroll de
  monstruos/trampas/mapeo, 1×/nivel, `autoplay_find_detection`/`AP_DETECT`/`ap_detected`);
  **velocidad** (Potion of Speed si foe `danger*3>=HP` y no fast); **restaurar stats** drenadas
  + **des-maldición** (Remove Curse si lleva puesto algo maldito); **recuperar munición**
  (`cell_has_ammo`, detour a doble radio para shots/arrows/bolts). Plomería `AP_DEVICE`/
  `autoplay_use_device` (force_item + do_cmd_use_staff/zap_rod/read_scroll).
  - *Pendiente B*: rods de detección no-aware (hoy solo aware); resistencias temporales
    (Resist/Resistance) antes de peleas elementales; afinar cuándo usar Speed.
- **(A) Detección de LOOPS de posición — HECHO**: `autoplay_loop_track` (en `autoplay_step`)
  registra las posiciones que CAMBIAN (descansar/pelear/buscar en sitio no cuentan) en un
  ring de `AP_LOOP_WIN`=16; si en toda la ventana sólo hay ≤`AP_LOOP_DISTINCT`=5 celdas
  distintas → loop. **Strike 1**: abandona la persecución actual (`ap_ignore_idx`), bloquea
  (`explore_block_cell`) las celdas del loop y limpia metas/escalera para re-planificar.
  **Strike 2** (vuelve a loopear): `ap_force_unstick` → `decide` hace **recall** al pueblo
  (reset limpio). Los strikes decaen al progresar; todo se resetea por nivel y al arrancar.
  Red genérica sobre la guarda anti-stall (acción sin energía) y el stuck-cell detector.
- **(B) Objetos lanzables / a distancia ("Random bullshit go!") — HECHO (v1)**: paso 1d en
  `autoplay_decide`. Si en mazmorra, sin enemigo adyacente, no ciego y con línea de tiro
  (`projectable`), dispara el lanzador (`INVEN_BOW`+munición del carcaj/mochila) o tira un
  *flask* (no si gasta aceite de linterna) al **más cercano**. Clave: usa
  `autoplay_nearest_ranged` que **SÍ incluye** los `too_dangerous` (floating eyes), así los
  mata desde lejos sin melee. Mecanismo sin prompts: `autoplay_force_item`(+`_on`) alimenta
  `get_item`, y `target_who/row/col` + `get_aim_dir` auto-apuntan en auto-modo. `AP_SHOOT`/
  `AP_THROW` reusan `do_cmd_fire`/`do_cmd_throw` (ellos ponen `energy_use`).
  - **v2 HECHO**: `autoplay_throwable_score` puntúa por daño aproximado y `autoplay_find_throwable`
    coge el mejor — incluye **boulders** (con `SKILL_BOULDER`), **ammo sin lanzador** (shots/
    arrows/bolts que no podemos disparar), y flasks de oil (no si son fuel de linterna).
    Economía: `autoplay_shop_buy_needs` compra flasks de oil para todos (lanzables) y **munición**
    del lanzador (`AP_WANT_AMMO=40`) si hay arco. *Pendiente v3*: recoger ammo tirado del suelo.
- **(E1) Más política a Lua — HECHO**: `autoplay.lua` expone `heal_at`/`heal_big_at` (%HP
  para curar/heal grande), `speed_at` (beber Speed si el foe mata en N turnos), `loot_radius`/
  `item_radius`. Retuneable sin recompilar (vía `autoplay_cfg`, fallback a defaults).
- **(E2) Wilderness a pie — DIFERIDO (mini-proyecto, ROI bajo)**: en `wild_mode` el mapa es
  `wild_map[][]` (no `cave[][]`) → hace falta un **pathfinder de overworld nuevo** + localizar
  la entrada (`wild_map[y][x].entrance` >=1000 dungeon, <1000 town, `known`) + replicar el
  enter (`>` en wild, dungeon.c:3936-3960). El bot **nunca entra en wild_mode solo** (viaja por
  recall, que el Paso C apunta directo), así que hoy se para limpio en wilderness y está bien.
  Quitar ese halt sin navegador sólido = deambular/atascarse. Hacerlo bien = sesión propia.
- **(E3) Gestión de piedad — DIFERIDO (mini-proyecto, ROI bajo)**: ganar piedad = **sacrificar
  en altar** del dios (`do_cmd_sacrifice`, sobre `FEAT_ALTAR`). Necesita altar-seeking (raros)
  + sacrificables (Melkor: **cadáveres**, que el bot descarta a propósito) + blindar los
  `get_check` de auto-sacrificio de HP (con A1 se auto-responden SÍ → se haría daño). ROI bajo:
  bot melee no usa poderes divinos, decay lento (~4/30 mov), varios dioses ganan al matar.
  Piedad negativa rara en juego activo. Si interesa: corpse-carrying + altar-seeking god-specific.
- **Tabla de amenaza real — HECHO**: `autoplay_monster_danger(m)` = daño esperado/turno
  (suma de blows × ~60% acierto + extra por casters/breathers según HP×freq + bump por
  velocidad). `autoplay_too_dangerous` ahora incluye "un solo foe que me mata en
  `danger_turns` turnos (def 2) → evitar melee" además de los casos previos (paralizador,
  unique/out-of-depth).
  - **Cluster / packs — HECHO**: `autoplay_cluster_danger` suma el daño/turno de los foes
    visibles a ≤`cluster_range` (def 8). Paso **1c2** en `decide`: si hay ≥2 foes y
    `cluster_danger * pack_flee_turns (def 4) >= HP` → **escape (Phase Door/Teleport) o huir
    a pie**; si acorralado, cae a ranged/melee. Mata-packs de jackals/arañas resuelto. Knobs
    en `autoplay.lua`: `danger_turns`, `pack_flee_turns`, `cluster_range`.
  - **Afinado (C) — HECHO**: melee escala por el **acierto real vs nuestro AC**
    (`autoplay_blow_hit_pct`, espejo de `check_hit`: `power+lvl*3` vs `3/4·AC`, suelo 5%);
    el daño de **breaths** se reduce por nuestras **resistencias** a lo que respira (cada
    elemento resistido ~ −2/3 de su parte); **breeders** (`RF4_MULTIPLY`) +50% (su amenaza
    compone). Así un bot bien armado pelea más y uno frágil huye más.
  - *Pendiente*: power por-efecto exacto (hoy HURT=60 / resto=15 aproximado); resist de
    hechizos no-elementales.
- Afinar: umbrales de resupply/compra; **gates `plev` de la ruta** (data en `autoplay.lua`,
  con partidas reales).
- Completitud de quest por **muerte del guardián** (`r_info[FINAL_GUARDIAN].max_num==0`) en vez
  de "tocó fondo"; coger el `FINAL_ARTIFACT`/`FINAL_OBJECT` concreto antes de salir.
- Gates `plev` de la ruta = estimaciones; afinar con partidas reales (el bot melee-only es frágil).
