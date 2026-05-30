#!/usr/bin/env bash
# Empaqueta los artefactos de release de TomeTik.
# Espera (ejecutado desde la raíz del repo) los binarios ya compilados en dist/:
#   dist/tome-x11   dist/tome-gtk2   dist/tometik.exe
# Produce en dist/ los archivos versionados listos para subir a la Release:
#   tometik-<V>-linux-x11.tar.gz
#   tometik-<V>-linux-gtk2.tar.gz
#   tometik-<V>-windows.zip
set -euo pipefail

V="${1:?uso: package-release.sh <version>}"
ROOT="$(pwd)"
OUT="$ROOT/dist"
mkdir -p "$OUT"

# Copia lib/ a $1 sin datos derivados ni partidas: los .raw dependen del
# arch/SO, así que mejor que cada plataforma los regenere en el primer arranque
# (el juego lo hace solo). Tampoco distribuimos savefiles ni scores.
stage_lib() {
	cp -a "$ROOT/lib" "$1/lib"
	rm -f "$1"/lib/data/*.raw 2>/dev/null || true
	find "$1/lib/save" -type f ! -name 'delete.me' -delete 2>/dev/null || true
	rm -f "$1"/lib/apex/*.raw 2>/dev/null || true
}

mklauncher() {  # $1 dir destino, $2 módulo (x11|gtk2)
	cat > "$1/play.sh" <<EOF
#!/usr/bin/env bash
# Lanza TomeTik desde esta carpeta (el binario usa ./lib relativo al cwd).
cd "\$(dirname "\$0")"
exec ./tome -m$2 "\$@"
EOF
	chmod +x "$1/play.sh"
}

# --- Linux X11 (texto) ---
D="$OUT/tometik-$V-linux-x11"; rm -rf "$D"; mkdir -p "$D"
install -m755 "$OUT/tome-x11" "$D/tome"
stage_lib "$D"; mklauncher "$D" x11
tar -C "$OUT" -czf "$OUT/tometik-$V-linux-x11.tar.gz" "tometik-$V-linux-x11"
rm -rf "$D"

# --- Linux GTK2 (multiventana + tiles) ---
D="$OUT/tometik-$V-linux-gtk2"; rm -rf "$D"; mkdir -p "$D"
install -m755 "$OUT/tome-gtk2" "$D/tome"
stage_lib "$D"; mklauncher "$D" gtk2
tar -C "$OUT" -czf "$OUT/tometik-$V-linux-gtk2.tar.gz" "tometik-$V-linux-gtk2"
rm -rf "$D"

# --- Windows (GDI, 32-bit) ---
D="$OUT/tometik-$V-windows"; rm -rf "$D"; mkdir -p "$D"
cp "$OUT/tometik.exe" "$D/tometik.exe"
stage_lib "$D"
( cd "$OUT" && zip -qr "tometik-$V-windows.zip" "tometik-$V-windows" )
rm -rf "$D"

echo "=== artefactos generados ==="
ls -la "$OUT"/*.tar.gz "$OUT"/*.zip
