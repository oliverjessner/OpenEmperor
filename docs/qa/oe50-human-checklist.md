# OE50: menschliche Spielsitzung

**Alle menschlichen Ergebnisse sind NOT_RUN.** Erst die eigene beobachtete
Bedienung beziehungsweise ein konkreter Nutzerbericht ändert einen Eintrag.
CTest, Software-SDL, eine angeforderte Mausaktion und ein vorbereiteter Launcher
füllen keinen menschlichen PASS aus. Der [OE50-Bericht](oe50-native-playtest.md)
trennt diese Belegebenen.

## Build und isolierter Start

Die bereits geprüfte lokale27.0-QA-App kann ohne erneuten Build gestartet
werden. Die folgenden Pfade gehören zum isolierten OE50-Worktree. Den neuen
Human-Root nur verwenden, wenn er bislang unbenutzt ist; sonst einen neuen
Suffix wählen. Dieser Start wurde nicht als menschliche Sitzung ausgeführt:

```sh
oe50_workspace=/Users/oli/github/OpenEmperor/.local/oe49/worktree
oe50_candidate="$oe50_workspace/.local/oe50/dist-27-01/OpenEmperor.app"
open -n "$oe50_candidate" --args \
  --data /Users/oli/github/OpenEmperor/.local/gog-extracted/app \
  --app-root "$oe50_workspace/.local/oe50/human-session-01"
```

Die App enthält die tatsächlich geprüften privaten SDL3.4.14-/OpenSSL4.1.0-
Dependencies und verlangt macOS27.0. Der strikte Paket-/Relocation-/ZIP-Test ist
bestanden; Notarisierung, unabhängiger sauberer Mac und menschliches Spielen
bleiben ungeprüft. Für einen eigenen Build gilt der folgende separate Weg.

Vom Repository-Root auf einem Apple-Silicon-Mac mit den
[Build-Voraussetzungen](../../README.md) ausführen. Für jeden neuen Versuch
unbenutzte Verzeichnisse wählen; vorhandene Beweise oder Saves nicht ersetzen.
Der Datenpfad muss die ausdrücklich gewählte eigene Emperor-Installation sein.

```sh
git rev-parse HEAD
cmake -S . -B .local/oe50/human-build-01 -DCMAKE_BUILD_TYPE=Release \
  -DOPENEMPEROR_USE_SYSTEM_SDL3=ON -DOPENEMPEROR_BUILD_MACOS_APP=ON \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=27.0 \
  -DOPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT=/absoluter/pfad/zu/Emperor
cmake --build .local/oe50/human-build-01 --parallel 2 --target openemperor
```

Das Beispiel wählt27.0 ausdrücklich: Die aktuell geprüfte Homebrew-
OpenSSL4.1.0-Bibliothek verlangt macOS27.0; SDL verlangt26.0. Der erste
26.0-Paketversuch wurde deshalb korrekt vom strikten Prüfer verweigert.
Die tatsächlichen Mindestversionen der verwendeten Bibliotheken müssen zur
gewählten App-Mindestversion und zum eigenen Mac passen. Der bestehende
Paketstandard26.0 bleibt für passende Dependencies erhalten; er wird nicht
heimlich angehoben. Ein Paket für diesen27.0-Datensatz verlangt ausdrücklich
`--min-os 27.0` im bestehenden Paketablauf. Dieser frische27.0-Paketnachweis
ist bestanden; der26.0-Fehlversuch bleibt mit seiner Diagnose erhalten.

Das erzeugt eine native CMake-Entwicklungs-App. Ihre installierten Bibliotheken
bleiben eine lokale Voraussetzung. Für ein Paket mit privaten Dependencies den
bestehenden [macOS-Paketablauf](../macos-packaging.md) mit frischen Build-/Dist-
Pfaden verwenden und dessen Abhängigkeits-/Lizenz-/Signaturprüfung behalten.
Weder die CMake-App noch eine Ad-hoc-Signatur belegt Notarisierung,
Finder/Gatekeeper oder einen unabhängigen sauberen Mac.

Für die Spielsitzung das App-Executable direkt starten. Dadurch sind
Datenpfad, eigener App-Root und getrennte Logs sichtbar; ein privates QA-Tool
wird nicht benötigt:

