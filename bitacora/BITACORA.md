# Bitácora de instalación — Jarvis-son V1

Formato: fecha/hora (UTC-5) | fase | comando | resultado | tiempo | incidencias.

## Fase A — Preparar el PC (Etapa 0)

| Fecha/hora | Comando | Resultado | Tiempo | Incidencias |
|---|---|---|---|---|
| 2026-11-12 16:49 | `uname -a` | MINGW64_NT-10.0-19045 (Windows 10 Pro 19045, Git Bash) | <1 s | - |
| 2026-11-12 16:49 | `ssh -V`; `python --version`; `git --version` | OpenSSH_10.6p1; Python 3.12.7; git 2.56.0.windows.4 | <1 s | - |
| 2026-11-12 16:50 | `adb version` | No instalado | - | Se instala con winget |
| 2026-11-12 16:51 | `winget install --id Google.PlatformTools -e` | Instalado: adb 1.0.41 (platform-tools 37.0.3) | ~60 s | El alias de winget no queda en PATH de Git Bash: se usa `scripts/env_pc.sh` que añade la ruta del paquete |
| 2026-11-12 16:53 | `mkdir -p` estructura del repo, `.gitignore`, `models/manifest.json` | Estructura creada según el plan | <1 s | - |

## Fase B — Conexión ADB e inventario (Etapa 0)

| Fecha/hora | Comando | Resultado | Tiempo | Incidencias |
|---|---|---|---|---|
| 2026-11-12 16:52 | `adb devices -l` | `device product:maltose_global model:M2101K7BL` (autorizado) | 2 s | - |
| 2026-11-12 16:53 | `adb shell getprop ...`, `/proc/cpuinfo`, `/proc/meminfo`, `df -h /data`, `dumpsys battery` | Ver `docs/inventario_dispositivo.md` | 5 s | Git Bash convertía `/data` en `C:/Program Files/Git/data`: se añade `MSYS_NO_PATHCONV=1` a `scripts/env_pc.sh` |
| 2026-11-12 16:54 | `adb shell svc power stayon usb` | Falla: `Killed`, código 137 | - | MIUI bloquea el comando sin "Depuración USB (ajustes de seguridad)" |
| 2026-11-12 16:54 | `adb shell settings put global stay_on_while_plugged_in 7`; `adb shell input keyevent 224` | `SecurityException` (WRITE_SECURE_SETTINGS / INJECT_EVENTS) | - | Confirma que falta "Depuración USB (ajustes de seguridad)" |

## Fase C — Termux y Termux:API (Etapa 1)

| Fecha/hora | Comando | Resultado | Tiempo | Incidencias |
|---|---|---|---|---|
| 2026-11-12 16:54 | API GitHub `releases/latest` | termux-app v0.118.3, termux-api v0.53.0 | 1 s | - |
| 2026-11-12 16:54 | `curl -L` APK + `sha256sum` | termux-app `72fdb596...a4e` (coincide con `sha256sums` oficial); termux-api `ecf916ff...e39a` (coincide con `digest` de GitHub) | 15 s | - |
| 2026-11-12 16:55 | `adb install -r termux-app.apk` | `INSTALL_FAILED_VERIFICATION_FAILURE: Install not allowed` | 5 s | Falta "Instalar vía USB" en MIUI. PUNTO HUMANO |

| 2026-11-12 (tras punto humano) | `adb shell input keyevent 224` | Sigue `SecurityException` | - | El ajuste de seguridad no se aplicó a la sesión ADB existente |
| 2026-11-12 | `adb reconnect`; `adb kill-server && adb start-server` | El dispositivo desaparece de `adb devices` | ~2 min | Tras cambiar ajustes de depuración, MIUI revoca la sesión USB; requiere reconectar el cable y volver a autorizar. PUNTO HUMANO |

