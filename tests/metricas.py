#!/usr/bin/env python3
"""Calcula p50/p95 por etapa y del turno total a partir de turnos.csv.

Uso: metricas.py turnos.csv [--modo wav|voz] [--desde ID]
Percentil: método del rango más cercano (nearest-rank), sin interpolación.
"""
import argparse
import csv
import math

ETAPAS = ["captura", "stt", "slm", "tts", "reproduccion"]


def percentil(valores: list[float], p: float) -> float:
    datos = sorted(valores)
    if not datos:
        return float("nan")
    rango = max(1, math.ceil(p / 100 * len(datos)))
    return datos[rango - 1]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("--modo")
    ap.add_argument("--desde", type=int, default=0)
    a = ap.parse_args()
    with open(a.csv, encoding="utf-8") as f:
        filas = [r for r in csv.DictReader(f)
                 if int(r["id"]) >= a.desde and (not a.modo or r["modo"] == a.modo)]
    ok = [r for r in filas if r["codigo"] == "0"]
    print(f"Turnos: {len(filas)} | completos (codigo 0): {len(ok)} | "
          f"vacíos (codigo 2): {sum(r['codigo'] == '2' for r in filas)} | "
          f"errores (codigo 1): {sum(r['codigo'] == '1' for r in filas)}")
    print()
    print("| Etapa | n | p50 (ms) | p95 (ms) | máx (ms) | RSS pico máx (MB) |")
    print("|---|---|---|---|---|---|")
    for e in ETAPAS + ["total"]:
        ms = [float(r[f"ms_{e}"]) for r in ok if float(r[f"ms_{e}"]) > 0]
        rss = [float(r[f"rss_kb_{e}"]) for r in ok] if e != "total" else []
        if not ms:
            continue
        rss_txt = f"{max(rss) / 1024:.0f}" if rss and max(rss) > 0 else "-"
        print(f"| {e} | {len(ms)} | {percentil(ms, 50):.0f} | {percentil(ms, 95):.0f} | "
              f"{max(ms):.0f} | {rss_txt} |")


if __name__ == "__main__":
    main()
