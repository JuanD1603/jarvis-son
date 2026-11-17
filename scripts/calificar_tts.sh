#!/data/data/com.termux/files/usr/bin/bash
# Etapa 5: escucha y calificación humana de voces TTS (1-5 en pronunciación, ritmo y naturalidad).
# Uso: calificar_tts.sh [salida.tsv]   Voces: VOCES="es_MX-ald-medium es_MX-claude-high android"
# "android" usa el motor TTS del sistema vía termux-tts-speak (fallback del plan; no genera WAV).
set -u
J="$HOME/jarvis"
SALIDA="${1:-$J/logs/tts_calificacion.tsv}"
VOCES="${VOCES:-es_MX-ald-medium es_MX-claude-high es_AR-daniela-high android}"
FRASES=(
  "Hola, soy Jarvis-son, tu asistente de voz. ¿En qué te puedo ayudar?"
  "La capital de Colombia es Bogotá, una ciudad ubicada a más de dos mil metros de altura."
  "Para dormir mejor, intenta acostarte a la misma hora y evita las pantallas antes de dormir."
)
WAV="$J/tmp/calificar.wav"
trap 'rm -f "$WAV"' EXIT

pedir() {  # $1 pregunta -> número 1-5
  local r
  while true; do
    read -r -p "   $1 (1-5): " r < /dev/tty
    case "$r" in [1-5]) echo "$r"; return;; esac
  done
}

[ -f "$SALIDA" ] || printf 'voz\tfrase\tpronunciacion\tritmo\tnaturalidad\tms_sintesis\tdur_audio_s\n' > "$SALIDA"
clear
echo "=== Calificación de voz Jarvis-son ==="
echo "Escucharás 3 frases con cada voz (A, B, C...). Califica de 1 (muy malo) a 5 (excelente)."
echo "Sube el volumen multimedia. Enter para empezar."
read -r _ < /dev/tty
LETRAS=(A B C D E F)
k=0
LEYENDA=""
for V in $VOCES; do
  letra=${LETRAS[$k]}; k=$((k + 1)); LEYENDA="$LEYENDA $letra=$V"
  D="$J/models/tts/vits-piper-$V"
  echo
  echo "##### VOZ $letra #####"
  for i in "${!FRASES[@]}"; do
    F="${FRASES[$i]}"
    INI=$(date +%s%3N)
    DUR=""
    if [ "$V" != "android" ]; then
      "$J/bin/tts.sh" --vits-model="$D/$V.onnx" --vits-tokens="$D/tokens.txt" --vits-data-dir="$D/espeak-ng-data" \
        --num-threads=4 --output-filename="$WAV" "$F" > /dev/null 2>&1
      DUR=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$WAV")
    fi
    FIN=$(date +%s%3N)
    while true; do
      echo
      echo " Frase $((i + 1)): $F"
      if [ "$V" = "android" ]; then termux-tts-speak -l es -n MX "$F"; else play-audio "$WAV"; fi
      read -r -p "   ¿Repetir? (r + Enter = repetir, Enter = calificar): " R < /dev/tty
      [ "$R" = "r" ] || break
    done
    P=$(pedir "Pronunciación"); RI=$(pedir "Ritmo"); N=$(pedir "Naturalidad")
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$V" "$((i + 1))" "$P" "$RI" "$N" "$((FIN - INI))" "$DUR" >> "$SALIDA"
  done
done
echo
read -r -p "¿Qué voz prefieres para Jarvis-son? (letra): " PREF < /dev/tty
echo "# preferida: $PREF ($LEYENDA )" >> "$SALIDA"
echo "FIN_CALIFICACION. Gracias."