| 2026-11-12 17:03 | Reconexión del cable y re-autorización (punto humano) | `device`; `input keyevent 224` y `svc power stayon usb` devuelven 0 | - | El ajuste de seguridad solo aplica a sesiones ADB nuevas |
| 2026-11-12 17:04 | Revisión de Opciones de desarrollador con `uiautomator dump` | "Instalar vía USB" y "Depuración USB (ajustes de seguridad)" activos | 30 s | - |
| 2026-11-12 17:05 | `adb install -r termux-app.apk` | Diálogo MIUI `AdbInstallActivity` "¿Quieres instalar la app no segura?" que exige huella/PIN; tras autenticación humana: `Success` | ~40 s | Claude no introduce PIN: la autenticación la hace la persona |
| 2026-11-12 17:06 | `adb install -r termux-api.apk` | `Success`; ambos con la misma firma (`db86cf3c`) | ~30 s | Mismo diálogo de huella |
| 2026-11-12 17:07 | `pm grant com.termux.api RECORD_AUDIO`; `pm grant com.termux POST_NOTIFICATIONS`; `deviceidle whitelist +com.termux(.api)`; `settings put global settings_enable_monitor_phantom_procs false`; `device_config ... max_phantom_processes 2147483647`; `settings put global stay_on_while_plugged_in 7`; `cmd appops set ... RUN_ANY_IN_BACKGROUND allow` | Todos con código 0; verificado `phantom_procs=false`, `max_phantom_processes=2147483647`, `RECORD_AUDIO granted=true` | 5 s | - |
| 2026-11-12 17:08 | `adb shell monkey -p com.termux ...` | Bootstrap completado en ~10 s; prompt `~ $` visible en captura | 10 s | - |

## Fase D — SSH a Termux por USB

| Fecha/hora | Comando | Resultado | Tiempo | Incidencias |
|---|---|---|---|---|
| 2026-11-12 17:08 | `adb shell run-as com.termux id` | Funciona (`uid=10282`): APK `github-debug` permite `run-as` (Método A) | <1 s | - |
| 2026-11-12 17:08 | `ssh-keygen -t ed25519 -f ~/.ssh/jarvis_phone -N "" -C jarvis-son` | Clave creada en el PC | <1 s | - |
| 2026-11-12 17:08 | `adb push` + `run-as com.termux` para copiar `scripts/termux_bootstrap_ssh.sh` a `~/b.sh`; `adb shell input text 'bash%sb.sh'` + `keyevent 66` | `pkg upgrade`, `pkg install openssh termux-api`, `authorized_keys`, autostart en `.bashrc`, `sshd`, `termux-wake-lock`: `BOOTSTRAP_OK` | 90 s | Mezcla Método A (escritura de archivos) + Método B (una sola orden tecleada) |
| 2026-11-12 17:10 | `adb forward tcp:8022 tcp:8022`; `ssh jarvis-phone 'uname -a; echo $PREFIX; nproc'` | `aarch64 Android`, `$PREFIX=/data/data/com.termux/files/usr`, 8 núcleos; `free -m`: 1619 MB disponibles | 1 s | - |
| 2026-11-12 17:11 | `bash scripts/conectar.sh` | `conexion OK` | 2 s | - |

## Fase E — Entorno de compilación y captura de audio (Etapas 1 y 2)

| Fecha/hora | Comando | Resultado | Tiempo | Incidencias |
|---|---|---|---|---|
| 2026-11-12 17:13 | `pkg install -y clang cmake make git ffmpeg termux-api wget curl jq time play-audio python binutils ninja` | clang 21.1.8, cmake 4.4.5, make 4.4.1, git 2.56.1, ffmpeg 9.0.2, python 3.14.10, ninja 1.13.2, play-audio 0.6, termux-api 0.60.0 | 65 s | - |
| 2026-11-12 17:14 | `mkdir -p ~/jarvis/{bin,models,tmp,logs,src}` | Creado | <1 s | - |
| 2026-11-12 17:14 | `clang++ -O2 -march=native hello.cpp` | Compila y ejecuta | 2 s | - |
| 2026-11-12 17:14 | `clang -march=native -E -dM - </dev/null \| grep DOTPROD` | **Sin** `__ARM_FEATURE_DOTPROD`: `-march=native` resuelve a `-target-cpu generic` en Termux | <1 s | Con `-mcpu=native` o `-march=armv8.2-a+dotprod+fp16` sí aparece `__ARM_FEATURE_DOTPROD`. Decisión: banderas explícitas (ver ADR-005) |
| 2026-11-12 17:15 | `capturar.sh 3` (prueba técnica, ruido ambiente) | `OK canales=1 tasa=16000 fmt=s16 duracion=3.2`; volumedetect mean -20,6 dB, max -5,4 dB | 5,1 s | Prueba con voz humana agrupada con la evaluación STT (Fase G) |

