#!/data/data/com.termux/files/usr/bin/bash
# Envoltorio de sherpa-onnx-offline-tts: fija LD_LIBRARY_PATH y reenvía los argumentos sin reinterpretarlos.
TTS_DIR="${TTS_DIR:-$HOME/jarvis/tts}"
export LD_LIBRARY_PATH="$TTS_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$TTS_DIR/bin/sherpa-onnx-offline-tts" "$@"
