# TomeTik — Handoff de sesión (continuar con otro agente)

> Estado a 2026-05-31. Rama: **`iso-tiles`**. El usuario hace SUS PROPIOS commits
> ("he commiteado, sigue con X"). NO commitees salvo que lo pida.

## Qué es esto
`/Users/raul/Documents/GitHub/TomeTik` = port moderno de **ToME 2.2.2 + tiles**
(antiguo "TomeTik", Windows GDI) que ahora compila en Docker (Debian 12, gcc 12).
Frontends: **X11/curses**, **GTK2 + tiles Gervais 32×32**, **Windows GDI (.exe mingw)**.
Acceso por **VNC :5900 + noVNC navegador :6080** (pass `tometik`). Pipeline de release
en GitHub Actions (rama `release/vX.Y.Z`). Roadmap futuro en `docs/roadmap.md`.

## Foco actual: RENDERER ISOMÉTRICO (modo `-i` del frontend GTK2)
Se eliminó el viejo motor Simutrans/SDL (muerto) y se escribió uno nuevo y limpio.

### Estado: FUNCIONA. Town de Bree jugable en iso con actores y tiendas.
- Núcleo portable (solo geometría, reusable en GDI luego): **`src/iso/iso_render.{h,c}`**
  — `iso_project()` (celda→pixel, jugador centrado) + `iso_render_scene()` (recorre
  celdas por profundidad cx+cy, back-to-front, + clipping + callback `iso_cell_fn`).
  Consts: TILE 54×49, FLOOR_H 27, STEP_X 25, STEP_Y 12, SHEET 14×15.
