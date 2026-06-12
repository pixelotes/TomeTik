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

### `src/skills.c` — gasto automático de skill points (level-up)
Función pública nueva **`autoplay_spend_skills()`** (tras `do_cmd_skill`), equivalente
*headless* del editor de skills. El bot gana 5 puntos/nivel (`xtra2.c`) y sin gastarlos
se queda permanentemente débil. Detalles:
- **Replica el modelo de commit de `do_cmd_skill`**: la verdad vive en `invest[]`;
  `recalc_skills_theory(invest,base_val,base_mod,bonus)` materializa `value` (incl.
  propagación padre↔hijo); `recalc_skills(FALSE)` cierra (HP/maná/hechizos). **No** llama a
  `increase_skill()` directamente: esa función lanza un `msg_box` modal al chocar el cap
  (cuelgue headless). En su lugar pre-chequea el cap con la **misma** condición
  (`(value+mod)/SKILL_STEP >= lev+overage+1`, `overage` de `get_module_info`) y solo entonces
  `skill_points--; invest[sk]++`.
- **Arquetipo auto-detectado** por el mejor `s_info[i].mod` de la clase (melee/archer/caster/
  priest); **melee por defecto** en empates. Listas de prioridad **ponderadas** (helpers
  `ap_*`): cada punto va a la skill más atrasada respecto a su peso (`invest*1000/weight`) →
  reparto proporcional ~40/30/20, y el cap nivel+`overage` derrama el sobrante hacia abajo.
  Slots dinámicos: maestría del arma equipada (`get_weaponmastery_skill`, fallback al mejor mod)
  y del arco (`get_archery_skill`); top-2 escuelas para caster. **Veto** a `SKILL_ANTIMAGIC`
  (rompe sus propios objetos/teleport) y `SKILL_SORCERY` (penaliza HP/melee): nunca en las listas.
  `SKILL_DEVICE` (Magic-Device=56) universal de supervivencia en todos los builds.
- OJO índices: `SKILL_DEVICE` es **56** (no 38). No existe `SKILL_SAVE` (39=`SKILL_ALCHEMY`).

### `src/externs.h`
- `extern void autoplay_spend_skills(void);` (junto a `do_cmd_skill`).
- `extern int get_weaponmastery_skill(void);` / `extern int get_archery_skill(void);` (de
  `xtra1.c`, antes sin prototipo — usadas dentro de su .c por declaración implícita K&R).
- `extern int autoplay_cfg(cptr key, int def);` (ahora público, ver abajo).

### `src/cmd1.c` — enganche + cfg público
- `autoplay_cfg` deja de ser `static` (lo lee `autoplay_spend_skills` para el knob `skill_build`).
- En `autoplay_step()`, antes de `autoplay_decide`: `if (p_ptr->skill_points > 0)
  autoplay_spend_skills();` — no consume turno; captura level-ups en vivo y puntos de birth/pociones.

### `lib/scpt/autoplay.lua`
- Knob nuevo `skill_build` (0=off / 1=auto / 2=melee / 3=archer / 4=caster / 5=priest).
  Default 1. Retuneable sin recompilar (vía `autoplay_cfg`).

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

