/* File: main-gtk.c */

/*
 * Copyright (c) 2000-2001 Robert Ruehlmann,
 * Steven Fuerst, Uwe Siems, "pelpel", et al.
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.
 */

/*
 * Robert Ruehlmann wrote the original Gtk port. Since an initial work is
 * much harder than enhancements, his effort worth more credits than
 * others.
 *
 * Steven Fuerst implemented colour-depth independent X server support,
 * graphics, resizing and big screen support for ZAngband as well as
 * fast image rescaling that is included here.
 *
 * Uwe Siems wrote smooth tiles rescaling code (on by default).
 * Try this with 8x8 tiles. They *will* look different.
 *
 * "pelpel" wrote another colour-depth independent X support
 * using GdkRGB, added several hooks and callbacks for various
 * reasons, wrote no-backing store mode (off by default),
 * added GtkItemFactory based menu system, introduced
 * USE_GRAPHICS code bloat (^ ^;), added comments (I have
 * a strange habit of writing comments while I code...)
 * and reorganised the file a bit.
 */

#include "angband.h"


/*
 * Activate variant-specific features
 *
 * Angband 2.9.3 and close variants don't require any.
 *
 * Angband 2.9.4 alpha and later removed the short-lived
 * can_save flag, so please #define can_save TRUE, or remove
 * all the references to it. They also changed long-lived
 * z-virt macro names. Find C_FREE/C_KILL and replace them
 * with FREE/KILL, which takes one pointer parameter.
 *
 * [Z]-based variants (Gum and Cth, for example) usually need
 * ANG293_COMPAT, ANG291_COMPAT and ANG281_RESET_VISUALS.
 *
 * [O] needs ANG293_COMPAT and ZANG_SAVE_GAME.
 *
 * ZAngband has its own enhanced main-gtk.c as mentioned above, and
 * you *should* use it :-)
 *
 * ANG291_COMPAT does not include Angband 2.9.x's gamma correction code.
 * If you like to use SUPPORT_GAMMA, copy the code bracketed
 * inside of #ifdef SUPPORT_GAMMA in util.c of Angband 2.9.1 or greater.
 */
#define TOME

#ifdef TOME
# define ANG293_COMPAT	/* Requires V2.9.3 compatibility code */
# define ANG291_COMPAT	/* Requires V2.9.1 compatibility code */
# define ANG281_RESET_VISUALS	/* The old style reset_visuals() */
# define INTERACTIVE_GAMMA	/* Supports interactive gamma correction */
# define SAVEFILE_SCREEN	/* New/Open integrated into the game */
# define USE_DOUBLE_TILES	/* Mogami's bigtile patch */
#endif /* TOME */

/*
 * Some examples
 */
#ifdef ANGBAND300
# define can_save TRUE	/* Mimick the short-lived flag */
# define C_FREE(P, N, T)	FREE(P)	/* Emulate the long-lived macro */
# define USE_TRANSPARENCY	/* Because it's default now */
#endif /* ANGBAND300 */

#ifdef GUMBAND
# define ANG293_COMPAT	/* Requires V2.9.3 compatibility code */
# define ANG291_COMPAT	/* Requires V2.9.1 compatibility code */
# define ANG281_RESET_VISUALS	/* The old style reset_visuals() */
# define OLD_SAVEFILE_CODE /* See also SAVEFILE_MUTABLE in files.c */
# define NO_REDRAW_SECTION	/* Doesn't have Term_redraw_section() */
#endif /* GUMBAND */

#ifdef OANGBAND
# define ANG293_COMPAT	/* Requires V2.9.3 compatibility code */
# define ZANG_SAVE_GAME	/* do_cmd_save_game with auto_save parameter */
#endif /* OANGBAND */


#ifdef USE_GTK2

/* Force ANSI standard */
/* #define __STRICT_ANSI__ */

/* No GCC-specific includes */
/* #undef __GNUC__ */

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

/* /me pffts Solaris */
#ifndef NAME_MAX
#define	NAME_MAX	_POSIX_NAME_MAX
#endif


/*
 * Include some helpful X11 code.
 */
#ifndef ANG293_COMPAT
# include "maid-x11.h"
#endif /* !ANG293_COMPAT */


/*
 * Number of pixels inserted between the menu bar and the main screen
 */
#define NO_PADDING 0


/*
 * Largest possible number of terminal windows supported by the game
 */
#define MAX_TERM_DATA 8


/*
 * Extra data to associate with each "window"
 *
 * Each "window" is represented by a "term_data" structure, which
 * contains a "term" structure, which contains a pointer (t->data)
 * back to the term_data structure.
 */

#ifdef USE_GRAPHICS

/*
 * Since GdkRGB doesn't provide us some useful functions...
 */
typedef struct GdkRGBImage GdkRGBImage;

struct GdkRGBImage
{
	gint width;
	gint height;
	gint ref_count;
	guchar *image;
};

#endif /* USE_GRAPHICS */


/*
 * This structure holds everything you need to manipulate terminals
 */
typedef struct term_data term_data;

struct term_data
{
	term t;

	GtkWidget *window;
	GtkWidget *drawing_area;
	GdkPixmap *backing_store;
	GdkFont *font;
	GdkGC *gc;

	bool shown;
	byte last_attr;

	int font_wid;
	int font_hgt;

	int rows;
	int cols;

#ifdef USE_GRAPHICS

	int tile_wid;
	int tile_hgt;

	GdkRGBImage *tiles;
# ifdef USE_TRANSPARENCY
	guint32 bg_pixel;
	GdkRGBImage *trans_buf;
# endif  /* USE_TRANSPARENCY */

#endif /* USE_GRAPHICS */

	cptr name;
};


/*
 * Where to draw when we call Gdk drawing primitives
 */
# define TERM_DATA_DRAWABLE(td) \
((td)->backing_store ? (td)->backing_store : (td)->drawing_area->window)

# define TERM_DATA_REFRESH(td, x, y, wid, hgt) \
if ((td)->backing_store) gdk_draw_pixmap( \
(td)->drawing_area->window, \
(td)->gc, \
(td)->backing_store, \
(x) * (td)->font_wid, \
(y) * (td)->font_hgt, \
(x) * (td)->font_wid, \
(y) * (td)->font_hgt, \
(wid) * (td)->font_wid, \
(hgt) * (td)->font_hgt)


#if 0

/* Compile time option version */

# ifdef USE_BACKING_STORE

# define TERM_DATA_DRAWABLE(td) (td)->backing_store

# define TERM_DATA_REFRESH(td, x, y, wid, hgt) \
gdk_draw_pixmap( \
(td)->drawing_area->window, \
(td)->gc, \
(td)->backing_store, \
(x) * (td)->font_wid, \
(y) * (td)->font_hgt, \
(x) * (td)->font_wid, \
(y) * (td)->font_hgt, \
(wid) * (td)->font_wid, \
(hgt) * (td)->font_hgt)

# else /* USE_BACKING_STORE */

# define TERM_DATA_DRAWABLE(td) (td)->drawing_area->window
# define TERM_DATA_REFRESH(td, x, y, wid, hgt)

# endif  /* USE_BACKING_STORE */

#endif /* 0 */


/*
 * An array of "term_data" structures, one for each "sub-window"
 */
static term_data data[MAX_TERM_DATA];

/* --- TomeTik: modo isométrico (renderer nuevo, ver src/iso/iso_render.{h,c}) --- */
#include "iso/iso_render.h"
static bool iso_mode = FALSE;        /* TRUE solo en GRAF_MODE_ISO (lo fija init_graphics) */
extern bool iso_in_store;            /* store.c: TRUE en pantalla de tienda */

/* --- TomeTik: tooltip de casilla al pasar el ratón (ver motion handler) --- */
static GtkWidget *tooltip_win = NULL;    /* popup borderless que sigue al ratón */
static GtkWidget *tooltip_label = NULL;
static int tooltip_cy = -1;              /* última celda descrita (para no repetir) */
static int tooltip_cx = -1;
static guint tooltip_timer = 0;          /* g_timeout pendiente (0 = ninguno) */
static char tooltip_pending[256];        /* texto a mostrar cuando salte el timer */
static gint tooltip_px = 0, tooltip_py = 0;  /* posición (raíz) donde mostrarlo */
#define TOOLTIP_DELAY_MS 250             /* espera antes de mostrar el tooltip */

/* Celda del cave resaltada bajo el ratón en modo iso (-1 = ninguna). */
static int iso_hover_y = -1, iso_hover_x = -1;
static GdkPixbuf *iso_sheet = NULL;  /* dg_iso32.gif (14x15 tiles 54x49, cian transp.) */
/* Fase 4: lámina Gervais 2D 32x32 (lib/xtra/graf/32x32.bmp) para actores
 * (jugador/monstruos/objetos). map_info da (a,c); tile = fila a&0x7F, col c&0x7F.
 * Fondo (24,24,24) -> transparente. */
static GdkPixbuf *gerv_sheet = NULL;
static int gerv_cols = 0, gerv_rows = 0;

/* TomeTik: tiles extra de Dungeon Odyssey (iso, 54x54, magenta #FF00FF transp.) para
 * features sin equivalente en dg_iso32 (fuente, altares, fuego, pit, trampa, pools).
 * Sheet ensamblada en lib/xtra/iso/do_extra.png (7 cols). Se blitean con offset -5
 * en Y para alinear el rombo de suelo (DO 54px vs dg_iso32 49px). Ver iso_do_tile(). */
static GdkPixbuf *do_sheet = NULL;
/* TomeTik: sprite especial (PNG con alfa, 54x49) para los edificios del pueblo;
 * más alto que el cubo 70 y con la tapa tintada. lib/xtra/iso/building_block.png.
 * Si falta, se cae al cubo de piedra normal (ISO_T_SINGLE). */
static GdkPixbuf *bldg_block = NULL;
/* TomeTik: tile custom de hierba con flores (FEAT_FLOWER), 54x49, cian transp.
 * lib/xtra/iso/grass_flowers.png. Si falta, FEAT_FLOWER cae al suelo de hierba (0). */
static GdkPixbuf *flower_tile = NULL;
/* TomeTik: tile custom de escombros (FEAT_RUBBLE 49 y 206 "pile of rubble"),
 * 54x54, magenta #FF00FF transp., overlay sobre el suelo. lib/xtra/iso/rubble.png. */
static GdkPixbuf *rubble_tile = NULL;
#define DO_TILE_W   54
#define DO_TILE_H   54
#define DO_COLS      7
#define DO_DY       (-5)        /* offset vertical para casar el rombo de suelo */
/* Cuánto bajar el sprite del actor (jugador/monstruo/objeto) respecto al centro
 * del rombo, para que "toque el suelo" en vez de flotar. */
#define ISO_ACTOR_DROP  12
enum {
	DO_FOUNTAIN = 0, DO_PIT, DO_FIRE, DO_TRAP, DO_MONTRAP,
	DO_ALTAR_BEING, DO_ALTAR_WINDS, DO_ALTAR_FORCE, DO_ALTAR_DARK, DO_ALTAR_NATURE,
	DO_NETHER, DO_MIRKY, DO_WATER, DO_EMBERS,
	DO_GRAVEYARD, DO_DARKWATER, DO_GRAVE_POOF, DO_DARKWATER_CORRUPT,
	DO_TUNNEL, DO_PORTAL, DO_FLOORSTONE, DO_TOWN, DO_GLYPH_GREEN, DO_GLYPH_RED
};

/*
 * TomeTik: layout por defecto de las ventanas, pensado para la pantalla
 * ~1280x800 del contenedor noVNC. Posición (x,y) en píxeles, tamaño en celdas
 * (cols x rows) y fuente X. Ventana 0 = mapa principal (fuente grande 10x20
 * para que los tiles 32x32 se vean grandes); 1 = Mirror (inventario), 2 =
 * Recall (monstruo/objeto), 3 = Choice (mensajes). 4-7 quedan en cascada.
 * Usado por term_data_init (tamaño), get_default_font (fuente) y
 * init_gtk_window (posición).
 */
static const struct
{
	int x, y, cols, rows;
	cptr font;
} tometik_layout[MAX_TERM_DATA] =
{
	{   0,   0, 80, 24, "10x20" },  /* 0 mapa principal + tiles */
	{ 806,   0, 56, 26, "8x13"  },  /* 1 Mirror = inventario     */
	{ 806, 366, 56, 26, "8x13"  },  /* 2 Recall = recall mon/obj */
	{   0, 512, 99, 10, "8x13"  },  /* 3 Choice = mensajes       */
	{  60,  60, 80, 24, "8x13"  },  /* 4-7 extra (cascada)       */
	{  90,  90, 80, 24, "8x13"  },
	{ 120, 120, 80, 24, "8x13"  },
	{ 150, 150, 80, 24, "8x13"  },
};

/*
 * Number of active terms. TomeTik: 4 por defecto (mapa + Mirror/Recall/Choice)
 * para que la UI multiventana esté lista al arrancar. Se puede cambiar con -n.
 */
static int num_term = 4;


/*
 * RGB values of the sixteen Angband colours
 */
static guint32 angband_colours[16];


/*
 * Set to TRUE when a game is in progress
 */
static bool game_in_progress = FALSE;


/*
 * This is in some cases used for double buffering as well as
 * a backing store, speeding things up under client-server
 * configurations, while turning this off *might* work better
 * with the MIT Shm extention which is usually active if you run
 * Angband locally, because it reduces amount of memory-to-memory copy.
 */
static bool use_backing_store = TRUE;




/**** Vanilla compatibility functions ****/

#ifdef ANG293_COMPAT

/*
 * Look up some environment variables to find font name for each window.
 */
static cptr get_default_font(int term)
{
	char buf[64];
	cptr font_name;

	/* Window specific font name */
	strnfmt(buf, 64, "ANGBAND_X11_FONT_%s", angband_term_name[term]);

	/* Check environment for that font */
	font_name = getenv(buf);

	/* Window specific font name */
	strnfmt(buf, 64, "ANGBAND_X11_FONT_%d", term);

	/* Check environment for that font */
	if (!font_name) font_name = getenv(buf);

	/* Check environment for "base" font */
	if (!font_name) font_name = getenv("ANGBAND_X11_FONT");

	/* No environment variables, use the per-window default layout font */
	if (!font_name) font_name = tometik_layout[term].font;

	return (font_name);
}


# ifndef SAVEFILE_SCREEN

/*
 * In [V]2.9.3, this frees all dynamically allocated memory
 */
static void cleanup_angband(void)
{
	/* XXX XXX XXX */
}

# endif  /* !SAVEFILE_SCREEN */

/*
 * New global flag to indicate if it's safe to save now
 */
#define can_save TRUE

#endif /* ANG293_COMPAT */


#ifdef ANG291_COMPAT

/*
 * The standard game uses this to implement lighting effects
 * for 16x16 tiles in cave.c...
 *
 * Because of the way it is implemented in X11 ports,
 * we can set this to TRUE even if we are using the 8x8 tileset.
 */
static bool use_transparency = TRUE;

#endif /* ANG291_COMPAT */




/**** Low level routines - memory allocation ****/

/*
 * Hook to "release" memory
 */
#ifdef ANGBAND300
static vptr hook_rnfree(vptr v)
#else
static vptr hook_rnfree(vptr v, huge size)
#endif /* ANGBAND300 */
{
	/* Dispose */
	g_free(v);

	/* Success */
	return (NULL);
}


/*
 * Hook to "allocate" memory
 */
static vptr hook_ralloc(huge size)
{
	/* Make a new pointer */
	return (g_malloc(size));
}



/**** Low level routines - colours and graphics ****/

#ifdef SUPPORT_GAMMA

/*
 * When set to TRUE, indicates that we can use gamma_table
 */
static bool gamma_table_ready = FALSE;


# ifdef INTERACTIVE_GAMMA

/*
 * Initialise the gamma-correction table for current gamma_val
 * - interactive version
 */
static void setup_gamma_table(void)
{
	static u16b old_gamma_val = 0;

	/* Don't have to rebuild the table */
	if (gamma_val == old_gamma_val) return;

	/* Temporarily inactivate the table */
	gamma_table_ready = FALSE;

	/* Validate gamma_val */
	if ((gamma_val <= 0) || (gamma_val >= 256))
	{
		/* Reset */
		old_gamma_val = gamma_val = 0;

		/* Leave it inactive */
		return;
	}

	/* (Re)build the gamma table */
	build_gamma_table(gamma_val);

	/* Remember the gamma value used */
	old_gamma_val = gamma_val;

	/* Activate the table */
	gamma_table_ready = TRUE;
}

# else /* INTERACTIVE_GAMMA */

/*
 * Initialise the gamma-correction table if environment variable
 * ANGBAND_X11_GAMMA is set and contains a meaningful value
 *
 * Restored for cross-variant compatibility
 */
static void setup_gamma_table(void)
{
	cptr tmp;
	int gamma_val;


	/* The table's already set up */
	if (gamma_table_ready) return;

	/*
	 * XXX XXX It's documented nowhere, but ANGBAND_X11_GAMMA is
	 * 256 * (1 / gamma), rounded to integer. A recommended value
	 * is 183, which is an approximation of the Macintosh hardware
	 * gamma of 1.4.
	 *
	 *   gamma	ANGBAND_X11_GAMMA
	 *   -----	-----------------
	 *   1.2	213
	 *   1.25	205
	 *   1.3	197
	 *   1.35	190
	 *   1.4	183
	 *   1.45	177
	 *   1.5	171
	 *   1.6	160
	 *   1.7	151
	 *   ...
	 *
	 * XXX XXX The environment variable, or better,
	 * the interact with colours command should allow users
	 * to specify gamma values (or gamma value * 100).
	 */
	tmp = getenv("ANGBAND_X11_GAMMA");

	/* Nothing to do */
	if (tmp == NULL) return;

	/* Extract the value */
	gamma_val = atoi(tmp);

	/*
	 * Only need to build the table if gamma exists and set to
	 * a meaningful value.
	 *
	 * XXX It may be a good idea to prevent use of very high gamma values,
	 * say, greater than 2.5, which is gamma of normal CRT display IIRC.
	 */
	if ((gamma_val <= 0) || (gamma_val >= 256)) return;

	/* Build the gamma correction table */
	build_gamma_table(gamma_val);

	/* The table is properly set up */
	gamma_table_ready = TRUE;
}

# endif  /* INTERACTIVE_GAMMA */

#endif /* SUPPORT_GAMMA */


/*
 * Remeber RGB values for sixteen Angband colours, in a format
 * that is convinient for GdkRGB GC functions.
 *
 * XXX XXX Duplication of maid-x11.c is far from the Angband
 * ideal of code cleanliness, but the whole point of using GdkRGB
 * is to let it handle colour allocation which it does in a very
 * clever fashion. Ditto for the tile scaling code and the BMP loader
 * below.
 */
static void init_colours(void)
{
	int i;


#ifdef SUPPORT_GAMMA

	/* (Re)build gamma table if necessary */
	setup_gamma_table();

#endif /* SUPPORT_GAMMA */

	/* Process each colour */
	for (i = 0; i < 16; i++)
	{
		u32b red, green, blue;

		/* Retrieve RGB values from the game */
		red = angband_color_table[i][1];
		green = angband_color_table[i][2];
		blue = angband_color_table[i][3];

#ifdef SUPPORT_GAMMA

		/* Hack -- Gamma Correction */
		if (gamma_table_ready)
		{
			red = gamma_table[red];
			green = gamma_table[green];
			blue = gamma_table[blue];
		}

#endif /* SUPPORT_GAMMA */

		/* Remember a GdkRGB value, that is 0xRRGGBB */
		angband_colours[i] = (red << 16) | (green << 8) | blue;
	}
}


/*
 * Set foreground colour of window td to attr, only when it is necessary
 */
static void term_data_set_fg(term_data *td, byte attr)
{
	/* We can use the current gc */
	if (td->last_attr == attr) return;

	/* Activate the colour */
	gdk_rgb_gc_set_foreground(td->gc, angband_colours[attr]);

	/* Remember it */
	td->last_attr = attr;
}


#ifdef USE_GRAPHICS

/*
 * Graphics mode selector - current setting and requested value
 */
#define GRAF_MODE_NONE	0
#define GRAF_MODE_OLD	1
#define GRAF_MODE_NEW	2
#define GRAF_MODE_ISO	3   /* TomeTik: isométrico (Gervais 2D + overlay iso en data[0]) */

static int graf_mode = GRAF_MODE_NONE;
/* TomeTik: arrancar con tiles Gervais activados (GRAF_MODE_NEW = 32x32 Gervais).
 * Se puede volver a texto en Options -> Graphics -> None. */
static int graf_mode_request = GRAF_MODE_NEW;

/*
 * Use smooth rescaling?
 */
static bool smooth_rescaling = TRUE;
static bool smooth_rescaling_request = TRUE;

/*
 * Dithering
 */
static GdkRgbDither dith_mode = GDK_RGB_DITHER_NORMAL;

/*
 * Need to reload and resize tiles when fonts are changed.
 */
static bool resize_request = FALSE;

/*
 * Numbers of columns and rows in current tileset
 * calculated and set by the tile loading code in graf_init()
 * and used by Term_pict_gtk()
 */
static int tile_rows;
static int tile_cols;


/*
 * Directory name(s)
 */
static cptr ANGBAND_DIR_XTRA_GRAF;


/*
 * Be nice to old graphics hardwares -- using GdkRGB.
 *
 * We don't have colour allocation failure any longer this way,
 * even with 8bpp X servers. Gimp *does* work with 8bpp, why not Angband?
 *
 * Initialisation (before any widgets are created)
 *	gdk_rgb_init();
 *	gtk_widget_set_default_colormap (gdk_rgb_get_cmap());
 *	gtk_widget_set_default_visual (gdk_rgb_get_visual());
 *
 * Setting fg/bg colours
 *	void gdk_rgb_gc_set_foreground(GdkGC *gc, guint32 rgb);
 *	void gdk_rgb_gc_set_background(GdkGC *gc, guint32 rgb);
 * where rgb is 0xRRGGBB.
 *
 * Drawing rgb images
 *	void gdk_draw_rgb_image(
 *		GdkDrawable *drawable,
 *		GdkGC *gc,
 *		gint x, gint y,
 *		gint width, gint height,
 *		GdkRgbDither dith,
 *		guchar *rgb_buf,
 *		gint rowstride);
 *
 * dith:
 *	GDK_RGB_DITHER_NORMAL : dither if 8bpp or below
 *	GDK_RGB_DITHER_MAX : dither if 16bpp or below.
 *
 * for 0 <= i < width and 0 <= j < height,
 * the pixel (x + i, y + j) is colored with
 *  red value rgb_buf[j * rowstride + i * 3],
 *  green value rgb_buf[j * rowstride + i * 3 + 1], and
 *  blue value rgb_buf[j * rowstride + i * 3 + 2].
 */

/*
 * gdk_image compatibility functions - should be part of gdk, IMHO.
 */

/*
 * Create GdkRGBImage of width * height and return pointer
 * to it. Returns NULL on failure
 */
static GdkRGBImage *gdk_rgb_image_new(
        gint width,
        gint height)
{
	GdkRGBImage *result;

	/* Allocate a struct */
	result = g_new(GdkRGBImage, 1);

	/* Oops */
	if (result == NULL) return (NULL);

	/* Allocate buffer */
	result->image = g_new0(guchar, width * height * 3);

	/* Oops */
	if (result->image == NULL)
	{
		g_free(result);
		return (NULL);
	}

	/* Initialise size fields */
	result->width = width;
	result->height = height;

	/* Initialise reference count */
	result->ref_count = 1;

	/* Success */
	return (result);
}

/*
 * Free a GdkRGBImage
 */
static void gdk_rgb_image_destroy(
        GdkRGBImage *im)
{
	/* Paranoia */
	if (im == NULL) return;

	/* Free the RGB buffer */
	g_free(im->image);

	/* Free the structure */
	g_free(im);
}


#if 0

/*
 * Unref a GdkRGBImage
 */
static void gdk_rgb_image_unref(
        GdkRGBImage *im)
{
	/* Paranoia */
	g_return_if_fail(im != NULL);

	/* Decrease reference count by 1 */
	im->ref_count--;

	/* Free if nobody's using it */
	if (im->ref_count <= 0) gdk_rgb_image_destroy(im);
}


/*
 * Reference a GdkRGBImage
 */
static void gdk_rgb_image_ref(
        GdkRGBImage *im)
{
	/* Paranoia */
	g_return_if_fail(im != NULL);

	/* Increase reference count by 1 */
	im->ref_count++;
}

#endif /* 0 */


/*
 * Write RGB pixel of the format 0xRRGGBB to (x, y) in GdkRGBImage
 */
static void gdk_rgb_image_put_pixel(
        GdkRGBImage *im,
        gint x,
        gint y,
        guint32 pixel)
{
	guchar *rgbp;

	/* Paranoia */
	g_return_if_fail(im != NULL);

	/* Paranoia */
	if ((x < 0) || (x >= im->width)) return;

	/* Paranoia */
	if ((y < 0) || (y >= im->height)) return;

	/* Access RGB data */
	rgbp = &im->image[(y * im->width * 3) + (x * 3)];

	/* Red */
	*rgbp++ = (pixel >> 16) & 0xFF;
	/* Green */
	*rgbp++ = (pixel >> 8) & 0xFF;
	/* Blue */
	*rgbp = pixel & 0xFF;
}


