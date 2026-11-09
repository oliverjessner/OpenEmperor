# Originalwirtschaft spielen

Die optionale Originalwirtschaft bietet bezahlte Gebäude, Produktion, echte
Kurierlieferungen und Hausentwicklung. Sie ist ein spielbarer Entwicklungsstand;
vollständige Originalspiel- und Grafikparität fehlen noch.

Die [OE50-Checkliste](../qa/oe50-human-checklist.md) führt durch eine isolierte
menschliche Spielsitzung mit echtem Ausbau, Lieferankünften und vollständigem
Neustart. Der [OE58-Nutzerbericht](../qa/oe58-human-playtest.md) bestätigt
konkrete Start-, Bau-, Inspector- und Einzugsaktionen. Nicht einzeln berichtete
Untertests und die menschliche Abnahme der neuen Darstellung bleiben NOT_RUN.

## Start und Ausbau

Nach dem [Build](../../README.md) mit der eigenen Emperor-Installation starten:

```sh
./build/openemperor --data /pfad/zur/Emperor-Installation
```

Im Hauptmenü **Play Original economy** wählen. Das ist der empfohlene,
experimentelle Einstieg für eine **neue** Stadt mit der bestehenden
**Rule18 / Schema34 / Policy3**; es wird keine neue Regelgeneration angelegt.
Datenordner, **Cities/Xia.map**, **Normal** und **Paid four-house starter**
prüfen, dann **Start sandbox** wählen. Alternativ beginnt **Empty city** ohne
Starterbauten. Die Stadt beginnt pausiert.
**Space** startet die Simulation; **1x / 2x / 4x** unten ändern ihr Tempo.

Ein bestandener **Map Rules Check** prüft die allgemeinen Kartenregeln.
Der bezahlte Starter braucht zusätzlich eine freie, legale ebene **24×13-Fläche**.
Fehlt sie, zeigt **Paid starter unavailable** die Ursache vollständig an.
**P Use Empty city** wählt eine leere Stadt auf derselben Karte; nach der
erneuten Kartenprüfung mit **Start sandbox** starten. **Choose another map**
kehrt zur Kartenauswahl zurück. Die Karte wechselt dabei nicht automatisch.

Ein gekauftes, noch unbewohntes Common-Haus ist **Vacant / Type2**. Sein
authored Baugrundstück zeigt den tatsächlichen 2×2-Footprint; es ist noch kein
bewohntes Zelt. Erst echter Einzug aktiviert **Shelter / Type3** und dessen
bereits unterstützte Originalgrafik. Die native Type2-Grafikzuordnung bleibt
ungeklärt; siehe [Darstellungsvertrag](../reverse/original-house-visuals.md).

Der [OE48-Bauplan](oe48-citywide-fire-reliability.md) erreicht mit unveränderter
Rule18 eine bezahlte Stadt mit 13 bewohnten Häusern und 5200/5200 erfüllten
aktuellen Bedürfnissen im geprüften 20400-Tick-Fenster. **Fünf Hausbrände bleiben
real**; weder unbegrenzte Stabilität noch vollständige Originalparität oder
menschliche Abnahme folgen daraus. Der Vier-Haus-Starter allein ist dieser
ausgebaute Bauplan nicht.

**L Load Save** lädt stattdessen einen bestehenden OpenEmperor-Spielstand;
**Enter Continue** erscheint nur für eine bereits im Speicher gehaltene Stadt.
Beides behält deren Regelgeneration. Fehlende Originaldaten oder eine nicht
zugelassene Karte verhindern den Start mit Diagnose. Ein fehlgeschlagener
Start/Ladevorgang ersetzt keine bestehende Stadt.

Unter **Advanced / Legacy** bleiben die bisherigen Experimente und Profile
einzeln wählbar. **O/F/S/M/W/I/D/T/U** und **N** funktionieren weiterhin als
Hauptmenü-Tastenkürzel. **V Safe demolition economy** ist eine zusätzliche,
separate Rule19-Auswahl für den unten beschriebenen begrenzten Abriss.
**N New Sandbox** behält den City-v11-Rule3-Standard und
die gespeicherte Profilwahl; gewöhnlicher City-v16 bleibt Rule4. Bridge-Rule5
und Ramp-Rule6 werden dort weiterhin ausdrücklich gewählt. Die Empfehlung
ändert keine gespeicherten Einstellungen oder alten Spielstände.

