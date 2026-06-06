# Wall-shapes (auto-tiling de muro, estilo "roof") para Gervais 32×32

Banco de formas para portar el auto-tiling de muro de ZAngbandTk/OAngbandTk
(las shapes `corner_*`/`tri_*`/`ns`/`we`/`quad`) al **modo cenital Gervais 32×32**
de TomeTik. No existe arte Gervais cenital para esto (ni en OmnibandTk: allí el
roof cenital es Adam Bolt 16px, y las formas Gervais solo viven en la config ISO).
Por eso hay que dibujarlo. Estos ficheros son el punto de partida.

## Ficheros

| Fichero | Qué es | Uso |
|---|---|---|
| `shape_template_32.png` | Plantillas EN BLANCO, 128×96 (4×3 tiles de 32px), fondo transparente. Solo silueta (cian) + cara expuesta marcada en rojo. | Importar en editor de píxeles y **pintar encima**. |
| `shape_reference_32.png` | Las 12 formas recortadas proceduralmente del **granito plano** (feat 56, `0x80/0x84`), con el corte diagonal transparente. El "equivalente 2D" de arranque. | Referencia / base a repintar. |
| `shape_guide.png` | Versión 8× anotada (nombre + caras abiertas), damero detrás para ver la transparencia. | Guía humana mientras dibujas. |

## Layout (índice → forma), rejilla 4 columnas × 3 filas

```
 0 quad        1 ns          2 we          3 single
 4 corner_nw   5 corner_ne   6 corner_sw   7 corner_se
 8 tri_n       9 tri_s       10 tri_e      11 tri_w
```

## Convención

- **`open` = lados donde el vecino es SUELO** (cara expuesta del muro). Ahí el "tejado"
  baja en pendiente / se sombrea. En la plantilla van marcados en **rojo**.
- **`quad`**: 4 vecinos muro → interior, sin bisel, tile lleno y plano.
- **`ns` / `we`**: tramo recto (vertical / horizontal) → tile lleno; la pendiente va en
  las dos caras expuestas (E-W o N-S). Sin corte diagonal.
- **`corner_*`**: esquina exterior → triángulo de granito + **corte diagonal a 45°**
  por donde se ve el suelo. `corner_nw` = abierto al N y al W (muro va a S y E), etc.
- **`tri_*`**: unión en T, una sola cara expuesta (la indicada por la letra).
- **`single`**: pilar aislado (4 caras abiertas) → octágono (4 esquinas chaflanadas).

> ⚠️ La nomenclatura de `tri_*` / `corner_*` aquí es la **mía** y hay que reconciliarla
> con `iso_wall_off[]` / `iso_wall_shape()` de `src/main-gtk2.c` al cablear (la lógica
> de vecinos C ya existe ahí; solo cambia a qué tile mapea cada shape en cenital).

## Transparencia

Las PNG llevan alfa real para que dibujes cómodo. Al empaquetar en `32x32.bmp`
(24-bit, sin alfa) la zona transparente debe ser el **color-key** de la hoja
(negro `0x000000` por convención Gervais). El revelado del suelo bajo el corte
diagonal lo hace ya `overlay_tiles_2()` en `main-gtk2.c`: capa terreno = suelo,
capa overlay = la pieza de forma; donde el overlay == `bg_pixel` translúce el suelo.

## Siguiente paso al cablear (resumen)

1. Reservar 12 slots vacíos en `32x32.bmp` (hay sitio: 64×71 tiles).
2. En `map_info()` (cave.c), gated por `use_graphics==GRAPHICS_GERVAIS` + flag:
   para feats de muro, terrain layer ← suelo, overlay layer ← `BASE_SHAPE + wall_shape(y,x)`.
3. `wall_shape(y,x)`: extraer la lógica de `iso_wall_shape()` a función compartida.