/*
 * Returns RGB pixel (0xRRGGBB) at (x, y) in GdkRGBImage
 */
static guint32 gdk_rgb_image_get_pixel(
        GdkRGBImage *im,
        gint x,
        gint y)
{
	guchar *rgbp;

	/* Paranoia */
	if (im == NULL) return (0);

	/* Paranoia - returns black */
	if ((x < 0) || (x >= im->width)) return (0);

	/* Paranoia */
	if ((y < 0) || (y >= im->height)) return (0);

	/* Access RGB data */
	rgbp = &im->image[(y * im->width * 3) + (x * 3)];

	/* Return result */
	return ((rgbp[0] << 16) | (rgbp[1] << 8) | (rgbp[2]));
}


/*
 * Since gdk_draw_rgb_image is a bit harder to use than it's
 * GdkImage counterpart, I wrote a grue function that takes
 * exactly the same parameters as gdk_draw_image, with
 * the GdkImage parameter replaced with GdkRGBImage.
 */
static void gdk_draw_rgb_image_2(
        GdkDrawable *drawable,
        GdkGC *gc,
        GdkRGBImage *image,
        gint xsrc,
        gint ysrc,
        gint xdest,
        gint ydest,
        gint width,
        gint height)
{
	/* Paranoia */
	g_return_if_fail(drawable != NULL);
	g_return_if_fail(image != NULL);

	/* Paranoia */
	if (xsrc < 0 || (xsrc + width - 1) >= image->width) return;
	if (ysrc < 0 || (ysrc + height - 1) >= image->height) return;

	/* Draw the image at (xdest, ydest), with dithering if bpp <= 8/16 */
	gdk_draw_rgb_image(
	        drawable,
	        gc,
	        xdest,
	        ydest,
	        width,
	        height,
	        dith_mode,
	        &image->image[(ysrc * image->width * 3) + (xsrc * 3)],
	        image->width * 3);
}


/*
 * Code for smooth icon rescaling from Uwe Siems, Jan 2000
 *
 * XXX XXX Duplication of maid-x11.c, again. It doesn't do any colour
 * allocation, either.
 */

/*
 * to save ourselves some labour, define a maximum expected icon width here:
 */
#define MAX_ICON_WIDTH 32


/*
 * Each pixel is kept in this structure during smooth rescaling
 * calculations, to make things a bit easier
 */
typedef struct rgb_type rgb_type;

struct rgb_type
{
	guint32 red;
	guint32 green;
	guint32 blue;
};

/*
 * Because there are many occurences of this, and because
 * it's logical to do so...
 */
#define pixel_to_rgb(pix, rgb_buf) \
(rgb_buf)->red   = ((pix) >> 16) & 0xFF; \
(rgb_buf)->green = ((pix) >> 8)  & 0xFF; \
(rgb_buf)->blue  = (pix) & 0xFF


/*
 * get_scaled_row reads a scan from the given GdkRGBImage, scales it smoothly
 * and returns the red, green and blue values in arrays.
 * The values in this arrays must be divided by a certain value that is
 * calculated in scale_icon.
 * x, y is the position, iw is the input width and ow the output width
 * scan must be sufficiently sized
 */
static void get_scaled_row(
        GdkRGBImage *im,
        int x,
        int y,
        int iw,
        int ow,
        rgb_type *scan)
{
	int xi, si, sifrac, ci, cifrac, add_whole, add_frac;
	guint32 pix;
	rgb_type prev;
	rgb_type next;
	bool get_next_pix;

	/* Unscaled */
	if (iw == ow)
	{
		for (xi = 0; xi < ow; xi++)
		{
			pix = gdk_rgb_image_get_pixel(im, x + xi, y);
			pixel_to_rgb(pix, &scan[xi]);
		}
	}

	/* Scaling by subsampling (grow) */
	else if (iw < ow)
	{
		iw--;
		ow--;

		/* read first pixel: */
		pix = gdk_rgb_image_get_pixel(im, x, y);
		pixel_to_rgb(pix, &next);
		prev = next;

		/* si and sifrac give the subsampling position: */
		si = x;
		sifrac = 0;

		/* get_next_pix tells us, that we need the next pixel */
		get_next_pix = TRUE;

		for (xi = 0; xi <= ow; xi++)
		{
			if (get_next_pix)
			{
				prev = next;
				if (xi < ow)
				{
					/* only get next pixel if in same icon */
					pix = gdk_rgb_image_get_pixel(im, si + 1, y);
					pixel_to_rgb(pix, &next);
				}
			}

			/* calculate subsampled color values: */
			/* division by ow occurs in scale_icon */
			scan[xi].red = prev.red * (ow - sifrac) + next.red * sifrac;
			scan[xi].green = prev.green * (ow - sifrac) + next.green * sifrac;
			scan[xi].blue = prev.blue * (ow - sifrac) + next.blue * sifrac;

			/* advance sampling position: */
			sifrac += iw;
			if (sifrac >= ow)
			{
				si++;
				sifrac -= ow;
				get_next_pix = TRUE;
			}
			else
			{
				get_next_pix = FALSE;
			}

		}
	}

	/* Scaling by averaging (shrink) */
	else
	{
		/* width of an output pixel in input pixels: */
		add_whole = iw / ow;
		add_frac = iw % ow;

		/* start position of the first output pixel: */
		si = x;
		sifrac = 0;

		/* get first input pixel: */
		pix = gdk_rgb_image_get_pixel(im, x, y);
		pixel_to_rgb(pix, &next);

		for (xi = 0; xi < ow; xi++)
		{
			/* find endpoint of the current output pixel: */
			ci = si + add_whole;
			cifrac = sifrac + add_frac;
			if (cifrac >= ow)
			{
				ci++;
				cifrac -= ow;
			}

			/* take fraction of current input pixel (starting segment): */
			scan[xi].red = next.red * (ow - sifrac);
			scan[xi].green = next.green * (ow - sifrac);
			scan[xi].blue = next.blue * (ow - sifrac);
			si++;

			/* add values for whole pixels: */
			while (si < ci)
			{
				rgb_type tmp_rgb;

				pix = gdk_rgb_image_get_pixel(im, si, y);
				pixel_to_rgb(pix, &tmp_rgb);
				scan[xi].red += tmp_rgb.red * ow;
				scan[xi].green += tmp_rgb.green * ow;
				scan[xi].blue += tmp_rgb.blue * ow;
				si++;
			}

			/* add fraction of current input pixel (ending segment): */
			if (xi < ow - 1)
			{
				/* only get next pixel if still in icon: */
				pix = gdk_rgb_image_get_pixel(im, si, y);
				pixel_to_rgb(pix, &next);
			}

			sifrac = cifrac;
			if (sifrac > 0)
			{
				scan[xi].red += next.red * sifrac;
				scan[xi].green += next.green * sifrac;
				scan[xi].blue += next.blue * sifrac;
			}
		}
	}
}


/*
 * put_rgb_scan takes arrays for red, green and blue and writes pixel values
 * according to this values in the GdkRGBImage-structure. w is the number of
 * pixels to write and div is the value by which all red/green/blue values
 * are divided first.
 */
static void put_rgb_scan(
        GdkRGBImage *im,
        int x,
        int y,
        int w,
        int div,
        rgb_type *scan)
{
	int xi;
	guint32 pix;
	guint32 adj = div / 2;

	for (xi = 0; xi < w; xi++)
	{
		byte r, g, b;

		/* un-factor the RGB values */
		r = (scan[xi].red + adj) / div;
		g = (scan[xi].green + adj) / div;
		b = (scan[xi].blue + adj) / div;

#ifdef SUPPORT_GAMMA

		/* Apply gamma correction if requested and available */
		if (gamma_table_ready)
		{
			r = gamma_table[r];
			g = gamma_table[g];
			b = gamma_table[b];
		}

#endif /* SUPPORT_GAMMA */

		/* Make a (virtual) 24-bit pixel */
		pix = (r << 16) | (g << 8) | (b);

		/* Draw it into image */
		gdk_rgb_image_put_pixel(im, x + xi, y, pix);
	}
}


/*
 * scale_icon transfers an area from GdkRGBImage im_in, locate (x1,y1) to
 * im_out, locate (x2, y2). Source size is (ix, iy) and destination size
 * is (ox, oy).
 *
 * It does this by getting icon scan line from get_scaled_scan and handling
 * them the same way as pixels are handled in get_scaled_scan.
 * This even allows icons to be scaled differently in horizontal and
 * vertical directions (eg. shrink horizontal, grow vertical).
 */
static void scale_icon(
        GdkRGBImage *im_in,
        GdkRGBImage *im_out,
        int x1,
        int y1,
        int x2,
        int y2,
        int ix,
        int iy,
        int ox,
        int oy)
{
	int div;
	int xi, yi, si, sifrac, ci, cifrac, add_whole, add_frac;

	/* buffers for pixel rows: */
	rgb_type prev[MAX_ICON_WIDTH];
	rgb_type next[MAX_ICON_WIDTH];
	rgb_type temp[MAX_ICON_WIDTH];

	bool get_next_row;

	/* get divider value for the horizontal scaling: */
	if (ix == ox)
		div = 1;
	else if (ix < ox)
		div = ox - 1;
	else
		div = ix;

	/* no scaling needed vertically: */
	if (iy == oy)
	{
		for (yi = 0; yi < oy; yi++)
		{
			get_scaled_row(im_in, x1, y1 + yi, ix, ox, temp);
			put_rgb_scan(im_out, x2, y2 + yi, ox, div, temp);
		}
	}

	/* scaling by subsampling (grow): */
	else if (iy < oy)
	{
		iy--;
		oy--;
		div *= oy;

		/* get first row: */
		get_scaled_row(im_in, x1, y1, ix, ox, next);

		/* si and sifrac give the subsampling position: */
		si = y1;
		sifrac = 0;

		/* get_next_row tells us, that we need the next row */
		get_next_row = TRUE;
		for (yi = 0; yi <= oy; yi++)
		{
			if (get_next_row)
			{
				for (xi = 0; xi < ox; xi++)
				{
					prev[xi] = next[xi];
				}
				if (yi < oy)
				{
					/* only get next row if in same icon */
					get_scaled_row(im_in, x1, si + 1, ix, ox, next);
				}
			}

			/* calculate subsampled color values: */
			/* division by oy occurs in put_rgb_scan */
			for (xi = 0; xi < ox; xi++)
			{
				temp[xi].red = (prev[xi].red * (oy - sifrac) +
				                next[xi].red * sifrac);
				temp[xi].green = (prev[xi].green * (oy - sifrac) +
				                  next[xi].green * sifrac);
				temp[xi].blue = (prev[xi].blue * (oy - sifrac) +
				                 next[xi].blue * sifrac);
			}

			/* write row to output image: */
			put_rgb_scan(im_out, x2, y2 + yi, ox, div, temp);

			/* advance sampling position: */
			sifrac += iy;
			if (sifrac >= oy)
			{
				si++;
				sifrac -= oy;
				get_next_row = TRUE;
			}
			else
			{
				get_next_row = FALSE;
			}

		}
	}

	/* scaling by averaging (shrink) */
	else
	{
		div *= iy;

		/* height of a output row in input rows: */
		add_whole = iy / oy;
		add_frac = iy % oy;

		/* start position of the first output row: */
		si = y1;
		sifrac = 0;

		/* get first input row: */
		get_scaled_row(im_in, x1, y1, ix, ox, next);
		for (yi = 0; yi < oy; yi++)
		{
			/* find endpoint of the current output row: */
			ci = si + add_whole;
			cifrac = sifrac + add_frac;
			if (cifrac >= oy)
			{
				ci++;
				cifrac -= oy;
			}

			/* take fraction of current input row (starting segment): */
			for (xi = 0; xi < ox; xi++)
			{
				temp[xi].red = next[xi].red * (oy - sifrac);
				temp[xi].green = next[xi].green * (oy - sifrac);
				temp[xi].blue = next[xi].blue * (oy - sifrac);
			}
			si++;

			/* add values for whole pixels: */
			while (si < ci)
			{
				get_scaled_row(im_in, x1, si, ix, ox, next);
				for (xi = 0; xi < ox; xi++)
				{
					temp[xi].red += next[xi].red * oy;
					temp[xi].green += next[xi].green * oy;
					temp[xi].blue += next[xi].blue * oy;
				}
				si++;
			}

			/* add fraction of current input row (ending segment): */
			if (yi < oy - 1)
			{
				/* only get next row if still in icon: */
				get_scaled_row(im_in, x1, si, ix, ox, next);
			}
			sifrac = cifrac;
			for (xi = 0; xi < ox; xi++)
			{
				temp[xi].red += next[xi].red * sifrac;
				temp[xi].green += next[xi].green * sifrac;
				temp[xi].blue += next[xi].blue * sifrac;
			}

			/* write row to output image: */
			put_rgb_scan(im_out, x2, y2 + yi, ox, div, temp);
		}
	}
}


/*
 * Rescale icons using sort of anti-aliasing technique.
 */
static GdkRGBImage *resize_tiles_smooth(
        GdkRGBImage *im,
        int ix,
        int iy,
        int ox,
        int oy)
{
	int width1, height1, width2, height2;
	int x1, x2, y1, y2;

	GdkRGBImage *tmp;

	/* Original size */
	width1 = im->width;
	height1 = im->height;

	/* Rescaled size */
	width2 = ox * width1 / ix;
	height2 = oy * height1 / iy;

	/* Allocate GdkRGBImage for resized tiles */
	tmp = gdk_rgb_image_new(width2, height2);

	/* Oops */
	if (tmp == NULL) return (NULL);

	/* Scale each icon */
	for (y1 = 0, y2 = 0; (y1 < height1) && (y2 < height2); y1 += iy, y2 += oy)
	{
		for (x1 = 0, x2 = 0; (x1 < width1) && (x2 < width2); x1 += ix, x2 += ox)
		{
			scale_icon(im, tmp, x1, y1, x2, y2,
			           ix, iy, ox, oy);
		}
	}

	return tmp;
}


/*
 * Steven Fuerst's tile resizing code
 * Taken from Z because I think the algorithm is cool.
 */

/* 24-bit version - GdkRGB uses 24 bit RGB data internally */
static void copy_pixels(
        int wid,
        int y,
        int offset,
        int *xoffsets,
        GdkRGBImage *old_image,
        GdkRGBImage *new_image)
{
	int i;

	/* Get source and destination */
	byte *src = &old_image->image[offset * old_image->width * 3];
	byte *dst = &new_image->image[y * new_image->width * 3];

	/* Copy to the image */
	for (i = 0; i < wid; i++)
	{
#ifdef SUPPORT_GAMMA

		if (gamma_table_ready)
		{
			*dst++ = gamma_table[src[3 * xoffsets[i]]];
			*dst++ = gamma_table[src[3 * xoffsets[i] + 1]];
			*dst++ = gamma_table[src[3 * xoffsets[i] + 2]];

			continue;
		}

#endif /* SUPPORT_GAMMA */

		*dst++ = src[3 * xoffsets[i]];
		*dst++ = src[3 * xoffsets[i] + 1];
		*dst++ = src[3 * xoffsets[i] + 2];
	}
}


#if 0

/* 32-bit version: it might be useful in the future */
static void copy_pixels(
        int wid,
        int y,
        int offset,
        int *xoffsets,
        GdkRGBImage *old_image,
        GdkRGBImage *new_image)
{
	int i;

	/* Get source and destination */
	byte *src = &old_image->image[offset * old_image->width * 4];
	byte *dst = &new_image->image[y * new_image->width * 4];

	/* Copy to the image */
	for (i = 0; i < wid; i++)
	{
		*dst++ = src[4 * xoffsets[i]];
		*dst++ = src[4 * xoffsets[i] + 1];
		*dst++ = src[4 * xoffsets[i] + 2];
		*dst++ = src[4 * xoffsets[i] + 3];
	}
}

#endif


/*
 * Resize ix * iy pixel tiles in old to ox * oy pixels
 * and return a new GdkRGBImage containing the resized tiles
 */
static GdkRGBImage *resize_tiles_fast(
        GdkRGBImage *old_image,
        int ix,
        int iy,
        int ox,
        int oy)
{
	GdkRGBImage *new_image;

	int old_wid, old_hgt;

	int new_wid, new_hgt;

	int add, remainder, rem_tot, offset;

	int *xoffsets;

	int i;


	/* Get the size of the old image */
	old_wid = old_image->width;
	old_hgt = old_image->height;

	/* Calculate the size of the new image */
	new_wid = (old_wid / ix) * ox;
	new_hgt = (old_hgt / iy) * oy;

	/* Allocate a GdkRGBImage to store resized tiles */
	new_image = gdk_rgb_image_new(new_wid, new_hgt);

	/* Paranoia */
	if (new_image == NULL) return (NULL);

	/* now begins the cool part of SF's code */

	/*
	 * Calculate an offsets table, so the transformation
	 * is faster.  This is much like the Bresenham algorithm
	 */

	/* Set up x offset table */
	C_MAKE(xoffsets, new_wid, int);

	/* Initialize line parameters */
	add = old_wid / new_wid;
	remainder = old_wid % new_wid;

	/* Start at left */
	offset = 0;

	/* Half-tile offset so 'line' is centered correctly */
	rem_tot = new_wid / 2;

	for (i = 0; i < new_wid; i++)
	{
		/* Store into the table */
		xoffsets[i] = offset;

		/* Move to next entry */
		offset += add;

		/* Take care of fractional part */
		rem_tot += remainder;
		if (rem_tot >= new_wid)
		{
			rem_tot -= new_wid;
			offset++;
		}
	}

	/* Scan each row */

	/* Initialize line parameters */
	add = old_hgt / new_hgt;
	remainder = old_hgt % new_hgt;

	/* Start at left */
	offset = 0;

	/* Half-tile offset so 'line' is centered correctly */
	rem_tot = new_hgt / 2;

	for (i = 0; i < new_hgt; i++)
	{
		/* Copy pixels to new image */
		copy_pixels(new_wid, i, offset, xoffsets, old_image, new_image);

		/* Move to next entry */
		offset += add;

		/* Take care of fractional part */
		rem_tot += remainder;
		if (rem_tot >= new_hgt)
		{
			rem_tot -= new_hgt;
			offset++;
		}
	}

	/* Free offset table */
	C_FREE(xoffsets, new_wid, int);

	return (new_image);
}


/*
 * Resize an image of ix * iy pixels and return a newly allocated
 * image of ox * oy pixels.
 */
static GdkRGBImage *resize_tiles(
        GdkRGBImage *im,
        int ix,
        int iy,
        int ox,
        int oy)
{
	GdkRGBImage *result;

	/*
	 * I hope we can always use this with GdkRGB, which uses a 5x5x5
	 * colour cube (125 colours) by default, and resort to dithering
	 * when it can't find good match there or expand the cube, so it
	 * works with 8bpp X servers.
	 */
	if (smooth_rescaling_request && (ix != ox || iy != oy))
	{
		result = resize_tiles_smooth(im, ix, iy, ox, oy);
	}

	/*
	 * Unless smoothing is requested by user, we use the fast
	 * resizing code.
	 */
	else
	{
		result = resize_tiles_fast(im, ix, iy, ox, oy);
	}

	/* Return rescaled tiles, or NULL */
	return (result);
}


/*
 * Tile loaders - XPM and BMP
 */

/*
 * A helper function for the XPM loader
 *
 * Read next string delimited by double quotes from
 * the input stream. Return TRUE on success, FALSE
 * if it finds EOF or buffer overflow.
 *
 * I never mean this to be generic, so its EOF and buffer
 * overflow behaviour is terribly stupid -- there are no
 * provisions for recovery.
 *
 * CAVEAT: treatment of backslash is not compatible with the standard
 * C usage XXX XXX XXX XXX
 */
static bool read_str(char *buf, u32b len, FILE *f)
{
	int c;

	/* Paranoia - Buffer too small */
	if (len <= 0) return (FALSE);

	/* Find " */
	while ((c = getc(f)) != '"')
	{
		/* Premature EOF */
		if (c == EOF) return (FALSE);
	}

	while (1)
	{
		/* Read next char */
		c = getc(f);

		/* Premature EOF */
		if (c == EOF) return (FALSE);

		/* Terminating " */
		if (c == '"') break;

		/* Escape */
		if (c == '\\')
		{
			/* Use next char */
			c = getc(f);

			/* Premature EOF */
			if (c == EOF) return (FALSE);
		}

		/* Store character in the buffer */
		*buf++ = c;

		/* Decrement count */
		len--;

		/* Buffer full - we have to place a NULL at the end */
		if (len <= 0) return (FALSE);
	}

	/* Make a C string if there's room left */
	if (len > 0) *buf = '\0';

	/* Success */
	return (TRUE);
}


/*
 * Remember pixel symbol to RGB colour mappings
 */

/*
 * I've forgot the formula, but I remember prime number yields
 * good results
 */
#define HASH_SIZE 19

typedef struct pal_type pal_type;

struct pal_type
{
	u32b str;
	u32b rgb;
	pal_type *next;
};


/*
 * A simple, slow and stupid XPM loader
 */
static GdkRGBImage *load_xpm(cptr filename)
{
	FILE *f;
	GdkRGBImage *img = NULL;
	int width, height, colours, chars;
	int i, j, k;
	bool ret;
	pal_type *pal = NULL;
	pal_type *head[HASH_SIZE];
	u32b buflen = 0;
	char *lin = NULL;
	char buf[1024];

	/* Build path to the XPM file */
	path_build(buf, 1024, ANGBAND_DIR_XTRA_GRAF, filename);

	/* Open it */
	f = my_fopen(buf, "r");

	/* Oops */
	if (f == NULL) return (NULL);

	/* Read header */
	ret = read_str(buf, 1024, f);

	/* Oops */
	if (!ret)
	{
		/* Notify error */
		plog("Cannot find XPM header");

		/* Failure */
		goto oops;
	}

	/* Parse header */
	if (4 != sscanf(buf, "%d %d %d %d", &width, &height, &colours, &chars))
	{
		/* Notify error */
		plog("Bad XPM header");

		/* Failure */
		goto oops;
	}

	/*
	 * Paranoia - the code can handle upto four letters per pixel,
	 * but such large number of colours certainly requires a smarter
	 * symbol-to-colour mapping algorithm...
	 */
	if ((width <= 0) || (height <= 0) || (colours <= 0) || (chars <= 0) ||
	                (chars > 2))
	{
		/* Notify error */
		plog("Invalid width/height/depth");

		/* Failure */
		goto oops;
	}

	/* Allocate palette */
	C_MAKE(pal, colours, pal_type);

	/* Initialise hash table */
	for (i = 0; i < HASH_SIZE; i++) head[i] = NULL;

	/* Parse palette */
	for (i = 0; i < colours; i++)
	{
		u32b tmp;
		int h_idx;

		/* Read next string */
		ret = read_str(buf, 1024, f);

		/* Check I/O result */
		if (!ret)
		{
			/* Notify error */
			plog("EOF in palette");

			/* Failure */
			goto oops;
		}

		/* Clear symbol code */
		tmp = 0;

		/* Encode pixel symbol */
		for (j = 0; j < chars; j++)
		{
			tmp = (tmp << 8) | (buf[j] & 0xFF);
		}

		/* Remember it */
		pal[i].str = tmp;

		/* Skip spaces */
		while ((buf[j] == ' ') || (buf[j] == '\t')) j++;

		/* Verify 'c' */
		if (buf[j] != 'c')
		{
			/* Notify error */
			plog("No 'c' in palette definition");

			/* Failure */
			goto oops;
		}

		/* Advance cursor */
		j++;

		/* Skip spaces */
		while ((buf[j] == ' ') || (buf[j] == '\t')) j++;

		/* Hack - Assume 'None' */
		if (buf[j] == 'N')
		{
			/* Angband always uses black background */
			pal[i].rgb = 0x000000;
		}

		/* Read colour */
		else if ((1 != sscanf(&buf[j], "#%06lX", &tmp)) &&
		                (1 != sscanf(&buf[j], "#%06lx", &tmp)))
		{
			/* Notify error */
			plog("Badly formatted colour");

			/* Failure */
			goto oops;
		}

		/* Remember it */
		pal[i].rgb = tmp;

		/* Store it in hash table as well */
		h_idx = pal[i].str % HASH_SIZE;

		/* Link the entry */
		pal[i].next = head[h_idx];
		head[h_idx] = &pal[i];
	}

	/* Allocate image */
	img = gdk_rgb_image_new(width, height);

	/* Oops */
	if (img == NULL)
	{
		/* Notify error */
		plog("Cannot allocate image");

		/* Failure */
		goto oops;
	}

	/* Calculate buffer length */
	buflen = width * chars + 1;

	/* Allocate line buffer */
	C_MAKE(lin, buflen, char);

	/* For each row */
	for (i = 0; i < height; i++)
	{
		/* Read a row of image data */
		ret = read_str(lin, buflen, f);

		/* Oops */
		if (!ret)
		{
			/* Notify error */
			plog("EOF in middle of image data");

			/* Failure */
			goto oops;
		}

		/* For each column */
		for (j = 0; j < width; j++)
		{
			u32b tmp;
			pal_type *h_ptr;

			/* Clear encoded pixel */
			tmp = 0;

			/* Encode pixel symbol */
			for (k = 0; k < chars; k++)
			{
				tmp = (tmp << 8) | (lin[j * chars + k] & 0xFF);
			}

			/* Find colour */
			for (h_ptr = head[tmp % HASH_SIZE];
			                h_ptr != NULL;
			                h_ptr = h_ptr->next)
			{
				/* Found a match */
				if (h_ptr->str == tmp) break;
			}

			/* No match found */
			if (h_ptr == NULL)
			{
				/* Notify error */
				plog("Invalid pixel symbol");

				/* Failure */
				goto oops;
			}

			/* Draw it */
			gdk_rgb_image_put_pixel(
			        img,
			        j,
			        i,
			        h_ptr->rgb);
		}
	}

	/* Close file */
	my_fclose(f);

	/* Free line buffer */
	C_FREE(lin, buflen, char);

	/* Free palette */
	C_FREE(pal, colours, pal_type);

	/* Return result */
	return (img);

oops:

	/* Close file */
	my_fclose(f);

	/* Free image */
	if (img) gdk_rgb_image_destroy(img);

	/* Free line buffer */
	if (lin) C_FREE(lin, buflen, char);

	/* Free palette */
	if (pal) C_FREE(pal, colours, pal_type);

	/* Failure */
	return (NULL);
}


