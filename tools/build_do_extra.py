#!/usr/bin/env python3
"""
Ensambla lib/xtra/iso/do_extra.png a partir de tiles isométricos de Dungeon Odyssey.

TomeTik usa dg_iso32.gif (de OmnibandTk) para el terreno iso, pero le faltan varias
features. Dungeon Odyssey (abandonware) aporta tiles iso 54x54 (y algunos overlays
32x32) que rellenan esos huecos. Este script empaqueta los elegidos en una hoja de
7 columnas, con fondo magenta (#FF00FF) como color clave, y la guarda en
lib/xtra/iso/do_extra.png.

El frontend GTK2 (src/main-gtk2.c) la carga en `do_sheet` (magenta -> alfa) y la
blitea con offset -5 en Y (DO 54px vs dg_iso32 49px). El ORDEN de esta lista define
los índices del enum DO_* en main-gtk2.c -> NO reordenar sin actualizar el enum.

Tipos de entrada:
  ('copy', fichero)                      -> tile iso 54x54 tal cual
  ('pad',  fichero, (cx,cy))             -> overlay 32x32 centrado en (cx,cy) sobre
                                            fondo magenta (p.ej. glyphs de suelo)
  ('comp', base, overlay, (ox,oy), gif)  -> base 54x54 + overlay encima en (ox,oy)
                                            (gif=True para poof.gif con transp. índice)

Uso:  python3 tools/build_do_extra.py [RUTA_DUNGEONODYSSEY]
      (por defecto ./dungeonodyssey)
"""
import sys, os
from PIL import Image

ROOT = sys.argv[1] if len(sys.argv) > 1 else 'dungeonodyssey'
OUT = 'lib/xtra/iso/do_extra.png'
COLS = 7
TW = TH = 54
MAGENTA = (255, 0, 255)

# rutas relativas a ROOT
P = {
    'Fountain':     'Terrain/L2_Fountain01.PNG',
    'PitOpen':      'Terrain/L2_PitOpen.PNG',
    'PoolFire':     'Terrain/L2_PoolFire.PNG',
    'TrapDoor':     'Terrain/L2_TrapDoor.PNG',
    'PitSpikes':    'Terrain/L2_PitSpikesSilver.PNG',
    'AltarNeutral': 'Terrain/L2_AltarNeutral.PNG',
    'Altar04':      'Terrain/L2_Altar04.PNG',
    'AltarGood':    'Terrain/L2_AltarGood.PNG',
    'AltarEvil':    'Terrain/L2_AltarEvil.PNG',
    'Altar05':      'Terrain/L2_Altar05.PNG',
    'PoolPoison':   'Terrain/L2_PoolPoison.PNG',
    'PoolMirky':    'Terrain/L2_PoolMirky.PNG',
    'PoolWater':    'Terrain/L2_PoolWater.PNG',
    'PoolEmbers':   'Terrain/L2_PoolEmbers.PNG',
    'Graveyard':    'XR module/L1_HB_Graveyard01.PNG',
    'DarkWater':    'XR module/L1_HB_Darkwater01.PNG',
    'Terrain027':   'Terrain/L1_Terrain027.PNG',
    'Terrain049':   'Terrain/L1_Terrain049.PNG',
    'Portal':       'XR module/HB_SummoningPortal01.PNG',
    'Floorstone':   'XR module/L1_HB_FloorStone06.PNG',
    'Town':         'Terrain/L2_Town01.PNG',
    'GlyphGreen':   'Items/GlyphGreen.PNG',
    'GlyphRed':     'Items/GlyphRed.PNG',
    'poof':         'Silmar/poof.gif',
}

