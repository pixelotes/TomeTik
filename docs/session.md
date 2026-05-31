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

### MODO "Isometric" en el menú Graphics — HECHO 2026-05-31 (probado a mano)
El iso era un flag `-i` siempre activo; no se podía alternar (ni acceder a otros
modos de tiles). Ahora el iso es un 4º MODO de gráficos, junto a None/Old/New:
- `GRAF_MODE_ISO = 3` (main-gtk2.c). Entrada de menú `/Options/Graphics/Isometric`
  + check sincronizado en `graf_menu_update_handler`.
- En `init_graphics`: `GRAF_MODE_ISO` se configura IGUAL que `GRAF_MODE_NEW` (tiles
  Gervais 32x32 por debajo: el term 2D los pinta y `map_info` da los índices que el
  overlay iso usa para los actores). Al final, `iso_mode = (graf_mode==GRAF_MODE_ISO)`
  -> sólo en ese modo `TERM_XTRA_FRESH` pinta la escena iso; en None/Old/New el iso
  se apaga y se ve el render 2D normal. Cambio en caliente vía el menú (KTRL+R React).
- Láminas iso (dg_iso32.gif + 32x32.bmp) ahora con CARGA PEREZOSA en
  `iso_load_sheets()` (1ª vez que se entra a iso); si fallan, fallback a New.
- `-i` ahora hace `graf_mode_request = GRAF_MODE_ISO` (sigue arrancando en iso por
  defecto vía TOMETIK_ISO=1). Se eliminó el bloque de carga de láminas del arranque.
- IMPORTANTE: NO tocar `character_icky` para gatear el iso. Es TRUE durante TODO el
  juego normal (dungeon.c:5619-5857 envuelve el bucle) -> gatearlo apagaría el iso
  siempre y además rompía el redibujado global. Por eso el gate de tiendas es el flag
  puntual `iso_in_store` (que va bien). Alternar a otro modo es la vía para acceder a
  pantallas full-screen sin que las tape el iso.

### AUDITORÍA DE COBERTURA DE TILES iso — HECHO 2026-05-31
Por qué importa SOLO en iso: en iso solo bliteamos el sprite del actor si map_info
devuelve un tile gráfico (a&0x80). Una entidad sin tile (x_attr sin 0x80) es
INVISIBLE en iso (en 2D sale como letra ASCII). Herramienta: `iso_audit_coverage()`
en main-gtk2.c, disparada UNA vez en el 1er `iso_draw_scene` si `TOMETIK_ISO_AUDIT`
está en el entorno (OJO: hay que auditar en tiempo de RENDER, no en init_graphics:
allí los x_attr aún no están poblados por el prf -> falso "todo sin tile"). Escribe
`lib/user/iso_coverage.txt` (ANGBAND_DIR_USER). Recorre r_info/k_info (x_attr&0x80)
y f_info (features que iso_cell_cb pinta como suelo gris tile 13).
RESULTADO con save PLAYER: 0 monstruos, 0 objetos sin tile; 43 features como suelo
gris (5 falsos positivos: open floor, cobblestone road, town, field, rocky ground).
- **Neil, the Sorceror (R:1076)** no tenía tile (último r_idx, sin entrada en el prf)
  -> añadido `R:1076:0xAB/0x98` en graf-gervais.prf (reusa el tile del Sorcerer
  genérico R:638). Verificado: 0 monstruos sin tile.

### EXTRACCIÓN DE TILES 2D DE FEATURES FALTANTES — HECHO 2026-05-31
Para buscar equivalentes iso a ojo: extraídos los 42 tiles 2D Gervais (32x32) de las
features que en iso salen como suelo gris, a `screencaps/missing_tiles_2d/` con nombre
`F<idx>_<nombre>.png` + `_contact_sheet.png` (hoja etiquetada con idx/nombre/row,col).
Método (sin recompilar): parsear el informe + `lib/pref/graf-gervais.prf` (`F:idx:
0xAttr/0xChar` -> row=attr&0x7F, col=char&0x7F) y recortar `lib/xtra/graf/32x32.bmp`
(64x71 tiles de 32px). `F212 dead small tree` es ASCII (sin tile 2D). NOTA: `F183
void` salió como un gato blanco (posible rareza del prf), extraído fiel igualmente.

