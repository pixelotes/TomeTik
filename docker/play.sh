#!/usr/bin/env bash
# Lanzador en el HOST: arranca TomeTik en el contenedor y lo expone por VNC en
# localhost:5900. Conéctate luego con cualquier cliente VNC (en Mac: Finder ->
# Cmd+K -> vnc://localhost:5900, contraseña 'tometik').
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE=tometik-build

cd "${REPO}"

if ! docker image inspect "${IMAGE}" >/dev/null 2>&1; then
    echo "=== construyendo imagen ${IMAGE} ==="
    docker build -t "${IMAGE}" -f docker/Dockerfile docker
fi

echo "=== arrancando TomeTik + VNC/noVNC (Ctrl+C para parar) ==="
# Puertos solo accesibles desde el propio Mac (127.0.0.1):
#   6080 -> noVNC (navegador),  5900 -> VNC nativo.
docker run --rm -it \
    --name tometik-play \
    -p 127.0.0.1:6080:6080 \
    -p 127.0.0.1:5900:5900 \
    -v "${REPO}:/work" \
    -w /work \
    "${IMAGE}" \
    bash /work/docker/run-vnc.sh