/*
 * A BMP loader, yet another duplication of maid-x11.c functions.
 *
 * Another duplication, again because of different image format and
 * avoidance of colour allocation.
 *
 * XXX XXX XXX XXX Should avoid using a propriatary and closed format.
 * Since it's much bigger than gif that was used before, why don't
 * we switch to XPM?  NetHack does.  Well, NH has always been much
 * closer to the GNU/Un*x camp and it's GPL'ed quite early...
 *
 * The names and naming convention are worse than the worst I've ever
 * seen, so I deliberately changed them to fit well with the rest of
 * the code. Or are they what xx calls them? If it's the case, there's
 * no reason to follow *their* words.
 */

/*
 * BMP file header
 */
typedef struct bmp_file_type bmp_file_type;

struct bmp_file_type
{
	u16b type;
	u32b size;
	u16b reserved1;
	u16b reserved2;
	u32b offset;
};


/*
 * BMP file information fields
 */
typedef struct bmp_info_type bmp_info_type;

struct bmp_info_type
{
	u32b size;
	u32b width;
	u32b height;
	u16b planes;
	u16b bit_count;
	u32b compression;
	u32b size_image;
	u32b x_pels_per_meter;
	u32b y_pels_per_meter;
	u32b colors_used;
	u32b color_importand;
};

/*
 * "RGBQUAD" type.
 */
typedef struct rgb_quad_type rgb_quad_type;

struct rgb_quad_type
{
	unsigned char b, g, r;
	unsigned char filler;
};


/*** Helper functions for system independent file loading. ***/

static byte get_byte(FILE *fff)
{
	/* Get a character, and return it */
	return (getc(fff) & 0xFF);
}

static void rd_byte(FILE *fff, byte *ip)
{
	*ip = get_byte(fff);
}

static void rd_u16b(FILE *fff, u16b *ip)
{
	(*ip) = get_byte(fff);
	(*ip) |= ((u16b)(get_byte(fff)) << 8);
}

static void rd_u32b(FILE *fff, u32b *ip)
{
	(*ip) = get_byte(fff);
	(*ip) |= ((u32b)(get_byte(fff)) << 8);
	(*ip) |= ((u32b)(get_byte(fff)) << 16);
	(*ip) |= ((u32b)(get_byte(fff)) << 24);
}


/*
 * Read a BMP file (a certain trademark nuked)
 *
 * This function replaces the old ReadRaw and RemapColors functions.
 *
 * Assumes that the bitmap has a size such that no padding is needed in
 * various places.  Currently only handles bitmaps with 3 to 256 colors.
 */
GdkRGBImage *load_bmp(cptr filename)
{
	FILE *f;

	char path[1024];

	bmp_file_type file_hdr;
	bmp_info_type info_hdr;

	GdkRGBImage *result = NULL;

	int ncol;

	int i;

	u32b x, y;

	guint32 colour_pixels[256];


	/* Build the path to the bmp file */
	path_build(path, 1024, ANGBAND_DIR_XTRA_GRAF, filename);

	/* Open the BMP file */
	f = fopen(path, "r");

	/* No such file */
	if (f == NULL)
	{
		return (NULL);
	}

	/* Read the "bmp_file_type" */
	rd_u16b(f, &file_hdr.type);
	rd_u32b(f, &file_hdr.size);
	rd_u16b(f, &file_hdr.reserved1);
	rd_u16b(f, &file_hdr.reserved2);
	rd_u32b(f, &file_hdr.offset);

	/* Read the "bmp_info_type" */
	rd_u32b(f, &info_hdr.size);
	rd_u32b(f, &info_hdr.width);
	rd_u32b(f, &info_hdr.height);
	rd_u16b(f, &info_hdr.planes);
	rd_u16b(f, &info_hdr.bit_count);
	rd_u32b(f, &info_hdr.compression);
	rd_u32b(f, &info_hdr.size_image);
	rd_u32b(f, &info_hdr.x_pels_per_meter);
	rd_u32b(f, &info_hdr.y_pels_per_meter);
	rd_u32b(f, &info_hdr.colors_used);
	rd_u32b(f, &info_hdr.color_importand);

	/* Verify the header */
	if (feof(f) ||
	                (file_hdr.type != 19778) ||
	                (info_hdr.size != 40))
	{
		plog(format("Incorrect BMP file format %s", filename));
		fclose(f);
		return (NULL);
	}

	/*
	 * The two headers above occupy 54 bytes total
	 * The "offset" field says where the data starts
	 * The "colors_used" field does not seem to be reliable
	 */

	/* Compute number of colors recorded */
	ncol = (file_hdr.offset - 54) / 4;

	for (i = 0; i < ncol; i++)
	{
		rgb_quad_type clr;

		/* Read an "rgb_quad_type" */
		rd_byte(f, &clr.b);
		rd_byte(f, &clr.g);
		rd_byte(f, &clr.r);
		rd_byte(f, &clr.filler);

		/* Remember the pixel */
		colour_pixels[i] = (clr.r << 16) | (clr.g << 8) | (clr.b);
	}

	/* Allocate GdkRGBImage large enough to store the image */
	result = gdk_rgb_image_new(info_hdr.width, info_hdr.height);

	/* Failure */
	if (result == NULL)
	{
		fclose(f);
		return (NULL);
	}

	for (y = 0; y < info_hdr.height; y++)
	{
		u32b y2 = info_hdr.height - y - 1;

		for (x = 0; x < info_hdr.width; x++)
		{
			int ch = getc(f);

			/* Verify not at end of file XXX XXX */
			if (feof(f))
			{
				plog(format("Unexpected end of file in %s", filename));
				gdk_rgb_image_destroy(result);
				fclose(f);
				return (NULL);
			}

			if (info_hdr.bit_count == 24)
			{
				int c3, c2 = getc(f);

				/* Verify not at end of file XXX XXX */
				if (feof(f))
				{
					plog(format("Unexpected end of file in %s", filename));
					gdk_rgb_image_destroy(result);
					fclose(f);
					return (NULL);
				}

				c3 = getc(f);

				/* Verify not at end of file XXX XXX */
				if (feof(f))
				{
					plog(format("Unexpected end of file in %s", filename));
					gdk_rgb_image_destroy(result);
					fclose(f);
					return (NULL);
				}

				/* Draw the pixel */
				gdk_rgb_image_put_pixel(
				        result,
				        x,
				        y2,
				        (ch << 16) | (c2 << 8) | (c3));
			}
			else if (info_hdr.bit_count == 8)
			{
				gdk_rgb_image_put_pixel(result, x, y2, colour_pixels[ch]);
			}
			else if (info_hdr.bit_count == 4)
			{
				gdk_rgb_image_put_pixel(result, x, y2, colour_pixels[ch / 16]);
				x++;
				gdk_rgb_image_put_pixel(result, x, y2, colour_pixels[ch % 16]);
			}
			else
			{
				/* Technically 1 bit is legal too */
				plog(format("Illegal bit count %d in %s",
				            info_hdr.bit_count, filename));
				gdk_rgb_image_destroy(result);
				fclose(f);
				return (NULL);
			}
		}
	}

	fclose(f);

	return result;
}


/*
 * Try to load an XPM file, or a BMP file if it fails
 *
 * Choice of file format may better be made yet another option XXX
 */
static GdkRGBImage *load_tiles(cptr basename)
{
	char buf[32];
	GdkRGBImage *img;

	/* build xpm file name */
	strnfmt(buf, 32, "%s.xpm", basename);

	/* Try to load it */
	img = load_xpm(buf);

	/* OK */
	if (img) return (img);

	/* Try again for a bmp file */
	strnfmt(buf, 32, "%s.bmp", basename);

	/* Try loading it */
	img = load_bmp(buf);

	/* Return result, success or failure */
	return (img);
}


/*
 * Free all tiles and graphics buffers associated with windows
 *
 * This is conspirator of graf_init() below, sharing its inefficiency
 */
static void graf_nuke()
{
	int i;

	term_data *td;


	/* Nuke all terms */
	for (i = 0; i < MAX_TERM_DATA; i++)
	{
		/* Access term_data structure */
		td = &data[i];

		/* Disable graphics */
		td->t.higher_pict = FALSE;

		/* Free previously allocated tiles */
		if (td->tiles) gdk_rgb_image_destroy(td->tiles);

		/* Forget pointer */
		td->tiles = NULL;

# ifdef USE_TRANSPARENCY

		/* Free previously allocated transparency buffer */
		if (td->trans_buf) gdk_rgb_image_destroy(td->trans_buf);

		/* Forget stale pointer */
		td->trans_buf = NULL;

# endif  /* USE_TRANSPARENCY */

	}
}


/*
 * Load tiles, scale them to current font size, and store a pointer
 * to them in a term_data structure for each term.
 *
 * XXX XXX XXX This is a terribly stupid quick hack.
 *
 * XXX XXX XXX Windows using the same font should share resized tiles
 */
static bool graf_init(
        cptr filename,
        int tile_wid,
        int tile_hgt)
{
	term_data *td;

	bool result;

	GdkRGBImage *raw_tiles, *scaled_tiles;

# ifdef USE_TRANSPARENCY
	GdkRGBImage *buffer;
# endif  /* USE_TRANSPARENCY */

	int i;


	/* Paranoia */
	if (filename == NULL) return (FALSE);

	/* Load tiles */
	raw_tiles = load_tiles(filename);

	/* Oops */
	if (raw_tiles == NULL)
	{
		/* Clean up */
		graf_nuke();

		/* Failure */
		return (FALSE);
	}

	/* Calculate and remember numbers of rows and columns */
	tile_rows = raw_tiles->height / tile_hgt;
	tile_cols = raw_tiles->width / tile_wid;

	/* Be optimistic */
	result = TRUE;


	/*
	 * (Re-)init each term
	 * XXX It might help speeding this up to avoid doing so if a window
	 * doesn't need graphics (e.g. inventory/equipment and message recall).
	 */
	for (i = 0; i < MAX_TERM_DATA; i++)
	{
		/* Access term_data */
		td = &data[i];

		/* Shouldn't waste anything for unused terms */
		if (!td->shown) continue;

		/* Enable graphics */
		td->t.higher_pict = TRUE;

		/* See if we need rescaled tiles XXX */
		if ((td->tiles == NULL) ||
		                (td->tiles->width != td->tile_wid * tile_cols) ||
		                (td->tiles->height != td->tile_hgt * tile_rows))
		{
			/* Free old tiles if present */
			if (td->tiles) gdk_rgb_image_destroy(td->tiles);

			/* Forget pointer */
			td->tiles = NULL;

			/* Scale the tiles to current font bounding rect */
			scaled_tiles = resize_tiles(
			                       raw_tiles,
			                       tile_wid, tile_hgt,
			                       td->tile_wid, td->tile_hgt);

			/* Oops */
			if (scaled_tiles == NULL)
			{
				/* Failure */
				result = FALSE;

				break;
			}

			/* Store it */
			td->tiles = scaled_tiles;
		}

# ifdef USE_TRANSPARENCY

		/* See if we have to (re)allocate a new buffer XXX */
		if ((td->trans_buf == NULL) ||
		                (td->trans_buf->width != td->tile_wid) ||
		                (td->trans_buf->height != td->tile_hgt))
		{
			/* Free old buffer if present */
			if (td->trans_buf) gdk_rgb_image_destroy(td->trans_buf);

			/* Forget pointer */
			td->trans_buf = NULL;

			/* Allocate a new buffer */
			buffer = gdk_rgb_image_new(td->tile_wid, td->tile_hgt);

			/* Oops */
			if (buffer == NULL)
			{
				/* Failure */
				result = FALSE;

				break;
			}

			/* Store it */
			td->trans_buf = buffer;
		}

		/*
		 * Giga-Hack - assume top left corner of 0x86/0x80 should be
		 * in the background colour XXX XXX XXX XXX
		 */
		td->bg_pixel = gdk_rgb_image_get_pixel(
		                       raw_tiles,
		                       0,
		                       tile_hgt * 6);

# endif  /* USE_TRANSPARENCY */

	}


	/* Alas, we need to free wasted images */
	if (result == FALSE) graf_nuke();

	/* We don't need the raw image any longer */
	gdk_rgb_image_destroy(raw_tiles);

	/* Report success or failure */
	return (result);
}


/*
 * React to various changes in graphics mode settings
 *
 * It is *not* a requirement for tiles to have same pixel width and height.
 * The program can work with any conbinations of graf_wid and graf_hgt
 * (oops, they must be representable by u16b), as long as they are lesser
 * or equal to 32 if you use smooth rescaling.
 */
/*
 * TomeTik: carga (perezosa, una sola vez) las láminas del modo isométrico:
 *  - iso_sheet  = lib/xtra/iso/dg_iso32.gif (terreno iso; cian #00FFFF -> alfa)
 *  - gerv_sheet = lib/xtra/graf/32x32.bmp   (actores; negro (0,0,0) -> alfa)
 * Devuelve TRUE si iso_sheet quedó disponible (mínimo para dibujar el terreno).
 * Llamada al entrar en GRAF_MODE_ISO; segura de invocar varias veces.
 */
static bool iso_load_sheets(void)
{
	char path[1024];
	GdkPixbuf *raw;

	if (iso_sheet) return TRUE;   /* ya cargadas */

	path_build(path, 1024, ANGBAND_DIR_XTRA, "iso/dg_iso32.gif");
	raw = gdk_pixbuf_new_from_file(path, NULL);
	if (!raw)
	{
		plog_fmt("iso: no pude cargar %s; modo iso no disponible", path);
		return FALSE;
	}
	iso_sheet = gdk_pixbuf_add_alpha(raw, TRUE, 0x00, 0xFF, 0xFF);
	g_object_unref(raw);

	/* Lámina Gervais 2D para actores. El mismo fichero que usa el render 2D;
	 * NEGRO PURO (0,0,0) = color de fondo -> alfa transparente. */
	path_build(path, 1024, ANGBAND_DIR_XTRA_GRAF, "32x32.bmp");
	raw = gdk_pixbuf_new_from_file(path, NULL);
	if (raw)
	{
		gerv_sheet = gdk_pixbuf_add_alpha(raw, TRUE, 0, 0, 0);
		gerv_cols = gdk_pixbuf_get_width(gerv_sheet) / 32;
		gerv_rows = gdk_pixbuf_get_height(gerv_sheet) / 32;
		g_object_unref(raw);
	}
	else
	{
		plog_fmt("iso: no pude cargar %s; sin actores", path);
	}

	/* Tiles extra de Dungeon Odyssey (opcional; si falta, esas features caen a
	 * suelo gris como antes). Magenta #FF00FF -> alfa. */
	path_build(path, 1024, ANGBAND_DIR_XTRA, "iso/do_extra.png");
	raw = gdk_pixbuf_new_from_file(path, NULL);
	if (raw)
	{
		do_sheet = gdk_pixbuf_add_alpha(raw, TRUE, 0xFF, 0x00, 0xFF);
		g_object_unref(raw);
	}
	else
	{
		plog_fmt("iso: no pude cargar %s; features extra como suelo gris", path);
	}

	/* Sprite especial de edificio (PNG con canal alfa propio; opcional). */
	path_build(path, 1024, ANGBAND_DIR_XTRA, "iso/building_block.png");
	bldg_block = gdk_pixbuf_new_from_file(path, NULL);
	if (!bldg_block)
		plog_fmt("iso: no pude cargar %s; edificios como cubo 70", path);

	/* Tile custom de hierba con flores (cian #00FFFF -> alfa; opcional). */
	path_build(path, 1024, ANGBAND_DIR_XTRA, "iso/grass_flowers.png");
	raw = gdk_pixbuf_new_from_file(path, NULL);
	if (raw)
	{
		flower_tile = gdk_pixbuf_add_alpha(raw, TRUE, 0x00, 0xFF, 0xFF);
		g_object_unref(raw);
	}
	else
	{
		plog_fmt("iso: no pude cargar %s; FEAT_FLOWER como hierba", path);
	}

	/* Tile custom de escombros (magenta #FF00FF -> alfa; opcional). */
	path_build(path, 1024, ANGBAND_DIR_XTRA, "iso/rubble.png");
	raw = gdk_pixbuf_new_from_file(path, NULL);
	if (raw)
	{
		rubble_tile = gdk_pixbuf_add_alpha(raw, TRUE, 0xFF, 0x00, 0xFF);
		g_object_unref(raw);
	}
	else
	{
		plog_fmt("iso: no pude cargar %s; escombros como overlay 58", path);
	}

	return TRUE;
}

static void init_graphics(void)
{
	cptr tile_name;

	u16b graf_wid = 0, graf_hgt = 0;


	/* No graphics requests are made - Can't this be simpler? XXX XXX */
	if ((graf_mode_request == graf_mode) &&
	                (smooth_rescaling_request == smooth_rescaling) &&
	                !resize_request) return;

	/* Prevent further unsolicited reaction */
	resize_request = FALSE;


	/* Dispose unusable old tiles - awkward... XXX XXX */
	if ((graf_mode_request == GRAF_MODE_NONE) ||
	                (graf_mode_request != graf_mode) ||
	                (smooth_rescaling_request != smooth_rescaling)) graf_nuke();


	/* Setup parameters according to request */
	switch (graf_mode_request)
	{
		/* ASCII - no graphics whatsoever */
	default:
	case GRAF_MODE_NONE:
		{
			tile_name = NULL;
			use_graphics = arg_graphics = FALSE;

			break;
		}

		/*
		 * 8x8 tiles originally collected for the Amiga port
		 * from several contributers by Lars Haugseth, converted
		 * to 256 colours and expanded by the Z devteam
		 *
		 * Use the "old" tile assignments
		 *
		 * Dawnmist is working on it for ToME
		 */
	case GRAF_MODE_OLD:
		{
			tile_name = "8x8";
			graf_wid = graf_hgt = 8;
			ANGBAND_GRAF = "old";
			use_graphics = arg_graphics = TRUE;

			break;
		}

		/*
		 * Adam Bolt's 16x16 tiles
		 * "new" tile assignments
		 * It is updated for ToME by Andreas Koch
		 */
	/* TomeTik: el modo isométrico se apoya en los tiles Gervais 32x32: el term
	 * 2D los pinta debajo y map_info() devuelve los índices (a,c) que el overlay
	 * iso usa para los actores. Así que se configura igual que GRAF_MODE_NEW; lo
	 * único distinto (activar el repintado iso) lo decide iso_mode más abajo. */
	case GRAF_MODE_ISO:
	case GRAF_MODE_NEW:
		{
			/* TomeTik: usar el tileset 32x32 de David Gervais (lo que
			 * usaba el frontend Windows GDI), mapeado por graf-gervais.prf.
			 * El frontend GTK2 stock traía 16x16 Adam Bolt ("new"); aquí lo
			 * sustituimos por Gervais, que es la seña de identidad de TomeTik.
			 * 32 es el máximo que admite el reescalado suave de este frontend. */
			tile_name = "32x32";
			graf_wid = graf_hgt = 32;
			ANGBAND_GRAF = "gervais";
			use_graphics = arg_graphics = TRUE;

			break;
		}
	}


	/* load tiles and set them up if tiles are requested */
	if ((graf_mode_request != GRAF_MODE_NONE) &&
	                !graf_init(tile_name, graf_wid, graf_hgt))
	{
		/* Oops */
		plog("Cannot initialize graphics");

		/* reject requests */
		graf_mode_request = GRAF_MODE_NONE;
		smooth_rescaling_request = smooth_rescaling;

		/* reset graphics flags */
		use_graphics = arg_graphics = FALSE;
	}

	/* Update current graphics mode */
	graf_mode = graf_mode_request;
	smooth_rescaling = smooth_rescaling_request;

	/* TomeTik: el repintado isométrico se activa SOLO en GRAF_MODE_ISO. En el
	 * resto de modos (None/Old/New) iso_mode=FALSE -> TERM_XTRA_FRESH no pinta la
	 * escena iso y se ve el render 2D normal del term. Carga perezosa de láminas
	 * al entrar la primera vez; si fallan, se cae de vuelta a Gervais 2D (New). */
	iso_mode = (graf_mode == GRAF_MODE_ISO);
	if (iso_mode && !iso_load_sheets())
	{
		iso_mode = FALSE;
		graf_mode = graf_mode_request = GRAF_MODE_NEW;
	}

	/* Reset visuals */
#ifndef ANG281_RESET_VISUALS
	reset_visuals(TRUE);
#else
	reset_visuals();
#endif /* !ANG281_RESET_VISUALS */
}

#endif /* USE_GRAPHICS */




/**** Term package support routines ****/


/*
 * Free data used by a term
 */
static void Term_nuke_gtk(term *t)
{
	term_data *td = t->data;


	/* Free name */
	if (td->name) string_free(td->name);

	/* Forget it */
	td->name = NULL;

	/* Free font */
	if (td->font) gdk_font_unref(td->font);

	/* Forget it */
	td->font = NULL;

	/* Free backing store */
	if (td->backing_store) gdk_pixmap_unref(td->backing_store);

	/* Forget it too */
	td->backing_store = NULL;

#ifdef USE_GRAPHICS

	/* Free tiles */
	if (td->tiles) gdk_rgb_image_destroy(td->tiles);

	/* Forget pointer */
	td->tiles = NULL;

# ifdef USE_TRANSPARENCY

	/* Free transparency buffer */
	if (td->trans_buf) gdk_rgb_image_destroy(td->trans_buf);

	/* Amnesia */
	td->trans_buf = NULL;

# endif  /* USE_TRANSPARENCY */

#endif /* USE_GRAPHICS */
}


/*
 * Erase the whole term.
 */
static errr Term_clear_gtk(void)
{
	term_data *td = (term_data*)(Term->data);


	/* Don't draw to hidden windows */
	if (!td->shown) return (0);

	/* Paranoia */
	g_assert(td->drawing_area->window != 0);

	/* Clear the area */
	gdk_draw_rectangle(
	        TERM_DATA_DRAWABLE(td),
	        td->drawing_area->style->black_gc,
	        1,
	        0,
	        0,
	        td->cols * td->font_wid,
	        td->rows * td->font_hgt);

	/* Copy image from backing store if present */
	TERM_DATA_REFRESH(td, 0, 0, td->cols, td->rows);

	/* Success */
	return (0);
}


/*
 * Erase some characters.
 */
static errr Term_wipe_gtk(int x, int y, int n)
{
	term_data *td = (term_data*)(Term->data);


	/* Don't draw to hidden windows */
	if (!td->shown) return (0);

	/* Paranoia */
	g_assert(td->drawing_area->window != 0);

	/* Fill the area with the background colour */
	gdk_draw_rectangle(
	        TERM_DATA_DRAWABLE(td),
	        td->drawing_area->style->black_gc,
	        TRUE,
	        x * td->font_wid,
	        y * td->font_hgt,
	        n * td->font_wid,
	        td->font_hgt);

	/* Copy image from backing store if present */
	TERM_DATA_REFRESH(td, x, y, n, 1);

	/* Success */
	return (0);
}


/*
 * Draw some textual characters.
 */