5. **Selección de objetivo en oscuridad total.** El núcleo de exploración usa **fronteras
   vistas** (celda vista junto a una no vista); a oscuras no hay frontera. **Mitigado de fondo
   por `autoplay_blind_goal`/`AP_DELVE`** (BFS sobre terreno **real** `cave[][].feat`, ground
   truth): la sala a oscuras se delvea un paso → la luz revela → vuelve la frontera. Cero luz =
   problema de **supervivencia** (red de seguridad de luz → recall), no de navegación (el delve
   lee terreno real). **Residual = puertas secretas**, ahora atacado en **L1 (HECHO)**:
   - **L1 — búsqueda dirigida y persistente** (cmd1.c). El barrido 11c original usaba
     `feat >= FEAT_SECRET`, que (0x30) incluye **todos** los muros/vetas → buscaba 1 vez en
     cada borde del perímetro (inútil, y gastaba hasta `AP_SEARCH_MAX`=60 turnos antes de
     scummear). Ahora `autoplay_find_search_spot` apunta a la **puerta secreta real**
     (`cave[ny][nx].feat == FEAT_SECRET` exacto, ground truth — coherente con que el bot ya
     navega por terreno real) e **insiste** en el mismo spot hasta revelarla: `ap_searched`
     pasó de bool a **contador por celda** con presupuesto `AP_SEARCH_PER_SPOT`=20 (`search()`
     es probabilístico por turno); al revelarse, `FEAT_SECRET` (0x30) → puerta (0x20) sale del
     filtro y la exploración normal sigue. Si no hay puerta secreta alcanzable → 0 spots → al
     scum directo (sin barrido de 60 turnos). `case AP_SEARCH`: `ap_searched[...]++`.
   - **L2 — detección legítima en el dead-end (HECHO).** Reutiliza la navegación de L1: cuando
     el bot está plantado en el spot adyacente a la puerta secreta, si lleva un item
     revela-puertas usa `AP_DEVICE` en vez del `search()` probabilístico; si no, cae a
     `AP_SEARCH` (L1). `autoplay_find_reveal_doors(&kind)` (cmd1.c, tras
     `autoplay_find_detection`) busca **Scroll Detect Doors & Stairs** (`SV_SCROLL_DETECT_DOOR`,
     cmd6.c:3376→`detect_doors`), **Rod Detect Door** (aware+cargado) o **Staff Reveal Ways**.
     `detect_doors()` (spells2.c:2050) **convierte `FEAT_SECRET`→puerta** en `DEFAULT_RADIUS`;
     desde adyacente siempre cae en radio → revela al instante y la exploración normal la cruza.
     **No** toca `ap_detected` (ese flag es de la detección proactiva 1×/nivel de monstruos/
     trampas). Compra en pueblo: `AP_WANT_REVEAL=3` / knob `want_reveal` (tras los Identify), y
     `autoplay_keep_item` protege el `SV_SCROLL_DETECT_DOOR` de la venta agresiva (si no, yo-yo
     compra→vende). *Nota*: `SV_ROD_MAPPING` solo hace `map_area` (NO revela secretas), por eso
     no entra en el finder.
   - **L3 — selección de objetivo unificada (HECHO).** `autoplay_explore_target(gy,gx,kind,hook)`
     (cmd1.c, reemplaza a `autoplay_blind_goal`): **un solo BFS sobre terreno real** (ground
     truth) que devuelve la celda de interés más cercana en orden de distancia — **loot visto**
     (LOOT; `cell_has_*` ya exigen `o_ptr->marked`, sin trampa extra) o **suelo no visto**
     (DELVE). Funde la vieja cascada frontera-vista (`explore_pick_goal`) + delve-a-oscuras
     (`autoplay_blind_goal`) + loot en una pasada: sin costura entre frontera iluminada y la
     oscuridad de detrás, y la oscuridad es solo "suelo no visto = objetivo". El paso 10 de
     `autoplay_decide` lo llama con `real_walkable_clear` → `AP_DELVE` (el step real-terreno
     sirve para loot vista y suelo oscuro); si no hay meta, re-flood con **`real_walkable_friend`**
     (nuevo hook = terreno real + bloquea hostiles, permite amistosos) → `AP_PUSHPAST` (que pasó
     a usar ese hook, porque la meta puede ser una celda no vista tras el NPC). La búsqueda de
     puertas secretas (L1/L2) sigue como **fallback** después (no se mete en este BFS porque
     `FEAT_SECRET` no es transitable). **El autoexplore Ctrl-E NO se toca**: conserva su
     `explore_pick_goal` honesto (solo frontera vista). `explore_walkable_friend` queda sin uso
     (lo sustituye `real_walkable_friend`); inofensivo con `-w`.
   - *Pendiente L4*: puertas con cerrojo / tunelar (`AP_OPEN`/`AP_DISARM`/`AP_TUNNEL`) cuando un
     paso esté bloqueado por algo forzable (hoy: block+reroute+scum). Bajo ROI.
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
- **Completitud**: `autoplay_dungeon_done(dn,tgt)` — con `FINAL_GUARDIAN` la mazmorra
  está hecha solo con el **guardián muerto** (`r_info[g].max_num==0`); sin guardián
  (la espina PRINCIPAL), por profundidad (`max_dlv >= tgt`). `autoplay_guardian_ok(g)`
  salta de la ruta un boss fuera de liga (nivel > plev+5, o paralizador sin Free Action).
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
- **(BOSS) Cazar al FINAL_GUARDIAN y recoger su botín — HECHO**. El objetivo del bot ya no
  es "tocar fondo" sino **completar la mazmorra**: matar al guardián y embolsarse el
  `FINAL_ARTIFACT`/`FINAL_OBJECT`. Sin spoilers externos: `d_info.txt` ES el spoiler
  (`FINAL_GUARDIAN_x`/`FINAL_OBJECT_x`), y `generate.c` coloca al guardián en el nivel
  `maxdepth` **con el botín en su inventario** (cae al suelo al morir). Piezas (cmd1.c):
  - Helpers: `autoplay_boss_level()` (¿fondo de mazmorra con guardián vivo?),
    `autoplay_is_objective_guardian(m)`, `autoplay_find_guardian()` (escaneo de `m_list`,
    map-cheat coherente con el explorador L3), `autoplay_find_boss_drop()` (escaneo de
    `o_list` por `name1==final_artifact` / `k_idx==final_object` en el suelo).
  - **Paso 10b (HUNT)** en `autoplay_decide`, tras agotar explore y antes de las escaleras:
    en el boss level con todo explorado → `AP_HUNT` hacia el guardián (re-target cada
    turno, hook `travel_walkable_real`: bump = ataque; probe `autoplay_can_reach_hook`
    antes — si inalcanzable, cae al sweep de secretas / scum, que regenera nivel y boss).
    Boss muerto → "Fetch the guardian's drop": `AP_DELVE` al botín **sin radio de detour**
    (si no, el recall de "Cleared" lo abandonaría); se salta con mochila llena.
  - **Exenciones de combate**: `autoplay_too_dangerous` devuelve FALSE para el
    guardián-objetivo (la ruta ya lo calibró con `guardian_ok`; Lua `autoplay_avoid`
    conserva la última palabra); el give-up de `chase_turns` (15) no aplica al boss
    (una pelea de boss dura más; el loop-tracker sigue cubriendo el atasco real).
  - **Retirada con memoria**: si el loop-tracker fuerza el bail (`ap_force_unstick` →
    recall) en un boss level, se apunta `ap_boss_postpone_dn/lev` (= plev +
    `boss_retry_levels`, knob Lua, def 5) y `autoplay_objective` salta esa mazmorra
    hasta ser más fuerte — sin esto la ruta re-elegía el mismo boss en bucle. Un solo
    slot (basta para romper el re-pick inmediato); se resetea al arrancar autoplay.
  - **Fuera de alcance**: el endgame real (Anillo/Sauron/Morgoth) va por quests/plots
    (`q_*.c`), no por `FINAL_GUARDIAN` — fase aparte.
