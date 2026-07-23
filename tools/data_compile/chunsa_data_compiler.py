#!/usr/bin/env python3
"""Offline, deterministic SPEC-002 CHDB v1 compiler.

The module deliberately contains both writer and hostile-input reader: a blob is
never published until the reader accepts exactly the bytes produced by writer.
"""
from __future__ import annotations

import argparse
import datetime as _dt
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import struct
import sys
import tempfile
import unicodedata

import yaml
from jsonschema import Draft202012Validator
from referencing import Registry, Resource

MAX_FILE = 1 << 20
MAX_MAP_FILE = 16 << 20
MAX_BLOB = 64 << 20
MAX_STRING = 65535
MAX_DEPTH = 16
MAX_COLLECTION = 65535
KIND_INFO = (("manifest", "manifest.yaml", 1, 1), ("unit", "units", 2, 65535),
             ("building", "buildings", 3, 65535), ("tech", "tech", 4, 65535),
             ("civ", "civilizations", 5, 1024), ("map", "maps", 6, 1024),
             ("ai-profile", "ai_profiles", 7, 1024))
KIND_BY_DIR = {x[1]: x for x in KIND_INFO[1:]}
RESOURCES = {"A", "B", "P", "W", "Me", "F", "I", "El"}
PLAIN_INT = re.compile(r"-?(0|[1-9][0-9]*)$")
DATE_PLAIN = re.compile(r"\d{4}-\d{2}-\d{2}$")
FLOAT_PLAIN = re.compile(
    r"(?:[+-]?(?:(?:\d[\d_]*\.\d*|\d*\.\d[\d_]*)(?:[eE][+-]?\d[\d_]*)?|"
    r"\d[\d_]*[eE][+-]?\d[\d_]*|\.inf|\.nan))$", re.I)
PREFIXED_OR_ODD_INT = re.compile(
    r"(?:\+\d+|-?(?=[0-9_]*\d)[0-9_]*_[0-9_]*|-?0[xX][0-9a-fA-F_]+|-?0[oO][0-7_]+|"
    r"-?0[bB][01_]+|-?0[0-9]+|-?\d+(?::[0-5]?\d)+)$")

class CompileError(Exception):
    def __init__(self, code, message): self.code, self.message = code, message

class ValidationIoError(Exception):
    """Validation already printed one or more stable E_IO diagnostics."""

def _err(errors, code, kind, ident, pointer, message):
    errors.append((kind, ident or "", pointer, code, f"ERROR {code} {kind} {ident or '-'} {pointer}: {message}"))

def _utf8_key(value): return value.encode("utf-8")

def _scan_restricted(text):
    try:
        for tok in yaml.scan(text, Loader=yaml.BaseLoader):
            if isinstance(tok, (yaml.tokens.AliasToken, yaml.tokens.AnchorToken, yaml.tokens.TagToken)):
                raise CompileError("E_YAML_PROFILE", "aliases, anchors and tags are prohibited")
    except yaml.YAMLError as e:
        raise CompileError("E_YAML_PROFILE", str(e)) from e

def _scalar(node):
    s = node.value
    if "\0" in s: raise CompileError("E_YAML_PROFILE", "NUL prohibited")
    if node.style is not None:
        if len(s.encode("utf-8")) > MAX_STRING: raise CompileError("E_LIMIT", "string exceeds 65535 bytes")
        return s
    low = s.lower()
    if s in ("true", "false"): return s == "true"
    if low in {"", "yes", "no", "on", "off", "null", "~", "true", "false"}:
        raise CompileError("E_YAML_PROFILE", f"reserved plain scalar {s!r}")
    if PLAIN_INT.fullmatch(s):
        n = int(s)
        if not -(1 << 63) <= n < (1 << 63): raise CompileError("E_LIMIT", "integer outside int64")
        return n
    if (s.startswith("+")
            or DATE_PLAIN.fullmatch(s) or FLOAT_PLAIN.fullmatch(s)
            or PREFIXED_OR_ODD_INT.fullmatch(s)):
        raise CompileError("E_YAML_PROFILE", f"invalid plain scalar {s!r}")
    if len(s.encode("utf-8")) > MAX_STRING:
        raise CompileError("E_LIMIT", "string exceeds 65535 bytes")
    return s

def _node(node, depth=1):
    if depth > MAX_DEPTH: raise CompileError("E_LIMIT", "nesting exceeds 16")
    if isinstance(node, yaml.ScalarNode): return _scalar(node)
    if isinstance(node, yaml.SequenceNode):
        if len(node.value) > MAX_COLLECTION: raise CompileError("E_LIMIT", "collection exceeds 65535 items")
        return [_node(x, depth + 1) for x in node.value]
    if isinstance(node, yaml.MappingNode):
        if len(node.value) > MAX_COLLECTION: raise CompileError("E_LIMIT", "collection exceeds 65535 pairs")
        out = {}
        for key, val in node.value:
            if not isinstance(key, yaml.ScalarNode): raise CompileError("E_YAML_PROFILE", "mapping key is not string")
            k = _scalar(key)
            if not isinstance(k, str) or k == "<<": raise CompileError("E_YAML_PROFILE", "invalid mapping key")
            if len(k.encode("utf-8")) > MAX_STRING: raise CompileError("E_LIMIT", "key exceeds 65535 bytes")
            if k in out: raise CompileError("E_YAML_PROFILE", f"duplicate key {k!r}")
            out[k] = _node(val, depth + 1)
        return out
    raise CompileError("E_YAML_PROFILE", "unknown YAML node")

