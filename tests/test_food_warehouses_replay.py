#!/usr/bin/env python3
"""Independently audit paid Rule35 stock, physical hops and separate replay."""
import argparse
import json
import pathlib
import subprocess
import sys
import uuid

from test_food_processing_replay import (arrival, batch, check, first_food_event,
                                         mills, read, sha, source, unique)


def totals(world):
    food = world["processed_food"]
    check(not any(k in world for k in ("hunting_food", "fishing_food", "agriculture")), "parallel Food authority")
    check(len(food["stocks"]) <= 82 and len(food["trips"]) <= 58 and len(world["buildings"]) <= 82,
          "unchanged physical limits")
    channels = len(world["couriers"]) + len(world["fishing"]["quays"]) + len(world["hunting"]["tents"]) + 3 * len(world["hunting"]["habitats"])
    check(channels <= 58, "Warehouse raised movement budgets")
    origin, held, carried, used, departed, received, returned = ([0] * 12 for _ in range(7))
    fields = {r["field_id"]: r for r in food["fields"]}
    for row in world["physical_food_growers"]["fields"]:
        origin[fields[row["field_id"]]["crop"]] += row["returned"]
    origin[5] = sum(r["unloaded"] for r in world["fishing"]["quays"])
    origin[6] = sum(r["unloaded"] for r in world["hunting"]["tents"])
    processing = world["food_processing"]
    mills(processing["mills"])
    for row in processing["mills"]:
        for g, n in enumerate(row["completed_by_grade"]):
            origin[7 + g] += (g + 1) * n
    accounts = {r["building_id"]: r for r in food["stocks"]}
    check(len(accounts) == len(food["stocks"]), "duplicate Food owner")
    for row in food["stocks"]:
        for key, target in (("amounts", held), ("consumed", used), ("departed", departed), ("received", received), ("returned", returned)):
            check(len(row[key]) == 12 and all(isinstance(n, int) and n >= 0 for n in row[key]), "typed finite12-column Food")
            for g, n in enumerate(row[key]):
                target[g] += n
    for row in food["trips"]:
        for key in ("loaded", "delivered", "returned"):
            check(len(row[key]) == 12, "typed physical cargo")
        for g in range(12):
            n = row["loaded"][g] - row["delivered"][g] - row["returned"][g]
            check(0 <= n <= 4, "Cart cargo lost, duplicated or retyped")
            carried[g] += n
    for g in range(12):
        check(origin[g] == held[g] + carried[g] + used[g], f"physical origin conservation slot{g}")
        check(departed[g] == received[g] + returned[g] + carried[g], f"arrival/return conservation slot{g}")
    check(not origin[11] and not held[11] and not used[11] and not carried[11], "unavailable grade5")
    state = world["food_warehouses"]
    check(set(state) == {"revision", "warehouses", "markets"} and state["revision"] == 1 and len(state["warehouses"]) <= 2,
          "exact bounded Warehouse contract")
    couriers = {c["id"]: c for c in world["couriers"]}
    warehouse_ids = {h["warehouse_id"] for h in state["warehouses"]}
    for row in state["warehouses"]:
        check(set(row) == {"warehouse_id", "first_received", "first_delivered"} and len(row["first_received"]) == len(row["first_delivered"]) == 12,
              "fixed-size Warehouse provenance")
        stock = accounts[row["warehouse_id"]]
        incoming, back = [0] * 12, [0] * 12
        for trip in food["trips"]:
            for g in range(12):
                n = trip["loaded"][g] - trip["delivered"][g] - trip["returned"][g]
                if trip["target"] == row["warehouse_id"] and trip["phase"] == 1:
                    incoming[g] += n
                if trip["source"] == row["warehouse_id"]:
                    back[g] += n
        check(sum(stock["amounts"]) + sum(incoming) + sum(back) <= 32, "shared32-slot Food capacity")
        check(not stock["quality_witness"] and not stock["missed_demands"] and not any(stock["consumed"]), "Warehouse acquired House authority")
        actors = [c for c in world["couriers"] if c["owner"] == row["warehouse_id"]]
        check(len(actors) == 3 and len([c for c in actors if c["role"] == 34]) == 1, "third counted Warehouse Food channel")
        for g in range(12):
            check(stock["amounts"][g] + incoming[g] + back[g] <= 4, "per-kind4 intake/return quota")
            first = row["first_received"][g]
            check(bool(first) == bool(stock["received"][g]), "missing or invented first Warehouse receipt")
            if first:
                arrival(first)
                actor = couriers[first["transfer"]["courier_id"]]
                check(first["source"] not in warehouse_ids and actor["owner"] == first["source"] and actor["role"] == (13 if g < 7 else 32),
                      "Warehouse received from a foreign source or another Warehouse")
                check(first["amount"] <= stock["received"][g] and first["tick"] <= world["ticks"], "Warehouse first amount/epoch")
            outgoing = row["first_delivered"][g]
            if outgoing:
                check(set(outgoing) == {"transfer", "target", "tick", "amount"} and outgoing["target"] not in warehouse_ids and
                      couriers[outgoing["transfer"]["courier_id"]]["owner"] == row["warehouse_id"] and
                      couriers[outgoing["transfer"]["courier_id"]]["role"] == 34 and 0 < outgoing["amount"] <= stock["departed"][g] and
                      first and first["tick"] < outgoing["tick"] <= world["ticks"], "accepted outbound first proof")
    for row in state["markets"]:
        check(set(row) == {"market_id", "first_received"} and len(row["first_received"]) == 7, "fixed raw Market provenance")
        for g, first in enumerate(row["first_received"]):
            check(bool(first) == bool(accounts[row["market_id"]]["received"][g]), "Market first raw receipt missing")
            if first:
                arrival(first)
    return accounts, origin