- **(F1) Ritmo de buceo — HECHO.** El bot moría a plev 5-7 por bucear más rápido de lo
  que subía: nada regulaba el descenso. Ahora en `autoplay_decide` el bloque de bajar
  escaleras se gatea con `dive_ok = (plev >= dun_level+1 + dive_margin)` (knob Lua, def 3).
  Si no está listo, NO baja: cae al scummer de dead-end, que regenera el nivel ACTUAL
  (sube y baja escalera) = grind con monstruos frescos a esta profundidad hasta cerrar
  el gap. Mensajes del Oráculo distintos ("Grinding this depth (not deep-ready yet)").
  Descender a tope de HP ya lo garantizaba el orden (paso 9 AP_REST va antes).
- **(F2) Cautela táctica — HECHO.** Tres piezas en cmd1.c:
  - **Kiting** (paso 2, antes del melee): foe ADYACENTE, estrictamente más lento
    (`mspeed < pspeed`), sin spells (`freq_spell|freq_inate == 0`) y con ranged
    disponible (`autoplay_has_ranged_attack`: lanzador+munición o arrojable) →
    `AP_FLEE` un paso; el paso 1d dispara al turno siguiente desde el hueco. Un foe
    más lento no recupera la distancia: golpes cero, tiros gratis. Knob `kite` (0/1).
  - **Chokepoints** (1c2, tier nuevo): pack que haría daño pero aún no letal
    (`cd * pack_choke_turns >= HP`, def 8) y estamos en suelo abierto → `AP_FALLBACK`
    al pasillo/puerta más cercano (`autoplay_find_chokepoint`: BFS radio 8 sobre suelo
    sin monstruos, celda con ≤2 vecinos transitables y sin contacto hostil) y pelear
    ahí de uno en uno. En el tier letal, **nunca huir DESDE un chokepoint** (huir al
    abierto es lo que te rodea).
  - **Escalera = escape** (1c2, tier letal): escalera bajo los pies → tomarla
    (AP_ASCEND/AP_DESCEND, salida garantizada, el nivel se regenera); si no, scroll de
    escape; si no, dash a escalera conocida a ≤`stair_dash` (def 8) si nada adyacente;
    si no, flee a pie. Orden nuevo del ladder de pánico.
  - Helpers nuevos: `autoplay_has_ranged_attack`, `autoplay_passable_neighbors`,
    `autoplay_on_chokepoint`, `autoplay_cell_in_contact`, `autoplay_find_chokepoint`;
    acción `AP_FALLBACK` (step con `explore_walkable_clear`). Knobs Lua nuevos:
    `dive_margin`, `pack_choke_turns`, `stair_dash`, `kite`, `boss_retry_levels`.
