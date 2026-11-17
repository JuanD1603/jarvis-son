# Resultados de Jarvis-son V1 (Etapa 7)

**Equipo:** Xiaomi Redmi Note 10S (M2101K7BL), MediaTek Helio G95 (2x A76 + 6x A55), 6 GB RAM, Android 13 / MIUI 14.0.11. Termux 0.118.3. Mediciones del 2026-11-12, teléfono conectado por USB y cargando. Datos crudos: `datos/turnos.csv` (29 turnos, sin texto de conversación).

**Configuración final** (`config/config.ini.ejemplo`):

| Etapa | Componente | Parámetros |
|---|---|---|
| Captura | Termux:API `termux-microphone-record` + FFmpeg | 5 s, WAV mono 16 kHz s16le |
| STT | whisper.cpp `d2c6f05`, `ggml-base-q5_1.bin` | `-l es -t 4 -ac 512 --prompt "Hola, Jarvis."` |
| SLM | llama.cpp `92d6c8a`, Qwen2.5-1.5B-Instruct Q4_K_M | `-t 2 -c 1024 -n 96 --temp 0.7 -nr`, caché de prompt de sistema solo lectura |
| TTS | sherpa-onnx v1.13.8, Piper `es_AR-daniela-high` | 8 hilos |
| Reproducción | `play-audio` | bloqueante |

## 1. Pruebas realizadas

| Prueba | Turnos | Completos (código 0) | Errores | OOM | Red |
|---|---|---|---|---|---|
| Estabilidad automática (`--wav`, WAV sintéticos), ids 1-10 | 10 | 10 | 0 | 0 | con red |
| Modo avión automática (`--wav`), ids 11-20 | 10 | 10 | 0 | 0 | **sin red** |
| Turno de verificación con voz final, id 21 | 1 | 1 | 0 | 0 | sin red |
| **Demo con voz humana** (interactivo), ids 22-29 | **8** | 8 | 0 | 0 | **sin red** |

Ids 1-20 usaron la voz `es_MX-ald-medium`; ids 21-29, la voz final `es_AR-daniela-high`. La demo humana registró 8 turnos (se pedían 10).

Memoria del sistema (`free -m`, columna *available*): 1625 MB antes y 2161 MB después de los 10 turnos de estabilidad; 2018 MB antes y 2191 MB después de los 10 turnos en modo avión. No hubo pérdida de memoria acumulada ni cierres por `lmkd`.

Verificación sin red: `settings get global airplane_mode_on` = 1, `dumpsys connectivity` -> `Active default network: none`, `ping 8.8.8.8` y `curl https://huggingface.co` fallan; el control se hizo por SSH sobre el túnel USB de ADB.

## 2. Latencia por etapa

Percentil por rango más cercano (`tests/metricas.py`). Unidades: milisegundos.

### Demo con voz humana (n = 8, modo avión, configuración final)

| Etapa | n | p50 | p95 | máx |
|---|---|---|---|---|
| Captura (5 s + arranque del micrófono) | 8 | 6082 | 6507 | 6507 |
| STT | 8 | 2139 | 3039 | 3039 |
| SLM | 8 | 5806 | 11879 | 11879 |
| TTS | 8 | 8083 | 20787 | 20787 |
| Reproducción | 8 | 5890 | 12136 | 12136 |
| **Turno total** | 8 | **27467** | **53021** | 53021 |

Tiempo desde que termina de hablar el usuario hasta que empieza a oír la respuesta (STT + SLM + TTS), mediana: ~16 s.

### Pruebas automáticas `--wav` (n = 21, sin captura)

| Etapa | n | p50 | p95 | máx |
|---|---|---|---|---|
| STT | 21 | 2234 | 3143 | 3172 |
| SLM | 21 | 6877 | 17290 | 17397 |
| TTS | 21 | 5647 | 8635 | 9487 |
| Reproducción | 21 | 5998 | 21289 | 21397 |
| **Turno total** | 21 | **19281** | **46326** | 49867 |

La cola (p95) la producen las respuestas largas (225-290 caracteres): más tokens generados, más audio que sintetizar y reproducir. Con muestras de 8 y 21 turnos, el p95 equivale al máximo o casi.

## 3. Memoria pico por etapa (RSS, `wait4` -> `ru_maxrss`)

| Etapa | RSS pico máx (MiB) | Límite (ADR-001) |
|---|---|---|
| Captura | 37 | 1,7 GB |
| STT (whisper base) | 174 | 1,7 GB |
| **SLM (Qwen2.5-1.5B, `-nr`)** | **1144** | 1,7 GB |
| TTS (daniela-high, texto largo) | 433 | 1,7 GB |
| Reproducción | 34 | 1,7 GB |

Las etapas son secuenciales: el pico del sistema es el de la etapa SLM (1,12 GiB), dentro del objetivo de 1,7 GB. Sin `-nr` el SLM llegaba a 2055 MiB (ver `slm.md`).

## 4. Térmica

Temperatura de batería: 38,8 °C -> 40,3 °C en 10 turnos (estabilidad) y 40,8 -> 40,7 °C en los 10 en modo avión. En la evaluación del SLM la generación bajó de ~10,4 a ~7,4 tok/s tras varias preguntas seguidas (posible *throttling*). La prueba térmica opcional de 20 turnos no se realizó.

## 5. Criterios de aceptación del plan

| Criterio | Resultado | Evidencia |
|---|---|---|
| 10 turnos consecutivos completos sin OOM ni pérdida de estado | **Cumplido** (automático, 10/10 con red y 10/10 sin red). Demo humana: 8/8 | Secciones 1 y 3 |
| >= 18/20 frases silenciosas utilizables; reportar ruido | **No cumplido**: 17/20 (ronda 1) y 14/20 (ronda 3) con base; ruido no evaluado | `stt.md` |
| Cada respuesta se reproduce en voz; no solapa TTS e inferencia | **Cumplido**: 29/29 turnos con audio; orquestador secuencial (`fork`/`wait4` antes de la siguiente etapa) | `turnos.csv`, `arquitectura.md` |
| Funciona sin conexión tras descargar modelos y voz (modo avión) | **Cumplido**: 19 turnos en modo avión (10 automáticos, 1 de verificación y 8 con voz humana) | Sección 1 |
| El repositorio compila en CI y documenta instalación, licencias, checksums y límites | **Verificado en el teléfono**: compilación, `ctest` 2/2 y `clang-format --Werror`. En GitHub, la CI se ejecuta en cada PR al estar fusionado `ci.yml` | `.github/workflows/ci.yml`, `README.md`, `models/manifest.json` |

## 6. Limitaciones observadas

- Latencia total alta (p50 ~27 s por turno con voz); la voz elegida es la etapa más lenta.
- STT por debajo del criterio; errores en nombres propios y palabras poco frecuentes.
- El SLM no resuelve aritmética simple, a veces inventa (p. ej., afirma ser "creado por Anthropic") y en 1 de 29 turnos devolvió una respuesta vacía (se dijo el mensaje de respaldo).
- Las mediciones son de un único teléfono y de una sola persona; no se extrapolan a otros equipos de 3-6 GB.
