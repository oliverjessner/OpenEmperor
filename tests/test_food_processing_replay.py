#!/usr/bin/env python3
"""Independent actual-model recipe/accounting audit plus two separate processes."""
import argparse
import hashlib
import json
import pathlib
import subprocess
import sys
import uuid


def check(ok, reason):
    if not ok:
        raise AssertionError(reason)


def unique(pairs):
    out = {}
    for key, value in pairs:
        check(key not in out, f"duplicate JSON key {key}")
        out[key] = value
    return out


def read(path):
    return json.loads(path.read_text(), object_pairs_hook=unique)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source(path):
    stat = path.stat()
    return sha(path), stat.st_mode, stat.st_mtime_ns


def batch(row, completed=False):
    check(set(row) == {"sequence", "started_tick", "grade", "ingredients", "work"}, "recipe keys")
    grade = row["grade"]
    check(1 <= grade <= 4 and row["sequence"] > 0, "unavailable recipe")
    check(len(row["ingredients"]) == 7 and all(n in (0, 1) for n in row["ingredients"])
          and sum(row["ingredients"]) == grade, "distinct ingredient recipe")
    check(row["work"] == 20 * grade if completed else 1 <= row["work"] < 20 * grade, "recipe real work")


def mills(rows):
    for row in rows:
        active = row["active"]
        completed = row["completed_by_grade"]
        check(len(completed) == 4 and sum(completed) == row["completed"], "recipe completion totals")
        check(row["started"] == row["completed"] + bool(active), "active job count")
        check(row["productive_ticks"] == sum((g + 1) * 20 * n for g, n in enumerate(completed))
              + (active["work"] if active else 0), "staffed work equation")
        if active:
            batch(active)
            check(active["sequence"] == row["started"], "active sequence")
        used = row["ingredients_consumed"]
        check(len(used) == 4, "recipe input grade width")
        for g, values in enumerate(used):
            cycles = completed[g] + bool(active and active["grade"] == g + 1)
            check(len(values) == 7 and sum(values) == (g + 1) * cycles
                  and all(0 <= n <= cycles for n in values), "recipe ingredient history")


def arrival(row):
    check(set(row) == {"transfer", "source", "tick", "amount"}, "first physical arrival keys")
    check(set(row["transfer"]) == {"courier_id", "sequence"} and
          row["transfer"]["courier_id"] > 0 and row["transfer"]["sequence"] > 0 and
          row["source"] > 0 and row["tick"] > 0 and 1 <= row["amount"] <= 4,
          "first physical arrival identity/amount")


