#!/usr/bin/env python3
"""Independently audit paid wet/dry growth and separate-process schema49 replay."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CONTINUATION_TICKS = 24000
WINDOWS = (((2, 6),), ((6, 10),), ((5, 9),), ((7, 11),), ((4, 8),),
           ((3, 8),), ((2, 4), (5, 7), (8, 10)), ((1, 6),), ((3, 5), (6, 8)))


def growing(kind, tick):
    slot = tick // 816 % 12
    return any(start <= slot < end for start, end in WINDOWS[kind])


def expiry(kind, ready_tick):
    year, slot = ready_tick // 9792, ready_tick // 816 % 12
    for start, end in WINDOWS[kind]:
        if start <= slot < end:
            return year * 9792 + (end + 1) * 816
    raise AssertionError("Ready epoch is outside its actual crop window")


def work_opportunities(kind, placed, through):
    def prefix(limit):
        years, remainder = divmod(limit, 9792)
        per_year = sum((end - start) * 816 for start, end in WINDOWS[kind])
        return years * per_year + sum(max(0, min(remainder - start * 816, (end - start) * 816))
                                      for start, end in WINDOWS[kind])
    return max(0, prefix(through + 1) - prefix(placed + 1))


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def fingerprint(path):
    info = path.stat()
    return (info.st_size, info.st_mode, info.st_mtime_ns,
            hashlib.sha256(path.read_bytes()).hexdigest())


def load(path):
    return json.loads(path.read_text())


def snapshot(document):
    require(document["schema_version"] == 49 and document["rules"]["version"] == 33,
            "Checkpoint changed exact Rule33/schema49")
    require(document["rules"]["seasonal_agriculture"] == "authored-seasonal-agriculture-v1" and
            document["rules"]["seasonal_agriculture_revision"] == 1,
            "Checkpoint lacks exact authored seasonal identity")
    require(document["rules"]["irrigation_productivity"] == "authored-irrigation-productivity-v1" and
            document["rules"]["irrigation_productivity_revision"] == 1,
            "Checkpoint lacks exact authored irrigation productivity identity")
    world = document["world"]
    productivity = world["irrigation_productivity"]
    require(set(productivity) == {"revision", "work"} and productivity["revision"] == 1,
            "Productivity save contains a second water cache or unknown authority")
    irrigation = {row["building_id"]: row for row in productivity["work"]}
    identities = [row["building_id"] for row in productivity["work"]]
    require(identities == sorted(set(identities)) and all(identity > 0 for identity in identities),
            "Wet work rows lost canonical positive paid identities")
    bases = {}
    require("agriculture" not in world and "fishing_food" not in world,
            "Seasonal checkpoint duplicated the sole seven-kind Food ledger")
    season = world["seasonal_agriculture"]
    require(season["revision"] == 1 and not {"calendar", "year", "month", "season"}.intersection(season),
            "Seasonal state introduced a second advancing calendar")
    fields = {row["field_id"]: row for row in world["hunting_food"]["fields"]}
    physical = {row["field_id"]: row for row in world["physical_food_growers"]["fields"]}
    work = {row["field_id"]: row for row in world["multi_field_agriculture"]["fields"]}
    rows = {row["field_id"]: row for row in season["fields"]}
    require(fields.keys() == physical.keys() == work.keys() == rows.keys(),
            "Seasonal source, crop, lifetime work and actual receipt identities differ")
    picked = [0] * 7
    returned = [0] * 7
    grower_cargo = [0] * 7
    held = [0] * 7
    cart_cargo = [0] * 7
    consumed = [0] * 7
    departed = [0] * 7
    arrived = [0] * 7
    cart_returned = [0] * 7
    for identity, field in fields.items():
        row = rows[identity]
        bases[identity] = work[identity]["productive_ticks"]
        current = bases[identity] + irrigation[identity]["bonus_work"] - row["wasted_work"] - 80 * field["harvested"]
        require(0 <= current <= 80 and (row["ready_tick"] is not None) == (current == 80),
                "Seasonal earned/wasted/harvested work equation failed")
        require(row["harvest_work"] == 5 * field["harvested"] + row["wasted_harvest_work"] + row["harvest_progress"] and
                0 <= row["harvest_progress"] < 5, "Physical harvest labor equation failed")
        picked[field["crop"]] += field["harvested"]
        returned[field["crop"]] += physical[identity]["returned"]
    for farm in world["physical_food_growers"]["farms"]:
        trip = farm["latest_trip"]
        if trip and trip["harvested_tick"] is not None and trip["returned_tick"] is None:
            grower_cargo[trip["crop"]] += 1
    for crop in range(5):
        require(picked[crop] == returned[crop] + grower_cargo[crop],
                "A field harvest is neither actual Grower cargo nor a physical Farm receipt")
    for quay in world["fishing"]["quays"]:
        returned[5] += quay["unloaded"]
    for tent in world["hunting"]["tents"]:
        returned[6] += tent["unloaded"]
    for stock in world["hunting_food"]["stocks"]:
        require(len(stock["amounts"]) == 7 and all(amount >= 0 for amount in stock["amounts"]),
                "Invalid sole Food stock")
        for crop in range(7):
            held[crop] += stock["amounts"][crop]
            consumed[crop] += stock["consumed"][crop]
            departed[crop] += stock["departed"][crop]
            arrived[crop] += stock["received"][crop]
            cart_returned[crop] += stock["returned"][crop]
    for trip in world["hunting_food"]["trips"]:
        for crop in range(7):
            remaining = trip["loaded"][crop] - trip["delivered"][crop] - trip["returned"][crop]
            require(0 <= remaining <= 4, "Impossible physical Food cart cargo")
            cart_cargo[crop] += remaining
    for crop in range(7):
        require(returned[crop] == held[crop] + cart_cargo[crop] + consumed[crop],
                "Food origin was credited before a genuine source return")
        require(departed[crop] == arrived[crop] + cart_returned[crop] + cart_cargo[crop],
                "Physical Food transfer equation failed")
    plants = {row["plant_id"]: row for row in world["plantations"]["plants"]}
    for row in season["plants"]:
        plant = plants[row["plant_id"]]
        bases[row["plant_id"]] = plant["productive_ticks"]
        require(plant["productive_ticks"] + irrigation[row["plant_id"]]["bonus_work"] ==
                80 * plant["harvested"] + row["wasted_work"] + plant["growth"],
                "Plantation lifetime work equation failed")
        require(plant["grown"] == plant["harvested"] + row["expired_ready"] + (row["ready_tick"] is not None),
                "Plantation picked/ready/expired equation failed")

    buildings = {body["id"]: body for body in world["buildings"]}
    actors = {actor["id"]: actor for actor in world["couriers"]}
    hemp_rows = season["hemp_farms"]
    base_rows = world["original_production"]["hemp_farms"]
    hemp = {row["farm_id"]: row for row in hemp_rows}
    base_hemp = {row["farm_id"]: row for row in base_rows}
    paid_hemp = {body["id"] for body in world["buildings"] if body["kind"] == 14}
    require(len(hemp) == len(hemp_rows) and len(base_hemp) == len(base_rows) and
            hemp.keys() == base_hemp.keys() == paid_hemp,
            "Seasonal Hemp has missing, duplicate or foreign paid Farm identities")
    bound_fields = [row["field_id"] for row in base_rows if row["field_id"] is not None]
    require(len(bound_fields) == len(set(bound_fields)) and set(bound_fields) ==
            {body["id"] for body in world["buildings"] if body["kind"] == 15},
            "Seasonal Hemp lost its uniquely paid Field bindings")
    channels = [row["courier_id"] for row in hemp_rows]
    require(len(channels) == len(set(channels)) and set(channels) ==
            {actor["id"] for actor in world["couriers"] if actor["role"] == 31},
            "Seasonal Hemp has missing, duplicate or orphan movement channels")
    for identity, row in hemp.items():
        source, base = buildings[identity], base_hemp[identity]
        require(source["placed"] and 0 <= source["progress"] <= 80,
                "Hemp work lacks its paid body or bounded actual progress")
        for key in ("productive_ticks", "picked", "wasted_work", "grown", "expired_ready", "spawned", "retired"):
            require(isinstance(row[key], int) and row[key] >= 0, "Negative or non-integer Hemp lifetime history")
        trip = row["latest_trip"]
        live = trip is not None and trip["returned_tick"] is None
        cargo = int(live and trip["picked_tick"] is not None)
        bonus = 0
        if base["field_id"] is not None:
            bases[base["field_id"]] = row["productive_ticks"]
            bonus = irrigation[base["field_id"]]["bonus_work"]
        require(row["productive_ticks"] + bonus == 80 * row["picked"] + row["wasted_work"] + source["progress"],
                "Hemp earned/picked/wasted/current-work equation failed")
        require(row["grown"] == row["picked"] + row["expired_ready"] + (row["ready_tick"] is not None) and
                (row["ready_tick"] is not None) == (source["progress"] == 80),
                "Hemp picked/ready/expired cycle equation failed")
        require(row["picked"] == base["harvested"] + cargo,
                "Hemp pickup became a source receipt before its actual physical return")
        require(row["spawned"] == row["retired"] + int(live),
                "Hemp allocated more than one live Grower or lost actual retirement history")
        actor = actors[row["courier_id"]]
        require(actor["owner"] == identity and actor["role"] == 31 and actor["cargo"] == actor["reserved"] == 0,
                "Hemp movement channel changed owner/role or duplicated typed cargo in legacy mirrors")
        if trip is not None:
            require(trip["field_id"] == base["field_id"], "Hemp worker changed its uniquely paid Field")
        if base["field_id"] is None:
            require(row["productive_ticks"] == row["grown"] == row["picked"] == row["wasted_work"] == 0 and
                    trip is None, "Hemp gained work or cargo without a paid Field")
    require(irrigation.keys() == bases.keys() ==
            {body["id"] for body in world["buildings"] if body["kind"] in {15, 17, 18, 36, 37, 38, 42, 43, 44}},
            "Irrigation work differs from the exact paid Food/Hemp/material field identities")
    for identity, row in irrigation.items():
        require(set(row) == {"building_id", "wet_work_ticks", "bonus_work", "clipped_bonus"},
                "Saved wet work includes unknown state or a derived water cache")
        require(all(type(row[key]) is int and 0 <= row[key] <= 2**64 - 1
                    for key in ("wet_work_ticks", "bonus_work", "clipped_bonus")),
                "Wet work history is negative, fractional or exceeds uint64")
        require(row["wet_work_ticks"] <= bases[identity] and
                row["wet_work_ticks"] // 4 == row["bonus_work"] + row["clipped_bonus"],
                "Actual base work and the exact wet quarter/carry equation disagree")
    for farm in world["physical_food_growers"]["farms"]:
        trip = farm["latest_trip"]
        if trip:
            require(0 <= trip["irrigation_bonus_before"] <= irrigation[trip["field_id"]]["bonus_work"] and
                    trip["irrigation_bonus_before"] <= trip["work_before"] // 4,
                    "A Food trip borrowed future wet bonus work")
    for identity, field in physical.items():
        receipt = field["last_receipt"]
        if receipt:
            bonus = receipt["irrigation_bonus_at_harvest"]
            require(0 <= bonus <= irrigation[identity]["bonus_work"] and
                    bonus <= receipt["work_at_harvest"] // 4 and
                    receipt["work_at_harvest"] + bonus - receipt["wasted_work_at_harvest"] == 80 * field["returned"] and
                    receipt["harvested_tick"] >= receipt["ready_tick"] + 5,
                    "A genuine Food source receipt lost wet provenance or actual harvest labor")
    subjects = {row[0]: row for row in raw_subject_rows(world)}
    pumps = world["irrigation"]["pumps"]
    fills = []
    for pump in pumps:
        placed = buildings[pump["building_id"]]["placed_tick"]
        fills.append(28 if placed < 28 else 28 + ((placed - 28) // 51 + 1) * 51)
    first_supply = min(fills) if fills else None
    ditches = world["irrigation"]["ditches"]
    if first_supply is not None and not world["irrigation"]["ditches_removed_total"] and ditches:
        first_ditch = min(ditch["placed_tick"] for ditch in ditches)
        first_ditch_fill = 28 if first_ditch < 28 else 28 + ((first_ditch - 28) // 51 + 1) * 51
        first_supply = max(first_supply, first_ditch_fill)

    def wet_capacity(identity, through):
        if first_supply is None or not world["irrigation"]["ditches_placed_total"] or first_supply >= through:
            return 0
        return work_opportunities(subjects[identity][1], max(buildings[identity]["placed_tick"], first_supply), through)

    for identity, sample in subjects.items():
        require(sample[3] <= wet_capacity(identity, world["ticks"]),
                "Saved wet work predates any possible paid Pump/ditch refill")
        if sample[7] is not None:
            require(sample[3] <= wet_capacity(identity, sample[7]),
                    "A currently ready crop borrowed wet work from after its ready epoch")
    for farm in world["physical_food_growers"]["farms"]:
        trip = farm["latest_trip"]
        if trip:
            require(trip["irrigation_bonus_before"] <= wet_capacity(trip["field_id"], trip["dispatched_tick"]) // 4,
                    "Food dispatch borrowed bonus work before any possible actual water supply")
            if trip["ready_tick"] is not None:
                require(irrigation[trip["field_id"]]["wet_work_ticks"] <= wet_capacity(trip["field_id"], trip["ready_tick"]),
                        "Food worker borrowed later wet work for an earlier ready epoch")
    for identity, field in physical.items():
        receipt = field["last_receipt"]
        if receipt:
            require(receipt["irrigation_bonus_at_harvest"] <= wet_capacity(identity, receipt["ready_tick"]) // 4,
                    "Food receipt used bonus work unavailable at its actual ready epoch")

    def ready_witness(observation, identity, ordinal):
        witness = observation["irrigation_ready_work"]
        require(type(witness) is dict and set(witness) ==
                {"base_work", "wet_work_ticks", "bonus_work", "clipped_bonus", "wasted_work"},
                "Material trip/receipt lacks its exact immutable five-counter ready witness")
        require(all(type(amount) is int and 0 <= amount <= 2**64 - 1 for amount in witness.values()),
                "Ready witness contains negative, fractional or overflowing history")
        base, wet = witness["base_work"], witness["wet_work_ticks"]
        bonus, clipped, wasted = witness["bonus_work"], witness["clipped_bonus"], witness["wasted_work"]
        current, ready = subjects[identity], observation["ready_tick"]
        require(ordinal > 0 and buildings[identity]["placed_tick"] < ready <= world["ticks"] and
                ready <= observation["dispatched_tick"] and growing(current[1], ready) and
                observation["dispatched_tick"] < expiry(current[1], ready),
                "Material witness lacks a physically dispatched live growing epoch")
        require(wet <= base and wet // 4 == bonus + clipped and base + bonus <= 2**64 - 1 and
                base + bonus - wasted == ordinal * 80 and clipped <= (base + bonus) // 80,
                "Ready witness lost its exact wet quarter, cap or picked-cycle ordinal")
        require(all(a <= b for a, b in zip((base, wet, bonus, clipped, wasted),
                                           (current[2], current[3], current[4], current[5], current[15]))) and
                base <= work_opportunities(current[1], buildings[identity]["placed_tick"], ready) and
                wet <= wet_capacity(identity, ready),
                "Material readiness borrowed future source work or impossible pre-supply wet ticks")
        require(current[2] - base <= work_opportunities(current[1], ready, world["ticks"]) and
                current[3] - wet <= wet_capacity(identity, world["ticks"]) - wet_capacity(identity, ready),
                "Material witness borrowed later passive regrowth for an earlier ready cycle")
        if current[7] == ready:
            require((base, wet, bonus, clipped, wasted) ==
                    (current[2], current[3], current[4], current[5], current[15]),
                    "Material witness contradicts the frozen canonical work of its still-current ready epoch")
        picked = observation.get("picked_tick")
        if picked is not None:
            require(ready <= picked < expiry(current[1], ready), "Physical material pickup used an expired readiness witness")
        return witness

    plant_sources = {row["building_id"]: row for row in world["plantations"]["sources"]}
    for plant in world["plantations"]["plants"]:
        identity = plant["plant_id"]
        source, receipt = plant_sources[plant["source_id"]], plant["last_receipt"]
        trip = source["latest_trip"]
        if trip is not None and trip["plant_id"] == identity:
            ready_witness(trip, identity, plant["harvested"] + int(trip["picked_tick"] is None))
        if receipt is not None:
            ready_witness(receipt, identity, plant["returned"])
            if trip is not None and trip["grower_id"] == receipt["grower_id"]:
                require(trip["irrigation_ready_work"] == receipt["irrigation_ready_work"],
                        "The same plantation Grower retained contradictory readiness witnesses")
    for farm in hemp.values():
        identity = base_hemp[farm["farm_id"]]["field_id"]
        trip, receipt = farm["latest_trip"], farm["last_receipt"]
        if trip is not None:
            ready_witness(trip, identity, farm["picked"] + int(trip["picked_tick"] is None))
        if receipt is not None:
            ready_witness(receipt, identity, base_hemp[farm["farm_id"]]["harvested"])
            if trip is not None and trip["grower_id"] == receipt["grower_id"]:
                require(trip["irrigation_ready_work"] == receipt["irrigation_ready_work"],
                        "The same Hemp Grower retained contradictory readiness witnesses")
    return world


def trace(path, expected_ticks=CONTINUATION_TICKS):
    """Stream actual pre/post tick samples, including supply/fire boundaries."""
    previous = {"dry": None, "wet": None}
    first_ready = {"dry": [None] * 9, "wet": [None] * 9}
    first_return = {"dry": [None] * 9, "wet": [None] * 9}
    total_base = {"dry": [0] * 9, "wet": [0] * 9}
    total_wet = [0] * 9
    increased = [0] * 9
    shared_base = [0] * 9
    return_events = {"dry": [], "wet": []}
    first = {}
    count = 0
    previous_tick = None
    # Water and workforce are derived and may change through a real command
    # between adjacent ticks. Lifetime growth/cargo and stock may not jump.
    persistent = list(range(11)) + [14, 15, 16]
    with path.open() as source:
        for line_number, line in enumerate(source, 1):
            row = json.loads(line)
            tick = row["tick"]
            require(type(tick) is int and tick > 0 and
                    (previous_tick is None or tick == previous_tick + 1),
                    f"Trace skipped or relabelled actual World tick at line {line_number}")
            pairs = {}
            for cohort in ("dry", "wet"):
                before, after = row[f"{cohort}_before"], row[f"{cohort}_after"]
                require(len(before) == len(after) == 9 and
                        [sample[1] for sample in before] == list(range(9)) and
                        [sample[1] for sample in after] == list(range(9)),
                        "Trace lacks the exact nine paid crop types")
                if cohort not in first:
                    first[cohort] = before
                for kind, (a, b) in enumerate(zip(before, after)):
                    require(len(a) == len(b) == 17 and a[:2] == b[:2] and a[0] > 0,
                            "Trace changed a productive paid identity or tuple schema")
                    for sample in (a, b):
                        require(all(type(sample[index]) is int and sample[index] >= 0
                                    for index in list(range(7)) + list(range(8, 11)) + list(range(12, 17))) and
                                type(sample[11]) is bool and
                                (sample[7] is None or type(sample[7]) is int and 0 < sample[7] <= tick),
                                "Trace contains negative, fractional or future work/cargo history")
                        require(sample[3] <= sample[2] and sample[3] // 4 == sample[4] + sample[5] and
                                0 <= sample[6] <= 80 and (sample[7] is not None) == (sample[6] == 80),
                                "Trace wet-quarter, readiness or bounded-cap equation failed")
                        require(sample[2] + sample[4] == 80 * sample[8] + sample[15] + sample[6] and
                                sample[8] == sample[9] + sample[10] and sample[10] <= 1,
                                "Trace credited growth or a source return without actual earned work/cargo")
                        if cohort == "dry":
                            require(sample[3] == sample[4] == sample[5] == 0,
                                    "Disconnected dry control earned wet work or bonus")
                    require(a[7] is None or a[7] < tick,
                            "Pre-tick readiness borrowed this tick's future work")
                    if previous[cohort] is not None:
                        prior = previous[cohort][kind]
                        require(all(prior[index] == a[index] for index in persistent),
                                "Paid counters, cargo or stock jumped between adjacent trace ticks")
                    base, wet = b[2] - a[2], b[3] - a[3]
                    bonus, clipped = b[4] - a[4], b[5] - a[5]
                    picked, returned = b[8] - a[8], b[9] - a[9]
                    require(base in (0, 1) and wet == (base if a[11] else 0) and
                            bonus >= 0 and clipped >= 0 and bonus + clipped == b[3] // 4 - a[3] // 4 and
                            picked in (0, 1) and returned in (0, 1) and b[15] >= a[15] and b[16] >= a[16],
                            "A real tick fabricated labor, doubled a provider, lost carry or used post-tick water")
                    if base:
                        require(a[12] >= a[13] > 0 and growing(kind, tick),
                                "Crop growth lacked actual complete workforce or its sole-calendar growing window")
                    # A ready Tea cycle may expire exactly when its next
                    # interval opens. Expiry precedes this tick's genuine
                    # growth, so apply the observed loss before cap checks.
                    remaining_progress = a[6] - (b[15] - a[15])
                    require(remaining_progress >= 0,
                            "Expiry discarded more than the actual pending cycle")
                    if b[15] > a[15]:
                        require(b[15] - a[15] == a[6] and
                                (tick == expiry(kind, a[7]) if a[7] is not None else not growing(kind, tick)),
                                "Pending work expired outside the actual partial/ready calendar boundary")
                    if clipped:
                        require(base == wet == clipped == 1 and bonus == 0 and remaining_progress == 79,
                                "Bonus clipping did not arise from a real fourth wet tick at progress79")
                    if bonus:
                        require(base == wet == bonus == 1 and clipped == 0 and remaining_progress < 79,
                                "Bonus exceeded one quarter-earned unit or the remaining cap")
                    if kind < 5:
                        require(b[16] - a[16] <= 1 and not (base and b[16] > a[16]),
                                "Bonus fabricated Food harvest labor or doubled one Farm's real work")
                    total_base[cohort][kind] += base
                    if first_ready[cohort][kind] is None and b[7] is not None:
                        first_ready[cohort][kind] = {"tick": tick, "base": b[2]}
                    if returned:
                        if first_return[cohort][kind] is None:
                            first_return[cohort][kind] = tick
                        return_events[cohort].append({"tick": tick, "kind": kind,
                                                     "building_id": b[0], "amount": returned,
                                                     "cumulative": b[9]})
                    if cohort == "wet":
                        total_wet[kind] += wet
                previous[cohort] = after
                pairs[cohort] = (before, after)
            require([sample[0] for sample in pairs["dry"][0]] ==
                    [sample[0] for sample in pairs["wet"][0]],
                    "Matched cohorts changed productive paid identities")
            for kind in range(9):
                a, b = pairs["dry"][0][kind], pairs["dry"][1][kind]
                c, d = pairs["wet"][0][kind], pairs["wet"][1][kind]
                shared = (b[2] > a[2] and d[2] > c[2] and
                          a[12] == a[13] > 0 and c[12] == c[13] > 0)
                shared_base[kind] += shared
                increased[kind] += shared and d[4] > c[4]
            previous_tick = tick
            count += 1
    require(count == expected_ticks and all(total_wet) and all(increased) and all(shared_base),
            "Trace lacks requested real ticks or actual shared-base/wet/increased-increment coverage for every type")
    return {"ticks": count, "begin_tick": previous_tick - count,
            "end_tick": previous_tick, "first": first, "last": previous,
            "first_ready": first_ready, "first_return": first_return,
            "total_base": total_base, "total_wet": total_wet,
            "increased_increment_ticks": increased, "shared_base_ticks": shared_base,
            "return_events": return_events}

def raw_subject_rows(world):
    """Extract saved counters directly; no cached wet state is saved authority."""
    work = {row["building_id"]: row for row in world["irrigation_productivity"]["work"]}
    bodies = {row["id"]: row for row in world["buildings"]}
    fields = {row["field_id"]: row for row in world["hunting_food"]["fields"]}
    food_work = {row["field_id"]: row for row in world["multi_field_agriculture"]["fields"]}
    physical = {row["field_id"]: row for row in world["physical_food_growers"]["fields"]}
    farms = {row["farm_id"]: row for row in world["physical_food_growers"]["farms"]}
    season = world["seasonal_agriculture"]
    food_seasons = {row["field_id"]: row for row in season["fields"]}
    hemp_base = {row["field_id"]: row for row in world["original_production"]["hemp_farms"] if row["field_id"] is not None}
    hemp = {row["farm_id"]: row for row in season["hemp_farms"]}
    plants = {row["plant_id"]: row for row in world["plantations"]["plants"]}
    plant_seasons = {row["plant_id"]: row for row in season["plants"]}
    sources = {row["building_id"]: row for row in world["plantations"]["sources"]}
    result = []
    for identity, row in work.items():
        harvest_work = 0
        if identity in fields:
            f, s = fields[identity], food_seasons[identity]
            kind, base, picked = f["crop"], food_work[identity]["productive_ticks"], f["harvested"]
            returned = physical[identity]["returned"]
            trip = farms[f["farm_id"]]["latest_trip"]
            cargo = int(trip is not None and trip["field_id"] == identity and
                        trip["harvested_tick"] is not None and trip["returned_tick"] is None)
            progress = base + row["bonus_work"] - s["wasted_work"] - 80 * picked
            harvest_work = s["harvest_work"]
        elif identity in hemp_base:
            f = hemp_base[identity]
            s = hemp[f["farm_id"]]
            kind, base, picked, returned = 5, s["productive_ticks"], s["picked"], f["harvested"]
            trip = s["latest_trip"]
            cargo = int(trip is not None and trip["picked_tick"] is not None and trip["returned_tick"] is None)
            progress = bodies[f["farm_id"]]["progress"]
        else:
            f, s = plants[identity], plant_seasons[identity]
            kind = {36: 6, 37: 7, 38: 8}[bodies[identity]["kind"]]
            base, picked, returned, progress = f["productive_ticks"], f["harvested"], f["returned"], f["growth"]
            trip = sources[f["source_id"]]["latest_trip"]
            cargo = int(trip is not None and trip["plant_id"] == identity and
                        trip["picked_tick"] is not None and trip["returned_tick"] is None)
        result.append([identity, kind, base, row["wet_work_ticks"], row["bonus_work"], row["clipped_bonus"],
                       progress, s["ready_tick"], picked, returned, cargo, None, None, None, None,
                       s["wasted_work"], harvest_work])
    return sorted(result, key=lambda row: row[1])


def report(value, audit, output):
    require(value["format"] == "openemperor-irrigation-productivity-acceptance-v1" and
            value["status"] == "PASS" and value["rule"] == 33 and value["schema"] == 49,
            "Positive irrigation acceptance failed its exact Rule33/schema49 identity")
    require(value["real_ticks"] == audit["ticks"] == CONTINUATION_TICKS and audit["begin_tick"] == 0,
            "The requested 24k continuation lacks its sole actual World-tick trace")
    require(value["unchanged_supplied_model"] and value["source_bytes_mode_mtime_unchanged"] and
            not value["snapshot_stock_or_population_injection"] and value["minimum_funds"] >= 0 and
            value["maximum_supplied_ditch_cells"] >= 21,
            "Actual proof altered its model, injected authority, exhausted Funds or lacks21 supplied ditch cells")
    require(value["recovery_exact"] and value["fresh_restore_continuation_exact"],
            "Actual Save/Recovery or restored continuation differs")
    require(value["work_trace"] == "paired-work.jsonl" and value["trace_columns"] ==
            ["building_id", "kind", "base", "wet", "bonus", "clipped", "progress", "ready_tick", "picked", "returned",
             "cargo", "water_available", "source_workers", "source_required", "source_stock", "wasted", "harvest_work"],
            "Unknown streamed source-work schema")
    names = ["Wheat", "Millet", "Rice", "Cabbage", "SoyBeanCurd", "Hemp", "Tea", "Lacquer", "Mulberry"]
    rows = value["nine_type_comparison"]
    require(len(rows) == 9 and [row["kind"] for row in rows] == names,
            "Missing, duplicate or relabelled crop comparison")
    for kind, row in enumerate(rows):
        dry, wet = audit["last"]["dry"][kind], audit["last"]["wet"][kind]
        require(row["building_id"] == wet[0] == dry[0] and
                row["actual_returns_dry"] == dry[9] > 0 and row["actual_returns_wet"] == wet[9] > 0 and
                row["actual_base_work_dry"] == dry[2] and row["actual_base_work_wet"] == wet[2] and
                row["wet_work_ticks"] == wet[3] > 0 and row["bonus_work"] == wet[4] > 0 and row["clipped_bonus"] == wet[5],
                "A reported type differs from actual tick work, bonus or physical source returns")
        require(row["matched_fully_staffed_base_ticks"] == audit["shared_base_ticks"][kind] > 0 and
                row["measured_increased_work_increment_ticks"] == audit["increased_increment_ticks"][kind] > 0,
                "A type lacks increased growth on the same real fully staffed base-work tick")
        require(row["first_ready_base_dry"] == audit["first_ready"]["dry"][kind]["base"] and
                row["first_ready_base_wet"] == audit["first_ready"]["wet"][kind]["base"] < row["first_ready_base_dry"] and
                row["first_return_tick_dry"] == audit["first_return"]["dry"][kind] and
                row["first_return_tick_wet"] == audit["first_return"]["wet"][kind],
                "Actual first readiness/work/return chronology contradicts the claimed productivity change")
    for cohort in ("dry", "wet"):
        require(value[f"actual_{cohort}_returns"] == audit["return_events"][cohort],
                "Reported receipt events differ from actual per-tick source-return deltas")

    manifest = value["checkpoints"]
    require(len({entry["file"] for entry in manifest}) == len(manifest), "Checkpoint manifest reused a file")
    persistent = list(range(11)) + [15, 16]
    for cohort in ("dry", "wet"):
        for name, traced in (("seed", audit["first"][cohort]), ("final", audit["last"][cohort])):
            document = load(output / f"{cohort}-{name}.save.json")
            world = snapshot(document)
            require(world["ticks"] == (audit["begin_tick"] if name == "seed" else audit["end_tick"]) and
                    world["treasury"] >= 0, "Saved endpoint lost actual tick chronology or real Funds")
            saved = raw_subject_rows(world)
            require(len(saved) == 9 and all(all(a[index] == b[index] for index in persistent)
                    for a, b in zip(saved, traced)), "Raw saved counters/cargo differ from the independently streamed endpoint")
    for entry in manifest:
        file = Path(entry["file"])
        require(file.name == str(file) and file.suffixes[-2:] == [".save", ".json"] and entry["exact_roundtrip"],
                "Checkpoint is not one bounded canonical private save")
        document = load(output / file)
        require(snapshot(document)["ticks"] == entry["tick"], "Checkpoint was relabelled to a different actual tick")
        roundtrip = output / (entry["label"] + ".roundtrip.json")
        require((output / file).read_bytes() == roundtrip.read_bytes(), "Canonical schema49 roundtrip bytes changed")
        if not entry["label"].endswith("-seed"):
            require(entry["exact_recovery"], "A transition checkpoint lacks exact bounded Recovery")
    if "final_scenes" in value:
        require(len(value["final_scenes"]) == 2 and all(scene["tick"] == audit["end_tick"] and
                scene["funds"] >= 0 and scene["buildings"] <= 82 and scene["movement_channels"] <= 58
                for scene in value["final_scenes"]), "Final real city exceeded unchanged limits or exhausted Funds")
    pump_events(value, output)
    return {"real_ticks": audit["ticks"], "crop_types": 9,
            "shared_base_ticks": audit["shared_base_ticks"],
            "increased_increment_ticks": audit["increased_increment_ticks"],
            "raw_checkpoints": len(manifest)}

def pump_events(value, output):
    events = value["pump_transition_events"]
    expected = ["natural-pump-fire", "actual-inspector-extinguish", "post-fire-phase27-supply",
                "paid-ditch-cut", "paid-ditch-reconnect", "reconnected-phase27-supply",
                "pump-paused", "pump-restarted", "restarted-phase27-supply"]
    require([event["type"] for event in events] == expected,
            "Missing, duplicated or reordered genuine Pump transition")
    states = [event["state"] for event in events]
    require(all(a["tick"] < b["tick"] for a, b in zip(states, states[1:])),
            "Pump evidence is not an actual chronological sequence")
    fire, arrival, rewet, cut, reconnect, connected, paused, restart, resumed = states
    require(fire["pump_burning"] and not fire["pump_supplying"] and fire["supplied_ditch_cells"] == 0 and
            fire["pump_eligible_since_tick"] is None, "Natural Pump fire failed to end supply immediately")
    require(not arrival["pump_burning"] and not arrival["pump_supplying"] and arrival["supplied_ditch_cells"] == 0 and
            arrival["pump_workers"] > 0 and arrival["pump_eligible_since_tick"] == arrival["tick"] + 1 and
            events[1]["arrival_tick"] == arrival["tick"],
            "Physical extinguish arrival granted early supply or lacks paid actual staff")
    for state in (rewet, connected, resumed):
        require(state["supplied_ditch_cells"] >= 21 and state["pump_supplying"] and state["pump_workers"] > 0 and
                state["pump_phase_tick"] == state["tick"] and (state["tick"] - 28) % 51 == 0 and
                not state["pump_burning"], "Resupply did not arise on the existing actual phase27")
    next_phase = lambda after: 28 + max(0, (after - 28 + 50) // 51) * 51
    require(rewet["tick"] == next_phase(arrival["tick"] + 1) and
            connected["tick"] == next_phase(reconnect["tick"] + 1) and
            resumed["tick"] == next_phase(restart["tick"] + 1),
            "Recovery/reconnect/restart bypassed or skipped the first genuine eligible source phase")
    require(cut["supplied_ditch_cells"] < 21 and reconnect["supplied_ditch_cells"] < 21 and
            paused["supplied_ditch_cells"] == restart["supplied_ditch_cells"] == 0 and
            not paused["pump_supplying"] and not restart["pump_supplying"] and
            paused["pump_eligible_since_tick"] is None and restart["pump_eligible_since_tick"] == restart["tick"] + 1,
            "A network cut or operation transition retained unsupported downstream water")
    extinguished = snapshot(load(output / "wet-pump-extinguished-before-phase.save.json"))
    pump = extinguished["irrigation"]["pumps"][0]["building_id"]
    bodies = {body["id"]: body for body in extinguished["buildings"]}
    require(bodies[events[1]["paid_watch"]]["kind"] == 10 and
            bodies[events[1]["paid_watch"]]["placed_tick"] == fire["tick"],
            "The recovery Inspector lacks an actually purchased post-fire Watch")
    require(any(row[0] == arrival["tick"] and row[1] and row[2][0] == events[1]["courier"] and
                row[2][1] == events[1]["paid_watch"] and row[2][2] == pump and
                fire["tick"] < row[2][3] < arrival["tick"]
                for row in extinguished["original_production"]["fire_protection"]["arrivals"]),
            "No raw saved actual Inspector trip physically extinguished this real Pump")
    commands = load(output / "wet-commands.json")
    watch = bodies[events[1]["paid_watch"]]
    require(any(command["tick"] == fire["tick"] and command["command"] == 13 and
                command["cell"] == watch["cell"] and command["changed"] and
                command["funds_before"] > command["funds_after"] and
                command["sequence_after"] == command["sequence_before"] + 1 for command in commands),
            "Recovery Watch lacks a real charged public command")
    before = snapshot(load(output / "wet-before-cut-with-cargo.save.json"))
    after = snapshot(load(output / "wet-network-cut-dry.save.json"))
    require(before["ticks"] == after["ticks"] == cut["tick"] and
            raw_subject_rows(before) == raw_subject_rows(after) and
            any(row[10] for row in raw_subject_rows(before)) and
            len(before["irrigation"]["ditches"]) == len(after["irrigation"]["ditches"]) + 1,
            "Paid ditch cut changed actual work/cargo or lacks a genuine loaded return witness")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--data", type=Path, required=True)
    parser.add_argument("--map", default="Cities/Chengdu.map")
    args = parser.parse_args()
    data = args.data.resolve()
    sources = [data / "Model/EmperorBuildingModels.txt", data / args.map]
    if not all(path.is_file() for path in sources):
        print("NOT_RUN: supplied Original map/model unavailable")
        return 77
    before = {path: fingerprint(path) for path in sources}
    parent = ROOT / ".local/agri-mac08"
    parent.mkdir(parents=True, exist_ok=True)
    proof = Path(tempfile.mkdtemp(prefix="positive-replay-", dir=parent))
    executable = args.executable.resolve(strict=True)
    checks = []
    creator_inputs = None
    for label, extra in (("creator", []), ("replay", ["--replay", proof / "creator"])):
        output = proof / label
        command = [str(executable), "--data", str(data), "--map", args.map,
                   "--output", str(output), "--ticks", str(CONTINUATION_TICKS), *map(str, extra)]
        (proof / f"{label}.command.json").write_text(json.dumps(command, indent=2) + "\n")
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=600)
        (proof / f"{label}.log").write_text(result.stdout + result.stderr)
        require(result.returncode == 0, f"{label} failed; retained {proof}: {result.stdout}{result.stderr}")
        value = load(output / "report.json")
        audit = trace(output / "paired-work.jsonl")
        checks.append({"process": label, **report(value, audit, output)})
        if label == "creator":
            creator_inputs = {path: fingerprint(path) for path in output.glob("*.save.json")}
        else:
            require(value["separate_process_replay_exact"], "No separate-process exact replay witness")
            require(creator_inputs == {path: fingerprint(path) for path in creator_inputs},
                    "Replay modified explicitly supplied creator checkpoint bytes, mode or mtime")
    for cohort in ("dry", "wet"):
        require((proof / f"creator/{cohort}-final.save.json").read_bytes() ==
                (proof / f"replay/{cohort}-final.save.json").read_bytes(),
                "Separate-process canonical dry/wet endpoints differ")
    require((proof / "creator/paired-work.jsonl").read_bytes() == (proof / "replay/paired-work.jsonl").read_bytes(),
            "Separate-process actual base/wet/carry/physical-return timeline differs")
    require(before == {path: fingerprint(path) for path in sources}, "Supplied Original source was modified")
    (proof / "independent-work-checks.json").write_text(json.dumps(checks, indent=2) + "\n")
    print(f"PASS: nine paid matched wet/dry chains, actual Pump fire/recovery, quarter carry and separate-process24k; {proof}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