static errr Term_text_gtk(int x, int y, int n, byte a, cptr s)
{
	term_data *td = (term_data*)(Term->data);


	/* Don't draw to hidden windows */
	if (!td->shown) return (0);

	/* Paranoia */
	g_assert(td->drawing_area->window != 0);

	/* Set foreground colour */
	term_data_set_fg(td, a);

	/* Clear the line */
	Term_wipe_gtk(x, y, n);

	/* Draw the text to the window */
	gdk_draw_text(
	        TERM_DATA_DRAWABLE(td),
	        td->font,
	        td->gc,
	        x * td->font_wid,
	        td->font->ascent + y * td->font_hgt,
	        s,
	        n);

	/* Copy image from backing store if present */
	TERM_DATA_REFRESH(td, x, y, n, 1);

	/* Success */
	return (0);
}


/*
 * Draw software cursor at (x, y)
 */
static errr Term_curs_gtk(int x, int y)
{
	term_data *td = (term_data*)(Term->data);
	int cells = 1;


	/* Don't draw to hidden windows */
	if (!td->shown) return (0);

	/* Paranoia */
	g_assert(td->drawing_area->window != 0);

	/* Set foreground colour */
	term_data_set_fg(td, TERM_YELLOW);

#ifdef USE_DOUBLE_TILES

	/* Mogami's bigtile patch */

	/* Adjust it if wide tiles are requested */
	if (use_bigtile &&
	                (x + 1 < Term->wid) &&
	                (Term->old->a[y][x + 1] == 255))
	{
		cells = 2;
	}

#endif /* USE_DOUBLE_TILES */

	/* Draw the software cursor */
	gdk_draw_rectangle(
	        TERM_DATA_DRAWABLE(td),
	        td->gc,
	        FALSE,
	        x * td->font_wid,
	        y * td->font_hgt,
	        td->font_wid * cells - 1,
	        td->font_hgt - 1);

	/* Copy image from backing store if present */
	TERM_DATA_REFRESH(td, x, y, cells, 1);

	/* Success */
	return (0);
}


#ifdef USE_GRAPHICS

# ifdef USE_TRANSPARENCY

/*
 * XXX XXX Low level graphics helper
 * Draw a tile at (s_x, s_y) over one at (t_x, t_y) and store the
 * result in td->trans_buf
 *
 * XXX XXX Even if CPU's are faster than necessary these days,
 * this should be made inline. Or better, there should be an API
 * to take advantage of graphics hardware. They almost always have
 * assortment of builtin bitblt's...
 */
static void overlay_tiles_2(
        term_data *td,
        int s_x, int s_y,
        int t_x, int t_y)
{
	guint32 pix;
	int x, y;


	/* Process each row */
	for (y = 0; y < td->tile_hgt; y++)
	{
		/* Process each column */
		for (x = 0; x < td->tile_wid; x++)
		{
			/* Get an overlay pixel */
			pix = gdk_rgb_image_get_pixel(td->tiles, s_x + x, s_y + y);

			/* If it's in background color, use terrain instead */
			if (pix == td->bg_pixel)
				pix = gdk_rgb_image_get_pixel(td->tiles, t_x + x, t_y + y);

			/* Store the result in trans_buf */
			gdk_rgb_image_put_pixel(td->trans_buf, x, y, pix);
		}
	}
}


# ifdef USE_EGO_GRAPHICS

/*
 * XXX XXX Low level graphics helper
 * Draw a tile at (e_x, e_y) over one at (s_x, s_y) over another one
 * at (t_x, t_y) and store the result in td->trans_buf
 *
 * XXX XXX The same comment applies as that for the above...
 */
static void overlay_tiles_3(
        term_data *td,
        int e_x, int e_y,
        int s_x, int s_y,
        int t_x, int t_y)
{
	guint32 pix;
	int x, y;


	/* Process each row */
	for (y = 0; y < td->tile_hgt; y++)
	{
		/* Process each column */
		for (x = 0; x < td->tile_wid; x++)
		{
			/* Get an overlay pixel */
			pix = gdk_rgb_image_get_pixel(td->tiles, e_x + x, e_y + y);

			/*
			 * If it's background colour, try to use one from
			 * the second layer
			 */
			if (pix == td->bg_pixel)
				pix = gdk_rgb_image_get_pixel(td->tiles, s_x + x, s_y + y);

			/*
			 * If it's background colour again, fall back to
			 * the terrain layer
			 */
			if (pix == td->bg_pixel)
				pix = gdk_rgb_image_get_pixel(td->tiles, t_x + x, t_y + y);

			/* Store the pixel in trans_buf */
			gdk_rgb_image_put_pixel(td->trans_buf, x, y, pix);
		}
	}
}

# endif  /* USE_EGO_GRAPHICS */

# endif  /* USE_TRANSPARENCY */


/*
 * Low level graphics (Assumes valid input)
 *
 * Draw "n" tiles/characters starting at (x,y)
 */
# ifdef USE_TRANSPARENCY
# ifdef USE_EGO_GRAPHICS
static errr Term_pict_gtk(
        int x, int y, int n,
        const byte *ap, const char *cp,
        const byte *tap, const char *tcp,
        const byte *eap, const char *ecp)
# else /* USE_EGO_GRAPHICS */
static errr Term_pict_gtk(
        int x, int y, int n,
        const byte *ap, const char *cp,
        const byte *tap, const char *tcp)
# endif  /* USE_EGO_GRAPHICS */
# else /* USE_TRANSPARENCY */
static errr Term_pict_gtk(
        int x, int y, int n,
        const byte *ap, const char *cp)
# endif  /* USE_TRANSPARENCY */
{
	term_data *td = (term_data*)(Term->data);

	int i;

	int d_x, d_y;

# ifdef USE_DOUBLE_TILES

	/* Hack - remember real number of columns affected XXX XXX XXX */
	int cols;

# endif  /* USE_DOUBLE_TILES */


	/* Don't draw to hidden windows */
	if (!td->shown) return (0);

	/* Paranoia */
	g_assert(td->drawing_area->window != 0);

	/* Top left corner of the destination rect */
	d_x = x * td->font_wid;
	d_y = y * td->font_hgt;


# ifdef USE_DOUBLE_TILES

	/* Reset column counter */
	cols = 0;

# endif  /* USE_DOUBLE_TILES */

	/* Scan the input */
	for (i = 0; i < n; i++)
	{
		byte a;
		char c;
		int s_x, s_y;

# ifdef USE_TRANSPARENCY

		byte ta;
		char tc;
		int t_x, t_y;

# ifdef USE_EGO_GRAPHICS

		byte ea;
		char ec;
		int e_x = 0, e_y = 0;
		bool has_overlay;

# endif  /* USE_EGO_GRAPHICS */

# endif  /* USE_TRANSPARENCY */


		/* Grid attr/char */
		a = *ap++;
		c = *cp++;

# ifdef USE_TRANSPARENCY

		/* Terrain attr/char */
		ta = *tap++;
		tc = *tcp++;

# ifdef USE_EGO_GRAPHICS

		/* Overlay attr/char */
		ea = *eap++;
		ec = *ecp++;
		has_overlay = (ea && ec);

# endif  /* USE_EGO_GRAPHICS */

# endif  /* USE_TRANSPARENCY */

		/* Row and Col */
		s_y = (((byte)a & 0x7F) % tile_rows) * td->tile_hgt;
		s_x = (((byte)c & 0x7F) % tile_cols) * td->tile_wid;

# ifdef USE_TRANSPARENCY

		/* Terrain Row and Col */
		t_y = (((byte)ta & 0x7F) % tile_rows) * td->tile_hgt;
		t_x = (((byte)tc & 0x7F) % tile_cols) * td->tile_wid;

# ifdef USE_EGO_GRAPHICS

		/* Overlay Row and Col */
		if (has_overlay)
		{
			e_y = (((byte)ea & 0x7F) % tile_rows) * td->tile_hgt;
			e_x = (((byte)ec & 0x7F) % tile_cols) * td->tile_wid;
		}

# endif  /* USE_EGO_GRAPHICS */


# ifdef USE_DOUBLE_TILES

		/* Mogami's bigtile patch */

		/* Hack -- a filler for wide tile */
		if (use_bigtile && (a == 255))
		{
			/* Advance */
			d_x += td->font_wid;

			/* Ignore */
			continue;
		}

# endif  /* USE_DOUBLE_TILES */

		/* Optimise the common case: terrain == obj/mons */
		if (!use_transparency ||
		                ((s_x == t_x) && (s_y == t_y)))
		{

# ifdef USE_EGO_GRAPHICS

			/* The simplest possible case - no overlay */
			if (!has_overlay)
			{
				/* Draw the tile */
				gdk_draw_rgb_image_2(
				        TERM_DATA_DRAWABLE(td), td->gc, td->tiles,
				        s_x, s_y,
				        d_x, d_y,
				        td->tile_wid, td->tile_hgt);
			}

			/* We have to draw overlay... */
			else
			{
				/* Overlay */
				overlay_tiles_2(td, e_x, e_y, s_x, s_y);

				/* And draw the result */
				gdk_draw_rgb_image_2(
				        TERM_DATA_DRAWABLE(td), td->gc, td->trans_buf,
				        0, 0,
				        d_x, d_y,
				        td->tile_wid, td->tile_hgt);

				/* Hack -- Prevent potential display problem */
				gdk_flush();
			}

# else /* USE_EGO_GRAPHICS */

			/* Draw the tile */
			gdk_draw_rgb_image_2(
			        TERM_DATA_DRAWABLE(td), td->gc, td->tiles,
			        s_x, s_y,
			        d_x, d_y,
			        td->tile_wid, td->tile_hgt);

# endif  /* USE_EGO_GRAPHICS */

		}

		/*
		 * Since there's no masking bitblt in X,
		 * we have to do that manually...
		 */
		else
		{

# ifndef USE_EGO_GRAPHICS

			/* Draw mon/PC/obj over terrain */
			overlay_tiles_2(td, s_x, s_y, t_x, t_y);

# else /* !USE_EGO_GRAPHICS */

			/* No overlay */
			if (!has_overlay)
			{
				/* Build terrain + masked overlay image */
				overlay_tiles_2(td, s_x, s_y, t_x, t_y);
			}

			/* With overlay */
			else
			{
				/* Ego over mon/PC over terrain */
				overlay_tiles_3(td, e_x, e_y, s_x, s_y,
				                t_x, t_y);
			}

# endif  /* !USE_EGO_GRAPHICS */

			/* Draw it */
			gdk_draw_rgb_image_2(
			        TERM_DATA_DRAWABLE(td), td->gc, td->trans_buf,
			        0, 0,
			        d_x, d_y,
			        td->tile_wid, td->tile_hgt);

			/* Hack -- Prevent potential display problem */
			gdk_flush();
		}

# else /* USE_TRANSPARENCY */

		/* Draw the tile */
		gdk_draw_rgb_image_2(
		        TERM_DATA_DRAWABLE(td), td->gc, td->tiles,
		        s_x, s_y,
		        d_x, d_y,
		        td->tile_wid, td->tile_hgt);

# endif  /* USE_TRANSPARENCY */

		/*
		 * Advance x-coordinate - wide font fillers are taken care of
		 * before entering the tile drawing code.
		 */
		d_x += td->font_wid;

# ifdef USE_DOUBLE_TILES

		/* Add up *real* number of columns updated XXX XXX XXX */
		cols += use_bigtile ? 2 : 1;

# endif  /* USE_DOUBLE_TILES */
	}

# ifndef USE_DOUBLE_TILES

	/* Copy image from backing store if present */
	TERM_DATA_REFRESH(td, x, y, n, 1);

# else

	/* Copy image from backing store if present */
	TERM_DATA_REFRESH(td, x, y, cols, 1);

# endif  /* USE_DOUBLE_TILES */

	/* Success */
	return (0);
}

#endif /* USE_GRAPHICS */


/*
 * Process an event, if there's none block when wait is set true,
 * return immediately otherwise.
 */
static void CheckEvent(bool wait)
{
	/* Process an event */
	(void)gtk_main_iteration_do(wait);
}


/*
 * Process all pending events (without blocking)
 */
static void DrainEvents(void)
{
	while (gtk_events_pending())
		gtk_main_iteration();
}


/*
 * Handle a "special request"
 */
/*
 * TomeTik: mapeo feature -> tile de dg_iso32.gif (Fase 3b).
 *
 * Los índices de tile y el auto-tiling de muros están PORTADOS del sistema iso
 * de OmnibandTk: índices de tk/config/dg32+iso.cfg y el cálculo de forma de
 * muro de src/common/icon1.c (wall_shape). Tema fijo "light smooth".
 */
#define ISO_T_FLOOR   13   /* suelo */
#define ISO_T_WALL    71   /* muro con forma: base + offset (0..10) */
#define ISO_T_SINGLE  70   /* muro aislado / pilar */
#define ISO_T_DOOR    93   /* abierta +0/+1, cerrada +2/+3, rota +4/+5 (ns/we) */
#define ISO_T_STAIR   99   /* subir +0, bajar +1 */
#define ISO_T_WOODDOOR 64  /* puerta de madera: ns +0, we +1 (entrada a tienda) */
#define ISO_T_DARK   208   /* rombo oscuro: celda desconocida (feature 0 en cfg) */

/* Formas de muro (orden propio); offset dentro del set de 11 tiles. */
enum {
	ISH_SINGLE, ISH_NS, ISH_WE, ISH_NW, ISH_NE, ISH_SW, ISH_SE,
	ISH_TRI_N, ISH_TRI_S, ISH_TRI_W, ISH_TRI_E, ISH_QUAD, ISH_NOT
};
/* offset de tile por forma (de los "offsets" de dg32+iso.cfg). -1 = usar SINGLE. */
static const int iso_wall_off[] = {
	-1, /* SINGLE */  0, /* NS */  1, /* WE */
	 5, /* NW */      2, /* NE */   4, /* SW */   3, /* SE */
	 7, /* TRI_N */   9, /* TRI_S */ 6, /* TRI_W */ 8, /* TRI_E */
	10, /* QUAD */   -1  /* NOT */
};

static bool iso_inb(int y, int x)
{
	return (y >= 0 && x >= 0 && y < cur_hgt && x < cur_wid);
}

/* Declarados antes de iso_is_wall_feat porque éste los usa para excluir puertas
 * y terreno de tipo overlay. */
static int iso_overlay_tile(int f);
static bool iso_is_door_feat(int f);

static bool iso_is_wall_feat(int f)
{
	/* Muro iso (cubo) = cualquier feature con el flag FF1_WALL, EXCEPTO:
	 *  - puertas (se pintan como arco aparte; las cerradas también son WALL),
	 *  - terreno tipo overlay (árbol/montaña/escombros/árbol-muerto: WALL en
	 *    f_info pero los pintamos como sprite transparente sobre el suelo).
	 * Usar el flag (vía f_info) en vez de listas de rangos cubre granito, vetas,
	 * permanente, secreta, cristal, lava... y ADEMÁS los TEJADOS/ventanas/barril
	 * de los edificios del pueblo (feats 190-198, todos WALL), que antes caían al
	 * suelo y se veían como losas planas. */
	if ((f < 0) || (f >= max_f_idx)) return FALSE;
	/* Muro de ilusión: es FLOOR (atravesable) pero SE VE como muro -> en iso lo
	 * pintamos como cubo para preservar la ilusión. */
	if (f == FEAT_ILLUS_WALL) return TRUE;
	if (iso_is_door_feat(f)) return FALSE;
	if (iso_overlay_tile(f) >= 0) return FALSE;
	return (f_info[f].flags1 & FF1_WALL) != 0;
}

/* Tile de SUELO (rombo completo) por feature. */
static int iso_ground_tile(int f)
{
	switch (f)
	{
		case FEAT_GRASS: case FEAT_FLOWER:
		case FEAT_TREES: case FEAT_SMALL_TREES: case FEAT_DEAD_TREE:
		case FEAT_DEAD_SMALL_TREE:         return 0;    /* hierba (árboles van de overlay) */
		case 181:                          return 30;   /* field -> surcos de cultivo */
		case 200: case 201:                return 15;   /* cobblestone road -> adoquín */
		case 207:                          return 58;   /* rocky ground -> grava */
		case FEAT_DIRT: case FEAT_SAND:    return 9;    /* tierra/arena */
		case FEAT_MUD:                     return 22;
		case FEAT_ICE:                     return 39;
		case FEAT_ASH:                     return 14;
		case FEAT_SHAL_WATER:              return 3;    /* agua poco profunda */
		case FEAT_DEEP_WATER: case FEAT_EKKAIA:
		case FEAT_TAINTED_WATER:           return 5;    /* agua profunda */
		case FEAT_SHAL_LAVA:               return 17;
		case FEAT_DEEP_LAVA:               return 18;
		default:                           return 13;   /* piedra */
	}
}

/* Overlay (sprite transparente) sobre el suelo, o -1 si ninguno. */
static int iso_overlay_tile(int f)
{
	switch (f)
	{
		case FEAT_TREES:        return 47;
		case FEAT_SMALL_TREES:  return 48;
		case FEAT_DEAD_TREE:    return 46;
		case FEAT_DEAD_SMALL_TREE: return 46;   /* árbol seco pequeño -> árbol muerto */
		case FEAT_MOUNTAIN:     return 34;
		case FEAT_RUBBLE:       return 58;
		case 16:                return 42;      /* web -> telaraña/red iso */
		default:                return -1;
	}
}

static bool iso_is_up_stair(int f)
{
	return f == FEAT_LESS || f == FEAT_WAY_LESS ||
	       f == FEAT_SHAFT_UP || f == FEAT_QUEST_UP;
}
static bool iso_is_down_stair(int f)
{
	return f == FEAT_MORE || f == FEAT_WAY_MORE || f == FEAT_BETWEEN ||
	       f == FEAT_SHAFT_DOWN || f == FEAT_QUEST_DOWN || f == FEAT_QUEST_ENTER;
}

static bool iso_is_door_feat(int f)
{
	return (f == FEAT_OPEN) || (f == FEAT_BROKEN) ||
	       (f >= FEAT_DOOR_HEAD && f <= FEAT_DOOR_TAIL);
}

/* ¿celda conocida (memorizada)? */
static bool iso_marked(int y, int x)
{
	if (!iso_inb(y, x)) return FALSE;
	return (cave[y][x].info & CAVE_MARK) != 0;
}

/* ¿la celda cuenta como muro/puerta para el auto-tiling (y es conocida)? */
static bool iso_walldoor(int y, int x)
{
	int f;
	if (!iso_marked(y, x)) return FALSE;
	f = cave[y][x].feat;
	return iso_is_wall_feat(f) || iso_is_door_feat(f);
}

/* Forma de muro a partir de los 8 vecinos (port de wall_shape, icon1.c). */
static int iso_wall_shape(int y, int x)
{
	bool wall[3][3];
	int n = 0, nswe = 0, col0n = 0, col2n = 0, row0n = 0, row2n = 0;
	int i, j, shape = ISH_NOT;

	if (!iso_walldoor(y, x)) return ISH_NOT;

	for (j = 0; j < 3; j++)
	{
		for (i = 0; i < 3; i++)
		{
			if (i == 1 && j == 1) { wall[j][i] = FALSE; continue; }
			wall[j][i] = iso_walldoor(y - 1 + j, x - 1 + i);
			if (wall[j][i])
			{
				++n;
				if (!i) ++col0n; else if (i == 1) ++nswe; else ++col2n;
				if (!j) ++row0n; else if (j == 1) ++nswe; else ++row2n;
			}
		}
	}

	if (n == 8) return ISH_SINGLE;
	if (!n || !nswe) return ISH_SINGLE;   /* aislado / sin vecino ortogonal */

	if (nswe == 4)
	{
		shape = ISH_QUAD;
		if (n < 6) return shape;
		if (n == 6)
		{
			if (row0n == 3) return ISH_TRI_S;
			if (row2n == 3) return ISH_TRI_N;
			if (col0n == 3) return ISH_TRI_E;
			if (col2n == 3) return ISH_TRI_W;
			return shape;
		}
		/* n == 7: una esquina falta */
		if (!wall[0][0]) return ISH_SE;
		if (!wall[2][0]) return ISH_NE;
		if (!wall[0][2]) return ISH_SW;
		return ISH_NW;
	}

	if (nswe == 3)
	{
		if (wall[0][1] && wall[2][1])   /* muros a N y S */
		{
			if (col0n == 3 || col2n == 3) return ISH_NS;
			return wall[1][0] ? ISH_TRI_W : ISH_TRI_E;
		}
		else                            /* muros a W y E */
		{
			if (row0n == 3 || row2n == 3) return ISH_WE;
			return wall[0][1] ? ISH_TRI_N : ISH_TRI_S;
		}
	}

	if (nswe == 2)
	{
		if (wall[0][1] && wall[2][1]) shape = ISH_NS;
		if (wall[1][0] && wall[1][2]) shape = ISH_WE;
		if (wall[0][1] && wall[1][0]) shape = ISH_SE;
		if (wall[0][1] && wall[1][2]) shape = ISH_SW;
		if (wall[2][1] && wall[1][0]) shape = ISH_NE;
		if (wall[2][1] && wall[1][2]) shape = ISH_NW;
		return shape;
	}

	if (nswe == 1)
		return (wall[0][1] || wall[2][1]) ? ISH_NS : ISH_WE;

	return ISH_SINGLE;
}

/*
 * Orientación de una puerta: TRUE = "we" (la puerta forma parte de una línea de
 * muro horizontal W-E, se cruza N-S), FALSE = "ns" (línea de muro vertical, se
 * cruza W-E). Se decide por el eje con más muros/puertas flanqueando la celda;
 * así funciona también cuando solo hay muro a un lado o en juntas (antes exigía
 * muro a AMBOS lados W y E y, si no, caía siempre a "ns" -> puertas torcidas). */
static bool iso_door_we(int y, int x)
{
	/* Suma muros/puertas a lo largo de cada eje mirando DOS celdas a cada lado:
	 * una línea de muro real continúa más allá del vecino inmediato, así que
	 * esto detecta la dirección de la pared mucho mejor que solo radio 1 (que
	 * fallaba en cruces y extremos -> arcos torcidos). */
	int we = (iso_walldoor(y, x - 1) ? 1 : 0) + (iso_walldoor(y, x + 1) ? 1 : 0) +
	         (iso_walldoor(y, x - 2) ? 1 : 0) + (iso_walldoor(y, x + 2) ? 1 : 0);
	int ns = (iso_walldoor(y - 1, x) ? 1 : 0) + (iso_walldoor(y + 1, x) ? 1 : 0) +
	         (iso_walldoor(y - 2, x) ? 1 : 0) + (iso_walldoor(y + 2, x) ? 1 : 0);

	/* Eje dominante de la línea de muro. */
	if (we != ns) return (we > ns);

	/* Empate (esquina real / aislada): usa los vecinos inmediatos como antes. */
	return (iso_walldoor(y, x - 1) && iso_walldoor(y, x + 1));
}

/* Blit de un tile de la lámina por índice (col = idx%14, fila = idx/14). */
static void iso_blit(term_data *td, int idx, int sx, int sy)
{
	int col, row;
	if (idx < 0) return;
	col = idx % ISO_SHEET_COLS;
	row = idx / ISO_SHEET_COLS;
	gdk_draw_pixbuf(td->drawing_area->window, td->gc, iso_sheet,
	                col * ISO_TILE_W, row * ISO_TILE_H,
	                sx, sy, ISO_TILE_W, ISO_TILE_H,
	                GDK_RGB_DITHER_NONE, 0, 0);
}

/* Blit de un tile extra de Dungeon Odyssey (54x54) en (sx, sy+DO_DY) para alinear
 * el rombo de suelo con los tiles dg_iso32 (49px). */
static void do_blit(term_data *td, int idx, int sx, int sy)
{
	int col, row;
	if (!do_sheet || idx < 0) return;
	col = idx % DO_COLS;
	row = idx / DO_COLS;
	gdk_draw_pixbuf(td->drawing_area->window, td->gc, do_sheet,
	                col * DO_TILE_W, row * DO_TILE_H,
	                sx, sy + DO_DY, DO_TILE_W, DO_TILE_H,
	                GDK_RGB_DITHER_NONE, 0, 0);
}

/* Mapea una feature a un tile extra de Dungeon Odyssey, o -1 si ninguno.
 * Estas features no tienen equivalente en dg_iso32; se pintan sobre el suelo. */
