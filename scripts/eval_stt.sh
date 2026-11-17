#!/data/data/com.termux/files/usr/bin/bash
# Etapa 3: evaluación real de STT con voz humana. Se ejecuta en la ventana de Termux del teléfono.
# Uso: eval_stt.sh [frases.txt] [salida.tsv] [condicion=silencio|ruido] [segundos=6]
# Fase 1: muestra cada frase, espera Enter y graba (sin esperas entre frases).
# Fase 2: transcribe todas las grabaciones con cada configuración de CONFIGS y borra los audios.
# Solo se conservan textos (frases predefinidas) y métricas.
set -u
J="$HOME/jarvis"
FRASES="${1:-$J/tests/frases_stt.txt}"
SALIDA="${2:-$J/logs/eval_stt.tsv}"
COND="${3:-silencio}"
SEG="${4:-6}"
HILOS="${HILOS:-4}"
# modelo:contexto_de_audio (1500 = ventana completa de 30 s; 512 = ~10 s)
CONFIGS="${CONFIGS:-tiny:1500 base:512 base:1500}"
PISTA="${PISTA:-Hola, Jarvis.}"  # misma pista de vocabulario que usa config.ini
DIR="$J/tmp/eval_stt_$COND"
rm -rf "$DIR"; mkdir -p "$DIR"
trap 'rm -rf "$DIR"' EXIT

[ -f "$SALIDA" ] || printf 'id\tcondicion\treferencia\tdur_s\tconfig\tms\trss_kb\thipotesis\n' > "$SALIDA"
clear
echo "=== Evaluación STT Jarvis-son ($COND) ==="
echo "Lee cada frase en voz alta, con tono normal, a unos 20-30 cm del teléfono."
echo "Enter = grabar ($SEG s) | s + Enter = saltar | q + Enter = terminar"
mapfile -t LISTA < <(grep -v '^[[:space:]]*$' "$FRASES")
TOTAL=${#LISTA[@]}
for i in "${!LISTA[@]}"; do
  n=$((i + 1)); NN=$(printf '%02d' "$n")
  echo
  echo "[$n/$TOTAL]  ${LISTA[$i]}"
  read -r -p "Enter para grabar > " R < /dev/tty
  [ "$R" = "q" ] && break
  [ "$R" = "s" ] && continue
  echo "   Preparando micrófono... espera el aviso HABLA AHORA"
  if "$J/bin/capturar.sh" "$SEG" "$DIR/$NN.wav" > /dev/null; then
    echo "   ok"
  else
    echo "   error de captura; frase omitida"
  fi
done

echo
echo "Grabación terminada. Transcribiendo (puede tardar unos minutos, no hace falta hacer nada)..."
for i in "${!LISTA[@]}"; do
  NN=$(printf '%02d' "$((i + 1))"); W="$DIR/$NN.wav"
  [ -f "$W" ] || continue
  DUR=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$W")
  for C in $CONFIGS; do
    M="${C%%:*}"; AC="${C##*:}"
    INI=$(date +%s%3N)
    command time -v "$J/bin/whisper-cli" -m "$J/models/stt/ggml-$M-q5_1.bin" -l es -t "$HILOS" -ac "$AC" \
      --prompt "$PISTA" -nt -np -otxt -of "$DIR/out" -f "$W" > /dev/null 2> "$DIR/err" < /dev/null
    FIN=$(date +%s%3N)
    RSS=$(sed -n 's/.*Maximum resident set size (kbytes): //p' "$DIR/err")
    HIP=$(tr '\n\t' '  ' < "$DIR/out.txt" 2>/dev/null | sed 's/^ *//; s/ *$//')
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$NN" "$COND" "${LISTA[$i]}" "$DUR" "$C" "$((FIN - INI))" "$RSS" "$HIP" >> "$SALIDA"
    rm -f "$DIR/out.txt" "$DIR/err"
  done
  rm -f "$W"
  echo "   $NN listo"
done
echo
echo "FIN_EVAL_STT ($COND). Resultados en $SALIDA. Audios borrados."