def first_food_event(tick):
    return min(offset + max(0, (tick - offset + 815) // 816) * 816 for offset in (40, 397))


def totals(world):
    food = world["processed_food"]
    check(len(food["stocks"]) <= 82 and len(food["trips"]) <= 58, "Food collection bound")
    origin = [0] * 12
    fields = {r["field_id"]: r for r in food["fields"]}
    for row in world["physical_food_growers"]["fields"]:
        origin[fields[row["field_id"]]["crop"]] += row["returned"]
    origin[5] = sum(row["unloaded"] for row in world["fishing"]["quays"])
    origin[6] = sum(row["unloaded"] for row in world["hunting"]["tents"])
    processing = world["food_processing"]
    check(set(processing) == {"revision", "mills", "shops", "houses"} and processing["revision"] == 1,
          "processing exact state")
    mills(processing["mills"])
    for row in processing["mills"]:
        check(set(row) == {"mill_id", "requested_grade", "started", "completed", "productive_ticks",
                           "ingredients_consumed", "completed_by_grade", "first_ingredients", "first_completions",
                           "active", "last_receipt"}, "exact Mill provenance fields")
    input_used = [0] * 7
    for row in processing["mills"]:
        for g, n in enumerate(row["completed_by_grade"]):
            origin[7 + g] += (g + 1) * n
        for grade in row["ingredients_consumed"]:
            input_used = [a + b for a, b in zip(input_used, grade)]
        receipt = row["last_receipt"]
        if receipt:
            check(set(receipt) == {"batch", "completed_tick"}, "completion receipt keys")
            batch(receipt["batch"], True)
            check(receipt["batch"]["sequence"] == row["completed"] and
                  receipt["batch"]["started_tick"] < receipt["completed_tick"] <= world["ticks"], "completion epoch")
    held, cargo, used, departed, received, returned = ([0] * 12 for _ in range(6))
    accounts = {r["building_id"]: r for r in food["stocks"]}
    bodies = {r["id"]: r for r in world["buildings"]}
    for row in food["stocks"]:
        for key, target in (("amounts", held), ("consumed", used), ("departed", departed),
                            ("received", received), ("returned", returned)):
            check(len(row[key]) == 12 and all(n >= 0 for n in row[key]), "twelve-column physical Food")
            for g, amount in enumerate(row[key]):
                target[g] += amount
    for row in food["trips"]:
        for key in ("loaded", "delivered", "returned"):
            check(len(row[key]) == 12, "transport Food width")
        for g in range(12):
            amount = row["loaded"][g] - row["delivered"][g] - row["returned"][g]
            check(0 <= amount <= 4, "real finite cargo")
            cargo[g] += amount
    for g in range(12):
        check(origin[g] == held[g] + cargo[g] + used[g], f"origin conservation {g}")
        check(departed[g] == received[g] + returned[g] + cargo[g], f"arrival conservation {g}")
    check(origin[11] == used[11] == 0, "unavailable grade5 output")
    for row in processing["mills"]:
        stock = accounts[row["mill_id"]]
        check(stock["consumed"][:7] == [sum(r[g] for r in row["ingredients_consumed"]) for g in range(7)]
              and not any(stock["consumed"][7:]), "Mill sole input debit")
    couriers = {row["id"]: row for row in world["couriers"]}
    mill_rows = {row["mill_id"]: row for row in processing["mills"]}
    shop_rows = {row["shop_id"]: row for row in processing["shops"]}
    raw_sources = {row["farm_id"] for row in food["farms"]} | {row["quay_id"] for row in world["fishing"]["quays"]} | {row["tent_id"] for row in world["hunting"]["tents"]}
    def physical(value, target, slot, role, sources):
        arrival(value)
        actor = couriers[value["transfer"]["courier_id"]]
        check(value["source"] in sources and actor["owner"] == value["source"] and actor["role"] == role and
              bodies[value["source"]]["placed_tick"] < value["tick"] <= world["ticks"] and
              value["amount"] <= accounts[target]["received"][slot], "actual first arrival source/age/history")
    for row in processing["mills"]:
        inputs, completions = row["first_ingredients"], row["first_completions"]
        check(len(inputs) == 7 and len(completions) == 4, "bounded first Mill history widths")
        for g, value in enumerate(inputs):
            check(bool(value) == bool(accounts[row["mill_id"]]["received"][g]), "first ingredient required exactly once")
            if value:
                physical(value, row["mill_id"], g, 13, raw_sources)
        def causal(recipe):
            for g, used in enumerate(recipe["ingredients"]):
                if used:
                    check(inputs[g] and inputs[g]["tick"] < recipe["started_tick"], "job predates actual ingredient arrival")
        if row["active"]:
            causal(row["active"])
        if row["last_receipt"]:
            causal(row["last_receipt"]["batch"])
        for g, value in enumerate(completions):
            check(bool(value) == bool(row["completed_by_grade"][g]), "first grade completion required exactly once")
            if value:
                check(set(value) == {"batch", "completed_tick"}, "first completion keys")
                batch(value["batch"], True)
                causal(value["batch"])
                check(value["batch"]["grade"] == g + 1 and
                      value["batch"]["started_tick"] + 20 * (g + 1) - 1 <= value["completed_tick"] <= world["ticks"],
                      "first completion actual work/grade epoch")
                if row["last_receipt"] and value["batch"]["sequence"] == row["last_receipt"]["batch"]["sequence"]:
                    check(value == row["last_receipt"], "one actual completion has contradictory receipts")
    for row in processing["shops"]:
        check(set(row) == {"shop_id", "market_id", "first_meals"} and len(row["first_meals"]) == 4, "exact Shop first grade histories")
        for g, value in enumerate(row["first_meals"]):
            check(bool(value) == bool(accounts[row["shop_id"]]["received"][7 + g]), "Shop first grade arrival required exactly once")
            if value:
                physical(value, row["shop_id"], 7 + g, 32, mill_rows)
                completed = mill_rows[value["source"]]["first_completions"][g]
                check(completed and completed["completed_tick"] < value["tick"], "Shop arrival predates first real grade completion")
    for row in processing["shops"]:
        check(row["shop_id"] in bodies and row["market_id"] in bodies and row["shop_id"] != row["market_id"], "actual parent binding")
    for row in processing["houses"]:
        check(set(row) == {"house_id", "first_receipt", "first_by_grade", "first_consumption"} and len(row["first_by_grade"]) == 4,
              "immutable House provenance keys")
        witness = row["first_receipt"]
        check(len(witness["accepted"]) == len(witness["consumed_before"]) == 12 and
              any(witness["accepted"][7:11]) and not witness["accepted"][11] and witness["tick"] <= world["ticks"], "actual first meal receipt")
        first = row["first_by_grade"]
        for g, value in enumerate(first):
            check(bool(value) == bool(accounts[row["house_id"]]["received"][7 + g]), "House first grade arrival required exactly once")
            if value:
                physical(value, row["house_id"], 7 + g, 33, shop_rows)
                shop = shop_rows[value["source"]]["first_meals"][g]
                check(shop and shop["tick"] < value["tick"], "House arrival predates Shop grade stock")
        earliest = min(value["tick"] for value in first if value)
        check(witness["tick"] == earliest and 0 <= witness["previous_quality"] <= 90, "immutable first receipt epoch/quality")
        for g, value in enumerate(first):
            if value and value["tick"] == earliest:
                check(value["transfer"] == witness["transfer"] and witness["accepted"][7 + g] == value["amount"], "first receipt contradicts per-grade arrival")
        current = accounts[row["house_id"]]["quality_witness"]
        if current and current["transfer"] == witness["transfer"]:
            check(current == witness, "same current/first receipt has contradictory immutable quality")
        consumed = row["first_consumption"]
        event = first_food_event(earliest)
        if consumed:
            grade = min(g + 1 for g, value in enumerate(first) if value and value["tick"] <= event)
            check(consumed["tick"] == event <= world["ticks"] and consumed["grade"] == grade and
                  accounts[row["house_id"]]["consumed"][6 + grade] > 0, "exact first meal-first consumption event/grade")
        else:
            check(event > world["ticks"], "due first consumption provenance missing")
    return origin, input_used


def audit(directory, separate):
    report = read(directory / "report.json")
    check(report["status"] == "PASS" and (report["rule"], report["schema"]) == (34, 50), "actual acceptance status")
    check(report["real_ticks"] >= 12000 and report["minimum_funds"] >= 0, "duration/treasury")
    for key in ("unchanged_supplied_model", "source_bytes_mode_mtime_unchanged", "recovery_exact", "fresh_restore_continuation_exact"):
        check(report[key] is True, key)
    check(report["separate_process_replay_exact"] is separate and report["snapshot_stock_or_population_injection"] is False, "independent replay/no injection")
    for key in ("mill_ingredient_arrivals", "actual_recipe_ingredients_consumed"):
        check(len(report[key]) == 7 and all(n > 0 for n in report[key]), "all seven actual ingredient chains")
    for key in ("origin", "shop_received", "house_consumed"):
        check(all(n > 0 for n in report["final"][key][7:11]), "four actual processed grades")
    checkpoints = 0
    checkpoint_worlds = []
    for path in directory.glob("*.save.json"):
        value = read(path)
        check(value["schema_version"] == 50 and value["rules"]["version"] == 34 and
              value["rules"]["food_processing"] == "authored-food-processing-v1" and value["rules"]["food_processing_revision"] == 1, "exact50 identity")
        world = value["world"]
        check("processed_food" in world and "food_processing" in world and not any(key in world for key in ("hunting_food", "fishing_food", "agriculture")), "sole Food ledger")
        totals(world)
        checkpoint_worlds.append(world)
        check(path.read_bytes() == path.with_name(path.name.replace(".save.json", ".roundtrip.json")).read_bytes(), "canonical checkpoint")
        checkpoints += 1
    seed = read(directory / "seed.save.json")["world"]
    check(seed["ticks"] == 0 and not any(row["amounts"][g] for row in seed["processed_food"]["stocks"] for g in range(12)), "zero earned seed")
    last = None
    lines = 0
    observed_arrivals, observed_completions, observed_consumption = {}, {}, {}
    with (directory / report["work_trace"]).open() as stream:
        for line in stream:
            row = json.loads(line, object_pairs_hook=unique)
            before, after = row["before"], row["after"]
            lines += 1
            check(row["tick"] == lines and before["tick"] == lines - 1 and after["tick"] == lines, "tick trace continuity")
            # Player commands may change paused/recipe selection between ticks;
            # earned accounting cannot change before the next actual tick.
            if last:
                for key in ("origin", "held", "cargo", "used", "house_consumed", "house_received", "shop_received", "meal_recipients", "transfers"):
                    check(before[key] == last[key], f"non-tick inventory mutation {key}")
            for sample in (before, after):
                mills(sample["mills"])
                for g in range(12):
                    check(sample["origin"][g] == sample["held"][g] + sample["cargo"][g] + sample["used"][g], "streamed conservation")
            old_trips = {(t["transfer"]["courier_id"], t["transfer"]["sequence"]): t for t in before["transfers"]}
            for trip in after["transfers"]:
                key = (trip["transfer"]["courier_id"], trip["transfer"]["sequence"])
                old = old_trips.get(key)
                for g, delivered in enumerate(trip["delivered"]):
                    delta = delivered - (old["delivered"][g] if old else 0)
                    check(delta >= 0, "physical arrival history regressed within one transfer")
                    if delta:
                        observed_arrivals.setdefault((trip["target"], g),
                            {"transfer": trip["transfer"], "source": trip["source"], "tick": lines, "amount": delta})
            previous_houses = {h["house_id"]: h for h in before["meal_recipients"]}
            for house in after["meal_recipients"]:
                previous = previous_houses[house["house_id"]]
                deltas = [b - a for a, b in zip(previous["consumed"], house["consumed"])]
                check(all(n >= 0 for n in deltas) and sum(deltas) <= 1, "more than one meal in sole House event")
                if any(deltas):
                    check(lines == first_food_event(lines), "meal consumption outside sole Food event")
                    observed_consumption.setdefault(house["house_id"], {"tick": lines, "grade": deltas.index(1) + 1})
            for a, b in zip(before["mills"], after["mills"]):
                for g, (old, current) in enumerate(zip(a["completed_by_grade"], b["completed_by_grade"])):
                    if current > old:
                        check(current == old + 1 and a["active"] and a["active"]["grade"] == g + 1,
                              "completion lacks the real prior active grade")
                        recipe = dict(a["active"], work=a["active"]["work"] + 1)
                        observed_completions.setdefault((b["mill_id"], g), {"batch": recipe, "completed_tick": lines})
                delta = b["productive_ticks"] - a["productive_ticks"]
                check(delta in (0, 1), "more than one actual work tick")
                if delta:
                    check(a["operating"] and not a["burning"] and a["workers"] == a["required"], "unearned processing work")
                check(b["started"] - a["started"] in (0, 1) and b["completed"] - a["completed"] in (0, 1), "recipe generation jumped")
            last = after
    check(lines == report["real_ticks"] and last == report["final"], "terminal trace/report mismatch")
    def observed_before(rows, key, tick, epoch="tick"):
        value = rows.get(key)
        return value if value and value[epoch] <= tick else None
    for world in checkpoint_worlds:
        tick, state = world["ticks"], world["food_processing"]
        for row in state["mills"]:
            for g, first in enumerate(row["first_ingredients"]):
                check(first == observed_before(observed_arrivals, (row["mill_id"], g), tick), "immutable raw receipt differs from first observed physical delivery")
            for g, first in enumerate(row["first_completions"]):
                check(first == observed_before(observed_completions, (row["mill_id"], g), tick, "completed_tick"), "immutable grade completion differs from observed staffed work")
        for row in state["shops"]:
            for g, first in enumerate(row["first_meals"]):
                check(first == observed_before(observed_arrivals, (row["shop_id"], 7 + g), tick), "immutable Shop receipt differs from actual first delivery")
        for row in state["houses"]:
            for g, first in enumerate(row["first_by_grade"]):
                check(first == observed_before(observed_arrivals, (row["house_id"], 7 + g), tick), "immutable House grade receipt differs from actual first delivery")
            check(row["first_consumption"] == observed_before(observed_consumption, row["house_id"], tick), "immutable first consumption differs from first observed real meal debit")
    required = {"processing-active", "processing-paused", "processing-resumed", "processing-completed", "meals-loaded", "route-cut-loaded", "route-repaired", "house-meal-arrived", "house-meal-consumed", "final", "seed"}
    check(required <= {r["label"] for r in report["checkpoints"]}, "missing actual transition checkpoints")
    paused = read(directory / "processing-paused.save.json")["world"]
    resumed = read(directory / "processing-resumed.save.json")["world"]
    check(resumed["ticks"] - paused["ticks"] == 17 and
          paused["food_processing"]["mills"] == resumed["food_processing"]["mills"], "paused committed work changed")
    events = report["actual_route_events"]
    check(len(events) == 2 and events[0]["type"] == "paid-road-cut" and events[1]["type"] == "paid-road-repair"
          and events[1]["tick"] - events[0]["tick"] == 31 and events[0]["source_target_disconnected"] is True,
          "real loaded route cut/paid repair")
    cut = read(directory / "route-cut-loaded.save.json")["world"]
    trip = next(row for row in cut["processed_food"]["trips"] if
                (row["transfer"]["courier_id"], row["transfer"]["sequence"]) ==
                (events[0]["courier"], events[0]["sequence"]))
    check((trip["source"], trip["target"]) == (events[0]["source"], events[0]["target"]) and
          sum(trip["loaded"][g] - trip["delivered"][g] - trip["returned"][g] for g in range(7, 12)) ==
          events[0]["actual_meal_cargo_preserved"] > 0, "route cut lost actual bound meal cargo")
    repair_type = next(row["command"]["type"] for row in read(directory / "commands.json")
                       if row["tick"] == events[1]["tick"] and row["command"]["cell"] == events[0]["cell"])
    check(any(row["tick"] == events[1]["tick"] and row["command"]["type"] == repair_type and
              row["funds_after"] < row["funds_before"] for row in read(directory / "commands.json")),
          "road repair lacked actual paid construction debit")
    return {"checkpoints": checkpoints, "streamed_ticks": lines, "final_sha256": sha(directory / "final.save.json")}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True, type=pathlib.Path)
    parser.add_argument("--data", required=True, type=pathlib.Path)
    args = parser.parse_args()
    if not (args.data / "Model/EmperorBuildingModels.txt").is_file():
        print("SKIP: supplied Original model/data unavailable")
        return 77
    repo = pathlib.Path(__file__).resolve().parents[1]
    root = repo / ".local/agri-mac09" / ("ctest-" + uuid.uuid4().hex)
    files = [args.data / "Model/EmperorBuildingModels.txt", args.data / "Cities/Chengdu.map"]
    before = {path: source(path) for path in files}
    common = [str(args.executable.resolve()), "--data", str(args.data.resolve()), "--ticks", "12000"]
    creator, replay = root / "creator", root / "replay"
    for destination, extra in ((creator, []), (replay, ["--replay", str(creator)])):
        result = subprocess.run(common + ["--output", str(destination)] + extra, cwd=repo, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=600)
        check(result.returncode == 0, result.stdout)
    proof = {"creator": audit(creator, False), "replay": audit(replay, True)}
    check((creator / "final.save.json").read_bytes() == (replay / "final.save.json").read_bytes(), "separate canonical final mismatch")
    check((creator / "work.jsonl").read_bytes() == (replay / "work.jsonl").read_bytes(), "separate trace mismatch")
    check(before == {path: source(path) for path in files}, "source bytes/mode/mtime changed")
    root.mkdir(parents=True, exist_ok=True)
    (root / "independent-audit.json").write_text(json.dumps(proof, indent=2) + "\n")
    print("Food processing actual7 ingredient/4 grade/12k separate replay audit PASS", root)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (AssertionError, OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        print("Food processing replay FAIL:", error, file=sys.stderr)
        sys.exit(1)