static int iso_do_tile(int f)
{
	switch (f)
	{
		case FEAT_FOUNTAIN:                return DO_FOUNTAIN;
		case 15:                           return DO_FOUNTAIN;   /* fountain (2ª) */
		case FEAT_DARK_PIT:                return DO_PIT;
		case FEAT_GREAT_FIRE: case FEAT_FIRE: return DO_FIRE;    /* 178 / 205 */
		case FEAT_TRAP:                    return DO_TRAP;
		case FEAT_MON_TRAP:                return DO_MONTRAP;
		case 161:                          return DO_ALTAR_BEING;
		case 162:                          return DO_ALTAR_WINDS;
		case 163:                          return DO_ALTAR_FORCE;
		case 164:                          return DO_ALTAR_DARK;
		case 165:                          return DO_ALTAR_NATURE;
		case 102:                          return DO_NETHER;     /* nether mist */
		case 208: case 210:                return DO_MIRKY;      /* vapour / dense mist */
		case 209:                          return DO_WATER;      /* condensing water */
		case FEAT_GLYPH:                   return DO_GLYPH_GREEN; /* glyph of warding (3) */
		case FEAT_MINOR_GLYPH:             return DO_GLYPH_RED;  /* explosive rune (64) */
		/* Straight Road (camino mágico): tramos 65-70 suelo "graveyard" teal;
		 * 71 descargado (dark water); 72 salida (graveyard + poof); 73 corrupto. */
		case 65: case 66: case 67:
		case 68: case 69: case 70:         return DO_GRAVEYARD;
		case 71:                           return DO_DARKWATER;
		case 72:                           return DO_GRAVE_POOF;
		case 73:                           return DO_DARKWATER_CORRUPT;
		case 173: case 204:                return DO_TUNNEL;     /* Underground Tunnel */
		case FEAT_BETWEEN2:                return DO_PORTAL;     /* Void Jumpgate (176) */
		case 183:                          return DO_FLOORSTONE; /* void */
		case FEAT_TOWN:                    return DO_TOWN;       /* town (203) */
		default:                           return -1;
	}
}

/* Dibuja el rombo de resaltado (hover) en la posición de pantalla (sx,sy) (ya
 * proyectada y desplazada). Se llama UNA vez al final de la escena para que
 * tenga prioridad sobre cualquier tile/sprite (si no, las celdas dibujadas
 * después en orden de profundidad lo taparían). */
static void iso_hover_outline(term_data *td, int sx, int sy)
{
	GdkColor hi;
	GdkPoint pts[4];
	int mx, my;

	mx = sx + ISO_TILE_W / 2;
	my = sy + ISO_FLOOR_CY;

	pts[0].x = mx;               pts[0].y = my - ISO_FLOOR_H / 2;  /* arriba */
	pts[1].x = sx + ISO_TILE_W;  pts[1].y = my;                    /* derecha */
	pts[2].x = mx;               pts[2].y = my + ISO_FLOOR_H / 2;  /* abajo */
	pts[3].x = sx;               pts[3].y = my;                    /* izquierda */

	hi.red = 0xFFFF; hi.green = 0xFFFF; hi.blue = 0x3000;  /* amarillo */
	gdk_gc_set_rgb_fg_color(td->gc, &hi);
	gdk_gc_set_line_attributes(td->gc, 2, GDK_LINE_SOLID,
	                           GDK_CAP_BUTT, GDK_JOIN_MITER);
	gdk_draw_polygon(td->drawing_area->window, td->gc, FALSE, pts, 4);
	gdk_gc_set_line_attributes(td->gc, 0, GDK_LINE_SOLID,
	                           GDK_CAP_BUTT, GDK_JOIN_MITER);
}

/*
 * Callback de celda del núcleo iso: elige y dibuja el/los tile(s) de la celda.
 * Se dibuja una celda si está MEMORIZADA (CAVE_MARK) o VISIBLE ahora mismo
 * (CAVE_SEEN): en mazmorra el suelo iluminado solo por la antorcha queda
 * CAVE_SEEN pero NO se memoriza, así que con solo CAVE_MARK el jugador y su
 * radio de luz salían en negro (el 2D dibuja lo visible, no solo lo memorizado).
 * Muros llevan suelo debajo (los tiles de muro son transparentes en la zona del
 * rombo, igual que los "dynamic" del cfg).
 */
static void iso_cell_cb(void *ctx, int cx, int cy, int sx, int sy)
{
	term_data *td = (term_data *)ctx;
	int f;

	if (!iso_inb(cy, cx)) return;

	/* Celda ni memorizada ni visible: rombo oscuro (rellena los huecos del
	 * borde explorado que asoman bajo la parte transparente de los muros).
	 * EXCEPCIÓN: la celda del propio jugador siempre se dibuja (suelo + sprite),
	 * aunque su rejilla no esté marcada/iluminada (p.ej. pueblo de noche), para
	 * que el personaje no desaparezca. */
	if (!(cave[cy][cx].info & (CAVE_MARK | CAVE_SEEN)) &&
	                !((cy == p_ptr->py) && (cx == p_ptr->px)))
	{
		iso_blit(td, ISO_T_DARK, sx, sy);
		return;
	}

	f = cave[cy][cx].feat;

	/* Escombros (FEAT_RUBBLE 49 y 206 "pile of rubble"): tile custom como overlay
	 * sobre el suelo. Antes del check de muro para que mande aunque la feature
	 * tenga el flag WALL. */
	if (((f == FEAT_RUBBLE) || (f == 206)) && rubble_tile)
	{
		iso_blit(td, iso_ground_tile(f), sx, sy);
		gdk_draw_pixbuf(td->drawing_area->window, td->gc, rubble_tile,
		                0, 0, sx, sy + DO_DY, DO_TILE_W, DO_TILE_H,
		                GDK_RGB_DITHER_NONE, 0, 0);
	}
	else if (iso_is_wall_feat(f))
	{
		iso_blit(td, ISO_T_FLOOR, sx, sy);                       /* suelo debajo */

		if (dun_level == 0)
		{
			/* PUEBLO: rectángulos MACIZOS (sin auto-tiling). Diferenciamos CASA de
			 * MURALLA: las feats de edificio (190..198 = tejados/remates/chimeneas,
			 * ventanas, barril) Y los muros PERM que tocan una de ellas (la base/
			 * cara de la casa, que es FEAT_PERM_SOLID) se pintan con el bloque-
			 * edificio (tapa roja) -> casa uniforme. El resto de muros del pueblo
			 * (muralla suelta) van con el cubo de piedra gris. */
			bool is_building = (f >= 190) && (f <= 198);

			/* Un muro PERM es parte de una casa si hay un tejado (190..198) cerca.
			 * Radio 2 porque algunos edificios (p.ej. alcalde+museo) tienen la
			 * pared sur de DOS filas de grosor y la exterior queda a 2 celdas del
			 * tejado. La muralla/borde del mapa está lejísimos de cualquier
			 * tejado, así que sigue cayendo en el cubo de piedra. */
			if (!is_building)
			{
				int dy, dx;
				for (dy = -2; (dy <= 2) && !is_building; dy++)
				{
					for (dx = -2; dx <= 2; dx++)
					{
						int ny = cy + dy, nx = cx + dx;
						int nf;
						if (!iso_inb(ny, nx)) continue;
						nf = cave[ny][nx].feat;
						if ((nf >= 190) && (nf <= 198)) { is_building = TRUE; break; }
					}
				}
			}

			if (bldg_block && is_building)
				gdk_draw_pixbuf(td->drawing_area->window, td->gc, bldg_block,
				                0, 0, sx, sy, 54, 49, GDK_RGB_DITHER_NONE, 0, 0);
			else
				iso_blit(td, ISO_T_SINGLE, sx, sy);
		}
		else
		{
			/* MAZMORRA: muros finos (corredores/salas) -> auto-tiling por forma. */
			int off = iso_wall_off[iso_wall_shape(cy, cx)];
			iso_blit(td, (off < 0) ? ISO_T_SINGLE : ISO_T_WALL + off, sx, sy);
		}
	}
	else if (iso_is_door_feat(f))
	{
		int base = (f == FEAT_OPEN)   ? ISO_T_DOOR :
		           (f == FEAT_BROKEN) ? ISO_T_DOOR + 4 : ISO_T_DOOR + 2;
		iso_blit(td, ISO_T_FLOOR, sx, sy);                       /* suelo bajo la puerta */
		iso_blit(td, base + (iso_door_we(cy, cx) ? 1 : 0), sx, sy);
	}
	else if (f == FEAT_SHOP)
	{
		/* entrada a tienda: puerta de madera (64 ns / 65 we) sobre suelo */
		iso_blit(td, ISO_T_FLOOR, sx, sy);
		iso_blit(td, ISO_T_WOODDOOR + (iso_door_we(cy, cx) ? 1 : 0), sx, sy);
	}
	else if ((f == FEAT_QUEST_EXIT) || (f == 12))    /* 12 = town exit */
	{
		/* Salida de quest / del pueblo: arco de piedra ABIERTO (93 ns / 94 we),
		 * como una puerta/portón de salida. */
		iso_blit(td, ISO_T_FLOOR, sx, sy);
		iso_blit(td, ISO_T_DOOR + (iso_door_we(cy, cx) ? 1 : 0), sx, sy);
	}
	else if (iso_is_up_stair(f))
	{
		iso_blit(td, iso_ground_tile(f), sx, sy);
		iso_blit(td, ISO_T_STAIR, sx, sy);
	}
	else if (iso_is_down_stair(f))
	{
		iso_blit(td, iso_ground_tile(f), sx, sy);
		iso_blit(td, ISO_T_STAIR + 1, sx, sy);
	}
	else if ((f == FEAT_FLOWER) && flower_tile)
	{
		/* hierba con flores: tile custom (lib/xtra/iso/grass_flowers.png). */
		gdk_draw_pixbuf(td->drawing_area->window, td->gc, flower_tile,
		                0, 0, sx, sy, 54, 49, GDK_RGB_DITHER_NONE, 0, 0);
	}
	else if (do_sheet && iso_do_tile(f) >= 0)
	{
		/* feature sin tile en dg_iso32 -> tile extra de Dungeon Odyssey sobre suelo
		 * (fuente, altar, fuego, pit, trampa, pools de niebla/agua). */
		iso_blit(td, ISO_T_FLOOR, sx, sy);
		do_blit(td, iso_do_tile(f), sx, sy);
	}
	else
	{
		/* terreno general: suelo + posible overlay (árbol/montaña/escombros) */
		int ov = iso_overlay_tile(f);
		iso_blit(td, iso_ground_tile(f), sx, sy);
		if (ov >= 0) iso_blit(td, ov, sx, sy);
	}

	/* Overlay de ACTOR (jugador/monstruo/objeto) con la lámina Gervais 32x32.
	 * map_info da (a,c)=lo de encima y (ta,tc)=terreno; si difieren y es un tile
	 * gráfico (bit alto), lo bliteamos centrado y apoyado en el rombo del suelo. */
	if (gerv_sheet && gerv_cols && gerv_rows)
	{
		byte a, ta, ea;
		char c, tc, ec;

		map_info(cy, cx, &a, &c, &ta, &tc, &ea, &ec);

		if ((a & 0x80) && ((a != ta) || (c != tc)))
		{
			int col = (c & 0x7F) % gerv_cols;
			int row = (a & 0x7F) % gerv_rows;
			/* 32 de ancho centrado en el tile (54); pies hacia el centro del
			 * rombo (alto 49) para que el actor "se pose" en la celda. */
			int dx = sx + (ISO_TILE_W - 32) / 2;
			int dy = sy + ISO_TILE_H / 2 - 32 + ISO_ACTOR_DROP;

			gdk_draw_pixbuf(td->drawing_area->window, td->gc, gerv_sheet,
			                col * 32, row * 32, dx, dy, 32, 32,
			                GDK_RGB_DITHER_NONE, 0, 0);
		}
	}

	/* Barra de vida sobre el actor (jugador o monstruo visible) si no está al
	 * 100%. Verde = vida restante, rojo = daño, con marco negro. */
	{
		int chp = -1, mhp = 0;

		if ((cy == p_ptr->py) && (cx == p_ptr->px))
		{
			chp = p_ptr->chp;
			mhp = p_ptr->mhp;
		}
		else if (cave[cy][cx].m_idx)
		{
			monster_type *m_ptr = &m_list[cave[cy][cx].m_idx];
			if (m_ptr->ml)
			{
				chp = m_ptr->hp;
				mhp = m_ptr->maxhp;
			}
		}

		if ((mhp > 0) && (chp >= 0) && (chp < mhp))
		{
			GdkColor col_bg, col_red, col_green;
			int bw = 28, bh = 4;
			int bx = sx + (ISO_TILE_W - bw) / 2;
			int by = sy + ISO_TILE_H / 2 - 32 + ISO_ACTOR_DROP - bh - 2;  /* sobre el sprite */
			int gw = (bw * chp) / mhp;

			if (gw < 0) gw = 0;
			if (gw > bw) gw = bw;

			col_bg.red = col_bg.green = col_bg.blue = 0x0000;     /* negro */
			col_red.red = 0xD000;  col_red.green = 0x1000; col_red.blue = 0x1000;
			col_green.red = 0x1000; col_green.green = 0xC000; col_green.blue = 0x1000;

			/* Marco negro (relleno) como fondo. */
			gdk_gc_set_rgb_fg_color(td->gc, &col_bg);
			gdk_draw_rectangle(td->drawing_area->window, td->gc, TRUE,
			                   bx - 1, by - 1, bw + 2, bh + 2);
			/* Daño (rojo) y vida restante (verde). */
			gdk_gc_set_rgb_fg_color(td->gc, &col_red);
			gdk_draw_rectangle(td->drawing_area->window, td->gc, TRUE,
			                   bx, by, bw, bh);
			gdk_gc_set_rgb_fg_color(td->gc, &col_green);
			gdk_draw_rectangle(td->drawing_area->window, td->gc, TRUE,
			                   bx, by, gw, bh);
		}
	}

}

/*
 * TomeTik: auditoría de cobertura de tiles en modo ISO. Escribe un informe en
 * ANGBAND_DIR_USER/iso_coverage.txt con:
 *  - MONSTRUOS y OBJETOS sin tile gráfico (x_attr sin el bit 0x80): en iso solo
 *    dibujamos el sprite si map_info devuelve un tile gráfico, así que esas
 *    entidades son INVISIBLES en iso (en 2D salen como letra ASCII).
 *  - FEATURES que el render iso pinta como suelo gris genérico (tile 13) por no
 *    tener tratamiento propio en iso_cell_cb (posibles huecos del mapeo de terreno).
 * Se dispara solo si la variable de entorno TOMETIK_ISO_AUDIT está definida, al
 * entrar en el modo iso. No afecta al juego normal.
 */
static void iso_audit_coverage(void)
{
	FILE *fp;
	char path[1024];
	int i, n_mon = 0, n_obj = 0, n_feat = 0;

	path_build(path, sizeof(path), ANGBAND_DIR_USER, "iso_coverage.txt");
	fp = my_fopen(path, "w");
	if (!fp) { plog_fmt("iso-audit: no pude abrir %s", path); return; }

	fprintf(fp, "# TomeTik - auditoria de cobertura de tiles en modo ISO\n");
	fprintf(fp, "# Entidades sin tile grafico (x_attr sin bit 0x80) -> INVISIBLES en iso.\n");
	fprintf(fp, "# (En modo 2D salen como caracter ASCII; en iso no se dibujan.)\n\n");

	fprintf(fp, "== MONSTRUOS sin tile (invisibles en iso) ==\n");
	for (i = 1; i < max_r_idx; i++)
	{
		monster_race *r = &r_info[i];
		if (!r->name) continue;
		if (!(r->x_attr & 0x80))
		{
			fprintf(fp, "  R:%-4d %s\n", i, r_name + r->name);
			n_mon++;
		}
	}
	fprintf(fp, "  --- total monstruos sin tile: %d ---\n\n", n_mon);

	fprintf(fp, "== OBJETOS sin tile (invisibles en iso) ==\n");
	for (i = 1; i < max_k_idx; i++)
	{
		object_kind *k = &k_info[i];
		if (!k->name) continue;
		if (!(k->x_attr & 0x80))
		{
			fprintf(fp, "  K:%-4d %s\n", i, k_name + k->name);
			n_obj++;
		}
	}
	fprintf(fp, "  --- total objetos sin tile: %d ---\n\n", n_obj);

	fprintf(fp, "== FEATURES que el iso pinta como SUELO GRIS generico (tile 13) ==\n");
	fprintf(fp, "# Sin tratamiento propio en iso_cell_cb; revisar si necesitan tile real\n");
	fprintf(fp, "# (puerta entre mundos, trampa, altar, fuente... no deberian ser suelo).\n");
	for (i = 1; i < max_f_idx; i++)
	{
		feature_type *f = &f_info[i];
		if (!f->name) continue;
		/* ¿el iso le da tratamiento propio? */
		if (iso_is_wall_feat(i) || iso_is_door_feat(i) || (i == FEAT_SHOP) ||
		                (i == FEAT_QUEST_EXIT) || (i == 12) /* town exit */ ||
		                iso_is_up_stair(i) || iso_is_down_stair(i) ||
		                (iso_overlay_tile(i) >= 0) ||
		                (do_sheet && iso_do_tile(i) >= 0))
			continue;
		/* iso_ground_tile devuelve 13 SOLO en el caso por defecto (no reconocido). */
		if (iso_ground_tile(i) == ISO_T_FLOOR)
		{
			fprintf(fp, "  F:%-4d %s\n", i, f_name + f->name);
			n_feat++;
		}
	}
	fprintf(fp, "  --- total features como suelo gris: %d ---\n\n", n_feat);

	fprintf(fp, "RESUMEN: %d monstruos, %d objetos sin tile (invisibles en iso); "
	            "%d features pintadas como suelo gris.\n", n_mon, n_obj, n_feat);

	/* Histograma de features del nivel actual (cave): para saber de qué están
	 * hechos los edificios vs el borde del pueblo (afinar el sprite de edificio). */
	{
		static int hist[256];
		int y, x;
		for (i = 0; i < 256; i++) hist[i] = 0;
		for (y = 0; y < cur_hgt; y++)
			for (x = 0; x < cur_wid; x++)
				hist[cave[y][x].feat & 0xFF]++;
		fprintf(fp, "\n== HISTOGRAMA DE FEATURES DEL NIVEL ACTUAL (dun_level=%d) ==\n",
		        dun_level);
		for (i = 0; i < 256; i++)
			if (hist[i])
				fprintf(fp, "  feat %3d (0x%02X) x%-5d %s%s\n", i, i, hist[i],
				        (i < max_f_idx) ? (f_name + f_info[i].name) : "?",
				        iso_is_wall_feat(i) ? "  [WALL]" : "");
	}

	my_fclose(fp);
	plog_fmt("iso-audit: informe escrito en %s (%d mon, %d obj, %d feat)",
	         path, n_mon, n_obj, n_feat);
}

/* TomeTik: recompone una fila de texto plano del term por encima de la escena
 * iso. El renderer iso pinta negro + tiles directamente sobre la ventana
 * (saltándose el backing store), así que borra el texto que el term ya había
 * dibujado en esa fila -- típicamente la línea de mensajes/prompt (fila 0),
 * p.ej. "(Inven: c-c, ESC) Wear/Wield which item?". La releemos del backing
 * store del term (scr) y la repintamos, igual que en 2D el prompt va siempre
 * sobre el mapa. */
static void iso_overlay_text_row(term_data *td, int row)
{
	term_win *scr = td->t.scr;
	int x, first = -1, last = -1;

	if (!scr || row < 0 || row >= td->rows) return;

	/* Localiza el tramo con contenido (primer/último carácter no-blanco) */
	for (x = 0; x < td->cols; x++)
	{
		if (scr->c[row][x] != ' ')
		{
			if (first < 0) first = x;
			last = x;
		}
	}

	/* Fila vacía: deja ver la escena iso (no pintamos banda negra) */
	if (first < 0) return;

	/* Fondo negro contiguo bajo el texto (como la barra de mensajes 2D),
	 * cubriendo los espacios internos del prompt para que sea legible. */
	gdk_draw_rectangle(td->drawing_area->window,
	                   td->drawing_area->style->black_gc, TRUE,
	                   first * td->font_wid, row * td->font_hgt,
	                   (last - first + 1) * td->font_wid, td->font_hgt);

	/* Redibuja el texto agrupando celdas contiguas del mismo color */
	x = first;
	while (x <= last)
	{
		byte a = scr->a[row][x];
		char buf[256];
		int start = x, len = 0;

		while (x <= last && scr->a[row][x] == a && len < (int)sizeof(buf) - 1)
		{
			buf[len++] = scr->c[row][x];
			x++;
		}

		term_data_set_fg(td, a);
		gdk_draw_text(td->drawing_area->window, td->font, td->gc,
		              start * td->font_wid,
		              td->font->ascent + row * td->font_hgt,
		              buf, len);
	}
}

/* Pinta la escena isométrica completa sobre la ventana principal. */
static void iso_draw_scene(term_data *td)
{
	int fw = td->font_wid, fh = td->font_hgt;
	int win_w = td->cols * fw;
	int win_h = td->rows * fh;

	/* Reservamos a la IZQUIERDA la barra de stats (cols 0..COL_MAP-1) y ARRIBA
	 * la línea de mensajes (fila 0), igual que el resto de modos: la escena iso
	 * se dibuja solo en la región del mapa y esos márgenes se recomponen desde
	 * el render 2D del term (backing store). */
	int ox = COL_MAP * fw;
	int oy = ROW_MAP * fh;
	int map_w = win_w - ox;
	int map_h = win_h - oy;

	GdkRectangle clip;

	if (!iso_sheet || !td->drawing_area->window) return;

	/* Por si el term fuese diminuto: sin sitio para márgenes, pinta a pantalla
	 * completa (comportamiento anterior). */
	if (map_w <= 0 || map_h <= 0) { ox = oy = 0; map_w = win_w; map_h = win_h; }

	/* Auditoría de cobertura (una sola vez, bajo TOMETIK_ISO_AUDIT). Aquí los
	 * x_attr/x_char ya están poblados por el prf de gráficos (estamos pintando
	 * tiles), a diferencia de init_graphics que corre antes de cargarse. */
	if (getenv("TOMETIK_ISO_AUDIT"))
	{
		static bool iso_audited = FALSE;
		if (!iso_audited) { iso_audited = TRUE; iso_audit_coverage(); }
	}

	/* Fondo negro SOLO en la región del mapa. */
	gdk_draw_rectangle(td->drawing_area->window,
	                   td->drawing_area->style->black_gc, TRUE,
	                   ox, oy, map_w, map_h);

	/* Recorta los blits del mapa a su región (muros/sprites altos cerca del
	 * borde no invaden la barra ni la línea de mensajes). */
	clip.x = ox; clip.y = oy; clip.width = map_w; clip.height = map_h;
	gdk_gc_set_clip_rectangle(td->gc, &clip);

	iso_render_scene(td, p_ptr->px, p_ptr->py, map_w, map_h, ox, oy, iso_cell_cb);

	/* Rombo de resaltado del tile bajo el ratón, AL FINAL para que quede ENCIMA
	 * de todo (tiles, muros altos, actores). Se proyecta su celda como hace la
	 * escena (mismas dimensiones y offset). Aún con el clip del mapa activo. */
	if ((iso_hover_y >= 0) && (iso_hover_x >= 0))
	{
		int hsx, hsy;
		iso_project(iso_hover_x, iso_hover_y, p_ptr->px, p_ptr->py,
		            map_w, map_h, &hsx, &hsy);
		iso_hover_outline(td, hsx + ox, hsy + oy);
	}

	gdk_gc_set_clip_rectangle(td->gc, NULL);

	/* Recompón la barra de stats y la línea de mensajes desde el backing store
	 * (la escena iso no las toca). Si no hay backing store, al menos recompón
	 * el texto de la fila 0 (prompts de wield/quaff/...). */
	if (td->backing_store)
	{
		gdk_draw_pixmap(td->drawing_area->window, td->gc, td->backing_store,
		                0, 0, 0, 0, win_w, oy);              /* línea de mensajes */
		gdk_draw_pixmap(td->drawing_area->window, td->gc, td->backing_store,
		                0, oy, 0, oy, ox, win_h - oy);       /* barra lateral */
	}
	else
	{
		iso_overlay_text_row(td, 0);
	}
}