```sh
OPENEMPEROR_INPUT_DIAGNOSTICS=1 \
  .local/oe50/human-build-01/OpenEmperor.app/Contents/MacOS/OpenEmperor \
  --data /absoluter/pfad/zu/Emperor \
  --app-root "$PWD/.local/oe50/human-session-01" \
  > .local/oe50/human-input-01.jsonl 2> .local/oe50/human-stderr-01.log
```

Dieses Terminal bleibt während der App offen. Es gibt keine automatische
Zeitbegrenzung und kein automatisches Beenden. Für einen separat beobachteten
LaunchServices-Start nach manuellem Beenden derselben App:

```sh
open -n "$PWD/.local/oe50/human-build-01/OpenEmperor.app" --args \
  --data /absoluter/pfad/zu/Emperor \
  --app-root "$PWD/.local/oe50/human-session-01"
```

Nicht beide Starts gleichzeitig ausführen. Nur die eigene QA-App beenden;
persönliche Sitzungen unangetastet lassen. Ein normaler Finder-Doppelklick
verwendet ohne expliziten App-Root die persönlichen App-Einstellungen und ist
ein gesonderter Test. Originaldaten sind in allen Fällen read-only.

## Vorhandenen OpenEmperor-Stand als Arbeitskopie laden

Nur einen ausdrücklich bereitgestellten **OpenEmperor-JSON-Spielstand** wählen,
keinen originalen Emperor-Save und keine Suche in persönlichen Saves. Für diese
Runde einen weiteren frischen Root verwenden. Das Quell-JSON vor und nach der
Sitzung hashen; F5 darf nur die Kopie schreiben.

```sh
oe50_source_save=/absoluter/pfad/zum/bereitgestellten-openemperor-save.json
oe50_copy_root="$PWD/.local/oe50/human-copy-session-01"
mkdir -p "$oe50_copy_root/saves"
cp -n "$oe50_source_save" "$oe50_copy_root/saves/01-working-copy.json"
chmod u+w "$oe50_copy_root/saves/01-working-copy.json"
shasum -a 256 "$oe50_source_save" "$oe50_copy_root/saves/01-working-copy.json"
```

Nur mit demselben passenden Daten-/Modellpfad und
`--app-root "$oe50_copy_root"` starten. **L Load Save → 01-working-copy →
Open selected** lädt die Kopie pausiert. Profil/Rule/Schema prüfen; keine
Originalwirtschafts-Generation upgraden. Gleiche Hashes vor dem Spielen belegen
die Kopie; ein unveränderter Quellhash nachher schützt den gelieferten Stand.
Ändert sich die Quelle unerwartet, Sitzung stoppen und den Vorgang melden.

## Sitzung und Ergebnisblatt

Vor Beginn die tatsächlich verwendeten Angaben notieren:

| Angabe | Menschlicher Eintrag |
| --- | --- |
| Datum, Dauer, Person | Noch nicht beobachtet |
| Git-SHA / Binary-SHA256 / App-Pfad | Noch nicht erfasst |
| macOS, CPU, SDL, tatsächlicher Renderer | Noch nicht erfasst |
| Fenster-/Pixelgröße, Display-/DPI-Skalierung, externe Displays | Noch nicht erfasst |
| Daten-/Modell-/Kartenquelle und Hashes | Noch nicht erfasst |
| Map, Profil, Rule, Policy, Schema, Starttick | Noch nicht erfasst |
| Eigener App-Root / Arbeitskopie / Quellhash | Noch nicht erfasst |

Eine überschaubare erste Stadt: **Play Original economy → Cities/Xia.map →
Normal → Paid four-house starter → Start sandbox**. Datenwurzel und ausgewählten
Modus vorher prüfen. Das wählt ausdrücklich die bestehende Rule18/Schema34/
Policy3. Die Stadt startet pausiert; mit dem geprüften Modell stehen vier
bezahlte Häuser und10596 von12000 Funds, leere Warenlager und noch keine
Bewohner bereit. Andere Quellen können einen anderen Preis ergeben; den echten
Preis/Modellhash notieren. **Space** startet echte Ticks. Die
[Spielanleitung](../original-economy/player-handoff.md) erklärt den Ausbau.

