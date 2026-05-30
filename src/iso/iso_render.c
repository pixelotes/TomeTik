/*
 * iso_render.c - Núcleo portable del renderer isométrico (ver iso_render.h).
 *
 * Solo geometría: proyección celda->pantalla y recorrido en orden de
 * profundidad. Sin dependencias de toolkit. El frontend hace el blit.
 *
 * Proyección isométrica clásica (igual que OmnibandTk, widget1-dll.c):
 *   iso_col = cx - cy        (eje horizontal en pantalla)
 *   iso_row = cx + cy        (eje de profundidad / vertical)
 * relativas a la celda del jugador (px,py), que queda centrada en la ventana.
 */

#include "iso_render.h"

void iso_project(int cx, int cy, int px, int py,
                 int win_w, int win_h, int *sx, int *sy)
{
	/* Coordenadas isométricas relativas al jugador. */
	int icol = (cx - cy) - (px - py);
	int irow = (cx + cy) - (px + py);

	/* Centrar el tile del jugador (icol=irow=0) en la ventana. */
	*sx = win_w / 2 - ISO_TILE_W / 2 + icol * ISO_STEP_X;
	*sy = win_h / 2 - ISO_TILE_H / 2 + irow * ISO_STEP_Y;
}

void iso_render_scene(void *ctx, int px, int py,
                      int win_w, int win_h, iso_cell_fn cell)
{
	/* Alcance en celdas del cave que cubre la ventana (con margen para que
	 * los tiles altos -muros- que entran por los bordes no se corten). */
	int rx = win_w / (2 * ISO_STEP_X) + 3;
	int ry = win_h / (2 * ISO_STEP_Y) + 3;
	int reach = rx + ry;
	int depth;

	/* Recorrer por profundidad creciente (cx+cy) = de atrás hacia delante,
	 * para que las celdas más cercanas (mayor x+y, más abajo en pantalla)
	 * se dibujen ENCIMA de las lejanas (z-order correcto por sobrescritura). */
	for (depth = -2 * reach; depth <= 2 * reach; depth++)
	{
		int cx;
		for (cx = px - reach; cx <= px + reach; cx++)
		{
			/* En esta línea de profundidad: cx + cy = (px+py) + depth. */
			int cy = (px + py + depth) - cx;
			int sx, sy;

			if (cy < py - reach || cy > py + reach) continue;

			iso_project(cx, cy, px, py, win_w, win_h, &sx, &sy);

			/* Clipping grueso a la ventana. */
			if (sx + ISO_TILE_W < 0 || sx > win_w) continue;
			if (sy + ISO_TILE_H < 0 || sy > win_h) continue;

			cell(ctx, cx, cy, sx, sy);
		}
	}
}