- **(F3) Packs preventivos + huida por velocidad — HECHO.** Tres piezas:
  - `autoplay_cluster_danger` toma ahora el **radio como parámetro**: el tier letal
    escanea corto (`cluster_range`, 8) y el preventivo ancho (`pack_sight`, 12) —
    el fallback a chokepoint salta al AVISTAR el pack, no cuando ya muerde.
  - **Huida consciente de velocidad**: huir a pie solo de lo que podemos dejar atrás
    (`mspeed <= pspeed`); de un pack más rápido → mantener/buscar chokepoint
    ("too fast to outrun -- hold a choke point"); en combate singular desesperado,
    el flee a pie también se gatea por velocidad (el escape scroll va después y
    el kiting de F2 ya exigía foe estrictamente más lento).
  - **Phase Door en la economía**: `autoplay_shop_buy_needs` compra
    `SV_SCROLL_PHASE_DOOR` hasta `want_phase` (knob, def 5) y `autoplay_keep_item`
    protege Phase Door + Teleport de la venta agresiva (antes un Teleport hallado
    en mazmorra se vendía).
- **(F4) Amenaza v2 — HECHO.** `autoplay_monster_danger` afinado en tres frentes:
  - **Casters no-breath** (bolts/balls/causes/arrows/rocket): su daño escala con el
    NIVEL del caster, no con sus HP (el modelo viejo `maxhp/6` infravaloraba al
    hechicero frágil). Máscaras `AP_RF4_ATTACK`/`AP_RF5_ATTACK`; estima `nivel*3`
    por cast × frecuencia y se queda con el peor de los dos modelos.
  - **Invocadores** = pack diferido: máscaras `AP_RF4_SUMMON`/`AP_RF6_SUMMON`
    (todos los RF6_S_*); `dmg += dmg/2 + nivel`. El bot los mata primero cuando
    puede y los esquiva cuando no.
  - **Blow power por efecto real**: `autoplay_blow_power(effect)` replica la tabla
    de `check_hit` de melee1.c (HURT/SHATTER 60, UN_BONUS 20, UN_POWER 15,
    elementales/CONF/TERRIFY 10, drains/EXP 5, BLIND/PARALYZE 2, resto 0) en vez
    del plano HURT=60/resto=15 — el acierto estimado coincide con el del juego.
