# TomeTik — Roadmap

Mejoras y objetivos futuros (ToME 2.2.2 + tiles, port moderno en Docker).

## Render isométrico
- [ ] **Finalizar el modo isométrico.**
- [ ] **Hacer que el tamaño de los tiles sea seleccionable.**
- [ ] **Arreglar el cambio dinámico** isométrico ⇄ tiles 2D ⇄ ASCII.

## Ratón
- [x] **Tooltips de casilla** ("You are looking at a wall", etc.). Texto del motor
  vía `describe_grid()` (xtra2.c, gemelo no interactivo de `target_set_aux`,
  modelado en `angtk_examine` de OmnibandTk). GTK2: popup que sigue al ratón en
  los 3 modos (iso/2D/ASCII) y single/double wide. GDI: tracking tooltip Win32.
- [x] **Ir a coordenadas al hacer clic** (click-to-walk). Clic izq. en casilla
  transitable → A\* (con corner-cutting) + travel paso a paso por turno
  (`travel_to`/`travel_step`/`travel_cancel`, cmd1.c). Reusa la identificación de
  celda del tooltip; GTK2 (iso/2D/ASCII) y GDI. Mensajes de inicio/llegada/parada.
- [ ] **Menú contextual con clic derecho.**

## Interfaz / UX
- [ ] **Barras de vida sobre los personajes** (jugador y monstruos) cuando los PV
  están por debajo del 100%. Mejora de UX: feedback visual del estado de salud sin
  abrir menús. Mostrar solo si HP < máximo; ocultar al 100%.

## Jugabilidad / motor
- [ ] **Campo de visión / niebla de guerra.**
- [x] **Pathfinding A\*** (`pathfind.{h,c}`): A* genérico con heap binario, 4-dir /
  8-dir / 8-dir con corner-cutting, callback de caminabilidad sobre `cave[][]`.
- [x] **Autoexplore** (estilo DCSS, tecla **Ctrl-E**): viaja a la frontera no
  explorada más cercana (BFS desde el jugador), tramo a tramo, hasta "Done
  exploring." Capa sobre travel (`do_cmd_explore`/`explore_step`, cmd1.c). Usa un
  bitmap "visto alguna vez" por nivel (reset con `old_turn`) porque ToME no
  memoriza el suelo de pasillos oscuros. Se detiene con `disturb()` (monstruos,
  daño, tecla). Pendiente de pulir: recoger objetos / parar en escaleras y rincones.

## Base de código
- [ ] **Actualizar la base de ToME a 2.3.5.**
- [ ] **Backportear mejoras de ToME 2.4.0ah** (la versión C++).

## Audio
- [ ] **Habilitar sonidos y música.** Los sonidos de OmnibandTk ya están copiados en
  **`lib/egg/`** (398 WAVs, ~18MB). El mapeo evento→sonido y la lógica de música de
  OmnibandTk (Tcl, solo de referencia) están en **`docs/sound-reference/`**. Falta un
  backend de sonido en C (SDL_mixer/OpenAL en Linux; equivalente en Windows/GDI).
- [ ] **Música:** buscar música de inspiración medieval con licencia libre (no copyright).
  - OmnibandTk usaba **FMOD** (`fmod.dll`); formatos soportados (de `music.tcl`):
    módulos de tracker (`.mod .it .xm .s3m .mtm .umx .mo3`), `.mp3/.mp2/.mp1`, `.ogg`,
    `.wav`. **NO MIDI** (habría que sintetizarlo a OGG/WAV antes).
  - Backend nuevo recomendado: **SDL_mixer** (soporta OGG/MP3/WAV/MOD). Formatos
    sweet-spot: **OGG** (cómodo, mucha música CC) o **módulos de tracker** (`.it/.xm/.mod`,
    diminutos, catálogos grandes de música medieval/fantasy libre, p.ej. modarchive.org).