- Todo lo iso-específico de ToME vive en **`src/main-gtk2.c`** (busca `iso_`):
  - Se activa con `-i` (tras `--`: `./tome -mgtk2 -uPLAYER -- -i`). `run-vnc.sh`
    lo pasa con `TOMETIK_ISO=1`.
  - Carga `lib/xtra/iso/dg_iso32.gif` (cian #00FFFF→alfa) en `iso_sheet`.
  - `iso_cell_cb()`: lee `cave[cy][cx].feat` + `.info&CAVE_MARK`. Mapea feature→tile
    (índices portados de `../AngbandPlus/tk/config/dg32+iso.cfg`, tema "light smooth")
    + **auto-tiling de muros** `iso_wall_shape()` (portado de
    `../AngbandPlus/src/common/icon1.c::wall_shape`). Suelo 13, muro 71+forma, pilar 70,
    puerta 93/95/97 (+ns/we), escalera 99/100, puerta-madera-tienda 64/65, oscuro 208;
    hierba 0, agua 3/5, tierra 9, árbol 47, montaña 34, escombros 58 (overlays).
  - **Actores** (jugador/monstruos/objetos): `gerv_sheet` = `lib/xtra/graf/32x32.bmp`
    (Gervais 64×71 tiles 32px) con **NEGRO (0,0,0) → transparente** (es el bg del 2D,
    NO (24,24,24)). En cada celda, `map_info(cy,cx,&a,&c,&ta,&tc,&ea,&ec)` (firma 8
    params); si `(a&0x80)&&(a!=ta||c!=tc)` → blit tile fila=(a&0x7F) col=(c&0x7F),
    32×32 sobre el rombo. `map_info` ya incluye al `@`.
  - **Edificios town** = bloques macizos de muro permanente (en save PLAYER son
    `FEAT_PERM_SOLID`, no PERM_EXTRA). Decisión del usuario: dejarlos como **bloques de
    piedra normales** (opción A, sin tejado). Entrada `FEAT_SHOP` → puerta de madera.
  - **Tienda en iso**: el iso repintaba sobre el texto de la tienda. `character_icky`
    NO sirve (también TRUE al pintar el mapa → pantalla negra). Solución: flag
    **`bool iso_in_store`** en `src/store.c` (puesto junto a cada `character_icky` de
    store.c), `extern` en main-gtk2.c, gate `&& !iso_in_store` en `TERM_XTRA_FRESH`.
    Verificado: tienda muestra texto, ESC vuelve al iso.

### EDIFICIOS DEL PUEBLO (tejados) — RESUELTO 2026-05-31 (commiteado)
Síntoma: los edificios salían como losas grises planas y "transparentes" (solo se
pintaba el muro sur). CAUSA: los edificios del pueblo NO son `FEAT_PERM_SOLID`, sino
features de **TEJADO** (190 grass roof, 191 roof top, 192 chimney, 193-195 brick roof,
196/197 ventanas, 198 barril) — todas con flag `FF1_WALL` en f_info. No estaban en
`iso_is_wall_feat` → caían al `else` y se pintaban como suelo plano (tile 13).
FIX en `src/main-gtk2.c` (commiteado):
1. `iso_is_wall_feat` reescrito: un feat es muro iso si tiene `FF1_WALL` (vía
   `f_info[f].flags1`), EXCLUYENDO puertas (`iso_is_door_feat`, arco aparte) y
   overlays (`iso_overlay_tile(f)>=0`: árbol/montaña/escombros/árbol-muerto, que son
   WALL en f_info pero se pintan como sprite). Captura tejados/ventanas/barril +
   todos los muros reales sin listas de rangos. Forward-decls + guarda `max_f_idx`.
2. En el pueblo (`dun_level == 0`) los muros NO usan auto-tiling: cada celda = cubo
   entero (tile 70) uniforme. Replica el original (dg32+iso.cfg ChooseTheme fuerza
   `which=9` "white block" con !$depth). Edificio macizo -> masa de cubos limpia,
   opaca, sin transparencias. En mazmorra (dun_level>0) se mantiene el auto-tiling.
VERIFICADO por el usuario en VNC: "ahora se ve perfecto".

### PENDIENTE inmediato del iso
- **EL ELEFANTE**: cualquier pantalla full-screen (hoja de personaje `C`, inventario,
  mapa `M`, menús, ayuda, listas de hechizos, mensajes...) la TAPA el iso, porque
  `TERM_XTRA_FRESH` repinta la escena iso sobre data[0] incondicionalmente. El flag
  `iso_in_store` fue un parche puntual; falta una solución GENERAL (NO `character_icky`:
  también es TRUE al pintar el mapa de este TomeTik -> pantalla negra en el town).
- Mejor representación de casas (tejados con altura/color real, el usuario lo dejó
  para el futuro; hoy son cubos blancos macizos uniformes).
- Recuadro negro tras los sprites de actor: aceptado por ahora (arte Gervais).

## Cómo construir / ejecutar / PROBAR
```bash
cd /Users/raul/Documents/GitHub/TomeTik
./docker/build.sh gtk2            # compila (rc=0). Salida: src/tome y ./tome
# lanzar con la partida PLAYER (town de Bree) en iso:
docker rm -f tometik-play 2>/dev/null
docker run -dit --name tometik-play -e TOMETIK_SAVE=PLAYER -e TOMETIK_ISO=1 \
  -p 127.0.0.1:6080:6080 -p 127.0.0.1:5900:5900 -v "$PWD:/work" -w /work \
  tometik-build bash /work/docker/run-vnc.sh
# jugar: http://localhost:6080/vnc.html   (pass: tometik)
```
NO uses `docker ps --filter publish=5900 | xargs rm -f` (mató un contenedor del
usuario una vez; el classifier ya lo bloquea). Gestiona SOLO `tometik-play`.

### Tooling de captura/test (IMPORTANTE)
- `xdotool` NO viene en la imagen del contenedor recién creado (sí está en el
  Dockerfile ya, pero un contenedor ya construido sin rebuild de imagen lo necesita):
  tras `docker run`, ejecutar `docker exec tometik-play sh -c 'apt-get update -qq && apt-get install -y -qq xdotool'`.
- Avanzar el título / enfocar: `WID=$(xdotool search --name "TomeTik"|head -1);
  xdotool windowactivate $WID; xdotool key --window $WID Return` (×6). Mover: teclas
  numéricas `1-9` (numpad). ESC = `Escape`.
- Captura: hay un helper `/tmp/grab.sh` (xwd→PNG). Cabecera XWD: ancho@16, alto@20,
  bpp@44, bytes_per_line@48, ncolors@76 (¡el ancho NO está en 20!).
- **El agente NO podía VER imágenes** (límite de imágenes en contexto las rechaza).
  Solución usada: analizarlas con **PIL dentro del contenedor** (`docker exec ...
  python3 -c "from PIL import Image ..."`). Town iso ≈ 45% verde, 2% negro; pantalla
  de tienda/menú ≈ 93% negro + texto blanco/cian/amarillo. Capturas en `screencaps/`.

## Ficheros del iso (ya commiteados)
- `src/main-gtk2.c` (todo el iso: carga, mapeo feature→tile vía flag FF1_WALL,
  wall_shape en mazmorra, cubos uniformes en pueblo, actores, gate de tienda).
- `src/store.c` (flag `iso_in_store`).
- `src/iso/iso_render.{h,c}` (núcleo portable de geometría).
- `docker/Dockerfile` (+xdotool), `docker/run-vnc.sh` (TOMETIK_ISO).
- `docs/roadmap.md`, `session.md`, `screencaps/`.

## Memoria persistente
Lee la memoria del proyecto: `tometik-port-goal` (en
`/Users/raul/.claude/projects/-Users-raul-Documents-GitHub-AngbandPlus/memory/`).
Tiene el historial completo (compilación, GTK2, Windows, pipeline, todo el iso).
Constraints de plataforma: [[omnibandtk-platform-constraints]] (Mac arm64, sin macOS
nativo, dev arm64 vs release x86_64). NO usar la variante `../AngbandPlus/variant/ToMETk/`
como referencia (incompleta); usar el sistema `tk/` compartido de AngbandPlus.