**O Original economy** wählt weiter die historische **Rule10 / Schema26**.
Für den separat versionierten Brandschutz **F Original fire
protection** wählen. Das startet **Rule11 / Schema27 / Policy3** mit derselben
bezahlten Stadt. **O Original economy** bleibt Rule10; bestehende Saves werden
nicht umgewandelt. Über **F FireWatch $80 +2 workers** bis zu zwei Wachen
regulär kaufen und mit Straßen verbinden. Jede Wache benötigt zwei tatsächlich
zugeteilte Mitarbeiter und laufenden Betrieb für neue Inspector-Fahrten.

Erst die echte Ankunft eines Inspectors löscht einen Brand und schützt das
besuchte Gebäude für 2400 Ticks. Bei **Select** zeigen Gebäude und Häuser
Brandstatus und verbleibenden Schutz; die Wache zeigt Betrieb und Staffing,
Walker die laufende Fahrt. Wachen können selbst brennen und brauchen die andere
Wache als Besucher. Betriebspausen stoppen neue Fahrten, bereits entsandte
Inspectors beenden ihre Fahrt. Brandschutz beseitigt das Wirtschaftsdefizit
nicht; Kaufpreis und laufender Unterhalt kommen hinzu.

**S Sustainable economy** startet separat **Rule12 / Schema28 / Policy3**.
Hier sind drei regulär bezahlte Food-Farmen mit jeweils eigenem Feld erlaubt;
der Unterhalt von 2 Funds pro benötigtem Mitarbeiter fällt alle 816 statt 400
Ticks an. Alle Originalmodellwerte, Steuern, Erntezeiten und Brandschutzregeln
bleiben erhalten. Alte Städte behalten ihre Regeln. Für den gezielten
[Zwölf-Häuser-Ausbau](oe37-sustainable-economy.md) **Empty** wählen; der
Vier-Häuser-Starter ist weiterhin eine eigene bezahlte Startoption.
Der geprüfte Ausbau hält zwölf bewohnte Häuser und 318 Einwohner über 100000
Ticks nach der Anlaufphase; vier volle 20400-Tick-Fenster erwirtschaften je 1950
Funds nach sämtlichem Unterhalt. Das belegt diesen begrenzten Bauplan, keinen
Gewinn für jede Stadt oder einen vollständigen Aufstieg zu höchsten Hausstufen.

**M Needs-aware logistics** startet ausdrücklich **Rule13 / Schema29 / Policy3**.
Der Modus behält die drei Food-Ketten, den 816-Tick-Unterhalt, Brandschutz,
Preise und Transportgrenzen von Rule12. Warehouse und Market wählen Waren
nach tatsächlichem Hausbedarf: fehlender aktueller Bedarf zuerst, danach
Vorrat und erfüllbare nächste Stufen. Erst echte Lieferankünfte füllen die
Häuser. Die Auswahl ist eine eigene OpenEmperor-Regel; bestehende Rule12-Städte
werden nicht umgestellt. Der direkte Start mit bezahltem Starter lautet:

```sh
./build/openemperor --data /pfad/zur/Emperor-Installation --sandbox Cities/Xia.map --sandbox-rules sandbox-city-v16 --original-market-logistics
```

Die [Marktdiagnose](oe39-market-diagnosis.md) erklärt die gemessenen Rule12-Lücken.
Der dort [geprüfte bezahlte Ausbau](oe39-market-retention.md) mit näherer Töpferei
hält Common10 von Tick22837 bis43237. Alle zwölf Häuser bleiben im Fenster ohne
aktuellen Fehlbedarf oder Rückentwicklung; der Betrieb erwirtschaftet4360 Funds.
Dabei sinken die angesparten Hemp-Bestände von94 auf67. Unbegrenzte Versorgung,
eine brandsichere Gesamtstadt und die menschliche Abnahme bleiben offen.

