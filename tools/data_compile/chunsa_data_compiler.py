#!/usr/bin/env python3
"""chunsa_data_compiler — YAML restringido → blob canónico (base §2.8, SPEC-001 §15).

Autor: Arquitecto (regla de fallback: 2 intentos delegados agotados por timeout).
Dos pasos: schema_validate (a mano, contrato unit v1) + semantic_compile.
El motor NUNCA parsea YAML: consume este blob. Blob determinista → mismo sha256
en cualquier máquina (sin timestamps ni rutas).
"""
import argparse
import hashlib
import re
import struct
import sys
from pathlib import Path

import yaml

MAX_FILE = 1 << 20
MAX_DEPTH = 8
CLASSES = ["infantry", "cavalry", "artillery", "siege", "naval_light", "citizen"]
TAGS = ["can_take_cover", "formation_capable", "suppression_resist_low",
        "suppression_resist_high", "drop_off_carrier"]
RESOURCES = ["A", "B", "P", "W", "Me", "F", "I", "El"]
BONUS_KEYS = ["infantry", "cavalry", "artillery", "siege", "naval_light"]
STATUS = {"H": 0, "C": 1, "P": 2, "X": 3}
ID_RE = re.compile(r"^[a-z0-9_]+:[a-z0-9_]+$")
MODULE_RE = re.compile(r"^[a-z0-9_]+$")
STATS = {"hp": (1, 1000000), "attack": (0, 1000000), "range_millitiles": (0, 100000),
         "speed_millitile_tick": (1, 100000), "morale": (0, 100),
         "build_time_ticks": (1, 1000000)}


class RestrictedLoader(yaml.SafeLoader):
    """Perfil restringido (base §2.4.1): sin aliases/anchors, sin claves duplicadas."""

    def compose_node(self, parent, index):
        if self.check_event(yaml.AliasEvent):
            raise yaml.YAMLError("aliases/anchors prohibidos")
        return super().compose_node(parent, index)

    def construct_mapping(self, node, deep=False):
        seen = set()
        for k_node, _ in node.value:
            k = self.construct_object(k_node, deep=True)
            if k in seen:
                raise yaml.YAMLError(f"clave duplicada: {k}")
            seen.add(k)
        return super().construct_mapping(node, deep)


def find_floats(obj, path=""):
    """Los floats están prohibidos en todo el documento (cantidades enteras)."""
    if isinstance(obj, float):
        yield path or "(raíz)"
    elif isinstance(obj, dict):
        for k, v in obj.items():
            yield from find_floats(v, f"{path}.{k}" if path else str(k))
    elif isinstance(obj, list):
        for i, v in enumerate(obj):
            yield from find_floats(v, f"{path}[{i}]")


def depth_of(obj, d=1):
    if isinstance(obj, dict):
        return max([d] + [depth_of(v, d + 1) for v in obj.values()])
    if isinstance(obj, list):
        return max([d] + [depth_of(v, d + 1) for v in obj])
    return d


def is_int(v):
    return isinstance(v, int) and not isinstance(v, bool)


def validate_unit(doc, err):
    """Paso 1: schema a mano (contrato unit v1). Acumula mensajes en err."""
    if not isinstance(doc, dict):
        err.append("el documento raíz debe ser un mapa")
        return
    known = {"schema_version", "id", "module", "epoch_window", "class", "tags",
             "costs", "stats", "provenance", "bonus_vs_bp"}
    for k in doc:
        if k not in known:
            err.append(f"campo desconocido: {k}")
    for k in ("schema_version", "id", "module", "epoch_window", "class", "tags",
              "costs", "stats", "provenance"):
        if k not in doc:
            err.append(f"falta el campo requerido: {k}")
    if doc.get("schema_version") != 1:
        err.append("schema_version debe ser 1")
    if not (isinstance(doc.get("id"), str) and ID_RE.match(doc.get("id", ""))):
        err.append("id inválido (formato modulo:unidad, [a-z0-9_])")
    if not (isinstance(doc.get("module"), str) and MODULE_RE.match(doc.get("module", ""))):
        err.append("module inválido ([a-z0-9_])")
    ew = doc.get("epoch_window")
    if not (isinstance(ew, list) and len(ew) == 2 and all(is_int(x) and 1 <= x <= 15 for x in ew)):
        err.append("epoch_window debe ser [int 1..15, int 1..15]")
    if doc.get("class") not in CLASSES:
        err.append(f"class inválida (∈ {CLASSES})")
    tags = doc.get("tags")
    if not isinstance(tags, list) or any(t not in TAGS for t in tags) or len(set(tags)) != len(tags or []):
        err.append("tags inválidos (lista sin duplicados del catálogo)")
    costs = doc.get("costs")
    if not isinstance(costs, dict) or not costs or any(k not in RESOURCES for k in costs) \
            or any(not (is_int(v) and v >= 0) for v in costs.values()):
        err.append("costs inválidos (dict no vacío de recursos con enteros >=0)")
    stats = doc.get("stats")
    if not isinstance(stats, dict) or set(stats) != set(STATS):
        err.append(f"stats debe tener exactamente las claves {sorted(STATS)}")
    else:
        for k, (lo, hi) in STATS.items():
            v = stats[k]
            if not (is_int(v) and lo <= v <= hi):
                err.append(f"stats.{k} fuera de rango [{lo},{hi}] o no entero")
    bv = doc.get("bonus_vs_bp", {})
    if not isinstance(bv, dict) or any(k not in BONUS_KEYS for k in bv) \
            or any(not (is_int(v) and -10000 <= v <= 10000) for v in bv.values()):
        err.append("bonus_vs_bp inválido")
    prov = doc.get("provenance")
    if not isinstance(prov, dict) or prov.get("status") not in STATUS \
            or any(k not in ("status", "sources") for k in prov) \
            or not all(isinstance(s, str) for s in prov.get("sources", [])):
        err.append("provenance inválida (status H/C/P/X; sources lista de strings)")


