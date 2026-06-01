# TomeTik — Roadmap

Mejoras y objetivos futuros (ToME 2.3.5 + tiles, port moderno en Docker).

## Render isométrico
- [x] **Finalizar el modo isométrico.**
- [ ] **Hacer que el tamaño de los tiles sea seleccionable.**
- [x] **Arreglar el cambio dinámico** isométrico ⇄ tiles 2D ⇄ ASCII.
- [ ] **Integrar los tiles sueltos en `do_extra.png`** (building_block.png,
  grass_flowers.png, rubble.png → al atlas, en vez de PNGs individuales).
- [ ] **Mejorar el rendimiento del redibujado iso** (hoy se repinta la escena
  entera en cada FRESH y en cada cambio de celda en hover).

## Ratón
- [x] **Tooltips de casilla** ("You are looking at a wall", etc.). Texto del motor
  vía `describe_grid()` (xtra2.c, gemelo no interactivo de `target_set_aux`,
  modelado en `angtk_examine` de OmnibandTk). GTK2: popup que sigue al ratón en
  los 3 modos (iso/2D/ASCII) y single/double wide. GDI: tracking tooltip Win32.
- [x] **Ir a coordenadas al hacer clic** (click-to-walk). Clic izq. en casilla
  transitable → A\* (con corner-cutting) + travel paso a paso por turno
  (`travel_to`/`travel_step`/`travel_cancel`, cmd1.c). Reusa la identificación de
  celda del tooltip; GTK2 (iso/2D/ASCII) y GDI. Mensajes de inicio/llegada/parada.
- [x] **Resaltar el tile bajo el ratón**: en iso, rombo de suelo amarillo en la
  celda en hover, dibujado al final con prioridad sobre todo (también sobre celdas
  desconocidas). En 2D ya funciona también. (ASCII no aplica.)
- [x] **Atacar al hacer clic en un enemigo adyacente** (`do_cmd_click`/`click_act_step`,
  cmd1.c): `move_player_aux` hacia el monstruo → ataca hostiles, intercambia con
  aliados. Clic lejano sigue viajando y parándose al lado.
- [ ] **Menú contextual con clic derecho.**
- [ ] **Punteros de ratón contextuales** en iso (botas=mover, espada=atacar,
  labios=hablar) según lo que haya bajo el cursor. Ligado al autodesplazamiento.

## Interfaz / UX
- [x] **Barras de vida sobre los personajes** (jugador y monstruos) con HP<100%:
  barra verde/rojo con marco negro sobre el sprite, en `iso_cell_cb`. **Solo modo
  iso** (decisión: no se portan a 2D/ASCII por purismo).
- [x] **Tooltip de casilla muestra TODO** lo del tile (monstruo + objetos + trampa +
  suelo), con retardo de aparición y estilo (GTK2 y GDI).
- [X] **Sidebar de stats en iso**: hecho parcial — se reserva el margen izquierdo y
  se recompone la barra 2D; revisar si el viewport iso queda bien proporcionado.
- [ ] **Bug: el menú `wield` no aparece en la pantalla principal** (sí salen eat /
  inventory / equip). Posible trigger/refresco que falta al abrirlo.
- [ ] **Mejoras surtidas de UX** (varias, a definir).

## Jugabilidad / motor
- [~] **Campo de visión / niebla de guerra.** En **superficie** (pueblo/exterior
  local, `!dun_level && !wild_mode`) la visión es **total**: al entrar al nivel se
  llama `wiz_lite()` (ilumina+memoriza todo), como de día. En **mazmorra** se
  conserva el FOV/radio de antorcha normal. Pendiente: niebla de guerra propiamente
  (recordar visto vs visible), revelar monstruos en superficie (ahora solo en LOS).
- [x] **Pathfinding A\*** (`pathfind.{h,c}`): A* genérico con heap binario, 4-dir /
  8-dir / 8-dir con corner-cutting, callback de caminabilidad sobre `cave[][]`.
- [x] **Autoexplore** (estilo DCSS, tecla **Ctrl-E**): viaja a la frontera no
  explorada más cercana (BFS desde el jugador), tramo a tramo, hasta "Done
  exploring." Capa sobre travel (`do_cmd_explore`/`explore_step`, cmd1.c). Usa un
  bitmap "visto alguna vez" por nivel (reset con `old_turn`) porque ToME no
  memoriza el suelo de pasillos oscuros. Se detiene con `disturb()` (monstruos,
  daño, tecla). Pendiente de pulir: recoger objetos / parar en escaleras y rincones.
- [ ] **Autoexplore más inteligente.** Mejorar el selector de objetivo (evitar la
  segunda vuelta por rincones/finales de pasillo sin ver), priorizar objetos/
  escaleras/features, recoger objetos al pasar, y mejores condiciones de parada.

## Base de código
- [ ] **Actualizar la base de ToME a 2.3.5.**
- [ ] **Backportear mejoras de ToME 2.4.0ah** (la versión C++).

## Audio
- [x] **Sonidos.** Backend hecho en ambos frontends, **desactivado por defecto**,
  activable desde el nuevo **menú Audio → Sound**. GTK2: **SDL2_mixer** (mezcla real,
  carga perezosa, parsea `lib/xtra/sound/Sound.cfg`). GDI: `PlaySound` (Win32, ya
  existía bajo `USE_SOUND`, ahora habilitado). Assets: 59/64 eventos de ToME mapeados
  a los WAV de OmnibandTk (`lib/egg`), **copiados+renombrados** a `lib/xtra/sound/` con
  el nombre del evento, + `Sound.cfg` generado. Vacíos a propósito: flee, walk,
  hitwall, wakeup, unused. **OJO:** no se oye en el Docker/noVNC (headless, sin
  tarjeta de sonido ni audio por VNC); se prueba en el `.exe` Windows o build Linux
  nativo con audio. Docker build lleva ahora `libsdl2-dev`/`libsdl2-mixer-dev`.
- [~] **Música.** Toggle **Audio → Music** + backend GTK2 (SDL2_mixer `Mix_Music`,
  busca el primer fichero en `lib/xtra/music/`, loop). **Falta la música en sí**
  (medieval, licencia libre). GDI: toggle presente, reproducción pendiente.
  Pendiente: buscar música de inspiración medieval con licencia libre (no copyright).
  - OmnibandTk usaba **FMOD** (`fmod.dll`); formatos soportados (de `music.tcl`):
    módulos de tracker (`.mod .it .xm .s3m .mtm .umx .mo3`), `.mp3/.mp2/.mp1`, `.ogg`,
    `.wav`. **NO MIDI** (habría que sintetizarlo a OGG/WAV antes).
  - Backend nuevo recomendado: **SDL_mixer** (soporta OGG/MP3/WAV/MOD). Formatos
    sweet-spot: **OGG** (cómodo, mucha música CC) o **módulos de tracker** (`.it/.xm/.mod`,
    diminutos, catálogos grandes de música medieval/fantasy libre, p.ej. modarchive.org).
