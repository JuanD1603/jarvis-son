# Problemas conocidos y soluciones

Observados durante la instalación en el Redmi Note 10S (MIUI 14). Detalle cronológico en `bitacora/BITACORA.md`.

| Síntoma | Causa | Solución aplicada |
|---|---|---|
| `adb install` -> `INSTALL_FAILED_VERIFICATION_FAILURE: Install not allowed` | MIUI exige "Instalar vía USB" y además confirmar cada instalación con huella o PIN (`AdbInstallActivity`) | Activar "Instalar vía USB" (requiere cuenta Mi) y autenticar en el teléfono al instalar. Un agente no debe introducir el PIN |
| `input`, `settings put`, `svc power stayon` -> `SecurityException` o código 137 | Falta "Depuración USB (ajustes de seguridad)" | Activarla y **reconectar el cable**: el ajuste no se aplica a la sesión ADB abierta |
| El teléfono desaparece de `adb devices` tras cambiar ajustes de depuración | MIUI revoca la sesión USB | Desconectar/conectar el cable y volver a autorizar |
| Git Bash convierte `/data/...` en `C:/Program Files/Git/data/...` | Conversión de rutas de MSYS | `export MSYS_NO_PATHCONV=1` (en `scripts/env_pc.sh`) |
| `clang -march=native` no activa dotprod en Termux | clang resuelve `native` a CPU `generic` en este kernel | Banderas explícitas `armv8.2-a+dotprod+fp16` (ADR-005) |
| `ninja: unknown target 'llama-cli'` | El objetivo cambió en llama.cpp reciente | Usar `llama-completion` (ADR-007) |
| Qwen2.5-1.5B usa 2,0 GB de RSS | El repack de pesos Q4 duplica los pesos en RAM | `-nr` (sin repack): 1,12 GiB (ADR-006) |
| Velocidad de generación baja de ~10 a ~7 tok/s tras varias preguntas | Calentamiento / reparto de hilos big.LITTLE | Usar 2 hilos; dejar enfriar entre sesiones largas |
| `pkill -f patron` por SSH cierra la propia sesión SSH | El patrón coincide con la línea de comando del shell remoto | Usar PID concretos o `pgrep -x` |
| Ningún SLM probado resuelve bien aritmética simple ("15 x 4") | Límite de modelos de 0,5-1,5 B cuantizados | Limitación documentada; no se usa el asistente para cálculos |
| La primera palabra se pierde al grabar | `termux-microphone-record` tarda ~300 ms en empezar | Aviso "HABLA AHORA" cuando el archivo ya crece (ADR-012) |
| `moov atom not found` al convertir el m4a | Termux:API cierra el archivo de forma asíncrona tras `-q` | Esperar a que `ffprobe` lo lea |
| `adb shell input text '~/...'` escribe `//...` | `input text` no transmite `~` | Enlace `$PREFIX/bin/jarvis`; escribir solo `jarvis` |
| Los archivos del repositorio aparecen con CRLF en Windows | Un editor del PC reescribió los finales de línea | `.gitattributes` con `eol=lf`; normalizar con `sed -i 's/\r$//'` antes de copiar al teléfono |
| sherpa-onnx v2.x no sintetiza con voces Piper (`phoneme_codepoints is empty`) | v2 eliminó espeak-ng integrado | Usar v1.13.8 (ADR-008) |
| Respuesta del SLM vacía en algún turno | El modelo emite fin de texto de inmediato (no determinista aunque la semilla sea fija) | Se dice "No tengo una respuesta para eso." |
| Turnos de 40-50 s con respuestas largas | Más tokens + TTS `high` lento | Reducir `slm.max_tokens` a 64 o usar `es_MX-claude-high` |
| TTS falla con `cannot locate symbol "OrtGetApiBase"` tras `pkg upgrade` o tras ejecutar de nuevo el instalador | onnxruntime versiona sus símbolos (`VERS_1.32.0`); un `sherpa-onnx-offline-tts` compilado contra otra versión deja de enlazar | Reinstalar la versión con la que se compiló (`dpkg -i` del `.deb` en la caché de apt) o recompilar sherpa-onnx; para evitarlo, `apt-mark hold onnxruntime` (2026-11-23, Fase M) |
