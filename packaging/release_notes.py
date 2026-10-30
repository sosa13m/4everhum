#!/usr/bin/env python3
# Arma el texto de una Release de GitHub: como descargar + la seccion de esa
# version en CHANGELOG.md.  Uso: python3 packaging/release_notes.py v1.1.0
import pathlib
import re
import sys

tag = sys.argv[1] if len(sys.argv) > 1 else ""
version = tag.lstrip("v")
root = pathlib.Path(__file__).resolve().parent.parent
changelog = (root / "CHANGELOG.md").read_text(encoding="utf-8")

# La seccion "## <version>" hasta la siguiente "## ".
match = re.search(r"^## " + re.escape(version) + r"\s*$(.*?)(?=^## |\Z)", changelog, re.M | re.S)
section = match.group(1).strip() if match else "Ver CHANGELOG.md."

print(f"""## Descargar

| Sistema | Archivo |
|---|---|
| **Windows** 10/11 (64 bits) | `4EVERHUM-windows.zip` → descomprime y doble clic en `instalar.bat` |
| **macOS** 10.15+ (Apple Silicon e Intel) | `4EVERHUM-macos.zip` → descomprime y abre `instalar.command` |

Formatos: VST3 (Windows y Mac), AU (Mac) y app sin DAW. Instrucciones completas en el
`LEEME` que trae cada zip y en el [README](https://github.com/sosa13m/4everhum#readme).

El plugin no está firmado digitalmente: Windows o macOS pueden pedir confirmación
la primera vez.

## Novedades

{section}

---
Software libre bajo AGPLv3, sin garantía.
""")
