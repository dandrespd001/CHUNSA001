"""Contract and hostile-input tests for the SPEC-002 CHDB compiler."""
from __future__ import annotations

import contextlib
import copy
import importlib.util
import io
import json
from pathlib import Path
import re
import struct
import tempfile
import unittest

import yaml
from jsonschema import Draft202012Validator
from referencing import Registry, Resource

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
spec = importlib.util.spec_from_file_location("compiler", HERE / "chunsa_data_compiler.py")
compiler = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(compiler)
schema_spec = importlib.util.spec_from_file_location("schema_fixtures", HERE / "test_schemas.py")
schema_fixtures = importlib.util.module_from_spec(schema_spec)
assert schema_spec.loader is not None
schema_spec.loader.exec_module(schema_fixtures)

FILES = {
    "manifest": "manifest.yaml", "unit": "units/a.yaml",
    "building": "buildings/a.yaml", "tech": "tech/a.yaml",
    "civ": "civilizations/a.yaml", "map": "maps/a.yaml",
    "ai-profile": "ai_profiles/a.yaml", "resource": "resources/a.yaml",
}


def directory(blob: bytes):
    section_count = struct.unpack_from("<I", blob, 20)[0]
    return [
        struct.unpack_from("<HHIQQ", blob, 40 + 24 * pos)
        for pos in range(section_count)
    ]


def rebuild(blob: bytes, replacements: dict[int, bytes]):
    entries = directory(blob)
    sections = [blob[offset:offset + size] for _, _, _, offset, size in entries]
    sections = [replacements.get(pos, section) for pos, section in enumerate(sections)]
    major, minor, schema_set, flags, section_count, entry_size, reserved = (
        struct.unpack_from("<HHIIIII", blob, 8)
    )
    offset = 40 + section_count * entry_size
    new_entries = []
    for pos, section in enumerate(sections):
        kind, version, count, _, _ = entries[pos]
        new_entries.append((kind, version, count, offset, len(section))); offset += len(section)
    return (b"CHNSDB1\0" + struct.pack(
                "<HHIIIIIQ", major, minor, schema_set, flags,
                section_count, entry_size, reserved, offset)
            + b"".join(struct.pack("<HHIQQ", *entry) for entry in new_entries)
            + b"".join(sections))


def replace_first_payload(blob: bytes, section_index: int, payload: bytes):
    entry = directory(blob)[section_index]
    section = blob[entry[3]:entry[3] + entry[4]]
    old_size = struct.unpack_from("<I", section)[0]
    return rebuild(blob, {section_index: struct.pack("<I", len(payload)) + payload + section[4 + old_size:]})


