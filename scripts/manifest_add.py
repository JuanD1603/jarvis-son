#!/usr/bin/env python3
"""Añade o actualiza una entrada en models/manifest.json (clave: nombre de archivo)."""
import json
import sys
from pathlib import Path

RUTA = Path(__file__).resolve().parent.parent / "models" / "manifest.json"
CAMPOS = ["archivo", "componente", "repo", "url", "revision", "licencia", "tamano_bytes", "sha256", "fecha", "notas"]


def main(argv: list[str]) -> None:
    if len(argv) != len(CAMPOS):
        sys.exit("uso: manifest_add.py " + " ".join(CAMPOS))
    entrada = dict(zip(CAMPOS, argv))
    entrada["tamano_bytes"] = int(entrada["tamano_bytes"])
    datos = json.loads(RUTA.read_text(encoding="utf-8"))
    datos["modelos"] = [m for m in datos["modelos"] if m["archivo"] != entrada["archivo"]] + [entrada]
    RUTA.write_text(json.dumps(datos, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main(sys.argv[1:])
