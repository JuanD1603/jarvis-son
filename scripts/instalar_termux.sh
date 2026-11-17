#!/data/data/com.termux/files/usr/bin/bash
# Instalación reproducible de Jarvis-son V1 en un Termux limpio (Android arm64).
# Uso (desde la raíz del repositorio clonado en Termux):  bash scripts/instalar_termux.sh
# Variables opcionales: JARVIS_HOME (por defecto ~/jarvis), JOBS (por defecto 4),
#   SOLO_MODELOS=1 (omite compilación), SIN_MODELOS=1 (omite descargas).
# Versiones fijadas: las usadas y medidas en docs/metricas (ver bitacora/BITACORA.md).
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
J="${JARVIS_HOME:-$HOME/jarvis}"
JOBS="${JOBS:-4}"
LLAMA_COMMIT="92d6c8a5caa2c11b3fb49a7a201b568d040be4b2"
WHISPER_COMMIT="d2c6f05a46dacf663e357bcf0ed40f542a72825d"
SHERPA_COMMIT="11afbd009a7f8c08f4bcf2fc1b265d0df4670fbf"  # v1.13.8: ultima 1.x con espeak-ng integrado (ADR-008)
ARM_ARCH="armv8.2-a+dotprod+fp16"  # Helio G95; ajustar si la CPU no tiene dotprod (ver ADR-005)

log() { printf '\n==> %s\n' "$*"; }

clonar_en() {  # $1 url, $2 destino, $3 commit
  if [ ! -d "$2/.git" ]; then
    git init -q "$2"
    git -C "$2" remote add origin "$1"
  fi
  git -C "$2" fetch -q --depth 1 origin "$3"
  git -C "$2" checkout -q FETCH_HEAD
}

log "Paquetes de Termux"
pkg update -y
pkg install -y clang cmake make ninja git ffmpeg termux-api wget curl jq time play-audio python binutils onnxruntime

