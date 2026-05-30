#!/usr/bin/env bash
# Entrypoint de la imagen runtime distribuible (docker/Dockerfile.runtime).
# Arranca X virtual + fluxbox + x11vnc + noVNC y lanza TomeTik. Pensado para:
#   docker run -p 127.0.0.1:6080:6080 ghcr.io/<owner>/tometik
# y conectarse desde el navegador a http://localhost:6080/vnc.html
set -uo pipefail

VNCPASS="${TOMETIK_VNCPASS:-tometik}"
MODULE="${TOMETIK_MODULE:-gtk2}"   # gtk2 (tiles+multiventana) | x11 | gcu

export HOME=/tmp
export DISPLAY=:99

Xvfb :99 -screen 0 1280x800x24 -nolisten tcp >/tmp/xvfb.log 2>&1 &
sleep 2
mkdir -p /tmp/.fluxbox
fluxbox >/tmp/fluxbox.log 2>&1 &
sleep 1
x11vnc -storepasswd "${VNCPASS}" /tmp/vncpw >/dev/null 2>&1
x11vnc -display :99 -forever -shared -rfbauth /tmp/vncpw -rfbport 5900 >/tmp/x11vnc.log 2>&1 &
websockify --web=/usr/share/novnc 6080 localhost:5900 >/tmp/novnc.log 2>&1 &
sleep 1

cat <<EOF

===================================================================
  TomeTik listo. Abre en el navegador:
    http://localhost:6080/vnc.html?autoconnect=true&password=${VNCPASS}
  (o cliente VNC nativo: localhost:5900, contraseña ${VNCPASS})
===================================================================

EOF

# El binario usa DEFAULT_PATH=./lib/ relativo al cwd -> ejecutar desde /game.
cd /game
exec ./tome -m"${MODULE}"
