# Original religion sources: construction, visits and Confucian prerequisites

This OE-22 investigation extends the bounded
[visit/timer](original-service-timer-contract.md) and
[spawn-counter](original-service-spawn-contract.md) evidence. It establishes a
concrete initial producer of the previously opaque religion condition. It does
not establish every archive, reset or session transition. The
[runtime integration](../original-economy/architecture-map.md#paid-sources-inventories-and-actual-arrivals) deliberately
uses authored behavior at the two Confucian boundaries described below.

The read-only executable is `Emperor.exe`, SHA-256
`6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`.
Addresses are static virtual addresses in its PE32 image based at `0x400000`.
The model SHA-256 is
`7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6`.
No executable was run. Original bytes, model tables, disassembly and generated
research reports remain private.

The private reproduction is:

```sh
python3 -B .local/original-economy/oe22/research/check_religion_sources.py
```

Its actual static check passed **129 assertions, 29 contiguous instruction
windows, 1,326 instructions and 3,979 bytes**. Each instruction was checked
against the PE section mapping; all seven pinned source files retained their
hash, size, mode and modification time. This is static evidence, not a game
test or visual acceptance.

## Typed sources and physical dimensions

| Model type and exact label | Raw Cost / Employees | Initial square side | Concrete source |
|---|---:|---:|---|
| 215 `Daoist Shrine` | 45 / 4 | 2 | `cReligiousBldg`, vtable `0x7bc0b4` |
| 217 `Buddhist Shrine` | 45 / 4 | 2 | `cReligiousBldg`, vtable `0x7bc0b4` |
| 219 `Confucian Academny` | 80 / 12 | 3 | `cConfucianAcademy`, vtable `0x7bc318` |

The six-case factory `0x5aae70`, table `0x5ab018`, sends 215/217 to
`0x5ab0d0`; 219 goes through `0x5ab860`, `0x5abf40` and the ordinary
constructor. The constructors actually publish the independently identified
RTTI vtables. The common initializer at `0x428abc` reads
`0x823598 + 24 * type` into source byte `+0x07`. The false special-shape
callback `0x413a00` and side getter `0x416ad0` bind the ordinary square path.
This establishes these initial dimensions, not every subsequent lifecycle
shape or a geometry inferred from a model label.

## Actual Actor, visit and timer binding

All three vtables bind spawn `+0x20` to `0x5ab300`, which passes Actor subtype
35 to the shared attempt `0x51cf90`. Actor row
`0x84e780 + 40 * 35` names `FIG_PRIEST` and callback `0x4d5d00`.
The actual spawned Actor receives source ID `source+0xb4` in its word `+0x62`
at `0x51d05e`. The priest callback resolves that ID through manager
`0x8c7634` and invokes its source `+0x24`, bound to `0x51d0c0`.

That source state handler reaches shared movement at `0x4e6b70` and
`0x4e47a0`; their observed branches reach distribution `0x4eacd0`.
Distribution resolves the real source again, invokes source `+0x28` with the
Actor, and the bounded candidate scan calls source `+0x2c` at `0x42a0df`.
For these sources that last slot is `0x5ab580`, the DirectReligion callback.
The scan is Actor-centered and has additional filters. Its bound is not a
static building coverage radius.

The source index mapper `0x5ab080` / table `0x5ab0b0` maps 215/216 to 1,
217/218 to 2, and 219 to 3. Constructor `0x5ab11d` stores the index at
`+0x154`. At a permitted actual visit, `0x5ab5f2` writes duration 40
(`0x517270`) to own-info byte `+0x0d + index`:

| Religion | Own-info timer |
|---|---|
| Daoist | `+0x0e` |
| Buddhist | `+0x0f` |
| Confucian | `+0x10` |

The independent unsigned nonzero decrements are at `0x517315`. Daoist OR
Buddhist is a needs predicate over two timers; neither duration nor stock is
combined. The pure nine-role service kernel remains the same. Original spawn
thresholds differ: ordinary 215/217 use `0x5ab330`, while 219 uses `0x5ab6f0`.
A due counter means an attempt, not an arrival or a recovered World tick rate.

## A proven initial producer of `source+0x174`

This field is separate from the material stock at source `+0x40`.
The full initial dataflow is now observed:

1. `0x5a77d0` loads the actual global manager `0x1312f08` and jumps to
   constructor `0x5a7940`. Helper `0x426a60` returns that same manager.
2. The manager constructs four entries using `0x5a7010`, storing category
   pointers at `+0x38`, `+0x3c`, `+0x40` and `+0x44`.
3. Entry constructor `0x5a7010` reads its fourth argument at current stack
   `+0x34` (`0x5a7093`) and writes it to entry byte `+0x16` (`0x5a709e`).
4. At `0x5a79a1`, EBX becomes 1. The category-3 construction at `0x5a7a0b`
   pushes that EBX as argument four and stores the result at manager `+0x44`.
   The other three construction sites push 0 for that argument.
5. Religion source constructor `0x5ab123` passes its category to
   `0x5a7ea0` on the manager returned by `0x426a60`. The bounded lookup reads
   `manager+0x38+4*index`; `0x5aa960` returns entry byte `+0x16`.
   At `0x5ab144`, the result becomes source byte `+0x174`.
6. DirectReligion `0x5ab5c5` checks that byte. When true it requires an elite
   House even if a common House has positive occupants.

Thus this actual global construction path initializes Ancestor/Daoist/Buddhist
to false and **Confucian to true**. The stronger claim that every restored or
modified session always has those values is not established. Revision22's
explicit `elite_only=false` for Confucian is an authored deviation from this
observed initial behavior, not an Original fact or a conclusion from a class
name. Elite integration must revisit this boundary using the actual source
condition and appropriate historical visit eligibility.

## Confucian material input and consumption

Confucian vtable `+0x1bc` binds `0x508160`: signed source WORD `+0x40`
must be positive before the ordinary staffing percentage at virtual `+0x1b4`
is exposed. Buying an empty staffed Academy alone therefore does not reproduce
the Original's successful dispatch prerequisites.

The same class has concrete material operations:

| Binding | Observed operation |
|---|---|
| `+0x90 → 0x5ab8c0` | Setup calls global helper `0x480740 → 0xc058d0`, then queries `0x4812e0` with resource 27. True sets source WORD `+0x16=27`; false sets it to 10. |
| `+0x158 → 0x5aba20` | Acceptance checks the selected resource, available capacity and optional outstanding-reservation headroom. |
| `+0x154 → 0x5abb10` | Actual receipt checks status 1/3 and resource identity, adds accepted quantity into WORD `+0x40` bounded by virtual capacity, then calls source `+0x160`. |
| `+0xa0 → 0x5abaa0` | Positive staffing and stock accumulate an own-info signed-word counter. At 100, reset that counter, subtract 100 raw units from WORD `+0x40`, and clamp a negative remainder to zero. |

The actual resource-name pointer table `0x872030 + 4 * id` resolves
**10 → `Wood`** (`0x872260`) and **27 → `Paper`** (`0x8721cc`). The
adjacent default-price table `0x8720a4`, indexed by accessor `0x408d00`, has
45/125 respectively, independently matching the original `Model/Trade.txt`
(SHA-256 `904a7da8ef334b963941b39ca17faeb8136810179a4b52ae14ede2afba0237ed`).
The names do not come only from the previous asset audit's authored labels.

Neither resource belongs to the frozen six household `Goods`. There is no
current paid Wood/Paper producer, material transport or Academy material-stock
authority in Revision22. The global resource-27 predicate's complete gameplay
cause, upstream routing, material unit conversion and consumption-call cadence
remain outside this bounded proof. Do not credit stock, reinterpret a current
household good, or claim that staffing alone reproduces Original behavior.
The approved staffing-only adapter is explicitly limited. Later physical
upstream and elite work must resolve both prerequisites before full actual
Academy behavior can be accepted; the numbered luxury/elite tasks do not by
themselves create a Wood/Paper contract.

## Historical adapter and current integration

The static results above record the historical investigation; the current
integration described here does not imply a fresh run of its private checker.
The staffing-only and `elite_only=false` adapter descriptions refer to
historical development revision22. Current revision27 instead prepares the
Confucian elite-only condition and requires actual Paper delivered through the
paid Hemp→Paper production and auxiliary transport chain. A successfully
published Academy tour consumes exactly one Paper; the owned inventory ledger
binds that sink to real tour sequences. The current World additionally requires
positive occupants for all religious targets, so even Confucian's paid visits do
not reproduce the pure Original predicate's empty-Elite allowance.

These are separate evidence boundaries. The current physical Paper supply is
not an implementation of the observed Original27/10 resource choice or the
100-raw-stock debit after 100 eligible consumption-counter calls. Their complete
units, selector and cadence remain unresolved. Ancestor214, Daoist215,
Buddhist217 and Confucian219 retain distinct actual-arrival House40 timers;
Temple216/Pagoda218, offerings, festivals, Hero/God AI and happiness are not
admitted as new paid mechanics. The [culture/venue guide](../original-economy/oe56-culture-venues.md#religion-remains-a-separate-service-contract)
records the current implementation gaps; the Rule22 Music experiment changes
none of these religious contracts.

## Original art selectors for later visual integration

| Source | General static key / physical record | SprMain walking key / record | Alternate key / record |
|---|---|---|---|
| 215 | `0x49b` / 2245 | `0x4c70` / 7528 | `0x4c71` / 7624 |
| 217 | `0x49c` / 2258 | `0x4c26` / 2321 | `0x4c27` / 2417 |
| 219 | `0x49f` / 2309 | `0x4c3c` / 4149 | `0x4c3d` / 4245 |

Static source registration VAs are `0x8249c8`, `0x8249f8`, `0x824a28`.
Source selector `+0x244` is `0x5ab520` for 215/217 and `0x5abba0` for219.
Its observed argument 1 selects the alternate; other arguments select walking.
Walking groups have 12 phases and stride8. Alternate groups have 12 phases
and stride1. General metadata records have stride64; SprMain records have
stride72. This check inspected raw metadata without decoding or opening new
images. Original feet, complete runtime animation selection and final visual
acceptance remain separate obligations.
