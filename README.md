# OpenEmperor

A clean-room reimplementation of _Emperor: Rise of the Middle Kingdom_ for
native macOS on Apple Silicon. **0.1.0-alpha.2** includes a playable city
sandbox and an optional original-economy experiment. Gameplay and visual
parity remain incomplete; original campaigns and savegames are unsupported.

You need your own installed or extracted Emperor game data. Original assets
are read directly and are never bundled.

## Build and run

Install Xcode Command Line Tools, CMake and OpenSSL 3. CMake fetches pinned
SDL3 and JSON dependencies when needed and uses system zlib.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --parallel
./build/openemperor
```

On first launch, select the Emperor data directory. **Play Original economy**
opens the recommended experimental mode for a new city. Check the data folder,
map, difficulty and paid starter or empty start before **Start sandbox**.

For a first city, choose **Cities/Xia.map**, **Normal** and **Paid four-house
starter**, then press **Space** to run. This explicitly selects the existing
Rule18 economy; its measured city still has five House fires. The
[player guide](docs/original-economy/player-handoff.md) covers production,
development, saving and the remaining limits.

**P Playable Agriculture** starts the separate authored farming mode: Wheat,
Millet, Rice, Cabbage and Soy-to-BeanCurd feed the existing Market/House supply.
Each paid Farm supports one productive field; Rice needs fertility100 ground.
This mode also includes the existing plantations and irrigation infrastructure.
See the [agriculture guide](docs/original-economy/agriculture.md#playable-agriculture-agri-mac01).

**F Playable Fishing** starts the separate successor with Fish production:
a paid shore Quay launches real boats to finite fishing areas, returns and
unloads Fish, then supplies Markets and Houses through physical deliveries.
It also retains the five crops, plantations and irrigation. See the
[fishing guide](docs/original-economy/agriculture.md#playable-fishing-agri-mac02).

**Y Playable Hunting** adds paid Hunter's Tents, living wildlife and Meat.
Hunters pursue actual animals and carry Meat back before Markets and Houses
can receive and consume it. This authored mode retains all crops and fishing.
See the [hunting guide](docs/original-economy/agriculture.md#playable-hunting-agri-mac03).

**C Multi-field Agriculture** shares each Farm's finite labor across up to eight
paid fields. Every field retains its own crop progress and harvest history;
additional fields share the Farm's work rather than adding free throughput.
Select a Farm or one of its fields to bind another field to that Farm.
The mode retains hunting, fishing, plantations and irrigation. See the
[multi-field guide](docs/original-economy/agriculture.md#multi-field-agriculture-agri-mac05).

**E Physical Growers** sends a Farm worker to tend each selected crop, carry
its harvest back and credit Food only after the real Farm return. The worker
shares the Farm's existing staff and finite storage. Working and returning
Growers use an explicitly authored marker; their inspector shows crop and cargo.

**Z Seasons** selects the separate Rule32 seasonal successor. The existing World
calendar gates all five Food crops, Hemp, Tea, Lacquer and Mulberry; each plant's
inspector shows its numeric season windows, next opening and any pending expiry.
Food needs real field harvest work and Farm return, and Hemp needs real pickup
and return. Plan stored reserves and year-round fishing/hunting for scarcity.
These schedules and yields are authored; older modes keep their existing clocks.
See the [seasonal guide](docs/original-economy/agriculture.md#seasonal-agriculture-agri-mac07).

**X Irrigation yields** or `--irrigation-productivity` selects separate Rule33.
An actually supplied, eligible crop earns one extra growth unit per four real
wet work ticks. Inspect a field or plant for its Pump, wet/dry state, retained
carry and current work blocker. Harvesting and physical return still credit stock.

**Q Food processing** or `--food-processing` selects separate Rule34. Build a
Mill (Shift-M), then a FoodShop (Shift-K) beside a paid Market. Select the Mill
and use **Recipe grade** / Shift-G to choose one to four distinct ingredients.
Actual staffed work turns each consumed ingredient portion into one Dinners
portion of that grade. Physical shop deliveries share the existing House Food
stock and consumption event. Inspect recipes, cargo, returns and first receipts.
These recipes and timing are authored; Salt, Spices and grade5 remain unavailable.

**V Food Warehouses** or `--food-warehouses` selects separate Rule35. Existing
paid Warehouses gain a finite Food compartment and their own Food courier.
Raw Food travels to Mills or Markets; stored Dinners travel to FoodShops.
Inspect the Warehouse for typed stock, capacity, cargo and actual receipts.
Goods/Aux storage and older modes retain their existing contracts.

**R Reconfigure** or `--agriculture-reconfiguration` selects Rule36. Drain and
remove settled paid Food fields, plantation plants and Pumps, then rebuild at
full cost with new identities. Real work, cargo and histories remain conserved;
Farmhouses, Quays, Tents, processors and stock owners remain protected. See the
[reconfiguration guide](docs/original-economy/agriculture.md#safe-agriculture-reconfiguration-agri-mac11).
This mode also shows actual field/plant stages, work and water overlays, and
physical workers, boats and wildlife. Qualified Original art and labelled
authored geometry are distinguished in the [visual guide](docs/building-visual-profile.md#rule36-agriculture-state-presentation-agri-mac12).

The [human playtest checklist](HUMAN_PLAYTEST.md) covers the complete current
Agriculture mode. Its [technical checks](docs/testing.md#final-agriculture-integration-agri-mac14)
separate actual production/consumption from remaining long-play, luxury and
native-pointer limitations. Human acceptance remains outstanding.

**Advanced / Legacy → N New Sandbox** retains the classic sandbox profiles and
their defaults; **O/F/S/M/W/I/D/T/U** retain the individual economy experiments.
**L Load Save** continues an existing OpenEmperor save with its own rules.
Saving or loading never upgrades an original-economy generation.

## Controls

- **Space** pauses or resumes; **.** advances one paused tick.
- **Arrow keys** pan; the mouse wheel zooms; **Z** cycles 1×/2×/4×.
- **F5 / F9** save and load; **Escape** returns to the menu.
- **Help** lists the current profile's tools and shortcuts.

Saves and settings normally live in
`~/Library/Application Support/OpenEmperor/OpenEmperor/` on macOS.

See [known issues](KNOWN_ISSUES.md) for current limitations and
[documentation](docs/README.md) for gameplay, development, testing and packaging.
