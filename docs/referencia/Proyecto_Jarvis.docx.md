**Proyecto Asistente Local “Jarvis-son”**

*Informe de investigación técnica y diseño de infraestructura para la implementación de un asistente de voz local en un dispositivo Edge (Android)*

Juan José Sánchez, Santiago Arcos, Juan David Osorio

Inteligencia Artificial

Felipe Gutiérrez Isaza

31 de Octubre del 2026

# **Introducción**

Jarvis-son es un asistente de voz que funciona sin conexión a internet: todo el procesamiento ocurre dentro del teléfono. Su funcionamiento es un ciclo de tres fases: el audio del usuario se convierte en texto (STT, Speech-to-Text), el texto es procesado por un modelo de lenguaje (LLM) que genera la respuesta, y esa respuesta se convierte nuevamente en audio (TTS, Text-to-Speech). Ejecutar las tres fases localmente en un celular de gama media exige resolver problemas de memoria, de cómputo y de latencia, que son los que se investigan en este documento.

El dispositivo de referencia para todo el diseño es un **Xiaomi Redmi Note 10S (modelo M2101K7BL)**, por lo que todas las cifras y decisiones de este informe se ajustan a sus capacidades.

# **1\. Brecha de Conocimiento**

## **1.1 Optimización matemática: cuantización de modelos (GGUF)**

La **cuantización** consiste en reducir la precisión numérica con la que se almacenan los pesos de una red neuronal. Normalmente los pesos se guardan en coma flotante de 16 o 32 bits; al cuantizar, se representan con enteros de 8, 5, 4 o incluso menos bits. Los pesos se agrupan en bloques pequeños y cada bloque guarda un factor de escala, de modo que el valor original se reconstruye aproximadamente como: peso ≈ escala × entero cuantizado.

**GGUF** es el formato de archivo creado por el proyecto llama.cpp (librería ggml). Guarda en un solo archivo los pesos cuantizados, el tokenizador y los metadatos del modelo, y está diseñado para cargarse mediante mmap (mapeo de memoria), lo que permite arrancar rápido y ejecutarse eficientemente en CPU. Las variantes más usadas son las llamadas “K-quants”, como Q4\_K\_M, Q5\_K\_M y Q8\_0.

**Reducción de memoria.** El tamaño aproximado de un modelo es: número de parámetros × bits por peso ÷ 8\. Por ejemplo, un modelo de 1.500 millones de parámetros ocupa unos 3,1 GB en FP16, pero cerca de 1,0 GB en Q4\_K\_M (aproximadamente 4,8 bits efectivos por peso). Esto es lo que hace posible que el modelo quepa en la RAM de un celular. Además, como la generación de texto está limitada por el ancho de banda de memoria, un modelo más pequeño también se ejecuta más rápido.

**El precio: perplejidad.** La perplejidad mide qué tan “sorprendido” está el modelo ante un texto real; mientras más baja, mejor predice el lenguaje. Al cuantizar se introduce error de redondeo en los pesos, y la perplejidad sube. En la práctica, Q8\_0 es casi indistinguible del original, Q5\_K\_M y Q4\_K\_M tienen una pérdida pequeña y aceptable, y por debajo de 4 bits (Q3, Q2) la coherencia se degrada de forma notoria: aparecen repeticiones, errores gramaticales y pérdida de instrucciones. Un punto importante es que **los modelos pequeños (0,5 a 2 mil millones de parámetros) sufren más con la cuantización que los grandes**, porque tienen menos redundancia; por eso en este proyecto no se recomienda bajar de Q4\_K\_M.

## **1.2 Gestión dinámica de memoria: KV Cache**

En la arquitectura Transformer, cada token se relaciona con los anteriores mediante el mecanismo de atención, que utiliza tres vectores: Query (Q), Key (K) y Value (V). Al generar texto palabra por palabra, los vectores K y V de los tokens anteriores no cambian. El **KV Cache** los guarda en memoria para no recalcular toda la conversación en cada paso; sin él, cada token nuevo obligaría a reprocesar todo el historial.

**Impacto de n\_ctx.** El parámetro n\_ctx define cuántos tokens como máximo caben en el contexto, y llama.cpp reserva el KV Cache completo para ese tamaño al iniciar. Su consumo se calcula así:

**Memoria KV \= 2 × n\_capas × n\_ctx × n\_cabezas\_KV × dimensión\_cabeza × bytes por valor**

