#!/usr/bin/env bash
# Se ejecuta DENTRO del contenedor de toolchain, con el repo montado en /work.
# Compila ToME 2.2.2 (+tiles TomeTik) eligiendo frontend con el 1er argumento:
#   x11   -> makefile.std  (X11 + curses, modo texto)        [por defecto]
#   gtk2  -> makefile.gtk2 (GTK2 multi-ventana + tiles + X11/curses fallback)
#
# Los makefiles ya están adaptados a Debian (ncurses, sin /usr/X11R6, flags
# -std=gnu89 -fcommon -DL64). Aquí solo disparamos el build y resumimos errores.
set -uo pipefail

FRONTEND="${1:-x11}"
case "${FRONTEND}" in
    x11)  MK=makefile.std  ;;
    gtk2) MK=makefile.gtk2 ;;
    *) echo "frontend desconocido: ${FRONTEND} (usa x11|gtk2)"; exit 2 ;;
esac

SRC=/work/src
LOG=/work/docker/build.log

cd "${SRC}" || { echo "no existe ${SRC}"; exit 1; }

echo "=== frontend=${FRONTEND}  makefile=${MK} ===" | tee "${LOG}"
echo "=== limpiando objetos previos ===" | tee -a "${LOG}"
make -f "${MK}" clean 2>&1 | tee -a "${LOG}"
# tolua y los stubs w_*.c también, para forzar regeneración limpia.
rm -f ./tolua w_*.c 2>/dev/null || true

# Paso 1: construir el generador 'tolua' EN SERIE. El makefile no declara
# './tolua' como dependencia de las reglas que generan w_*.c, así que con -j
# esas reglas corren antes de que el binario exista (carrera). Lo forzamos antes.
echo "=== make -f ${MK} ./tolua (serie) ===" | tee -a "${LOG}"
make -f "${MK}" ./tolua 2>&1 | tee -a "${LOG}"

echo "=== make -f ${MK} mini_install (jN) ===" | tee -a "${LOG}"
# mini_install: genera w_*.c con ./tolua, compila 'tome' y lo copia a ..
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