**W Cardinal-water economy** wählt separat **Rule14 / Schema30 / Policy3**.
Dieser Modus übernimmt Rule13 und ergänzt den belegten Haus-DES-Bonus von10
bei Wasser direkt am Rand des tatsächlichen Gebäudes. Diagonales Wasser zählt
nicht. Der State-Inspector zeigt Zellwert, Wasserbonus und Gesamtsumme getrennt.
Ein leerer Elite-Plot erhält keinen Bonus vor seiner legalen4×4-Materialisierung.
Karte und Modell bleiben unveränderte Quellen; ältere Saves werden nicht
umgestellt. Ein geprüfter, regulär bezahlter Xia-Ausbau erreicht Elite17 bei
Tick7333 mit DES70+10, erfüllten aktuellen/nächsten Bedürfnissen und 4455 Funds.
Die folgenden 20400 Ticks halten Versorgung und Brandsicherheit nicht durch;
der Betrieb verliert 3241 Funds, obwohl 1214 Funds übrig bleiben. Auch zwei
zusätzliche bezahlte Märkte sichern den Aufstieg im separaten Versuch nicht.
Das ist kein dauerhaft selbsttragender Musterbauplan. Die
[Quellenprüfung](oe40-desirability-investigation.md) trennt Aufstieg,
Versorgung, Finanzierung und offene menschliche Abnahme.
Der erfolgreiche Testplatz liegt am Rand des gerenderten Kartendiamanten.
Seine beiden auslösenden gespeicherten Randfelder enthalten`0x80005`, also
Wasserbit4 und die Off-map-Markierung. Rule14 prüft das rohe gespeicherte Bit;
der Erfolg belegt damit diesen Snapshot-Adapter, kein sichtbares Ufer und keinen
Elite17-Aufstieg im Originalspiel. Originale Änderungen nach dem Kartenladen
bleiben ungeklärt.
Direktstart: dieselbe CLI wie oben mit `--original-cardinal-water` statt
`--original-market-logistics`.

**I Inspector coordination** wählt ausdrücklich den experimentellen Modus
**Rule15 / Schema31 / Policy3**. Er übernimmt Rule14 und vermeidet neue
Inspector-Fahrten zu nichtbrennenden, noch geschützten Gebäuden oder einem Ziel, zu dem ein
anderer Inspector tatsächlich unterwegs ist. Schutz entsteht weiterhin erst
bei Ankunft. Warten auf eine reparierte Straße reserviert kein Ziel; schon
fahrende Inspectors beenden ihre Fahrt auch bei pausierter Wache.
Direktstart: `--original-inspector-coordination` statt des anderen Originalmodus.
Preise, Bedürfnisse, Brandfristen und Unterhalt bleiben erhalten. Die bisherigen
[OE44-Versuche](oe44-elite17-stability.md) halten Elite17 auch hier nicht
unterbrechungsfrei; der Modus ist kein dauerhaft stabiler Musterbauplan.

**D Deadline-aware fire dispatch** wählt separat den experimentellen Modus
**Rule16 / Schema32 / Policy3**. Die Wachen vergleichen echte Reisezeiten,
Schutzablauf und Brandrisiko für alle zugelassenen bezahlten Gebäudeklassen.
Physisch rechtzeitig erreichbare Ziele kommen zuerst; unter ihnen entscheidet
der knappste zeitliche Spielraum. Bei Gleichstand folgen frühere Frist,
kürzere Hin-/Rückfahrt und die bestehende Zielrotation. Ein bereits fahrender
Inspector vermeidet eine neue Fahrt, wenn seine tatsächliche Route eine
rechtzeitige oder mindestens ebenso frühe Ankunft erwarten lässt. Wartende oder
noch zu reparierende Routen versprechen keinen Besuch. Ein brennendes Gebäude
hat keinen pauschalen Vorrang und das Elite-Haus keinen Sonderbonus.

Erst die tatsächliche Ankunft löscht beziehungsweise schützt. Laufende Fahrten
bleiben bei einer späteren Wachenpause erhalten. Modellwerte, Bedürfnisse,
Preise, Gebäude-/Walkergrenzen, Bewegung, Brand- und Schutzzeiten sowie der
816-Tick-Unterhalt ändern sich nicht; Karten-/Terrainbindung und bedarfsabhängige
Warenlogistik bleiben übernommen. Rule16 hat eigene Create-/Restore-Pfade und
den exakten Marker `authored-deadline-fire-dispatch-v1`; Rules1–15 und ihre Saves
werden nicht umgewandelt. Direktstart mit dem regulär bezahlten Starter:

