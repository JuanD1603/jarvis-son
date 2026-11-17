# Métricas del TTS (Etapa 5)

sherpa-onnx **v1.13.8** (`11afbd0`) compilado en Termux con `onnxruntime` 1.31.1 del repositorio de Termux (ADR-008). Voces Piper desde la release `tts-models` de sherpa-onnx (sha256 en `models/manifest.json`). Mediciones en el Redmi Note 10S, 2026-11-12. Todas las voces funcionan **sin red** (espeak-ng integrado en el binario).

## 1. Identificación y licencias

| Voz | Variante | Calidad | Muestreo | Archivo | Licencia voz | Dataset |
|---|---|---|---|---|---|---|
| `es_MX-ald-medium` | México | medium | 22 050 Hz | 64 MiB (tar.bz2) | MIT (piper-voices) | Ald Mexican Spanish, Unlicense |
| `es_MX-claude-high` | México | high | 22 050 Hz | 64 MiB | MIT | HirCoir/Piper-TTS-Spanish, declarado apache-2.0 |
| **`es_AR-daniela-high`** | Argentina | high | 22 050 Hz | 110 MiB | MIT | OpenSLR 61, **CC BY-SA 4.0** (requiere atribución y compartir igual si se redistribuye el dataset) |
| Motor de Android (`termux-tts-speak`) | `es-MX` | - | - | - | Google | - |

## 2. Rendimiento (frase de ~5 s de audio; tiempo de proceso completo incluida la carga)

| Voz | Hilos | Tiempo (ms) | RTF síntesis | RSS pico |
|---|---|---|---|---|
| es_MX-ald-medium | 2 / 4 | 3708 / 3501 | 0,22 / 0,21 | 189 MiB |
| es_MX-claude-high | 2 / 4 | 3768 / 3352 | 0,22 / 0,20 | 182 MiB |
| es_AR-daniela-high | 2 / 4 / 6 / **8** | 8783 / 7790 / 7627 / **7125** | 1,34 / 1,15 / 1,07 / **0,92** | 206 MiB |

daniela-high es un modelo `high` más grande: ~2x más lento que las otras. En los turnos reales con respuestas largas (225-241 caracteres) el TTS llegó a 19-21 s (pico RSS 433 MiB con texto largo).

## 3. Evaluación humana (escala 1-5; 3 frases por voz; una persona del equipo)

| Voz | Pronunciación | Ritmo | Naturalidad | Sesión |
|---|---|---|---|---|
| es_MX-ald-medium | 1,7 | 2,0 | 1,3 | 1 |
| es_MX-claude-high | 2,0 | 1,7 | 1,3 | 1 |
| es_MX-ald-medium | 2,0 | 2,0 | 2,0 | 2 |
| es_MX-claude-high | 2,7 | 3,0 | 2,7 | 2 |
| **es_AR-daniela-high** | **4,0** | **4,0** | **4,0** | 2 |
| Motor de Android (es-MX) | 3,3 | 3,3 | 3,3 | 2 |

Preferida en la sesión 2: **es_AR-daniela-high**. El motor de Android se escuchó correctamente en **modo avión** (la voz es-MX está descargada en este teléfono; no se puede garantizar en otros).

## 4. Decisión

- **Elegida: `es_AR-daniela-high`, 8 hilos** (mejor calificada; acento rioplatense, no mexicano, pero latinoamericano).
- Alternativa rápida: `es_MX-claude-high` con 4 hilos (~2x más rápida, peor calificada). Se cambia en `config.ini` (`tts.nombre_voz`, `tts.hilos`).
- Fallback del plan (`termux-tts-speak`): funciona sin red en este equipo, pero no genera WAV ni permite medir; no se integra en V1.