| ID / menschliche Aktion | Erwartetes, konkret zu beobachtendes Ergebnis | Ergebnis |
| --- | --- | --- |
| H01 Daten und Menü | Nur die gewählte Datenwurzel wird gelesen. Empfohlener neuer Modus, Load Save, Advanced / Legacy und gegebenenfalls Continue sind unterscheidbar; die bestätigte Karte/Schwierigkeit/Startart stimmt. | **NOT_RUN** |
| H02 Echte Pointer-Eingabe | Down und Up mit dem eigenen Zeigegerät treffen denselben sichtbaren Button beziehungsweise die gewählte erlaubte Zelle; nur diese Aktion wirkt. Bei Diagnoseprüfung tatsächliche gelieferte Koordinaten notieren. | **NOT_RUN** |
| H03 Bezahlter Bau | Eine legale Straße und ein zusätzliches Haus regulär kaufen. Vorher/nachher Funds, IDs, vollständigen Footprint und genau eine Abbuchung notieren; belegte/gesperrte Zelle wird ohne Zustandsänderung verweigert. | **NOT_RUN** |
| H04 Road-Drag | Eine erlaubte, finanzierte Kette endet an der tatsächlichen Release-Zelle und wird einmal bezahlt. Dieselbe Geste nacheinander über Esc, Rechtsklick, Fokusverlust, Resize und Release über UI/außerhalb abbrechen: keine Teilstraße und keine Abbuchung. | **NOT_RUN** |
| H05 Kamera und Fenster | Pfeile nur während des Haltens; kein klebender Zustand nach App-Wechsel. Mausrad/Z zeigt1×/2×/4×;1280×800,1600×900 und1920×1080 soweit verfügbar; Resize und verfügbare Fullscreen-/Mehrschirmfälle separat notieren. | **NOT_RUN** |
| H06 Inspector und Hilfe | Gebäude an allen eigenen Footprint-Zellen auswählen; lange Listen/Details bis zu beiden Enden scrollen. State/Needs/Goods/Visits lesen; sichtbare Zeilen/Buttons treffen. Help/Modal blockiert Kartenbau; Fokus/Resize während einer gedrückten Schaltfläche bestätigt nichts. | **NOT_RUN** |
| H07 Produktion und reale Lieferung | Food-/Hemp-Farm mit eigenem Feld, Clay/Pottery/Warehouse und Market prüfen. Quelle produziert, Kurier lädt, erreicht das Ziel und entlädt; Hausbestand/Arrival-Witness ändert sich erst dort. Mehrere echte Consumptions beobachten. Dispatch/Stock allein ist kein Liefersieg. | **NOT_RUN** |
| H08 Wasser, Medizin, Religion, Kultur | Quellen regulär finanzieren und verbinden. Tatsächliche Besuche in Needs/Visits und Ablauf bestehender Timer verfolgen. Music/Acrobat/Drama sind der authored Schule→Haus-Pfad; keine Original-Venue-/Performer-Funktion erwarten. | **NOT_RUN** |
| H09 Arbeiter und Betrieb | Tatsächlichen Zuzug/Worker abwarten. Ein Gebäude pausieren/fortsetzen und Priorität ändern; aktiver Transport und Waren bleiben kohärent. Auch pausierte installierte Mitarbeiteranforderungen verursachen den vertraglichen Unterhalt. | **NOT_RUN** |
| H10 Ein bis zwei Hausentwicklungen | Bei einem ausgewählten bezahlten Haus ein oder zwei echte Typaufstiege beobachten. Typ/ID/Tick, erfüllte aktuelle/nächste Needs, reale Goods/Visits und Funds festhalten; nichts im Save editieren und keinen freien Zieltyp setzen. | **NOT_RUN** |
| H11 Feuerwehr und verbleibende Brände | FireWatch regulär kaufen/besetzen/verbinden, reale Inspector-Fahrt und Ankunft verfolgen. Erst Ankunft löscht/schützt. Brandstatus/Schutzablauf notieren; die gemessene Rule18-Stadt ist weiterhin nicht brandsicher. | **NOT_RUN** |
| H12 Eigene Betriebsfinanzierung | Über ein benanntes Tickfenster Anfangs-/End-Funds, echte Steuern, gesamten installierten Unterhalt und gesonderte Käufe festhalten. Betriebssaldo ist Steuern minus Unterhalt; positiver Restbetrag allein ist kein Gewinn. Keine unbezahlten Waren/Worker oder übersprungenen Ticks. | **NOT_RUN** |
| H13 Speichern und vollständiger Neustart | F5 in eigenen Root/Kopie; tick/schema/Funds und Cargo/Reservierungen notieren. App manuell vollständig beenden, mit demselben Root neu starten, L Load Save laden: gleicher vollständiger Stand, pausiert. Danach weiter spielen und F9 prüfen. | **NOT_RUN** |
| H14 Recovery | Autosave ausdrücklich aktivieren und60 simulierte Sekunden abwarten. Load Save → Show recovery history → Open selected öffnet den gewählten Checkpoint pausiert; Quelle/manuelles Save bleibt erhalten. | **NOT_RUN** |
| H15 Zweite Karte und historische Stadt | Eine zweite tatsächlich zugelassene Karte wählen und regulären Bau/Verweigerung prüfen. Fehlt eine legale24×13-Starterfläche, Diagnose festhalten oder ausdrücklich Empty wählen. Separat bereitgestellte historische Arbeitskopie laden; deren Rule/Schema bleibt erhalten. | **NOT_RUN** |
| H16 Grafik und Verständlichkeit | Bei1×/2×/4× Walkerfüße, Gebäude, Occlusion und UI beobachten. Fallbacks/Cropping und verwirrende Wege, fehlende Anleitung oder unerwartet leere Ware mit Screenshot/Schritten melden. Originalkampagnen/-Saves, Shipping/Ferry und vollständige Originalparität werden nicht versprochen. | **NOT_RUN** |