El crecimiento de la memoria es **lineal** con n\_ctx (lo que crece de forma cuadrática es el costo de cómputo de la atención, no la memoria). Como ejemplo, para el modelo Qwen2.5-1.5B (28 capas, 2 cabezas KV, dimensión 128, valores FP16 de 2 bytes) cada token ocupa unos 28 KB. Con n\_ctx \= 2048 el KV Cache ocupa cerca de 56 MB, mientras que con el contexto máximo del modelo (32.768 tokens) ocuparía cerca de 0,9 GB, algo inviable en este teléfono.

**Estrategias para limitar o truncar el contexto en dispositivos Edge:**

* **n\_ctx fijo y pequeño** (1024 a 2048 tokens), suficiente para una conversación de voz, que suele ser corta.

* **Ventana deslizante con n\_keep:** cuando el contexto se llena, se descartan los mensajes más antiguos pero se conserva siempre el prompt de sistema (la “personalidad” de Jarvis-son). llama.cpp permite esto con el parámetro n\_keep y el desplazamiento de contexto (context shift).

* **Resumen del historial:** los turnos antiguos se reemplazan por un resumen corto generado por el mismo modelo.

* **Cuantización del KV Cache** (por ejemplo a q8\_0), que reduce su tamaño aproximadamente a la mitad con poca pérdida de calidad.

* **Limitar la longitud de la respuesta** (n\_predict de 100 a 200 tokens), lo cual además encaja con la naturaleza de las respuestas habladas.

## **1.3 Arquitectura de software e inferencia concurrente**

**Streaming de tokens.** En lugar de esperar a que el LLM termine toda la respuesta, el modelo entrega cada token en cuanto lo genera. Esto permite empezar a trabajar con el inicio de la respuesta mientras el resto todavía se está calculando.

**Patrón Productor-Consumidor.** Dos o más tareas se comunican mediante una cola (buffer): el productor deposita elementos y el consumidor los retira a su propio ritmo, de modo que ninguno bloquea al otro. En Python se implementa con asyncio.Queue (programación asíncrona) o con queue.Queue y hilos (multihilo). En este proyecto la canalización tiene tres etapas:

* **Productor (LLM):** genera tokens y los acumula en un búfer de texto.

* **Segmentador:** cuando detecta un signo de puntuación (punto, coma, signo de interrogación o de exclamación, punto y coma) y el fragmento tiene una longitud mínima razonable, envía esa cláusula a la cola del TTS.

* **Consumidor (TTS y reproductor):** sintetiza cada cláusula y la reproduce mientras la siguiente ya se está generando.

Un detalle técnico importante: las llamadas a llama.cpp y a los motores de TTS son operaciones de C/C++ que bloquean el hilo, por lo que con asyncio deben ejecutarse en hilos separados (por ejemplo con asyncio.to\_thread o run\_in\_executor). De lo contrario, el bucle de eventos se congela y se pierde la concurrencia.

**Cómo se reduce la latencia.** Sin streaming, el usuario espera el tiempo total de generación más el tiempo total de síntesis antes de oír algo. Con la segmentación por cláusulas, el usuario empieza a escuchar en cuanto existe la primera frase, es decir, el tiempo hasta el primer audio pasa a ser: tiempo de la primera cláusula \+ síntesis de esa cláusula. La generación del resto del texto queda “oculta” detrás de la reproducción del audio. La latencia total de cómputo es la misma, pero la **latencia percibida** se reduce drásticamente.

## **1.4 Hardware y eficiencia de ejecución (SIMD)**

**SIMD** (Single Instruction, Multiple Data) son instrucciones que aplican una misma operación a varios datos a la vez. En lugar de multiplicar dos números, el procesador multiplica vectores completos en una sola instrucción. En computadores x86 existen AVX2 y AVX-512; en procesadores ARM de 64 bits existe **NEON (ASIMD)**, que trabaja con registros de 128 bits. Las CPU ARMv8.2-A, como las del teléfono de referencia, añaden además las instrucciones **dotprod** (SDOT/UDOT), que calculan productos punto de enteros de 8 bits, exactamente la operación central de un modelo cuantizado, y aritmética **FP16**.

**¿Por qué compilar con banderas nativas?** Un LLM es, en esencia, una enorme cantidad de multiplicaciones de matrices. Si llama.cpp o whisper.cpp se compilan con una configuración genérica (pensada para cualquier procesador), el compilador no puede usar las instrucciones más avanzadas del chip y recurre a rutas lentas o escalares. Al compilar para la CPU objetivo, se activan los kernels optimizados con NEON y dotprod, y la diferencia de velocidad puede ser de varias veces.

