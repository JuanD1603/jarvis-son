# Registro de decisiones (ADR)

Formato corto: contexto, decisión, alternativas, evidencia. Las cifras marcadas "estimada" provienen de documentos, no de mediciones.

## ADR-001 — Presupuesto de memoria por etapa

- **Contexto:** el plan del docente toma como referencia un móvil de 3 GB; el informe del equipo usa el Redmi Note 10S (6 GB) con presupuesto 2,0 GB (objetivo 1,7 GB). La "RAM virtual" de MIUI es swap y no cuenta.
- **Decisión:** pico de RSS por etapa <= 1,7 GB (máximo tolerado 2,0 GB). Como las etapas son secuenciales (un solo modelo residente a la vez), el límite aplica al **pico de cada etapa**, no a la suma.
- **Alternativas:** sumar STT+SLM+TTS (solo tendría sentido con modelos residentes simultáneos, que V1 no usa).
- **Evidencia:** `MemAvailable` medido en reposo: 2 272 220 kB (inventario) y 1619 MB (`free -m` en Termux con el PC conectado). Mediciones de pico por etapa en `docs/metricas/`.

## ADR-002 — V1 secuencial con orquestador en C++

- **Contexto:** el plan exige "terminar cada proceso antes de iniciar el siguiente" y el criterio "no solapa TTS e inferencia"; el informe del equipo propone streaming productor-consumidor en Python.
- **Decisión:** V1 = secuencial, orquestador en C++ con `fork`/`execvp`/`wait4`. El streaming por cláusulas queda como mejora V2.
- **Alternativas:** streaming por cláusulas (reduce latencia percibida, pero solapa TTS e inferencia y duplica memoria residente); Python (no cumple la rúbrica).
- **Evidencia:** rúbrica del plan (C++ coordinador), criterios de aceptación.

## ADR-003 — Fuente del modelo Qwen3.5-0.8B Q4_0

- **Contexto:** el plan pide el "GGUF oficial Qwen3.5-0.8B Q4_0 (563 MB)". La organización `Qwen` en Hugging Face no publica repositorio GGUF para Qwen3.5-0.8B (la API devuelve error de acceso).
- **Decisión:** usar `ggml-org/Qwen3.5-0.8B-GGUF` (organización oficial de llama.cpp), archivo `Qwen3.5-0.8B-Q4_0.gguf`, 563 036 064 bytes, idéntico al tamaño citado por el plan.
- **Alternativas:** `unsloth/Qwen3.5-0.8B-GGUF` Q4_0 (507 MB, cuantización distinta), `bartowski/...`.
- **Evidencia:** `models/manifest.json` (revisión y sha256).

## ADR-004 — Medir varios SLM antes de elegir

- **Contexto:** el plan propone Qwen3.5-0.8B Q4_0 (contexto 1024, salida 64-96); el informe, Qwen2.5-1.5B-Instruct Q4_K_M (n_ctx 2048). El plan permite elegir otro modelo si el equipo lo justifica.
- **Decisión:** medir Qwen3.5-0.8B Q4_0, Qwen2.5-1.5B-Instruct Q4_K_M y Qwen2.5-0.5B-Instruct Q4_K_M (contingencia) con los parámetros del plan (ctx 1024, 96 tokens). Regla: el de mejor calidad con pico RSS <= 1,7 GB y p50 <= ~10 s para 96 tokens.
- **Evidencia:** ver ADR-006 (resultado).

## ADR-005 — Banderas de compilación explícitas en lugar de `-march=native`

- **Contexto:** en Termux (clang 21.1.8, kernel 4.14) `-march=native` resuelve a `-target-cpu generic` y **no** define `__ARM_FEATURE_DOTPROD`, aunque `/proc/cpuinfo` muestra `asimddp`.
- **Decisión:** compilar llama.cpp y whisper.cpp con `-DGGML_NATIVE=OFF -DGGML_CPU_ARM_ARCH=armv8.2-a+dotprod+fp16`, y el orquestador con `-march=armv8.2-a+dotprod+fp16` (o `-mcpu=native`, que sí detecta dotprod).
- **Alternativas:** `GGML_NATIVE=ON` (perdería kernels dotprod si la detección falla igual que en clang).
- **Evidencia:** `clang -march=native -E -dM` sin DOTPROD; `clang -march=armv8.2-a+dotprod+fp16 -E -dM` con `__ARM_FEATURE_DOTPROD 1`; log de CMake de llama.cpp: `HAVE_DOTPROD - Success`, `Adding CPU backend variant ggml-cpu: -march=armv8.2-a+dotprod+fp16`.

## ADR-006 — SLM elegido: Qwen2.5-1.5B-Instruct Q4_K_M con `-nr` y 2 hilos

- **Contexto:** regla de ADR-004. Mediciones en `docs/metricas/slm.md`.
- **Decisión:** Qwen2.5-1.5B-Instruct Q4_K_M, `-t 2`, `-c 1024`, `-n 96`, `--temp 0.7`, **`-nr` (sin repack de pesos)**.
- **Alternativas:** Qwen3.5-0.8B Q4_0 (base del plan; p50 5,3 s, 1170 MiB, 5/10 respuestas aceptables) como alternativa rápida; Qwen2.5-0.5B (3/10) como contingencia.
- **Evidencia:** 1.5B con `-nr`: RSS pico 1151 MiB, p50 9836 ms, p95 13643 ms, 7/10 aceptables. Sin `-nr` el pico sube a 2055 MiB (el repack duplica los pesos en memoria anónima). `llama-bench`: 2 hilos gana en generación para los tres modelos (big.LITTLE).

