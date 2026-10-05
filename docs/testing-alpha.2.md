# Testing OpenEmperor 0.1.0-alpha.2

Current native input delivery and the executable short human check are centralized in [Native Input Reliability](input-reliability.md) and [input acceptance](testing-input-acceptance.md). Historical observations below retain their original scope; they do not mark the current human checklist PASS or close profile-specific balance/replanning playthroughs.

This is a 20–30 minute test of the local alpha.2 candidate. Use your own legally obtained Emperor installation. Never attach original `.sg3`, `.555`, map, installer, or decoded asset files to a bug report.

1. Launch `OpenEmperor.app`, select the installed or extracted game-data folder, and open **New Sandbox**.
2. Confirm **City - markets, population and workforce controls**, technical profile `sandbox-city-v11`, rule `3`, and **Prepared starter settlement**. Choose a supported map and start.
3. Let the city run. Observe real Food and Pottery deliveries, a Service visit, a fully supplied Household demand, and a Treasury increase with no simultaneous construction purchase.
4. Spend only Funds the city actually earned on one small extension. Check that **Available**, **Assigned**, **Active demand**, and **Installed demand** explain the workforce result.
5. Select a business in the Inspector. Pause it, confirm its goods and progress remain, resume it, and set a non-Normal priority. Observe that an already moving delivery finishes.
6. Save with F5. Quit the whole application, launch the same build again, load the save, and verify the saved tick, population, treasury, stocks, active routes, operation state, and priority. Resume and confirm normal continuation.
7. In a separate saved test copy, remove a future road segment, observe waiting/rerouting or supply loss, then repair it. Do not expect every deliberately exhausted city to remain recoverable.

For a City-v11 rule-2 save, normal load must remain v2. Use **Enable operation controls in a copy** only when you want an explicitly confirmed v3/schema-12 copy; verify that the source file remains unchanged.

Bug reports should include the display version and build revision, map, profile, rule version, exact steps, expected result, actual result, and, when useful, the affected **OpenEmperor JSON save**. Do not provide original Emperor files. OpenEmperor sends no logs or telemetry automatically.


## Local demolition candidate

Current source supports explicit City-v11 rule 4/schema 13; new games retain v3/schema 12 until native replanning acceptance. This is a local candidate, not a published release. See [demolition validation and outstanding manual acceptance](testing-city-v11-v4.md). Test with your own original files:

```sh
cmake --build build --parallel
./build/openemperor --data "$PWD/.local/gog-extracted/app"
```

Create a City-v11 prepared starter, pause, F5 save, return to the menu, and choose its save in **Load Sandbox → Enable demolition in a copy**. Confirm the separate v4 copy. Then select a freshly placed empty building, cancel Demolish once, confirm and rebuild. Check a House removal lowers worker supply. A stocked Warehouse must explain its blocker; drain using normal deliveries/demand before removing it. A Market rebuild costs 140 and needs new roads as appropriate. Save, quit and load the same save paused; deleted entities and new IDs must persist. On a v3 save, ordinary Load keeps v3; explicitly choose **Enable demolition in a copy** for v4. Do not expect a stocked city to offer instant destruction or refunds.
