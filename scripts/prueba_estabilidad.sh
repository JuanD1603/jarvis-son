#!/data/data/com.termux/files/usr/bin/bash
# Fase J: N turnos consecutivos con --wav (sin humano). Criterio: todos con codigo 0, sin OOM.
# Uso: prueba_estabilidad.sh [n=10] [etiqueta=estabilidad]
set -u
J="$HOME/jarvis"; N="${1:-10}"; ET="${2:-estabilidad}"
LOG="$J/logs/prueba_$ET.log"
{
  echo "== $ET $(date '+%F %T')"
  echo "-- free -m antes"; free -m
  echo "-- bateria antes"; termux-battery-status 2>/dev/null | grep -E 'percentage|temperature'
  ID0=$(($(wc -l < "$J/logs/turnos.csv" 2>/dev/null || echo 1)))
  for i in $(seq 1 "$N"); do
    W=$(printf '%s/tests/wav/frase_%02d.wav' "$J" $(( (i - 1) % 20 + 1 )))
    "$J/bin/jarvis" --wav "$W" | grep -E 'Tú:|Jarvis-son:|ERROR|causa'
  done
  echo "-- free -m despues"; free -m
  echo "-- bateria despues"; termux-battery-status 2>/dev/null | grep -E 'percentage|temperature'
  echo "-- id_inicial=$ID0"
} 2>&1 | tee "$LOG"