# índice = posición = enum DO_* en C. (entrada, feature(s) ToME)
TILES = [
    (('copy', 'Fountain'),                          'fountain (2, 15)'),          # 0  DO_FOUNTAIN
    (('copy', 'PitOpen'),                           'dark pit (87)'),             # 1  DO_PIT
    (('copy', 'PoolFire'),                          'great fire (178)/fire (205)'),# 2 DO_FIRE
    (('copy', 'TrapDoor'),                          'trap (17)'),                 # 3  DO_TRAP
    (('copy', 'PitSpikes'),                         'monster trap (175)'),        # 4  DO_MONTRAP
    (('copy', 'AltarNeutral'),                      'Altar of Being (161)'),      # 5  DO_ALTAR_BEING
    (('copy', 'Altar04'),                           'Altar of Winds (162)'),      # 6  DO_ALTAR_WINDS
    (('copy', 'AltarGood'),                         'Altar of Force (163)'),      # 7  DO_ALTAR_FORCE
    (('copy', 'AltarEvil'),                         'Altar of Darkness (164)'),   # 8  DO_ALTAR_DARK
    (('copy', 'Altar05'),                           'Altar of Nature (165)'),     # 9  DO_ALTAR_NATURE
    (('copy', 'PoolPoison'),                        'nether mist (102)'),         # 10 DO_NETHER
    (('copy', 'PoolMirky'),                         'vapour/dense mist (208,210)'),# 11 DO_MIRKY
    (('copy', 'PoolWater'),                         'condensing water (209)'),    # 12 DO_WATER
    (('copy', 'PoolEmbers'),                        'embers (fuego alt.)'),       # 13 DO_EMBERS
    # --- tanda 3 ---
    (('copy', 'Graveyard'),                         'Straight Road 65-70'),       # 14 DO_GRAVEYARD
    (('copy', 'DarkWater'),                         'Straight Road discharged (71)'),# 15 DO_DARKWATER
    (('comp', 'Graveyard', 'poof', (11, 6), True),  'Straight Road exit (72)'),   # 16 DO_GRAVE_POOF
    (('comp', 'DarkWater', 'Terrain027', (0, 0), False),'corrupted Straight Road (73)'),# 17 DO_DARKWATER_CORRUPT
    (('copy', 'Terrain049'),                        'Underground Tunnel (173,204)'),# 18 DO_TUNNEL
    (('copy', 'Portal'),                            'Void Jumpgate (176)'),       # 19 DO_PORTAL
    (('copy', 'Floorstone'),                        'void (183)'),                # 20 DO_FLOORSTONE
    (('copy', 'Town'),                              'town (203)'),                # 21 DO_TOWN
    (('pad',  'GlyphGreen', (27, 37)),              'glyph of warding (3)'),      # 22 DO_GLYPH_GREEN
    (('pad',  'GlyphRed',   (27, 37)),              'explosive rune (64)'),       # 23 DO_GLYPH_RED
    # --- tiles antes sueltos (de lib/xtra/iso/, NO de dungeonodyssey) ---
    (('local', 'building_block.png',     5),        'edificio pueblo'),           # 24 DO_BUILDING
    (('local', 'rubble.png',             0),        'escombros (FEAT_RUBBLE/206)'),# 25 DO_RUBBLE
    (('local', 'grass_flowers.png',      5),        'flores BLANCAS (en juego)'), # 26 DO_FLOWERS
    (('local', 'grass_flowers.old.png',  5),        'flores antiguas (sin usar)'),# 27 DO_FLOWERS_OLD
]


def load_rgba(key, gif=False):
    """Carga un fichero y devuelve RGBA con magenta (y transp. GIF) -> alfa 0."""
    im = Image.open(os.path.join(ROOT, P[key]))
    if gif:
        im = im.convert('RGBA')   # respeta el índice transparente del GIF
    else:
        im = im.convert('RGB').convert('RGBA')
    px = im.load()
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = px[x, y]
            if r > 200 and g < 60 and b > 200:
                px[x, y] = (0, 0, 0, 0)
    return im


def make_tile(spec):
    """Construye un tile RGB 54x54 con fondo magenta a partir de la spec."""
    kind = spec[0]
    cell = Image.new('RGBA', (TW, TH), MAGENTA + (255,))
    if kind == 'copy':
        base = load_rgba(spec[1])
        cell.alpha_composite(base, (0, 0))
    elif kind == 'pad':
        _, key, (cx, cy) = spec
        ov = load_rgba(key)
        cell.alpha_composite(ov, (cx - ov.width // 2, cy - ov.height // 2))
    elif kind == 'comp':
        _, base_k, ov_k, (ox, oy), gif = spec
        cell.alpha_composite(load_rgba(base_k), (0, 0))
        cell.alpha_composite(load_rgba(ov_k, gif), (ox, oy))
    elif kind == 'local':
        # tile fuente en tools/iso-src/ (custom, NO de dungeonodyssey; sacado de
        # lib/xtra/iso/ porque ya está integrado en el atlas y el juego no lo
        # carga suelto). Su transp. puede ser alfa (building_block), magenta
        # (rubble) o CIAN (grass_flowers); normalizamos cian -> alfa 0 para que al
        # componer sobre magenta quede el colorkey del sheet. Offset y: los de
        # 54x49 van a y=5 (casar DO_DY=-5).
        _, fn, yoff = spec
        im = Image.open(os.path.join('tools/iso-src', fn)).convert('RGBA')
        px = im.load()
        for yy in range(im.height):
            for xx in range(im.width):
                r, g, b, a = px[xx, yy]
                if r < 60 and g > 200 and b > 200:
                    px[xx, yy] = (r, g, b, 0)
        cell.alpha_composite(im, (0, yoff))
    else:
        raise SystemExit('spec desconocida: ' + repr(spec))
    return cell.convert('RGB')


def main():
    rows = (len(TILES) + COLS - 1) // COLS
    sheet = Image.new('RGB', (COLS * TW, rows * TH), MAGENTA)
    for i, (spec, desc) in enumerate(TILES):
        tile = make_tile(spec)
        c, r = i % COLS, i // COLS
        sheet.paste(tile, (c * TW, r * TH))
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    sheet.save(OUT)
    print(f"{OUT}: {sheet.size} = {COLS}x{rows} tiles 54x54, {len(TILES)} usados")
    for i, (spec, desc) in enumerate(TILES):
        print(f"  {i:2d}  {spec[1]:12s} -> {desc}")


if __name__ == '__main__':
    main()
