# Privacidad

Jarvis-son V1 procesa la voz **solo en el teléfono**. Después de descargar modelos y voz, no necesita red (verificado en modo avión, ver `docs/metricas/resultados_v1.md`).

## Qué se guarda y qué no

| Dato | Dónde | Duración |
|---|---|---|
| Audio grabado (`tmp/in.wav`) | Teléfono | Se borra al final de cada turno, con éxito o con error |
| Transcripción, prompt y respuesta (`tmp/stt.txt`, `tmp/prompt.txt`, `tmp/slm.txt`) | Teléfono | Se borran al final de cada turno |
| Audio sintetizado (`tmp/out.wav`) | Teléfono | Se borra al final de cada turno |
| `logs/turnos.csv` | Teléfono | Permanente; **solo métricas** (tiempos, longitudes en caracteres, RSS, códigos). Sin texto ni audio |
| `logs/<etapa>.err` | Teléfono | Se sobrescribe cada turno; contiene mensajes técnicos de las herramientas. El de STT puede incluir fragmentos transcritos: no publicar |
| Evaluación STT (`logs/eval_stt_*.tsv`) | Teléfono / `docs/metricas/datos` | Contiene solo frases de prueba predefinidas (`tests/frases_stt.txt`), sin datos personales. Los audios se borran inmediatamente |

## Reglas del repositorio

- `.gitignore` excluye pesos (`*.gguf`, `*.bin`, `*.onnx`), audio (`*.wav`, `*.m4a`, `*.mp3`), `tmp/` y `bitacora/privado/`. La CI rechaza esas extensiones.
- `docs/inventario_dispositivo.md` no incluye IMEI, número de serie ni cuentas.
- No se almacenan tokens ni claves: la clave SSH del PC vive en `~/.ssh/` del PC y no se versiona; `scripts/termux_bootstrap_ssh.sh` la recibe como argumento.
- Las frases de evaluación son genéricas. Grabar solo con consentimiento de quien habla.
