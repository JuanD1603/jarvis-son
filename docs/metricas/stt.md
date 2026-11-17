# Métricas del STT (Etapa 3)

whisper.cpp commit `d2c6f05` (2026-11-10), compilado en el teléfono con `armv8.2-a+dotprod+fp16`. Modelos multilingües cuantizados q5_1 (`models/manifest.json`). Parámetros comunes: `-l es -t 4 -nt -np`. Todas las cifras son **medidas** en el Redmi Note 10S el 2026-11-12.

**Criterio "utilizable":** WER <= 0,30 tras normalizar (minúsculas, sin tildes ni puntuación, enteros en palabras). Es un criterio automático y estricto: en frases de 4-6 palabras, un solo error cuenta 0,17-0,25 y dos errores ya suelen superar 0,30. Script: `tests/wer.py`, resumen `tests/resumen_stt.py`.

## 1. Prueba de funcionamiento (`samples/jfk.wav`, 11 s, inglés)

| Modelo | 2 hilos | 4 hilos | RSS |
|---|---|---|---|
| tiny q5_1 | 3419 ms | 2747 ms (RTF 0,25) | 131 MiB |
| base q5_1 | 7188 ms | 5581 ms (RTF 0,51) | 198 MiB |

## 2. Prueba automática con voz sintética (20 frases de `tests/frases_stt.txt`, voz TTS `es_MX-claude-high`, ~2,6 s por frase)

| Config | Utilizables | WER global | p50 (ms) | p95 (ms) | RSS máx |
|---|---|---|---|---|---|
| tiny, ventana 30 s | 16/20 | 0,176 | 3607 | 4180 | 130 MiB |
| base, ventana 30 s | 19/20 | 0,104 | 6874 | 8541 | 197 MiB |
| tiny, `-ac 512` | 13/20 | 0,424 | 1327 | 3639 | 110 MiB |
| base, `-ac 512` | 18/20 | 0,128 | 2357 | 2452 | 171 MiB |
| base, `-ac 512`, pista "Hola, Jarvis." | 18/20 | 0,120 | - | - | - |

Hallazgo: whisper procesa siempre una ventana de 30 s, así que con clips cortos el tiempo es casi constante. `-ac 512` (contexto de ~10 s) reduce base de 6,9 s a 2,4 s con pérdida mínima en base (en tiny la pérdida es grande). La pista `--prompt "Hola, Jarvis."` corrige "Harbys" -> "Jarvis" sin regresiones.

## 3. Evaluación con voz humana (20 frases, ambiente silencioso, una persona del equipo)

| Ronda | Config | Utilizables | WER global | p50 (ms) | p95 (ms) | RTF p50 | RSS máx |
|---|---|---|---|---|---|---|---|
| 1 | tiny, ventana 30 s | 8/20 | 0,416 | 2409 | 3037 | 0,39 | 133 MiB |
| 1 | base, ventana 30 s | 16/20 | 0,144 | 4801 | 6122 | 0,78 | 199 MiB |
| 1 | **base, `-ac 512`** | **17/20** | 0,120 | 2130 | 2567 | 0,35 | 174 MiB |
| 2 (*) | base, `-ac 512` | 9/20 | 0,304 | 2190 | 2593 | 0,36 | 174 MiB |
| 2 (*) | small, `-ac 512` | 14/20 | 0,152 | 7161 | 8069 | 1,17 | 428 MiB |
| 3 | **base, `-ac 512`** | **14/20** | 0,216 | 2104 | 2491 | 0,34 | 174 MiB |
| 3 | small, `-ac 512` | 15/20 | 0,184 | 6576 | 7917 | 1,07 | 428 MiB |

(*) Ronda 2 invalidada como medida de calidad: el aviso "habla ahora" aparecía ~0,6 s antes de que el micrófono grabara y se perdía la primera palabra ("Hola Jarvis" -> "Vis"). Se corrigió `capturar.sh` (aviso + vibración cuando el archivo ya recibe audio) antes de la ronda 3.

Rondas 1 y 3 usan la misma configuración y la misma persona; la diferencia (17 vs 14) refleja la variabilidad entre sesiones (dicción, distancia al micrófono). Ruido moderado: **no evaluado** (la prueba opcional no se realizó).

Errores típicos (ronda 3, base): "Cartago" -> "encartado", "bisiesto" -> "y sexto", "¿Qué hora es?" -> "¿Qué horas?", "Por qué" -> "Porque". Nombres propios y palabras poco frecuentes son la principal fuente de error.

## 4. Decisión y criterio del plan

- **Elegido: whisper base q5_1, `-ac 512`, 4 hilos, pista "Hola, Jarvis."**: mejor equilibrio (14-17/20, 2,1 s, 174 MiB). small no mejora de forma consistente (+1 frase en la ronda 3) y triplica la latencia.
- **Criterio del plan (>= 18/20 en silencio): NO cumplido.** Mejor resultado: 17/20 (ronda 1). Alternativas para V2: whisper small/medium con más hilos o en un móvil con más núcleos rápidos, grabación más cerca del micrófono, VAD para recortar silencios, ajuste fino (*fine-tuning*) con frases del dominio, o corrección de la transcripción por el usuario antes del SLM (el plan lo sugiere: "permitir corregir el texto").
