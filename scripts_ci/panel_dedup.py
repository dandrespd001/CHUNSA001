#!/usr/bin/env python3
"""De-duplica las respuestas del panel.

El proxy de OmniRoute concatena los chunks de streaming de forma acumulativa:
cada segmento repite todo lo anterior. Sin esto las respuestas de las rutas
Gemini son ilegibles. Escribe <fichero>.clean.md junto al original.
"""
import re, sys, pathlib

ENTRY = re.compile(r'\[[VI?]\][^\[]{10,}?->\s*\[CHUNSA\][^\[]{10,}?\.(?=\s|$)')

def clean(path: pathlib.Path) -> int:
    txt = path.read_text(encoding='utf-8', errors='replace')
    seen, out = set(), []
    for m in ENTRY.finditer(txt):
        e = ' '.join(m.group(0).split())
        if e not in seen:
            seen.add(e)
            out.append(e)
    if not out:                      # no era formato de panel: dejar tal cual
        return 0
    path.with_suffix('.clean.md').write_text('\n'.join(out) + '\n', encoding='utf-8')
    return len(out)

if __name__ == '__main__':
    for f in sys.argv[1:]:
        p = pathlib.Path(f)
        if not p.is_file():
            continue
        n = clean(p)
        print(f"  {p.name}: {n} entradas unicas" if n else f"  {p.name}: sin formato de panel, intacto")