## ADR-007 — Prompt ChatML por archivo y `llama-completion` no interactivo

- **Contexto:** `llama-cli` ya no existe como objetivo de compilación en el commit `92d6c8a`; `llama-completion` mantiene el modo de completado. El texto del usuario no debe pasar por argv de una shell.
- **Decisión:** el orquestador escribe el prompt completo en formato ChatML (`<|im_start|>system ... <|im_start|>assistant\n`) en `tmp/prompt.txt` y ejecuta `llama-completion -f prompt.txt -no-cnv --no-display-prompt`; la salida estándar es solo la respuesta. Para Qwen3.5 se añade un bloque `<think>\n\n</think>\n\n` vacío (desactiva el razonamiento) y, por seguridad, la limpieza elimina cualquier `<think>…</think>`.
- **Alternativas:** modo conversación `-cnv -st` (mezcla el eco del prompt y marcas en la salida); `--reasoning off` (depende de la plantilla jinja de cada modelo).
- **Evidencia:** 50 ejecuciones de `eval_slm.sh` sin bloques `<think>` ni eco del prompt en la salida; la única marca residual es `[end of text]`, eliminada por `limpiar_para_tts`.

## ADR-008 — sherpa-onnx v1.13.8 compilado en Termux con el onnxruntime del sistema

- **Contexto:** no existe paquete `sherpa-onnx` en Termux; sí `onnxruntime` 1.31.1. La rama principal de sherpa-onnx (v2.x, `29b735c`) compila, pero eliminó el fonemizador espeak-ng integrado: las voces Piper ya no convierten texto a fonemas sin un fonemizador externo.
- **Decisión:** compilar la etiqueta **v1.13.8** (última 1.x, 2026-09-10) en el teléfono con `-DSHERPA_ONNX_USE_PRE_INSTALLED_ONNXRUNTIME_IF_AVAILABLE=ON` y `SHERPA_ONNXRUNTIME_LIB_DIR=$PREFIX/lib`, solo el binario `sherpa-onnx-offline-tts`, `BUILD_SHARED_LIBS=ON`. Se ejecuta mediante `scripts/tts.sh` (fija `LD_LIBRARY_PATH`).
- **Alternativas:** v2.x + `piper_phonemize` externo (más piezas que compilar); compilación cruzada con NDK (requiere WSL, no instalado en el PC); `termux-tts-speak` (fallback del plan, depende del motor de Android).
- **Evidencia:** error de v2 registrado en la bitácora; resultado de v1.13.8 en `docs/metricas/tts.md`.

## ADR-009 — STT: whisper base q5_1 con contexto de audio reducido (`-ac 512`) y pista de vocabulario

- **Contexto:** el plan propone tiny y comparar base "solo si tiny queda corto". Con voz real tiny dio 8/20 utilizables.
- **Decisión:** `ggml-base-q5_1.bin`, `-t 4 -ac 512 --prompt "Hola, Jarvis."`. `-ac 512` limita el codificador a ~10 s de audio (la grabación dura 5 s) y reduce el tiempo de 4,8 s a 2,1 s.
- **Alternativas:** tiny (rápido pero 8/20); base con ventana de 30 s (16/20, 4,8 s); small `-ac 512` (14-15/20, 6,6-7,2 s, 428 MiB).
- **Evidencia:** `docs/metricas/stt.md`. El criterio >= 18/20 no se alcanza con ninguna opción probada (mejor: 17/20).

## ADR-010 — TTS: voz `es_AR-daniela-high` con 8 hilos

- **Contexto:** las voces mexicanas medium/high obtuvieron 1,3-3,0 en la escucha humana.
- **Decisión:** `es_AR-daniela-high` (4/4/4), 8 hilos (RTF 0,92).
- **Alternativas:** `es_MX-claude-high` (2x más rápida, 2,7/3/2,7); motor de Android (3,3; funciona sin red en este equipo, pero sin WAV ni métricas).
- **Evidencia:** `docs/metricas/tts.md`. Costo: el TTS pasa a ser la etapa más lenta (p50 8,1 s en la demo).

## ADR-011 — Caché de prompt del SLM en modo solo lectura

- **Contexto:** con `-nr` el procesamiento del prompt baja a ~15 tok/s; el prompt de sistema (~51 tokens) costaba ~4 s por turno.
- **Decisión:** al arrancar, el orquestador genera `<modelo>.sistema.cache` con **solo** el prompt de sistema (`slm.cmd_preparar_cache`) y cada turno usa `--prompt-cache ... --prompt-cache-ro`. Se regenera si cambia el prompt de sistema.
- **Alternativas:** `--prompt-cache` en escritura (guardaría la pregunta del usuario en disco: rechazado por privacidad); servidor residente `llama-server` (modelo residente; viola la ejecución secuencial de V1).
- **Evidencia:** pregunta nueva: 14,7 s -> 8,2 s; `slm.err`: "session file matches 51 / 68 tokens of prompt".

## ADR-012 — Aviso de grabación sincronizado con el micrófono

- **Contexto:** `termux-microphone-record` es asíncrono y tarda ~300 ms en escribir datos; el aviso se mostraba antes y se perdía la primera palabra (ronda 2 de STT: base cayó a 9/20).
- **Decisión:** `capturar.sh` muestra "HABLA AHORA" (y vibra) cuando el archivo ya crece; `-l` pasa a ser un tope y la grabación se detiene con `-q` tras N segundos contados desde el aviso; se espera a que el m4a sea legible antes de convertir.
- **Evidencia:** bitácora (Fase J), 5/5 capturas válidas tras el cambio; ronda 3 sin palabras iniciales perdidas.
