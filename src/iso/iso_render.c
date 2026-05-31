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

void iso_unproject(int mx, int my, int px, int py,
                   int win_w, int win_h, int *cx, int *cy)
{
	/*
	 * iso_project es lineal: sx depende solo de icol, sy solo de irow, donde
	 *   icol = (cx - cy) - (px - py),  irow = (cx + cy) - (px + py).
	 * El centro del rombo de suelo del jugador (icol=irow=0) cae en pantalla en
	 *   X0 = win_w/2,  Y0 = win_h/2 - ISO_TILE_H/2 + ISO_FLOOR_CY.
	 * Invirtiendo (en reales) y despejando cx,cy:
	 *   icol = (mx - X0) / ISO_STEP_X,  irow = (my - Y0) / ISO_STEP_Y
	 *   cx = px + (icol + irow)/2,      cy = py + (irow - icol)/2
	 * La división entre 2 garantiza enteros con la paridad correcta (icol e irow
	 * siempre comparten paridad), así que redondeamos cx,cy directamente. */
	double x0 = win_w / 2.0;
	double y0 = win_h / 2.0 - ISO_TILE_H / 2.0 + ISO_FLOOR_CY;

	double icol = (mx - x0) / ISO_STEP_X;
	double irow = (my - y0) / ISO_STEP_Y;

	double a = (icol + irow) / 2.0;
	double b = (irow - icol) / 2.0;

	/* Redondeo al entero más cercano (válido para negativos). */
	*cx = px + (int)(a >= 0 ? a + 0.5 : a - 0.5);
	*cy = py + (int)(b >= 0 ? b + 0.5 : b - 0.5);
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