```sh
./build/openemperor --data /path/to/data --sandbox Cities/Xia.map --sandbox-rules sandbox-city-v16 --original-deadline-fire-dispatch
```

Der [OE45-Ergebnisbericht](oe45-fire-coverage.md) belegt den vollständig bezahlten
Near-Watch/Grid-Plan: echter16→17-Aufstieg bei Tick5446, danach20400Ticks mit
400/400 erfüllten aktuellen Bedürfnissen, ohne Zielbrand, Rückstufung oder
Leerstand. Der operative Überschuss beträgt4951Funds, der niedrigste Kontostand
5017. Alle zwölf übrigen Häuser bleiben bewohnt; dort bestehen weiterhin
Versorgungsdefizite, Rückstufungen und Brände in der Stadt. Unbegrenzte
Stabilität und menschliche native Abnahme bleiben offen.

**T Deadline-aware services** wählt **Rule17 / Schema33 / Policy3** mit
fristabhängiger Auswahl tatsächlicher Dienstbesuche. **U Deadline-aware food**
wählt **Rule18 / Schema34 / Policy3**, also denselben Modus wie der empfohlene
Einstieg. Nur die Auswahl von Market→House-Food-Lieferzielen kommt gegenüber
Rule17 hinzu; Farm→Market, Mengen, Kapazitäten, Bewegung, Verbrauch und Unterhalt
bleiben erhalten. Tatsächliche Ankunft erfüllt Versorgung beziehungsweise
erneuert bestehende Besuchstimer. Die
[OE46-](oe46-citywide-reliability.md) und
[OE47-Auswertungen](oe47-food-reliability.md) halten Grenzen der jeweiligen
Generation getrennt. Direkter Rule18-Start:

```sh
./build/openemperor --data /pfad/zur/Emperor-Installation --sandbox Cities/Xia.map --sandbox-rules sandbox-city-v16 --original-deadline-food-dispatch
```

Der [bezahlte High-Tier-Ausbau](oe38-paid-high-tier-progression.md) erreicht
Common10 (Luxurious Apartment). In den geprüften20400 Folgeticks entstehen
Hemp-/Ceramics-Lücken und Rückentwicklungen trotz positivem Betriebssaldo;
ein zweiter Markt allein behebt sie nicht. Elite17 ist im unveränderten
Normal-Rule12-Vertrag geometrisch blockiert: Die zugelassenen Quellen und
Footprints erlauben höchstens DES79, der Aufstieg16→17 verlangt80.

Der Starter bezahlt Straßen, vier Häuser, Wasser, Ancestor, Market, Food-/Hemp-
Ketten, Clay/Pottery/Warehouse, Herbalist, Acupuncture und Music. Mit der
geprüften Modelldatei bleiben **10596 von 12000 Funds**. Bewohner und Waren
beginnen bei **0**: Zuzug, Arbeit, Produktion und tatsächliche Ankünfte folgen
erst beim Spielen. Eine gebaute Quelle erfüllt allein keinen Hausbedarf.

Der Bauplan braucht eine legale ebene **24×13-Fläche**. Scheitert die Suche,
eine andere Karte oder **Empty** wählen. Für das Wohnziel sind mindestens
sechs Häuser nötig; zusätzliche Häuser und Versorgung regulär kaufen.
Auch pausierte Gebäude kosten Unterhalt. Häuser und Produktionsgebäude bleiben
in Rules10–18 nicht abreißbar; dort gilt Abriss nur für die bestehenden neutralen
Well72-Einflussquellen.

Der [geprüfte Ausbauplan](paid-progression.md) ergänzt zwei Häuser, vier Straßen
sowie je eine Food-/Hemp-Farm mit Feld für 266 Funds. Er belegt das Wohnziel und
ein begrenztes Versorgungsfenster; Brände und laufende Verluste bleiben offen.

## Begrenzter Abriss und Wiederaufbau

Für eine **neue** Stadt unter **Advanced / Legacy → V Safe demolition economy**
ausdrücklich **Rule19 / Schema35 / Policy3** wählen. Der empfohlene Einstieg
bleibt Rule18; Laden oder F5/F9 wandelt keine ältere Stadt um. Direktstart:

