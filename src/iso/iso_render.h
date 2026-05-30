/*
 * iso_render.h - Núcleo PORTABLE del renderer isométrico de TomeTik.
 *
 * Reemplaza al antiguo motor Simutrans/SDL (eliminado). Reutiliza los tiles
 * isométricos de David Gervais que usa OmnibandTk (lib/xtra/iso/dg_iso32.gif,
 * lámina de 14x15 tiles de 54x49) y su esquema de proyección (de dg32+iso.cfg:
 * width 54, height 49, floor 27, bottom 11, overlap 2/1).
 *
 * DISEÑO: este módulo NO depende de ningún toolkit (ni GTK ni SDL ni GDI).
 * Contiene solo la GEOMETRÍA isométrica (proyección celda->pantalla y el
 * recorrido en orden de profundidad). El frontend concreto aporta:
 *   - una función de "blit" (dibujar un tile de la lámina en (sx,sy)),
 *   - la captura del mapa y la selección de tile por celda.
 * Así, el iso de GTK2 habilita GDI casi gratis (solo cambia el blit).
 */

#ifndef ISO_RENDER_H
#define ISO_RENDER_H

/* --- Geometría del tile iso (de dg32+iso.cfg) --- */
#define ISO_TILE_W    54          /* ancho del tile en la lámina            */
#define ISO_TILE_H    49          /* alto total del tile (suelo + pared)    */
#define ISO_FLOOR_H   27          /* alto de la cara-suelo (rombo)          */
#define ISO_OVERLAPX  2           /* solape horizontal entre celdas         */
#define ISO_OVERLAPY  1           /* solape vertical entre celdas           */

/* Paso en pantalla por unidad de coordenada isométrica (rombo 2:1). */
#define ISO_STEP_X    (ISO_TILE_W / 2 - ISO_OVERLAPX)   /* 25 */
#define ISO_STEP_Y    (ISO_FLOOR_H / 2 - ISO_OVERLAPY)  /* 12 */

/* Lámina dg_iso32.gif: rejilla de tiles. */
#define ISO_SHEET_COLS 14
#define ISO_SHEET_ROWS 15
#define ISO_TRANSPARENT 0x00FFFF   /* color clave (cian) en la lámina       */

/*
 * Proyecta una celda del cave (cx,cy) a la esquina superior-izquierda del
 * tile en la ventana, centrando la vista en la celda del jugador (px,py).
 * win_w/win_h = tamaño de la ventana en píxeles.
 */
void iso_project(int cx, int cy, int px, int py,
                 int win_w, int win_h, int *sx, int *sy);

/*
 * Callback por celda visible. El núcleo lo invoca en ORDEN DE PROFUNDIDAD
 * (de atrás -menor x+y- hacia delante), pasando la celda del cave y su
 * posición en pantalla ya proyectada. El frontend selecciona el/los tile(s)
 * y los dibuja (suelo, luego muro/objeto/monstruo). ctx es opaco al núcleo.
 */
typedef void (*iso_cell_fn)(void *ctx, int cx, int cy, int sx, int sy);

/*
 * Recorre todas las celdas del cave potencialmente visibles en una ventana
 * win_w x win_h centrada en el jugador (px,py), en orden de profundidad, y
 * llama a 'cell' por cada una. Hace el clipping grueso (celdas fuera de la
 * ventana se omiten). No dibuja nada por sí mismo.
 */
void iso_render_scene(void *ctx, int px, int py,
                      int win_w, int win_h, iso_cell_fn cell);

#endif /* ISO_RENDER_H */
