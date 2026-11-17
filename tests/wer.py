#!/usr/bin/env python3
"""Calcula WER por frase y tasa de transcripciones utilizables.

Entrada: archivo TSV con columnas `id<TAB>referencia<TAB>hipotesis`.
Criterio de "utilizable" (documentado en docs/metricas/stt.md): WER <= 0.30
tras normalizar (minúsculas, sin tildes ni puntuación, enteros 0-999 escritos en palabras).
"""
import sys
import unicodedata
import re

UMBRAL_UTILIZABLE = 0.30


_UNIDADES = ("cero uno dos tres cuatro cinco seis siete ocho nueve diez once doce trece catorce quince "
             "dieciseis diecisiete dieciocho diecinueve veinte veintiuno veintidos veintitres veinticuatro "
             "veinticinco veintiseis veintisiete veintiocho veintinueve").split()
_DECENAS = "_ _ _ treinta cuarenta cincuenta sesenta setenta ochenta noventa".split()
_CENTENAS = "_ ciento doscientos trescientos cuatrocientos quinientos seiscientos setecientos ochocientos novecientos".split()


def numero_a_palabras(n: int) -> str:
    """Convierte enteros 0-999 a palabras en español sin tildes (Whisper suele escribir dígitos)."""
    if n < 30:
        return _UNIDADES[n]
    if n < 100:
        d, u = divmod(n, 10)
        return _DECENAS[d] + ("" if u == 0 else " y " + _UNIDADES[u])
    if n == 100:
        return "cien"
    c, r = divmod(n, 100)
    return _CENTENAS[c] + ("" if r == 0 else " " + numero_a_palabras(r))


def normalizar(texto: str) -> list[str]:
    texto = unicodedata.normalize("NFD", texto.lower())
    texto = "".join(c for c in texto if unicodedata.category(c) != "Mn")
    texto = re.sub(r"[^a-z0-9ñ ]+", " ", texto.replace("+", " mas "))
    texto = re.sub(r"(?<!\d)\d{1,3}(?!\d)", lambda m: numero_a_palabras(int(m.group(0))), texto)
    return texto.split()


def wer(ref: list[str], hip: list[str]) -> float:
    if not ref:
        return 0.0 if not hip else 1.0
    d = list(range(len(hip) + 1))
    for i, r in enumerate(ref, 1):
        prev, d[0] = d[0], i
        for j, h in enumerate(hip, 1):
            prev, d[j] = d[j], min(d[j] + 1, d[j - 1] + 1, prev + (r != h))
    return d[len(hip)] / len(ref)


def main(ruta: str) -> None:
    filas = []
    with open(ruta, encoding="utf-8") as f:
        for linea in f:
            partes = linea.rstrip("\n").split("\t")
            if len(partes) < 2 or partes[0] == "id":
                continue
            ref, hip = partes[1], partes[2] if len(partes) > 2 else ""
            w = wer(normalizar(ref), normalizar(hip))
            filas.append((partes[0], ref, hip, w))
    if not filas:
        print("sin datos"); return
    utiles = sum(1 for f in filas if f[3] <= UMBRAL_UTILIZABLE)
    total_err = sum(f[3] * len(normalizar(f[1])) for f in filas)
    total_pal = sum(len(normalizar(f[1])) for f in filas)
    print("| id | WER | utilizable | referencia | hipótesis |")
    print("|---|---|---|---|---|")
    for i, r, h, w in filas:
        print(f"| {i} | {w:.2f} | {'si' if w <= UMBRAL_UTILIZABLE else 'no'} | {r} | {h} |")
    print()
    print(f"Frases: {len(filas)} | utilizables (WER <= {UMBRAL_UTILIZABLE}): {utiles}/{len(filas)}"
          f" | WER global: {total_err / total_pal:.3f}")


if __name__ == "__main__":
    main(sys.argv[1])