```sh
./build/openemperor --data /pfad/zur/Emperor-Installation --sandbox Cities/Xia.map --sandbox-rules sandbox-city-v16 --original-safe-demolition
```

Mit **5 Select** das regulär gekaufte Gebäude wählen. Der Inspector zeigt
**Demolish** und den tatsächlichen Freigabe- oder Ablehnungsgrund. V1 erlaubt
nur ein nie bewohntes, vollständig leeres **Common2×2-Haus** sowie eine
**pausierte ClaySource** ohne Teilproduktion, Vorrat, Reservierung oder laufende
Fahrt. Bereits gelieferter Clay bleibt am echten Ziel; der eigene Kurier muss
vollständig heimgekehrt sein. Zusätzliche historische Verweise können den
Abriss weiterhin verhindern. Ein lediglich pausiertes oder nahe einem anderen
Gebäude stehendes Objekt ist nicht automatisch leer.

**Demolish → Demolish (Enter)** bestätigt einmalig. **Cancel / Esc**, Fokusverlust
oder eine Größenänderung brechen ab. Es gibt **keine Rückerstattung**. Auf den
freigewordenen zulässigen Zellen mit dem normalen Bauwerkzeug neu bauen:
regulärer Kaufpreis, neue ID, keine geerbten Bewohner oder Waren.
Bewohnte oder früher bewohnte Häuser, Elite-Plots/4×4-Elites, Lager, Märkte,
Farmen/Felder und andere Produktion/Dienste bleiben gesperrt. Der gespeicherte
Abrissnachweis fasst höchstens acht neue Entfernungen; vorhandene Gebäude- und
Kurierbudgets gelten weiter. Neutrale Well72-Entfernung behält ihren bisherigen
separaten Vertrag. Details und Abnahmegrenzen:
[OE51-Abrissvertrag](../gameplay/oe51-demolition.md).

## Musikschule und Veranstaltungsstätte

Unter **Advanced / Legacy → B Music venues (rule 22)** lässt sich ausdrücklich
eine neue **Rule22 / Schema38 / Policy3**-Stadt starten. Der empfohlene Einstieg
bleibt Rule18. **E Music** kauft die Musikschule, **G Theatre75** das separat
bezahlte 4×4-Theatre; Toolbar und Tastatur wählen dieselben Bauwerkzeuge.
Beide Gebäude benötigen legale Straßen und ihre tatsächlich zugeteilten
Mitarbeiter. Erst ein wirklich gespawnter Performer mit eigener ID läuft von
der Schule zum Theatre. Eine erfolgreiche Ankunft setzt dessen eigenen
Musik-Zustand auf 32; der Performer endet dort ohne sichtbare Rückfahrt.

Der Inspector trennt Spawnversuche, aktive Performer, tatsächliche Ankünfte und
den verbleibenden Venue-Timer. Die Darstellung des Theatre und Performers ist
ein eigener Fallback. **Dieser Modus versorgt keine Häuser mit Musik**: Weder
Versand noch Venue-Ankunft setzt den Music96-Haustimer. Damit können
musikabhängige Hausbedürfnisse unerfüllt bleiben. Die bisherigen direkten
School→House-Besuche in Rule18 bleiben unverändert. Actor36, weitere Venue-Arten,
Festivals und neue Religionsaktionen sind nicht freigeschaltet. Die Musikschule
behält ihren bisherigen physischen DES-Beitrag aus dem vorbereiteten Modell;
dieser ist vom Music96-Haustimer getrennt. Das Theatre erhält keinen neuen
DES-Beitrag. Die begrenzte Kette ist automatisiert im World-, Speicher- und
geskripteten SDL-Pfad geprüft; ihre native und menschliche Abnahme bleibt
NOT_RUN. Vertrag und Prüfpfad stehen im
[Musik-/Venue-Leitfaden](oe56-culture-venues.md).

## Bedienung und Versorgung prüfen

- **Pfeiltasten** verschieben, Mausrad oder **Z** zoomen; **H / Help** erklärt
  die Bauwerkzeuge. **.** schaltet bei Pause einen einzelnen Tick weiter.
