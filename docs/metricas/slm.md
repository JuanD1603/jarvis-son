# Métricas del SLM (Etapa 4)

Dispositivo: Redmi Note 10S (Helio G95, 6 GB). llama.cpp commit `92d6c8a` (2026-11-12), clang 21.1.8, `-march=armv8.2-a+dotprod+fp16`. Todas las cifras son **medidas** en el teléfono el 2026-11-12, con el PC conectado por USB y el teléfono cargando.

## 1. llama-bench (`-p 128 -n 64 -r 3`, tokens/s)

| Modelo | Archivo | 2 hilos pp128 / tg64 | 4 hilos pp128 / tg64 | 6 hilos pp128 / tg64 |
|---|---|---|---|---|
| Qwen3.5-0.8B Q4_0 | 526,5 MiB | **84,3** / **16,9** | 61,9 / 15,0 | 72,3 / 13,3 |
| Qwen2.5-0.5B-Instruct Q4_K_M | 463,0 MiB | 42,5 / **20,1** | 37,6 / 18,3 | 44,6 / 19,5 |
| Qwen2.5-1.5B-Instruct Q4_K_M | 1,04 GiB | 25,1 / **9,5** | 26,3 / 8,6 | 33,0 / 8,4 |

Conclusión: la **generación** (lo que domina una respuesta de voz) es más rápida con **2 hilos** en los tres modelos; 4 y 6 hilos meten núcleos A55 que frenan a los A76 en cada sincronización. Se fija `-t 2`. Tablas completas: `llama_bench_*.md`.

## 2. Prueba de calidad: 10 preguntas (`tests/prompts_slm.txt`)

Parámetros: `-c 1024 -n 96 --temp 0.7 -s 42 -t 2`, prompt de sistema del plan, plantilla ChatML escrita por archivo (`-f`), `-no-cnv --no-display-prompt --no-warmup`. Tiempo = proceso completo (carga del modelo + prompt + generación), medido con `date` alrededor de `command time -v`; RSS = "Maximum resident set size".

| Modelo | Opciones | n | p50 (ms) | p95 (ms) | RSS pico (MiB) | tok/s gen. (mediana) | tokens generados (mediana) |
|---|---|---|---|---|---|---|---|
| Qwen3.5-0.8B Q4_0 | repack (por defecto) + `<think></think>` vacío | 10 | 5290 | 9672 | 1170 | 16,9 | 22 |
| Qwen3.5-0.8B Q4_0 | `-nr` + `<think></think>` vacío | 10 | 5383 | 11479 | 645 | 13,7 | 22 |
| Qwen2.5-1.5B-Instruct Q4_K_M | repack (por defecto) | 10 | 10579 | 13780 | **2055** | - | - |
| Qwen2.5-1.5B-Instruct Q4_K_M | `-nr` | 10 | **9836** | 13643 | **1151** | 7,4 | 31 |
| Qwen2.5-0.5B-Instruct Q4_K_M | repack (por defecto) | 10 | 5400 | 7949 | 732 | - | - |

"-" = no capturado en esa corrida (el script de la primera corrida no extraía tokens/s; se corrigió).

### Hallazgo: el *repack* de pesos duplica la RAM

En CPU ARM con dotprod, llama.cpp reordena ("repack") los pesos Q4 a un formato intercalado en memoria anónima, **además** de las páginas del archivo mapeado con `mmap`. Con Qwen2.5-1.5B el pico pasa de 1151 MiB (`-nr`) a 2055 MiB (por defecto), por encima del máximo de 2,0 GB. La velocidad de generación con `-nr` es similar (10,5 vs 11,4 tok/s en una prueba corta de 32 tokens); el procesamiento del prompt sí es más lento (14,8 vs 26,0 tok/s), pero los prompts de voz son cortos.

### Observación térmica

En la corrida de Qwen2.5-1.5B `-nr` la velocidad de generación bajó de ~10,4 a ~7,4 tok/s a partir de la pregunta 3 y se mantuvo así (posible *thermal throttling* o migración de hilos a núcleos A55). Se repetirá en la prueba térmica de la Fase J.

### Calidad (evaluación cualitativa del equipo integrador)

Criterio: respuesta correcta, en español, 1-3 frases cortas, sin listas.

| # | Pregunta | Qwen3.5-0.8B | Qwen2.5-1.5B | Qwen2.5-0.5B |
|---|---|---|---|---|
| 1 | Saludo | ok | ok | ok |
| 2 | Capital de Colombia | ok | ok | ok |
| 3 | 15 x 4 | error ("3.75") | error ("veinte") | error ("3,75") |
| 4 | Consejo para dormir | aceptable | aceptable | **lista numerada**, cortada |
| 5 | Qué es IA | ok (algo largo) | ok | impreciso |
| 6 | Fotosíntesis | 3 frases largas, un error ("corteza") | ok, breve | impreciso |
| 7 | Recomendación | incoherente | aceptable | incoherente |
| 8 | Traducir "mariposa" | responde en inglés sin traducir bien | **correcto** ("A butterfly") | error ("mosquiela") |
| 9 | Cielo azul | parcialmente erróneo (nubes) | parcialmente erróneo, mezcla "sun" | erróneo |
| 10 | Dato de océanos | dudoso | ok | erróneo |
| | **Respuestas aceptables** | **5/10** | **7/10** | **3/10** |

Ningún modelo resuelve bien aritmética simple: es una limitación conocida a documentar.

## 3. Decisión

Regla (ADR-004): el modelo de mejor calidad con pico RSS <= 1,7 GB y p50 <= ~10 s para 96 tokens.

- Qwen2.5-1.5B-Instruct Q4_K_M **con `-nr`**: RSS 1151 MiB, p50 9,8 s, 7/10. **Cumple y es el de mejor calidad: elegido.**
- Qwen3.5-0.8B Q4_0 (modelo base del plan): cumple con holgura (p50 5,3 s), 5/10. Queda como **alternativa rápida** configurable en `config.ini`.
- Qwen2.5-0.5B: cumple, pero 3/10. Solo contingencia.

Riesgo asumido: el p50 del 1.5B está en el límite y el p95 (13,6 s) lo supera; si en la integración el turno total resulta excesivo, se cambia a Qwen3.5-0.8B editando una línea de la configuración.
