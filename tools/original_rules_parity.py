#!/usr/bin/env python3
"""Bounded, authored cross-implementation probes; private fork/data stay read-only.

An expected discrepancy is not evidence of correctness. Each numerical result
has its own evidence classification; unsupported World/clock/wall integration
is recorded separately and cannot silently become a parity result.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
FORK_REVISION = "d861d2912337c09751cb68196f85d5a30752726e"
MODEL_SHA256 = "7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6"
HOUSING_EXPECTED = [35,43,54,59,193,72,207,1,0,3,3,4,4,1,1,3,4,1]
HOUSING_FORK_MODULES = ("ModelData", "Desirability", "HouseEvolution", "OriginalHousingView")
HOUSING_SIMULATION_DEPENDENCIES = ("original-rules-model", "original-desirability",
                                   "original-goods-ledger", "original-goods-rules",
                                   "original-service-rules",
                                   "original-house-evolution", "original-model")
# Reviewed Rules13-21 API: the actual fourth-module probe retains its
# historical HouseholdV3/CityV7 calls. Rule19 adds explicit retirement factories,
# predicates, blockers and an optional production-state journal, mandatory only
# in that new generation. Historical defaults and probe behavior stay exact.
# Rule20 appends its source-cell command, explicit APIs and default-empty World/
# snapshot members. Its directly included SourceCellRamp DTO is pinned separately;
# fresh matching libraries retain the unchanged HouseholdV3/CityV7 observations.
# Rule21 appends finite trade commands/courier identity and default-empty trade
# state plus immutable boundary/model context. Its new direct AuthoredLandTrade
# DTO is pinned separately; old Original predicates and the actual historical
# probe calls remain unchanged. World layout therefore requires matching fresh
# libraries; the pin is a reviewed ABI boundary, not trade parity evidence.
# Matching archives are freshly linked; these exact headers do not certify
# the new rules' gameplay, dispatch or terrain logic.
# Rule22 culture/model additions preserve legacy calls; these are ABI pins only.
# Rule23 appends optional venue-walker evidence and explicit APIs. Its provider
# and courier identities are admitted only by that generation. Older optional
# values stay empty and legacy probe behavior must remain byte-identical. The
# matching fresh archives and three strict DTO/API pins prevent stale ABI reuse;
# these pins do not certify Rule23 gameplay, saves or Original roaming parity.
REVIEWED_HOUSING_WORLD_API_GIT_BLOBS = {
    "OriginalCultureState.h": "c577ef5b6111833508a718b967be649597dce55e",
    "World.h": "7c504432a42cb3a254900b08f04c1b020efae0fd",
    "AuthoredLandTrade.h": "88647c25c07976be248851cf608bab822189c67a",
    "SourceCellRamp.h": "7279cfbd61fd20ac24e35739c0f5f3138380dd38",
    "OriginalTerrainFacts.h": "1a24ed7cb80a0152560b76d9445c04e5b0829ed5",
    "OriginalHousingDiagnosis.h": "62d1ebd836ae700c3f790ef993c155b589d2a3d1",
    "OriginalSafeDemolitionState.h": "a4f27b05d422d004cc4bfcae49ccc5f67a6c6121",
}
# OE-00's independently retrieved, pinned file identities. A mutable local
# manifest is not trusted as the source of the expected hashes.
FORK_SHA256 = {
    "CMakeLists.txt": "fe08f23fb8116aa82df4aa1e7a1b68a951b2762247750c0a48f80795e6edc8b6",
    "LICENSE": "8dce34779ad07c246757a4e8ecb0ad9df60a89f49c3d61db7c60d9aea6740b15",
    "src/simulation/original/Desirability.cpp": "69988ee13253eb979f9407cc27e1678804132e88b32320565182a865af066476",
    "src/simulation/original/Desirability.h": "26949b26de3f085382121e04617c79d6cd5115ef12cdcba5a7c0ab73248e6eb1",
    "src/simulation/original/HouseEvolution.cpp": "2cc416aa263b867664a174b979b3cab8f25d2880ed68f0e8f00acaba37d67f75",
    "src/simulation/original/HouseEvolution.h": "0ebf2055fd477dd46c453e0555d5e0fc9977e9da5d386119b68bdf314246b306",
    "src/simulation/original/ModelData.cpp": "b56961fd7533ff93ae5c281824d5092cdfd3f5292e813a29c9e9a250764dbc8d",
    "src/simulation/original/ModelData.h": "ce06e760bdf8ad7171c9afda52f1c90e1b6e3d0a5e07970855406fb2ff509afc",
    "src/simulation/original/OriginalHousingView.cpp": "054f4cdf4f25c5aeb521cfacf7003c2c662b6ceeac7df2d3959b6a00af00357d",
    "src/simulation/original/OriginalHousingView.h": "54e6cc589381fda9c90567fa71a9aa31555c2623a399a8357c48c414d8b0307e",
    "tests/OriginalRulesTests.cpp": "389f154cff0e3f1583ece58d51fd35778c78bd51b33202982220ee05fc8f9c25",
}
HOUSE_LABELS = ["1: Shelter", "2: Hut", "3: Plain Cottage", "4: Attractive Cottage",
                "5: Spacious Dwelling", "6: Elegant Dwelling", "7: Ornate Apartment",
                "8: Lux. Apartment", "Elite: Unoccupied", "Elite 0: Abandoned",
                "Elite 1: Modest Siheyuan", "Elite 2: Lavish Siheyuan",
                "Elite 3: Humble Compound", "Elite 4: Impressive Compound",
                "Elite 5: Heavenly Compound"]
DIFFICULTY_LABELS = ["Very Easy Modifiers", "Easy Modifiers", "Normal Modifier",
                     "Hard Modifiers", "Very Hard Modifiers"]
CONFIGURATIONS = {
    "debug": (["-O0", "-g"], ".local/market-walkers-finish/debug-build"),
    "release": (["-O2", "-DNDEBUG"], ".local/reswall/release-build"),
    "sanitizer": (["-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"],
                  ".local/market-walkers-finish/sanitizer-build"),
}


class SkipComparison(Exception):
    """A genuinely absent optional study source."""


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def validate_fork(path: Path) -> dict[str, Any]:
    if not path.exists() and not path.is_symlink():
        raise SkipComparison("optional pinned fork mirror is absent")
    if not path.is_dir():
        raise ValueError("present fork path is not a directory")
    hashes = {}
    for name, expected in FORK_SHA256.items():
        candidate = path / name
        if not candidate.is_file() or candidate.is_symlink() or candidate.stat().st_size > 512*1024:
            raise ValueError(f"present fork mirror is incomplete/unsafe: {name}")
        actual = sha256(candidate)
        if actual != expected:
            raise ValueError(f"pinned fork source mismatch: {name}")
        hashes[name] = actual
    manifest = path / "file-manifest.json"
    if not manifest.is_file() or manifest.stat().st_size > 64 * 1024:
        raise ValueError("present fork mirror has no bounded provenance manifest")
    metadata = json.loads(manifest.read_text())
    if not isinstance(metadata,dict):
        raise ValueError("fork manifest must be an object")
    if metadata.get("fork_head") != FORK_REVISION:
        raise ValueError("fork manifest revision mismatch")
    entries = metadata.get("files", [])
    if not isinstance(entries,list) or any(not isinstance(v,dict) for v in entries) or len(entries) != len(FORK_SHA256) or {v.get("path") for v in entries} != set(FORK_SHA256):
        raise ValueError("fork manifest file set mismatch")
    for entry in entries:
        if entry.get("sha256") != hashes[entry["path"]] or entry.get("bytes") != (path / entry["path"]).stat().st_size:
            raise ValueError("fork manifest file provenance mismatch")
    return {"revision": FORK_REVISION, "files": hashes,
            "code_permalinks":{name:f"https://github.com/amscova/OpenEmperor/blob/{FORK_REVISION}/{name}" for name in hashes if name.startswith("src/")}}


def fresh_output(path: Path, *, repo: Path = ROOT, inputs: tuple[Path, ...] = ()) -> Path:
    candidate = path.resolve()
    repository = repo.resolve()
    private = repository / ".local"
    lexical = Path(os.path.abspath(path))
    lexical_repository = Path(os.path.abspath(repo))
    if (lexical.is_relative_to(repository) or lexical.is_relative_to(lexical_repository)) and not candidate.is_relative_to(private):
        raise ValueError("repository output must not escape .local through a symlink")
    if candidate == repository or (candidate.is_relative_to(repository) and not candidate.is_relative_to(private)):
        raise ValueError("comparison output inside the repository must be under .local")
    known_inputs = (repository / ".local/gog-extracted/app", repository / ".local/original-economy/oe00/fork",
                    *(repository / values[1] for values in CONFIGURATIONS.values()))
    for source in (*inputs,*known_inputs):
        source = source.resolve()
        if candidate == source or candidate.is_relative_to(source) or source.is_relative_to(candidate):
            raise ValueError("output must be separate from read-only inputs")
    if path.exists() or path.is_symlink():
        raise ValueError("output directory already exists; use a fresh path")
    candidate.mkdir(parents=True, exist_ok=False)
    return candidate


def authored_fixture(kind: str = "base") -> str:
    """New small numerical cases, not a transcription of the original table."""
    rows = [[0] * 24 for _ in range(15)]
    for index, row in enumerate(rows):
        row[0:2] = [-5, 5]
        row[17] = index + 1
        if kind == "goods":
            for field in (9, 10, 11, 12, 13):
                row[field] = index + 2
        elif kind == "three":
            for field in (9, 10, 11, 12, 13):
                row[field] = 3
        elif kind == "distinct_goods":
            for field, amount in zip((9,10,11,12,13),(2,4,6,8,10)):
                row[field] = amount
    if kind == "religion":
        rows[1][23] = 1
    elif kind == "bronze":
        rows[1][13] = 5
    elif kind == "required_water":
        rows[0][3] = 1
    elif kind == "arithmetic":
        rows[0][17] = 150
        rows[1][17] = -110
    def line(label: str, values: list[int]) -> str:
        return label + ",{," + ",".join(map(str, values)) + ",},,,\n"
    text = "Independently authored OE-07 comparison fixture\nBUILDING MODS\n"
    for i, label in enumerate(DIFFICULTY_LABELS):
        text += line(f"{i},{label}", [75 if kind == "arithmetic" else 100] * 13)
    text += "ALL BUILDINGS\n" + line("72,Well", [-123] + [0] * 12)
    text += "HOUSE MODS\n"
    for label in DIFFICULTY_LABELS:
        text += line(label, [5 if f == 17 and kind == "arithmetic" else 0 for f in range(24)])
    text += "ALL HOUSES\n"
    for label, row in zip(HOUSE_LABELS, rows):
        text += line(label, row)
    if kind == "duplicate_building":
        text = text.replace("HOUSE MODS\n", line("72,Well", [0] * 13) + "HOUSE MODS\n")
    elif kind == "extra_house_value":
        text = text.replace(line(HOUSE_LABELS[0], rows[0]), line(HOUSE_LABELS[0], rows[0] + [99]))
    if kind != "missing_end":
        text += "End of model data.\n-------------------\n"
    return text


def house(**changes: int) -> list[int]:
    names = "type desirability occupants counter water herbalist acupuncture music acrobat drama ancestor confucian daoist buddhist quality hemp ceramics tea silk bronzeware lacquerware".split()
    values = [4, 0, 1, 0, 0] + [0] * 16
    for name, value in changes.items():
        values[names.index(name)] = value
    return values


def cases() -> list[dict[str, Any]]:
    result: list[dict[str, Any]] = []
    def add(identity: str, area: str, operation: str, values: list[int], fork: list[int], current: list[int] | None = None,
            fixture: str = "base", classification: str = "parity", evidence: str = "") -> None:
        if not evidence:
            if area == "service":
                evidence = "docs/reverse/original-service-spawn-contract.md" if identity.startswith("spawn.") else "docs/reverse/original-service-timer-contract.md"
            elif area == "goods":
                evidence = "docs/reverse/original-goods-consumption-food-contract.md" if identity.startswith(("consume.","food.")) else "docs/reverse/original-goods-delivery-contract.md"
            else:
                evidence = f"docs/reverse/original-{area}-contract.md"
        result.append({"id": identity, "area": area, "classification": classification,
                       "evidence": evidence,
                       "input": {"fixture": fixture, "operation": operation, "values": values},
                       "expected": {"fork": fork, "current": fork if current is None else current}})
    model_evidence = "docs/reverse/original-model-contract.md"
    for identity, operation, values, expected in [
        ("model.signed_percent", "building_value", [72, 0, 2], [-123, -92]),
        ("model.house_upper_clamp", "house_value", [0, 17, 2], [150, 100]),
        ("model.house_lower_clamp", "house_value", [1, 17, 2], [-110, -99]),
    ]:
        add(identity, "model", operation, values, expected, fixture="arithmetic", evidence=model_evidence)
    for fixture in ("duplicate_building", "extra_house_value", "missing_end"):
        add(f"parser.{fixture}", "model", "parse", [], [1], [0], fixture=fixture,
            classification="unsupported", evidence="docs/original-economy/model-provenance.md")
    for identity, vals, expected, current in [
        ("rings.positive_decay", [5,5,1,9,1,-3,5], [0,9,6,3,0,0], None),
        ("rings.negative_decay", [5,5,1,-9,1,3,5], [0,-9,-6,-3,0,0], None),
        ("rings.zero_step", [5,5,1,9,0,-3,5], [0,9,9,9,9,9], [0,9,6,3,0,0]),
        ("rings.negative_step", [5,5,1,-9,-2,3,5], [0,-9,-9,-9,-9,-9], [0,-9,-6,-3,0,0]),
        ("rings.footprint_two", [5,5,2,7,1,0,2], [0,0,7,7,0,0], None),
    ]:
        add(identity,"desirability","spread",vals,expected,current,
            classification="parity" if current is None else "corrected_bug")
    add("rings.order_ppn","desirability","order",[100,100,-100],[0])
    add("rings.order_pnp","desirability","order",[100,-100,100],[100])
    add("needs.religion_or","house-needs","needs",[1]+house(daoist=1),[1],fixture="religion")
    add("needs.religion_missing","house-needs","needs",[1]+house(),[0],fixture="religion")
    add("needs.bronze_lacquer_not_sum","house-needs","needs",[1]+house(bronzeware=3,lacquerware=3),[0],fixture="bronze")
    add("needs.lacquer_alternative","house-needs","needs",[1]+house(lacquerware=5),[1],fixture="bronze")
    add("evolution.threshold_equality","house-evolution","evolve",house(desirability=5),[5,0])
    add("evolution.devolve_limit","house-evolution","evolve",house(desirability=-5,counter=9),[3,0])
    add("evolution.vacant_needs","house-evolution","evolve",house(type=2,desirability=0),[3,0],[2,0],fixture="required_water",classification="corrected_bug")
    add("evolution.empty_shelter","house-evolution","evolve",house(type=3,occupants=0,desirability=5),[4,0],[3,0],classification="corrected_bug")
    add("evolution.elite_abandoned","house-evolution","evolve",house(type=12,desirability=-100),[13,0])
    add("timers.byte_edges","service","timers",house(herbalist=0,acupuncture=1,music=2,acrobat=40,drama=96,ancestor=127,confucian=128,daoist=254,buddhist=255),[0,0,1,39,95,126,127,253,254])
    for role in range(9):
        add(f"visit.occupied_role_{role}","service","visit",[role]+house(),[1,96 if role<5 else 40])
    add("visit.empty_elite_service","service","visit",[0]+house(type=13,occupants=0),[1,96],[0,0],classification="corrected_bug")
    add("visit.empty_elite_religion","service","visit",[5]+house(type=13,occupants=0),[1,40])
    for identity, provider, staffing, fork, current in [
        ("acupuncture",1,50,7,7),("herbalist",0,50,7,5),
        ("music",2,100,1,3),("drama",4,100,1,6),
        ("shrine",5,100,3,3),("temple",7,100,3,6),
    ]:
        add(f"spawn.{identity}","service","spawn",[provider,staffing,0],[fork],[current],classification="parity" if fork==current else "corrected_bug")
    add("spawn.strict_threshold","service","spawn_tick",[1,100,1],[0,1])
    add("spawn.no_staff","service","spawn_tick",[1,0,64],[0,1],[64,0],classification="corrected_bug")
    add("spawn.byte_wrap","service","spawn_tick",[1,100,255],[0,1],[0,0],classification="corrected_bug")
    for phase in (0,1,7):
        add(f"composition.authored_phase_{phase}","composition","composition",[phase]+house(herbalist=2,hemp=5,ceramics=5,tea=5,silk=5,bronzeware=5,lacquerware=5),[4,0,1,5 if phase==1 else 2],fixture="three",classification="unverified",evidence="docs/original-economy/core-parity.md#clock-and-wrapper")
    add("goods.next_row","goods","need",[0,1]+house(),[3],[4],fixture="goods",classification="corrected_bug")
    add("goods.large_elite","goods","need",[0,2]+house(type=13),[48],[104],fixture="goods",classification="corrected_bug")
    add("goods.luxury_row_stays","goods","need",[0,1]+house(type=10),[9],fixture="goods")
    add("goods.empty_hemp_query","goods","need",[0,2]+house(occupants=0),[10],fixture="goods")
    for commodity, need in enumerate((2,4,6,8,10,10)):
        add(f"goods.distinct_field_{commodity}","goods","need",[commodity,1]+house(),[need],fixture="distinct_goods")
    for identity, source, expected_fork, expected_current in [
        ("large_source",100,[3,97,3],[10,90,10]),
        ("partial_source",5,[3,2,3],[3,2,3]),
        ("tiny_source",2,[2,0,2],[2,0,2]),
    ]:
        add(f"delivery.{identity}","goods","delivery",[0,1]+house()+[source],expected_fork,expected_current,fixture="three",classification="parity" if expected_fork==expected_current else "corrected_bug")
    add("delivery.empty_common","goods","delivery",[0,1]+house(occupants=0)+[100],[10,90,10],[0,100,0],fixture="three",classification="corrected_bug")
    add("delivery.stock_above_need_tiny_source","goods","delivery",[0,1]+house(hemp=7)+[1],[1,0,8],[0,1,7],fixture="three",classification="corrected_bug")
    add("delivery.stock_below_need_large_source","goods","delivery",[0,1]+house(hemp=2)+[100],[1,99,3],[8,92,10],fixture="three",classification="corrected_bug")
    add("delivery.full_storage","goods","delivery",[0,1]+house(hemp=32767)+[2147483647],[0,2147483647,32767],fixture="three")
    add("consume.separate_alternatives","goods","consume",house(hemp=5,ceramics=5,tea=5,silk=5,bronzeware=5,lacquerware=5),[2]*6,fixture="three")
    add("consume.distinct_fields","goods","consume",house(hemp=31,ceramics=29,tea=23,silk=19,bronzeware=17,lacquerware=13),[29,25,17,11,7,3],fixture="distinct_goods")
    add("consume.empty","goods","consume",house(occupants=0,hemp=5),[5,0,0,0,0,0],fixture="three")
    add("consume.negative_occupants","goods","consume",house(occupants=-1,hemp=5),[5,0,0,0,0,0],[2,0,0,0,0,0],fixture="three",classification="corrected_bug")
    add("food.improved","goods","food",[10,20,1,10],[20])
    add("food.weighted_half","goods","food",[90,30,50,100],[70])
    add("food.weighted_above_half","goods","food",[90,30,51,100],[60])
    add("food.zero_delivery_boundary","goods","food",[10,20,0,1],[20],[10],classification="unsupported",evidence="docs/reverse/original-goods-consumption-food-contract.md")
    for identity, amount, fork, current in [("half",50000001,60,70),("third_decimal",33000001,70,75),("three",300000001,45,50)]:
        add(f"food.binary32_{identity}","goods","food",[90,30,amount,100000000],[fork],[current],classification="unverified",evidence="docs/reverse/original-goods-consumption-food-contract.md")
    return result


def compare_observations(case: dict[str, Any], observed: dict[str, list[int]]) -> dict[str, Any]:
    if set(observed) != {"fork","current"} or any(not isinstance(row,list) or any(type(value) is not int for value in row) for row in observed.values()) or any(not isinstance(row,list) or any(type(value) is not int for value in row) for row in case["expected"].values()):
        raise ValueError("comparison requires two strict numerical observations")
    return {**case, "observed": observed,
            "passed": observed == case["expected"]}


def validate_cases(catalog: list[dict[str, Any]]) -> None:
    if not catalog or len({case["id"] for case in catalog}) != len(catalog):
        raise ValueError("empty catalog or duplicate case IDs")
    for case in catalog:
        if case["classification"] not in {"parity","corrected_bug","unsupported","unverified"}:
            raise ValueError("unknown evidence classification")
        if set(case["expected"]) != {"fork","current"} or any(not isinstance(row,list) or any(type(value) is not int for value in row) for row in case["expected"].values()):
            raise ValueError("expected observations must be two strict numerical arrays")
        source, _, anchor = case["evidence"].partition("#")
        evidence = ROOT / source
        if not evidence.is_file() or not source.startswith("docs/"):
            raise ValueError(f"missing case evidence: {source}")
        if anchor:
            import re
            headings = [re.sub(r"[^\w -]", "", line.lstrip("#").strip().lower()).replace(" ","-") for line in evidence.read_text().splitlines() if line.startswith("#")]
            if anchor not in headings:
                raise ValueError(f"missing case evidence anchor: {case['evidence']}")


def checked_run(command: list[str], *, stdin: str | None = None, timeout: int = 180) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, input=stdin, text=True, capture_output=True, timeout=timeout, check=False)
    if result.returncode:
        raise RuntimeError(f"command failed ({result.returncode}): {shlex.join(command)}\n{result.stderr[-8000:]}")
    return result


def isolate_housing_source(name: str, source: str) -> str:
    """Rename only the pinned namespace and four local includes, never rules.

    World now includes the current original DTO types. The separate fourth-
    module adapter therefore needs distinct fork symbols while using the exact
    current World ABI. Numerical fork/current probes remain untransformed.
    """
    allowed = {module + suffix for module in HOUSING_FORK_MODULES for suffix in (".h", ".cpp")}
    if name not in allowed:
        raise ValueError("housing isolation source is outside the four reviewed modules")
    namespace_open = "namespace openemperor::original {"
    namespace_close = "}  // namespace openemperor::original"
    includes = {f'#include "simulation/original/{module}.h"': f'#include "{module}.h"'
                for module in HOUSING_FORK_MODULES}
    opened = closed = 0
    output = []
    for line in source.splitlines(keepends=True):
        content = line.removesuffix("\n")
        if content == namespace_open:
            opened += 1
            content = "namespace openemperor::parity_fork_original {"
        elif content == namespace_close:
            closed += 1
            content = "}  // namespace openemperor::parity_fork_original"
        elif content in includes:
            content = includes[content]
        elif "openemperor::original" in content or '#include "simulation/original/' in content:
            raise ValueError("housing isolation encountered an unreviewed namespace/include spelling")
        output.append(content + ("\n" if line.endswith("\n") else ""))
    if opened != 1 or closed != 1:
        raise ValueError("housing isolation requires exactly one reviewed namespace declaration")
    return "".join(output)


def prepare_housing_fork(fork: Path, output: Path) -> Path:
    isolated = output / "housing-fork-source"
    isolated.mkdir(exist_ok=False)
    files = {}
    for module in HOUSING_FORK_MODULES:
        for suffix in (".h", ".cpp"):
            name = module + suffix
            relative = "src/simulation/original/" + name
            source = fork / relative
            if source.is_symlink() or not source.is_file():
                raise ValueError(f"unsafe housing fork source: {relative}")
            original = source.read_bytes()
            if hashlib.sha256(original).hexdigest() != FORK_SHA256[relative]:
                raise ValueError(f"pinned housing fork source mismatch: {relative}")
            transformed = isolate_housing_source(name, original.decode("utf-8")).encode("utf-8")
            (isolated / name).write_bytes(transformed)
            files[name] = {"source_sha256": FORK_SHA256[relative],
                           "generated_sha256": hashlib.sha256(transformed).hexdigest()}
    (isolated / "transform-manifest.json").write_text(json.dumps({
        "namespace": "openemperor::parity_fork_original", "files": files,
        "changes": "Exact namespace declaration/comment and local includes only; no rule logic changes."
    }, indent=2, sort_keys=True) + "\n")
    return isolated


def housing_link_libraries(simulation_library: Path) -> tuple[Path, ...]:
    # Match the production archive dependency order without compiling another
    # copy of current rules into the namespace-isolated fork housing adapter.
    return (simulation_library, *(simulation_library.parent / ("libopenemperor-" + name + ".a")
                                  for name in HOUSING_SIMULATION_DEPENDENCIES))


def compile_probes(fork: Path, output: Path, compiler: str, configuration: str, simulation_library: Path) -> dict[str, Path]:
    flags = ["-std=c++20", "-Wall", "-Wextra", "-Wpedantic", "-Wconversion", "-Wshadow", *CONFIGURATIONS[configuration][0]]
    openssl = shlex.split(checked_run(["pkg-config", "--cflags", "--libs", "openssl"]).stdout)
    module = "src/simulation/original/"
    adapter = ROOT / "tools/original-rules-parity/RuleProbe.cpp"
    binaries = {key: output / key for key in ("fork-probe", "current-probe", "housing-probe")}
    common_fork = [str(fork / (module + name + ".cpp")) for name in ("ModelData", "Desirability", "HouseEvolution")]
    housing_fork = prepare_housing_fork(fork, output)
    commands = {
        "fork-probe": [compiler,*flags,"-DOE_PARITY_FORK=1","-I",str(fork / "src"),str(adapter),*common_fork,"-o",str(binaries["fork-probe"])],
        "current-probe": [compiler,*flags,"-I",str(ROOT / "src"),str(adapter),*[str(ROOT / (module+name+".cpp")) for name in ("ModelData","ModelSchema","Desirability","HouseEvolution","ServiceRules","GoodsRules")],*openssl,"-o",str(binaries["current-probe"])],
        "housing-probe": [compiler,*flags,"-I",str(housing_fork),"-I",str(ROOT / "src"),str(ROOT / "tools/original-rules-parity/HousingViewProbe.cpp"),*[str(housing_fork / (name+".cpp")) for name in HOUSING_FORK_MODULES],*[str(path) for path in housing_link_libraries(simulation_library)],*openssl,"-o",str(binaries["housing-probe"])],
    }
    for key, command in commands.items():
        result = checked_run(command)
        (output / (key+"-compile.log")).write_text(shlex.join(command)+"\n"+result.stdout+result.stderr)
    return binaries


def parse_probe_output(stdout: str, expected_count: int) -> list[list[int]]:
    values = [json.loads(line) for line in stdout.splitlines()]
    if len(values) != expected_count or any(not isinstance(row,list) or any(type(v) is not int for v in row) for row in values):
        raise RuntimeError("adapter produced an invalid numerical result set")
    return values


def probe(binary: Path, fixture: Path, commands: list[str]) -> list[list[int]]:
    result = checked_run([str(binary),str(fixture)], stdin="\n".join(commands)+"\n", timeout=60)
    return parse_probe_output(result.stdout,len(commands))


def public_report(report: dict[str, Any]) -> dict[str, Any]:
    """Portable, bounded evidence suitable for review; excludes local paths/logs."""
    if report.get("schema_version") != 1 or report.get("status") != "PASS" or not report.get("cases") or any(case.get("passed") is not True or case.get("observed") != case.get("expected") for case in report["cases"]):
        raise ValueError("only a complete successful comparison can be published as evidence")
    validate_cases(report["cases"])
    canonical = {case["id"]:case for case in cases()}
    if len(report["cases"]) != len(canonical) or any({key:case[key] for key in canonical.get(case["id"],{})} != canonical.get(case["id"]) for case in report["cases"]):
        raise ValueError("report differs from the independently specified canonical cases")
    for case in report["cases"]:
        compare_observations(case,case["observed"])
    if {case["area"] for case in report["cases"]} != {"model","desirability","house-needs","house-evolution","service","goods","composition"} or report.get("housing_view",{}).get("passed") is not True:
        raise ValueError("incomplete four-module comparison report")
    expected_summary = {"cases":len(canonical),"passed":len(canonical),"classifications":{kind:sum(case["classification"]==kind for case in canonical.values()) for kind in ("parity","corrected_bug","unverified","unsupported")}}
    if report.get("summary") != expected_summary:
        raise ValueError("comparison summary does not match the verified case set")
    fork = report.get("fork",{})
    if fork.get("revision") != FORK_REVISION or fork.get("files") != FORK_SHA256:
        raise ValueError("comparison is not bound to the reviewed fork pin")
    if report.get("current",{}).get("files") != current_source_hashes():
        raise ValueError("comparison source fingerprints are stale")
    housing = report["housing_view"]
    if housing.get("fork_expected") != HOUSING_EXPECTED or housing.get("fork_observed") != HOUSING_EXPECTED or any(type(value) is not int for value in housing.get("fork_observed",[])):
        raise ValueError("fourth-module observation does not match its expectation")
    real = report.get("real_model",{})
    if real.get("status") not in {"PASS","NOT_RUN"}:
        raise ValueError("real comparison needs an explicit successful or not-requested status")
    if real["status"] == "PASS":
        values = real.get("observed",{})
        if real.get("sha256") != MODEL_SHA256 or set(values) != {"fork","current"} or values["fork"] != values["current"] or len(values["fork"]) != 3 or values["fork"][0] != 1800 or any(type(value) is not int for row in values.values() for value in row):
            raise ValueError("real-model digest or source identity is inconsistent")
    return {key: report[key] for key in ("schema_version","status","fork","current","cases","summary","real_model","housing_view","limitations") if key in report}


def current_source_hashes() -> dict[str,str]:
    module = ROOT / "src/simulation/original"
    sources = list(module.glob("*.h")) + list(module.glob("*.cpp"))
    sources += list((ROOT / "tools/original-rules-parity").glob("*.cpp")) + [Path(__file__).resolve(), ROOT / "src/simulation/OriginalEconomyState.h", ROOT / "src/simulation/OriginalInfluenceState.h", ROOT / "src/simulation/OriginalGoodsState.h", ROOT / "src/simulation/OriginalProductionState.h", ROOT / "src/simulation/OriginalFoodState.h", ROOT / "src/simulation/OriginalServicesState.h", ROOT / "src/simulation/OriginalWaterState.h", ROOT / "src/simulation/OriginalHousingState.h", ROOT / "src/simulation/OriginalIndustryState.h", ROOT / "src/simulation/OriginalFinanceState.h"]
    sources.append(ROOT / "src/simulation/OriginalFireProtectionState.h")
    sources += [ROOT / "src/simulation" / name for name in REVIEWED_HOUSING_WORLD_API_GIT_BLOBS]
    return {str(path.relative_to(ROOT)):sha256(path) for path in sorted(sources)}


def validate_housing_world_api(root: Path = ROOT) -> dict[str,str]:
    """Fail closed on any unreviewed World or directly affected DTO header."""
    actual = {}
    for name, expected in REVIEWED_HOUSING_WORLD_API_GIT_BLOBS.items():
        header = root / "src/simulation" / name
        if header.is_symlink() or not header.is_file() or header.stat().st_size > 512*1024:
            raise ValueError(f"World API drift: fourth-module header is missing/unsafe: {name}")
        data = header.read_bytes()
        blob = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
        if blob != expected:
            raise ValueError(f"World API drift: fourth-module adapter needs a reviewed compatibility contract: {name}")
        actual[name] = blob
    return actual


def real_model_path(data: Path) -> Path | None:
    for path in (data,data / "Model"):
        if not path.exists() and not path.is_symlink():
            return None
        if not path.is_dir() or path.is_symlink():
            raise ValueError("present real-data directory is not a regular directory")
    real = data / "Model/EmperorBuildingModels.txt"
    if not real.exists() and not real.is_symlink():
        return None
    if not real.is_file() or real.is_symlink() or real.stat().st_size > 256*1024:
        raise ValueError("present real model is not a bounded regular file")
    return real


def run_comparison(args: argparse.Namespace, output: Path) -> dict[str, Any]:
    # Inspect the supplied entry before canonicalization: resolving a dangling
    # symlink first would incorrectly turn a present invalid source into SKIP.
    fork_provenance = validate_fork(args.fork)
    fork = args.fork.resolve()
    report: dict[str, Any] = {"schema_version":1,"status":"FAIL","fork":fork_provenance,
                              "configuration":args.configuration,"cases":[]}
    report["current"] = {"files":current_source_hashes()}
    library = args.simulation_library or ROOT / CONFIGURATIONS[args.configuration][1] / "libopenemperor-simulation.a"
    libraries = housing_link_libraries(library)
    if any(not path.is_file() for path in libraries):
        raise ValueError("matching prebuilt simulation library/dependencies are missing; build the selected configuration first")
    # OE40 reviewed the prior Rule12 header through additive Rule13/14 APIs,
    # immutable terrain ownership and appended diagnostic fields. No historical
    # numerical vector or fourth-module observation is changed by that review.
    reviewed_world_api = validate_housing_world_api()
    world_blob = reviewed_world_api["World.h"]
    dto = ROOT / "src/simulation/OriginalEconomyState.h"
    dto_blob = hashlib.sha1(b"blob " + str(dto.stat().st_size).encode() + b"\0" + dto.read_bytes()).hexdigest()
    if dto_blob != "4c66306ee98c5408524187e910066d35a72ae857":
        raise ValueError("Original-state API drift: fourth-module adapter needs a reviewed compatibility contract")
    influence_dto = ROOT / "src/simulation/OriginalInfluenceState.h"
    influence_blob = hashlib.sha1(b"blob " + str(influence_dto.stat().st_size).encode() + b"\0" + influence_dto.read_bytes()).hexdigest()
    if influence_blob != "5f93ab4b1fb587f98dcda62b16b0cdc814fb2467":
        raise ValueError("Original-influence API drift: fourth-module adapter needs a reviewed compatibility contract")
    goods_dto = ROOT / "src/simulation/OriginalGoodsState.h"
    goods_blob = hashlib.sha1(b"blob " + str(goods_dto.stat().st_size).encode() + b"\0" + goods_dto.read_bytes()).hexdigest()
    if goods_blob != "2bdcb4b1e9bc0e0932c8cb64bcf9044d62d1d9e3":
        raise ValueError("Original-goods API drift: fourth-module adapter needs a reviewed compatibility contract")
    production_dto = ROOT / "src/simulation/OriginalProductionState.h"
    production_blob = hashlib.sha1(b"blob " + str(production_dto.stat().st_size).encode() + b"\0" + production_dto.read_bytes()).hexdigest()
    if production_blob != "543abfab1f88640bb27cc87107e938fe2a8d8916":
        raise ValueError("Original-production API drift: fourth-module adapter needs a reviewed compatibility contract")
    food_dto = ROOT / "src/simulation/OriginalFoodState.h"
    food_blob = hashlib.sha1(b"blob " + str(food_dto.stat().st_size).encode() + b"\0" + food_dto.read_bytes()).hexdigest()
    if food_blob != "24e0aa32ecf9293e038a6c7f465e644d6d96b1dc":
        raise ValueError("Original-food API drift: fourth-module adapter needs a reviewed compatibility contract")
    services_dto = ROOT / "src/simulation/OriginalServicesState.h"
    services_blob = hashlib.sha1(b"blob " + str(services_dto.stat().st_size).encode() + b"\0" + services_dto.read_bytes()).hexdigest()
    if services_blob != "4f3070f21c7ce4bf5f58226ca910100de1542cfe":
        raise ValueError("Original-services API drift: fourth-module adapter needs a reviewed compatibility contract")
    fire_header = ROOT / "src/simulation/OriginalFireProtectionState.h"
    fire_bytes = fire_header.read_bytes()
    fire_blob = hashlib.sha1(b"blob " + str(len(fire_bytes)).encode() + b"\0" + fire_bytes).hexdigest()
    if fire_blob != "a5d45a75fffbebced30b02e4f5a4e18e889049c6":
        raise ValueError("Fire protection ABI drift requires reviewed compatibility contract")
    added_header_blobs = {"OriginalFireProtectionState.h": fire_blob}
    added_header = ROOT / "src/simulation/OriginalWaterState.h"
    added_bytes = added_header.read_bytes()
    added_blob = hashlib.sha1(b"blob " + str(len(added_bytes)).encode() + b"\0" + added_bytes).hexdigest()
    if added_blob != "3eed36af08a859e318198bbee150966137cca0dc":
        raise ValueError("Water/Housing ABI drift requires reviewed compatibility contract")
    added_header_blobs["OriginalWaterState.h"] = added_blob
    added_header = ROOT / "src/simulation/OriginalHousingState.h"
    added_bytes = added_header.read_bytes()
    added_blob = hashlib.sha1(b"blob " + str(len(added_bytes)).encode() + b"\0" + added_bytes).hexdigest()
    if added_blob != "4265e759e568fb39cb69ad3958a421468cd4a8f2":
        raise ValueError("Water/Housing ABI drift requires reviewed compatibility contract")
    added_header_blobs["OriginalHousingState.h"] = added_blob
    industry_dto = ROOT / "src/simulation/OriginalIndustryState.h"
    industry_bytes = industry_dto.read_bytes()
    industry_blob = hashlib.sha1(b"blob " + str(len(industry_bytes)).encode() + b"\0" + industry_bytes).hexdigest()
    if industry_blob != "3420978efeaf0ef3ba97c3caeba06890b77ced34":
        raise ValueError("Original-industry ABI drift requires reviewed compatibility contract")
    finance_dto = ROOT / "src/simulation/OriginalFinanceState.h"
    finance_bytes = finance_dto.read_bytes()
    finance_blob = hashlib.sha1(b"blob " + str(len(finance_bytes)).encode() + b"\0" + finance_bytes).hexdigest()
    if finance_blob != "4c42d35342a89c113b44965562635421efccecd1":
        raise ValueError("Original-finance ABI drift requires reviewed compatibility contract")
    model_header_blobs = {}
    for name, expected in (("ModelSchema.h", "ea9c84c240d47efe0479b740ec298009c2d12185"),
                           ("OriginalRulesModel.h", "63e6f7f167dce0d7e1480835759f01bd49d3a4a5")):
        header = ROOT / "src/simulation/original" / name
        actual = hashlib.sha1(b"blob " + str(header.stat().st_size).encode() + b"\0" + header.read_bytes()).hexdigest()
        if actual != expected:
            raise ValueError("Production-model API drift: fourth-module adapter needs a reviewed compatibility contract")
        model_header_blobs[name] = actual
    library_hashes = {str(path.resolve()):sha256(path) for path in libraries}
    report["build_provenance"] = {"simulation_library":str(library.resolve()),"sha256":sha256(library),"housing_link_libraries":library_hashes,"world_header_git_blob":world_blob,"original_state_git_blob":dto_blob,"original_influence_state_git_blob":influence_blob,"original_goods_state_git_blob":goods_blob,"original_production_state_git_blob":production_blob}
    report["build_provenance"]["reviewed_housing_world_api_git_blobs"] = reviewed_world_api
    report["build_provenance"]["water_housing_header_git_blobs"] = added_header_blobs
    report["build_provenance"]["production_model_header_git_blobs"] = model_header_blobs
    report["build_provenance"]["original_food_state_git_blob"] = food_blob
    report["build_provenance"]["original_services_state_git_blob"] = services_blob
    report["build_provenance"]["original_industry_state_git_blob"] = industry_blob
    report["build_provenance"]["original_finance_state_git_blob"] = finance_blob
    authored = cases()
    validate_cases(authored)
    binaries = compile_probes(fork,output,args.compiler,args.configuration,library)
    fixtures: dict[str,Path] = {}
    for kind in sorted({case["input"]["fixture"] for case in authored}):
        fixture = output / "fixtures" / kind / "Model" / "EmperorBuildingModels.txt"
        fixture.parent.mkdir(parents=True)
        fixture.write_text(authored_fixture(kind))
        fixtures[kind] = fixture
    observed: dict[str,dict[str,list[int]]] = {case["id"]:{} for case in authored}
    for kind, fixture in fixtures.items():
        group = [case for case in authored if case["input"]["fixture"] == kind]
        commands = [case["input"]["operation"]+" "+" ".join(map(str,case["input"]["values"])) for case in group]
        for side in ("fork","current"):
            values = probe(binaries[side+"-probe"],fixture,commands)
            for case, value in zip(group,values):
                observed[case["id"]][side] = value
    report["cases"] = [compare_observations(case,observed[case["id"]]) for case in authored]
    expected_housing = HOUSING_EXPECTED
    housing = json.loads(checked_run([str(binaries["housing-probe"]),str(fixtures["base"].parents[1])],timeout=60).stdout)
    report["housing_view"] = {"classification":"unsupported","fork_expected":expected_housing,
        "fork_observed":housing,"passed":housing==expected_housing,
        "current":"No World-to-original-house projection: intentionally absent in Phase A.",
        "evidence":"docs/original-economy/core-parity.md#originalhousingview",
        "observation_fields":["ClaySource model","Pottery model","Warehouse model","Market model","Farm model","Well model","HealthPost model","empty reports","World population","zero-step peak","zero-step settled","one-step peak","one-step settled","empty World unchanged","real food present","quality option 0 settled","quality option 100 settled","fed World unchanged"]}
    report["real_model"] = {"status":"NOT_RUN","reason":"--data was not requested"}
    if args.data is not None:
        real = real_model_path(args.data)
        if real is None:
            report["real_model"] = {"status":"SKIP","reason":"optional real model is absent"}
        else:
            if sha256(real) != MODEL_SHA256:
                raise ValueError("present real model does not match the reviewed original source pin")
            values = {side:probe(binaries[side+"-probe"],real,["real_digest"])[0] for side in ("fork","current")}
            report["real_model"] = {"status":"PASS" if values["fork"]==values["current"] and values["fork"][0]==1800 else "FAIL",
                                    "sha256":MODEL_SHA256,"scope":"15 rows x 24 fields x 5 difficulties; bounded FNV-1a digest, no numeric table","observed":values}
            if sha256(real) != MODEL_SHA256:
                raise ValueError("read-only original model changed during comparison")
    report["summary"] = {"cases":len(authored),"passed":sum(case["passed"] for case in report["cases"]),
                           "classifications":{kind:sum(case["classification"]==kind for case in authored) for kind in ("parity","corrected_bug","unverified","unsupported")}}
    report["limitations"] = ["Static EXE evidence is independent of fork comparison; no original executable was run.",
        "World tick/month scheduling and step_house composition are unsupported; no inferred clock contract.",
        "Original wall sectors/coordinate-unit mismatch remain unverified; no original wall-parity claim.",
        "Food binary32 nearest-even modeling does not establish the original global FPU environment.",
        "No production World, navigation, renderer, input or save integration is introduced."]
    report["status"] = "PASS" if all(case["passed"] for case in report["cases"]) and report["housing_view"]["passed"] and report["real_model"]["status"] not in ("FAIL",) else "FAIL"
    if report["status"]=="PASS" and report["real_model"]["status"]=="SKIP":
        report["status"]="SKIP"
    if validate_fork(fork) != report["fork"] or current_source_hashes() != report["current"]["files"] or {str(path.resolve()):sha256(path) for path in libraries} != library_hashes:
        raise ValueError("comparison source or library changed during execution")
    return report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fork", type=Path, default=ROOT / ".local/original-economy/oe00/fork")
    parser.add_argument("--data", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compiler", default="c++")
    parser.add_argument("--configuration", choices=tuple(CONFIGURATIONS),default="debug")
    parser.add_argument("--simulation-library", type=Path)
    args = parser.parse_args(argv)
    output = None
    try:
        output = fresh_output(args.output,inputs=tuple(path for path in (args.fork,args.data,args.simulation_library) if path is not None))
        report = run_comparison(args,output)
    except SkipComparison as error:
        report = {"schema_version":1,"status":"SKIP","reason":str(error)}
    except (OSError,ValueError,RuntimeError,subprocess.TimeoutExpired) as error:
        report = {"schema_version":1,"status":"FAIL","reason":str(error)}
    if output is not None:
        (output / "report.json").write_text(json.dumps(report,indent=2,sort_keys=True)+"\n")
    print(json.dumps({"status":report["status"],"report":str(output / "report.json") if output else None,"reason":report.get("reason"),"summary":report.get("summary")},sort_keys=True))
    return {"PASS":0,"SKIP":77,"FAIL":1}[report["status"]]


if __name__ == "__main__":
    raise SystemExit(main())