def load_all(src_dir):
    """Carga + valida todos los .yaml. Devuelve (unidades_válidas, lista_de_errores)."""
    errors = []
    units = []
    ids = {}
    files = sorted(Path(src_dir).glob("*.yaml"))
    if not files:
        errors.append(("(src)", "no hay archivos .yaml"))
    for f in files:
        rel = f.name
        try:
            raw = f.read_bytes()
        except OSError as e:
            errors.append((rel, f"E/S: {e}"))
            continue
        if len(raw) > MAX_FILE:
            errors.append((rel, "archivo excede 1 MiB"))
            continue
        try:
            doc = yaml.load(raw.decode("utf-8"), Loader=RestrictedLoader)
        except (yaml.YAMLError, UnicodeDecodeError) as e:
            errors.append((rel, f"yaml: {e}"))
            continue
        if depth_of(doc) > MAX_DEPTH:
            errors.append((rel, "profundidad de anidamiento > 8"))
            continue
        file_errs = [f"float prohibido en {p}" for p in find_floats(doc)]
        validate_unit(doc, file_errs)
        # Paso 2: semántica (solo si la forma es sana para leer los campos)
        if not file_errs:
            uid, mod = doc["id"], doc["module"]
            if uid.split(":", 1)[0] != mod:
                file_errs.append("id fuera de su namespace (prefijo != module)")
            if doc["epoch_window"][0] > doc["epoch_window"][1]:
                file_errs.append("ventana de época invertida")
            if uid in ids:
                file_errs.append(f"id duplicado (también en {ids[uid]})")
            else:
                ids[uid] = rel
        if file_errs:
            errors.extend((rel, m) for m in file_errs)
        else:
            units.append(doc)
    return units, sorted(errors)


def pack_str(w, s, wide=True):
    b = s.encode("utf-8")
    w += struct.pack("<H" if wide else "<B", len(b)) + b
    return w


def compile_blob(units):
    """Paso 3: blob canónico determinista (little-endian, ordenado por id)."""
    out = b"CBLB" + struct.pack("<I", 1)
    out = pack_str(out, "unit", wide=False)
    out += struct.pack("<I", len(units))
    for u in sorted(units, key=lambda d: d["id"].encode("utf-8")):
        out = pack_str(out, u["id"])
        out = pack_str(out, u["module"])
        out += struct.pack("<BB", u["epoch_window"][0], u["epoch_window"][1])
        out += struct.pack("<B", CLASSES.index(u["class"]))
        mask = 0
        for t in u["tags"]:
            mask |= 1 << TAGS.index(t)
        out += struct.pack("<B", mask)
        costs = sorted((RESOURCES.index(k), v) for k, v in u["costs"].items())
        out += struct.pack("<B", len(costs))
        for idx, v in costs:
            out += struct.pack("<BI", idx, v)
        s = u["stats"]
        out += struct.pack("<IIIIBI", s["hp"], s["attack"], s["range_millitiles"],
                           s["speed_millitile_tick"], s["morale"], s["build_time_ticks"])
        bonus = sorted((BONUS_KEYS.index(k), v) for k, v in u.get("bonus_vs_bp", {}).items())
        out += struct.pack("<B", len(bonus))
        for idx, v in bonus:
            out += struct.pack("<Bi", idx, v)
        prov = u["provenance"]
        out += struct.pack("<BB", STATUS[prov["status"]], len(prov.get("sources", [])))
        for src in prov.get("sources", []):
            out = pack_str(out, src)
    return out


def main():
    ap = argparse.ArgumentParser(prog="chunsa_data_compiler")
    ap.add_argument("cmd", choices=["compile", "validate"])
    ap.add_argument("src_dir")
    ap.add_argument("--schema", required=True, choices=["unit"])
    ap.add_argument("--out")
    ap.add_argument("--print-hash", action="store_true")
    a = ap.parse_args()

    units, errors = load_all(a.src_dir)
    if errors:
        for f, m in errors:
            print(f"ERROR {f}: {m}")
        return 1
    if a.cmd == "validate":
        print(f"OK: {len(units)} unidades válidas")
        return 0
    if not a.out:
        print("ERROR (cli): compile requiere --out", file=sys.stderr)
        return 2
    blob = compile_blob(units)
    try:
        Path(a.out).write_bytes(blob)
    except OSError as e:
        print(f"ERROR E/S: {e}", file=sys.stderr)
        return 2
    if a.print_hash:
        print(f"blob sha256: {hashlib.sha256(blob).hexdigest()}")
        print(f"unidades: {len(units)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
