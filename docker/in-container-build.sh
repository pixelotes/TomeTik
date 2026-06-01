#!/usr/bin/env bash
# Se ejecuta DENTRO del contenedor de toolchain, con el repo montado en /work.
# Compila ToME 2.2.2 (+tiles TomeTik) eligiendo frontend con el 1er argumento:
#   x11      -> makefile.std   (X11 + curses, modo texto)        [por defecto]
#   gtk2     -> makefile.gtk2  (GTK2 multi-ventana + tiles + X11/curses fallback)
#   windows  -> makefile.mingw (frontend GDI, cross-compile Win64 con mingw-w64)
#
# Los makefiles ya están adaptados (flags -std=gnu89 -fcommon; -DL64 solo en
# Linux LP64, NO en Win64). Aquí disparamos el build y resumimos errores.
set -uo pipefail

FRONTEND="${1:-x11}"
SRC=/work/src
LOG=/work/docker/build.log

cd "${SRC}" || { echo "no existe ${SRC}"; exit 1; }

# ---------------------------------------------------------------------------
# Windows (mingw-w64): tolua es un binario del HOST. Generamos ./tolua y los
# w_*.c con el toolchain NATIVO (makefile.std), y luego cross-compilamos con
# makefile.mingw (que asume los w_*.c ya presentes y no toca tolua).
# ---------------------------------------------------------------------------
if [ "${FRONTEND}" = "windows" ]; then
    echo "=== frontend=windows  makefile=makefile.mingw ===" | tee "${LOG}"
    echo "=== limpiando ===" | tee -a "${LOG}"
    make -f makefile.mingw clean 2>&1 | tee -a "${LOG}"
    make -f makefile.std clean 2>&1 | tee -a "${LOG}"
    rm -f ./tolua w_*.c 2>/dev/null || true

    echo "=== tolua nativo + generación de w_*.c ===" | tee -a "${LOG}"
    make -f makefile.std ./tolua 2>&1 | tee -a "${LOG}"
    # Generar todos los stubs Lua con el tolua nativo.
    make -f makefile.std w_mnster.c w_player.c w_play_c.c w_z_pack.c \
         w_obj.c w_util.c w_spells.c w_quest.c w_dun.c 2>&1 | tee -a "${LOG}"

    # CLAVE: el paso anterior compiló *.o y lua/*.o con el gcc NATIVO (arm64).
    # Hay que borrarlos para que mingw los recompile a PE/COFF; si no, el link
    # falla con "Relocations in generic ELF (EM: 183)". El binario ./tolua ya
    # está enlazado y los w_*.c ya generados, así que es seguro borrar los .o.
    rm -f *.o lua/*.o iso/*.o 2>/dev/null || true

    echo "=== cross-compile tometik.exe (jN) ===" | tee -a "${LOG}"
    make -f makefile.mingw -j"$(nproc)" -k 2>&1 | tee -a "${LOG}"
    BUILD_RC=${PIPESTATUS[0]}

    # Empaquetar junto a lib/ para distribución.
    [ -f "${SRC}/tometik.exe" ] && cp -f "${SRC}/tometik.exe" /work/tometik.exe

    echo "=== RESUMEN ===" | tee -a "${LOG}"
    echo "build rc=${BUILD_RC}" | tee -a "${LOG}"
    ls -la "${SRC}/tometik.exe" /work/tometik.exe 2>/dev/null | tee -a "${LOG}"
    echo "--- nº de errores ---" | tee -a "${LOG}"
    grep -cE ":[0-9]+:[0-9]+: error:" "${LOG}" | tail -1
    echo "--- primeros 40 errores ---" | tee -a "${LOG}"
    grep -E ":[0-9]+:[0-9]+: error:|undefined reference|ld returned" "${LOG}" | head -40
    exit "${BUILD_RC}"
fi

# ---------------------------------------------------------------------------
# Linux (X11 / GTK2)
# ---------------------------------------------------------------------------
case "${FRONTEND}" in
    x11)  MK=makefile.std  ;;
    gtk2) MK=makefile.gtk2 ;;
    *) echo "frontend desconocido: ${FRONTEND} (usa x11|gtk2|windows)"; exit 2 ;;
esac

echo "=== frontend=${FRONTEND}  makefile=${MK} ===" | tee "${LOG}"
echo "=== limpiando objetos previos ===" | tee -a "${LOG}"
make -f "${MK}" clean 2>&1 | tee -a "${LOG}"
# Limpieza a fondo: el clean de los makefiles NO borra lua/*.o, y un build de
# windows previo deja ahí objetos mingw (i686 PE) que romperían el link nativo
# ("file in wrong format"). Borramos todos los .o, los stubs y tolua.
rm -f ./tolua tometik.exe w_*.c *.o lua/*.o iso/*.o 2>/dev/null || true

# Paso 1: construir el generador 'tolua' EN SERIE. El makefile no declara
# './tolua' como dependencia de las reglas que generan w_*.c, así que con -j
# esas reglas corren antes de que el binario exista (carrera). Lo forzamos antes.
echo "=== make -f ${MK} ./tolua (serie) ===" | tee -a "${LOG}"
make -f "${MK}" ./tolua 2>&1 | tee -a "${LOG}"

# Paso 2: generar los stubs w_*.c con ./tolua EN SERIE, antes del -j. Las reglas
# que compilan w_*.o NO declaran dependencia del fichero w_*.c generado, así que
# con -j el compilador puede arrancar sobre un w_*.c aún inexistente ("No such
# file"). Generándolos antes evitamos esa carrera (mismo patrón que la ruta
# windows de más arriba).
echo "=== make -f ${MK} w_*.c (serie) ===" | tee -a "${LOG}"
make -f "${MK}" w_mnster.c w_player.c w_play_c.c w_z_pack.c \
     w_obj.c w_util.c w_spells.c w_quest.c w_dun.c 2>&1 | tee -a "${LOG}"

echo "=== make -f ${MK} mini_install (jN) ===" | tee -a "${LOG}"
# mini_install: compila 'tome' (con los w_*.c ya generados) y lo copia a ..
make -f "${MK}" -j"$(nproc)" -k mini_install 2>&1 | tee -a "${LOG}"
BUILD_RC=${PIPESTATUS[0]}

echo "=== RESUMEN ===" | tee -a "${LOG}"
echo "build rc=${BUILD_RC}" | tee -a "${LOG}"
echo "--- binario producido ---" | tee -a "${LOG}"
ls -la "${SRC}/tome" /work/tome 2>/dev/null | tee -a "${LOG}"
echo "--- nº de errores (formato gcc fichero:linea:col: error:) ---" | tee -a "${LOG}"
grep -cE ":[0-9]+:[0-9]+: error:" "${LOG}" | tail -1
echo "--- primeros 40 errores ---" | tee -a "${LOG}"
grep -E ":[0-9]+:[0-9]+: error:|undefined reference|ld returned" "${LOG}" | head -40

exit "${BUILD_RC}"
