#!/usr/bin/env python3
"""Resume eval_stt.tsv: por configuración y condición, utilizables (WER <= 0.30), WER global, p50/p95 de tiempo, RTF y RSS."""
import csv
import math
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from wer import UMBRAL_UTILIZABLE, normalizar, wer  # noqa: E402


def percentil(v: list[float], p: float) -> float:
    v = sorted(v)
    return v[max(1, math.ceil(p / 100 * len(v))) - 1]


def main(ruta: str) -> None:
    grupos: dict[tuple[str, str], list[dict[str, str]]] = defaultdict(list)
    with open(ruta, encoding="utf-8") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            grupos[(r["condicion"], r["config"])].append(r)
    print("| Condición | Config | n | Utilizables | WER global | p50 (ms) | p95 (ms) | RTF p50 | RSS máx (MiB) |")
    print("|---|---|---|---|---|---|---|---|---|")
    for (cond, cfg), filas in sorted(grupos.items()):
        errores = palabras = utiles = 0
        for r in filas:
            ref, hip = normalizar(r["referencia"]), normalizar(r["hipotesis"])
            w = wer(ref, hip)
            utiles += w <= UMBRAL_UTILIZABLE
            errores += w * len(ref)
            palabras += len(ref)
        ms = [float(r["ms"]) for r in filas]
        rtf = [float(r["ms"]) / 1000 / float(r["dur_s"]) for r in filas]
        rss = max(float(r["rss_kb"] or 0) for r in filas) / 1024
        print(f"| {cond} | {cfg} | {len(filas)} | {utiles}/{len(filas)} | {errores / palabras:.3f} | "
              f"{percentil(ms, 50):.0f} | {percentil(ms, 95):.0f} | {percentil(rtf, 50):.2f} | {rss:.0f} |")


if __name__ == "__main__":
    main(sys.argv[1])