## Fase F — SLM: llama.cpp + Qwen (Etapa 4)

| Fecha/hora | Comando | Resultado | Tiempo | Incidencias |
|---|---|---|---|---|
| 2026-11-12 17:16 | `git clone --depth 1 https://github.com/ggml-org/llama.cpp` | commit `92d6c8a5caa2c11b3fb49a7a201b568d040be4b2` (2026-11-12) | 20 s | - |
| 2026-11-12 17:16 | `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DGGML_NATIVE=OFF -DGGML_CPU_ARM_ARCH=armv8.2-a+dotprod+fp16 -DLLAMA_CURL=OFF -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_SERVER=OFF` | `HAVE_DOTPROD - Success`; `Adding CPU backend variant ggml-cpu: -march=armv8.2-a+dotprod+fp16` | 30 s | - |
| 2026-11-12 17:17 | `cmake --build build -j 4 --target llama-cli llama-bench llama-completion` | `ninja: error: unknown target 'llama-cli'` | - | En este commit no existe `llama-cli` como objetivo; se usa `llama-completion` (ADR-007) |
| 2026-11-12 17:17 | `cmake --build build -j 4 --target llama-completion llama-bench` | `BUILD_RC=0`, 152 pasos | 611 s | - |
| 2026-11-12 17:13 | API HF `tree/main` | `Qwen/Qwen3.5-0.8B-GGUF` no existe (error de acceso); se usa `ggml-org/Qwen3.5-0.8B-GGUF` (ADR-003) | 2 s | - |
| 2026-11-12 17:14 | `descargar_verificado.sh` x3 (Qwen3.5-0.8B Q4_0, Qwen2.5-1.5B Q4_K_M, Qwen2.5-0.5B Q4_K_M) | Los 3 con sha256 verificado; registrados en `models/manifest.json` | ~90 s | - |
| 2026-11-12 17:26 | `llama-bench -m <modelo> -t 2,4,6 -p 128 -n 64 -r 3 -o md` (3 modelos) | 2 hilos gana en generación en los 3 modelos (ver `docs/metricas/slm.md`) | ~8 min | - |
| 2026-11-12 17:30 | `eval_slm.sh <modelo> 2 ...` (10 preguntas x 3 modelos) | Qwen2.5-1.5B: RSS pico 2055 MiB (> 2,0 GB) | ~5 min | Parseo de tokens/s fallaba (formato `common_perf_print` con marca de tiempo); corregido |
| 2026-11-12 17:36 | `llama-completion ... -nr` vs por defecto | RSS 1.5B: 2103944 kB -> 1170248 kB con `-nr` (el repack duplica pesos) | 10 s | - |
| 2026-11-12 17:37 | `pkill -f eval_slm.sh` por SSH | Mató también la sesión SSH (el patrón coincidía con su propia línea de comando) | - | Lección: usar `pgrep -x` o PID exactos |
| 2026-11-12 17:37 | `EXTRA=-nr eval_slm.sh` (0.8B y 1.5B) + 0.8B por defecto | Ver tabla en `docs/metricas/slm.md`. Elegido Qwen2.5-1.5B `-nr` (ADR-006) | ~5 min | - |