mkdir -p "$J"/{bin,models/llm,models/stt,models/tts,tmp,logs,src,tests}
cp "$REPO"/scripts/{capturar.sh,descargar_verificado.sh,eval_slm.sh,eval_stt.sh,tts.sh,calificar_tts.sh,generar_wav_prueba.sh,prueba_estabilidad.sh} "$J/bin/"
cp "$REPO"/tests/{prompts_slm.txt,frases_stt.txt,frases_stt_ruido.txt} "$J/tests/"
chmod +x "$J"/bin/*.sh
if [ ! -f "$J/config.ini" ]; then
  sed "s|^home = .*|home = $J|" "$REPO/config/config.ini.ejemplo" > "$J/config.ini"
fi

if [ "${SOLO_MODELOS:-0}" != "1" ]; then
  log "llama.cpp ($LLAMA_COMMIT)"
  clonar_en https://github.com/ggml-org/llama.cpp "$J/src/llama.cpp" "$LLAMA_COMMIT"
  cmake -S "$J/src/llama.cpp" -B "$J/src/llama.cpp/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DGGML_NATIVE=OFF -DGGML_CPU_ARM_ARCH="$ARM_ARCH" -DLLAMA_CURL=OFF \
    -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_SERVER=OFF
  cmake --build "$J/src/llama.cpp/build" -j "$JOBS" --target llama-completion llama-bench
  ln -sf "$J/src/llama.cpp/build/bin/llama-completion" "$J/bin/llama-completion"
  ln -sf "$J/src/llama.cpp/build/bin/llama-bench" "$J/bin/llama-bench"

  log "whisper.cpp ($WHISPER_COMMIT)"
  clonar_en https://github.com/ggml-org/whisper.cpp "$J/src/whisper.cpp" "$WHISPER_COMMIT"
  cmake -S "$J/src/whisper.cpp" -B "$J/src/whisper.cpp/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DGGML_NATIVE=OFF -DGGML_CPU_ARM_ARCH="$ARM_ARCH" -DWHISPER_BUILD_TESTS=OFF -DWHISPER_SDL2=OFF
  cmake --build "$J/src/whisper.cpp/build" -j "$JOBS" --target whisper-cli
  ln -sf "$J/src/whisper.cpp/build/bin/whisper-cli" "$J/bin/whisper-cli"

  log "sherpa-onnx ($SHERPA_COMMIT) con onnxruntime de Termux"
  clonar_en https://github.com/k2-fsa/sherpa-onnx "$J/src/sherpa-onnx" "$SHERPA_COMMIT"
  SHERPA_ONNXRUNTIME_LIB_DIR="$PREFIX/lib" SHERPA_ONNXRUNTIME_INCLUDE_DIR="$PREFIX/include/onnxruntime" \
    cmake -S "$J/src/sherpa-onnx" -B "$J/src/sherpa-onnx/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON -DSHERPA_ONNX_ENABLE_PYTHON=OFF -DSHERPA_ONNX_ENABLE_TESTS=OFF \
    -DSHERPA_ONNX_ENABLE_CHECK=OFF -DSHERPA_ONNX_ENABLE_PORTAUDIO=OFF -DSHERPA_ONNX_ENABLE_WEBSOCKET=OFF \
    -DSHERPA_ONNX_ENABLE_GPU=OFF -DSHERPA_ONNX_ENABLE_BINARY=ON -DSHERPA_ONNX_ENABLE_C_API=OFF \
    -DSHERPA_ONNX_ENABLE_JNI=OFF -DSHERPA_ONNX_USE_PRE_INSTALLED_ONNXRUNTIME_IF_AVAILABLE=ON \
    -DCMAKE_INSTALL_PREFIX="$J/tts"
  cmake --build "$J/src/sherpa-onnx/build" -j "$JOBS" --target sherpa-onnx-offline-tts
  mkdir -p "$J/tts/bin" "$J/tts/lib"
  cp "$J/src/sherpa-onnx/build/bin/sherpa-onnx-offline-tts" "$J/tts/bin/"
  find "$J/src/sherpa-onnx/build" -name '*.so*' -exec cp -P {} "$J/tts/lib/" \;

  log "Orquestador C++"
  cmake -S "$REPO" -B "$J/src/orquestador-build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DJARVIS_ARM_NATIVO=ON
  cmake --build "$J/src/orquestador-build" -j "$JOBS"
  (cd "$J/src/orquestador-build" && ctest --output-on-failure)
  cp "$J/src/orquestador-build/jarvis" "$J/bin/jarvis"
  ln -sf "$J/bin/jarvis" "$PREFIX/bin/jarvis"  # permite abrirlo escribiendo solo "jarvis"
fi

if [ "${SIN_MODELOS:-0}" != "1" ]; then
  log "Modelos (verificados por sha256; ver models/manifest.json)"
  # Lee el manifiesto y descarga cada entrada a su carpeta por componente.
  python3 - "$REPO/models/manifest.json" <<'PY' | while IFS=$'\t' read -r url destino sha; do
import json, sys
carpeta = {"slm": "llm", "stt": "stt", "tts": "tts"}
for m in json.load(open(sys.argv[1], encoding="utf-8"))["modelos"]:
    print(m["url"], carpeta[m["componente"]] + "/" + m["archivo"], m["sha256"], sep="\t")
PY
    # Las voces se guardan descomprimidas: si la carpeta ya existe no se descarga de nuevo.
    case "$destino" in
      *.tar.bz2) [ -d "$J/models/${destino%.tar.bz2}" ] && { echo "YA_EXISTE ${destino%.tar.bz2}"; continue; } ;;
    esac
    "$J/bin/descargar_verificado.sh" "$url" "$J/models/$destino" "$sha"
    case "$destino" in
      *.tar.bz2) tar -xjf "$J/models/$destino" -C "$J/models/tts" && rm -f "$J/models/$destino" ;;
    esac
  done
fi

log "Listo. Ejecuta: $J/bin/jarvis   (Enter para hablar, q para salir)"