- **(F5) Economía — HECHO.** Tres piezas:
  - **Loot por valor**: `autoplay_cell_loot_value` (espejo de las reglas de
    `cell_has_*`) + `autoplay_loot_radius_for` — el radio de desvío del BOT escala
    con el valor: morralla < `junk_value` (15) = radio 0 (ni se desvía), normal =
    radio base, ≥ `good_value` (50) = 2x, ≥ `rich_value` (200) = 3x. Oro siempre
    interesa (sin slot ni peso); munición propia conserva su 2x con mochila llena.
    El check del BFS de `autoplay_explore_target` usa esto (y excluye la celda
    propia: una pierna de longitud 0 estancaba). **Los knobs se cachean** en
    estáticos al arrancar autoplay (`ap_junk/good/rich_value`): una call_lua por
    celda del flood sería un crawl. Ctrl-E conserva sus radios fijos.
  - **Mochila llena → soltar morralla**: paso 7d (`AP_DROPJUNK`, gated `!enemy`):
    suelta el stack no-keep más barato (< `junk_value`); el gate de valor del loot
    evita re-apuntarlo. Lo que quede tras esto sí justifica el viaje de venta
    (`autoplay_needs_resupply` ya disparaba con pack lleno — eso ya existía).
  - **Compras v2**: `autoplay_buy_armor_fill` viste los **slots de armadura
    VACÍOS** (body/cloak/shield/head/hands/feet) con la pieza más barata de la
    tienda ANTES del pase de mejor-upgrade (`buy_best_gear` ya cubría slots
    vacíos pero por mayor ganancia → un arma cara podía comerse el oro de 5
    piezas de AC). Cheapest-first, respeta `min_gold`.
- **(F6) Morralla trivial — HECHO.** `autoplay_low_value_foe` añade el caso
  "fideuá": hostil ≥ `trash_levels` (10, cacheado en `ap_trash_levels`) niveles
  por debajo Y con `danger*25 < mhp` → ignorado (ni perseguir, ni munición, ni
  desvío; se le pega si se pone adyacente, como el resto de low-value). XP de
  algo tan bajo es despreciable, no afecta al grind de F1.
- **(Menwan) WoR como escape de emergencia — HECHO.** Menwan murió en L11 a plev 8
  con un WoR y 4 Phase Doors sin leer. Paso 1c1b en `autoplay_decide` (antes de la
  lógica de packs, solo si `word_recall == 0`): leer Word of Recall cuando (a)
  crítico (`heal_big_at`) y sin curas — la pelea está perdida, programar la
  extracción y que el ladder aguante los 15-35 turnos — o (b) el nivel queda grande
  (`plev < dun_level - too_deep_slack`, knob def 0; p.ej. tras un trapdoor, que
  esquiva el gate de buceo porque no usa escaleras). Además, en desesperado con el
  enemigo PEGADO (`ed <= 1`) el escape scroll va ANTES que el flee a pie (huir
  con contacto regalaba un zarpazo por paso).
- **(Cerco/dead-ends) — HECHO.** Dos fixes de feedback de juego real:
  - **Dead-ends sin dibujar**: ver el suelo de lejos no ilumina la pared de detrás
    → los finales de pasillo quedaban como agujero negro en el mapa (parecía a
    medio explorar). El objetivo de delve de `autoplay_explore_target` incluye
    ahora suelo visto con algún grid vecino sin `CAVE_MARK`
    (`autoplay_unmarked_beside`): el bot recorre el último tramo y `note_spot`
    rellena la pared.
  - **Cerco de packs (hienas/worm masses)**: rodeado sin casilla libre y con cada
    miembro del anillo vetado por `too_dangerous`, el bot se quedaba QUIETO
    recibiendo zarpazos (sin objetivo de melee y sin movimiento). Cola nueva del
    tier letal: `autoplay_boxed_in()` (todos los vecinos muro/cuerpo) → programar
    recall si no hubo escape scroll (ya probado antes en el ladder) → **abrirse
    paso peleando contra el hostil adyacente MÁS DÉBIL**, ignorando el veto de
    peligro (dentro del anillo no significa nada).
- *Pendiente del plan de inteligencia (2026-06-11)*: F7 telemetría (post-mortem de
  muertes + últimas decisiones del Oráculo). Ideas v2: triaje de mochila contra el
  valor del loot objetivo; munición a granel ya cubierta (want_ammo).
- Afinar: umbrales de resupply/compra; **gates `plev` de la ruta** (data en `autoplay.lua`,
  con partidas reales).
- Gates `plev` de la ruta = estimaciones; afinar con partidas reales (el bot melee-only es frágil).