class CompilerTests(unittest.TestCase):
    def make_root(self, data=None):
        td = tempfile.TemporaryDirectory(); root = Path(td.name)
        data = copy.deepcopy(data or schema_fixtures.fixtures())
        # Regression: digits/underscores in legitimate plain IDs must survive YAML.
        data["manifest"]["declared_behaviors"] = ["base:melee_line_v1"]
        data["manifest"]["declared_variant_groups"] = ["base:epoch4_metalworking"]
        for kind, relative in FILES.items():
            self.write(root, kind, data[kind])
        # P1-1: los fixtures de civ/tech citan verify/report.md como evidencia
        # de procedencia; el compilador ahora exige que exista bajo source_root.
        report = root / "verify" / "report.md"
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text("Fixture de evidencia de procedencia para tests.\n", encoding="utf-8")
        return td, root

    def write(self, root, kind, value, sort_keys=False, suffix="a.yaml"):
        relative = FILES[kind]
        path = root / relative if suffix == "a.yaml" else (root / Path(relative).parent / suffix)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(yaml.safe_dump(value, sort_keys=sort_keys), encoding="utf-8")
        return path

    def read(self, root, kind):
        return yaml.safe_load((root / FILES[kind]).read_text(encoding="utf-8"))

    def invoke(self, argv):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = compiler.main(argv)
        return code, out.getvalue(), err.getvalue()

    def compile_valid(self, root, name="base.chdb", profile="release"):
        out = root / name
        code, stdout, stderr = self.invoke(["compile", str(root), "--out", str(out),
                                            "--profile", profile, "--print-hash"])
        self.assertEqual((code, stderr), (0, ""), stderr)
        return out, stdout

    def assert_validation_error(self, root, code):
        rc, _, stderr = self.invoke(["validate", str(root)])
        self.assertEqual(rc, 1, stderr); self.assertIn(code, stderr)

    def test_yaml_profile_matrix_and_identifier_regressions(self):
        cases = json.loads((ROOT / "tests/data_compile/yaml_profile_cases.json").read_text("utf-8"))
        for scalar in cases["valid_plain"]:
            with self.subTest(valid=scalar):
                self.assertEqual(compiler.parse_yaml(f"x: {scalar}\n".encode())["x"], scalar)
        for scalar in cases["invalid_plain"]:
            with self.subTest(invalid=scalar), self.assertRaises(compiler.CompileError):
                compiler.parse_yaml(f"x: {scalar}\n".encode())
        documents = [
            b"x: 1\nx: 2\n", b"x: &a 1\ny: *a\n", b"x: &a {y: 1}\nz: {<<: *a}\n",
            b"x: !!str value\n", b"x: 1\n---\ny: 2\n",
        ]
        for raw in documents:
            with self.subTest(raw=raw), self.assertRaises(compiler.CompileError): compiler.parse_yaml(raw)
        with self.assertRaises(compiler.CompileError): compiler.parse_yaml("x: e\u0301\n".encode())
        with self.assertRaises(compiler.CompileError): compiler.parse_yaml(("x: '" + "a" * 65536 + "'\n").encode())
        with self.assertRaises(compiler.CompileError): compiler.parse_yaml(("x: " + "[" * 17 + "0" + "]" * 17 + "\n").encode())

    def test_source_file_caps(self):
        td, root = self.make_root()
        with td:
            path = root / FILES["unit"]
            path.write_bytes(path.read_bytes() + b"#" * (compiler.MAX_FILE + 1))
            self.assert_validation_error(root, "E_LIMIT")
        td, root = self.make_root()
        with td:
            path = root / FILES["map"]
            path.write_bytes(path.read_bytes() + b"#" * (compiler.MAX_MAP_FILE + 1))
            self.assert_validation_error(root, "E_LIMIT")

    def test_schema_negative_for_every_kind_and_zero_costs(self):
        mutations = {
            "manifest": lambda d: d.__setitem__("schema_version", 9),
            "unit": lambda d: d["stats"].__setitem__("hp", "bad"),
            "building": lambda d: d["footprint"].__setitem__("width_cells", 0),
            "tech": lambda d: d.__setitem__("epoch", 16),
            "civ": lambda d: d["historical_window"].__setitem__("start_year", 0),
            "map": lambda d: d.__setitem__("width_tiles", 0),
            "ai-profile": lambda d: d["strategic_weights_bp"].__setitem__("risk_tolerance_bp", 10001),
            "resource": lambda d: d.__setitem__("id", "not_namespaced"),
        }
        for kind, mutate in mutations.items():
            td, root = self.make_root()
            with td, self.subTest(kind=kind):
                value = self.read(root, kind); mutate(value); self.write(root, kind, value)
                rc, _, stderr = self.invoke(["validate", str(root)])
                self.assertEqual(rc, 1); self.assertRegex(stderr, "E_SCHEMA(_VERSION)?")
        for kind in ("unit", "building", "tech"):
            td, root = self.make_root()
            with td, self.subTest(zero_cost=kind):
                value = self.read(root, kind)
                value["resource_costs"] = {"chunsa:food": 0}
                self.write(root, kind, value); self.assert_validation_error(root, "E_SCHEMA")

    def test_semantic_continues_and_typed_references(self):
        td, root = self.make_root()
        with td:
            unit = self.read(root, "unit"); unit["stats"]["hp"] = "bad"; self.write(root, "unit", unit)
            building = self.read(root, "building"); building["trains"] = ["rome:roads"]; self.write(root, "building", building)
            rc, _, stderr = self.invoke(["validate", str(root)])
            self.assertEqual(rc, 1); self.assertIn("E_SCHEMA unit", stderr)
            self.assertNotIn("E_REFERENCE building", stderr)
        semantic_mutations = [
            ("duplicate", "building", lambda d: d.__setitem__("id", "rome:legionary"), "E_DUPLICATE_ID"),
            ("namespace", "unit", lambda d: d.__setitem__("id", "mali:legionary"), "E_NAMESPACE"),
            ("capability", "building", lambda d: d["required_capabilities"].append("rome:missing"), "E_REFERENCE"),
            ("variant", "tech", lambda d: d.__setitem__("regional_variant_group", "rome:missing"), "E_REFERENCE"),
            ("period", "unit", lambda d: d.__setitem__("playable_period_ids", ["rome:missing"]), "E_REFERENCE"),
            ("reverse", "civ", lambda d: d.__setitem__("unit_ids", []), "E_REFERENCE"),
            ("cycle", "tech", lambda d: d["prerequisites"].append("rome:roads"), "E_CYCLE"),
        ]
        for label, kind, mutate, expected in semantic_mutations:
            td, root = self.make_root()
            with td, self.subTest(label=label):
                value = self.read(root, kind); mutate(value); self.write(root, kind, value)
                self.assert_validation_error(root, expected)

    def test_recipe_map_ai_and_provenance_semantics(self):
        td, root = self.make_root()
        with td:
            building = self.read(root, "building")
            building["recipes"] = [{
                "id": "rome:smelt",
                "input_resource_costs": {"chunsa:food": 1},
                "output_resource_id": "chunsa:food",
                "output_amount": 1,
                "duration_ticks": 1,
            }]
            self.write(root, "building", building); self.assert_validation_error(root, "E_CYCLE")
        td, root = self.make_root()
        with td:
            game_map = self.read(root, "map"); game_map["cost_rle"] = [{"cost":255,"run":1}]
            self.write(root, "map", game_map); self.assert_validation_error(root, "E_MAP_RLE")
        td, root = self.make_root()
        with td:
            ai = self.read(root, "ai-profile")
            ai["tactical_behaviors"] = [{"group_type":"melee_line","behavior_id":"base:missing",
                "seek_cover":False,"suppression_response":"hold","formation_preference":"line",
                "retreat_hp_threshold_bp":1,"retreat_morale_threshold_bp":1}]
            self.write(root, "ai-profile", ai); self.assert_validation_error(root, "E_REFERENCE")
        for bad in ("2026-02-30", "../verify.md", "C:\\verify.md"):
            td, root = self.make_root()
            with td, self.subTest(provenance=bad):
                unit = self.read(root, "unit")
                if bad.startswith("2026"): unit["provenance"]["generated_on"] = bad
                else:
                    unit["provenance"]["historical_claims"] = {
                        "evidence":"H", "verification_reports":[bad], "sources":[{"citation":"Source"}]}
                self.write(root, "unit", unit); self.assert_validation_error(root, "E_PROVENANCE")

    def test_provenance_report_must_exist_under_source_root(self):
        # P1-1: una ruta bien formada (POSIX relativa) pero que no existe bajo
        # source_root debe rechazarse — la evidencia de procedencia debe estar
        # vendorizada dentro del repo, no solo tener una forma de ruta válida.
        td, root = self.make_root()
        with td:
            unit = self.read(root, "unit")
            unit["provenance"]["historical_claims"] = {
                "evidence": "H", "verification_reports": ["verify/missing_report.md"],
                "sources": [{"citation": "Source"}]}
            self.write(root, "unit", unit)
            self.assert_validation_error(root, "E_PROVENANCE")
        # Control: la misma referencia, apuntando a un archivo que sí existe,
        # debe validar sin error de procedencia.
        td, root = self.make_root()
        with td:
            unit = self.read(root, "unit")
            unit["provenance"]["historical_claims"] = {
                "evidence": "H", "verification_reports": ["verify/report.md"],
                "sources": [{"citation": "Source"}]}
            self.write(root, "unit", unit)
            rc, _, stderr = self.invoke(["validate", str(root)])
            self.assertEqual((rc, stderr), (0, ""), stderr)

    def test_release_dev_policy_and_flags(self):
        td, root = self.make_root()
        with td:
            unit = self.read(root, "unit"); unit["provenance"]["status"] = "verified"; self.write(root, "unit", unit)
            self.assert_validation_error(root, "E_PROVENANCE")
            out, _ = self.compile_valid(root, profile="dev")
            self.assertEqual(struct.unpack_from("<I", out.read_bytes(), 16)[0], 1)

    def test_determinism_reordering_and_semantic_change(self):
        td, root = self.make_root()
        with td:
            first, _ = self.compile_valid(root, "first.chdb")
            baseline = first.read_bytes()
            for kind in FILES:
                value = self.read(root, kind)
                if kind == "manifest":
                    value["owned_namespaces"].reverse(); value["declared_behaviors"].reverse()
                path = self.write(root, kind, value, sort_keys=True, suffix="z.yaml" if kind != "manifest" else "a.yaml")
                if kind != "manifest":
                    (root / FILES[kind]).unlink()
                path.write_text("# reordered\n" + path.read_text("utf-8"), encoding="utf-8")
            second, _ = self.compile_valid(root, "second.chdb")
            self.assertEqual(baseline, second.read_bytes())
            unit = yaml.safe_load((root / "units/z.yaml").read_text("utf-8")); unit["stats"]["hp"] += 1
            self.write(root, "unit", unit, sort_keys=True, suffix="z.yaml")
            third, _ = self.compile_valid(root, "third.chdb")
            self.assertNotEqual(baseline, third.read_bytes())

    def test_header_directory_sidecar_and_cli_outputs(self):
        td, root = self.make_root()
        with td:
            out, stdout = self.compile_valid(root)
            blob = out.read_bytes(); entries = directory(blob)
            self.assertEqual(blob[:8], b"CHNSDB1\0")
            self.assertEqual(
                struct.unpack_from("<HHIIIIIQ", blob, 8),
                (1, 1, 2, 0, 8, 24, 0, len(blob)),
            )
            cursor = 232
            for pos, (kind, version, count, offset, size) in enumerate(entries, 1):
                self.assertEqual((kind, version, offset), (pos, 2 if pos == 2 else 1, cursor)); cursor += size
                self.assertEqual(count, 1)
            self.assertEqual(cursor, len(blob))
            content_hash = stdout.splitlines()[0].split(":", 1)[1]
            expected = (f'{{"algorithm":"sha256","algorithm_version":1,"blob_format":"1.1",'
                        f'"content_hash":"{content_hash}","schema_set_version":2}}\n').encode()
            self.assertEqual(out.with_name(out.name + ".content.json").read_bytes(), expected)
            self.assertRegex(stdout, r"^content_hash=sha256-v1:[0-9a-f]{64}\nrecords unit=1 building=1 tech=1 civ=1 map=1 ai-profile=1 resource=1\n$")
            rc, plain, err = self.invoke(["inspect", str(out)])
            self.assertEqual((rc, err), (0, "")); self.assertIn("CHDB 1.1 flags=0", plain)
            rc, encoded, err = self.invoke(["inspect", str(out), "--json"])
            self.assertEqual((rc, err), (0, "")); self.assertEqual(json.loads(encoded)["counts"]["manifest"], 1)
            self.assertEqual(self.invoke([])[0], 2)
            self.assertEqual(self.invoke(["validate", str(root / "missing")])[0], 2)
            same = root / "same-output"
            rc, _, stderr = self.invoke(["compile", str(root), "--out", str(same),
                                         "--hash-out", str(same)])
            self.assertEqual(rc, 2); self.assertIn("must name different files", stderr)
            self.assertFalse(same.exists())

    def test_sources_use_normative_tuple_order(self):
        sources = [
            {"citation":"Zulu", "locator":"1"},
            {"citation":"Alpha", "url":"https://example.test/a", "accessed_on":"2026-07-22"},
        ]
        normalized = compiler._normalize({"sources": sources})["sources"]
        self.assertEqual([source["citation"] for source in normalized], ["Alpha", "Zulu"])
        self.assertEqual(compiler._normalize({"sources": list(reversed(sources))}),
                         {"sources": normalized})

    def test_repository_release_fixture_matches_versioned_golden(self):
        source_root = ROOT / "data"
        golden = source_root / "compiled/chunsa_base.chdb"
        golden_sidecar = source_root / "compiled/chunsa_base.chdb.content.json"
        self.assertTrue(golden.is_file())
        self.assertTrue(golden_sidecar.is_file())
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory) / "chunsa_base.chdb"
            sidecar = Path(directory) / "chunsa_base.chdb.content.json"
            rc, stdout, stderr = self.invoke([
                "compile", str(source_root), "--out", str(out),
                "--hash-out", str(sidecar), "--profile", "release", "--print-hash",
            ])
            self.assertEqual((rc, stderr), (0, ""), stderr)
            self.assertEqual(out.read_bytes(), golden.read_bytes())
            self.assertEqual(sidecar.read_bytes(), golden_sidecar.read_bytes())
            self.assertIn(
                "records unit=5 building=6 tech=4 civ=2 map=1 "
                "ai-profile=1 resource=30",
                stdout,
            )
            flags, records = compiler.parse_blob(out.read_bytes())
            self.assertEqual(flags, 0)
            self.assertEqual(
                [record["id"] for record in records[2]],
                [
                    "egipto:chariot_warrior", "egipto:work_crew",
                    "rome:ballista_crew", "rome:camp_work_crew", "rome:legionary",
                ],
            )

    def test_blob_header_directory_and_file_corruption(self):
        td, root = self.make_root()
        with td:
            out, _ = self.compile_valid(root); source = out.read_bytes()
            corruptions = []
            for offset, fmt, value in ((0,"B",0),(8,"H",2),(16,"I",2),(68,"I",65536),
                                       (48,"Q",209),(48,"Q",207),(48,"Q",(1<<64)-1)):
                blob=bytearray(source); struct.pack_into("<"+fmt,blob,offset,value); corruptions.append(bytes(blob))
            corruptions += [source+b"x", source[:-1]]
            for blob in corruptions:
                with self.subTest(size=len(blob)), self.assertRaises(compiler.CompileError): compiler.parse_blob(blob)

    def test_cve_hostile_depth_nodes_string_nfc_order_and_trailing(self):
        td, root = self.make_root()
        with td:
            out, _ = self.compile_valid(root); blob = out.read_bytes()
            depth = b"\x30\x01\x00\x00\x00" * 17 + b"\x01"
            inner = b"\x30" + struct.pack("<I", 65535) + b"\x01" * 65535
            nodes = b"\x30" + struct.pack("<I", 5) + inner * 5
            too_long = b"\x20" + struct.pack("<I", 65536)
            nfc_raw = "e\u0301".encode(); non_nfc = b"\x20" + struct.pack("<I",len(nfc_raw)) + nfc_raw
            key_order = (b"\x40\x02\x00\x00\x00" + b"\x01\x00\x00\x00b\x01"
                         + b"\x01\x00\x00\x00a\x01")
            entry = directory(blob)[0]; section = blob[entry[3]:entry[3]+entry[4]]
            size = struct.unpack_from("<I",section)[0]; valid_payload = section[4:4+size]
            nul_raw=b"a\x00b"; nul=b"\x20"+struct.pack("<I",len(nul_raw))+nul_raw
            for payload in (depth, nodes, too_long, non_nfc, nul, key_order, valid_payload+b"\x01"):
                with self.subTest(payload=len(payload)), self.assertRaises(compiler.CompileError):
                    compiler.parse_blob(replace_first_payload(blob, 0, payload))

    def test_blob_schema_record_order_and_set_canonicality(self):
        td, root = self.make_root()
        with td:
            # Add a second valid unit so record order can be corrupted.
            unit = self.read(root,"unit"); second=copy.deepcopy(unit); second["id"]="rome:auxiliary"
            civ=self.read(root,"civ"); civ["unit_ids"].append("rome:auxiliary"); self.write(root,"civ",civ)
            self.write(root,"unit",second,suffix="b.yaml")
            out,_=self.compile_valid(root); blob=out.read_bytes(); entry=directory(blob)[1]
            section=blob[entry[3]:entry[3]+entry[4]]; n1=4+struct.unpack_from("<I",section)[0]
            swapped=section[n1:]+section[:n1]
            with self.assertRaises(compiler.CompileError): compiler.parse_blob(rebuild(blob,{1:swapped}))
            manifest=self.read(root,"manifest"); manifest["owned_namespaces"]=["rome","base"]
            noncanonical=compiler.cve_encode(manifest)
            with self.assertRaises(compiler.CompileError): compiler.parse_blob(replace_first_payload(blob,0,noncanonical))
            bad=copy.deepcopy(manifest); bad["schema_version"]=9
            with self.assertRaises(compiler.CompileError): compiler.parse_blob(replace_first_payload(blob,0,compiler.cve_encode(compiler._normalize(bad))))

    def test_yaml_collection_cap_and_cve_nul(self):
        raw=("x: ["+",".join("a" for _ in range(65536))+"]\n").encode()
        with self.assertRaises(compiler.CompileError) as caught: compiler.parse_yaml(raw)
        self.assertEqual(caught.exception.code,"E_LIMIT")
        with self.assertRaises(compiler.CompileError): compiler.cve_encode("a\0b")
        with self.assertRaises(compiler.CompileError): compiler.cve_encode({"a\0b":1})

    def test_epoch_overlap_and_shared_periods(self):
        td, root=self.make_root()
        with td:
            unit=self.read(root,"unit"); unit["epoch_window"]=[4,5]; self.write(root,"unit",unit)
            self.compile_valid(root)

        td, root=self.make_root()
        with td:
            manifest=self.read(root,"manifest"); manifest["owned_namespaces"].append("egypt"); self.write(root,"manifest",manifest)
            civ=self.read(root,"civ"); second=copy.deepcopy(civ); second.update({
                "id":"egypt:module", "region_key":"egypt:region", "display_name_key":"egypt:module",
                "description_key":"egypt:module_desc", "unit_ids":[], "building_ids":[], "tech_ids":["rome:roads"]})
            second["playable_periods"]=[{"id":"egypt:period","start_year":-100,"end_year":100,"label_key":"egypt:period"}]
            second["gameplay_verb"]={"id":"rome:roads","name_key":"egypt:verb","description_key":"egypt:verb_desc"}
            self.write(root,"civ",second,suffix="b.yaml")
            tech=self.read(root,"tech"); tech["available_to"].append("egypt:module"); tech["playable_period_ids"].append("egypt:period"); self.write(root,"tech",tech)
            self.compile_valid(root)

    def test_parse_blob_runs_full_semantics(self):
        td, root=self.make_root()
        with td:
            out,_=self.compile_valid(root); blob=out.read_bytes(); entry=directory(blob)[1]
            section=blob[entry[3]:entry[3]+entry[4]]; size=struct.unpack_from("<I",section)[0]
            reader=compiler.Reader(section[4:4+size]); unit=reader.value(); unit["civ_id"]="rome:missing"
            payload=compiler.cve_encode(compiler._normalize(unit))
            with self.assertRaises(compiler.CompileError): compiler.parse_blob(replace_first_payload(blob,1,payload))


