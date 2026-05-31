/* File: pathfind.c */

/*
 * Generic A* (A-star) pathfinding. See pathfind.h for the public interface.
 *
 * This module is intentionally self-contained: it only uses the base types
 * (byte/bool/s16b) and the C standard library, so it links cleanly into every
 * frontend without dragging in the game globals.
 *
 * Implementation notes:
 *   - The open set is a binary min-heap keyed on the f-score (g + heuristic).
 *   - "Lazy deletion": a cell may be pushed onto the heap more than once; when
 *     popped, a cell already in the closed set is simply skipped. This avoids
 *     a decrease-key operation at the cost of some extra heap entries.
 *   - Costs are integers: 10 per orthogonal step, 14 per diagonal step (a fixed
 *     point approximation of 1 and sqrt(2)). The heuristic is Manhattan (4-dir)
 *     or octile (8-dir) using the same scale, so it never overestimates and the
 *     path returned is optimal.
 *   - Diagonal moves do not "cut corners": both orthogonally adjacent tiles
 *     must also be walkable, which keeps paths from slipping between walls.
 */

#include "pathfind.h"

#include <stdlib.h>

/* Step costs (fixed point: 10 == 1.0). */
#define COST_ORTHO 10
#define COST_DIAG 14

/* Per-cell exploration state. */
#define ST_UNSEEN 0
#define ST_OPEN 1
#define ST_CLOSED 2

/* One entry in the open-set heap: the cell index and the f-score it was
 * pushed with (snapshotted so heap ordering is stable under lazy deletion). */
typedef struct heap_node heap_node;
struct heap_node
{
	int cell;
	int f;
};

/* All scratch state for a single search, freed before returning. */
typedef struct astar_ctx astar_ctx;
struct astar_ctx
{
	int height;
	int width;
	int diagonals;

	astar_walkable_hook hook;
	void *user;

	int *g;        /* best known cost from start to each cell, or -1   */
	int *parent;   /* predecessor cell index on the best path, or -1   */
	byte *state;   /* ST_UNSEEN / ST_OPEN / ST_CLOSED per cell         */

	heap_node *heap;
	int heap_len;
	int heap_cap;
};

/*
 * Min-heap helpers (ordered by node.f, ascending).
 */

static bool heap_push(astar_ctx *c, int cell, int f)
{
	int i;

	if (c->heap_len >= c->heap_cap)
	{
		int new_cap = (c->heap_cap > 0) ? (c->heap_cap * 2) : 64;
		heap_node *grown = (heap_node *)realloc(c->heap,
		        (size_t)new_cap * sizeof(heap_node));
		if (!grown) return (FALSE);
		c->heap = grown;
		c->heap_cap = new_cap;
	}

	/* Append, then sift up. */
	i = c->heap_len++;
	c->heap[i].cell = cell;
	c->heap[i].f = f;

	while (i > 0)
	{
		int parent = (i - 1) / 2;
		if (c->heap[parent].f <= c->heap[i].f) break;
		{
			heap_node tmp = c->heap[parent];
			c->heap[parent] = c->heap[i];
			c->heap[i] = tmp;
		}
		i = parent;
	}

	return (TRUE);
}

/* Pop the minimum-f cell index; returns -1 when the heap is empty. */
static int heap_pop(astar_ctx *c)
{
	int best;
	int i;

	if (c->heap_len <= 0) return (-1);

	best = c->heap[0].cell;

	/* Move the last node to the root, then sift down. */
	c->heap_len--;
	if (c->heap_len > 0)
	{
		c->heap[0] = c->heap[c->heap_len];

		i = 0;
		while (TRUE)
		{
			int left = 2 * i + 1;
			int right = 2 * i + 2;
			int small = i;

			if (left < c->heap_len && c->heap[left].f < c->heap[small].f)
				small = left;
			if (right < c->heap_len && c->heap[right].f < c->heap[small].f)
				small = right;
			if (small == i) break;

			{
				heap_node tmp = c->heap[small];
				c->heap[small] = c->heap[i];
				c->heap[i] = tmp;
			}
			i = small;
		}
	}

	return (best);
}

/* Walkability of an in-bounds cell, via the search's hook. */
static bool walkable(astar_ctx *c, int y, int x)
{
	return (c->hook(y, x, c->user));
}

/* Admissible heuristic from (y, x) to the goal (gy, gx). */
static int heuristic(astar_ctx *c, int y, int x, int gy, int gx)
{
	int dy = (y > gy) ? (y - gy) : (gy - y);
	int dx = (x > gx) ? (x - gx) : (gx - x);

	if (c->diagonals != ASTAR_4DIR)
	{
		/* Octile distance. */
		int lo = (dy < dx) ? dy : dx;
		int hi = (dy < dx) ? dx : dy;
		return (COST_DIAG * lo + COST_ORTHO * (hi - lo));
	}

	/* Manhattan distance. */
	return (COST_ORTHO * (dy + dx));
}