## Fase G — STT: whisper.cpp (Etapa 3)

| Fecha/hora | Comando | Resultado | Tiempo | Incidencias |
|---|---|---|---|---|
| 2026-11-12 17:45 | `git clone --depth 1 https://github.com/ggml-org/whisper.cpp` | commit `d2c6f05a46dacf663e357bcf0ed40f542a72825d` (2026-11-10) | 15 s | - |
| 2026-11-12 17:45 | `cmake ... -DGGML_NATIVE=OFF -DGGML_CPU_ARM_ARCH=armv8.2-a+dotprod+fp16 -DWHISPER_SDL2=OFF`; `cmake --build build -j 4 --target whisper-cli` | `HAVE_DOTPROD - Success`, `BUILD_RC=0` | 135 s | - |
| 2026-11-12 17:46 | `descargar_verificado.sh` ggml-tiny-q5_1.bin, ggml-base-q5_1.bin (repo `ggerganov/whisper.cpp`, rev `5359861`) | sha256 verificados; manifiesto actualizado | 20 s | - |
| 2026-11-12 17:50 | `whisper-cli -m ggml-{tiny,base}-q5_1.bin -l en -t {2,4} -nt -np -f samples/jfk.wav` (11 s de audio) | tiny: 3419 ms (t=2), 2747 ms (t=4), RSS 131 MiB; base: 7188 ms (t=2), 5581 ms (t=4), RSS 198 MiB. Transcripción correcta | 20 s | Solo prueba de funcionamiento (audio en inglés) |

## Fase H — TTS: sherpa-onnx + Piper (Etapa 5)

| Fecha/hora | Comando | Resultado | Tiempo | Incidencias |
|---|---|---|---|---|
| 2026-11-12 17:46 | API GitHub release `tts-models` | Voces latinoamericanas: `es_MX-ald-medium`, `es_MX-claude-high`, `es_AR-daniela-high` (+ variantes fp16) | 2 s | - |
| 2026-11-12 17:47 | `descargar_verificado.sh` + `tar -xjf` (ald-medium, claude-high) | sha256 verificados (digest de GitHub) | 30 s | - |
| 2026-11-12 17:52 | `pkg search sherpa`; `pkg search onnxruntime` | Sin paquete sherpa; **sí** existe `onnxruntime 1.31.1` | 5 s | - |
| 2026-11-12 17:53 | `pkg install onnxruntime`; clonar sherpa-onnx (main, `29b735c`); `cmake ... -DSHERPA_ONNX_USE_PRE_INSTALLED_ONNXRUNTIME_IF_AVAILABLE=ON` con `SHERPA_ONNXRUNTIME_LIB_DIR=$PREFIX/lib` | CMake usa `/data/data/com.termux/files/usr/lib/libonnxruntime.so` (no descarga binarios glibc) | 60 s | - |
| 2026-11-12 17:54 | `cmake --build build -j 4 --target sherpa-onnx-offline-tts` | `BUILD_RC=0` | 1064 s | - |
| 2026-11-12 18:13 | `tts.sh --vits-model=... --vits-data-dir=.../espeak-ng-data ... "Hola, soy Jarvis-son..."` | Falla: `--vits-data-dir is deprecated and ignored in sherpa-onnx >= v2.0.0` y `phoneme_codepoints is empty, no lexicon, and no frontend available` | 2,6 s | sherpa-onnx v2 eliminó el fonemizador espeak-ng integrado. Cambio de enfoque: versión v1.13.8 (ADR-008) |
| 2026-11-12 18:15 | `git clone --branch v1.13.8` (`11afbd0`) y misma configuración | En curso | - | - |

