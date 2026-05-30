#!/usr/bin/env bash
# Se ejecuta DENTRO del contenedor (repo en /work). Arranca X virtual + fluxbox
# + x11vnc y lanza TomeTik con el frontend X11, para jugar desde el host (Mac)
# con un cliente VNC. Mismo patrón que ../AngbandPlus/docker/run-vnc.sh.
set -uo pipefail

VNCPASS="${TOMETIK_VNCPASS:-tometik}"
SRC=/work/src

# El binario espera DEFAULT_PATH=./lib/ relativo al cwd, así que corremos desde
# la raíz del repo, donde está lib/.
cd /work || { echo "no existe /work"; exit 1; }

if [ ! -x "${SRC}/tome" ]; then
    echo "No hay binario ${SRC}/tome. Compila antes con ./docker/build.sh"
    exit 1
fi

export HOME=/tmp
export DISPLAY=:99

echo "=== Arrancando Xvfb + fluxbox + x11vnc ==="
Xvfb :99 -screen 0 1280x800x24 -nolisten tcp >/tmp/xvfb.log 2>&1 &
sleep 2
mkdir -p /tmp/.fluxbox
fluxbox >/tmp/fluxbox.log 2>&1 &
sleep 1
# Password (compatible con "Compartir pantalla" de macOS).
x11vnc -storepasswd "${VNCPASS}" /tmp/vncpw >/dev/null 2>&1
# Sin -localhost: bind 0.0.0.0 dentro del contenedor; el host lo limita a
# 127.0.0.1 vía el mapeo de puerto de docker (ver play.sh).
x11vnc -display :99 -forever -shared -rfbauth /tmp/vncpw -rfbport 5900 >/tmp/x11vnc.log 2>&1 &
sleep 1

# noVNC: cliente VNC en el navegador. websockify sirve la web de noVNC en :6080
# y la puentea contra el x11vnc de :5900. El paquete 'novnc' instala la web en
# /usr/share/novnc (vnc.html es el cliente clásico; index.html redirige a él).
NOVNC_WEB=/usr/share/novnc
websockify --web="${NOVNC_WEB}" 6080 localhost:5900 >/tmp/novnc.log 2>&1 &
sleep 1

cat <<EOF

===================================================================
  Listo para jugar (TomeTik / ToME 2.2.2 + tiles).

  Opción A (navegador, recomendada):
    http://localhost:6080/vnc.html?host=localhost&port=6080
    Contraseña:  ${VNCPASS}

  Opción B (cliente VNC nativo, p.ej. Compartir pantalla de macOS):
    vnc://localhost:5900   (contraseña: ${VNCPASS})

  Pulsa Ctrl+C en esta terminal para parar.
===================================================================

EOF

# IMPORTANTE: ejecutar desde la raíz del repo (/work), NO desde src/. El binario
# usa DEFAULT_PATH="./lib/" relativo al cwd; si se corre desde src/ busca
# ./src/lib (inexistente) y falla al cargar datos/módulos Lua (mods_aux.lua...).
# mini_install copia el binario a /work/tome, así que lo lanzamos desde ahí.
#
# Módulo/frontend: -m<modulo>. gtk2 = UI multi-ventana + tiles (replica Windows);
# x11/gcu = texto. Por defecto gtk2; cambia con TOMETIK_MODULE=x11.
MODULE="${TOMETIK_MODULE:-gtk2}"
# TOMETIK_SAVE: nombre del savefile a cargar (-u<who>), p.ej. PLAYER, para
# saltarse la creación de personaje. Vacío = arranque normal.
SAVEARG=""
[ -n "${TOMETIK_SAVE:-}" ] && SAVEARG="-u${TOMETIK_SAVE}"
# TOMETIK_ISO=1 -> modo isométrico (solo gtk2). Los args del frontend van DESPUÉS
# de "--" (main.c para de parsear ahí y pasa el resto a init_gtk2).
ISOARG=""
[ "${TOMETIK_ISO:-0}" = "1" ] && ISOARG="-- -i"
cd /work
./tome -m"${MODULE}" ${SAVEARG} ${ISOARG} 2>&1 | tee /tmp/tome-play.log