### MAPEO iso DE FEATURES FALTANTES — TANDA 1 APLICADA 2026-05-31
Lámina iso completa etiquetada en `screencaps/iso_sheet_labeled.png` (índices 0-209).
Aplicados en iso_cell_cb (43 -> 34 features como suelo gris):
- web(16) -> overlay 42 (red); field(181) -> suelo 30 (surcos); cobblestone road
  (200,201) -> suelo 15 (adoquín); rocky ground(207) -> suelo 58 (grava); dead small
  tree(212) -> overlay 46 sobre hierba; illusion wall(189) -> cubo muro 70 (es FLOOR
  pero se ve muro; caso especial en iso_is_wall_feat); quest exit(9) y town exit(12)
  -> arco de piedra abierto 93/94. (Auditoría actualizada para no marcarlas.)
HALLAZGO: dg_iso32 tiene CASAS iso de 1 celda (171 choza paja, 172/173 adobe, 174
cobertizo, 175/177 tejado rojo, 176 tejado gris, 178 puesto/toldo, 179 tejado HIERBA,
180 torre mago, 182-184 tiendas, 185/186 fortaleza) -> para el futuro de "casas reales"
(casan con feats tejado 190-195).

### TILES EXTRA DE DUNGEON ODYSSEY (do_extra) — TANDA 2 APLICADA 2026-05-31
Segunda fuente de tiles iso: **Dungeon Odyssey** (abandonware, licencia OK por el
usuario), en `dungeonodyssey/` (PNGs sueltos 54x54, iso, magenta #FF00FF transp.).
Tiene justo los huecos que faltaban. Pipeline:
- `tools/build_do_extra.py` ensambla los tiles elegidos en `lib/xtra/iso/do_extra.png`
  (7 cols; el ORDEN define el enum DO_* en main-gtk2.c -> no reordenar sin tocar el enum).
- main-gtk2.c: `do_sheet` (cargado en iso_load_sheets, magenta->alfa), `do_blit(idx,
  sx,sy)` blitea en `sy+DO_DY` (DO_DY=-5: alinea rombo de suelo, DO 54px vs dg 49px),
  `iso_do_tile(f)` mapea feature->índice DO, y un branch en iso_cell_cb (suelo + tile DO).
- Mapeado (43->18 features grises): fuente(2,15)=Fountain01; dark pit(87)=PitOpen; great
  fire(178)/fire(205)=PoolFire; trap(17)=TrapDoor; monster trap(175)=PitSpikesSilver;
  altares(161-165)=AltarNeutral/04/Good/Evil/05; nether mist(102)=PoolPoison; vapour/
  mist(208,210)=PoolMirky; condensing water(209)=PoolWater. Render de muestra (cada tile
  sobre el rombo) en `screencaps/do_tiles_on_iso_floor.png`. Verificado: town sin regresión.
- OJO repo: `dungeonodyssey/` son ~3000 PNGs fuente; solo se commitea `do_extra.png`
  (ensamblado) + el script. Plantear gitignore de `dungeonodyssey/`.

### TANDA 3 DE MAPEO (DO ampliado) — APLICADA 2026-05-31. COBERTURA ~100%.
Añadidos 10 tiles a do_extra.png (índices 14-23, enum DO_* ampliado; build_do_extra.py
soporta 'copy'/'pad'/'comp'). Mapeos elegidos por el usuario:
- glyph of warding (FEAT_GLYPH 3) -> GlyphGreen (Items, 32x32 padeado y centrado).
- explosive rune (FEAT_MINOR_GLYPH 64) -> GlyphRed.
- Straight Road tramos (65-70) -> L1_HB_Graveyard01 (suelo teal mágico); descargado
  (71) -> L1_HB_Darkwater01; salida (72) -> Graveyard01 + poof.gif (composite horneado);
  corrupto (73) -> Darkwater01 + L1_Terrain027 (composite).
- Underground Tunnel (173, 204) -> L1_Terrain049.
- Void Jumpgate (FEAT_BETWEEN2 176) -> HB_SummoningPortal01.
- void (183) -> L1_HB_FloorStone06.  town (FEAT_TOWN 203) -> L2_Town01.
Fuentes en subdirs DO: Terrain/, XR module/, Items/, Silmar/ (poof). Las 32x32 se
padean centradas (27,37); poof es GIF (transp. índice) -> convert RGBA. Composites se
hornean en el script (magenta->alfa por capa). Preview de los 24 en /tmp (no commiteado).
AUDITORÍA FINAL: 0 mon, 0 obj sin tile; SOLO 2 features grises (F1 y F172 open floor),
que es CORRECTO (suelo = tile 13, el último de la 1ª fila de dg_iso32). Town sin regresión.

### SPRITE DE EDIFICIO (bloque rojo) — HECHO 2026-05-31
El usuario dibujó `lib/xtra/iso/building_block.png` (54x49 RGBA, alfa propio): cubo de
piedra MÁS ALTO (opaco y0-48 vs el 70 que era y7-48) con la TAPA TINTADA DE ROJO.
- main-gtk2.c: `bldg_block` (cargado en iso_load_sheets vía gdk_pixbuf_new_from_file,
  el PNG ya trae alfa -> sin color clave). En el branch del pueblo (dun_level==0), TODA
  celda de muro se blitea con bldg_block en (sx,sy); fallback a cubo 70 si no carga.
- Se PROBÓ distinguir edificio (tejados 190-198 / FEAT_PERM_EXTRA) vs muralla
  (FEAT_PERM_SOLID) para dejar la muralla gris, pero la CARA SUR de los edificios es
  FEAT_PERM_SOLID -> salía gris (mal). Revertido: todo el muro del pueblo = bloque rojo.
- Casas iso 1-celda de dg_iso32 (171-186) NO sirven: no son tileables (extraídas en
  screencaps/building_tiles/ por si acaso). El bloque rojo es la solución elegida.
- Diagnóstico útil: histograma de feats del nivel en la auditoría (TOMETIK_ISO_AUDIT).
  Bree = 628 PERM_SOLID (borde/muralla) + ~279 tejado (190-195, edificios).
Captura: screencaps/iso_town_red_buildings.png.

### ISO: estado al cerrar (2026-05-31) — buen punto de parada
Modo iso funcional y pulido: town de Bree jugable, modo "Isometric" conmutable en
Options->Graphics, cobertura de tiles ~100% (entidades vía graf-gervais.prf+Neil;
features vía dg_iso32 + Dungeon Odyssey do_extra), edificios con sprite rojo propio.

### PENDIENTE futuro del iso (cuando se retome)
- 2º foco no abordado: sprites ALTOS/GRANDES en iso (anclaje/altura + recuadro negro
  tras los actores), falta ejemplo concreto (en Bree solo humanoides 1x1).
- Verificar in-game los tiles DO (fuente/altar/portal/Straight Road/fuego) en
  mazmorra/quest: en Bree no aparecen, validados solo por render sintético.
- (Opcional) edificios de 2 alturas (apilar building_block con offset vertical).
- (Opcional) pantallas full-screen dentro del modo iso (hoja C, inventario, mapa M)
  las tapa el iso; hoy se accede cambiando de modo de gráficos.
- 2º foco del usuario: afinar sprites ALTOS/GRANDES en iso (anclaje/altura + recuadro
  negro). Falta un ejemplo concreto (en el pueblo solo hay humanoides 1x1).
- Mejor representación de casas (tejados con altura/color real; hoy cubos blancos).
- (Opcional) pantallas full-screen dentro del propio modo iso las sigue tapando el
  iso; de momento se accede cambiando de modo de gráficos.

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