| 2026-11-12 18:21 | `sherpa-onnx v1.13.8`: `cmake --build build -j 4 --target sherpa-onnx-offline-tts` | `BUILD_RC=0`; binario enlazado estáticamente salvo `libonnxruntime.so` de Termux | 892 s | - |
| 2026-11-12 18:22 | `tts.sh` con `es_MX-ald-medium` y `es_MX-claude-high`, 2 y 4 hilos | ald: 3501 ms (t=4), RTF 0,214, RSS 189 MiB; claude-high: 3352 ms (t=4), RTF 0,196, RSS 182 MiB | 15 s | Funciona sin red (espeak-ng integrado) |
| 2026-11-12 18:24 | `generar_wav_prueba.sh` (20 frases, voz claude-high, 16 kHz mono) | 20 WAV en `~/jarvis/tests/wav` | 77 s | - |
| 2026-11-12 18:26 | STT automático sobre los 20 WAV (tiny/base, ventana 30 s y `-ac 512`, pista "Hola, Jarvis.") | base `-ac 512` + pista: 18/20 utilizables, WER 0,120, p50 2357 ms | ~6 min | Ver `docs/metricas/stt.md` |

## Fase I — Orquestador C++ (Etapa 6)

| Fecha/hora | Comando | Resultado | Tiempo | Incidencias |
|---|---|---|---|---|
| 2026-11-12 17:20 | `cmake -B build -G Ninja -DJARVIS_ARM_NATIVO=ON && cmake --build build && ctest` (en el teléfono) | Primera compilación: error en prueba (cadena con salto de línea) y advertencia `-Wstring-concatenation`; corregidos | 40 s | - |
| 2026-11-12 17:24 | `ctest` | 2/2 (unitarias + turno `--mock`). El modo mock usaba `/tmp` (no existe en Termux): se usa `$TMPDIR` | 2 s | - |
| 2026-11-12 18:30 | `jarvis --wav tests/wav/frase_03.wav` | Primer turno completo voz a voz: STT 1692 ms, SLM 8458 ms, TTS 3432 ms, reproducción 3451 ms | 17 s | - |
| 2026-11-12 18:31 | `llama-completion ... --prompt-cache` | SLM 14,7 s -> 8,2 s en pregunta nueva. Implementado en modo `--prompt-cache-ro` con caché solo del prompt de sistema (privacidad) | - | - |
| 2026-11-12 18:48 | `jarvis --wav` con `--prompt {stt.pista}` | STT falla: `input file not found 'Jarvis.'` | - | `resolver_referencias` expandía `{stt.pista}` dentro de `cmd` antes de dividir argv. Corregido: las claves `cmd*` no se pre-resuelven; prueba unitaria añadida |

## Fase J — Integración y evaluación (Etapa 7)

| Fecha/hora | Comando | Resultado | Tiempo | Incidencias |
|---|---|---|---|---|
| 2026-11-12 18:49 | `prueba_estabilidad.sh 10 estabilidad` | 10/10 turnos con código 0, sin OOM. `free -m` available 1625 -> 2161 MB. Batería 38,8 -> 40,3 °C | 4,5 min | Turno 4: el SLM devolvió solo fin de texto -> mensaje de respaldo. Respuestas largas cortadas a mitad de frase: se añadió recorte a la última frase completa |
| 2026-11-12 18:55 | `adb shell cmd connectivity airplane-mode enable`; `adb shell svc wifi disable` | `airplane_mode_on=1`, `Active default network: none`; `ping` y `curl` fallan; SSH por USB sigue activo | 5 s | - |
| 2026-11-12 18:56 | `prueba_estabilidad.sh 10 modo_avion` | 10/10 turnos con código 0 sin red | 4 min | - |

| 2026-11-12 19:05 | `eval_stt.sh` (voz humana, silencio, 20 frases; tiny:1500, base:512, base:1500) | base:512 17/20 (WER 0,120, p50 2130 ms); base:1500 16/20; tiny:1500 8/20 | ~8 min | No alcanza el criterio >= 18/20 por una frase |
| 2026-11-12 19:20 | Modo avión off; `descargar_verificado.sh ggml-small-q5_1.bin`; modo avión on | sha256 verificado (190 MB) | 60 s | Descarga puntual con red; el resto de pruebas siguen sin red |
| 2026-11-12 19:24 | `whisper-cli -m ggml-small-q5_1.bin -ac 512` sobre WAV sintéticos fallidos con base | Corrige 04, 09 y 11; 5,8-8,6 s por frase; RSS 427 MiB | 2 min | Se pide una segunda ronda humana base:512 vs small:512 |

