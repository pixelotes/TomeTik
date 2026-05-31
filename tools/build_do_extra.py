#!/usr/bin/env python3
"""
Ensambla lib/xtra/iso/do_extra.png a partir de tiles isométricos de Dungeon Odyssey.

TomeTik usa la lámina dg_iso32.gif (de OmnibandTk) para el terreno iso, pero le
faltan varias features (fuente, altares, fuego, pit, trampa, pools de niebla/agua).
Dungeon Odyssey (abandonware) trae tiles iso 54x54 con esos huecos. Este script
toma los elegidos, los empaqueta en una sola hoja (7 columnas, fondo magenta como
color clave) y la guarda en lib/xtra/iso/do_extra.png.

El frontend GTK2 (src/main-gtk2.c) la carga en `do_sheet` (magenta #FF00FF -> alfa)
y la blitea con offset -5 en Y para alinear el rombo de suelo (DO 54px vs dg 49px).
El ORDEN de esta lista define los índices del enum DO_* en main-gtk2.c -> NO reordenar
sin actualizar el enum.

Uso:  python3 tools/build_do_extra.py [RUTA_TERRAIN]
      (por defecto ./dungeonodyssey/Terrain)
"""
import sys, os
from PIL import Image

SRC = sys.argv[1] if len(sys.argv) > 1 else 'dungeonodyssey/Terrain'
OUT = 'lib/xtra/iso/do_extra.png'
COLS = 7
TW = TH = 54
MAGENTA = (255, 0, 255)

# (fichero DO, feature(s) de ToME que cubre) -- el índice = posición = enum DO_* en C
TILES = [
    ('L2_Fountain01',     'fountain (feat 2, 15)'),         # DO_FOUNTAIN
    ('L2_PitOpen',        'dark pit (87)'),                 # DO_PIT
    ('L2_PoolFire',       'great fire (178) / fire (205)'), # DO_FIRE
    ('L2_TrapDoor',       'trap (17)'),                     # DO_TRAP
    ('L2_PitSpikesSilver','monster trap (175)'),            # DO_MONTRAP
    ('L2_AltarNeutral',   'Altar of Being (161)'),          # DO_ALTAR_BEING
    ('L2_Altar04',        'Altar of Winds (162)'),          # DO_ALTAR_WINDS
    ('L2_AltarGood',      'Altar of Force (163)'),          # DO_ALTAR_FORCE
    ('L2_AltarEvil',      'Altar of Darkness (164)'),       # DO_ALTAR_DARK
    ('L2_Altar05',        'Altar of Nature (165)'),         # DO_ALTAR_NATURE
    ('L2_PoolPoison',     'nether mist (102)'),             # DO_NETHER
    ('L2_PoolMirky',      'vapour/dense mist (208, 210)'),  # DO_MIRKY
    ('L2_PoolWater',      'condensing water (209)'),        # DO_WATER
    ('L2_PoolEmbers',     'embers (fuego alternativo)'),    # DO_EMBERS
]


def main():
    rows = (len(TILES) + COLS - 1) // COLS
    sheet = Image.new('RGB', (COLS * TW, rows * TH), MAGENTA)
    for i, (name, desc) in enumerate(TILES):
        path = os.path.join(SRC, name + '.PNG')
        im = Image.open(path).convert('RGB')
        if im.size != (TW, TH):
            raise SystemExit(f"{name}: tamaño {im.size}, se esperaba (54,54)")
        c, r = i % COLS, i // COLS
        sheet.paste(im, (c * TW, r * TH))
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    sheet.save(OUT)
    print(f"{OUT}: {sheet.size} = {COLS}x{rows} tiles 54x54, {len(TILES)} usados")
    for i, (n, d) in enumerate(TILES):
        print(f"  {i:2d}  {n:22s} -> {d}")


if __name__ == '__main__':
    main()
