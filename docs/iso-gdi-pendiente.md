# ISO en el frontend GDI (Windows) — tareas pendientes

Estado del port del **renderer isométrico al frontend Windows GDI** (`src/main-win.c`).
El iso ya funciona en GTK2/Linux; en GDI está **a medias (hito 1)** y **congelado a
petición** (2026-06-02). Este doc recoge todo lo que falta para terminarlo.

> Núcleo reutilizable (NO tocar, ya es portable): `src/iso/iso_render.{c,h}` —
> `iso_project()` (celda→píxel), `iso_unproject()` (píxel→celda) e
> `iso_render_scene()` (recorre el cave por profundidad y llama a un callback por
> celda). Constantes: tile 54×49, lámina 14×15.
>
> **Referencia a portar**: `src/main-gtk2.c::iso_cell_cb()` (≈línea 3611) — la
> implementación GDK/pixbuf completa (suelo, muros con auto-tiling, puertas,
> escaleras, tiendas, features especiales, actores Gervais 32×32, barras de vida).
> El trabajo de GDI es traducir ese callback de GDK a GDI (`BitBlt` con
> máscara), reusando el mismo `iso_render_scene()`.

## Hecho (hito 1) — en el working tree de la rama `2.2.2`, SIN commitear
- Menú **Options → Graphics → "Isometric view"** (`angband.rc`, id **412**;
  `IDM_OPTIONS_ISO` en `main-win.c`), con checkmark en `setup_menus()` y toggle
  en `process_menus()`.
- Globals: `iso_mode`, `iso_loaded`, `isoGraph`, `isoMask` (`DIBINIT`).
- `win_iso_load()`: carga perezosa de la lámina con `ReadDIB` desde
  `lib/xtra/iso/dg_iso32.bmp` (+ `dg_iso32_mask.bmp`), celda 54×49.
- `win_iso_test_blit()`: blit de UN tile en la esquina del mapa (validación de
  tubería) — llamado desde `WM_PAINT` de `AngbandWndProc` cuando `iso_mode`.
- **Assets nuevos** (generados de `dg_iso32.gif` con PIL; cian 0x00FFFF =
  transparente → color: cian→negro; máscara: cian→blanco, resto negro):
  `lib/xtra/iso/dg_iso32.bmp` y `lib/xtra/iso/dg_iso32_mask.bmp` (24-bit).
- Blit transparente = patrón de `Term_pict_win`: `BitBlt(SRCAND)` con la máscara
  + `BitBlt(SRCPAINT)` con el color. `ReadDIB` soporta 24-bit (readdib.c:117).

## Prerequisito (bloquea el hito 2)
- [ ] **Añadir `iso/iso_render.o` a los OBJS de `src/makefile.mingw`** (ahora
      solo está `pathfind.o`; `iso_render.c` NO se compila en Windows todavía).
      Comprobar también que el clean borra `iso/*.o` (ya se hace en
      `docker/in-container-build.sh`).
- [ ] `#include "iso/iso_render.h"` en `main-win.c`.

## Hito 2 — suelo (recorrer el cave)
- [ ] Sustituir `win_iso_test_blit()` por un render real: una función
      `win_iso_cell_cb(ctx, cx, cy, sx, sy)` (firma `iso_cell_fn`) que blitea el
      tile de SUELO de la celda, y un `win_iso_redraw(td)` que llama a
      `iso_render_scene(td, p_ptr->px, p_ptr->py, map_w, map_h, ox, oy, win_iso_cell_cb)`.
- [ ] Calcular `map_w/map_h/ox/oy` (área del mapa en píxeles dentro de la ventana
      data[0], descontando la barra lateral; ver `COL_MAP`/`ROW_MAP` y
      `win_map_pixel_to_cave()`).
- [ ] Limpiar el área de mapa (rellenar de negro) antes de pintar la escena.
- [ ] Selección de tile de suelo por `cave[cy][cx].feat` (mapeo feat→índice de la
      lámina; copiar las tablas de `main-gtk2.c`: suelo 13, etc.).

