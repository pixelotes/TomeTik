# TomeTik — Roadmap

Mejoras y objetivos futuros (ToME 2.2.2 + tiles, port moderno en Docker).

## Render isométrico
- [ ] **Finalizar el modo isométrico.**
- [ ] **Hacer que el tamaño de los tiles sea seleccionable.**
- [ ] **Arreglar el cambio dinámico** isométrico ⇄ tiles 2D ⇄ ASCII.

## Ratón
- [ ] **Tooltips de casilla** ("You are looking at a wall", etc.).
- [ ] **Ir a coordenadas al hacer clic.**
- [ ] **Menú contextual con clic derecho.**

## Jugabilidad / motor
- [ ] **Campo de visión / niebla de guerra.**
- [ ] **Pathfinding A\*.**
- [ ] **Autoexplore.**

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