class ResourceReconciliationTests(unittest.TestCase):
    """SPEC-007 §18.5 / Brief 1.8B acceptance tests.

    These tests intentionally exercise the source schema/compiler boundary:
    authors use record ids, while only the compiler may assign kernel indices.
    """

    # Brief 1.8C: catálogo pasa de 3 a 30 recursos (SPEC-007 §9.2).
    # El invariante crítico sigue siendo food/wood/stone en índices 0/1/2.
    RESOURCE_IDS = (
        "chunsa:aluminum", "chunsa:bauxite", "chunsa:bronze", "chunsa:cement",
        "chunsa:charcoal", "chunsa:clay", "chunsa:coal", "chunsa:coke",
        "chunsa:copper", "chunsa:food", "chunsa:gold", "chunsa:gunpowder",
        "chunsa:iron_ore", "chunsa:lead", "chunsa:limestone", "chunsa:nitre",
        "chunsa:nitrogen_fixed", "chunsa:oil", "chunsa:oil_products",
        "chunsa:quicklime", "chunsa:rare_earths", "chunsa:salt",
        "chunsa:silicon", "chunsa:steel", "chunsa:stone", "chunsa:sulfur",
        "chunsa:tin", "chunsa:uranium", "chunsa:wood", "chunsa:wrought_iron",
    )
    RESOURCE_INDEX = {
        "chunsa:food": 0,
        "chunsa:wood": 1,
        "chunsa:stone": 2,
        "chunsa:aluminum": 3, "chunsa:bauxite": 4, "chunsa:bronze": 5,
        "chunsa:cement": 6, "chunsa:charcoal": 7, "chunsa:clay": 8,
        "chunsa:coal": 9, "chunsa:coke": 10, "chunsa:copper": 11,
        "chunsa:gold": 12, "chunsa:gunpowder": 13, "chunsa:iron_ore": 14,
        "chunsa:lead": 15, "chunsa:limestone": 16, "chunsa:nitre": 17,
        "chunsa:nitrogen_fixed": 18, "chunsa:oil": 19, "chunsa:oil_products": 20,
        "chunsa:quicklime": 21, "chunsa:rare_earths": 22, "chunsa:salt": 23,
        "chunsa:silicon": 24, "chunsa:steel": 25, "chunsa:sulfur": 26,
        "chunsa:tin": 27, "chunsa:uranium": 28, "chunsa:wrought_iron": 29,
    }
    RECORD_ID = re.compile(
        r"^[a-z][a-z0-9_]{0,31}:[a-z][a-z0-9_]{0,63}$"
    )

    def invoke(self, argv):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = compiler.main(argv)
        return code, out.getvalue(), err.getvalue()

    @staticmethod
    def resource_record(resource_id):
        slug = resource_id.split(":", 1)[1]
        return {
            "schema_version": 1,
            "id": resource_id,
            "display_name_key": f"chunsa:resource.{slug}",
            "family": "subsistence",
            "appearance_epoch": 1,
            "nature": "collected",
            "provenance": schema_fixtures.provenance(),
        }

    def make_root(self, resource_filenames=None):
        td = tempfile.TemporaryDirectory()
        root = Path(td.name)
        data = copy.deepcopy(schema_fixtures.fixtures())
        if "chunsa" not in data["manifest"]["owned_namespaces"]:
            data["manifest"]["owned_namespaces"].append("chunsa")
        aliases = {
            "A": "chunsa:food",
            "B": "chunsa:wood",
            "P": "chunsa:stone",
            "Me": "chunsa:stone",
        }
        for kind in ("unit", "building", "tech"):
            costs = data[kind]["resource_costs"]
            data[kind]["resource_costs"] = {
                aliases.get(resource_id, resource_id): amount
                for resource_id, amount in costs.items()
            }
        for kind, relative in FILES.items():
            if kind == "resource":
                continue
            path = root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(
                yaml.safe_dump(data[kind], sort_keys=False),
                encoding="utf-8",
            )
        report = root / "verify" / "report.md"
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text(
            "Fixture de evidencia de procedencia para tests.\n",
            encoding="utf-8",
        )
        filenames = resource_filenames or {
            "chunsa:food": "food.yaml",
            "chunsa:wood": "wood.yaml",
            "chunsa:stone": "stone.yaml",
        }
        for resource_id, filename in filenames.items():
            path = root / "resources" / filename
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(
                yaml.safe_dump(self.resource_record(resource_id), sort_keys=False),
                encoding="utf-8",
            )
        return td, root

    @staticmethod
    def compile_once(invoke, root, name):
        out = root / name
        rc, stdout, stderr = invoke([
            "compile", str(root), "--out", str(out),
            "--profile", "release", "--print-hash",
        ])
        return rc, stdout, stderr, out

    def test_common_schema_has_no_legacy_resource_enum(self):
        common = json.loads(
            (ROOT / "data/schemas/common.schema.json").read_text(encoding="utf-8")
        )
        self.assertNotIn(
            "resource",
            common["$defs"],
            "the eight-letter storable-resource enum must be removed",
        )
        resource_costs = common["$defs"]["resource_costs"]
        self.assertEqual(
            resource_costs.get("propertyNames"),
            {"$ref": "#/$defs/record_id"},
        )

    def test_material_cost_vocabulary_is_absent_from_every_schema(self):
        offenders = []
        for path in sorted((ROOT / "data/schemas").glob("*.schema.json")):
            text = path.read_text(encoding="utf-8")
            for token in ('"material_cost"', '"material_costs"'):
                if token in text:
                    offenders.append(f"{path.name}:{token}")
        self.assertEqual([], offenders)

    def test_namespaced_resource_validates_against_resource_schema(self):
        common_path = ROOT / "data/schemas/common.schema.json"
        resource_path = ROOT / "data/schemas/resource.schema.json"
        self.assertTrue(resource_path.is_file(), "resource.schema.json is required")
        common = json.loads(common_path.read_text(encoding="utf-8"))
        resource_schema = json.loads(resource_path.read_text(encoding="utf-8"))
        registry = Registry().with_resources([
            (common["$id"], Resource.from_contents(common)),
            (resource_schema["$id"], Resource.from_contents(resource_schema)),
        ])
        validator = Draft202012Validator(resource_schema, registry=registry)
        self.assertEqual(
            [],
            list(validator.iter_errors(self.resource_record("chunsa:food"))),
        )

    def test_repository_resource_references_are_namespaced(self):
        legacy = []

        def inspect(node, path):
            if isinstance(node, dict):
                for key, value in node.items():
                    if key in {"resource_costs", "input_resource_costs"}:
                        for resource_id in value:
                            if not self.RECORD_ID.fullmatch(resource_id):
                                legacy.append(f"{path}:{resource_id}")
                    elif key == "dropoff_resources":
                        for resource_id in value:
                            if not self.RECORD_ID.fullmatch(resource_id):
                                legacy.append(f"{path}:{resource_id}")
                    elif key == "resource_spawns":
                        for spawn in value:
                            if (spawn.get("kind") == "resource"
                                    and not self.RECORD_ID.fullmatch(spawn["id"])):
                                legacy.append(f"{path}:{spawn['id']}")
                    elif key == "output_resource_id":
                        if not self.RECORD_ID.fullmatch(value):
                            legacy.append(f"{path}:{value}")
                    else:
                        inspect(value, path)
            elif isinstance(node, list):
                for value in node:
                    inspect(value, path)

        for directory in ("units", "buildings", "tech", "maps"):
            for path in sorted((ROOT / "data" / directory).glob("*.yaml")):
                inspect(yaml.safe_load(path.read_text(encoding="utf-8")), path.name)
        self.assertEqual([], legacy)

    def test_repository_declares_bootstrap_resources_at_indices_zero_one_two(self):
        resource_paths = sorted((ROOT / "data/resources").glob("*.yaml"))
        authored_ids = sorted(
            yaml.safe_load(path.read_text(encoding="utf-8"))["id"]
            for path in resource_paths
        )
        self.assertEqual(sorted(self.RESOURCE_IDS), authored_ids)

        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory) / "repository.chdb"
            rc, _, stderr = self.invoke([
                "compile", str(ROOT / "data"), "--out", str(out),
                "--profile", "release",
            ])
            self.assertEqual((0, ""), (rc, stderr))
            _, records = compiler.parse_blob(out.read_bytes())
            resource_kind = next(
                number for kind, _, number, _ in compiler.KIND_INFO
                if kind == "resource"
            )
            mapping = {
                record["id"]: record["index"]
                for record in records[resource_kind]
            }
            self.assertEqual(self.RESOURCE_INDEX, mapping)

    def test_two_compilations_are_byte_identical(self):
        td, root = self.make_root()
        with td:
            first = self.compile_once(self.invoke, root, "first.chdb")
            second = self.compile_once(self.invoke, root, "second.chdb")
            self.assertEqual(
                (0, "", 0, ""),
                (first[0], first[2], second[0], second[2]),
            )
            self.assertEqual(first[3].read_bytes(), second[3].read_bytes())

    def test_resource_indices_do_not_depend_on_file_order(self):
        first_names = {
            "chunsa:food": "00-food.yaml",
            "chunsa:wood": "01-wood.yaml",
            "chunsa:stone": "02-stone.yaml",
        }
        second_names = {
            "chunsa:stone": "00-stone.yaml",
            "chunsa:food": "01-food.yaml",
            "chunsa:wood": "02-wood.yaml",
        }
        td_a, root_a = self.make_root(first_names)
        td_b, root_b = self.make_root(second_names)
        with td_a, td_b:
            first = self.compile_once(self.invoke, root_a, "ordered.chdb")
            second = self.compile_once(self.invoke, root_b, "permuted.chdb")
            self.assertEqual(
                (0, "", 0, ""),
                (first[0], first[2], second[0], second[2]),
            )
            self.assertEqual(first[3].read_bytes(), second[3].read_bytes())

    def test_unknown_resource_reference_is_coded_load_error(self):
        td, root = self.make_root()
        with td:
            unit_path = root / FILES["unit"]
            unit = yaml.safe_load(unit_path.read_text(encoding="utf-8"))
            unit["resource_costs"] = {"chunsa:missing": 1}
            unit_path.write_text(
                yaml.safe_dump(unit, sort_keys=False),
                encoding="utf-8",
            )
            rc, _, stderr = self.invoke(["validate", str(root)])
            self.assertEqual(1, rc)
            self.assertIn("ERROR E_REFERENCE unit", stderr)
            self.assertIn("chunsa:missing", stderr)

    def test_duplicate_resource_id_is_coded_load_error(self):
        td, root = self.make_root()
        with td:
            duplicate = root / "resources" / "duplicate-food.yaml"
            duplicate.write_text(
                yaml.safe_dump(
                    self.resource_record("chunsa:food"),
                    sort_keys=False,
                ),
                encoding="utf-8",
            )
            rc, _, stderr = self.invoke(["validate", str(root)])
            self.assertEqual(1, rc)
            self.assertIn("ERROR E_DUPLICATE_ID resource chunsa:food", stderr)

    def test_unknown_resource_family_is_coded_load_error(self):
        td, root = self.make_root()
        with td:
            resource_path = root / "resources" / "food.yaml"
            resource = yaml.safe_load(resource_path.read_text(encoding="utf-8"))
            resource["family"] = "invalid_family"
            resource_path.write_text(
                yaml.safe_dump(resource, sort_keys=False),
                encoding="utf-8",
            )
            rc, _, stderr = self.invoke(["validate", str(root)])
            self.assertEqual(1, rc)
            self.assertIn("ERROR E_SCHEMA resource chunsa:food /family", stderr)

    def test_resource_appearance_epoch_out_of_range_is_coded_load_error(self):
        for invalid_epoch in (0, 16):
            with self.subTest(appearance_epoch=invalid_epoch):
                td, root = self.make_root()
                with td:
                    resource_path = root / "resources" / "food.yaml"
                    resource = yaml.safe_load(
                        resource_path.read_text(encoding="utf-8")
                    )
                    resource["appearance_epoch"] = invalid_epoch
                    resource_path.write_text(
                        yaml.safe_dump(resource, sort_keys=False),
                        encoding="utf-8",
                    )
                    rc, _, stderr = self.invoke(["validate", str(root)])
                    self.assertEqual(1, rc)
                    self.assertIn(
                        "ERROR E_SCHEMA resource chunsa:food /appearance_epoch",
                        stderr,
                    )


if __name__ == "__main__": unittest.main()
