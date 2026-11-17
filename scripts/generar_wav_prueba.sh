#!/data/data/com.termux/files/usr/bin/bash
# Genera WAV de prueba (16 kHz, mono, s16) con la voz TTS a partir de tests/frases_stt.txt,
# para pruebas automáticas de regresión TTS -> STT -> SLM sin voz humana.
# Uso: generar_wav_prueba.sh [frases.txt] [carpeta_salida] [voz_dir]
set -u
J="$HOME/jarvis"
FRASES="${1:-$J/tests/frases_stt.txt}"
OUT="${2:-$J/tests/wav}"
VOZ="${3:-$J/models/tts/vits-piper-es_MX-claude-high}"
MODELO="$(ls "$VOZ"/*.onnx | head -1)"
mkdir -p "$OUT"
n=0
while IFS= read -r F || [ -n "$F" ]; do
  [ -z "$F" ] && continue
  n=$((n + 1))
  NN=$(printf '%02d' "$n")
  "$J/bin/tts.sh" --vits-model="$MODELO" --vits-tokens="$VOZ/tokens.txt" \
    --vits-data-dir="$VOZ/espeak-ng-data" --num-threads=4 \
    --output-filename="$OUT/tmp_$NN.wav" "$F" > /dev/null 2>&1 || { echo "fallo TTS en $NN"; continue; }
  ffmpeg -nostdin -loglevel error -y -i "$OUT/tmp_$NN.wav" -ac 1 -ar 16000 -c:a pcm_s16le "$OUT/frase_$NN.wav"
  rm -f "$OUT/tmp_$NN.wav"
  printf '%s\t%s\n' "$NN" "$F" >> "$OUT/indice.tsv.tmp"
done < "$FRASES"
mv "$OUT/indice.tsv.tmp" "$OUT/indice.tsv"
echo "Generados $n WAV en $OUT"
