#!/usr/bin/env python3
"""Audit the real paid visual-state corpus independently from the C++ renderer."""
import argparse
import json
import math
import pathlib
import shutil
import subprocess
import sys
import uuid

from test_food_processing_replay import check, read, sha, source
from test_food_warehouses_replay import totals

NAMES = ("Wheat", "Millet", "Rice", "Cabbage", "SoyBeanCurd", "Hemp", "Tea", "Lacquer", "Mulberry")
INPUTS = ("Model/EmperorBuildingModels.txt", "Model/FarmConfig.txt", "Cities/Xia.map", "Cities/Chengdu.map",
          "DATA/China_Terrain.sg3", "DATA/China_Terrain.555", "DATA/China_Elevation.sg3", "DATA/China_Elevation.555",
          "DATA/China_General.sg3", "DATA/China_General.555", "DATA/SprMain.sg3", "DATA/SprMain.555",
          "DATA/SprMain2.sg3", "DATA/SprMain2.555", "Emperor.exe")


def crop_rows(world, scene):
    bodies = {b["id"]: b for b in world["buildings"]}
    irrigation = {r["building_id"]: r for r in world["irrigation_productivity"]["work"]}
    seasonal = world["seasonal_agriculture"]
    sw = {r["field_id"]: r for r in seasonal["fields"]}
    work = {r["field_id"]: r for r in world["multi_field_agriculture"]["fields"]}
    returns = {r["field_id"]: r for r in world["physical_food_growers"]["fields"]}
    rows = {}
    for f in world["processed_food"]["fields"]:
        fid = f["field_id"]
        base, water, cycle = work[fid]["productive_ticks"], irrigation[fid], sw[fid]
        progress = base + water["bonus_work"] - cycle["wasted_work"] - 80 * f["harvested"]
        check(0 <= progress <= 80, "independent Food earned-work equation")
        check(cycle["harvest_work"] == 5 * f["harvested"] + cycle["wasted_harvest_work"] + cycle["harvest_progress"], "five actual harvest-work equation")
        rows[fid] = [fid, f["crop"], f["farm_id"], progress, base, f["harvested"], returns[fid]["returned"], cycle["harvest_progress"], cycle["ready_tick"]]
    for f in seasonal["hemp_farms"]:
        origin = next(r for r in world["original_production"]["hemp_farms"] if r["farm_id"] == f["farm_id"])
        fid = origin["field_id"]
        progress = bodies[f["farm_id"]]["progress"]
        check(f["productive_ticks"] + irrigation[fid]["bonus_work"] == 80 * f["picked"] + f["wasted_work"] + progress, "independent Hemp earned-work equation")
        rows[fid] = [fid, 5, f["farm_id"], progress, f["productive_ticks"], f["picked"], origin["harvested"], 0, f["ready_tick"]]
    for p in world["plantations"]["plants"]:
        fid = p["plant_id"]
        index = 6 + scene["plants"].index(fid)
        cycle = next(r for r in seasonal["plants"] if r["plant_id"] == fid)
        check(p["productive_ticks"] + irrigation[fid]["bonus_work"] == 80 * p["harvested"] + cycle["wasted_work"] + p["growth"], "independent material earned-work equation")
        check(p["grown"] == p["harvested"] + cycle["expired_ready"] + int(cycle["ready_tick"] is not None), "material readiness conservation")
        rows[fid] = [fid, index, p["source_id"], p["growth"], p["productive_ticks"], p["harvested"], p["returned"], 0, cycle["ready_tick"]]
    for fid, water in irrigation.items():
        check(water["wet_work_ticks"] // 4 == water["bonus_work"] + water["clipped_bonus"], "quarter bonus/carry accounting")
        check(fid in rows, "water work belongs to a paid crop")
    check({r[1] for r in rows.values()} == set(range(9)), "paid city does not retain all nine crop kinds")
    return rows


def audit_checkpoint(directory, item, scene):
    path = directory / item["file"]
    document = read(path)
    check(document["schema_version"] == 52, "visual task changed save generation")
    world = document["world"]
    check(world["ticks"] == item["tick"] == item["actual"]["tick"], "checkpoint borrowed another epoch")
    check(path.read_bytes() == (directory / (item["label"] + ".roundtrip.json")).read_bytes(), "checkpoint is not canonical")
    check(not world["agriculture_reconfiguration"]["entries"], "visual corpus replayed demolition and lost crop identities")
    totals(world)
    rows = crop_rows(world, scene)
    check([r[:9] for r in item["actual"]["crops"]] == list(rows.values()), "reported work/stage differs from independently serialized counters")
    label = item["label"]
    if label.startswith("crop-"):
        fid = item["id"]
        row = rows[fid]
        kind = NAMES[row[1]]
        check(label.startswith("crop-" + kind + "-"), "crop name drift")
        phase = label[len(kind) + 6:]
        check(NAMES[row[1]] == kind and row[0] == fid and item["domain"] == "Building", "crop identity/role mismatch")
        p, harvested, returned, ready = row[3], row[5], row[6], row[8]
        predicates = {"initial": p == 0 and harvested == 0, "growing": 0 < p < 80,
                      "ready": p == 80 and ready == world["ticks"], "picked": p == 0 and harvested > 0,
                      "returned": returned > 0, "regrowing": 0 < p < 80 and harvested > 0, "harvesting": p == 80 and row[7] > 0}
        if phase.startswith("growth-band-"):
            index = row[1]
            percent = p * 125 // 100
            expected = 6 * p // 80 if index < 6 else min(3, 3 * percent // 32) if index == 6 else min(5, (2 if index == 8 else 1) * percent // 20)
            # Canonical Lacquer offset5 is selected only at actual ready80.
            predicates[phase] = (p < 80 if index < 6 else p <= 80) and int(phase.rsplit("-", 1)[1]) == expected
        check(predicates.get(phase, False), "invented crop visual phase")
    if item["domain"] == "Boat":
        boat = next(b for b in world["fishing"]["boats"] if b["id"] == item["id"])
        check(label == "boat-" + str(boat["phase"]) or label == "boat-moving" and boat["edge_progress"] > 0, "boat phase is not saved physical state")
        if boat["phase"] in (2, 3):
            check(boat["cargo"] > 0, "returning/unloading Boat lacks earned Fish")
    if item["domain"] == "Hunter":
        hunter = next(h for h in world["hunting"]["hunters"] if h["id"] == item["id"])
        check(label == "hunter-" + str(hunter["phase"]) or label == "hunter-kill" and hunter["killed_tick"] == world["ticks"] or label == "hunter-moving" and hunter["edge_progress"] > 0, "hunter phase/kill was not observed")
        if hunter["phase"] in (2, 3):
            check(hunter["cargo"] == 1 and hunter["killed_tick"] is not None, "Hunter return lacks a real kill")
    if item["domain"] == "Animal":
        animal = next(a for a in world["hunting"]["animals"] if a["id"] == item["id"])
        check(label == f'animal-{animal["species"]}-' + ("moving" if animal["edge_progress"] else "standing"), "animal pose was invented from elapsed time")
    if label == "mill-processing":
        mill = next(m for m in world["food_processing"]["mills"] if m["mill_id"] == item["id"])
        check(mill["active"] and 0 < mill["active"]["work"] < 20 * mill["active"]["grade"], "processor without committed paid work")
    if item["domain"] == "Courier":
        actor = next(c for c in world["couriers"] if c["id"] == item["id"])
        if label.startswith("direction-"):
            check(actor["edge_progress"] > 0 and actor["path_vertex"] + 1 < len(actor["path"]), "direction pose has no actual begun edge")
            a, b = actor["path"][actor["path_vertex"]:actor["path_vertex"] + 2]
            check(label == f"direction-{b[0]-a[0]}-{b[1]-a[1]}", "direction differs from saved physical edge")
        elif label.startswith("food-grower-"):
            farm = next(f for f in world["physical_food_growers"]["farms"] if f["courier_id"] == actor["id"])
            trip = farm["latest_trip"]
            check(trip and trip["returned_tick"] is None, "Food Grower is an idle channel")
            if label.endswith("loaded"):
                check(trip["harvested_tick"] is not None and actor["edge_progress"] > 0, "loaded Grower lacks actual harvest/movement")
            else:
                check(label == "food-grower-" + str(trip["phase"]), "Food Grower pose differs from actual phase")
        elif label == "hemp-grower-loaded":
            trip = next(f for f in world["seasonal_agriculture"]["hemp_farms"] if f["courier_id"] == actor["id"])["latest_trip"]
            check(trip["picked_tick"] is not None and trip["returned_tick"] is None and actor["edge_progress"] > 0, "Hemp cargo is not on its physical worker")
        elif label == "plantation-grower-loaded":
            trip = next(s for s in world["plantations"]["sources"] if s["courier_id"] == actor["id"])["latest_trip"]
            check(trip["picked_tick"] is not None and trip["returned_tick"] is None and actor["edge_progress"] > 0, "material cargo is not on its physical worker")
        else:
            trip = next(t for t in world["processed_food"]["trips"] if t["transfer"]["courier_id"] == actor["id"])
            cargo = sum(a - b - c for a, b, c in zip(trip["loaded"], trip["delivered"], trip["returned"]))
            check(cargo > 0 and actor["edge_progress"] > 0, "Food transport screenshot lacks actual moving cargo")
            if label == "warehouse-outbound":
                check(actor["role"] == 34, "Warehouse Food used a foreign channel")
            if label == "shop-meals-loaded":
                check(actor["role"] == 33 and sum(trip["loaded"][7:]) > 0, "Shop carrier has no real Dinners")
    if label == "pump-fire":
        check(item["actual"]["pump"]["burning"], "Pump fire was not observed")
    if label == "pump-extinguished":
        arrivals = world["original_production"]["fire_protection"]["arrivals"]
        check(any(r[1] and r[2][2] == scene["pump"] and r[0] == world["ticks"] for r in arrivals), "recovery has no actual Inspector extinguish receipt")
    if label in ("water-wet", "pump-recovered-wet"):
        check(item["actual"]["pump"]["supplying"] and item["actual"]["pump"]["phase_tick"] is not None, "wet overlay lacks actual published supply")
    return world


def required_labels():
    result = {f"crop-{kind}-{phase}" for kind in NAMES for phase in ("initial", "growing", "ready", "picked", "returned", "regrowing")}
    result |= {f"crop-{kind}-growth-band-{band}" for index, kind in enumerate(NAMES) for band in range(4 if index == 6 else 6)}
    result |= {f"crop-{kind}-harvesting" for kind in NAMES[:5]}
    result |= {"direction-0-1", "direction-0--1", "direction-1-0", "direction--1-0", "boat-moving", "hunter-moving"}
    result |= {f"food-grower-{p}" for p in range(1, 5)}
    result |= {f"{kind}-{p}" for kind in ("boat", "hunter") for p in range(4)}
    result |= {f"animal-{kind}-{state}" for kind in (75, 76, 77) for state in ("standing", "moving")}
    result |= {"food-grower-loaded", "hemp-grower-loaded", "plantation-grower-loaded", "hunter-kill", "mill-processing",
               "food-courier-loaded", "warehouse-inbound", "warehouse-outbound", "warehouse-stock", "shop-meals-loaded",
               "water-dry", "water-wet", "pump-fire", "pump-extinguished", "pump-recovered-wet"}
    return result


def audit(directory, separate):
    report, plan = read(directory / "report.json"), read(directory / "plan.json")
    check(report["status"] == "PASS" and (report["rule"], report["schema"]) == (36, 52) and report["real_ticks"] >= 12000, "actual unchanged Rule36 visual corpus")
    check(report["minimum_funds"] >= 0, "paid city depleted its real funds")
    check(not report["snapshot_stock_or_population_injection"] and report["fresh_restore_continuation_exact"] and report["source_bytes_mode_mtime_unchanged"] and report["separate_process_replay_exact"] is separate, "earned evidence/continuation boundary")
    check(report["native_metal"] == report["human_acceptance"] == "NOT_RUN", "backend claimed native or human evidence")
    scene = plan["scene"]
    check(scene["second_farm"] != scene["farm"] and len(scene["sources"]) == len(scene["plants"]) == 3, "city omitted multiple physical Growers/material chains")
    checkpoints = {r["label"]: r for r in report["checkpoints"]}
    check(len(checkpoints) == len(report["checkpoints"]) and required_labels() <= set(checkpoints), "visual milestone coverage was narrowed")
    worlds = {label: audit_checkpoint(directory, item, scene) for label, item in checkpoints.items()}
    initial = worlds["paid-city"]
    expected_sources = {scene[k] for k in ("farm", "second_farm", "market", "mill", "shop", "warehouse", "hemp", "pump") if k != "market"} | set(scene["sources"])
    expected_sources |= {r["quay_id"] for r in initial["fishing"]["quays"]} | {r["tent_id"] for r in initial["hunting"]["tents"]}
    check({f"body-{i}-source" for i in expected_sources} <= set(checkpoints), "a paid source body was omitted from visual capture")
    check(initial["ticks"] == 0 and not sum(h["house"]["occupants"] for h in initial["original_economy"]["houses"]) and not any(sum(s["amounts"]) for s in initial["processed_food"]["stocks"]), "fresh seed imported people/output")
    first, previous, final = None, None, None
    seen = set()
    with (directory / "work.jsonl").open() as stream:
        for number, line in enumerate(stream):
            current = json.loads(line)
            check(current["tick"] == number and current["funds"] >= 0, "trace continuity/paid funds")
            check(len({r[0] for r in current["crops"]}) == len(current["crops"]) and {r[1] for r in current["crops"]} == set(range(9)), "trace crop identity drift")
            if previous:
                old = {r[0]: r for r in previous["crops"]}
                farm_work = {}
                for row in current["crops"]:
                    delta = row[4] - old[row[0]][4]
                    check(0 <= delta <= 1, "render time fabricated productive work")
                    if row[1] < 5:
                        farm_work[row[2]] = farm_work.get(row[2], 0) + delta
                check(all(n <= 1 for n in farm_work.values()), "multiple Fields received one Farm's work tick")
            for item in report["checkpoints"]:
                if item["tick"] == number:
                    check(item["actual"] == current, "checkpoint evidence and independent trace differ")
                    seen.add(item["label"])
            first = current if first is None else first
            previous = final = current
    check(final["tick"] == report["real_ticks"] and final == report["final"] and seen == set(checkpoints), "missing trace/checkpoint epochs")
    check(all(n > 0 for n in final["returned"]), "a crop never physically returned")
    check(worlds["pump-fire"]["ticks"] < worlds["pump-extinguished"]["ticks"] < worlds["pump-recovered-wet"]["ticks"], "water/fire recovery was backdated")
    return {"status": "PASS", "ticks": report["real_ticks"], "checkpoints": len(checkpoints), "coverage": len(report["coverage"]), "all9_actual_returns": final["returned"]}


def audit_hostile_checkpoint_labels(root, creator, executable, data, repo):
    """Reject copied replay path escapes before any simulation/checkpoint write."""
    normal = read(creator / "plan.json")
    checkpoints = [i for i, event in enumerate(normal["events"]) if event["type"] == "checkpoint"]
    check(len(checkpoints) >= 2, "hostile label controls need actual paid replay checkpoints")
    cases = []
    for kind in ("relative", "absolute", "duplicate"):
        folder = root / ("hostile-label-" + kind)
        folder.mkdir()
        incoming, output = folder / "input", folder / "output"
        incoming.mkdir()
        sentinel = folder / "sentinel.save.json"
        shutil.copy2(creator / "final.save.json", sentinel)
        sentinel_before = source(sentinel)
        plan = json.loads(json.dumps(normal))
        if kind == "duplicate":
            plan["events"][checkpoints[1]]["label"] = plan["events"][checkpoints[0]]["label"]
            expected = "Duplicate replay checkpoint label"
        else:
            plan["events"][checkpoints[0]]["label"] = "../sentinel" if kind == "relative" else str(folder / "sentinel")
            expected = "Checkpoint label must be a bounded ASCII leaf identifier"
        (incoming / "plan.json").write_text(json.dumps(plan) + "\n")
        # These immutable replay inputs are never reached after early rejection.
        shutil.copy2(creator / "final.save.json", incoming / "final.save.json")
        (incoming / "work.jsonl").write_text("{}\n")
        guarded = {path: source(path) for path in incoming.iterdir()}
        cmd = [str(executable.resolve()), "--data", str(data.resolve()), "--output", str(output),
               "--replay", str(incoming), "--ticks", "24000"]
        process = subprocess.run(cmd, cwd=repo, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                 text=True, timeout=120)
        (folder / "process.log").write_text(process.stdout)
        check(process.returncode != 0 and expected in process.stdout, "hostile label did not fail at the intended early guard")
        check(source(sentinel) == sentinel_before and not (folder / "sentinel.roundtrip.json").exists(), "replay label escaped the fresh output capability")
        check(guarded == {path: source(path) for path in guarded}, "hostile replay inputs were modified")
        check(not list(output.glob("*.save.json")) and not list(output.glob("*.roundtrip.json")), "replay wrote a partial checkpoint before validating all labels")
        failure = read(output / "report.json")
        check(failure["status"] == "FAIL" and failure["last_completed_tick"] == 0, "unsafe replay simulated before rejecting labels")
        cases.append({"kind": kind, "status": "PASS", "sentinel_bytes_mode_mtime_unchanged": True,
                      "no_checkpoint_writes": True, "last_completed_tick": 0})
    return {"status": "PASS", "cases": cases}


def audit_view_regression(root, creator, executable, data, repo):
    """Real normal-view regression for promoted field IDs and physical actors."""
    labels = ("crop-Millet-initial", "crop-Rice-initial", "crop-Wheat-ready", "crop-Hemp-ready", "food-grower-loaded", "boat-1", "boat-3",
              "hunter-2", "animal-75-moving", "mill-processing")
    by_label = {row["label"]: row for row in read(creator / "report.json")["checkpoints"]}
    views = root / "view-regression"
    views.mkdir()
    results = []
    executable_before = source(executable)
    checkpoints_before = {path: source(path) for mode in ("creator", "replay") for path in (root / mode).glob("*.save.json")}
    expected_checkpoints = sum(len(read(root / mode / "report.json")["checkpoints"]) for mode in ("creator", "replay"))
    check(len(checkpoints_before) == expected_checkpoints, "real view regression lost protected checkpoints")
    for label in labels:
        item = by_label[label]
        input_path, output = creator / item["file"], views / label
        cmd = [str(executable.resolve()), "--data", str(data.resolve()), "--resources", str(repo / "resources"),
               "--input", str(input_path), "--output", str(output), "--domain", item["domain"],
               "--id", str(item["id"]), "--state", item["state"], "--renderer", "software", "--frames", "3"]
        process = subprocess.run(cmd, cwd=repo, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                 text=True, timeout=120)
        (views / (label + ".log")).write_text(process.stdout)
        check(process.returncode == 0, f"actual view regression {label}: {process.stdout}")
        report = read(output / "report.json")
        check(report["status"] == "PASS" and report["renderer"] == "software" and report["rule"] == 36 and report["schema"] == 52,
              "real view changed rule/schema or renderer")
        check(report["private_f5_f9_exact"] and report["step_reload_replay_exact"] and report["cold_f9_pose_exact"], "view save/reload/pose divergence")
        check((output / "saves/manual.save.json").read_bytes() == input_path.read_bytes(), "actual F5 canonical bytes differ from input")
        check([capture["zoom"] for capture in report["captures"]] == [1, 2, 4], "view omitted a zoom")
        for capture in report["captures"]:
            subject, picked = capture["subject"], capture["pixel_pick"]
            check(subject["domain"] == item["domain"] and subject["id"] == item["id"] and subject["submitted"] and subject["art"], "real paid draw identity/provenance absent")
            check(picked["status"] == "PASS" and picked["typed_id"] == picked["actual_id"] == item["id"], "normal pixel pick selected another entity")
            if item["domain"] == "Building":
                check(picked["building_has_no_selected_actor"], "actor owner ID masqueraded as an opaque building hit")
            else:
                check(picked["actual_domain"] == item["domain"], "typed actor domain changed")
            if label == "crop-Hemp-ready":
                check(subject["progress"] == subject["target_progress"] == 80,
                      "Hemp ready art changed the actual80-work readiness display")
                check(subject["sprite_drawn"] and subject["asset"] ==
                      {"archive": "DATA/China_General.sg3", "image_index": 2463},
                      "Hemp ready80 must retain the qualified last Original image bank5")
            if label == "crop-Millet-initial":
                check(subject["key"][3] > 2 ** 32 and subject["id"] < 2 ** 32, "promoted painter key regression witness was omitted")
            counters = capture["purity"]["counters"]
            check(len(counters) == 12 and not any(counters.values()), "view performed forbidden frame work")
            timing = capture["purity"]["timing_ms"]
            check(timing["samples"] == 1 and all(math.isfinite(timing[k]) and timing[k] >= 0 for k in ("total", "wall_total", "mean", "p95", "max")), "missing actual frame measurements")
            check((output / capture["path"]).is_file(), "real view PNG missing")
        check(report["human_acceptance"] == report["native_os_input"] == "NOT_RUN", "software claimed human/native input acceptance")
        results.append({"label": label, "status": "PASS", "report": str(output / "report.json"),
                        "literal_f5_exact": True, "zooms": [1, 2, 4]})
    check(source(executable) == executable_before and checkpoints_before == {path: source(path) for path in checkpoints_before}, "real view mutated executable or paid checkpoint inputs")
    return {"status": "PASS", "cases": results, "promoted_painter_key_distinct_from_building_id": True,
            "protected_checkpoints": len(checkpoints_before)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=pathlib.Path)
    parser.add_argument("--seed-executable", type=pathlib.Path)
    parser.add_argument("--seed-plan", type=pathlib.Path)
    parser.add_argument("--view-executable", type=pathlib.Path)
    parser.add_argument("--data", required=True, type=pathlib.Path)
    args = parser.parse_args()
    if not (args.data / INPUTS[0]).is_file():
        print("SKIP: supplied Original data unavailable")
        return 77
    repo = pathlib.Path(__file__).resolve().parents[1]
    root = repo / ".local/agri-mac12" / ("ctest-" + uuid.uuid4().hex)
    root.mkdir(parents=True)
    protected = {args.data / name: source(args.data / name) for name in INPUTS}
    seed = args.seed_plan
    if seed is None:
        check(args.seed_executable is not None, "explicit paid seed plan or its normal executable required")
        seed_root = repo / ".local/agri-mac10" / ("seed-for-mac12-" + uuid.uuid4().hex)
        cmd = [str(args.seed_executable.resolve()), "--data", str(args.data.resolve()), "--output", str(seed_root), "--ticks", "24000"]
        result = subprocess.run(cmd, cwd=repo, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=600)
        check(result.returncode == 0, result.stdout)
        seed = seed_root / "plan.json"
    seed_guard = source(seed)
    creator, replay = root / "creator", root / "replay"
    for output, extra in ((creator, ["--seed-plan", str(seed.resolve())]), (replay, ["--replay", str(creator)])):
        cmd = [str(args.executable.resolve()), "--data", str(args.data.resolve()), "--output", str(output), "--ticks", "24000"] + extra
        result = subprocess.run(cmd, cwd=repo, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=600)
        check(result.returncode == 0, result.stdout)
    proof = {"creator": audit(creator, False), "replay": audit(replay, True)}
    for name in ("final.save.json", "work.jsonl"):
        check((creator / name).read_bytes() == (replay / name).read_bytes(), "separate process canonical/trace mismatch")
    proof["hostile_checkpoint_labels"] = audit_hostile_checkpoint_labels(root, creator, args.executable, args.data, repo)
    proof["view_regression"] = audit_view_regression(root, creator, args.view_executable.resolve(), args.data, repo) if args.view_executable else {"status": "NOT_RUN"}
    check(protected == {p: source(p) for p in protected} and seed_guard == source(seed), "Original/seed bytes, modes or mtimes changed")
    (root / "independent-audit.json").write_text(json.dumps(proof, indent=2) + "\n")
    print("Agriculture visual all9 earned phases, physical actors and independent replay PASS", root)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (AssertionError, KeyError, ValueError, OSError, StopIteration, subprocess.SubprocessError) as error:
        print("Agriculture visual replay FAIL:", error, file=sys.stderr)
        sys.exit(1)
