#!/usr/bin/env bash
# Construye la imagen de toolchain (si hace falta) y compila TomeTik dentro,
# montando el repo. Pensado para iterar: editas el código en el host y
# relanzas ./docker/build.sh.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE=tometik-build

cd "${REPO}"

if ! docker image inspect "${IMAGE}" >/dev/null 2>&1; then
    echo "=== construyendo imagen ${IMAGE} (solo la primera vez) ==="
    docker build -t "${IMAGE}" -f docker/Dockerfile docker
fi

# Frontend a compilar: x11 (por defecto) o gtk2.
FRONTEND="${1:-x11}"

echo "=== compilando en contenedor (frontend=${FRONTEND}) ==="
docker run --rm \
    -v "${REPO}:/work" \
    -w /work \
    "${IMAGE}" \
    bash /work/docker/in-container-build.sh "${FRONTEND}"