Para el Redmi Note 10S, al compilar directamente en el teléfono (Termux) se puede usar la detección nativa (-march=native o la opción GGML\_NATIVE de CMake). Al compilar en un PC para el teléfono (Android NDK), se indican explícitamente banderas como **\-march=armv8.2-a+dotprod+fp16**. Cabe aclarar que la bandera \-mfpu=neon corresponde a ARM de 32 bits (ARMv7) y no aplica en ARM64, donde NEON viene activado por defecto; lo que realmente marca la diferencia en este chip es habilitar dotprod.

# **2\. Infraestructura Utilizada**

El diseño se basa en las especificaciones reales del dispositivo del equipo, obtenidas del menú de información del teléfono y verificadas con la ficha técnica del fabricante del chip.

| Característica | Especificación |
| :---- | :---- |
| Dispositivo | Xiaomi Redmi Note 10S (M2101K7BL, versión Latinoamérica) |
| Sistema operativo | Android 13 con MIUI Global 14.0.11, kernel Linux 4.14 |
| Procesador (SoC) | MediaTek Helio G95 (MT6785), fabricado a 12 nm, arquitectura ARM64 (ARMv8.2-A) |
| Núcleos de CPU | 8 núcleos: 2 × Cortex-A76 a 2,05 GHz (alto rendimiento) \+ 6 × Cortex-A55 a 2,0 GHz (eficiencia) |
| Instrucciones SIMD | NEON (ASIMD), dotprod (SDOT/UDOT) y FP16. No soporta i8mm ni SVE |
| GPU | ARM Mali-G76 MC4 |
| RAM física | 6 GB LPDDR4X (ancho de banda teórico aproximado de 17 GB/s) |
| RAM virtual | \+2 GB (extensión de memoria de MIUI, usa el almacenamiento interno) |
| Almacenamiento | UFS 2.2 |

## **2.1 Restricción de memoria**

El teléfono tiene 6 GB de RAM física, pero Android 13 y MIUI, junto con los servicios en segundo plano, ocupan normalmente entre 2,5 y 3,5 GB. Los 2 GB adicionales de “RAM virtual” no son memoria real: son un espacio de intercambio (swap) en el almacenamiento interno, cientos de veces más lento que la RAM, por lo que **no deben contarse dentro del presupuesto** del asistente.

Con base en esto, se fija un **presupuesto estricto de 2,0 GB** para todo el sistema (STT \+ LLM \+ TTS \+ programa principal), con un objetivo de diseño de **1,7 GB** para dejar margen de seguridad.

**¿Qué pasa si se excede el presupuesto, aunque sea por 1 MB?** Android no tiene un aviso previo: cuando la memoria libre cae por debajo de los umbrales del sistema, el demonio **lmkd (Low Memory Killer)**, y en último caso el **OOM Killer** del kernel de Linux, eliminan procesos sin advertencia. Si el proceso del asistente es el que más memoria consume, será el primero en ser destruido y la conversación se cortará de golpe. Antes de eso suele aparecer otro síntoma: como llama.cpp carga el modelo mediante mmap, el sistema puede descartar páginas del modelo y volver a leerlas del almacenamiento, lo que provoca un desplome de la velocidad (thrashing). Adicionalmente, desde Android 12 existe el “phantom process killer”, que puede cerrar procesos hijos de Termux; en Android 13 se desactiva desde las Opciones de desarrollador (“Desactivar restricciones de procesos secundarios”).

## **2.2 Capacidad de cómputo**

La unidad de procesamiento que se usará es la **CPU ARM**. Aunque el teléfono tiene una GPU Mali-G76 MC4, el soporte de llama.cpp y whisper.cpp para GPU Mali (mediante Vulkan u OpenCL) es limitado e inestable, y en la práctica suele ser igual o más lento que la CPU en este tipo de chip. El acelerador APU de MediaTek tampoco es utilizado por estas librerías. Por lo tanto, todo el cálculo matricial recae sobre los núcleos de la CPU.

Esto tiene tres consecuencias en el cálculo de matrices:

* **Dependencia total de SIMD:** sin NEON y dotprod, la multiplicación de matrices sería varias veces más lenta.

* **Arquitectura big.LITTLE:** solo 2 de los 8 núcleos son de alto rendimiento. Usar los 8 hilos no siempre es más rápido, porque los núcleos A55 retrasan a los A76 en cada sincronización. Se debe medir con llama-bench usando 2, 4 y 6 hilos para elegir el valor óptimo.