- **5 Select** wählt Gebäude. Beim Haus wechselt **PageUp / PageDown** zwischen
  **State / Needs / Goods / Visits**. Dort stehen aktuelle/nächste Anforderungen,
  vorhandene Bestände und tatsächliche Besuche. **F10** wechselt die Overlays
  **Off / Supply / DES / Risk**.
- **F4** vergleicht Gebäudegrafik und Fallback, **F6** Straßenmaterial.
  Benannte Fallbacks zeigen noch fehlende Originalgrafiken.
- Für einen Versorgungstest beide Ancestor-Gebäude mit **Pause operation**
  pausieren. Nach ausbleibenden Besuchen und negativen Hausauswertungen folgt
  Rückentwicklung; **Resume operation** und verbundene Straßen erlauben neue
  Besuche. Timer und Hausentwicklung reagieren verzögert.

## Speichern und wiederherstellen

**F5** speichert außerhalb der Originaldaten, **F9** lädt denselben Stand
pausiert. Nach einem Neustart **L Load Save → Open selected** wählen.
Difficulty, Modell- und Kartenquelle müssen passen. Alte Entwicklungsrevisionen
werden mit Diagnose verweigert; ein fehlgeschlagener Ladevorgang erhält die Stadt.
Die Ladeansicht zeigt Profil, Rule und Schema zur Diagnose. Rules10–18 laden
jeweils ihren eigenen Stand mit Schema26–34 und den exakten Vertragsmarkern.
Es gibt keine Upgrade-Funktion für Originalwirtschafts-Saves.
F5/F9 und Recovery ändern dabei keine Regelgeneration.
Der neue Versuch **Advanced / Legacy → K Venue House music (23)** startet
getrennt eine **Rule23 / Schema39 / Policy3**-Stadt. Der direkte Start lautet:

```sh
./build/openemperor --data /path/to/game --sandbox Cities/Xia.map --sandbox-rules sandbox-city-v16 --original-venue-house-service
```

Die bezahlte Schule liefert weiter einen tatsächlichen Performer an ein
bezahltes Theatre. Erst vorhandene Music und vier berechtigte, voll besetzte
Source-Aufrufe können einen eigenen Venue-Walker erzeugen. Dieser läuft über
eine echte Straße zu einem bewohnten Haus und physisch zurück. Nur eine gültige
Ankunft mit **dann noch vorhandener** Theatre-Music setzt House-Music auf96;
Bau, Nähe, angekündigte Route, Zeichnen, F5/F9 und Laden erzeugen keine Versorgung.
Bei Ablauf, pausierter/unbesetzter/brennender Quelle oder unzulässigem Haus
bleibt die Gutschrift aus. Straßenunterbrechungen behalten begonnene Kanten und
das normale Warten; Reparaturen werden regulär bezahlt. Ein zurückgekehrter
Walker ist beendet, der nächste bekommt eine neue Identität.

Die Schule zeigt Personal, Versuche und tatsächliche Performer. Das Theatre
zeigt aktuelle Music, Walker-ID, Ziel, Route und tatsächliche Ankunft; die
House-Seite **Visits** zeigt den echten Theatre-/Walker-/Performer-Beleg.
Theatre und Walker verwenden gekennzeichnete authored Fallbackdarstellung.
Zielwahl, Hin-/Rückfahrt und World-Zeitbasis sind ausdrücklich authored;
vollständiges originales Wandern, Festivals und Original-Save-Import fehlen.
Eine regulär bezahlte Xia/Normal-Probe mit fünf Häusern belegt zwei angenommene
School→Theatre-Zyklen, einen vollständigen Venue-Walker-Hin-/Rückweg und weitere
20400 aktive Ticks mit mindestens fünf bewohnten Häusern/110 Einwohnern und
74 weiteren Music-Ankünften. Das ist eine endliche Probe; die Plain-Stufe
entstand bereits vor Music, und vollständiger Brandschutz oder höhere
Music-abhängige Entwicklung sind damit nicht belegt. Native-/Human-Abnahme
bleibt NOT_RUN.
Rule18 bleibt empfohlen. Rule22/Schema38 und ältere Städte bleiben exakt in
ihrer eigenen Generation. Belege und Grenzen stehen im
[OE57-Leitfaden](oe57-venue-house-service.md).