/* Reconstruct the start..goal path by walking the parent chain backwards. */
static path_result *build_path(astar_ctx *c, int start, int goal)
{
	path_result *result;
	int count;
	int cur;
	int i;

	/* Count the steps first so we can allocate exactly. */
	count = 0;
	for (cur = goal; cur != -1; cur = c->parent[cur]) count++;

	result = (path_result *)malloc(sizeof(path_result));
	if (!result) return (NULL);

	result->length = count;
	result->steps = (path_point *)malloc((size_t)count * sizeof(path_point));
	if (!result->steps)
	{
		free(result);
		return (NULL);
	}

	/* Fill from the end so the array reads start -> goal. */
	cur = goal;
	for (i = count - 1; i >= 0; i--)
	{
		result->steps[i].y = (s16b)(cur / c->width);
		result->steps[i].x = (s16b)(cur % c->width);
		cur = c->parent[cur];
	}

	/* 'start' is implicitly the first element; silence unused warnings. */
	(void)start;

	return (result);
}

path_result *astar_find_path_cb(int height, int width,
                                astar_walkable_hook hook, void *user,
                                int sy, int sx, int gy, int gx,
                                int diagonals)
{
	astar_ctx c;
	path_result *result = NULL;
	int n;
	int start, goal;
	int i;

	/* Neighbour offsets: first 4 orthogonal, last 4 diagonal. */
	static const int ny[8] = { -1, 1, 0, 0, -1, -1, 1, 1 };
	static const int nx[8] = { 0, 0, -1, 1, -1, 1, -1, 1 };

	/* Reject degenerate grids and out-of-bounds endpoints. */
	if (height <= 0 || width <= 0 || !hook) return (NULL);
	if (sy < 0 || sy >= height || sx < 0 || sx >= width) return (NULL);
	if (gy < 0 || gy >= height || gx < 0 || gx >= width) return (NULL);

	c.height = height;
	c.width = width;
	c.diagonals = diagonals;
	c.hook = hook;
	c.user = user;
	c.heap = NULL;
	c.heap_len = 0;
	c.heap_cap = 0;

	n = height * width;
	c.g = (int *)malloc((size_t)n * sizeof(int));
	c.parent = (int *)malloc((size_t)n * sizeof(int));
	c.state = (byte *)calloc((size_t)n, sizeof(byte));
	if (!c.g || !c.parent || !c.state)
	{
		free(c.g);
		free(c.parent);
		free(c.state);
		return (NULL);
	}

	for (i = 0; i < n; i++)
	{
		c.g[i] = -1;
		c.parent[i] = -1;
	}

	start = sy * width + sx;
	goal = gy * width + gx;

	/* Both endpoints must be enterable. */
	if (!walkable(&c, sy, sx) || !walkable(&c, gy, gx))
	{
		free(c.g);
		free(c.parent);
		free(c.state);
		return (NULL);
	}

	c.g[start] = 0;
	c.state[start] = ST_OPEN;
	heap_push(&c, start, heuristic(&c, sy, sx, gy, gx));

	while (TRUE)
	{
		int cur = heap_pop(&c);
		int cy, cx;
		int dirs;
		int d;

		if (cur < 0) break;                 /* open set exhausted: no path */
		if (c.state[cur] == ST_CLOSED) continue;   /* stale heap entry     */

		if (cur == goal)
		{
			result = build_path(&c, start, goal);
			break;
		}

		c.state[cur] = ST_CLOSED;
		cy = cur / width;
		cx = cur % width;

		dirs = (diagonals != ASTAR_4DIR) ? 8 : 4;
		for (d = 0; d < dirs; d++)
		{
			int ty = cy + ny[d];
			int tx = cx + nx[d];
			int tcell;
			int step;
			int tentative;

			if (ty < 0 || ty >= height || tx < 0 || tx >= width) continue;
			if (!walkable(&c, ty, tx)) continue;

			/* Forbid diagonal moves that would cut a wall corner, unless the
			 * caller allows corner cutting (ASTAR_8DIR_CUT). */
			if (d >= 4 && diagonals == ASTAR_8DIR)
			{
				if (!walkable(&c, cy, tx)) continue;
				if (!walkable(&c, ty, cx)) continue;
			}

			tcell = ty * width + tx;
			if (c.state[tcell] == ST_CLOSED) continue;

			step = (d >= 4) ? COST_DIAG : COST_ORTHO;
			tentative = c.g[cur] + step;

			/* Relax only on a strictly better cost. */
			if (c.g[tcell] >= 0 && tentative >= c.g[tcell]) continue;

			c.g[tcell] = tentative;
			c.parent[tcell] = cur;
			c.state[tcell] = ST_OPEN;
			heap_push(&c, tcell, tentative + heuristic(&c, ty, tx, gy, gx));
		}
	}

	free(c.heap);
	free(c.g);
	free(c.parent);
	free(c.state);

	return (result);
}

/*
 * Matrix-backed walkability: the 'user' pointer is the byte matrix, and a
 * non-zero entry means the tile is walkable. (width is captured below.)
 */
typedef struct matrix_view matrix_view;
struct matrix_view
{
	const byte *walkable;
	int width;
};

static bool matrix_hook(int y, int x, void *user)
{
	matrix_view *mv = (matrix_view *)user;
	return (mv->walkable[y * mv->width + x] != 0);
}

path_result *astar_find_path(int height, int width, const byte *walkable,
                             int sy, int sx, int gy, int gx, int diagonals)
{
	matrix_view mv;

	if (!walkable) return (NULL);

	mv.walkable = walkable;
	mv.width = width;

	return (astar_find_path_cb(height, width, matrix_hook, &mv,
	                           sy, sx, gy, gx, diagonals));
}

void path_free(path_result *path)
{
	if (!path) return;
	free(path->steps);
	free(path);
}
