#!/usr/bin/env python3
"""Independent paid Rule36 work, retirement, new identities and replay audit."""
import argparse
import json
import pathlib
import subprocess
import sys
import uuid

from test_food_warehouses_replay import totals
from test_food_processing_replay import check, read, sha, source


def keyed(rows, key):
    result = {row[key]: row for row in rows}
    check(len(result) == len(rows), "duplicate typed identity")
    return result


def entry(world, target):
    return keyed(world["agriculture_reconfiguration"]["entries"], "building_id")[target]


def row(world, target, section, collection, key="field_id"):
    return keyed(world[section][collection], key)[target]


def audit(directory, replay=False, irrigated_harvest=False, quality_aware_food_dispatch=False):
    report, plan = read(directory / "report.json"), read(directory / "plan.json")
    check(not (irrigated_harvest and quality_aware_food_dispatch), "Select one explicit Agriculture successor")
    rule, schema = (38, 54) if quality_aware_food_dispatch else (37, 53) if irrigated_harvest else (36, 52)
    check(report["status"] == "PASS" and report["rule"] == rule and report["schema"] == schema,
          "actual successor did not complete")
    if irrigated_harvest or quality_aware_food_dispatch:
        check(plan["rule"] == rule and plan["policy"] == report["policy"] == 3 and plan["schema"] == schema and
              all(value["irrigated_harvest"] == "authored-irrigated-harvest-v1" and
                  value["irrigated_harvest_revision"] == 1 for value in (report, plan)),
              "explicit Rule37 report/plan contract metadata")
    if quality_aware_food_dispatch:
        check(all(value["quality_aware_food_dispatch"] == "authored-quality-aware-food-dispatch-v1" and
                  value["quality_aware_food_dispatch_revision"] == 1 for value in (report, plan)),
              "explicit Rule38 report/plan contract metadata")
    check(report["snapshot_stock_or_population_injection"] is False and
          report["source_bytes_mode_mtime_unchanged"] is True and
          report["fresh_restore_continuation_exact"] is True and report["recovery_exact"] is True and
          report["separate_process_replay_exact"] is replay, "proof tier or input/replay boundaries")
    check(report["native_metal"] == report["human_acceptance"] == "NOT_RUN", "scripted proof claimed native/human acceptance")
    check(plan["format"] == "openemperor-agriculture-reconfiguration-plan-v1" and len(plan["events"]) <= 1024,
          "bounded exact event plan")
    saves, initial_work_seen = {}, {}
    for checkpoint in report["checkpoints"]:
        path = directory / checkpoint["file"]
        doc = read(path)
        world = doc["world"]
        check(doc["schema_version"] == schema and doc["rules"]["version"] == rule and
              doc["rules"]["agriculture_reconfiguration"] == "authored-agriculture-reconfiguration-v1" and
              doc["rules"]["agriculture_reconfiguration_revision"] == 1, "exact Rule36/schema52 markers")
        if irrigated_harvest or quality_aware_food_dispatch:
            check(doc["rules"]["irrigated_harvest"] == "authored-irrigated-harvest-v1" and
                  doc["rules"]["irrigated_harvest_revision"] == 1 and
                  doc["profiles"]["map_permissions"]["version"] == 3,
                  "exact Rule37/schema53 contract, revision and policy")
        if quality_aware_food_dispatch:
            check(doc["rules"]["quality_aware_food_dispatch"] == "authored-quality-aware-food-dispatch-v1" and
                  doc["rules"]["quality_aware_food_dispatch_revision"] == 1, "exact Rule38/schema54 contract")
        check(world["ticks"] == checkpoint["tick"] and checkpoint["exact_roundtrip"] and checkpoint["exact_recovery"],
              "checkpoint clock/Recovery")
        check(path.read_bytes() == path.with_name(path.name.replace(".save.json", ".roundtrip.json")).read_bytes(),
              "canonical exact save bytes")
        totals(world)
        journal = world["agriculture_reconfiguration"]
        check(journal["revision"] == 1 and len(journal["entries"]) <= 8, "bounded lifetime journal")
        bodies = keyed(world["buildings"], "id")
        entries = keyed(journal["entries"], "building_id")
        for target, retirement in entries.items():
            body = bodies[target]
            check(body["placed_tick"] <= retirement["started_tick"] <= world["ticks"] and
                  target < retirement["started_sequence"] <= world["command_sequence"], "paid drain chronology")
            initial = retirement["initial_work"]
            if target in initial_work_seen:
                check(initial == initial_work_seen[target], "immutable initial drain work changed after request")
            else:
                check(retirement["started_tick"] == checkpoint["tick"], "first published request lacks its actual command tick")
                crops = keyed(world["processed_food"]["fields"], "field_id")
                plants_now = keyed(world["plantations"]["plants"], "plant_id")
                if target in crops or target in plants_now:
                    irrigation = keyed(world["irrigation_productivity"]["work"], "building_id")[target]
                    if target in crops:
                        base = keyed(world["multi_field_agriculture"]["fields"], "field_id")[target]["productive_ticks"]
                        waste = keyed(world["seasonal_agriculture"]["fields"], "field_id")[target]["wasted_work"]
                        harvested = crops[target]["harvested"]
                    else:
                        base = plants_now[target]["productive_ticks"]
                        waste = keyed(world["seasonal_agriculture"]["plants"], "plant_id")[target]["wasted_work"]
                        harvested = plants_now[target]["harvested"]
                    expected = {"work": {"base_work": base, "wet_work_ticks": irrigation["wet_work_ticks"],
                                "bonus_work": irrigation["bonus_work"], "clipped_bonus": irrigation["clipped_bonus"],
                                "wasted_work": waste}, "harvested": harvested}
                    check(initial == expected, "initial drain witness differs from actual at-command earned counters")
                else:
                    check(initial is None, "Pump gained fabricated crop authority")
                initial_work_seen[target] = initial
            removed = retirement["removed_tick"]
            check((removed is None) == (retirement["removed_sequence"] is None) and body["placed"] == (removed is None),
                  "tombstone/journal phase mismatch")
            if removed is not None:
                check(retirement["started_tick"] <= removed <= world["ticks"] and
                      retirement["started_sequence"] <= retirement["removed_sequence"] <= world["command_sequence"],
                      "removal preceded drain")
                check(not any(c["owner"] == target or (c["target"] == target and c["phase"] != 0)
                              for c in world["couriers"]), "retired leaf retained an active courier")
                check(not any(body[name] for name in ("input_clay", "output", "pottery_stock", "reserved_incoming",
                          "active_recipe_clay", "progress", "food_stock", "reserved_food_incoming", "population")),
                      "retired leaf retained physical stock/work/residents")
        check(all(b["placed"] or b["id"] in entries for b in world["buildings"]), "unowned unplaced body")
        saves[checkpoint["label"]] = world
    required = {"paid-seed", "idle-field-retired", "idle-field-rebuilt", "installed", "field-partial-draining",
                "field-loaded-return", "field-drained", "field-rebuilt", "plant-loaded-draining", "plant-rebuilt",
                "pump-draining", "pump-ditch-removed", "pump-ditch-rebuilt", "wet-production-recovered",
                "hunter-loaded-refused", "boat-loaded-refused", "food-loaded-refused", "mill-working-refused",
                "warehouse-stocked-refused", "final"}
    check(required <= saves.keys(), "missing real transition checkpoints")
    seed, retired, rebuilt, final = (saves[k] for k in ("paid-seed", "idle-field-retired", "idle-field-rebuilt", "final"))
    check(not seed["ticks"] and not sum(b["population"] for b in seed["buildings"]) and
          not any(sum(s["amounts"]) for s in seed["processed_food"]["stocks"]), "seed imported paid output or population")
    retired_ids = {b["id"] for b in retired["buildings"] if not b["placed"]}
    check(len(retired_ids) == 1, "idle paid removal identity")
    idle = next(iter(retired_ids))
    seed_body = keyed(seed["buildings"], "id")[idle]
    check(seed["treasury"] == retired["treasury"] and seed["next_building_id"] == retired["next_building_id"],
          "removal granted money or reused counter")
    replacement = keyed(rebuilt["buildings"], "id")[retired["next_building_id"]]
    check(replacement["id"] > idle and replacement["cell"] == seed_body["cell"] and
          replacement["kind"] != seed_body["kind"] and rebuilt["treasury"] < retired["treasury"] and
          rebuilt["next_building_id"] == replacement["id"] + 1, "new crop failed paid monotone rebuild")
    partial, loaded, drained = (saves[k] for k in ("field-partial-draining", "field-loaded-return", "field-drained"))
    field = next(e["building_id"] for e in partial["agriculture_reconfiguration"]["entries"] if e["removed_tick"] is None)
    active = [f["latest_trip"] for f in partial["physical_food_growers"]["farms"]
              if f["latest_trip"] and f["latest_trip"]["field_id"] == field and f["latest_trip"]["returned_tick"] is None]
    check(len(active) == 1 and active[0]["work_ticks"] > 0 and active[0]["harvested_tick"] is None,
          "partial drain lacked actual at-field work")
    harvest = row(loaded, field, "processed_food", "fields")["harvested"]
    receipt = row(loaded, field, "physical_food_growers", "fields")["returned"]
    check(harvest == receipt + 1 and any(f["latest_trip"] and f["latest_trip"]["field_id"] == field and
          f["latest_trip"]["harvested_tick"] is not None and f["latest_trip"]["returned_tick"] is None
          for f in loaded["physical_food_growers"]["farms"]), "loaded Grower Food was credited before return")
    check(row(drained, field, "physical_food_growers", "fields")["returned"] == harvest and
          row(drained, field, "seasonal_agriculture", "fields")["ready_tick"] is None and
          row(drained, field, "seasonal_agriculture", "fields")["harvest_progress"] == 0, "retirement preceded actual return")
    for section, collection in (("processed_food", "fields"), ("multi_field_agriculture", "fields"),
                                ("physical_food_growers", "fields"), ("seasonal_agriculture", "fields")):
        check(row(drained, field, section, collection) == row(final, field, section, collection),
              "retired Field history changed during later play")
    check(entry(final, field)["removed_tick"] >= drained["ticks"] and
          keyed(final["buildings"], "id")[field]["placed"] is False, "closed Field did not retain history")
    material = saves["plant-loaded-draining"]
    plants = [r for r in material["plantations"]["plants"] if r["harvested"] == r["returned"] + 1]
    check(len(plants) == 1 and plants[0]["growth"] == 0, "plant drain lacked actual loaded pickup")
    plant = plants[0]["plant_id"]
    final_plant = row(final, plant, "plantations", "plants", "plant_id")
    check(final_plant["growth"] == 0 and final_plant["harvested"] == final_plant["returned"] > 0 and
          entry(final, plant)["removed_tick"] is not None, "plant removal lost picked material")
    check(any(r["plant_id"] > plant and r["source_id"] == final_plant["source_id"] for r in final["plantations"]["plants"]),
          "plant was not genuinely rebuilt under the existing source")
    water_off, water_on = saves["pump-ditch-removed"], saves["pump-ditch-rebuilt"]
    pump_ids = {r["building_id"] for r in water_off["irrigation"]["pumps"] if not keyed(water_off["buildings"], "id")[r["building_id"]]["placed"]}
    check(len(pump_ids) == 1 and water_on["ticks"] - water_off["ticks"] == 31, "actual Pump drainage interval")
    old_pump = next(iter(pump_ids))
    old_body = keyed(water_off["buildings"], "id")[old_pump]
    check(any(r["building_id"] > old_pump and keyed(water_on["buildings"], "id")[r["building_id"]]["cell"] == old_body["cell"]
              for r in water_on["irrigation"]["pumps"]), "Pump did not receive new paid identity")
    check(sum(r["wet_work_ticks"] for r in water_off["irrigation_productivity"]["work"]) ==
          sum(r["wet_work_ticks"] for r in water_on["irrigation_productivity"]["work"]) <
          sum(r["wet_work_ticks"] for r in saves["wet-production-recovered"]["irrigation_productivity"]["work"]),
          "closed Pump granted wet work or paid restoration failed real recovery")
    check(any(r["cargo"] > 0 for r in saves["hunter-loaded-refused"]["hunting"]["hunters"]) and
          any(r["cargo"] > 0 for r in saves["boat-loaded-refused"]["fishing"]["boats"]), "loaded Hunter/Boat refusals were unearned")
    check(any(sum(t["loaded"]) > sum(t["delivered"]) + sum(t["returned"])
              for t in saves["food-loaded-refused"]["processed_food"]["trips"]), "Food owner refusal was not loaded")
    refusals = {}
    for index, event in enumerate(plan["events"]):
        if event["type"] == "checkpoint" and event["label"].endswith("-refused"):
            check(index > 0 and plan["events"][index - 1]["type"] == "refusal", "refusal checkpoint lacks actual refused command")
            refusals[event["label"]] = plan["events"][index - 1]["command"]["owner"]
    check(any(a["cargo"] > 0 and a["tent_id"] == refusals["hunter-loaded-refused"]
              for a in saves["hunter-loaded-refused"]["hunting"]["hunters"]), "Hunter refusal targeted a different owner")
    check(any(a["cargo"] > 0 and a["quay_id"] == refusals["boat-loaded-refused"]
              for a in saves["boat-loaded-refused"]["fishing"]["boats"]), "Boat refusal targeted a different owner")
    check(any(t["source"] == refusals["food-loaded-refused"] and
              sum(t["loaded"]) > sum(t["delivered"]) + sum(t["returned"])
              for t in saves["food-loaded-refused"]["processed_food"]["trips"]), "Food refusal targeted an unloaded/different source")
    check(any(m["mill_id"] == refusals["mill-working-refused"] and m["active"] is not None
              for m in saves["mill-working-refused"]["food_processing"]["mills"]), "working Mill refusal lacked actual debited inputs/work")
    check(any(a["building_id"] == refusals["warehouse-stocked-refused"] and sum(a["amounts"]) > 0
              for a in saves["warehouse-stocked-refused"]["processed_food"]["stocks"]), "Warehouse refusal lacked actual stored output")
    commands = [e for e in plan["events"] if e["type"] == "command"]
    check(any(e["funds_after"] < e["funds_before"] and e["tick"] == water_on["ticks"] for e in commands), "Pump/ditch repair evaded payment")
    check(any(e["type"] == "refusal" for e in plan["events"]), "atomic refusal events omitted")
    lines, previous = 0, None
    with (directory / "work.jsonl").open() as stream:
        for text in stream:
            tick = json.loads(text)
            lines += 1
            before, after = tick["before"], tick["after"]
            check(tick["tick"] == after["tick"] == lines and before["tick"] == lines - 1, "actual trace skipped ticks")
            for sample in (before, after):
                check(sample["bodies"] <= 82 and sample["channels"] <= 58 and sample["funds"] >= 0, "bounds or unaffordable continuation")
                check(all(sample["origin"][g] == sample["held"][g] + sample["carried"][g] + sample["used"][g]
                          for g in range(12)), "streamed Food lost/duplicated")
                for leaf in sample["fields"] + sample["plants"]:
                    if not leaf["placed"]:
                        check(not leaf.get("growth", leaf.get("progress", 0)) and leaf["harvested"] == leaf["returned"],
                              "retired leaf retained work/cargo")
            if previous is not None:
                old_fields = keyed(previous["fields"], "id")
                for current in before["fields"]:
                    old = old_fields.get(current["id"])
                    if old and not old["placed"]:
                        check(current == old, "closed Field mutated between actual ticks")
                old_plants = keyed(previous["plants"], "id")
                for current in before["plants"]:
                    old = old_plants.get(current["id"])
                    if old and not old["placed"]:
                        check(current == old, "closed plant regrew between actual ticks")
            previous = after
    check(lines == report["real_ticks"] >= 12000, "insufficient actual trace")
    return {"checkpoints": len(saves), "streamed_ticks": lines, "final_sha256": sha(directory / "final.save.json"),
            "trace_sha256": sha(directory / "work.jsonl"), "retained_field": field, "retained_plant": plant, "retained_pump": old_pump}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True, type=pathlib.Path)
    parser.add_argument("--seed-executable", type=pathlib.Path)
    parser.add_argument("--seed-plan", type=pathlib.Path)
    parser.add_argument("--data", required=True, type=pathlib.Path)
    generation = parser.add_mutually_exclusive_group()
    generation.add_argument("--irrigated-harvest", action="store_true")
    generation.add_argument("--quality-aware-food-dispatch", action="store_true")
    args = parser.parse_args()
    if not (args.data / "Model/EmperorBuildingModels.txt").is_file():
        print("SKIP: supplied Original data unavailable")
        return 77
    repo = pathlib.Path(__file__).resolve().parents[1]
    token = uuid.uuid4().hex
    root = repo / (".local/agri-mac14" if args.irrigated_harvest or args.quality_aware_food_dispatch else ".local/agri-mac11") / ("ctest-" + token)
    root.mkdir(parents=True)
    protected = [args.data / "Model/EmperorBuildingModels.txt", args.data / "Cities/Chengdu.map"]
    before = {p: source(p) for p in protected}
    seed = args.seed_plan
    if seed is None:
        check(args.seed_executable is not None, "portable acceptance needs explicit --seed-executable or --seed-plan")
        seed_root = repo / ".local/agri-mac10" / ("seed-for-mac11-" + token)
        cmd = [str(args.seed_executable.resolve()), "--data", str(args.data.resolve()), "--output", str(seed_root), "--ticks", "24000"]
        process = subprocess.run(cmd, cwd=repo, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=600)
        check(process.returncode == 0, process.stdout)
        seed = seed_root / "plan.json"
    seed_before = source(seed)
    common = [str(args.executable.resolve()), "--data", str(args.data.resolve()), "--ticks", "24000"]
    if args.quality_aware_food_dispatch:
        common.append("--quality-aware-food-dispatch")
    elif args.irrigated_harvest:
        common.append("--irrigated-harvest")
    creator, replay = root / "creator", root / "replay"
    for output, extra in ((creator, ["--seed-plan", str(seed.resolve())]), (replay, ["--replay", str(creator)])):
        process = subprocess.run(common + ["--output", str(output)] + extra, cwd=repo, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, text=True, timeout=600)
        check(process.returncode == 0, process.stdout)
    proof = {"creator": audit(creator, False, args.irrigated_harvest, args.quality_aware_food_dispatch),
             "replay": audit(replay, True, args.irrigated_harvest, args.quality_aware_food_dispatch)}
    check((creator / "final.save.json").read_bytes() == (replay / "final.save.json").read_bytes(), "separate process final canonical mismatch")
    check((creator / "work.jsonl").read_bytes() == (replay / "work.jsonl").read_bytes(), "separate process actual trace mismatch")
    check(before == {p: source(p) for p in protected} and seed_before == source(seed), "supplied/seed bytes, mode or mtime changed")
    (root / "independent-audit.json").write_text(json.dumps(proof, indent=2) + "\n")
    print("Agriculture reconfiguration actual drain, retained work, paid rebuild and independent replay PASS", root)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (AssertionError, OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        print("Agriculture reconfiguration replay FAIL:", error, file=sys.stderr)
        sys.exit(1)