static errr Term_xtra_gtk(int n, int v)
{
	/* Handle a subset of the legal requests */
	switch (n)
	{
		/* Make a noise */
	case TERM_XTRA_NOISE:
		{
			/* Beep */
			gdk_beep();

			/* Success */
			return (0);
		}

		/* Flush the output */
	case TERM_XTRA_FRESH:
		{
			/* TomeTik: en modo iso, repinta la escena isométrica sobre la
			 * ventana principal tras refrescar el term. NO mientras hay texto
			 * plano de pantalla completa por encima: la tienda (iso_in_store)
			 * o cualquier popup que pase por screen_save() -> character_icky
			 * (inventario 'i', hoja 'C', menús de hechizos/skills, ayuda,
			 * opciones, la lista de objetos con '*', prompts de askfor...).
			 * En esos casos dejamos ver el render 2D del term. */
			if (iso_mode && iso_sheet && game_in_progress && character_generated
			                && !iso_in_store && !character_icky
			                && (Term == &data[0].t))
			{
				iso_draw_scene(&data[0]);
			}

			/* Flush pending X requests - almost always no-op */
			gdk_flush();

			/* Success */
			return (0);
		}

		/* Process random events */
	case TERM_XTRA_BORED:
		{
			/* Process a pending event if there's one */
			CheckEvent(FALSE);

			/* Success */
			return (0);
		}

		/* Process Events */
	case TERM_XTRA_EVENT:
		{
			/* Process an event */
			CheckEvent(v);

			/* Success */
			return (0);
		}

		/* Flush the events */
	case TERM_XTRA_FLUSH:
		{
			/* Process all pending events */
			DrainEvents();

			/* Success */
			return (0);
		}

		/* Handle change in the "level" */
	case TERM_XTRA_LEVEL:
		return (0);

		/* Clear the screen */
	case TERM_XTRA_CLEAR:
		return (Term_clear_gtk());

		/* Delay for some milliseconds */
	case TERM_XTRA_DELAY:
		{
			/* sleep for v milliseconds */
			usleep(v * 1000);

			/* Done */
			return (0);
		}

		/* Get Delay of some milliseconds */
	case TERM_XTRA_GET_DELAY:
		{
			int ret;
			struct timeval tv;

			ret = gettimeofday(&tv, NULL);
			Term_xtra_long = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);

			return ret;
		}

		/* Subdirectory scan */
	case TERM_XTRA_SCANSUBDIR:
		{
			DIR *directory;
			struct dirent *entry;

			scansubdir_max = 0;

			directory = opendir(scansubdir_dir);
			if (!directory) return (1);

			while ((entry = readdir(directory)) != NULL)
			{
				char file[PATH_MAX + NAME_MAX + 2];
				struct stat filedata;

				file[PATH_MAX + NAME_MAX] = 0;
				strncpy(file, scansubdir_dir, PATH_MAX);
				strncat(file, "/", 2);
				strncat(file, entry->d_name, NAME_MAX);
				if ((stat(file, &filedata) == 0) && S_ISDIR(filedata.st_mode))
				{
					string_free(scansubdir_result[scansubdir_max]);
					scansubdir_result[scansubdir_max] =
					        string_make(entry->d_name);
					++scansubdir_max;
				}
			}
		}

		/* Rename main window */
	case TERM_XTRA_RENAME_MAIN_WIN: gtk_window_set_title(GTK_WINDOW(data[0].window), angband_term_name[0]); return (0);

		/* React to changes */
	case TERM_XTRA_REACT:
		{
			/* (re-)init colours */
			init_colours();

#ifdef USE_GRAPHICS

			/* Initialise graphics */
			init_graphics();

#endif /* USE_GRAPHICS */

			/* Success */
			return (0);
		}
	}

	/* Unknown */
	return (1);
}




/**** Event handlers ****/


/*
 * Operation overkill
 * Verify term size info - just because the other windowing ports have this
 */
static void term_data_check_size(term_data *td)
{
	/* Enforce minimum window size */
	if (td == &data[0])
	{
		if (td->cols < 80) td->cols = 80;
		if (td->rows < 24) td->rows = 24;
	}
	else
	{
		if (td->cols < 1) td->cols = 1;
		if (td->rows < 1) td->rows = 1;
	}

	/* Paranoia - Enforce maximum size allowed by the term package */
	if (td->cols > 255) td->cols = 255;
	if (td->rows > 255) td->rows = 255;
}


/*
 * Enforce these size constraints within Gtk/Gdk
 * These increments are nice, because you can see numbers of rows/cols
 * while you resize a term.
 */
static void term_data_set_geometry_hints(term_data *td)
{
	GdkGeometry geometry;

	/* Resizing is character size oriented */
	geometry.width_inc = td->font_wid;
	geometry.height_inc = td->font_hgt;

	/* Enforce minimum size - the main window */
	if (td == &data[0])
	{
		geometry.min_width = 80 * td->font_wid;
		geometry.min_height = 24 * td->font_hgt;
	}

	/* Subwindows can be much smaller */
	else
	{
		geometry.min_width = 1 * td->font_wid;
		geometry.min_height = 1 * td->font_hgt;
	}

	/* Enforce term package's hard limit */
	geometry.max_width = 255 * td->font_wid;
	geometry.max_height = 255 * td->font_hgt;

	/* This affects geometry display while we resize a term */
	geometry.base_width = 0;
	geometry.base_height = 0;

	/* Give the window a new set of resizing hints */
	gtk_window_set_geometry_hints(GTK_WINDOW(td->window),
	                              td->drawing_area, &geometry,
	                              GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE
	                              | GDK_HINT_BASE_SIZE | GDK_HINT_RESIZE_INC);
}


/*
 * (Re)allocate a backing store for the window
 */
static void term_data_set_backing_store(term_data *td)
{
	/* Paranoia */
	if (!GTK_WIDGET_REALIZED(td->drawing_area)) return;

	/* Free old one if we cannot use it any longer */
	if (td->backing_store)
	{
		int wid, hgt;

		/* Retrive the size of the old backing store */
		gdk_window_get_size(td->backing_store, &wid, &hgt);

		/* Continue using it if it's the same with desired size */
		if (use_backing_store &&
		                (td->cols * td->font_wid == wid) &&
		                (td->rows * td->font_hgt == hgt)) return;

		/* Free it */
		gdk_pixmap_unref(td->backing_store);

		/* Forget the pointer */
		td->backing_store = NULL;
	}

	/* See user preference */
	if (use_backing_store)
	{
		/* Allocate new backing store */
		td->backing_store = gdk_pixmap_new(
		                            td->drawing_area->window,
		                            td->cols * td->font_wid,
		                            td->rows * td->font_hgt,
		                            -1);

		/* Oops - but we can do without it */
		g_return_if_fail(td->backing_store != NULL);

		/* Clear the backing store */
		gdk_draw_rectangle(
		        td->backing_store,
		        td->drawing_area->style->black_gc,
		        TRUE,
		        0,
		        0,
		        td->cols * td->font_wid,
		        td->rows * td->font_hgt);
	}
}


/*
 * Save game only when it's safe to do so
 */
static void save_game_gtk(void)
{
	/* We have nothing to save, yet */
	if (!game_in_progress || !character_generated) return;

	/* It isn't safe to save game now */
	if (!inkey_flag || !can_save)
	{
		plog("You may not save right now.");
		return;
	}

	/* Hack -- Forget messages */
	msg_flag = FALSE;

	/* Save the game */
#ifdef ZANG_SAVE_GAME
	/* Also for OAngband - the parameter tells if it's autosave */
	do_cmd_save_game(FALSE);
#else
/* Everything else */
	do_cmd_save_game();
#endif /* ZANG_SAVE_GAME */
}


/*
 * Display message in a modal dialog
 */
static void gtk_message(cptr msg)
{
	GtkWidget *dialog, *label, *ok_button;

	/* Create the widgets */
	dialog = gtk_dialog_new();
	g_assert(dialog != NULL);

	label = gtk_label_new(msg);
	g_assert(label != NULL);

	ok_button = gtk_button_new_with_label("OK");
	g_assert(ok_button != NULL);

	/* Ensure that the dialogue box is destroyed when OK is clicked */
	gtk_signal_connect_object(
	        GTK_OBJECT(ok_button),
	        "clicked",
	        GTK_SIGNAL_FUNC(gtk_widget_destroy),
	        (gpointer)dialog);
	gtk_container_add(
	        GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
	        ok_button);

	/* Add the label, and show the dialog */
	gtk_container_add(
	        GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
	        label);

	/* And make it modal */
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

	/* Show the dialog */
	gtk_widget_show_all(dialog);
}


/*
 * Hook to tell the user something important
 */
static void hook_plog(cptr str)
{
	/* Warning message */
	gtk_message(str);
}


/*
 * Process File-Quit menu command
 */
static void quit_event_handler(
        GtkButton *was_clicked,
        gpointer user_data)
{
	/* Save current game */
	save_game_gtk();

	/* It's done */
	quit(NULL);
}


/*
 * Process File-Save menu command
 */
static void save_event_handler(
        GtkButton *was_clicked,
        gpointer user_data)
{
	/* Save current game */
	save_game_gtk();
}


/*
 * Handle destruction of the Angband window
 */
static void destroy_main_event_handler(
        GtkButton *was_clicked,
        gpointer user_data)
{
	/* This allows for cheating, but... */
	quit(NULL);
}


/*
 * Handle destruction of Subwindows
 */
static void destroy_sub_event_handler(
        GtkWidget *window,
        gpointer user_data)
{
	/* Hide the window */
	gtk_widget_hide_all(window);
}


#ifndef SAVEFILE_SCREEN

/*
 * Process File-New menu command
 */
static void new_event_handler(
        GtkButton *was_clicked,
        gpointer user_data)
{
	if (game_in_progress)
	{
		plog("You can't start a new game while you're still playing!");
		return;
	}

	/* The game is in progress */
	game_in_progress = TRUE;

	/* Flush input */
	Term_flush();

	/* Play game */
	play_game(TRUE);

	/* Houseclearing */
	cleanup_angband();

	/* Done */
	quit(NULL);
}

#endif /* !SAVEFILE_SCREEN */


/*
 * Load fond specified by an XLFD fontname and
 * set up related term_data members
 */
static void load_font(term_data *td, cptr fontname)
{
	GdkFont *old = td->font;

	/* Load font */
	td->font = gdk_font_load(fontname);

	if (td->font)
	{
		/* Free the old font */
		if (old) gdk_font_unref(old);
	}
	else
	{
		/* Oops, but we can still use the old one */
		td->font = old;
	}

	/* Calculate the size of the font XXX */
	td->font_wid = gdk_char_width(td->font, '@');
	td->font_hgt = td->font->ascent + td->font->descent;

#ifndef USE_DOUBLE_TILES

	/* Use the current font size for tiles as well */
	td->tile_wid = td->font_wid;
	td->tile_hgt = td->font_hgt;

#else /* !USE_DOUBLE_TILES */

	/* Calculate the size of tiles */
	if (use_bigtile && (td == &data[0])) td->tile_wid = td->font_wid * 2;
	else td->tile_wid = td->font_wid;
	td->tile_hgt = td->font_hgt;

#endif /* !USE_DOUBLE_TILES */
}


/*
 * React to OK button press in font selection dialogue
 */
static void font_ok_callback(
        GtkWidget *widget,
        GtkWidget *font_selector)
{
	gchar *fontname;
	term_data *td;

	td = gtk_object_get_data(GTK_OBJECT(font_selector), "term_data");

	g_assert(td != NULL);

	/* Retrieve font name from player's selection */
	fontname = gtk_font_selection_dialog_get_font_name(
	                   GTK_FONT_SELECTION_DIALOG(font_selector));

	/* Leave unless selection was valid */
	if (fontname == NULL) return;

	/* Load font and update font size info */
	load_font(td, fontname);

	/* Hack - Hide the window - finally found the trick... */
	gtk_widget_hide_all(td->window);

	/* Resizes the drawing area */
	gtk_drawing_area_size(
	        GTK_DRAWING_AREA(td->drawing_area),
	        td->cols * td->font_wid,
	        td->rows * td->font_hgt);

	/* Update the geometry hints for the window */
	term_data_set_geometry_hints(td);

	/* Reallocate the backing store */
	term_data_set_backing_store(td);

	/* Hack - Show the window */
	gtk_widget_show_all(td->window);

#ifdef USE_GRAPHICS

	/* We have to resize tiles when we are in graphics mode */
	resize_request = TRUE;

#endif /* USE_GRAPHICS */

	/* Hack - force redraw */
	Term_key_push(KTRL('R'));
}


/*
 * Process Options-Font-* menu command
 */
static void change_font_event_handler(
        GtkWidget *widget,
        gpointer user_data)
{
	GtkWidget *font_selector;

	gchar *spacings[] = { "c", "m", NULL };

	font_selector = gtk_font_selection_dialog_new("Select font");
#if 0 // DGDGDGDG
	gtk_object_set_data(
	        GTK_OBJECT(font_selector),
	        "term_data",
	        user_data);

	/* Filter to show only fixed-width fonts */
	gtk_font_selection_dialog_set_filter(
	        GTK_FONT_SELECTION_DIALOG(font_selector),
	        GTK_FONT_FILTER_BASE,
	        GTK_FONT_ALL,
	        NULL,
	        NULL,
	        NULL,
	        NULL,
	        spacings,
	        NULL);

	gtk_signal_connect(
	        GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(font_selector)->ok_button),
	        "clicked",
	        font_ok_callback,
	        (gpointer)font_selector);

	/*
	 * Ensure that the dialog box is destroyed when the user clicks
	 * a button.
	 */
	gtk_signal_connect_object(
	        GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(font_selector)->ok_button),
	        "clicked",
	        GTK_SIGNAL_FUNC(gtk_widget_destroy),
	        (gpointer)font_selector);

	gtk_signal_connect_object(
	        GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(font_selector)->cancel_button),
	        "clicked",
	        GTK_SIGNAL_FUNC(gtk_widget_destroy),
	        (gpointer)font_selector);

	gtk_widget_show(GTK_WIDGET(font_selector));
#endif
}


/*
 * Process Terms-* menu command - hide/show terminal window
 */
static void term_event_handler(
        GtkWidget *widget,
        gpointer user_data)
{
	term_data *td = &data[(int)(long)user_data];

	/* We don't mess with the Angband window */
	if (td == &data[0]) return;

	/* It's shown */
	if (td->shown)
	{
		/* Hide the window */
		gtk_widget_hide_all(td->window);
	}

	/* It's hidden */
	else
	{
		/* Show the window */
		gtk_widget_show_all(td->window);

		/*
		 * TomeTik: una sub-ventana recién mostrada salía en BLANCO. El juego
		 * ya había dibujado su contenido (vía window_stuff) MIENTRAS la ventana
		 * estaba oculta, así que la caché del term lo da por pintado y, al
		 * mostrarla, no repinta nada -> backing_store vacío -> negro.
		 *
		 * Solución: (1) bombear los eventos GTK pendientes para que la ventana
		 * se realice/dimensione/mapee y exista el backing_store; (2) reasegurar
		 * el contenido con window_stuff(); (3) forzar Term_redraw() sobre ESTE
		 * term, que ignora la caché y repinta todas las celdas al backing_store
		 * ya visible. window_stuff() solo afecta a terms con window_flag puesto
		 * (ver lib/user/user.prf).
		 */
		while (gtk_events_pending()) gtk_main_iteration();

		if (game_in_progress && character_generated)
		{
			term *old = Term;

			p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER | PW_M_LIST |
			                  PW_MESSAGE | PW_OVERHEAD | PW_MONSTER | PW_OBJECT);
			window_stuff();

			/* Forzar repintado completo de la ventana recién mostrada */
			Term_activate(&td->t);
			Term_redraw();
			Term_fresh();
			Term_activate(old);
		}
	}
}


/*
 * Toggles the boolean value of use_backing_store and
 * setup / remove backing store for each term
 */
static void change_backing_store_event_handler(
        GtkButton *was_clicked,
        gpointer user_data)
{
	int i;

	/* Toggle the backing store mode */
	use_backing_store = !use_backing_store;

	/* Reset terms */
	for (i = 0; i < MAX_TERM_DATA; i++)
	{
		term_data_set_backing_store(&data[i]);
	}
}


#ifdef USE_GRAPHICS

/*
 * Set graf_mode_request according to user selection,
 * and let Term_xtra react to the change.
 */
static void change_graf_mode_event_handler(
        GtkButton *was_clicked,
        gpointer user_data)
{
	/* Set request according to user selection */
	graf_mode_request = (int)user_data;

	/*
	 * Hack - force redraw
	 * This induces a call to Term_xtra(TERM_XTRA_REACT, 0) as well
	 */
	Term_key_push(KTRL('R'));
}


/*
 * Set dither_mode according to user selection
 */
static void change_dith_mode_event_handler(
        GtkButton *was_clicked,
        gpointer user_data)
{
	/* Set request according to user selection */
	dith_mode = (int)user_data;

	/*
	 * Hack - force redraw
	 */
	Term_key_push(KTRL('R'));
}


/*
 * Toggles the graphics tile scaling mode (Fast/Smooth)
 */
static void change_smooth_mode_event_handler(
        GtkButton *was_clicked,
        gpointer user_data)
{
	/* (Try to) toggle the smooth rescaling mode */
	smooth_rescaling_request = !smooth_rescaling;

	/*
	 * Hack - force redraw
	 * This induces a call to Term_xtra(TERM_XTRA_REACT, 0) as well
	 */
	Term_key_push(KTRL('R'));
}


# ifdef USE_DOUBLE_TILES

static void change_wide_tile_mode_event_handler(
        GtkButton *was_clicked,
        gpointer user_data)
{
	term *old = Term;
	term_data *td = &data[0];

	/* Toggle "use_bigtile" */
	use_bigtile = !use_bigtile;

#ifdef TOME
	/* T.o.M.E. requires this as well */
	arg_bigtile = use_bigtile;
#endif /* TOME */

	/* Double the width of tiles (only for the main window) */
	if (use_bigtile)
	{
		td->tile_wid = td->font_wid * 2;
	}

	/* Use the width of current font */
	else
	{
		td->tile_wid = td->font_wid;
	}

	/* Need to resize the tiles */
	resize_request = TRUE;

	/* Activate the main window */
	Term_activate(&td->t);

	/* Resize the term */
	Term_resize(td->cols, td->rows);

	/*
	 * TomeTik: recalcular el viewport del mapa para el nuevo estado de bigtile.
	 * Sin esto, el panel del mapa (panel_col_max) se queda con el nº de casillas
	 * viejo -> al desactivar wide tiles el mapa salía a medio ancho, y al
	 * activarlo quedaban artefactos en el sidebar. resize_map() resetea el panel,
	 * fuerza verify_panel()/panel_bounds() y marca PR_WIPE|PR_BASIC|PR_EXTRA|PR_MAP
	 * (redibuja mapa + sidebar). Se llama con la ventana principal activa porque
	 * lee el tamaño del término activo.
	 */
	resize_map();

	/* Activate the old term */
	Term_activate(old);

	/* Hack - force redraw XXX ??? XXX */
	Term_key_push(KTRL('R'));
}

# endif  /* USE_DOUBLE_TILES */


# ifdef USE_TRANSPARENCY

/*
 * Toggles the boolean value of use_transparency
 */
static void change_trans_mode_event_handler(
        GtkButton *was_clicked,
        gpointer user_data)
{
	/* Toggle the transparency mode */
	use_transparency = !use_transparency;

	/* Hack - force redraw */
	Term_key_push(KTRL('R'));
}

# endif  /* USE_TRANSPARENCY */

#endif /* USE_GRAPHICS */


#ifndef SAVEFILE_SCREEN

/*
 * Caution: Modal or not, callbacks are called by gtk_main(),
 * so this is the right place to start a game.
 */
static void file_ok_callback(
        GtkWidget *widget,
        GtkWidget *file_selector)
{
	strcpy(savefile,
	       gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_selector)));

	gtk_widget_destroy(file_selector);

	/* game is in progress */
	game_in_progress = TRUE;

	/* Flush input */
	Term_flush();

	/* Play game */
	play_game(FALSE);

	/* Free memory allocated by game */
	cleanup_angband();

	/* Done */
	quit(NULL);
}


/*
 * Process File-Open menu command
 */
static void open_event_handler(
        GtkButton *was_clicked,
        gpointer user_data)
{
	GtkWidget *file_selector;
	char buf[1024];


	if (game_in_progress)
	{
		plog("You can't open a new game while you're still playing!");
		return;
	}

	/* Prepare the savefile path */
	path_build(buf, 1024, ANGBAND_DIR_SAVE, "*");

	file_selector = gtk_file_selection_new("Select a savefile");
	gtk_file_selection_set_filename(
	        GTK_FILE_SELECTION(file_selector),
	        buf);
	gtk_signal_connect(
	        GTK_OBJECT(GTK_FILE_SELECTION(file_selector)->ok_button),
	        "clicked",
	        file_ok_callback,
	        (gpointer)file_selector);

	/*
	 * Ensure that the dialog box is destroyed when the user
	 * clicks a button.
	 */
	gtk_signal_connect_object(
	        GTK_OBJECT(GTK_FILE_SELECTION(file_selector)->ok_button),
	        "clicked",
	        GTK_SIGNAL_FUNC(gtk_widget_destroy),
	        (gpointer)file_selector);

	gtk_signal_connect_object(
	        GTK_OBJECT(GTK_FILE_SELECTION(file_selector)->cancel_button),
	        "clicked",
	        GTK_SIGNAL_FUNC(gtk_widget_destroy),
	        (gpointer)file_selector);

	gtk_window_set_modal(GTK_WINDOW(file_selector), TRUE);
	gtk_widget_show(GTK_WIDGET(file_selector));
}

#endif /* !SAVEFILE_SCREEN */


/*
 * React to "delete" signal sent to Window widgets
 */
static gboolean delete_event_handler(
        GtkWidget *widget,
        GdkEvent *event,
        gpointer user_data)
{
	/* Save game if possible */
	save_game_gtk();

	/* Don't prevent closure */
	return (FALSE);
}


/*
 * Convert keypress events to ASCII codes and enqueue them
 * for game
 */