def parse_yaml(raw):
    if b"\0" in raw: raise CompileError("E_YAML_PROFILE", "NUL prohibited")
    try: text = raw.decode("utf-8")
    except UnicodeDecodeError as e: raise CompileError("E_YAML_PROFILE", "invalid UTF-8") from e
    if unicodedata.normalize("NFC", text) != text: raise CompileError("E_YAML_PROFILE", "input is not NFC")
    _scan_restricted(text)
    try: docs = list(yaml.compose_all(text, Loader=yaml.BaseLoader))
    except yaml.YAMLError as e: raise CompileError("E_YAML_PROFILE", str(e)) from e
    if len(docs) != 1 or docs[0] is None: raise CompileError("E_YAML_PROFILE", "exactly one document required")
    return _node(docs[0])

def _schemas(root):
    schema_dir = Path(__file__).resolve().parents[2] / "data" / "schemas"
    names = [x[0] for x in KIND_INFO]
    loaded = {n: json.loads((schema_dir / f"{n}.schema.json").read_text("utf-8")) for n in names + ["common"] if (schema_dir / f"{n}.schema.json").exists()}
    resources = [(s["$id"], Resource.from_contents(s)) for s in loaded.values()]
    registry = Registry().with_resources(resources)
    return {n: Draft202012Validator(loaded[n], registry=registry) for n in names}

def _read_sources(root, errors):
    root = Path(root)
    if not root.is_dir(): raise CompileError("E_IO", "source root does not exist")
    records = {x[0]: [] for x in KIND_INFO}
    manifest = root / "manifest.yaml"
    paths = [("manifest", manifest)]
    for kind, directory, _, _ in KIND_INFO[1:]:
        d = root / directory
        if d.exists() and not d.is_dir(): _err(errors, "E_IO", kind, "", "", "category is not directory")
        # P2-4: *.yaml y *.yml son ambos YAML válido; se reconocen los dos en
        # lugar de ignorar *.yml en silencio. El orden se mantiene determinista
        # (sorted por clave UTF-8 del nombre) sobre la unión de ambos patrones.
        if d.is_dir():
            paths.extend((kind, p) for p in sorted(
                (*d.glob("*.yaml"), *d.glob("*.yml")), key=lambda p: _utf8_key(p.name)))
    validators = _schemas(root)
    for kind, path in paths:
        if not path.exists():
            if kind == "manifest": _err(errors, "E_IO", kind, "", "", "manifest.yaml missing")
            continue
        cap = MAX_MAP_FILE if kind == "map" else MAX_FILE
        try:
            info = path.stat()
            if not stat.S_ISREG(info.st_mode):
                _err(errors, "E_IO", kind, "", "", "source is not a regular file"); continue
            if info.st_size > cap:
                _err(errors, "E_LIMIT", kind, "", "", "file exceeds cap"); continue
            with path.open("rb") as source:
                raw = source.read(cap + 1)
        except OSError as e: _err(errors, "E_IO", kind, "", "", str(e)); continue
        if len(raw) > cap: _err(errors, "E_LIMIT", kind, "", "", "file exceeds cap"); continue
        try: doc = parse_yaml(raw)
        except CompileError as e: _err(errors, e.code, kind, "", "", e.message); continue
        ident = doc.get("id", doc.get("package_id", "")) if isinstance(doc, dict) else ""
        if isinstance(doc, dict) and "schema_version" in doc and doc["schema_version"] != (2 if kind == "unit" else 1):
            _err(errors, "E_SCHEMA_VERSION", kind, str(ident), "/schema_version", "wrong schema version")
        schema_errors = list(validators[kind].iter_errors(doc))
        for e in sorted(schema_errors, key=lambda e: ("/".join(str(x) for x in e.path), str(e.validator))):
            ptr = "/" + "/".join(str(x) for x in e.path)
            _err(errors, "E_SCHEMA", kind, str(ident), ptr, e.message)
        if not schema_errors: records[kind].append(doc)
    return records

def _date(s):
    try: _dt.date.fromisoformat(s); return True
    except (TypeError, ValueError): return False