| 2026-11-12 19:35 | `eval_stt.sh` ronda 2 (base:512 vs small:512, mismas grabaciones) | base:512 9/20 (WER 0,304); small:512 14/20 (WER 0,152, p50 7161 ms, 428 MiB) | ~12 min | Las hipótesis pierden el inicio ("Hola Jarvis" -> "Vis"). Causa: el aviso "habla ahora" salía antes de que el micrófono grabara |
| 2026-11-12 19:36 | `calificar_tts.sh` (ald-medium vs claude-high, 3 frases) | Medias pron/ritmo/nat: ald 1,7/2,0/1,3; claude 2,0/1,7/1,3. Preferida: claude-high | ~4 min | Calificación baja: se añaden es_AR-daniela-high y el motor de Android |
| 2026-11-12 19:37 | Medición de arranque de `termux-microphone-record` | El archivo recibe datos ~280-300 ms después de lanzar; sumado al `-q` previo, ~0,6 s perdidos | 15 s | `capturar.sh` ahora avisa "HABLA AHORA" (+ vibración) cuando el archivo ya crece |
| 2026-11-12 19:38 | `capturar.sh 3` tras el cambio | Duración 3,07 s: `-l` cuenta desde el arranque y cortaba antes | - | `-l SEG+3` como tope y detención con `-q` |
| 2026-11-12 19:39 | `capturar.sh 3` | `moov atom not found`: tras `-q` el m4a se cierra de forma asíncrona | - | Esperar hasta que `ffprobe` lo lea (máx. 4 s). 5/5 capturas OK |
| 2026-11-12 19:40 | Modo avión off; descarga `vits-piper-es_AR-daniela-high` (sha256 OK); modo avión on | daniela-high: 7764 ms, RTF 1,22, RSS 206 MiB | 1 min | - |
| 2026-11-12 19:41 | `termux-tts-engines`; `termux-tts-speak -l es -n MX ...` en modo avión | Motor `com.google.android.tts`; código 0 en 4829 ms | 5 s | Que suene sin red lo confirma la persona |
| 2026-11-12 19:42 | Sesión humana 3 lanzada (STT ronda 3 + 4 voces) | - | - | `calificar_tts.sh` tenía el bucle sin parchear (todas las voces "A"); corregido y subido antes de que la persona llegara a esa parte |

| 2026-11-12 19:55 | `eval_stt.sh` ronda 3 (captura corregida; base:512 vs small:512) | base:512 14/20 (WER 0,216, p50 2104 ms); small:512 15/20 (WER 0,184, p50 6576 ms) | ~12 min | Criterio >= 18/20 no alcanzado en ninguna ronda (mejor: 17/20, ronda 1) |
| 2026-11-12 19:55 | `calificar_tts.sh` (4 opciones) | Medias pron/ritmo/nat: ald 2/2/2; claude 2,7/3/2,7; daniela 4/4/4; Android 3,3/3,3/3,3. Preferida: daniela (C). Android suena en modo avión | ~6 min | - |
| 2026-11-12 19:56 | `tts.sh` daniela-high con 2/4/6/8 hilos | 8781 / 7790 / 7627 / 7125 ms; RTF 1,34 / 1,15 / 1,07 / 0,92 | 35 s | Se fija 8 hilos |
| 2026-11-12 19:57 | `jarvis --wav frase_03.wav` con config final | código 0: STT 2056, SLM 5637, TTS 6129, reproducción 2606 ms | 16 s | - |
| 2026-11-12 19:58 | `adb shell input text '~/jarvis/bin/jarvis'` | Termux recibió `//jarvis/bin/jarvis` | - | `input text` no transmite `~`. Se crea `$PREFIX/bin/jarvis` (enlace) para abrirlo escribiendo solo `jarvis` |