def audit(directory, separate):
    report, plan = read(directory / "report.json"), read(directory / "plan.json")
    check(report["status"] == "PASS" and (report["rule"], report["schema"]) == (35, 51), "actual Rule35 acceptance")
    check(report["real_ticks"] >= 12000 and report["minimum_funds"] >= 0, "actual duration/paid budget")
    for key in ("unchanged_supplied_model", "source_bytes_mode_mtime_unchanged", "recovery_exact", "fresh_restore_continuation_exact", "fresh_outlets_only_warehouse_origin"):
        check(report[key] is True, key)
    check(report["snapshot_stock_or_population_injection"] is False and report["separate_process_replay_exact"] is separate,
          "no injected Food / genuinely independent replay")
    check(all(report["source_classes_received"].get(k) is True for k in ("Farm", "Quay", "Tent", "Mill")) and report["maximum_simultaneous_raw_kinds"] >= 3,
          "all four actual producer classes and simultaneous typed storage")
    checkpoint_worlds = []
    for item in report["checkpoints"]:
        path = directory / item["file"]
        doc = read(path)
        check(doc["schema_version"] == 51 and doc["rules"]["version"] == 35 and doc["rules"]["food_warehouses"] == "authored-food-warehouses-v1" and
              doc["rules"]["food_warehouses_revision"] == 1, "exact schema51 identity")
        check(item["exact_roundtrip"] is True and path.read_bytes() == path.with_name(path.name.replace(".save.json", ".roundtrip.json")).read_bytes(), "canonical checkpoint bytes")
        totals(doc["world"])
        checkpoint_worlds.append(doc["world"])
    seed = read(directory / "seed.save.json")["world"]
    check(seed["ticks"] == 0 and not any(n for row in seed["processed_food"]["stocks"] for n in row["amounts"]), "paid empty seed")
    observed_in, observed_out, observed_completed, observed_consumed = {}, {}, {}, {}
    last, lines, maximum = None, 0, 0
    for literal in (directory / report["work_trace"]).open():
        row = json.loads(literal, object_pairs_hook=unique)
        before, after = row["before"], row["after"]
        lines += 1
        check(row["tick"] == lines == after["tick"] and before["tick"] == lines - 1, "continuous actual tick stream")
        if last:
            for key in ("origin", "held", "cargo", "used", "departed", "received", "returned", "house_consumed", "house_received", "house_uses"):
                check(before[key] == last[key], f"non-tick physical accounting mutation {key}")
        for sample in (before, after):
            mills(sample["mills"])
            check(sample["bodies"] <= 82 and sample["channels"] <= 58, "streamed unchanged budgets")
            for g in range(12):
                check(sample["origin"][g] == sample["held"][g] + sample["cargo"][g] + sample["used"][g] and
                      sample["departed"][g] == sample["received"][g] + sample["returned"][g] + sample["cargo"][g], "streamed conservation")
            for wh in sample["warehouses"]:
                check(sum(wh["amounts"]) + wh["incoming"] + wh["return_reserved"] <= 32 and all(0 <= n <= 4 for n in wh["amounts"]), "streamed finite shared/per-kind capacity")
                maximum = max(maximum, sum(n > 0 for n in wh["amounts"][:7]))
        prior_trips = {(t["transfer"]["courier_id"], t["transfer"]["sequence"]): t for t in before["transfers"]}
        for trip in after["transfers"]:
            key = (trip["transfer"]["courier_id"], trip["transfer"]["sequence"])
            old = prior_trips.get(key)
            check(sum(trip["loaded"]) <= 4 and all(n >= 0 for n in trip["loaded"]), "actual Cart load")
            for g, n in enumerate(trip["delivered"]):
                delta = n - (old["delivered"][g] if old else 0)
                check(delta >= 0, "arrival regressed in one actual transaction")
                if delta:
                    value = {"transfer": trip["transfer"], "source": trip["source"], "tick": lines, "amount": delta}
                    observed_in.setdefault((trip["target"], g), value)
                    if trip["source"] == plan["warehouse"]:
                        check(trip["role"] == 34 and trip["target"] in (plan["second_mill"], plan["second_market"], plan["shop"]), "Warehouse invented a direct House/other Warehouse leg")
                        check((g < 7) == (trip["target"] != plan["shop"]), "Warehouse retyped ingredient/meal identity")
                        observed_out.setdefault((trip["source"], g), {"transfer": trip["transfer"], "target": trip["target"], "tick": lines, "amount": delta})
                    if trip["target"] in (plan["second_mill"], plan["second_market"], plan["shop"]):
                        check(trip["source"] == plan["warehouse"], "Fresh outlet borrowed a direct producer origin")
        previous_houses = {h["house_id"]: h for h in before["house_uses"]}
        for house in after["house_uses"]:
            old = previous_houses[house["house_id"]]
            delta = [b - a for a, b in zip(old["consumed"], house["consumed"])]
            check(all(n >= 0 for n in delta) and sum(delta) <= 1, "more than one actual House Food portion per tick")
            if sum(delta):
                check(lines == first_food_event(lines), "Food consumed outside the existing sole event")
                for g, n in enumerate(delta):
                    if n:
                        observed_consumed.setdefault((house["house_id"], g), lines)
        for a, b in zip(before["mills"], after["mills"]):
            work = b["productive_ticks"] - a["productive_ticks"]
            check(work in (0, 1), "Mill created unearned extra work")
            if work:
                check(a["operating"] and not a["burning"] and a["workers"] == a["required"], "Mill work without actual eligibility")
            for g, (previous, current) in enumerate(zip(a["completed_by_grade"], b["completed_by_grade"])):
                if current > previous:
                    check(current == previous + 1 and a["active"] and a["active"]["grade"] == g + 1, "Meal completion without actual prior recipe")
                    recipe = dict(a["active"], work=a["active"]["work"] + 1)
                    batch(recipe, True)
                    observed_completed.setdefault((b["mill_id"], g), {"batch": recipe, "completed_tick": lines})
        last = after
    check(lines == report["real_ticks"] and last == report["final"] and maximum == report["maximum_simultaneous_raw_kinds"] >= 3, "terminal trace/report or typed storage mismatch")
    def seen(values, key, tick, epoch="tick"):
        first = values.get(key)
        return first if first and first[epoch] <= tick else None
    for world in checkpoint_worlds:
        now = world["ticks"]
        for wh in world["food_warehouses"]["warehouses"]:
            for g, first in enumerate(wh["first_received"]):
                check(first == seen(observed_in, (wh["warehouse_id"], g), now), "Warehouse first receipt differs from first observed actual arrival")
            for g, first in enumerate(wh["first_delivered"]):
                check(first == seen(observed_out, (wh["warehouse_id"], g), now), "Warehouse first delivery differs from first observed accepted outward arrival")
        for market in world["food_warehouses"]["markets"]:
            for g, first in enumerate(market["first_received"]):
                check(first == seen(observed_in, (market["market_id"], g), now), "Market first origin differs from physical receipt")
        for mill in world["food_processing"]["mills"]:
            for g, first in enumerate(mill["first_ingredients"]):
                check(first == seen(observed_in, (mill["mill_id"], g), now), "Mill ingredient origin differs from physical receipt")
            for g, first in enumerate(mill["first_completions"]):
                check(first == seen(observed_completed, (mill["mill_id"], g), now, "completed_tick"), "First recipe completion differs from actual staffed work")
        for shop in world["food_processing"]["shops"]:
            for g, first in enumerate(shop["first_meals"]):
                check(first == seen(observed_in, (shop["shop_id"], 7 + g), now), "Shop first meal differs from real Warehouse delivery")
        for house in world["food_processing"]["houses"]:
            for g, first in enumerate(house["first_by_grade"]):
                check(first == seen(observed_in, (house["house_id"], 7 + g), now), "House grade provenance differs from real Shop arrival")
    raw, meal = report["raw_consumption_witness"], report["meal_consumption_witness"]
    check(raw and observed_consumed[(raw["house_id"], raw["slot"])] <= raw["tick"] and raw["market_id"] == plan["second_market"], "actual Warehouse/Market raw consumption")
    check(meal and meal["grade"] == 3 and observed_consumed[(meal["house_id"], 9)] == meal["tick"], "actual stored grade3 meal consumption")
    required = {"seed", "warehouse-three-kinds", "warehouse-raw-loaded", "warehouse-meals-held", "warehouse-market-loaded", "route-cut-loaded", "route-repaired", "warehouse-returned", "warehouse-house-raw-consumed", "warehouse-house-meal-consumed", "final"}
    check(required <= {c["label"] for c in report["checkpoints"]}, "missing real Warehouse transition checkpoints")
    events = report["actual_route_events"]
    check(len(events) == 3 and [e["type"] for e in events] == ["paid-road-cut", "paid-road-repair", "actual-warehouse-return"] and
          events[1]["tick"] - events[0]["tick"] == 31 and events[0]["source_target_disconnected"] is True and
          events[0]["actual_cargo"] == sum(events[0]["loaded"]) > 0 and events[2]["returned"] == events[0]["loaded"], "actual cut/repair and exact physical return")
    for cell in events[0]["cells"]:
        check(any(c["tick"] == events[1]["tick"] and c["command"]["cell"] == cell and c["funds_after"] < c["funds_before"] for c in read(directory / "commands.json")), "road repair did not pay construction")
    return {"checkpoints": len(checkpoint_worlds), "streamed_ticks": lines, "final_sha256": sha(directory / "final.save.json"), "trace_sha256": sha(directory / "work.jsonl")}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True, type=pathlib.Path)
    parser.add_argument("--data", required=True, type=pathlib.Path)
    args = parser.parse_args()
    if not (args.data / "Model/EmperorBuildingModels.txt").is_file():
        print("SKIP: supplied Original model/data unavailable")
        return 77
    repo = pathlib.Path(__file__).resolve().parents[1]
    root = repo / ".local/agri-mac10" / ("ctest-" + uuid.uuid4().hex)
    protected = [args.data / "Model/EmperorBuildingModels.txt", args.data / "Cities/Chengdu.map"]
    before = {path: source(path) for path in protected}
    common = [str(args.executable.resolve()), "--data", str(args.data.resolve()), "--ticks", "24000"]
    creator, replay = root / "creator", root / "replay"
    for out, extra in ((creator, []), (replay, ["--replay", str(creator)])):
        result = subprocess.run(common + ["--output", str(out)] + extra, cwd=repo, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=600)
        check(result.returncode == 0, result.stdout)
    proof = {"creator": audit(creator, False), "replay": audit(replay, True)}
    check((creator / "final.save.json").read_bytes() == (replay / "final.save.json").read_bytes(), "independent process canonical mismatch")
    check((creator / "work.jsonl").read_bytes() == (replay / "work.jsonl").read_bytes(), "independent process tick trace mismatch")
    check(before == {path: source(path) for path in protected}, "Original bytes/mode/mtime changed")
    (root / "independent-audit.json").write_text(json.dumps(proof, indent=2) + "\n")
    print("Food Warehouses actual four-source storage, independent redistribution, physical return and House consumption replay PASS", root)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (AssertionError, OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        print("Food Warehouses replay FAIL:", error, file=sys.stderr)
        sys.exit(1)
