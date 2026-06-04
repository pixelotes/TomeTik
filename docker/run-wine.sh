#!/usr/bin/env bash
# Ejecuta tometik.exe (build Windows) bajo Wine dentro del contenedor amd64,
# expuesto por VNC :5900 + noVNC :6080 (pass: tometik). Se corre desde /work.
#
# Vars:
#   TOMETIK_VNCPASS   contraseña VNC (def. tometik)
# OJO: Wine != Windows fiel; ver docker/Dockerfile.wine.
set -uo pipefail

VNCPASS="${TOMETIK_VNCPASS:-tometik}"
export DISPLAY=:99
export WINEDEBUG="${WINEDEBUG:--all}"

cd /work

if [ ! -f /work/tometik.exe ]; then
    echo "No hay /work/tometik.exe. Compila antes con ./docker/build.sh windows"
    exit 1
fi

echo "=== Arrancando Xvfb + fluxbox + x11vnc + noVNC ==="
Xvfb :99 -screen 0 1280x800x24 -nolisten tcp >/tmp/xvfb.log 2>&1 &
sleep 2
fluxbox >/tmp/fluxbox.log 2>&1 &
sleep 1

mkdir -p "$HOME/.vnc"
x11vnc -storepasswd "$VNCPASS" "$HOME/.vnc/passwd" >/dev/null 2>&1
x11vnc -display :99 -rfbauth "$HOME/.vnc/passwd" -forever -shared -bg -o /tmp/x11vnc.log >/dev/null 2>&1
websockify --web=/usr/share/novnc 6080 localhost:5900 >/tmp/novnc.log 2>&1 &
sleep 1

cat <<EOF

===================================================================
  TomeTik (Windows/GDI) bajo Wine.

  Navegador:  http://localhost:6080/vnc.html   (contraseña: ${VNCPASS})
  VNC:        vnc://localhost:5900              (contraseña: ${VNCPASS})

  OJO: Wine no es Windows fiel (comctl32/tooltips pueden diferir).
  Ctrl+C para parar.
===================================================================

EOF

# Inicializar el prefijo de wine en silencio la primera vez.
wineboot -i >/tmp/wineboot.log 2>&1 || true

# Lanzar el juego (cwd=/work para que encuentre ./lib y tometik.ini).
wine /work/tometik.exe 2>&1 | tee /tmp/wine-tome.log
