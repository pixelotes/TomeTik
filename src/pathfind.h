/* File: pathfind.h */

/*
 * Generic A* (A-star) pathfinding, decoupled from the game state so it can be
 * reused from any frontend (GDI, GTK2, X11, iso, ...) and from any game code
 * (monster AI, "travel to", auto-explore, mouse "click to walk", ...).
 *
 * Coordinates use the ToME (y, x) convention (row, column).
 *
 * Two entry points are provided:
 *   - astar_find_path()    : you pass a flat byte matrix of walkable tiles.
 *   - astar_find_path_cb() : you pass a callback that answers "walkable?",
 *                            so you can wrap cave[][] without copying it.
 *
 * The returned path_result is heap-allocated; free it with path_free().
 */

#ifndef INCLUDED_PATHFIND_H
#define INCLUDED_PATHFIND_H

#include "h-basic.h"

/* A single grid coordinate on a path (row, column). */
typedef struct path_point path_point;
struct path_point
{
	s16b y;
	s16b x;
};

/*
 * The result of a pathfinding query.
 *
 * 'steps' lists the path from the start tile to the goal tile, both inclusive,
 * in walking order (steps[0] is the start, steps[length-1] is the goal).
 *
 * 'length' is 0 (and 'steps' is NULL) when no path exists.
 *
 * Always release with path_free().
 */
typedef struct path_result path_result;
struct path_result
{
	path_point *steps;
	int length;
};

/*
 * Walkability callback used by astar_find_path_cb().
 *
 * Return TRUE if the tile at (y, x) may be entered, FALSE otherwise.
 * (y, x) is always inside [0, height) x [0, width). 'user' is the opaque
 * pointer handed to astar_find_path_cb().
 */
typedef bool (*astar_walkable_hook)(int y, int x, void *user);

/* Movement model: orthogonal only, or orthogonal + diagonals. */
#define ASTAR_4DIR 0
#define ASTAR_8DIR 1

/*
 * Find the optimal path on a 'height' x 'width' grid described by 'walkable',
 * a flat row-major matrix of 'height * width' bytes where a non-zero value
 * means the tile can be entered.
 *
 * Returns the path from (sy, sx) to (gy, gx), or an empty result if no path
 * exists (including when the start or goal tile is itself blocked).
 *
 * 'diagonals' is ASTAR_4DIR or ASTAR_8DIR.
 */
extern path_result *astar_find_path(int height, int width, const byte *walkable,
                                    int sy, int sx, int gy, int gx,
                                    int diagonals);

/*
 * Same as astar_find_path(), but walkability is queried through 'hook' instead
 * of a pre-built matrix. Useful to path over cave[][] directly.
 */
extern path_result *astar_find_path_cb(int height, int width,
                                       astar_walkable_hook hook, void *user,
                                       int sy, int sx, int gy, int gx,
                                       int diagonals);

/* Release a path returned by the functions above (NULL is safe). */
extern void path_free(path_result *path);

#endif /* INCLUDED_PATHFIND_H */
