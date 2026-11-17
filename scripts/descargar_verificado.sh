#!/data/data/com.termux/files/usr/bin/bash
# Descarga un archivo y verifica su SHA-256. Si ya existe con el hash correcto, no lo descarga de nuevo.
# Uso: descargar_verificado.sh <url> <destino> <sha256>
# Salida: 0 ok, 1 argumentos, 2 descarga fallida, 3 hash distinto (el archivo se elimina).
set -u
URL="${1:?url}"; DEST="${2:?destino}"; SHA="${3:?sha256}"
mkdir -p "$(dirname "$DEST")"
if [ -f "$DEST" ] && [ "$(sha256sum "$DEST" | cut -d' ' -f1)" = "$SHA" ]; then
  echo "YA_EXISTE $DEST"; exit 0
fi
# -C - reanuda descargas parciales; se reintenta ante cortes de red.
if ! curl -fL --retry 5 --retry-delay 5 -C - -o "$DEST.part" "$URL"; then
  echo "ERROR descarga $URL" >&2; exit 2
fi
REAL="$(sha256sum "$DEST.part" | cut -d' ' -f1)"
if [ "$REAL" != "$SHA" ]; then
  echo "ERROR sha256 esperado=$SHA obtenido=$REAL" >&2; rm -f "$DEST.part"; exit 3
fi
mv "$DEST.part" "$DEST"
echo "OK $DEST $(stat -c %s "$DEST") bytes sha256=$REAL"