| 2026-11-12 20:00-20:06 | Demo humana interactiva (`jarvis`, modo avión) | 8 turnos (ids 22-29), todos código 0. p50 total 27,5 s; p95 53,0 s | 6 min | Se pedían 10 turnos; se registraron 8 |
| 2026-11-12 20:10 | `adb shell cmd connectivity airplane-mode disable`; `svc wifi enable` | `airplane_mode_on=0` | 2 s | - |
| 2026-11-12 20:11 | `python tests/metricas.py turnos.csv --modo voz` | Ver `docs/metricas/resultados_v1.md` | <1 s | - |

## Fase K — Repositorio listo para GitHub (sin publicar)

| Fecha/hora | Comando | Resultado | Tiempo | Incidencias |
|---|---|---|---|---|
| 2026-11-12 20:12 | `clang-format --dry-run --Werror` (clang-format 21.1.8 en Termux) | 23 avisos; `clang-format -i` aplicado; luego 0 | 5 s | Los archivos del PC tenían CRLF (reescritos por un editor): normalizados a LF |
| 2026-11-12 20:14 | `cmake --build build && ctest` en el teléfono | 2/2 pruebas (unitarias + `--mock`); `bash -n` OK en todos los scripts | 30 s | - |
| 2026-11-12 20:20 | `JARVIS_HOME=~/jarvis_prueba SOLO_MODELOS=1 SIN_MODELOS=1 bash scripts/instalar_termux.sh` (directorio limpio) | rc=0; estructura, scripts, frases y config creados. Directorio de prueba eliminado | ~2 min | `pkg install` actualizó cmake 4.4.5 -> 4.5.0; el sistema instalado sigue funcionando (turno `--wav` código 0). La plantilla tenía rutas fijas: ahora derivan de `{general.home}` y el instalador escribe `home` según `JARVIS_HOME`. La compilación completa y la descarga de modelos del instalador no se repitieron en limpio (ya validadas paso a paso en las Fases F-H) |
| 2026-11-12 20:22 | `git ls-files \| grep -E '\.(gguf\|bin\|onnx\|wav\|m4a\|apk)$'` | Sin coincidencias | <1 s | - |

## Fase L — Publicación en GitHub

El repositorio se publica por módulos en `JuanD1603/jarvis-son`: una rama y un pull request por etapa, revisado por un integrante distinto del autor, con issues por etapa y tablero del proyecto. La CI (GitHub Actions) se ejecuta en cada PR una vez fusionado `.github/workflows/ci.yml`. El código del equipo se publica con licencia MIT.

## Fase M — Capturas del proceso de instalación

Reproducción de la instalación en un directorio limpio (`~/jarvis_demo`) con `scripts/instalar_termux.sh`; la instalación original del 2026-11-12 está en las fases anteriores. Comandos enviados por SSH (Wi-Fi) a una sesión `tmux` visible en Termux; capturas con `adb exec-out screencap` (ADB inalámbrico y luego USB). Resultado: [docs/instalacion_capturas.md](../docs/instalacion_capturas.md).

