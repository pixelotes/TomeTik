# TomeTik — toolchain para compilar ToME 2.2.2 (+tiles) en Linux moderno.
#
# Patrón calcado del proyecto hermano ../AngbandPlus/docker: Debian 12 da
# gcc 12, que aún trata el C de 2003 (implicit-int, decls implícitas) como
# WARNING y no como error. El juego NO se hornea en la imagen: se compila
# montando el repo (ver docker/build.sh) para poder iterar deprisa.
#
# Frontend objetivo de este primer build: X11 + curses (makefile.std), el
# camino más corto a "compila limpio". GTK2 / iso vienen después.
FROM debian:12-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        make \
        gcc \
        libc6-dev \
        libx11-dev \
        libxext-dev \
        libncurses-dev \
        zlib1g-dev \
        ca-certificates \
        pkg-config \
        libgtk2.0-dev \
    && rm -rf /var/lib/apt/lists/*

# Runtime gráfico: X virtual + gestor de ventanas + servidor VNC, para jugar
# desde el host (Mac) con un cliente VNC. Mismo patrón que ../AngbandPlus/docker.
RUN apt-get update && apt-get install -y --no-install-recommends \
        xvfb \
        xauth \
        xfonts-base \
        x11-apps \
        x11vnc \
        fluxbox \
        novnc \
        websockify \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work