static gboolean keypress_event_handler(
        GtkWidget *widget,
        GdkEventKey *event,
        gpointer user_data)
{
#if 1
	int i, mc, ms, mo, mx;

	char msg[128];

	/* Hack - do not do anything until the player picks from the menu */
	if (!game_in_progress) return (TRUE);

	/* Hack - Ignore parameters */
	(void) widget;
	(void) user_data;

	/* Extract four "modifier flags" */
	mc = (event->state & GDK_CONTROL_MASK) ? TRUE : FALSE;
	ms = (event->state & GDK_SHIFT_MASK) ? TRUE : FALSE;
	mo = (event->state & GDK_MOD1_MASK) ? TRUE : FALSE;
	mx = (event->state & GDK_MOD3_MASK) ? TRUE : FALSE;

	/*
	 * Hack XXX
	 * Parse shifted numeric (keypad) keys specially.
	 */
	if ((event->state == GDK_SHIFT_MASK)
	                && (event->keyval >= GDK_KP_0) && (event->keyval <= GDK_KP_9))
	{
		/* Build the macro trigger string */
		strnfmt(msg, 128, "%cS_%X%c", 31, event->keyval, 13);

		/* Enqueue the "macro trigger" string */
		for (i = 0; msg[i]; i++) Term_keypress(msg[i]);

		/* Hack -- auto-define macros as needed */
		if (event->length && (macro_find_exact(msg) < 0))
		{
			/* Create a macro */
			macro_add(msg, event->string);
		}

		return (TRUE);
	}

	/* Normal keys with no modifiers */
	if (event->length && !mo && !mx)
	{
		/* Enqueue the normal key(s) */
		for (i = 0; i < event->length; i++) Term_keypress(event->string[i]);

		/* All done */
		return (TRUE);
	}


	/* Handle a few standard keys (bypass modifiers) XXX XXX XXX */
	switch ((uint) event->keyval)
	{
	case GDK_Escape:
		{
			Term_keypress(ESCAPE);
			return (TRUE);
		}

	case GDK_Return:
		{
			Term_keypress('\r');
			return (TRUE);
		}

	case GDK_Tab:
		{
			Term_keypress('\t');
			return (TRUE);
		}

	case GDK_Delete:
	case GDK_BackSpace:
		{
			Term_keypress('\010');
			return (TRUE);
		}

		/* Hack - the cursor keys */
	case GDK_Up:
		{
			Term_keypress('8');
			return (TRUE);
		}

	case GDK_Down:
		{
			Term_keypress('2');
			return (TRUE);
		}

	case GDK_Left:
		{
			Term_keypress('4');
			return (TRUE);
		}

	case GDK_Right:
		{
			Term_keypress('6');
			return (TRUE);
		}

		/*
		 * TomeTik: teclas del KEYPAD numérico -> dígitos de movimiento.
		 * Crítico para noVNC: el navegador/Xvfb traduce las flechas a teclas
		 * del keypad (KP_Up/KP_Left/...), no a las flechas dedicadas, así que
		 * sin esto las flechas no mueven por noVNC (sí por VNC nativo). Se
		 * cubren ambos estados de NumLock (KP_Up y KP_8) y las diagonales.
		 */
	case GDK_KP_Up:    case GDK_KP_8: { Term_keypress('8'); return (TRUE); }
	case GDK_KP_Down:  case GDK_KP_2: { Term_keypress('2'); return (TRUE); }
	case GDK_KP_Left:  case GDK_KP_4: { Term_keypress('4'); return (TRUE); }
	case GDK_KP_Right: case GDK_KP_6: { Term_keypress('6'); return (TRUE); }
	case GDK_KP_Home:      case GDK_KP_7: { Term_keypress('7'); return (TRUE); }
	case GDK_KP_Page_Up:   case GDK_KP_9: { Term_keypress('9'); return (TRUE); }
	case GDK_KP_End:       case GDK_KP_1: { Term_keypress('1'); return (TRUE); }
	case GDK_KP_Page_Down: case GDK_KP_3: { Term_keypress('3'); return (TRUE); }
	case GDK_KP_Begin:     case GDK_KP_5: { Term_keypress('5'); return (TRUE); }

	case GDK_Shift_L:
	case GDK_Shift_R:
	case GDK_Control_L:
	case GDK_Control_R:
	case GDK_Caps_Lock:
	case GDK_Shift_Lock:
	case GDK_Meta_L:
	case GDK_Meta_R:
	case GDK_Alt_L:
	case GDK_Alt_R:
	case GDK_Super_L:
	case GDK_Super_R:
	case GDK_Hyper_L:
	case GDK_Hyper_R:
		{
			/* Hack - do nothing to control characters */
			return (TRUE);
		}
	}

	/* Build the macro trigger string */
	strnfmt(msg, 128, "%c%s%s%s%s_%X%c", 31,
	        mc ? "N" : "", ms ? "S" : "",
	        mo ? "O" : "", mx ? "M" : "",
	        event->keyval, 13);

	/* Enqueue the "macro trigger" string */
	for (i = 0; msg[i]; i++) Term_keypress(msg[i]);

	/* Hack -- auto-define macros as needed */
	if (event->length && (macro_find_exact(msg) < 0))
	{
		/* Create a macro */
		macro_add(msg, event->string);
	}

	return (TRUE);

#else
	int i, mc, ms, mo, mx;

	char msg[128];


	/* Extract four "modifier flags" */
	mc = (event->state & GDK_CONTROL_MASK) ? TRUE : FALSE;
	ms = (event->state & GDK_SHIFT_MASK) ? TRUE : FALSE;
	mo = (event->state & GDK_MOD1_MASK) ? TRUE : FALSE;
	mx = (event->state & GDK_MOD3_MASK) ? TRUE : FALSE;
	printf("0=%d 9=%d;; keyval=%d; mc=%d, ms=%d  ::=:: ", GDK_KP_0, GDK_KP_9, event->keyval, mc, ms);
	/* Enqueue the normal key(s) */
	for (i = 0; i < event->length; i++) printf("%d;", event->string[i]);
	printf("\n");

	/*
	* Hack XXX
	* Parse shifted numeric (keypad) keys specially.
	*/
	if ((event->state & GDK_SHIFT_MASK)
	                && (event->keyval >= GDK_KP_Left) && (event->keyval <= GDK_KP_Delete))
	{
		/* Build the macro trigger string */
		strnfmt(msg, 128, "%cS_%X%c", 31, event->keyval, 13);
		printf("%cS_%X%c", 31, event->keyval, 13);

		/* Enqueue the "macro trigger" string */
		for (i = 0; msg[i]; i++) Term_keypress(msg[i]);

		/* Hack -- auto-define macros as needed */
		if (event->length && (macro_find_exact(msg) < 0))
		{
			/* Create a macro */
			macro_add(msg, event->string);
		}

		return (TRUE);
	}

	/* Normal keys with no modifiers */
	if (event->length && !mo && !mx)
	{
		/* Enqueue the normal key(s) */
		for (i = 0; i < event->length; i++) Term_keypress(event->string[i]);

		/* All done */
		return (TRUE);
	}

	/* Handle a few standard keys (bypass modifiers) XXX XXX XXX */
	switch ((uint) event->keyval)
	{
	case GDK_Escape:
		{
			Term_keypress(ESCAPE);
			return (TRUE);
		}

	case GDK_Return:
		{
			Term_keypress('\r');
			return (TRUE);
		}

	case GDK_Tab:
		{
			Term_keypress('\t');
			return (TRUE);
		}

	case GDK_Delete:
	case GDK_BackSpace:
		{
			Term_keypress('\010');
			return (TRUE);
		}

	case GDK_Shift_L:
	case GDK_Shift_R:
	case GDK_Control_L:
	case GDK_Control_R:
	case GDK_Caps_Lock:
	case GDK_Shift_Lock:
	case GDK_Meta_L:
	case GDK_Meta_R:
	case GDK_Alt_L:
	case GDK_Alt_R:
	case GDK_Super_L:
	case GDK_Super_R:
	case GDK_Hyper_L:
	case GDK_Hyper_R:
		{
			/* Hack - do nothing to control characters */
			return (TRUE);
		}
	}

	/* Build the macro trigger string */
	strnfmt(msg, 128, "%c%s%s%s%s_%X%c", 31,
	        mc ? "N" : "", ms ? "S" : "",
	        mo ? "O" : "", mx ? "M" : "",
	        event->keyval, 13);

	/* Enqueue the "macro trigger" string */
	for (i = 0; msg[i]; i++) Term_keypress(msg[i]);

	/* Hack -- auto-define macros as needed */
	if (event->length && (macro_find_exact(msg) < 0))
	{
		/* Create a macro */
		macro_add(msg, event->string);
	}

	return (TRUE);
#endif
}


/*
 * Widget customisation (for drawing area) - "realize" signal
 *
 * In this program, called when window containing the drawing
 * area is shown first time.
 */
static void realize_event_handler(
        GtkWidget *widget,
        gpointer user_data)
{
	term_data *td = (term_data *)user_data;

	/* Create graphic context */
	td->gc = gdk_gc_new(td->drawing_area->window);

	/* Set foreground and background colours - isn't bg used at all? */
	gdk_rgb_gc_set_background(td->gc, 0x000000);
	gdk_rgb_gc_set_foreground(td->gc, angband_colours[TERM_WHITE]);

	/* No last foreground colour, yet */
	td->last_attr = -1;

	/* Allocate the backing store */
	term_data_set_backing_store(td);

	/* Clear the window */
	gdk_draw_rectangle(
	        widget->window,
	        widget->style->black_gc,
	        TRUE,
	        0,
	        0,
	        td->cols * td->font_wid,
	        td->rows * td->font_hgt);
}


/*
 * Widget customisation (for drawing area) - "show" signal
 */
static void show_event_handler(
        GtkWidget *widget,
        gpointer user_data)
{
	term_data *td = (term_data *)user_data;

	/* Set the shown flag */
	td->shown = TRUE;
}


/*
 * Widget customisation (for drawing area) - "hide" signal
 */
static void hide_event_handler(
        GtkWidget *widget,
        gpointer user_data)
{
	term_data *td = (term_data *)user_data;

	/* Set the shown flag */
	td->shown = FALSE;
}


/*
 * Widget customisation (for drawing area)- handle size allocation requests
 */
static void size_allocate_event_handler(
        GtkWidget *widget,
        GtkAllocation *allocation,
        gpointer user_data)
{
	term_data *td = user_data;
	int old_rows, old_cols;
	term *old = Term;

	/* Paranoia */
	g_return_if_fail(widget != NULL);
	g_return_if_fail(allocation != NULL);
	g_return_if_fail(td != NULL);

	/* Remember old values */
	old_cols = td->cols;
	old_rows = td->rows;

	/* Update numbers of rows and columns */
	td->cols = (allocation->width + td->font_wid - 1) / td->font_wid;
	td->rows = (allocation->height + td->font_hgt - 1) / td->font_hgt;

	/* Overkill - Validate them */
	term_data_check_size(td);

	/* Adjust size request and set it */
	allocation->width = td->cols * td->font_wid;
	allocation->height = td->rows * td->font_hgt;
	widget->allocation = *allocation;

	/* Widget is realized, so we do some drawing works */
	if (GTK_WIDGET_REALIZED(widget))
	{
		/* Reallocate the backing store */
		term_data_set_backing_store(td);

		/* Actually handles resizing in Gtk */
		gdk_window_move_resize(
		        widget->window,
		        allocation->x,
		        allocation->y,
		        allocation->width,
		        allocation->height);

		/* And in the term package */
		Term_activate(&td->t);

		/* Resize if necessary */
		if ((td->cols != old_cols) || (td->rows != old_rows))
			(void)Term_resize(td->cols, td->rows);

		/* Redraw its content */
		Term_redraw();

		/* Refresh */
		Term_fresh();

		/* Restore */
		Term_activate(old);
	}
}


/*
 * Update exposed area in a window (for drawing area)
 */
static gboolean expose_event_handler(
        GtkWidget *widget,
        GdkEventExpose *event,
        gpointer user_data)
{
	term_data *td = user_data;

	term *old = Term;

#ifndef NO_REDRAW_SECTION

	int x1, x2, y1, y2;

#endif /* !NO_REDRAW_SECTION */


	/* Paranoia */
	if (td == NULL) return (TRUE);

	/* TomeTik: en modo iso la escena se dibuja DIRECTO a la ventana (el backing
	 * store guarda el render 2D del term). Por eso cualquier expose -p.ej. al
	 * arrastrarse el popup del tooltip por encima del mapa- debe repintar la
	 * escena iso, no blitear el 2D del backing store. Mismas condiciones que el
	 * repintado iso de TERM_XTRA_FRESH (incluido el gate por popups: con un
	 * texto plano de pantalla completa arriba restauramos el backing store
	 * 2D, no repintamos la escena iso). */
	if (iso_mode && iso_sheet && game_in_progress && character_generated
	                && !iso_in_store && !character_icky && (td == &data[0]))
	{
		iso_draw_scene(td);
		return (TRUE);
	}

	/* The window has a backing store */
	if (td->backing_store)
	{
		/* Simply restore the exposed area from the backing store */
		gdk_draw_pixmap(
		        td->drawing_area->window,
		        td->gc,
		        td->backing_store,
		        event->area.x,
		        event->area.y,
		        event->area.x,
		        event->area.y,
		        event->area.width,
		        event->area.height);
	}

	/* No backing store - use the game's code to redraw the area */
	else
	{

		/* Activate the relevant term */
		Term_activate(&td->t);

# ifdef NO_REDRAW_SECTION

		/* K.I.S.S. version */

		/* Redraw */
		Term_redraw();

# else /* NO_REDRAW_SECTION */

		/*
		 * Complex version - The above is enough, but since we have
		 * Term_redraw_section... This might help if we had a graphics
		 * mode.
		 */

		/* Convert coordinate in pixels to character cells */
		x1 = event->area.x / td->font_wid;
		x2 = (event->area.x + event->area.width) / td->font_wid;
		y1 = event->area.y / td->font_hgt;
		y2 = (event->area.y + event->area.height) / td->font_hgt;

		/*
		 * No paranoia - boundary checking is done in
		 * Term_redraw_section
		 */

		/* Redraw the area */
		Term_redraw_section(x1, y1, x2, y2);

# endif  /* NO_REDRAW_SECTION */

		/* Refresh */
		Term_fresh();

		/* Restore */
		Term_activate(old);
	}

	/* We've processed the event ourselves */
	return (TRUE);
}




/**** Initialisation ****/

/*
 * Initialise a term_data struct
 */
static errr term_data_init(term_data *td, int i)
{
	term *t = &td->t;
	char *p;

	/* TomeTik: tamaño por ventana según el layout por defecto. */
	td->cols = tometik_layout[i].cols;
	td->rows = tometik_layout[i].rows;

	/* Initialize the term */
	term_init(t, td->cols, td->rows, 1024);

	/* Store the name of the term */
	td->name = string_make(angband_term_name[i]);

	/* Instance names should start with a lowercase letter XXX */
	for (p = (char *)td->name; *p; p++) *p = tolower(*p);

	/* Use a "soft" cursor */
	t->soft_cursor = TRUE;

	/* Erase with "white space" */
	t->attr_blank = TERM_WHITE;
	t->char_blank = ' ';

	t->xtra_hook = Term_xtra_gtk;
	t->text_hook = Term_text_gtk;
	t->wipe_hook = Term_wipe_gtk;
	t->curs_hook = Term_curs_gtk;
#ifdef USE_GRAPHICS
	t->pict_hook = Term_pict_gtk;
#endif /* USE_GRAPHICS */
	t->nuke_hook = Term_nuke_gtk;

	/* Save the data */
	t->data = td;

	/* Activate (important) */
	Term_activate(t);

	/* Success */
	return (0);
}


/*
 * Neater menu code with GtkItemFactory.
 *
 * Menu bar of the Angband window
 *
 * Entry format: Path, Accelerator, Callback, Callback arg, type
 * where type is one of:
 * <Item> - simple item, alias NULL
 * <Branch> - has submenu
 * <Separator> - as you read it
 * <CheckItem> - has a check mark
 * <ToggleItem> - is a toggle
 */
static GtkItemFactoryEntry main_menu_items[] =
{
	/* "File" menu */
	{ "/File", NULL,
	  NULL, 0, "<Branch>"
	},
#ifndef SAVEFILE_SCREEN
	{ "/File/New", "<mod1>N",
	  new_event_handler, 0, NULL },
	{ "/File/Open", "<mod1>O",
	  open_event_handler, 0, NULL },
	{ "/File/sep1", NULL,
	  NULL, 0, "<Separator>" },
#endif /* !SAVEFILE_SCREEN */
	{ "/File/Save", "<mod1>S",
	  save_event_handler, 0, NULL },
	{ "/File/Quit", "<mod1>Q",
	  quit_event_handler, 0, NULL },

	/* "Terms" menu */
	{ "/Terms", NULL,
	  NULL, 0, "<Branch>" },
	/* XXX XXX XXX NULL's are replaced by the program */
	{ NULL, "<mod1>0",
	  term_event_handler, 0, "<CheckItem>" },
	{ NULL, "<mod1>1",
	  term_event_handler, 1, "<CheckItem>" },
	{ NULL, "<mod1>2",
	  term_event_handler, 2, "<CheckItem>" },
	{ NULL, "<mod1>3",
	  term_event_handler, 3, "<CheckItem>" },
	{ NULL, "<mod1>4",
	  term_event_handler, 4, "<CheckItem>" },
	{ NULL, "<mod1>5",
	  term_event_handler, 5, "<CheckItem>" },
	{ NULL, "<mod1>6",
	  term_event_handler, 6, "<CheckItem>" },
	{ NULL, "<mod1>7",
	  term_event_handler, 7, "<CheckItem>" },

	/* "Options" menu */
	{ "/Options", NULL,
	  NULL, 0, "<Branch>" },

	/* "Font" submenu */
	{ "/Options/Font", NULL,
	  NULL, 0, "<Branch>" },
	/* XXX XXX XXX Again, NULL's are filled by the program */
	{ NULL, NULL,
	  change_font_event_handler, 0, NULL },
	{ NULL, NULL,
	  change_font_event_handler, 1, NULL },
	{ NULL, NULL,
	  change_font_event_handler, 2, NULL },
	{ NULL, NULL,
	  change_font_event_handler, 3, NULL },
	{ NULL, NULL,
	  change_font_event_handler, 4, NULL },
	{ NULL, NULL,
	  change_font_event_handler, 5, NULL },
	{ NULL, NULL,
	  change_font_event_handler, 6, NULL },
	{ NULL, NULL,
	  change_font_event_handler, 7, NULL },

#ifdef USE_GRAPHICS

	/* "Graphics" submenu */
	{ "/Options/Graphics", NULL,
	  NULL, 0, "<Branch>" },
	{ "/Options/Graphics/None", NULL,
	  change_graf_mode_event_handler, GRAF_MODE_NONE, "<CheckItem>" },
	{ "/Options/Graphics/Old", NULL,
	  change_graf_mode_event_handler, GRAF_MODE_OLD, "<CheckItem>" },
	{ "/Options/Graphics/New", NULL,
	  change_graf_mode_event_handler, GRAF_MODE_NEW, "<CheckItem>" },
	{ "/Options/Graphics/Isometric", NULL,
	  change_graf_mode_event_handler, GRAF_MODE_ISO, "<CheckItem>" },
# ifdef USE_DOUBLE_TILES
	{ "/Options/Graphics/sep3", NULL,
	  NULL, 0, "<Separator>" },
	{ "/Options/Graphics/Wide tiles", NULL,
	  change_wide_tile_mode_event_handler, 0, "<CheckItem>" },
# endif  /* USE_DOUBLE_TILES */
	{ "/Options/Graphics/sep1", NULL,
	  NULL, 0, "<Separator>" },
	{ "/Options/Graphics/Dither if <= 8bpp", NULL,
	  change_dith_mode_event_handler, GDK_RGB_DITHER_NORMAL, "<CheckItem>" },
	{ "/Options/Graphics/Dither if <= 16bpp", NULL,
	  change_dith_mode_event_handler, GDK_RGB_DITHER_MAX, "<CheckItem>" },
	{ "/Options/Graphics/sep2", NULL,
	  NULL, 0, "<Separator>" },
	{ "/Options/Graphics/Smoothing", NULL,
	  change_smooth_mode_event_handler, 0, "<CheckItem>" },
# ifdef USE_TRANSPARENCY
	{ "/Options/Graphics/Transparency", NULL,
	  change_trans_mode_event_handler, 0, "<CheckItem>" },
# endif  /* USE_TRANSPARENCY */

#endif /* USE_GRAPHICS */

	/* "Misc" submenu */
	{ "/Options/Misc", NULL,
	  NULL, 0, "<Branch>" },
	{ "/Options/Misc/Backing store", NULL,
	  change_backing_store_event_handler, 0, "<CheckItem>" },
};


/*
 * XXX XXX Fill those NULL's in the menu definition with
 * angband_term_name[] strings
 */
static void setup_menu_paths(void)
{
	int i;
	int nmenu_items = sizeof(main_menu_items) / sizeof(main_menu_items[0]);
	GtkItemFactoryEntry *term_entry, *font_entry;
	char buf[64];

	/* Find the "Terms" menu */
	for (i = 0; i < nmenu_items; i++)
	{
		/* Skip NULLs */
		if (main_menu_items[i].path == NULL) continue;

		/* Find a match */
		if (streq(main_menu_items[i].path, "/Terms")) break;
	}
	g_assert(i < (nmenu_items - MAX_TERM_DATA));

	/* Remember the location */
	term_entry = &main_menu_items[i + 1];

	/* Find "Font" menu */
	for (i = 0; i < nmenu_items; i++)
	{
		/* Skip NULLs */
		if (main_menu_items[i].path == NULL) continue;

		/* Find a match */
		if (streq(main_menu_items[i].path, "/Options/Font")) break;
	}
	g_assert(i < (nmenu_items - MAX_TERM_DATA));

	/* Remember the location */
	font_entry = &main_menu_items[i + 1];

	/* For each terminal */
	for (i = 0; i < MAX_TERM_DATA; i++)
	{
		/* XXX XXX Build the real path name to the entry */
		strnfmt(buf, 64, "/Terms/%s", angband_term_name[i]);

		/* XXX XXX Store it in the menu definition */
		term_entry[i].path = (char *)string_make(buf);

		/* XXX XXX Build the real path name to the entry */
		strnfmt(buf, 64, "/Options/Font/%s", angband_term_name[i]);

		/* XXX XXX Store it in the menu definition */
		font_entry[i].path = (char *)string_make(buf);
	}
}


/*
 * XXX XXX Free strings allocated by setup_menu_paths()
 */
static void free_menu_paths(void)
{
	int i;
	int nmenu_items = sizeof(main_menu_items) / sizeof(main_menu_items[0]);
	GtkItemFactoryEntry *term_entry, *font_entry;

	/* Find the "Terms" menu */
	for (i = 0; i < nmenu_items; i++)
	{
		/* Skip NULLs */
		if (main_menu_items[i].path == NULL) continue;

		/* Find a match */
		if (streq(main_menu_items[i].path, "/Terms")) break;
	}
	g_assert(i < (nmenu_items - MAX_TERM_DATA));

	/* Remember the location */
	term_entry = &main_menu_items[i + 1];

	/* Find "Font" menu */
	for (i = 0; i < nmenu_items; i++)
	{
		/* Skip NULLs */
		if (main_menu_items[i].path == NULL) continue;

		/* Find a match */
		if (streq(main_menu_items[i].path, "/Options/Font")) break;
	}
	g_assert(i < (nmenu_items - MAX_TERM_DATA));

	/* Remember the location */
	font_entry = &main_menu_items[i + 1];

	/* For each terminal */
	for (i = 0; i < MAX_TERM_DATA; i++)
	{
		/* XXX XXX Free Term menu path */
		if (term_entry[i].path) string_free((cptr)term_entry[i].path);

		/* XXX XXX Free Font menu path */
		if (font_entry[i].path) string_free((cptr)font_entry[i].path);
	}
}


/*
 * Find widget corresponding to path name
 * return NULL on error
 */
static GtkWidget *get_widget_from_path(cptr path)
{
	GtkItemFactory *item_factory;
	GtkWidget *widget;

	/* Paranoia */
	if (path == NULL) return (NULL);

	/* Look up item factory */
	item_factory = gtk_item_factory_from_path(path);

	/* Oops */
	if (item_factory == NULL) return (NULL);

	/* Look up widget */
	widget = gtk_item_factory_get_widget(item_factory, path);

	/* Return result */
	return (widget);
}


/*
 * Enable/disable a menu item
 */
void enable_menu_item(cptr path, bool enabled)
{
	GtkWidget *widget;

	/* Access menu item widget */
	widget = get_widget_from_path(path);

	/* Paranoia */
	g_assert(widget != NULL);
	g_assert(GTK_IS_MENU_ITEM(widget));

	/*
	 * In Gtk's terminology, enabled is sensitive
	 * and disabled insensitive
	 */
	gtk_widget_set_sensitive(widget, enabled);
}


/*
 * Check/uncheck a menu item. The item should be of the GtkCheckMenuItem type
 */
void check_menu_item(cptr path, bool checked)
{
	GtkWidget *widget;

	/* Access menu item widget */
	widget = get_widget_from_path(path);

	/* Paranoia */
	g_assert(widget != NULL);
	g_assert(GTK_IS_CHECK_MENU_ITEM(widget));

	/*
	 * Put/remove check mark
	 *
	 * Mega-Hack -- The function supposed to be used here,
	 * gtk_check_menu_item_set_active(), emits an "activate" signal
	 * to the GtkMenuItem class of the widget, as if the menu item
	 * were selected by user, thereby causing bizarre behaviour.
	 * XXX XXX XXX
	 */
	GTK_CHECK_MENU_ITEM(widget)->active = checked;
}


/*
 * Update the "File" menu
 */
static void file_menu_update_handler(
        GtkWidget *widget,
        gpointer user_data)
{
#ifndef SAVEFILE_SCREEN
	bool game_start_ok;
#endif /* !SAVEFILE_SCREEN */
	bool save_ok, quit_ok;

#ifndef SAVEFILE_SCREEN

	/* Can we start a game now? */
	game_start_ok = !game_in_progress;

#endif /* !SAVEFILE_SCREEN */

	/* Cave we save/quit now? */
	if (!character_generated || !game_in_progress)
	{
		save_ok = FALSE;
		quit_ok = TRUE;
	}
	else
	{
		if (inkey_flag && can_save) save_ok = quit_ok = TRUE;
		else save_ok = quit_ok = FALSE;
	}

	/* Enable / disable menu items according to those conditions */
#ifndef SAVEFILE_SCREEN
	enable_menu_item("<Angband>/File/New", game_start_ok);
	enable_menu_item("<Angband>/File/Open", game_start_ok);
#endif /* !SAVEFILE_SCREEN */
	enable_menu_item("<Angband>/File/Save", save_ok);
	enable_menu_item("<Angband>/File/Quit", quit_ok);
}


/*
 * Update the "Terms" menu
 */
static void term_menu_update_handler(
        GtkWidget *widget,
        gpointer user_data)
{
	int i;
	char buf[64];

	/* For each term */
	for (i = 0; i < MAX_TERM_DATA; i++)
	{
		/* Build the path name */
		strnfmt(buf, 64, "<Angband>/Terms/%s", angband_term_name[i]);

		/* Update the check mark on the item */
		check_menu_item(buf, data[i].shown);
	}
}


/*
 * Update the "Font" submenu
 */
static void font_menu_update_handler(
        GtkWidget *widget,
        gpointer user_data)
{
	int i;
	char buf[64];

	/* For each term */
	for (i = 0; i < MAX_TERM_DATA; i++)
	{
		/* Build the path name */
		strnfmt(buf, 64, "<Angband>/Options/Font/%s", angband_term_name[i]);

		/* Enable selection if the term is shown */
		enable_menu_item(buf, data[i].shown);
	}
}


/*
 * Update the "Misc" submenu
 */
static void misc_menu_update_handler(
        GtkWidget *widget,
        gpointer user_data)
{
	/* Update an item */
	check_menu_item(
	        "<Angband>/Options/Misc/Backing store",
	        use_backing_store);
}


#ifdef USE_GRAPHICS

/*
 * Update the "Graphics" submenu
 */
static void graf_menu_update_handler(
        GtkWidget *widget,
        gpointer user_data)
{
	/* Update menu items */
	check_menu_item(
	        "<Angband>/Options/Graphics/None",
	        (graf_mode == GRAF_MODE_NONE));
	check_menu_item(
	        "<Angband>/Options/Graphics/Old",
	        (graf_mode == GRAF_MODE_OLD));
	check_menu_item(
	        "<Angband>/Options/Graphics/New",
	        (graf_mode == GRAF_MODE_NEW));
	check_menu_item(
	        "<Angband>/Options/Graphics/Isometric",
	        (graf_mode == GRAF_MODE_ISO));

#ifdef USE_DOUBLE_TILES

	check_menu_item(
	        "<Angband>/Options/Graphics/Wide tiles",
	        use_bigtile);

#endif /* USE_DOUBLE_TILES */

	check_menu_item(
	        "<Angband>/Options/Graphics/Dither if <= 8bpp",
	        (dith_mode == GDK_RGB_DITHER_NORMAL));
	check_menu_item(
	        "<Angband>/Options/Graphics/Dither if <= 16bpp",
	        (dith_mode == GDK_RGB_DITHER_MAX));

	check_menu_item(
	        "<Angband>/Options/Graphics/Smoothing",
	        smooth_rescaling);

# ifdef USE_TRANSPARENCY

	check_menu_item(
	        "<Angband>/Options/Graphics/Transparency",
	        use_transparency);

# endif  /* USE_TRANSPARENCY */
}