| Fecha/hora | Comando | Resultado | Tiempo | Incidencias |
|---|---|---|---|---|
| 2026-11-23 00:45 | `adb pair` (código de vinculación) | Dos intentos con `protocol fault` | 1 min | El código había caducado; con un código nuevo `adb pair` y `adb connect` funcionaron |
| 2026-11-23 00:47 | `ssh jarvis-wifi 'termux-battery-status'`; `pkg install -y tmux` | SSH por Wi-Fi OK; tmux 3.8 | 1 min | - |
| 2026-11-23 00:49-00:56 | Capturas 01-08 (inventario, CPU, ajustes, permisos, Termux, paquetes, clang, `capturar.sh 3`) | WAV mono 16 kHz s16 3,07 s; archivo borrado | 7 min | - |
| 2026-11-23 00:56 | `git clone .../jarvis-son jarvis-son-demo` | `main` en `f6469f6` | 5 s | - |
| 2026-11-23 00:57-01:24 | `JARVIS_HOME=~/jarvis_demo SIN_MODELOS=1 bash scripts/instalar_termux.sh` | rc=0. llama.cpp 152/152 (~9 min, `HAVE_DOTPROD - Success`), whisper.cpp 44/44 (~3 min), sherpa-onnx 326/326 (~15 min), orquestador 9/9, `ctest` 2/2 | 27 min | `pkg install` actualizó cmake 4.5.1, ffmpeg 9.1 y onnxruntime 1.32.0. El instalador re-enlazó `$PREFIX/bin/jarvis` a `~/jarvis_demo`; se restauró a `~/jarvis/bin/jarvis` |
| 2026-11-23 01:26 | `rm -rf ~/jarvis_demo ~/jarvis_demo_install.log` | 628 MB liberados; log copiado antes a `docs/metricas/instalacion_2026-11-23.log` | 2 s | - |
| 2026-11-23 01:26 | `sha256sum` de whisper base q5_1 y Qwen2.5-1.5B frente a `models/manifest.json` | Coinciden | 10 s | - |
| 2026-11-23 01:26 | `llama-bench -t 2 -p 64 -n 32 -r 1` | pp64 33,28 tok/s; tg32 11,29 tok/s | 10 s | Una sola repetición |
| 2026-11-23 01:27 | `whisper-cli ... -ac 512 -f frase_03.wav` | "Dime cuáles la capital de Colombia." | 3 s | - |
| 2026-11-23 01:27 | `jarvis --wav frase_03.wav` (turno 52) | `[ERROR] etapa: tts`, código 1 | 6 s | `tts.err`: `cannot locate symbol "OrtGetApiBase"`. onnxruntime 1.32.0 exporta `OrtGetApiBase@@VERS_1.32.0`; el sherpa-onnx de `~/jarvis` se enlazó contra 1.31.1 |
| 2026-11-23 01:27 | `dpkg -i /data/data/com.termux/cache/apt/archives/onnxruntime_1.31.1_aarch64.deb` | onnxruntime vuelve a 1.31.1 | 5 s | Paquete tomado de la caché de apt |
| 2026-11-23 01:27 | `jarvis --wav frase_03.wav` (turno 53) | Código 0: STT 1853, SLM 3195, TTS 5334, reproducción 2667 ms | 13 s | - |
| 2026-11-23 01:32 | Modo avión (persona) + `ping -c1 -W2 8.8.8.8; jarvis --wav frase_03.wav` (turno 54) | `Network is unreachable`; código 0, total 16,9 s | 1 min | Captura manual. Al salir del modo avión cambian el puerto y el código de la depuración inalámbrica: se siguió por USB |
| 2026-11-23 01:38 | `python tests/metricas.py turnos.csv --modo voz` | 24 turnos de voz, todos código 0; p50 total 28,8 s, p95 53,0 s | <1 s | - |
| 2026-11-23 01:38-01:40 | `jarvis-web`; la persona pulsa Hablar (turno 55) | "¿Cuál es la capital de Francia?" -> "La capital de Francia es París."; código 0, 24,7 s | 2 min | - |
| 2026-11-23 | `apt-mark hold onnxruntime` | onnxruntime queda fijado en 1.31.1 (`hi`); `pkg upgrade` ya no lo actualiza | <1 s | Evita que se repita el fallo del TTS. Para actualizarlo: `apt-mark unhold onnxruntime` y recompilar sherpa-onnx |

## Trabajo futuro

- Demo con voz humana de al menos 10 turnos seguidos (se registraron 8; el criterio de 10 turnos se cumplió en las pruebas automáticas, con y sin red).
- Evaluación del STT con ruido moderado y nuevas rondas en silencio para alcanzar >= 18/20.
