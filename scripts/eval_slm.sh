#!/data/data/com.termux/files/usr/bin/bash
# Etapa 4: evalúa un modelo GGUF con las preguntas de tests/prompts_slm.txt.
# Uso: eval_slm.sh <modelo.gguf> <hilos> <salida.tsv> [prefijo_asistente] [max_tokens=96] [ctx=1024]
# Variable EXTRA: opciones adicionales para llama-completion (p. ej. EXTRA=-nr desactiva el repack).
# Salida TSV: n, ms, rss_kb, tokens_generados, tok_s_generacion, pregunta, respuesta (en una línea).
set -u
MODELO="${1:?modelo}"; HILOS="${2:?hilos}"; SALIDA="${3:?salida}"
PREFIJO="${4:-}"; NMAX="${5:-96}"; CTX="${6:-1024}"
PREGUNTAS="${PREGUNTAS:-$HOME/jarvis/tests/prompts_slm.txt}"
BIN="$HOME/jarvis/bin/llama-completion"
TMP="$HOME/jarvis/tmp/eval_slm"
SISTEMA="Eres Jarvis-son, un asistente de voz. Responde siempre en español latinoamericano neutro, en una a tres frases cortas, sin listas, sin emojis, sin markdown."
mkdir -p "$TMP"
printf 'n\tms\trss_kb\ttokens\ttok_s\tpregunta\trespuesta\n' > "$SALIDA"
n=0
while IFS= read -r PREGUNTA || [ -n "$PREGUNTA" ]; do
  [ -z "$PREGUNTA" ] && continue
  n=$((n + 1))
  printf '<|im_start|>system\n%s<|im_end|>\n<|im_start|>user\n%s<|im_end|>\n<|im_start|>assistant\n%b' \
    "$SISTEMA" "$PREGUNTA" "$PREFIJO" > "$TMP/prompt.txt"
  INI=$(date +%s%3N)
  command time -v "$BIN" -m "$MODELO" -f "$TMP/prompt.txt" -n "$NMAX" -c "$CTX" -t "$HILOS" \
    --temp 0.7 -s 42 -no-cnv --no-display-prompt --no-warmup ${EXTRA:-} \
    > "$TMP/out.txt" 2> "$TMP/err.txt" < /dev/null
  FIN=$(date +%s%3N)
  RSS=$(sed -n 's/.*Maximum resident set size (kbytes): //p' "$TMP/err.txt")
  # Línea de perf: "... common_perf_print: eval time = X ms / N runs ( ... , T tokens per second)"
  EVAL=$(grep ' eval time' "$TMP/err.txt" | grep -v 'prompt eval' | tail -1)
  TOK=$(echo "$EVAL" | sed -n 's/.*\/ *\([0-9]*\) runs.*/\1/p')
  TPS=$(echo "$EVAL" | sed -n 's/.*, *\([0-9.]*\) tokens per second.*/\1/p')
  RESP=$(tr '\n\t' '  ' < "$TMP/out.txt")
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$n" "$((FIN - INI))" "$RSS" "$TOK" "$TPS" "$PREGUNTA" "$RESP" >> "$SALIDA"
  echo "[$n] $((FIN - INI)) ms, rss=${RSS} kB, ${TOK} tok, ${TPS} tok/s"
done < "$PREGUNTAS"
rm -rf "$TMP"