Nicht verfügbare Fälle als NOT_RUN stehen lassen. Produktive Häuser und
Produktionsgebäude im aktuellen Originalwirtschafts-Vertrag sind noch nicht
abreißbar. Keine laufende Stadt für einen nicht unterstützten Abrissversuch
verwenden. Bridge/Ramp sind gesonderte Rule5/6-Sitzungen und keine automatische
Erweiterung der empfohlenen Rule18-Stadt.

## Fehler und abschließende Einschätzung

Bei einem Fehler den Fall, erwartete/gelieferte Eingabe, sichtbares Ziel,
konkrete Schritte, Build/Binary, Map/Rule/Schema, Tick, Pause/Speed, Fenster/Zoom,
Funds/IDs sowie einen Screenshot unter `.local/oe50/` aufzeichnen. Wiederholung
und Kontrollversuch getrennt melden. P0 bedeutet Datenverlust/Korruption,
P1 blockierte Kernbedienung/Integrität, P2 eingeschränktes Gameplay, P3 Darstellung
oder Verständlichkeit; ein Zustellfehler der Desktop-Automation ist kein
belegter Koordinatenfehler der App. Originaldaten oder persönliche Saves nicht
als Git-Anhang aufnehmen.

| Menschliche Einschätzung | Ergebnis |
| --- | --- |
| Bedienung / Wirtschaft / Grafik / Speichern / Stabilität | **NOT_RUN** |
| Für externe Alpha-Nutzer bereit | **NOT_RUN** |

Nur die tatsächlich beobachteten Fälle mit PASS/FAIL/PARTIAL versehen und den
Rest NOT_RUN lassen. Eine vorbereitete oder technisch grüne Sitzung ist keine
menschliche Freigabe.

## Nachtrag vom 2026-11-09 — erste Rückmeldung in OE58

Der [OE58-Bericht](oe58-human-playtest.md) erfasst konkrete
**USER_REPORTED_PASS**-Ergebnisse: Xia/Normal/Rule18-Starter mit vier Häusern,
Straßen-/fünfter Hauskauf, Inspector und die Zeltgrafik nach tatsächlichem
Einzug. Er hält ebenso den schwer erkennbaren Vacant2-Fallback und die
abgeschnittene Loulan-Startermeldung fest. Eine Screenshotdatei liegt diesem
Auftrag nicht bei; eine neue eigene **HUMAN_OBSERVED**-Sichtung wird nicht behauptet.

Die obigen **NOT_RUN**-Felder bleiben als historisches OE50-Ergebnis erhalten.
„Alles andere funktioniert“ ersetzt keine Einzelbeobachtung zu jeder
Abbruchgeste, allen DPI-/Displayfällen, Recovery, mehrstündigem Spiel,
Clean-Mac/Gatekeeper oder historischen Generationen. Human-Abnahme der neuen
OE58-Änderungen bleibt bis zu einem konkreten Bericht **NOT_RUN**.

Die am selben dokumentierten Datum abgeschlossenen bezahlten House-/Menüfälle
und tatsächlich geöffneten Software-SDL-Bilder sind im
[OE58-Ergebnisledger](oe58-human-playtest.md#oe58-result-ledger) separat als
**AUTOMATED_PASS** erfasst. Sie füllen keine menschliche OE50-Zeile aus.
