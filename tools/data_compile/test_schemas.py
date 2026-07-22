"""Contract tests for the closed SPEC-002 JSON Schema set."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

from jsonschema import Draft202012Validator
from referencing import Registry, Resource


ROOT = Path(__file__).resolve().parents[2]
SCHEMA_DIR = ROOT / "data" / "schemas"
NAMES = (
    "common", "manifest", "unit", "building", "tech", "civ", "map", "ai-profile",
)


def provenance(evidence: str = "N") -> dict:
    claims = {"evidence": evidence, "verification_reports": [], "sources": []}
    if evidence in {"H", "C"}:
        claims.update({"verification_reports": ["verify/report.md"], "sources": [{"citation": "Source"}]})
    if evidence in {"P", "X"}:
        claims.update({"verification_reports": ["verify/report.md"], "sources": [{"citation": "Source"}], "evidence_label_key": "base:evidence.label"})
    return {
        "status": "promoted", "generator": "human", "task_id": "schema-tests",
        "generated_on": "2026-07-22", "reviewed_by": ["architect"],
        "historical_claims": claims,
        "balance_design": {"author": "architect", "rationale": "Gameplay value.", "reviewed_by": ["architect"]},
    }


def availability() -> dict:
    return {"playable_period_ids": ["rome:late_republic"], "availability_mode": "historical"}


def fixtures() -> dict:
    manifest = {
        "schema_version": 1, "package_id": "chunsa.base", "package_version": "0.4.0",
        "package_kind": "base", "owned_namespaces": ["base", "rome"],
        "declared_capabilities": ["rome:roads"], "declared_behaviors": ["base:melee_line"],
        "declared_variant_groups": ["base:metalworking"],
        "declared_materials": [{"id": "base:copper", "name_key": "base:material.copper", "kind": "deposit", "resource": "Me", "strategic": True}],
        "dependencies": [], "load_after": [], "provenance": provenance(),
    }
    unit = {
        "schema_version": 2, "id": "rome:legionary", "display_name_key": "rome:unit.legionary", "description_key": "rome:unit.legionary_desc", "civ_id": "rome:module", "epoch_window": [5, 5], "class": "infantry", "tags": ["formation_capable"], "resource_costs": {"A": 10}, "stats": {"hp": 100, "attack": 10, "range_millitiles": 1000, "speed_millitile_tick": 100, "morale": 80, "build_time_ticks": 40}, **availability(), "provenance": provenance(),
    }
    building = {
        "schema_version": 1, "id": "rome:forum", "civ_id": "rome:module", "display_name_key": "rome:building.forum", "description_key": "rome:building.forum_desc", "epoch_window": [5, 5], "kind": "civic", "footprint": {"width_cells": 2, "height_cells": 2, "blocks_movement": True}, "stats": {"hp": 1000}, "constructible": True, "resource_costs": {"P": 50}, "build_time_ticks": 100, "dropoff_resources": [], "trains": [], "researches": [], "required_capabilities": [], "grants_capabilities": [], "recipes": [], **availability(), "provenance": provenance(),
    }
    tech = {
        "schema_version": 1, "id": "rome:roads", "display_name_key": "rome:tech.roads", "description_key": "rome:tech.roads_desc", "available_to": ["rome:module"], "epoch": 5, "branch": "C", "evidence": "H", "resource_costs": {"P": 20}, "required_capabilities": [], "research_time_ticks": 100, "prerequisites": [], "required_buildings": [], "mutually_exclusive_with": [], "grants": {"units": [], "buildings": [], "capabilities": ["rome:roads"]}, **availability(), "provenance": provenance("H"),
    }
    civ = {
        "schema_version": 1, "id": "rome:module", "historical_window": {"start_year": -509, "end_year": 476}, "epoch_window": [5, 5], "region_key": "rome:region", "display_name_key": "rome:module", "description_key": "rome:module_desc", "gameplay_verb": {"id": "rome:roads", "name_key": "rome:verb.roads", "description_key": "rome:verb.roads_desc"}, "playable_periods": [{"id": "rome:late_republic", "start_year": -133, "end_year": -27, "label_key": "rome:period.late_republic"}], "institutions": [], "social_tensions": [], "unit_ids": ["rome:legionary"], "building_ids": ["rome:forum"], "tech_ids": ["rome:roads"], "art_rule_keys": ["rome:art.rule"], "review_roles": ["historian"], "provenance": provenance("H"),
    }
    game_map = {
        "schema_version": 1, "id": "base:test_map", "display_name_key": "base:map.test", "width_tiles": 1, "height_tiles": 1, "biome": "plain", "terrain_rle": [{"terrain": "plain", "run": 1}], "cost_rle": [{"cost": 1, "run": 1}], "resource_spawns": [], "starting_positions": [{"slot": 0, "x_millitiles": 0, "y_millitiles": 0}], "recommended_epoch_window": [1, 1], "provenance": provenance(),
    }
    ai = {
        "schema_version": 1, "id": "base:balanced_normal", "display_name_key": "base:ai.balanced", "personality": "balanced", "difficulty": "normal", "strategic_weights_bp": {"economy_focus_bp": 5000, "military_focus_bp": 5000, "tech_focus_bp": 5000, "expansion_aggressiveness_bp": 5000, "risk_tolerance_bp": 5000, "diplomacy_openness_bp": 5000}, "utility_curves": [{"consideration": "threat", "points": [{"input_bp": 0, "output_bp": 0}, {"input_bp": 10000, "output_bp": 10000}]}], "tactical_behaviors": [], "difficulty_params": {"decision_period_ticks": 20, "reaction_latency_ticks": 20, "micro_quality_bp": 5000, "build_order_variance_bp": 5000, "scouting_thoroughness_bp": 5000, "counter_reaction_delay_ticks": 20}, "performance_lod": {"min_decision_period_ticks": 20, "cache_ttl_ticks": 20}, "provenance": provenance(),
    }
    return {"manifest": manifest, "unit": unit, "building": building, "tech": tech, "civ": civ, "map": game_map, "ai-profile": ai}


class SchemaContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.schemas = {name: json.loads((SCHEMA_DIR / f"{name}.schema.json").read_text(encoding="utf-8")) for name in NAMES}
        for schema in cls.schemas.values():
            Draft202012Validator.check_schema(schema)
        resources = [(schema["$id"], Resource.from_contents(schema)) for schema in cls.schemas.values()]
        cls.registry = Registry().with_resources(resources)
        cls.validators = {name: Draft202012Validator(schema, registry=cls.registry) for name, schema in cls.schemas.items() if name != "common"}

    def assert_valid(self, name, instance):
        self.assertEqual([], list(self.validators[name].iter_errors(instance)))

    def assert_invalid(self, name, instance):
        self.assertTrue(list(self.validators[name].iter_errors(instance)))

    def test_minimal_fixtures_validate(self):
        for name, instance in fixtures().items():
            with self.subTest(name=name):
                self.assert_valid(name, instance)

    def test_extra_property_rejected(self):
        item = copy.deepcopy(fixtures()["unit"]); item["unexpected"] = True
        self.assert_invalid("unit", item)

    def test_float_rejected(self):
        item = copy.deepcopy(fixtures()["unit"]); item["stats"]["hp"] = 1.5
        self.assert_invalid("unit", item)

    def test_citizen_rules(self):
        item = copy.deepcopy(fixtures()["unit"]); item["class"] = "citizen"; item["stats"]["attack"] = 1
        self.assert_invalid("unit", item)
        item["stats"]["attack"] = 0; item["tags"] = []
        self.assert_invalid("unit", item)

    def test_suppression_tags_exclusive(self):
        item = copy.deepcopy(fixtures()["unit"]); item["tags"] = ["suppression_resist_low", "suppression_resist_high"]
        self.assert_invalid("unit", item)

    def test_availability_requirements(self):
        item = copy.deepcopy(fixtures()["unit"]); item["counterfactual_label_key"] = "rome:label"
        self.assert_invalid("unit", item)
        item["availability_mode"] = "counterfactual"; item.pop("counterfactual_label_key")
        self.assert_invalid("unit", item)

    def test_provenance_requirements(self):
        item = copy.deepcopy(fixtures()["unit"]); item["provenance"] = provenance("P"); item["provenance"]["historical_claims"].pop("evidence_label_key")
        self.assert_invalid("unit", item)
        item = copy.deepcopy(fixtures()["unit"]); item["provenance"] = provenance("H"); item["provenance"]["historical_claims"]["sources"] = [{"citation": "Source", "url": "https://example.invalid"}]
        self.assert_invalid("unit", item)

    def test_building_constructibility(self):
        item = copy.deepcopy(fixtures()["building"]); item["constructible"] = False
        self.assert_invalid("building", item)
        item = copy.deepcopy(fixtures()["building"]); item["resource_costs"] = {}
        self.assert_invalid("building", item)
        item = copy.deepcopy(fixtures()["building"]); item["resource_costs"] = {"P": 0}
        self.assert_invalid("building", item)

    def test_dropoff_resources_only_required_for_dropoff(self):
        item = copy.deepcopy(fixtures()["building"]); item.pop("dropoff_resources")
        self.assert_valid("building", item)
        item["kind"] = "dropoff"
        self.assert_invalid("building", item)

    def test_unit_requires_positive_resource_cost(self):
        item = copy.deepcopy(fixtures()["unit"]); item["resource_costs"] = {"A": 0}
        self.assert_invalid("unit", item)

    def test_tech_evidence_must_match_provenance(self):
        item = copy.deepcopy(fixtures()["tech"]); item["provenance"] = provenance("C")
        self.assert_invalid("tech", item)

    def test_noninstitution_tech_requires_positive_resource_cost(self):
        item = copy.deepcopy(fixtures()["tech"]); item["resource_costs"] = {"P": 0}
        self.assert_invalid("tech", item)

    def test_map_and_ai_bounds(self):
        item = copy.deepcopy(fixtures()["map"]); item["terrain_rle"][0]["run"] = 0
        self.assert_invalid("map", item)
        item = copy.deepcopy(fixtures()["ai-profile"]); item["utility_curves"][0]["points"] = [{"input_bp": 0, "output_bp": 0}]
        self.assert_invalid("ai-profile", item)


if __name__ == "__main__":
    unittest.main()