* **Límite por ancho de banda:** al generar texto, cada token exige leer todos los pesos del modelo desde la RAM. Con aproximadamente 17 GB/s de ancho de banda y un modelo de 1 GB, el límite teórico ronda los 17 tokens por segundo; en la práctica se espera bastante menos. Además, el chip puede reducir su frecuencia por temperatura (thermal throttling) durante sesiones largas.

## **2.3 Ecosistema de software**

Como punto de partida se usarán motores de inferencia escritos en C/C++, sin dependencias pesadas y compilables directamente para ARM64:

* **llama.cpp** (basado en la librería ggml): motor de inferencia para el LLM en formato GGUF. Incluye la herramienta llama-bench para medir el rendimiento.

* **whisper.cpp** (también basado en ggml): motor de reconocimiento de voz para modelos Whisper.

* **sherpa-onnx** (C++ sobre ONNX Runtime): motor para ejecutar voces Piper/VITS para el TTS, con soporte nativo para Android.

* **Entorno:** Termux (terminal Linux en Android) con clang y CMake para compilar en el propio teléfono, o el Android NDK para compilación cruzada desde un PC. Opcionalmente, Python con los enlaces llama-cpp-python para orquestar el ciclo.

# **3\. Modelos Seleccionados**

La selección prioriza el buen manejo del idioma español y el respeto del presupuesto de memoria de 1,7 a 2,0 GB. Los consumos de RAM son estimados y deben confirmarse con mediciones en el teléfono.

| Fase | Modelo propuesto | Motor | Tamaño / RAM estimada |
| :---- | :---- | :---- | :---- |
| STT | Whisper Base multilingüe, cuantizado Q5\_1 | whisper.cpp | Archivo ≈ 60 MB; RAM ≈ 150–200 MB |
| LLM | Qwen2.5-1.5B-Instruct, GGUF Q4\_K\_M, n\_ctx \= 2048 | llama.cpp | Archivo ≈ 1,0 GB; RAM ≈ 1,1–1,3 GB (incluye KV Cache ≈ 56 MB) |
| TTS | Voz Piper en español, calidad media (por ejemplo es\_MX-ald-medium) | sherpa-onnx | Archivo ≈ 60 MB; RAM ≈ 100–150 MB |
| Total |  |  | ≈ 1,4–1,7 GB (dentro del presupuesto) |

**Justificación de cada elección:**

* **STT – Whisper Base:** el modelo Tiny es más rápido pero comete muchos errores en español; Base ofrece un mejor equilibrio entre precisión y velocidad. Si la latencia resulta excesiva, Tiny queda como alternativa. Se debe usar la versión multilingüe (no la terminada en “.en”, que solo entiende inglés).

* **LLM – Qwen2.5-1.5B-Instruct:** tiene buen soporte multilingüe, incluido el español, y sigue bien las instrucciones para su tamaño. Gracias a los 6 GB del teléfono es posible usar un modelo de 1,5 mil millones de parámetros en lugar de uno de 0,5. Alternativas: Llama-3.2-1B-Instruct Q4\_K\_M (≈ 0,8 GB, más rápido) y, como plan de contingencia, Qwen2.5-0.5B-Instruct Q4\_K\_M (≈ 0,4 GB).

* **TTS – Piper vía sherpa-onnx:** voces neuronales ligeras que se sintetizan más rápido que el tiempo real en CPU ARM, con acento latinoamericano disponible. Alternativa sin costo de memoria: el motor TTS nativo de Android, aunque depende de que la voz en español esté descargada para uso sin conexión.

# **4\. Plan de Validación**

Antes de construir el asistente completo, se medirá cada componente por separado en el teléfono:

* Medir la RAM libre real con el teléfono en reposo (comando free \-m en Termux) y ajustar el presupuesto si es necesario.

* Ejecutar llama-bench con 2, 4 y 6 hilos para obtener la velocidad de procesamiento del prompt y de generación (tokens por segundo).

* Medir el tiempo que tarda whisper.cpp en transcribir una frase de 5 segundos.

* Medir el tiempo de síntesis de una frase corta con sherpa-onnx.

* Verificar el consumo máximo de memoria con los tres modelos cargados simultáneamente.

Con estos datos se confirmará o ajustará la selección de modelos antes de integrar el ciclo completo de Jarvis-son.