#endif /* USE_GRAPHICS */


/*
 * Construct a menu hierarchy using GtkItemFactory, setting up
 * callbacks and accelerators along the way, and return
 * a GtkMenuBar widget.
 */
GtkWidget *get_main_menu(term_data *td)
{
	GtkItemFactory *item_factory;
	GtkAccelGroup *accel_group;
	gint nmenu_items = sizeof(main_menu_items) / sizeof(main_menu_items[0]);


	/* XXX XXX Setup path names in the "Terms" and "Font" menus */
	setup_menu_paths();

	/* Allocate an accelerator group */
	accel_group = gtk_accel_group_new();
	g_assert(accel_group != NULL);

	/* Initialise the item factory */
	item_factory = gtk_item_factory_new(
	                       GTK_TYPE_MENU_BAR,
	                       "<Angband>",
	                       accel_group);
	g_assert(item_factory != NULL);

	/* Generate the menu items */
	gtk_item_factory_create_items(
	        item_factory,
	        nmenu_items,
	        main_menu_items,
	        NULL);

	/* Attach the new accelerator group to the window */
	gtk_window_add_accel_group(
	        GTK_WINDOW(td->window),
	        accel_group);

	/* Return the actual menu bar created */
	return (gtk_item_factory_get_widget(item_factory, "<Angband>"));
}


/*
 * Install callbacks to update menus
 */
static void add_menu_update_callbacks()
{
	GtkWidget *widget;

	/* Access the "File" menu */
	widget = get_widget_from_path("<Angband>/File");

	/* Paranoia */
	g_assert(widget != NULL);
	g_assert(GTK_IS_MENU(widget));

	/* Assign callback */
	gtk_signal_connect(
	        GTK_OBJECT(widget),
	        "show",
	        GTK_SIGNAL_FUNC(file_menu_update_handler),
	        NULL);

	/* Access the "Terms" menu */
	widget = get_widget_from_path("<Angband>/Terms");

	/* Paranoia */
	g_assert(widget != NULL);
	g_assert(GTK_IS_MENU(widget));

	/* Assign callback */
	gtk_signal_connect(
	        GTK_OBJECT(widget),
	        "show",
	        GTK_SIGNAL_FUNC(term_menu_update_handler),
	        NULL);

	/* Access the "Font" menu */
	widget = get_widget_from_path("<Angband>/Options/Font");

	/* Paranoia */
	g_assert(widget != NULL);
	g_assert(GTK_IS_MENU(widget));

	/* Assign callback */
	gtk_signal_connect(
	        GTK_OBJECT(widget),
	        "show",
	        GTK_SIGNAL_FUNC(font_menu_update_handler),
	        NULL);

	/* Access the "Misc" menu */
	widget = get_widget_from_path("<Angband>/Options/Misc");

	/* Paranoia */
	g_assert(widget != NULL);
	g_assert(GTK_IS_MENU(widget));

	/* Assign callback */
	gtk_signal_connect(
	        GTK_OBJECT(widget),
	        "show",
	        GTK_SIGNAL_FUNC(misc_menu_update_handler),
	        NULL);

#ifdef USE_GRAPHICS

	/* Access Graphics menu */
	widget = get_widget_from_path("<Angband>/Options/Graphics");

	/* Paranoia */
	g_assert(widget != NULL);
	g_assert(GTK_IS_MENU(widget));

	/* Assign callback */
	gtk_signal_connect(
	        GTK_OBJECT(widget),
	        "show",
	        GTK_SIGNAL_FUNC(graf_menu_update_handler),
	        NULL);

#endif /* USE_GRAPHICS */
}


/*
 * TomeTik: tooltips de casilla al pasar el ratón por el mapa.
 *
 * Funciona en los tres modos de render (iso / tiles 2D / ASCII) y con tiles
 * simples o dobles (bigtile): el texto lo genera el motor con describe_grid()
 * (el mismo "qué hay aquí" del comando look), y el píxel se convierte a celda
 * del cave según el modo. El globo es un GTK_WINDOW_POPUP que sigue al cursor.
 */

/* Convierte un píxel (px,py) del drawing area del mapa a la celda del cave
 * (*cy,*cx). Devuelve FALSE si el píxel cae fuera del área de mapa (p.ej. el
 * sidebar de texto o la línea superior en modo 2D/ASCII). */
static bool gtk_map_pixel_to_cave(term_data *td, int px, int py, int *cy, int *cx)
{
	if (iso_mode)
	{
		/* Inverso de la proyección isométrica. El mapa iso vive desplazado por la
		 * barra de stats (ox) y la línea de mensajes (oy); fuera de esa región no
		 * hay casilla de mapa. */
		int fw = td->font_wid, fh = td->font_hgt;
		int ox = COL_MAP * fw;
		int oy = ROW_MAP * fh;
		int map_w = td->cols * fw - ox;
		int map_h = td->rows * fh - oy;

		if (map_w <= 0 || map_h <= 0) { ox = oy = 0; map_w = td->cols * fw; map_h = td->rows * fh; }
		if (px < ox || py < oy) return FALSE;

		iso_unproject(px - ox, py - oy, p_ptr->px, p_ptr->py, map_w, map_h, cx, cy);
		return TRUE;
	}
	else
	{
		/* 2D / ASCII: inverso de panel_col_of()/panel_row_of() (cave.c). El
		 * mapa empieza en la celda (COL_MAP,ROW_MAP) del term; con bigtile cada
		 * columna del cave ocupa 2 celdas de term. */
		int scol = px / td->font_wid;
		int srow = py / td->font_hgt;
		int col = scol - COL_MAP;
		int row = srow - ROW_MAP;

		/* Fuera del área de mapa (sidebar / línea de mensajes). */
		if (col < 0 || row < 0) return FALSE;

		if (use_bigtile) col /= 2;
		if (use_zoom) { col /= arg_zoom; row /= arg_zoom; }

		*cx = col + panel_col_min;
		*cy = row + panel_row_min;
		return TRUE;
	}
}

/* Crea el popup del tooltip la primera vez, con aspecto de tooltip clásico
 * (fondo amarillo pálido, texto negro, multilínea alineado a la izquierda). */
static void tooltip_ensure(void)
{
	GdkColor bg, fg;

	if (tooltip_win) return;

	tooltip_win = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_widget_set_name(tooltip_win, "gtk-tooltips");
	gtk_container_set_border_width(GTK_CONTAINER(tooltip_win), 4);

	tooltip_label = gtk_label_new("");
	gtk_misc_set_alignment(GTK_MISC(tooltip_label), 0.0, 0.0);
	gtk_label_set_justify(GTK_LABEL(tooltip_label), GTK_JUSTIFY_LEFT);
	gtk_container_add(GTK_CONTAINER(tooltip_win), tooltip_label);
	gtk_widget_show(tooltip_label);

	/* Amarillo pálido clásico + texto negro (independiente del tema). */
	bg.red = 0xFFFF; bg.green = 0xFFFF; bg.blue = 0xC000;
	fg.red = 0x0000; fg.green = 0x0000; fg.blue = 0x0000;
	gtk_widget_modify_bg(tooltip_win, GTK_STATE_NORMAL, &bg);
	gtk_widget_modify_fg(tooltip_label, GTK_STATE_NORMAL, &fg);
}

/* Realmente pinta el globo (lo llama el timer tras el retardo). */
static gboolean tooltip_reveal_cb(gpointer data)
{
	tooltip_ensure();
	gtk_label_set_text(GTK_LABEL(tooltip_label), tooltip_pending);
	/* Un poco abajo-derecha del cursor, para no taparlo. */
	gtk_window_move(GTK_WINDOW(tooltip_win), tooltip_px + 12, tooltip_py + 16);
	gtk_widget_show(tooltip_win);

	tooltip_timer = 0;
	return FALSE;   /* one-shot */
}

/* Oculta el tooltip, cancela el timer pendiente y olvida la celda mostrada. */
static void tooltip_hide(void)
{
	tooltip_cy = tooltip_cx = -1;
	if (tooltip_timer) { g_source_remove(tooltip_timer); tooltip_timer = 0; }
	if (tooltip_win) gtk_widget_hide(tooltip_win);
}

/* Programa el tooltip para 'text' junto al puntero, tras TOOLTIP_DELAY_MS. */
static void tooltip_arm(cptr text, gint root_x, gint root_y)
{
	strnfmt(tooltip_pending, sizeof(tooltip_pending), "%s", text);
	tooltip_px = root_x;
	tooltip_py = root_y;

	if (tooltip_timer) g_source_remove(tooltip_timer);
	tooltip_timer = g_timeout_add(TOOLTIP_DELAY_MS, tooltip_reveal_cb, NULL);
}

/* Quita el resaltado de hover (iso) y repinta la escena si hacía falta. */
static void iso_clear_hover(term_data *td)
{
	if (!iso_mode) return;
	if ((iso_hover_y == -1) && (iso_hover_x == -1)) return;

	iso_hover_y = iso_hover_x = -1;
	if (game_in_progress && character_generated) iso_draw_scene(td);
}

/* Movimiento del ratón sobre el mapa: actualiza el tooltip de casilla. */
static gboolean motion_notify_event_handler(
        GtkWidget *widget,
        GdkEventMotion *event,
        gpointer user_data)
{
	term_data *td = (term_data *)user_data;
	int cy = 0, cx = 0;
	char buf[256];

	/* Solo con una partida realmente en curso (cave[] poblado). */
	if (!game_in_progress || !character_generated)
	{
		tooltip_hide();
		iso_clear_hover(td);
		return FALSE;
	}

	if (!gtk_map_pixel_to_cave(td, (int)event->x, (int)event->y, &cy, &cx))
	{
		tooltip_hide();
		iso_clear_hover(td);
		return FALSE;
	}

	/* Misma celda: si el globo ya está visible, lo seguimos con el cursor;
	 * si aún está en el retardo, actualizamos dónde aparecerá. */
	if (cy == tooltip_cy && cx == tooltip_cx)
	{
		if (tooltip_win && GTK_WIDGET_VISIBLE(tooltip_win))
			gtk_window_move(GTK_WINDOW(tooltip_win),
			                (gint)event->x_root + 12, (gint)event->y_root + 16);
		else if (tooltip_timer)
		{
			tooltip_px = (gint)event->x_root;
			tooltip_py = (gint)event->y_root;
		}
		return FALSE;
	}

	/* Cambiamos de celda: resaltar el tile bajo el ratón (iso) repintando la
	 * escena con el nuevo rombo marcado. */
	if (iso_mode && ((cy != iso_hover_y) || (cx != iso_hover_x)))
	{
		iso_hover_y = cy;
		iso_hover_x = cx;
		iso_draw_scene(td);
	}

	/* Pedir al motor el "qué hay aquí". */
	describe_grid(cy, cx, buf);

	if (buf[0] == '\0')
	{
		tooltip_hide();
		return FALSE;
	}

	/* Nueva celda con contenido: rearmar el retardo (oculta el globo previo). */
	tooltip_cy = cy;
	tooltip_cx = cx;
	if (tooltip_win) gtk_widget_hide(tooltip_win);
	tooltip_arm(buf, (gint)event->x_root, (gint)event->y_root);

	return FALSE;
}

/* El ratón sale del mapa: ocultar el tooltip y quitar el resaltado. */
static gboolean leave_notify_event_handler(
        GtkWidget *widget,
        GdkEventCrossing *event,
        gpointer user_data)
{
	term_data *td = (term_data *)user_data;

	tooltip_hide();
	iso_clear_hover(td);
	return FALSE;
}

/* Clic en el mapa: "go to" (click-to-walk). Botón izquierdo sobre una casilla
 * transitable -> el motor calcula la ruta A* y camina hasta allí. Reutiliza la
 * misma identificación de celda que el tooltip. El ESCAPE que inyectamos solo
 * sirve para desbloquear el inkey() en el que el motor está esperando comando;
 * el bucle de turnos ve 'travelling' y va dando los pasos. */
static gboolean button_press_event_handler(
        GtkWidget *widget,
        GdkEventButton *event,
        gpointer user_data)
{
	term_data *td = (term_data *)user_data;
	int cy = 0, cx = 0;

	/* Solo botón izquierdo, con partida en curso y no dentro de menú/tienda. */
	if (event->button != 1) return FALSE;
	if (!game_in_progress || !character_generated || character_icky) return FALSE;

	if (!gtk_map_pixel_to_cave(td, (int)event->x, (int)event->y, &cy, &cx))
		return FALSE;

	/* Clic: atacar/intercambiar si hay un monstruo adyacente, o viajar hasta la
	 * casilla. Si hace algo, desbloquear el inkey para que el bucle de turnos lo
	 * ejecute (el ESCAPE es un no-op de comando). */
	if (do_cmd_click(cy, cx))
	{
		tooltip_hide();
		Term_keypress(ESCAPE);
	}

	return TRUE;
}


/*
 * Create Gtk widgets for a terminal window and set up callbacks
 */
static void init_gtk_window(term_data *td, int i)
{
	GtkWidget *menu_bar = NULL, *box;
	cptr font;

	bool main_window = (i == 0) ? TRUE : FALSE;


	/* Create window */
	td->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	/* Set title */
	gtk_window_set_title(GTK_WINDOW(td->window), td->name);

	/* TomeTik: posición inicial según el layout por defecto (el WM la respeta
	 * al mapear la ventana). Evita que las sub-ventanas salgan amontonadas. */
	gtk_window_move(GTK_WINDOW(td->window), tometik_layout[i].x, tometik_layout[i].y);


	/* Get default font for this term */
	font = get_default_font(i);

	/* Load font and initialise related term_data fields */
	load_font(td, font);


	/* Create drawing area */
	td->drawing_area = gtk_drawing_area_new();

	/* Set the size of the drawing area */
	gtk_drawing_area_size(
	        GTK_DRAWING_AREA(td->drawing_area),
	        td->cols * td->font_wid,
	        td->rows * td->font_hgt);

	/* Set geometry hints */
	term_data_set_geometry_hints(td);


	/* Install window event handlers */
	gtk_signal_connect(
	        GTK_OBJECT(td->window),
	        "delete_event",
	        GTK_SIGNAL_FUNC(delete_event_handler),
	        NULL);
	gtk_signal_connect(
	        GTK_OBJECT(td->window),
	        "key_press_event",
	        GTK_SIGNAL_FUNC(keypress_event_handler),
	        NULL);

	/* Destroying the Angband window terminates the game */
	if (main_window)
	{
		gtk_signal_connect(
		        GTK_OBJECT(td->window),
		        "destroy_event",
		        GTK_SIGNAL_FUNC(destroy_main_event_handler),
		        NULL);
	}

	/* The other windows are just hidden */
	else
	{
		gtk_signal_connect(
		        GTK_OBJECT(td->window),
		        "destroy_event",
		        GTK_SIGNAL_FUNC(destroy_sub_event_handler),
		        td);
	}


	/* Install drawing area event handlers */
	gtk_signal_connect(
	        GTK_OBJECT(td->drawing_area),
	        "realize",
	        GTK_SIGNAL_FUNC(realize_event_handler),
	        (gpointer)td);
	gtk_signal_connect(
	        GTK_OBJECT(td->drawing_area),
	        "show",
	        GTK_SIGNAL_FUNC(show_event_handler),
	        (gpointer)td);
	gtk_signal_connect(
	        GTK_OBJECT(td->drawing_area),
	        "hide",
	        GTK_SIGNAL_FUNC(hide_event_handler),
	        (gpointer)td);
	gtk_signal_connect(
	        GTK_OBJECT(td->drawing_area),
	        "size_allocate",
	        GTK_SIGNAL_FUNC(size_allocate_event_handler),
	        (gpointer)td);
	gtk_signal_connect(
	        GTK_OBJECT(td->drawing_area),
	        "expose_event",
	        GTK_SIGNAL_FUNC(expose_event_handler),
	        (gpointer)td);

	/* TomeTik: tooltips de casilla al pasar el ratón (solo en el mapa). El
	 * drawing area no recibe eventos de movimiento por defecto: hay que pedir
	 * la máscara explícitamente. */
	if (main_window)
	{
		gtk_widget_add_events(td->drawing_area,
		                      GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK
		                      | GDK_BUTTON_PRESS_MASK);
		gtk_signal_connect(
		        GTK_OBJECT(td->drawing_area),
		        "motion_notify_event",
		        GTK_SIGNAL_FUNC(motion_notify_event_handler),
		        (gpointer)td);
		gtk_signal_connect(
		        GTK_OBJECT(td->drawing_area),
		        "leave_notify_event",
		        GTK_SIGNAL_FUNC(leave_notify_event_handler),
		        (gpointer)td);
		gtk_signal_connect(
		        GTK_OBJECT(td->drawing_area),
		        "button_press_event",
		        GTK_SIGNAL_FUNC(button_press_event_handler),
		        (gpointer)td);
	}


	/* Create menu */
	if (main_window)
	{
		/* Build the main menu bar */
		menu_bar = get_main_menu(td);
		g_assert(menu_bar != NULL);

		/* Since it's tedious to scatter the menu update code around */
		add_menu_update_callbacks();
	}


	/* Pack the menu bar together with the main window */
	/* For vertical placement of the menu bar and the drawing area */
	box = gtk_vbox_new(FALSE, 0);

	/* Let the window widget own it */
	gtk_container_add(GTK_CONTAINER(td->window), box);

	/* The main window has a menu bar */
	if (main_window)
		gtk_box_pack_start(
		        GTK_BOX(box),
		        menu_bar,
		        FALSE,
		        FALSE,
		        NO_PADDING);

	/* And place the drawing area just beneath it */
	gtk_box_pack_start_defaults(GTK_BOX(box), td->drawing_area);


	/* Show the widgets - use of td->shown is a dirty hack XXX XXX */
	if (td->shown) gtk_widget_show_all(td->window);
}


/*
 * To be hooked into quit(). See z-util.c
 */
static void hook_quit(cptr str)
{
	/* Free menu paths dynamically allocated */
	free_menu_paths();

# ifdef USE_GRAPHICS

	/* Free pathname string */
	if (ANGBAND_DIR_XTRA_GRAF) string_free(ANGBAND_DIR_XTRA_GRAF);

# endif  /* USE_GRAPHICS */

	/* Terminate the program */
	gtk_exit(0);
}


#ifdef ANGBAND300

/*
 * Help message for this port
 */
const char help_gtk[] =
        "GTK for X11, subopts -n<windows>\n"
        "           -b(acking store off)\n"
#ifdef USE_GRAPHICS
        "           -g(raphics) -o(ld graphics) -s(moothscaling off) \n"
        "           -t(ransparency on)\n"
# ifdef USE_DOUBLE_TILES
        "           -w(ide tiles)\n"
# endif  /* USE_DOUBLE_TILES */
#endif /* USE_GRAPHICS */
        "           and standard GTK options";

#endif /* ANGBAND300 */


/*
 * Initialization function
 */
errr init_gtk2(int argc, char **argv)
{
	int i;


	/* Initialize the environment */
	gtk_init(&argc, &argv);

	/*
	 * TomeTik: bigtile ("wide tiles") ACTIVADO por defecto. Con la fuente del
	 * mapa (10x20) esto hace que los tiles 32x32 de Gervais se rendericen
	 * CUADRADOS (~20x20) en vez de estirados, muy parecido al look nativo del
	 * build de Windows. Importante: se activa AQUÍ, de inicio, para que el
	 * panel del mapa y el render queden consistentes. Los bugs conocidos de
	 * bigtile (artefactos en el sidebar, mapa a medio ancho) solo aparecen al
	 * TOGGLEARLO en caliente desde Options -> "wide tiles", no al arrancar.
	 */
	use_bigtile = arg_bigtile = TRUE;

	/* Activate hooks - Use gtk/glib interface throughout */
	ralloc_aux = hook_ralloc;
	rnfree_aux = hook_rnfree;
	quit_aux = hook_quit;
	core_aux = hook_quit;

	/* Parse args */
	for (i = 1; i < argc; i++)
	{
		/* Number of terminals displayed at start up */
		if (prefix(argv[i], "-n"))
		{
			num_term = atoi(&argv[i][2]);
			if (num_term > MAX_TERM_DATA) num_term = MAX_TERM_DATA;
			else if (num_term < 1) num_term = 1;
			continue;
		}

		/* Disable use of pixmaps as backing store */
		if (streq(argv[i], "-b"))
		{
			use_backing_store = FALSE;
			continue;
		}

#ifdef USE_GRAPHICS

		/* Requests "old" graphics */
		if (streq(argv[i], "-o"))
		{
			graf_mode_request = GRAF_MODE_OLD;
			continue;
		}

		/* Requests "new" graphics */
		if (streq(argv[i], "-g"))
		{
			graf_mode_request = GRAF_MODE_NEW;
			continue;
		}

# ifdef USE_DOUBLE_TILES

		/* Requests wide tile mode */
		if (streq(argv[i], "-w"))
		{
			use_bigtile = TRUE;
# ifdef TOME
			/* T.o.M.E. uses older version of the patch */
			arg_bigtile = TRUE;
# endif  /* TOME */
			continue;
		}

# endif  /* USE_DOUBLE_TILES */


		/* Enable transparency effect */
		if (streq(argv[i], "-t"))
		{
			use_transparency = TRUE;
			continue;
		}

		/* Disable smooth rescaling of tiles */
		if (streq(argv[i], "-s"))
		{
			smooth_rescaling_request = FALSE;
			continue;
		}

		/* TomeTik: arrancar en modo isométrico (= seleccionar GRAF_MODE_ISO).
		 * init_graphics fija iso_mode y carga las láminas. Se puede alternar en
		 * caliente desde Options -> Graphics. */
		if (streq(argv[i], "-i"))
		{
			graf_mode_request = GRAF_MODE_ISO;
			continue;
		}

#endif /* USE_GRAPHICS */

		/* None of the above */
		plog_fmt("Ignoring option: %s", argv[i]);
	}

#ifdef USE_GRAPHICS

	{
		char path[1024];

		/* Build the "graf" path */
		path_build(path, 1024, ANGBAND_DIR_XTRA, "graf");

		/* Allocate the path */
		ANGBAND_DIR_XTRA_GRAF = string_make(path);
	}

#endif /* USE_GRAPHICS */

	/* Initialise colours */
	gdk_rgb_init();
	gtk_widget_set_default_colormap(gdk_rgb_get_cmap());
	gtk_widget_set_default_visual(gdk_rgb_get_visual());
	init_colours();

	/*
	 * Initialise the windows backwards, so that
	 * the Angband window comes in front
	 */
	for (i = MAX_TERM_DATA - 1; i >= 0; i--)
	{
		term_data *td = &data[i];

		/* Initialize the term_data */
		term_data_init(td, i);

		/* Hack - Set the shown flag, meaning "to be shown" XXX XXX */
		if (i < num_term) td->shown = TRUE;
		else td->shown = FALSE;

		/* Save global entry */
		angband_term[i] = Term;

		/* Init the window */
		init_gtk_window(td, i);
	}

	/* TomeTik: las láminas del modo iso (dg_iso32.gif + 32x32.bmp) se cargan de
	 * forma perezosa en init_graphics()/iso_load_sheets() la primera vez que se
	 * entra en GRAF_MODE_ISO (sea por -i al arrancar o por el menú Graphics). */

	/* Activate the "Angband" window screen */
	Term_activate(&data[0].t);

#ifndef SAVEFILE_SCREEN

	/* Set the system suffix */
	ANGBAND_SYS = "gtk";

	/* Catch nasty signals */
	signals_init();

	/* Initialize */
	init_angband();

#ifndef OLD_SAVEFILE_CODE

	/* Hack - because this port has New/Open menus XXX */
	savefile[0] = '\0';

#endif /* !OLD_SAVEFILE_CODE */

	/* Prompt the user */
	prt("[Choose 'New' or 'Open' from the 'File' menu]", 23, 17);
	Term_fresh();

	/* Activate more hook */
	plog_aux = hook_plog;


	/* Processing loop */
	gtk_main();


	/* Free allocated memory */
	cleanup_angband();

	/* Stop now */
	quit(NULL);

#else /* !SAVEFILE_SCREEN */

	/* Activate more hook */
	plog_aux = hook_plog;

	/* It's too early to set this, but cannot do so elsewhere XXX XXX */
	game_in_progress = TRUE;

#endif /* !SAVEFILE_SCREEN */

	/* Success */
	return (0);
}

#endif /* USE_GTK2 */
