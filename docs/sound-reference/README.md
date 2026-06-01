# Referencia de sonido (de OmnibandTk)

Estos ficheros **NO los usa TomeTik** (que es C, no Tcl). Son solo **referencia**
para cuando se implemente el backend de sonido (ver `docs/roadmap.md` → Audio).

- `init-other.tcl` — lógica de OmnibandTk que descubre los `*.wav` y asigna sonidos
  a eventos del juego (qué `.wav` suena en cada evento). Útil para reconstruir el
  mapeo evento→sonido en C.
- `music.tcl` — lógica de música de OmnibandTk.

Los WAVs en sí están en **`lib/egg/`** (copiados de `AngbandPlus/lib/egg`, 398 ficheros).
La música medieval con licencia libre habrá que buscarla aparte (no copyright).
