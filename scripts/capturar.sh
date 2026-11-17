#!/data/data/com.termux/files/usr/bin/bash
# Etapa 2: graba un clip con Termux:API y lo normaliza a WAV mono 16 kHz s16le.
# Uso: capturar.sh [segundos=5] [salida=~/jarvis/tmp/in.wav]
# Códigos de salida: 0 ok, 2 grabación fallida, 3 conversión fallida, 4 formato/duración inválidos.
set -u
SEG="${1:-5}"
OUT="${2:-$HOME/jarvis/tmp/in.wav}"
TMPDIR_J="$(dirname "$OUT")"
M4A="$TMPDIR_J/in_$$.m4a"
mkdir -p "$TMPDIR_J"

limpiar() { rm -f "$M4A"; }
trap limpiar EXIT

case "$SEG" in ''|*[!0-9]*) echo "segundos invalidos: $SEG" >&2; exit 4;; esac

# termux-microphone-record es asíncrono: inicia la grabación y retorna de inmediato.
termux-microphone-record -q >/dev/null 2>&1 || true
# -l es solo un tope de seguridad (cuenta desde el arranque): la grabación la detiene "-q" tras
# SEG segundos contados desde el aviso.
if ! termux-microphone-record -f "$M4A" -l $((SEG + 3)) -e aac >/dev/null 2>&1; then
  echo "ERROR: no se pudo iniciar la grabación (permiso RECORD_AUDIO de Termux:API?)" >&2
  exit 2
fi
# El micrófono tarda ~300 ms en empezar a escribir datos: el aviso "HABLA AHORA" se da solo cuando
# el archivo ya crece, para no perder la primera palabra (medido en bitacora, Fase J).
for _ in $(seq 1 40); do [ -s "$M4A" ] && break; sleep 0.05; done
sleep 0.15
if [ "${CAPTURA_AVISO:-1}" = "1" ]; then
  { printf '   >>> HABLA AHORA (%s s) <<<\n' "$SEG" > /dev/tty; } 2>/dev/null || printf 'HABLA AHORA\n' >&2
  termux-vibrate -d 60 -f >/dev/null 2>&1 &
fi
sleep "$SEG"
termux-microphone-record -q >/dev/null 2>&1 || true
# Espera a que el contenedor m4a quede cerrado.
# Tras "-q" Termux:API escribe el índice (moov) de forma asíncrona: se espera hasta que sea legible.
for _ in $(seq 1 40); do
  [ -s "$M4A" ] && ffprobe -v quiet "$M4A" && break
  sleep 0.1
done
[ -s "$M4A" ] || { echo "ERROR: archivo de grabación vacío" >&2; exit 2; }

if ! ffmpeg -nostdin -loglevel error -y -i "$M4A" -ac 1 -ar 16000 -c:a pcm_s16le "$OUT"; then
  echo "ERROR: ffmpeg no pudo convertir el audio" >&2
  exit 3
fi

INFO="$(ffprobe -v error -select_streams a:0 -show_entries stream=channels,sample_rate,sample_fmt:format=duration -of default=nw=1 "$OUT")"
CANALES="$(echo "$INFO" | sed -n 's/^channels=//p')"
TASA="$(echo "$INFO" | sed -n 's/^sample_rate=//p')"
FMT="$(echo "$INFO" | sed -n 's/^sample_fmt=//p')"
DUR="$(echo "$INFO" | sed -n 's/^duration=//p')"
if [ "$CANALES" != "1" ] || [ "$TASA" != "16000" ] || [ "$FMT" != "s16" ]; then
  echo "ERROR: formato inesperado ($INFO)" >&2; exit 4
fi
if ! awk -v d="$DUR" 'BEGIN{exit !(d>=1 && d<=11)}'; then
  echo "ERROR: duración fuera de rango 1-10 s ($DUR)" >&2; exit 4
fi
echo "OK $OUT canales=$CANALES tasa=$TASA fmt=$FMT duracion=$DUR"