def _semantic(records, profile, errors, source_root=None):
    if len(records["manifest"]) != 1:
        _err(errors, "E_SCHEMA", "manifest", "", "", "exactly one manifest required")
        return
    manifest = records["manifest"][0]
    owned = set(manifest["owned_namespaces"])
    capabilities = set(manifest["declared_capabilities"])
    behaviors = set(manifest["declared_behaviors"])
    variant_groups = set(manifest["declared_variant_groups"])
    materials = {}
    for pos, material in enumerate(manifest["declared_materials"]):
        mid = material["id"]
        if mid in materials:
            _err(errors, "E_DUPLICATE_ID", "manifest", manifest["package_id"],
                 f"/declared_materials/{pos}/id", "duplicate material id")
        else:
            materials[mid] = material

    for family, values in (("capability", capabilities), ("behavior", behaviors),
                           ("variant group", variant_groups), ("material", set(materials))):
        for value in values:
            if value.split(":", 1)[0] not in owned:
                _err(errors, "E_NAMESPACE", "manifest", manifest["package_id"], "/", f"{family} namespace not owned: {value}")

    all_records, ids = [], {}
    for kind, _, _, _ in KIND_INFO:
        for record in records[kind]:
            ident = record.get("id", record.get("package_id", ""))
            all_records.append((kind, ident, record))
            if kind != "manifest":
                if ident in ids:
                    _err(errors, "E_DUPLICATE_ID", kind, ident, "/id", "duplicate global id")
                else:
                    ids[ident] = (kind, record)
                if ident.split(":", 1)[0] not in owned:
                    _err(errors, "E_NAMESPACE", kind, ident, "/id", "namespace not owned")

            provenance = record["provenance"]
            if profile == "release" and provenance["status"] != "promoted":
                _err(errors, "E_PROVENANCE", kind, ident, "/provenance/status", "release requires promoted")
            if not _date(provenance["generated_on"]):
                _err(errors, "E_PROVENANCE", kind, ident, "/provenance/generated_on", "invalid calendar date")
            claims = provenance["historical_claims"]
            for pos, source in enumerate(claims["sources"]):
                if "accessed_on" in source and not _date(source["accessed_on"]):
                    _err(errors, "E_PROVENANCE", kind, ident,
                         f"/provenance/historical_claims/sources/{pos}/accessed_on", "invalid calendar date")
            for pos, report in enumerate(claims["verification_reports"]):
                pure = PurePosixPath(report)
                if (not report or report.startswith("/") or "\\" in report
                        or any(part in ("", ".", "..") for part in pure.parts)
                        or re.match(r"^[A-Za-z]:", report)):
                    _err(errors, "E_PROVENANCE", kind, ident,
                         f"/provenance/historical_claims/verification_reports/{pos}", "report path is not portable relative POSIX")
                elif source_root is not None and not (source_root / pure).is_file():
                    # P1-1: procedencia auto-contenida — la ruta debe existir dentro del
                    # repo, relativa al source_root, o un clon limpio no puede resolver
                    # la evidencia y la cadena "promoted" (ADR-014) queda rota.
                    _err(errors, "E_PROVENANCE", kind, ident,
                         f"/provenance/historical_claims/verification_reports/{pos}",
                         f"report path does not exist under source_root: {report}")

    def ref(kind, owner, target, expected, pointer):
        got = ids.get(target)
        if got is None or got[0] not in expected:
            _err(errors, "E_REFERENCE", kind, owner, pointer,
                 f"{target} is missing or not {'/'.join(expected)}")
            return None
        return got[1]

    for material_id in materials:
        if material_id in ids:
            _err(errors, "E_DUPLICATE_ID", "manifest", manifest["package_id"],
                 "/declared_materials", f"material id collides with record: {material_id}")

    def declared(kind, owner, value, pool, pointer, label):
        if value not in pool:
            _err(errors, "E_REFERENCE", kind, owner, pointer, f"undeclared {label}: {value}")

    def material_ref(kind, owner, item, pointer):
        mid = item["material_id"]
        if mid not in materials:
            _err(errors, "E_REFERENCE", kind, owner, pointer, f"undeclared material: {mid}")
            return None
        return materials[mid]

    civs = {ident: record for ident, (kind, record) in ids.items() if kind == "civ"}
    periods = {}
    for civ_id, civ in civs.items():
        window = civ["historical_window"]
        if window["start_year"] > window["end_year"]:
            _err(errors, "E_EPOCH_WINDOW", "civ", civ_id, "/historical_window", "inverted historical window")
        epoch = civ["epoch_window"]
        if epoch[0] > epoch[1]:
            _err(errors, "E_EPOCH_WINDOW", "civ", civ_id, "/epoch_window", "inverted epoch window")
        previous_end = None
        by_id = {}
        for pos, period in enumerate(civ["playable_periods"]):
            pid = period["id"]
            if pid in by_id:
                _err(errors, "E_DUPLICATE_ID", "civ", civ_id, f"/playable_periods/{pos}/id", "duplicate period id")
            by_id[pid] = period
            if (period["start_year"] > period["end_year"]
                    or period["start_year"] < window["start_year"]
                    or period["end_year"] > window["end_year"]
                    or previous_end is not None and period["start_year"] <= previous_end):
                _err(errors, "E_EPOCH_WINDOW", "civ", civ_id, f"/playable_periods/{pos}", "period outside window, overlapping, or unordered")
            previous_end = period["end_year"]
        periods[civ_id] = by_id
        declared("civ", civ_id, civ["gameplay_verb"]["id"], capabilities, "/gameplay_verb/id", "capability")
        for ipos, institution in enumerate(civ["institutions"]):
            for cpos, capability in enumerate(institution["required_capability_ids"]):
                declared("civ", civ_id, capability, capabilities,
                         f"/institutions/{ipos}/required_capability_ids/{cpos}", "capability")

    def availability(kind, ident, record, civ_ids, epoch_window=None, epoch=None):
        union_periods = set()
        for civ_id in civ_ids:
            civ = civs.get(civ_id)
            if civ is None:
                ref(kind, ident, civ_id, ("civ",), "/civ_id" if kind != "tech" else "/available_to")
                continue
            union_periods.update(periods.get(civ_id, {}))
            if epoch_window is not None:
                if (epoch_window[0] > epoch_window[1] or epoch_window[1] < civ["epoch_window"][0]
                        or epoch_window[0] > civ["epoch_window"][1]):
                    _err(errors, "E_EPOCH_WINDOW", kind, ident, "/epoch_window", f"does not overlap civ {civ_id}")
            if epoch is not None and not civ["epoch_window"][0] <= epoch <= civ["epoch_window"][1]:
                _err(errors, "E_EPOCH_WINDOW", kind, ident, "/epoch", f"outside civ {civ_id}")
            if not any(pid in periods.get(civ_id, {}) for pid in record["playable_period_ids"]):
                _err(errors, "E_REFERENCE", kind, ident, "/playable_period_ids", f"no playable period for civ {civ_id}")
        for pos, pid in enumerate(record["playable_period_ids"]):
            if pid not in union_periods:
                _err(errors, "E_REFERENCE", kind, ident, f"/playable_period_ids/{pos}", f"period does not belong to an available civ: {pid}")

    recipe_edges = {mid: set() for mid in materials}
    recipe_ids = set()
    for kind, ident, record in all_records:
        if kind == "unit":
            civ_id = record["civ_id"]
            availability(kind, ident, record, [civ_id], epoch_window=record["epoch_window"])
            if civ_id in civs and ident not in civs[civ_id]["unit_ids"]:
                _err(errors, "E_REFERENCE", kind, ident, "/civ_id", "unit missing from civ.unit_ids")
            if ident.split(":", 1)[0] != "base" and civ_id in civs and ident.split(":", 1)[0] != civ_id.split(":", 1)[0]:
                _err(errors, "E_NAMESPACE", kind, ident, "/civ_id", "unit and civ namespaces differ")
            if not any(value > 0 for value in record["resource_costs"].values()):
                _err(errors, "E_UNIT_CLASS", kind, ident, "/resource_costs", "unit requires a positive resource cost")
            tags = set(record["tags"])
            if {"suppression_resist_low", "suppression_resist_high"} <= tags:
                _err(errors, "E_UNIT_CLASS", kind, ident, "/tags", "suppression tags are exclusive")
            citizen = record["class"] == "citizen"
            if citizen and ("drop_off_carrier" not in tags or record["stats"]["attack"] != 0 or record["stats"]["range_millitiles"] != 0):
                _err(errors, "E_UNIT_CLASS", kind, ident, "/stats", "invalid citizen stats/tags")
            if not citizen and record["stats"]["attack"] <= 0:
                _err(errors, "E_UNIT_CLASS", kind, ident, "/stats/attack", "combat unit requires attack")
            for pos, item in enumerate(record.get("material_costs", [])):
                material_ref(kind, ident, item, f"/material_costs/{pos}")

        elif kind == "building":
            civ_id = record["civ_id"]
            availability(kind, ident, record, [civ_id], epoch_window=record["epoch_window"])
            if civ_id in civs and ident not in civs[civ_id]["building_ids"]:
                _err(errors, "E_REFERENCE", kind, ident, "/civ_id", "building missing from civ.building_ids")
            for field, expected in (("trains", ("unit",)), ("researches", ("tech",))):
                for pos, target in enumerate(record[field]): ref(kind, ident, target, expected, f"/{field}/{pos}")
            for field in ("required_capabilities", "grants_capabilities"):
                for pos, value in enumerate(record[field]): declared(kind, ident, value, capabilities, f"/{field}/{pos}", "capability")
            for pos, item in enumerate(record.get("material_costs", [])):
                material_ref(kind, ident, item, f"/material_costs/{pos}")
            has_cost = any(value > 0 for value in record["resource_costs"].values()) or bool(record.get("material_costs"))
            if record["constructible"] != bool(record["build_time_ticks"] > 0) or record["constructible"] != has_cost:
                _err(errors, "E_REFERENCE", kind, ident, "/constructible", "constructibility/cost/build time mismatch")
            for rpos, recipe in enumerate(record["recipes"]):
                rid = recipe["id"]
                if rid in recipe_ids: _err(errors, "E_DUPLICATE_ID", kind, ident, f"/recipes/{rpos}/id", "duplicate recipe id")
                recipe_ids.add(rid)
                output = materials.get(recipe["output_material_id"])
                if output is None or output["kind"] != "intermediate":
                    _err(errors, "E_REFERENCE", kind, ident, f"/recipes/{rpos}/output_material_id", "recipe output must be declared intermediate")
                for ipos, item in enumerate(recipe["input_material_costs"]):
                    source = material_ref(kind, ident, item, f"/recipes/{rpos}/input_material_costs/{ipos}")
                    if item["material_id"] == recipe["output_material_id"]:
                        _err(errors, "E_CYCLE", kind, ident, f"/recipes/{rpos}", "recipe consumes its output")
                    if source is not None and output is not None:
                        recipe_edges[item["material_id"]].add(recipe["output_material_id"])

        elif kind == "tech":
            availability(kind, ident, record, record["available_to"], epoch=record["epoch"])
            for civ_id in record["available_to"]:
                if civ_id in civs and ident not in civs[civ_id]["tech_ids"]:
                    _err(errors, "E_REFERENCE", kind, ident, "/available_to", f"tech missing from {civ_id}.tech_ids")
            for field, expected in (("prerequisites", ("tech",)), ("required_buildings", ("building",)),
                                    ("mutually_exclusive_with", ("tech",))):
                for pos, target in enumerate(record[field]): ref(kind, ident, target, expected, f"/{field}/{pos}")
            if ident in record["prerequisites"] or ident in record["mutually_exclusive_with"]:
                _err(errors, "E_REFERENCE", kind, ident, "/", "self prerequisite/exclusion")
            for pos, capability in enumerate(record["required_capabilities"]):
                declared(kind, ident, capability, capabilities, f"/required_capabilities/{pos}", "capability")
            for field, expected in (("units", ("unit",)), ("buildings", ("building",))):
                for pos, target in enumerate(record["grants"][field]): ref(kind, ident, target, expected, f"/grants/{field}/{pos}")
            for pos, capability in enumerate(record["grants"]["capabilities"]):
                declared(kind, ident, capability, capabilities, f"/grants/capabilities/{pos}", "capability")
            if "regional_variant_group" in record:
                declared(kind, ident, record["regional_variant_group"], variant_groups, "/regional_variant_group", "variant group")
            strategic = set()
            for pos, item in enumerate(record.get("material_costs", [])):
                material = material_ref(kind, ident, item, f"/material_costs/{pos}")
                if material is not None:
                    if material["strategic"]: strategic.add(item["material_id"])
            if len(strategic) > 2:
                _err(errors, "E_REFERENCE", kind, ident, "/material_costs", "more than two strategic materials")
            if record["branch"] != "institution" and not (any(v > 0 for v in record["resource_costs"].values()) or strategic):
                _err(errors, "E_REFERENCE", kind, ident, "/resource_costs", "non-institution tech requires a cost")

        elif kind == "civ":
            for field, expected, reverse in (("unit_ids", ("unit",), "civ_id"),
                                             ("building_ids", ("building",), "civ_id")):
                for pos, target in enumerate(record[field]):
                    other = ref(kind, ident, target, expected, f"/{field}/{pos}")
                    if other is not None and other[reverse] != ident:
                        _err(errors, "E_REFERENCE", kind, ident, f"/{field}/{pos}", "reverse civ reference mismatch")
            for pos, target in enumerate(record["tech_ids"]):
                other = ref(kind, ident, target, ("tech",), f"/tech_ids/{pos}")
                if other is not None and ident not in other["available_to"]:
                    _err(errors, "E_REFERENCE", kind, ident, f"/tech_ids/{pos}", "reverse tech reference mismatch")

        elif kind == "map":
            width, height = record["width_tiles"], record["height_tiles"]
            area = width * height
            terrain, costs = record["terrain_rle"], record["cost_rle"]
            terrain_total = sum(item["run"] for item in terrain)
            cost_total = sum(item["run"] for item in costs)
            valid_rle = terrain_total == area and cost_total == area
            if not valid_rle:
                _err(errors, "E_MAP_RLE", kind, ident, "/", "RLE does not cover width*height")
            if (any(a["terrain"] == b["terrain"] for a, b in zip(terrain, terrain[1:]))
                    or any(a["cost"] == b["cost"] for a, b in zip(costs, costs[1:]))):
                _err(errors, "E_MAP_RLE", kind, ident, "/", "adjacent equal RLE runs")

            def rle_value(runs, key, index):
                cursor = 0
                for item in runs:
                    cursor += item["run"]
                    if index < cursor: return item[key]
                return None

            start_slots, start_cells, spawn_cells = set(), set(), set()
            for pos, start in enumerate(record["starting_positions"]):
                x, y = start["x_millitiles"], start["y_millitiles"]
                cell = (x // 1000, y // 1000)
                bad = x >= width * 1000 or y >= height * 1000 or start["slot"] in start_slots or cell in start_cells
                if not bad and valid_rle:
                    index = cell[1] * width + cell[0]
                    bad = rle_value(costs, "cost", index) == 255 or rle_value(terrain, "terrain", index) == "water"
                if bad: _err(errors, "E_MAP_RLE", kind, ident, f"/starting_positions/{pos}", "invalid, duplicate, or impassable start")
                start_slots.add(start["slot"]); start_cells.add(cell)
            for pos, spawn in enumerate(record["resource_spawns"]):
                x, y = spawn["x_millitiles"], spawn["y_millitiles"]
                cell = (x // 1000, y // 1000)
                bad = x >= width * 1000 or y >= height * 1000 or cell in spawn_cells or cell in start_cells
                if not bad and valid_rle:
                    index = cell[1] * width + cell[0]
                    bad = rle_value(costs, "cost", index) == 255 or rle_value(terrain, "terrain", index) == "water"
                if bad: _err(errors, "E_MAP_RLE", kind, ident, f"/resource_spawns/{pos}", "invalid, duplicate, or impassable spawn")
                spawn_cells.add(cell)
                if spawn["kind"] == "resource":
                    if spawn["id"] not in RESOURCES: _err(errors, "E_REFERENCE", kind, ident, f"/resource_spawns/{pos}/id", "unknown resource")
                else:
                    material = materials.get(spawn["id"])
                    if material is None or material["kind"] != "deposit":
                        _err(errors, "E_REFERENCE", kind, ident, f"/resource_spawns/{pos}/id", "map material must be declared deposit")

        elif kind == "ai-profile":
            seen_considerations = set()
            for pos, curve in enumerate(record["utility_curves"]):
                if curve["consideration"] in seen_considerations:
                    _err(errors, "E_AI_FAIRPLAY", kind, ident, f"/utility_curves/{pos}/consideration", "duplicate consideration")
                seen_considerations.add(curve["consideration"])
                if any(a["input_bp"] >= b["input_bp"] for a, b in zip(curve["points"], curve["points"][1:])):
                    _err(errors, "E_AI_FAIRPLAY", kind, ident, f"/utility_curves/{pos}/points", "input_bp is not strictly increasing")
            seen_pairs = set()
            for pos, behavior in enumerate(record["tactical_behaviors"]):
                pair = (behavior["group_type"], behavior["behavior_id"])
                if pair in seen_pairs:
                    _err(errors, "E_AI_FAIRPLAY", kind, ident, f"/tactical_behaviors/{pos}", "duplicate group/behavior pair")
                seen_pairs.add(pair)
                declared(kind, ident, behavior["behavior_id"], behaviors,
                         f"/tactical_behaviors/{pos}/behavior_id", "behavior")

    techs = {ident: record for ident, (kind, record) in ids.items() if kind == "tech"}
    for ident, tech in techs.items():
        for target in tech["mutually_exclusive_with"]:
            if target in techs and ident not in techs[target]["mutually_exclusive_with"]:
                _err(errors, "E_REFERENCE", "tech", ident, "/mutually_exclusive_with", f"asymmetric exclusion with {target}")

    def graph_cycle(graph):
        visiting, done = set(), set()
        def visit(node):
            if node in visiting: return True
            if node in done: return False
            visiting.add(node)
            hit = any(target in graph and visit(target) for target in graph.get(node, ()))
            visiting.remove(node); done.add(node)
            return hit
        return next((node for node in sorted(graph, key=_utf8_key) if visit(node)), None)

    cycle = graph_cycle({ident: set(tech["prerequisites"]) for ident, tech in techs.items()})
    if cycle is not None: _err(errors, "E_CYCLE", "tech", cycle, "/prerequisites", "prerequisite cycle")
    cycle = graph_cycle(recipe_edges)
    if cycle is not None: _err(errors, "E_CYCLE", "building", cycle, "/recipes", "material production cycle")

def _normalize(value, key=None):
    if isinstance(value, dict): return {k: _normalize(value[k], k) for k in sorted(value, key=_utf8_key)}
    if isinstance(value, list):
        # JSON Schema sets are known by their key names; sorting scalar/object CVE encodings is canonical.
        sets = {
            "owned_namespaces", "declared_capabilities", "declared_behaviors",
            "declared_variant_groups", "declared_materials", "dependencies", "load_after",
            "tags", "playable_period_ids", "dropoff_resources", "trains", "researches",
            "required_capabilities", "grants_capabilities", "required_capability_ids",
            "available_to", "prerequisites", "required_buildings", "mutually_exclusive_with",
            "unit_ids", "building_ids", "tech_ids", "art_rule_keys", "review_roles",
            "material_costs", "input_material_costs", "recipes", "verification_reports",
            "sources", "reviewed_by", "units", "buildings", "capabilities",
        }
        vals = [_normalize(v) for v in value]
        if key == "sources":
            return sorted(vals, key=lambda source: tuple(
                _utf8_key(source.get(field, ""))
                for field in ("citation", "locator", "url", "accessed_on")))
        if key in sets: return sorted(vals, key=cve_encode)
        if key == "starting_positions": return sorted(vals, key=lambda x:x["slot"])
        if key == "resource_spawns": return sorted(vals, key=lambda x:(x["y_millitiles"],x["x_millitiles"],0 if x["kind"]=="resource" else 1,_utf8_key(x["id"]),x["amount"]))
        return vals
    return value

def cve_encode(v):
    if v is False: return b"\x01"
    if v is True: return b"\x02"
    if isinstance(v, int) and not isinstance(v, bool): return b"\x10" + struct.pack("<q", v)
    if isinstance(v, str):
        b=v.encode("utf-8")
        if "\0" in v or len(b)>MAX_STRING or unicodedata.normalize("NFC",v)!=v: raise CompileError("E_BLOB_PARSE","invalid string")
        return b"\x20"+struct.pack("<I",len(b))+b
    if isinstance(v, list):
        if len(v)>MAX_COLLECTION: raise CompileError("E_BLOB_PARSE","collection cap")
        return b"\x30"+struct.pack("<I",len(v))+b"".join(cve_encode(x) for x in v)
    if isinstance(v, dict):
        if len(v)>MAX_COLLECTION: raise CompileError("E_BLOB_PARSE","collection cap")
        out=[b"\x40",struct.pack("<I",len(v))]; last=None
        for k in sorted(v,key=_utf8_key):
            b=k.encode("utf-8")
            if "\0" in k or len(b)>MAX_STRING or last is not None and b<=last: raise CompileError("E_BLOB_PARSE","key order")
            out += [struct.pack("<I",len(b)),b,cve_encode(v[k])]; last=b
        return b"".join(out)
    raise CompileError("E_BLOB_PARSE","unsupported CVE type")

class Reader:
    def __init__(self,b): self.b,self.i,self.nodes=b,0,0
    def take(self,n):
        if n<0 or self.i+n>len(self.b): raise CompileError("E_BLOB_PARSE","truncated")
        x=self.b[self.i:self.i+n]; self.i+=n; return x
    def u16(self): return struct.unpack("<H",self.take(2))[0]
    def u32(self): return struct.unpack("<I",self.take(4))[0]
    def u64(self): return struct.unpack("<Q",self.take(8))[0]
    def value(self, depth=1):
        if depth>MAX_DEPTH: raise CompileError("E_BLOB_PARSE","CVE depth")
        self.nodes+=1
        if self.nodes>262144: raise CompileError("E_BLOB_PARSE","CVE nodes")
        tag=self.take(1)[0]
        if tag==1:return False
        if tag==2:return True
        if tag==16:return struct.unpack("<q",self.take(8))[0]
        if tag==32:
            n=self.u32()
            if n>MAX_STRING: raise CompileError("E_BLOB_PARSE","string cap")
            try:s=self.take(n).decode("utf-8")
            except UnicodeDecodeError as e:raise CompileError("E_BLOB_PARSE","utf8") from e
            if unicodedata.normalize("NFC",s)!=s:raise CompileError("E_BLOB_PARSE","NFC")
            if "\0" in s: raise CompileError("E_BLOB_PARSE", "NUL")
            return s
        if tag in (48,64):
            n=self.u32()
            if n>MAX_COLLECTION:raise CompileError("E_BLOB_PARSE","collection cap")
            if tag==48:return [self.value(depth+1) for _ in range(n)]
            out={};last=b""
            for _ in range(n):
                z=self.u32()
                if z>MAX_STRING:raise CompileError("E_BLOB_PARSE","key cap")
                kraw=self.take(z)
                if kraw<=last:raise CompileError("E_BLOB_PARSE","key order")
                try:k=kraw.decode("utf-8")
                except UnicodeDecodeError as e:raise CompileError("E_BLOB_PARSE","key utf8") from e
                if unicodedata.normalize("NFC",k)!=k:raise CompileError("E_BLOB_PARSE","key NFC")
                if "\0" in k: raise CompileError("E_BLOB_PARSE", "key NUL")
                out[k]=self.value(depth+1);last=kraw
            return out
        raise CompileError("E_BLOB_PARSE","unknown CVE tag")

def parse_blob(blob):
    if not isinstance(blob, bytes) or len(blob) > MAX_BLOB or len(blob) < 208:
        raise CompileError("E_BLOB_PARSE", "file size")
    reader = Reader(blob)
    if reader.take(8) != b"CHNSDB1\0": raise CompileError("E_BLOB_PARSE", "bad magic")
    major, minor, schema_set = reader.u16(), reader.u16(), reader.u32()
    if (major, minor, schema_set) != (1, 0, 1): raise CompileError("E_BLOB_PARSE", "unsupported version")
    flags, count, entry_size, reserved, file_size = (
        reader.u32(), reader.u32(), reader.u32(), reader.u32(), reader.u64())
    if flags & ~1: raise CompileError("E_BLOB_PARSE", "unknown flags")
    if count != 7 or entry_size != 24 or reserved != 0 or file_size != len(blob):
        raise CompileError("E_BLOB_PARSE", "invalid header fields")
    entries = []
    for kind_name, _, expected_kind, cap in KIND_INFO:
        kind, version, record_count, offset, byte_size = (
            reader.u16(), reader.u16(), reader.u32(), reader.u64(), reader.u64())
        expected_version = 2 if expected_kind == 2 else 1
        if kind != expected_kind or version != expected_version or record_count > cap:
            raise CompileError("E_BLOB_PARSE", f"invalid directory entry for {kind_name}")
        entries.append((kind_name, kind, record_count, offset, byte_size))
    if reader.i != 208: raise CompileError("E_BLOB_PARSE", "directory size")

    validators = _schemas(None)
    cursor = 208
    records = {}
    for kind_name, kind, record_count, offset, byte_size in entries:
        if offset != cursor or byte_size > len(blob) - offset:
            raise CompileError("E_BLOB_PARSE", "section gap, overlap, or overflow")
        if record_count > byte_size // 5:
            raise CompileError("E_BLOB_PARSE", "record count cannot fit section")
        section = Reader(blob[offset:offset + byte_size])
        values, previous_id = [], None
        for _ in range(record_count):
            payload_size = section.u32()
            payload_cap = MAX_MAP_FILE if kind == 6 else MAX_FILE
            if payload_size > payload_cap or payload_size > len(section.b) - section.i:
                raise CompileError("E_BLOB_PARSE", "record payload cap/truncation")
            payload = section.take(payload_size)
            payload_reader = Reader(payload)
            value = payload_reader.value()
            if payload_reader.i != payload_size or not isinstance(value, dict):
                raise CompileError("E_BLOB_PARSE", "record payload trailing/non-object")
            validation_errors = list(validators[kind_name].iter_errors(value))
            if validation_errors:
                raise CompileError("E_BLOB_PARSE", f"record schema invalid: {validation_errors[0].message}")
            if cve_encode(_normalize(value)) != payload:
                raise CompileError("E_BLOB_PARSE", "record is not canonically encoded/normalized")
            id_key = "package_id" if kind == 1 else "id"
            record_id = value[id_key]
            encoded_id = _utf8_key(record_id)
            if previous_id is not None and encoded_id <= previous_id:
                raise CompileError("E_BLOB_PARSE", "records not strictly ordered")
            previous_id = encoded_id
            values.append(value)
        if section.i != byte_size:
            raise CompileError("E_BLOB_PARSE", "section trailing bytes")
        records[kind] = values
        cursor = offset + byte_size
    if cursor != len(blob) or len(records[1]) != 1:
        raise CompileError("E_BLOB_PARSE", "file trailing bytes or manifest count")
    semantic_records = {kind_name: records[number]
                        for kind_name, _, number, _ in KIND_INFO}
    semantic_errors = []
    _semantic(semantic_records, "dev" if flags & 1 else "release", semantic_errors)
    if semantic_errors:
        raise CompileError("E_BLOB_PARSE", "semantic validation failed: " + semantic_errors[0][4])
    return flags, records

def compile_blob(records, profile):
    flags = 1 if profile == "dev" else 0
    sections=[]
    expected_ids = {}
    for kind,_,number,_ in KIND_INFO:
        rs=records[kind]
        key="package_id" if kind=="manifest" else "id"
        encoded = []
        ordered = sorted(rs,key=lambda x:_utf8_key(x[key]))
        expected_ids[number] = [x[key] for x in ordered]
        for record in ordered:
            item = cve_encode(_normalize(record))
            cap = MAX_MAP_FILE if kind == "map" else MAX_FILE
            if len(item) > cap: raise CompileError("E_LIMIT", f"{kind} record payload exceeds cap")
            encoded.append(struct.pack("<I", len(item)) + item)
        payload=b"".join(encoded)
        sections.append((number,2 if number==2 else 1,len(rs),payload))
    offset=208; directory=[]
    for k,v,n,p in sections: directory.append((k,v,n,offset,len(p))); offset+=len(p)
    if offset>MAX_BLOB:raise CompileError("E_LIMIT","blob cap")
    out=[b"CHNSDB1\0",struct.pack("<HHIIIIIQ",1,0,1,flags,7,24,0,offset)]
    out += [struct.pack("<HHIQQ",*d) for d in directory]; out += [x[3] for x in sections]
    blob=b"".join(out)
    _, parsed = parse_blob(blob)
    for number, expected in expected_ids.items():
        key = "package_id" if number == 1 else "id"
        if [record[key] for record in parsed[number]] != expected:
            raise CompileError("E_BLOB_PARSE", "self-parse semantic index mismatch")
    return blob

def _atomic(path, data):
    path=Path(path); path.parent.mkdir(parents=True,exist_ok=True)
    fd,tmp=tempfile.mkstemp(prefix=".chunsa-",dir=path.parent)
    try:
        with os.fdopen(fd,"wb") as f:f.write(data);f.flush();os.fsync(f.fileno())
        os.replace(tmp,path)
    except BaseException:
        try:os.unlink(tmp)
        except OSError:pass
        raise

def validate(root, profile):
    errors=[]; records=_read_sources(root,errors)
    # Fases dependientes no corren sobre un índice incompleto: un fallo de
    # parse/schema excluiría records y produciría referencias secundarias falsas.
    if not errors:
        _semantic(records,profile,errors,source_root=Path(root))
    if errors:
        for x in sorted(errors,key=lambda x:x[:4]): print(x[4],file=sys.stderr)
        if any(item[3] == "E_IO" for item in errors): raise ValidationIoError
        return None
    return records

def main(argv=None):
    p=argparse.ArgumentParser(prog="chunsa_data_compiler")
    sub=p.add_subparsers(dest="cmd",required=True)
    for name in ("validate","compile"):
        q=sub.add_parser(name);q.add_argument("source_root");q.add_argument("--profile",choices=("dev","release"),default="release")
        if name=="compile":q.add_argument("--out",required=True);q.add_argument("--hash-out");q.add_argument("--print-hash",action="store_true")
    q=sub.add_parser("inspect");q.add_argument("blob");q.add_argument("--json",action="store_true")
    try: a=p.parse_args(argv)
    except SystemExit:return 2
    try:
        if a.cmd=="inspect":
            path = Path(a.blob)
            info = path.stat()
            if not stat.S_ISREG(info.st_mode) or info.st_size > MAX_BLOB:
                raise CompileError("E_BLOB_PARSE", "input is not a regular file within cap")
            blob=path.read_bytes()
            if len(blob) > MAX_BLOB: raise CompileError("E_BLOB_PARSE", "file changed beyond cap")
            flags,recs=parse_blob(blob); h=hashlib.sha256(b"CHUNSA_CONTENT_V1\0"+blob).hexdigest()
            counts={KIND_INFO[k-1][0]:len(v) for k,v in recs.items()}
            obj={"flags":flags,"format":"1.0","content_hash":h,"records":recs,"counts":counts}
            print(json.dumps(obj,sort_keys=True,separators=(",",":")) if a.json else f"CHDB 1.0 flags={flags} content_hash=sha256-v1:{h}\n"+" ".join(f"{k}={v}" for k,v in counts.items()))
            return 0
        records=validate(a.source_root,a.profile)
        if records is None:return 1
        if a.cmd=="validate":return 0
        blob=compile_blob(records,a.profile); h=hashlib.sha256(b"CHUNSA_CONTENT_V1\0"+blob).hexdigest()
        side=("{\"algorithm\":\"sha256\",\"algorithm_version\":1,\"blob_format\":\"1.0\",\"content_hash\":\""+h+"\",\"schema_set_version\":1}\n").encode()
        out_path = Path(a.out).resolve()
        hash_path = Path(a.hash_out or a.out+".content.json").resolve()
        if out_path == hash_path:
            raise CompileError("E_IO", "--out and --hash-out must name different files")
        _atomic(out_path,blob);_atomic(hash_path,side)
        if a.print_hash:
            print(f"content_hash=sha256-v1:{h}"); print("records "+" ".join(f"{k}={len(records[k])}" for k,_,_,_ in KIND_INFO[1:]))
        return 0
    except ValidationIoError: return 2
    except CompileError as e:
        print(f"ERROR {e.code} - -: {e.message}",file=sys.stderr)
        return 2 if e.code == "E_IO" else 1
    except OSError as e: print(f"ERROR E_IO - -: {e}",file=sys.stderr); return 2
    except Exception as e: print(f"ERROR E_INTERNAL - -: {e}",file=sys.stderr); return 3

if __name__=="__main__":sys.exit(main())