Der Rule22-Musik-/Venue-Modus lädt ausschließlich seinen eigenen Rule22-/Schema38-Stand
mit dem Marker `authored-music-venue-v1`; alte Städte werden nicht umgewandelt.

**Autosave: ON** im Hauptmenü aktiviert Recovery: ein geschützter Startpunkt
und bis zu fünf Checkpoints im Abstand von **60 simulierten Sekunden**.
**Load Save → Show recovery history → Open selected** öffnet einen Checkpoint
pausiert in einer eigenen Historie und erhält seine Quelldatei. Erfolgreiches
F9 beginnt ebenfalls eine neue Historie; manuelle Saves bleiben separat.

## Grenzen und technische Einordnung

Originalwirtschaft bleibt ausdrücklich optional und experimentell. Der
empfohlene Einstieg verwendet die bestehende **City-v16 Rule18 / Schema34 /
Policy3** mit Wirtschaftsrevision27. **O** behält den historischen
**Rule10 / Schema26 / Policy3 / Entwicklungsrevision27**-Vertrag.
Frühere Regeln und Saves behalten ihre
Verträge. Modelldaten werden beim Start vorbereitet; Originaldateien bleiben
unverändert. [Modellherkunft](model-provenance.md) und
[Kernvergleich](core-parity.md) dokumentieren die belegten Regeln.

Rule11 nutzt dieselbe Wirtschaftsrevision27 und einen zusätzlichen authored
Brandschutzvertrag mit eigener Save-Prüfung. Die Preise und Mitarbeiter der
FireWatch stammen aus OpenEmperor, nicht aus verifizierten Originalwerten.
Ihre begrenzte Besuchs-/Steuerhistorie kann voll werden; dann pausiert die Stadt
vor dem nächsten Tick mit Diagnose, statt einen unprüfbaren Zustand zu erzeugen.
Der letzte Stand bleibt speicherbar. Grenzen und Messwerte stehen im
[Brandschutzvertrag](oe36-fire-protection.md).
Rule12 verwendet denselben begrenzten Brandschutz und eine eigene Save-Version;
auch seine sparsamere Finanzierung garantiert keinen Gewinn für jeden Ausbau.
Rule13 übernimmt diese Grenzen. Seine bedarfsabhängige Warenauswahl erhöht weder
Hauslager noch Kurierzahl, Transportmenge, Geschwindigkeit oder Brandschutz.

Rezepte, Ernte, Zuzug/Worker und Schule→Haus-Besuche enthalten eigene Regeln.
Zusätzliche Modellnamen für Landwirtschaft und Industrie sind keine neuen
Bauwerkzeuge. Die [OE55-Quellenprüfung](oe55-production.md) bleibt Forschung;
sie schaltet keine weitere Produktionskette frei. Auch Saison, Jagd und Fischerei
sind damit nicht als Originalverhalten umgesetzt.
Rule22 ergänzt nur die begrenzte School→Performer→Theatre-Kette. Der separate
Rule23-Versuch ergänzt echte Theatre→House-Music-Besuche mit authored Zielwahl
und Rückweg. Vollständige originale Veranstaltungsabläufe, Festivals,
Hero-/Götter-AI und Krankheit fehlen weiterhin. Mehrere Gebäude und Walker nutzen Fallbackbilder; passende
Hausbilder decken Typen3–17
ab. Common10 und separat in Rule14 auch Elite17 wurden bezahlt erreicht.
Der beschriebene DES79-Blocker bleibt für Rule12/13 unverändert; Rule14s
Elite17-Versorgung und Finanzierung sind noch nicht dauerhaft nachgewiesen.
Die native menschliche Abnahme bleibt offen.
Der unveränderte Vier-Haus-Plan ohne
Ausbau verlor im gemessenen100k-Lauf Geld und verpasste Food-Bedarfe; er ist
keine dauerhaft selbsttragende Musterstadt. Weitere Grenzen stehen in
[Known Issues](../../KNOWN_ISSUES.md).

Für einen Fehlerbericht Karte, Difficulty, Tick, Gebäude/Tool und Aktion nennen;
bei Grafikproblemen Screenshot und Zoom ergänzen.