## Hito 3 — muros, features, puertas, escaleras
- [ ] Auto-tiling de muros: portar `iso_wall_shape()`/`iso_wall_off[]` de
      `main-gtk2.c`. Pueblo (dun_level==0) = bloques macizos + heurística
      casa/muralla; mazmorra = muros finos por forma.
- [ ] Puertas (abierta/rota/cerrada, dirección con `iso_door_we`), `FEAT_SHOP`
      (puerta de madera), salidas de quest/pueblo, escaleras arriba/abajo.
- [ ] Features con tile propio de Dungeon Odyssey (`do_extra.png`) y custom
      (flores, escombros, bloque-edificio): en GDI habría que convertir esos PNG
      a `.bmp`+máscara como se hizo con `dg_iso32`, o de momento omitirlos.
- [ ] Celdas no vistas (`!CAVE_MARK && !CAVE_SEEN`) → tile oscuro.

## Hito 4 — actores (jugador / monstruos / objetos) + barras de vida
- [ ] Lámina Gervais 32×32 para actores: reusar `infGraph` cuando
      `arg_graphics==3` (es `32x32.bmp`+`mask32.bmp`), o cargar aparte. Negro =
      transparente en esa lámina.
- [ ] Por celda: `map_info(cy, cx, &a,&c,&ta,&tc,&ea,&ec)`; si `(a&0x80)` y
      `(a!=ta || c!=tc)` → blit sprite fila=`a&0x7F`, col=`c&0x7F`, 32×32,
      apoyado en el rombo del suelo (ver offsets de `iso_cell_cb`).
- [ ] Barras de vida sobre jugador/monstruos visibles (`p_ptr->chp/mhp`,
      `m_list[].hp/maxhp`): rectángulos GDI (verde/rojo/negro), como en gtk2.

## Hito 5 — integración / UX
- [ ] **El toggle debe cambiar el render del mapa de verdad** (no solo overlay
      de prueba): cuando `iso_mode`, NO dibujar la rejilla de texto del área de
      mapa; dibujar la escena iso. La barra lateral / mensajes (texto del term)
      se mantienen. Decidir el hook: en `WM_PAINT` y/o forzar redibujado iso en
      cada cambio de mapa (equivalente al refresh del gtk2; ver cómo gtk2 dispara
      `iso_render_scene` en su expose/refresh).
- [ ] **Ratón → celda con `iso_unproject()`** (la actual `win_map_pixel_to_cave`
      es para rejilla 2D). Necesario para click-to-move y tooltips de casilla en
      modo iso.
- [ ] Tooltips (#3 ya arreglado) y click-to-move deben usar `iso_unproject`
      cuando `iso_mode`.
- [ ] (Opcional) Persistir `iso_mode` en `tometik.ini`.
- [ ] Interacción con bigtile/zoom: el iso tiene su propia escala (54×49); al
      activar iso conviene ignorar/forzar el modo de tiles 2D.

## Gotchas
- GDK (gtk2) usa pixbuf con alfa; GDI usa máscara monocroma (AND/OR). Por eso
  cada lámina iso necesita su `.bmp` + `_mask.bmp` (ya hecho para `dg_iso32`).
- Paleta: las `.bmp` iso son 24-bit (sin tabla de color) para evitar líos de
  paleta con `infGraph`. Si se cambian a 8-bit, vigilar `RealizePalette`.
- Probar SIEMPRE en **Windows real**: Wine arranca el `.exe` pero **crashea al
  jugar** (no sirve para probar el iso). El loop es: compilar en Mac
  (`./docker/build.sh windows`) → copiar `.exe` **+ `lib/xtra/iso/*.bmp`** a
  Windows → probar.

## Cómo se construye/prueba
```sh
./docker/build.sh windows     # genera tometik.exe (cross mingw-w64)
# copiar tometik.exe + lib/xtra/iso/dg_iso32*.bmp a la instalación Windows
```
