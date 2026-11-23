# Jarvis-son V1

Asistente de voz **local y por turnos** para Android (Termux), proyecto académico de la asignatura Inteligencia Artificial de **Cotecnova** (Cartago, 2026). Docente: Ing. Felipe Gutiérrez Isaza. Equipo: Juan José Sánchez, Santiago Arcos, Juan David Osorio.

El usuario pulsa Enter, habla 5 segundos y escucha una respuesta breve en español. Todo se ejecuta en el teléfono, sin conexión, después de descargar los modelos.

```
[Enter] -> GRABANDO (Termux:API + FFmpeg) -> TRANSCRIBIENDO (whisper.cpp)
        -> GENERANDO (llama.cpp + Qwen2.5-1.5B) -> HABLANDO (sherpa-onnx + Piper) -> LISTO
```

## Arquitectura

Un orquestador en **C++17** lanza cada herramienta como proceso hijo con argumentos fijos (`fork` + `execvp` + `wait4`), con timeout por etapa, y espera a que termine antes de iniciar la siguiente: nunca hay dos modelos en memoria a la vez y la voz no se solapa con la inferencia. El texto del usuario nunca pasa por una shell. Detalles, máquina de estados y contratos entre etapas: [docs/arquitectura.md](docs/arquitectura.md). Decisiones: [docs/decisiones.md](docs/decisiones.md).

| Etapa | Componente | Modelo |
|---|---|---|
| Captura | Termux:API + FFmpeg | WAV mono 16 kHz, 5 s |
| STT | whisper.cpp | Whisper base multilingüe q5_1 (`-ac 512`) |
| SLM | llama.cpp (`llama-completion`) | Qwen2.5-1.5B-Instruct Q4_K_M (`-nr`, 2 hilos, contexto 1024, 96 tokens) |
| TTS | sherpa-onnx v1.13.8 | Piper `es_AR-daniela-high` |

## Resultados (Redmi Note 10S, Helio G95, 6 GB, Android 13)

| Métrica | Valor |
|---|---|
| Turno completo con voz, p50 / p95 (n = 8, modo avión) | 27,5 s / 53,0 s |
| STT p50 / SLM p50 / TTS p50 | 2,1 s / 5,8 s / 8,1 s |
| Memoria pico (etapa SLM) | 1144 MiB (presupuesto 1,7 GB) |
| Turnos sin error | 29/29 (19 en modo avión) |
| STT con voz real en silencio | 14-17 de 20 frases utilizables (criterio del plan: 18) |

Informe completo: [docs/metricas/resultados_v1.md](docs/metricas/resultados_v1.md) ([SLM](docs/metricas/slm.md), [STT](docs/metricas/stt.md), [TTS](docs/metricas/tts.md)).

## Requisitos

- Teléfono Android 13+ arm64 con ~6 GB de RAM (probado solo en el equipo de [docs/inventario_dispositivo.md](docs/inventario_dispositivo.md)), ~5 GB libres. CPU con `dotprod` recomendada (si no la tiene, cambiar `ARM_ARCH` en `scripts/instalar_termux.sh`).
- **Termux** y **Termux:API** de la misma fuente (releases de GitHub: termux-app v0.118.3, termux-api v0.53.0).
- Permiso de micrófono para Termux:API. En Android 12+/MIUI, desactivar las restricciones de procesos secundarios ("phantom process killer").

## Instalación

En Termux (necesita internet solo durante la instalación; ~1 h, la mayor parte compilando):

```bash
pkg install -y git
git clone https://github.com/JuanD1603/jarvis-son jarvis-son
cd jarvis-son
bash scripts/instalar_termux.sh
```

El script instala paquetes, compila llama.cpp, whisper.cpp, sherpa-onnx y el orquestador en los commits fijados, ejecuta las pruebas y descarga los modelos verificando su SHA-256 contra [models/manifest.json](models/manifest.json). La configuración queda en `~/jarvis/config.ini` (plantilla: [config/config.ini.ejemplo](config/config.ini.ejemplo)).

Pasos manuales previos y control desde un PC por ADB/SSH: [bitacora/BITACORA.md](bitacora/BITACORA.md) (Fases A-D) y [docs/problemas_conocidos.md](docs/problemas_conocidos.md).

Capturas de pantalla del proceso de instalación en el teléfono, paso a paso: [docs/instalacion_capturas.md](docs/instalacion_capturas.md).

## Uso

En Termux, directamente en el teléfono:

```bash
jarvis
```

`Enter` para hablar (espera el aviso **HABLA AHORA**), `q` + `Enter` para salir. Desde un PC conectado por USB: `bash scripts/conectar.sh` y `ssh jarvis-phone jarvis`.

Otros modos: `jarvis --wav archivo.wav [--repetir N]` (pruebas automáticas) y `jarvis --mock` (sin modelos; usado en CI). Métricas por turno en `~/jarvis/logs/turnos.csv`; resumen con `python tests/metricas.py turnos.csv`.

## Desarrollo

```bash
cmake -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure
clang-format --dry-run --Werror src/*.cpp include/jarvis/*.hpp tests/unit/*.cpp
```

La CI ([.github/workflows/ci.yml](.github/workflows/ci.yml)) compila en `ubuntu-latest`, ejecuta las pruebas unitarias y un turno `--mock`, revisa el formato y rechaza pesos o audios en el repositorio.

## Limitaciones conocidas

- Latencia alta: ~16 s desde que el usuario termina de hablar hasta que empieza la respuesta (mediana).
- El STT no alcanza el criterio de 18/20; falla con nombres propios y palabras poco frecuentes. No se evaluó con ruido.
- El SLM (1,5 B parámetros) no resuelve aritmética simple y puede inventar datos.
- Sin palabra de activación, sin interrupción de la voz (*barge-in*) ni memoria entre turnos.
- Resultados de un solo teléfono y una sola persona.

## Licencias

| Componente | Licencia |
|---|---|
| Código de este repositorio | MIT (ver [LICENSE](LICENSE)) |
| llama.cpp, whisper.cpp | MIT |
| sherpa-onnx | Apache-2.0 |
| ONNX Runtime | MIT |
| Qwen2.5-1.5B-Instruct, Qwen3.5-0.8B | Apache-2.0 |
| Whisper (pesos) | MIT |
| Voces Piper | MIT; datasets: daniela = OpenSLR 61 (CC BY-SA 4.0), ald = Unlicense, claude = apache-2.0 declarado |

Los pesos no se versionan: se descargan desde su origen (repositorio, revisión y SHA-256 en `models/manifest.json`). Privacidad: [docs/privacidad.md](docs/privacidad.md).
