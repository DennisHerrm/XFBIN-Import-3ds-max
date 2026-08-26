# Changelog

## 1.9.2 - Sichtbarkeits-Keys entstehen jetzt wirklich

### Warum viele Sequenzen keine hatten

`INode::SetVisibility` legt **keinen Key an, wenn sich der Wert nicht
aendert.** Fuer eine Sequenz, in der ein Mesh durchgehend unsichtbar
ist, entstand damit gar nichts - und genau die braucht der
Warcraft-3-Export: je Sequenz einen Key am Anfang und einen am Ende,
auch wenn dazwischen nichts passiert.

Die drei Faelle waren also richtig gedacht, aber zwei davon kamen nie
in der Szene an.

### Der Weg ueber IKeyControl

Jetzt wird der Key angehaengt, ohne dass jemand den Wert vergleicht -
dasselbe Verfahren, das das Animation Merge Tool fuer die Bones
benutzt:

    Control* vc = node->GetVisController();
    if (!vc) { vc = CreateInstance(CTRL_FLOAT_CLASS_ID,
                                   HYBRIDINTERP_FLOAT_CLASS_ID);
               node->SetVisController(vc); }
    IKeyControl* ik = GetKeyControlInterface(vc);

    IBezFloatKey k;
    k.time = t; k.val = v;
    SetInTanType(k.flags,  BEZKEY_STEP);
    SetOutTanType(k.flags, BEZKEY_STEP);
    ik->AppendKey(&k);

Zwei Dinge kommen dabei gratis dazu:

- Die **Step-Tangenten** stehen direkt im Key. Der
  MaxScript-Nachlauf bleibt als Sicherheitsnetz, ist aber nicht mehr
  noetig.
- Kein `AnimateOn` mehr - `AppendKey` schreibt unabhaengig vom
  Animationsmodus.

`AppendKey` verlangt aufsteigende Zeiten. Der Sequenzmodus haelt das
ein; wer eine Animation einzeln nachtraegt, nicht. Deshalb laeuft am
Ende ein `SortKeys()` ueber jede Spur - das bringt sie wieder in
Ordnung, statt sie stillschweigend zu verderben.

### Neu: tools/VIS_CHECK.ms

Liest die Sequenzen aus dem Note Track des Szenen-Wurzelknotens und
prueft fuer JEDES Mesh, ob im Bereich JEDER Sequenz mindestens zwei
Sichtbarkeits-Keys liegen. Meldet die ersten fuenfzehn Fehlstellen mit
Objekt- und Sequenznamen.

Bei 632 Meshes und 104 Sequenzen sind das rund 65.000 Paarungen - von
Hand nicht zu pruefen, und mit dem Auge im Viewport erst recht nicht.


## 1.9.1 - Zwei Fehler, die nur das 2016er SDK zeigt

### std::map fehlte im Header

    error C2039: "map" ist kein Member von "std"

`sceneMaterials_` steht als `std::map` im Header, das `#include <map>`
stand aber nur in der `.cpp` - und die bindet den Header VOR ihren
eigenen Includes ein. Auf den neueren SDKs faellt das nicht auf, weil
deren Header `<map>` ohnehin mitziehen; das SDK von 2016 tut das nicht.

Ein Header muss haben, was er benutzt. Sonst haengt er davon ab, was
zufaellig vorher eingebunden wurde.

### GetParamBlock ist verdeckt

    error C2660: "BaseObject::GetParamBlock" akzeptiert keine 1 Argumente
    error C2440: "IParamArray*" nicht in "IParamBlock2*" konvertierbar

`Object` erbt von `BaseObject` ein parameterloses `GetParamBlock()`,
das ein `IParamArray*` liefert - die alte Parameterblock-Fassung. Diese
Ueberladung **verdeckt** `Animatable::GetParamBlock(int)`, der
Compiler findet die gewuenschte Fassung also gar nicht mehr.

Dieselbe Stelle ist in einem Forumsfall zum V-Ray-SDK beschrieben,
mit derselben Antwort: `GetParamBlockByID()` benutzen. Der kommt direkt
von `Animatable` und wird nicht verdeckt. Als Rueckfallebene steht der
ausdruecklich qualifizierte Aufruf `Animatable::GetParamBlock(0)`
dahinter, und `FindBaseObject()` sorgt dafuer, dass wirklich das
Basisobjekt gefragt wird.

### Neu: tools/PRUEFE_INCLUDES.py

    python tools\PRUEFE_INCLUDES.py src\*.h src\*.cpp

Prueft fuer sechzehn haeufige `std`-Typen, ob die Datei den passenden
Header einbindet. Eine `.cpp` erbt dabei die Includes ihres
gleichnamigen Headers - sonst meldet die Pruefung lauter Treffer, die
keine sind.

Gegengeprueft: mit entferntem `#include <map>` findet sie genau die
Zeile, die den Build gekostet hat. Dabei kam gleich noch ein fehlendes
`<utility>` heraus.

Damit sind es vier Pruefungen, die vor dem Ausliefern laufen:

| Werkzeug | prueft |
|---|---|
| `PRUEFE_SCRIPTS.py` | die MaxScript-Dateien |
| `PRUEFE_API.py` | Plugin-Funktionen an allen drei Eintragungsstellen |
| `PRUEFE_INCLUDES.py` | std-Typen gegen ihre Header |
| `VERSION_SETZEN.py` | alle Versionsstellen auf einmal |

Keine davon braucht 3ds Max.


## 1.9.0 - Material-Animationen

Die letzte offene Stelle aus der Liste bekannter Einschraenkungen.

### Was in den Daten steht

Ein Material-Eintrag animiert achtzehn Groessen. Ueber die vier
Animationsdateien von Pein sind das 868 Kurven je Groesse, aber die
meisten haben genau einen Key - sie sind konstant:

| Kurve | Keyframes | |
|---|---|---|
| `V0_LocY` | 4.371 | animiert |
| `U0_LocX` | 3.153 | animiert |
| `V1_LocY` | 1.103 | leicht animiert |
| `U1_LocX` | 1.023 | leicht animiert |
| `Alpha` | 954 | leicht animiert |
| `Glare`, `OutlineID`, `U3/V3` u. a. | je 868 | konstant |

Bewegt wird also im Wesentlichen der Offset der ersten UV-Ebene:
klassisches UV-Scrollen fuer Augen, Haare und Effektflaechen.

### Was uebertragen wird

| XFBIN | 3ds Max |
|---|---|
| `U0_LocX` / `V0_LocY` | Offset der Bitmap (`StdUVGen::SetUOffs` / `SetVOffs`) |
| `U0_ScaleX` / `V0_ScaleY` | Kachelung der Bitmap |
| `Alpha` | Deckkraft des Materials (`StdMat2::SetOpacity`) |

Alle vier Methoden nehmen eine `TimeValue` entgegen, im
`AnimateOn`-Block entstehen daraus also Keys - dieselbe Mechanik wie
bei den Bones.

Nicht uebertragen: `Glare`, `Falloff`, `BlendRate1/2`, `OutlineID`.
Das sind Groessen des Shaders der Spiel-Engine, fuer die es in Max
nichts Entsprechendes gibt. Sie werden gezaehlt und gemeldet - aber nur
wenn sie tatsaechlich animiert sind, sonst waere die Meldung bei jeder
Sequenz da.

### Das Vorzeichen von V

Beim Mesh-Import wird die V-Achse gespiegelt (`1-v`), weil Max die
Textur andersherum aufzieht. Damit ist ein Versatz von `+dv` in der
Datei ein Versatz von `-dv` in Max:

    v_max  = 1 - v_datei
    v_max' = 1 - (v_datei + dv) = v_max - dv

Der V-Offset wird also negiert. Ohne das liefe eine scrollende Textur
in die falsche Richtung - und zwar so plausibel, dass es beim
Draufschauen kaum auffaellt.

Die Kachelung bleibt unveraendert: bei einer gespiegelten Achse laesst
sich eine Skalierung nicht sauber uebertragen, und in diesen Daten ist
sie ohnehin konstant.

### Neu in der Oberflaeche

Haken **"Material anim"** neben "Visibility", standardmaessig an. Wirkt
im Sequenzmodus wie beim einzelnen Setzen.

Dafuer merkt sich das Plugin jetzt, welches Max-Material aus welchem
XFBIN-Material entstanden ist - sonst faende die Animation ihr Ziel
nicht wieder.


## 1.8.1 - Bone-Groesse und Sichtbarkeit an der richtigen Stelle

### Warum manche Bones mit 4 kamen

Die Groesse wurde bis hierher in einem MaxScript-Nachlauf gesetzt, der
nach dem Import ueber alle Objekte lief. Beim Umbau des Imports in
1.6.0 ist der **Aufruf verlorengegangen** - die Funktion stand noch da
und haengte weiter am Spinner, aber der Import rief sie nicht mehr auf.
Wer den Spinner nicht anfasste, bekam Max' Standardwert 4.

Jetzt setzt das Plugin Breite und Hoehe direkt beim Anlegen des
Bone-Objekts:

    IParamBlock2* bpb = bobj->GetParamBlock(0);
    bpb->SetValue(0, 0, boneSize_);   // boneobj_width
    bpb->SetValue(1, 0, boneSize_);   // boneobj_height

`boneobj_width` und `boneobj_height` sind die ersten beiden Eintraege
des Parameterblocks - so steht die Aufzaehlung in der SDK-Referenz. Die
Oberflaeche gibt den Wert ueber `setBoneSize` weiter, bevor etwas
angelegt wird.

Der Unterschied ist nicht kosmetisch: ein Nachlauf kann vergessen
werden, eine Zeile im Erzeugungspfad nicht.

### Sichtbarkeit jetzt lueckenlos

Zwei Luecken geschlossen:

**Frame 0.** Die Bind-Pose hatte keinen Sichtbarkeits-Key. Damit hatte
der erste Key der ersten Sequenz keinen Vorgaenger, und Max zog seinen
Wert bis Frame 0 zurueck - schon die Ausgangslage war falsch.
`buildBindPoseKey` setzt jetzt alle Meshes dort auf sichtbar.

**Einzelne Animation.** "Apply selected" hat gar keine Sichtbarkeit
gesetzt; es stand alles gleichzeitig da. Jetzt laeuft dort dasselbe wie
im Sequenzmodus, inklusive der Step-Tangenten.

Im Sequenzmodus war es schon vorher so, wie es sein soll: **jedes Mesh
bekommt in JEDER Animation einen Key an beiden Enden** - aus der
Deckkraftkurve, oder auf 1 wenn der Clump vorkommt aber keine Kurve
hat, oder auf 0 wenn die Animation ihn gar nicht anspricht. Genau die
drei Faelle, die der NeoDex-Import auch unterscheidet.


## 1.8.0 - Layer und Bezier-Step-Sichtbarkeit

### Layer je Modell

Neuer Haken **"Sort into layers"**, standardmaessig an. Nach dem Import
liegt jedes Skelett in zwei eigenen Layern:

    2peabod1 Bones      2peabod1 Meshes
    2pecbod1 Bones      2pecbod1 Meshes
    2kycbod2 #2 Bones   2kycbod2 #2 Meshes
    ...

Bei siebzehn Skeletten und 1.793 Bones ist das der Unterschied zwischen
einer benutzbaren Szene und einem Knaeuel.

Die Zuordnung kommt aus dem Plugin - `layerReport()` liefert eine Zeile
je Knoten mit Clump, Instanz, Art und Handle. Gebaut werden die Layer
in MaxScript: dort sind es zwei benannte Aufrufe, in C++ waeren es
`ILayerManager` und `ILayerProperties`. Dieselbe Ueberlegung wie bei
der Bone-Groesse und beim Material-Editor.

### Sichtbarkeit als Bezier Step

Max schreibt Sichtbarkeits-Keys als `bezier_float` mit weichen
Tangenten - ein Objekt blendet also ueber. Das ist hier falsch: eine
Waffe ist da oder nicht, und Warcraft 3 kennt kein halb sichtbares
Objekt.

Uebernommen aus dem Visibility Keyer und dem NeoDex-Importer, die
beide dasselbe tun:

    o.visibility = bezier_float()
    local k = addNewKey c t
    k.value = v
    k.inTangentType  = #step
    k.outTangentType = #step

Nach dem Sequenzlauf bekommt jeder Sichtbarkeits-Key auf jedem Objekt
`#step` auf beiden Seiten - auch die Keys der nicht benutzten Waffen,
die durchgehend auf 0 stehen. Die Statuszeile nennt die Anzahl.

Das laeuft **einmal am Ende**, nicht je Animation: bei 104 Sequenzen
waere ein Durchlauf ueber alle Objekte pro Sequenz laenger als der
ganze Import.

### MAXScript kennt kein "continue"

Beim Layerbau hatte ich zwei `continue` stehen - gewohnt aus anderen
Sprachen, in MAXScript aber nicht vorhanden. Waere zur Laufzeit
aufgeschlagen, sobald eine Zeile des Berichts unvollstaendig ist.

Ersetzt durch verschachtelte `if`-Bloecke, und als siebte Pruefung in
`tools/PRUEFE_SCRIPTS.py` aufgenommen. Gegengeprueft: der Linter
findet ein eingebautes `continue`.


## 1.7.2 - Deskriptor-Eintraege ergaenzt

### Der echte Fehler

`clearAnims` und `parseAnimsAppend` standen nicht im
Interface-Deskriptor. Sie waren im enum und in der
`BEGIN_FUNCTION_MAP` eingetragen, aber nicht in der Liste, mit der
sich das Interface bei MaxScript anmeldet.

Das uebersetzt anstandslos - die Funktionen existieren im C++-Teil,
sie sind nur nicht veroeffentlicht. In MaxScript sieht das genauso aus
wie ein zu altes Plugin: "Unknown property". Ein Neubau haette den
Fehler NICHT behoben.

Eine Funktion muss an drei Stellen stehen:

| Stelle | Datei |
|---|---|
| `fn_`-Wert im enum | `xfbinimport.h` |
| Zeile in `BEGIN_FUNCTION_MAP` | `xfbinimport.h` |
| Eintrag im Interface-Deskriptor | `xfbinimport.cpp` |

### Neu: tools/PRUEFE_API.py

Vergleicht die drei Listen und meldet jede Funktion, die nicht ueberall
steht. Gegengeprueft mit absichtlich entfernten Eintraegen. Aktueller
Stand: 57 Funktionen, an allen drei Stellen.

Das ist die Pruefung, die den Fehler tatsaechlich gefunden haette - sie
liest den Quelltext, nicht den laufenden Zustand.

### Wieder entfernt: die Funktionspruefung zur Laufzeit

Zwischenzeitlich sollten die Skripte beim Start ueber `getPropNames`
abfragen, welche Funktionen das geladene Plugin kennt. Das war falsch:
`getPropNames` liefert EIGENSCHAFTEN, und die Funktionen dieses
Interfaces sind ueber `FN_0` und `FN_3` als METHODEN veroeffentlicht.
Die Liste kam leer zurueck, also galten alle neun als fehlend - auch
bei einem tagesaktuellen Plugin.

Die Doku sagt zwar, dass `getPropNames` mit Interface-Werten
funktioniert; sie sagt aber nicht, dass Methoden darin auftauchen. Ich
habe das nicht nachgeprueft, bevor ich eine Sperre darauf gebaut habe,
und damit ein funktionierendes Plugin blockiert.

Raus. Es bleibt bei der Anzeige der Plugin-Version in der Kopfzeile.

## 1.7.1 - Versionsnummern, die zusammenpassen

Beim Importieren:

    Unknown property: "clearAnims" in <Interface:XfbinCpp>

Ursache war einfach: die neuen Skripte waren installiert, das Plugin
aber nicht neu gebaut. `clearAnims`, `parseAnimsAppend` und
`buildVisibility` sind neu im C++-Teil, und `INSTALLIERE.bat` kopiert
nur - es baut nicht.

### Die Oberflaeche sagt es jetzt beim Start

Das Skript kennt die Plugin-Version, die es erwartet, und vergleicht
sie beim Oeffnen. Passt sie nicht, steht es in der Kopfzeile und in
der Statuszeile, mit dem naechsten Schritt dabei - statt mitten im
Import als "Unknown property" aufzuschlagen.

### Dabei aufgefallen: der Header stand auf 1.4.0

Bei 1.4.1 war die Aenderung reines MaxScript, also blieb die
Plugin-Version bewusst stehen. Alle folgenden Erhoehungen liefen dann
ueber "alte Nummer suchen, neue einsetzen" - und suchten eine Nummer,
die im Header gar nicht mehr stand. Der Ersetzungsvorgang fand nichts,
meldete aber auch nichts.

Ergebnis: das Fenster zeigte fuenf Versionen lang "Plugin 1.4.0",
waehrend Paket und Skripte weiterliefen. Nur kosmetisch, aber es haette
die neue Versionspruefung sofort blockiert.

### Neu: tools/VERSION_SETZEN.py

    python tools\VERSION_SETZEN.py 1.8.0

Setzt alle sechs Stellen auf einmal - Plugin-Header,
`PackageContents.xml`, beide Batchdateien, die Kopfzeilen der
MaxScript-Dateien und die vom Skript erwartete Plugin-Version. Vergibt
dabei einen neuen `ProductCode`, wie die Autodesk-Doku es fuer jede
Aenderung von `AppVersion` verlangt; `UpgradeCode` bleibt.

Entscheidend: es **setzt** die Stellen per regulaerem Ausdruck, statt
eine alte Nummer zu suchen. Steht eine Stelle aus irgendeinem Grund
woanders, wird sie trotzdem richtig gesetzt. Danach liest das Werkzeug
alle drei Leitstellen zurueck und meldet einen Fehler, falls sie
auseinanderlaufen - gegengeprueft mit einer absichtlich verstellten
Datei.


## 1.7.0 - Sichtbarkeit

Die Frage war, ob zwei Modelle in einer Animation korrekt dargestellt
werden. Die Bewegung ja - seit 1.6.0 stehen alle Skelette nebeneinander
in der Szene und werden gemeinsam angesteuert. Die Sichtbarkeit nicht.

### Was fehlte

Kanal 3 eines Bone-Eintrags ist die Deckkraft, und sie ist in diesen
Dateien das Mittel, mit dem Modelle erscheinen und verschwinden.
Nachgezaehlt in `2peabod1l`:

| Clump | wird ein-/ausgeblendet in |
|---|---|
| `2peabod1` (Pein selbst) | 11 Animationen |
| `2kyfbod1` | 8 |
| `2kycbod2` | 6 |
| `2pesword` | 5 |

Die Kurve wurde gelesen, aber nicht angewendet. Damit stand jedes
Modell in JEDER Sequenz sichtbar herum - bei 104 Sequenzen und
siebzehn Skeletten ein Bild, in dem der halbe Bosskampf gleichzeitig
auf der Matte steht.

### Was jetzt passiert

`buildVisibility <index> <start> <end>` unterscheidet drei Faelle:

| Lage | Ergebnis |
|---|---|
| Clump kommt in dieser Animation gar nicht vor | unsichtbar ueber die ganze Sequenz |
| Mesh-Bone hat eine Deckkraft-Kurve | deren Verlauf als Sichtbarkeit |
| Clump kommt vor, aber ohne Kurve | sichtbar, mit Key an beiden Enden |

Der Key an beiden Enden ist aus demselben Grund noetig wie bei den
Rest keys: sonst blendet Max zwischen zwei Sequenzen ueber.

In der Oberflaeche der Haken **"Visibility"** neben "Rest keys",
standardmaessig an.

### Dafuer noetig: sceneMeshes_

`meshNodes_` wird bei jedem `buildMeshes` geleert und kennt nur die
zuletzt geladene Datei - fuer `buildMaterials` reicht das, weil es
direkt danach laeuft. Die Sichtbarkeit braucht den ganzen Bestand ueber
alle sechs Modelldateien hinweg, mit Clump, Mesh-Bone und
Instanznummer je Objekt. Dafuer gibt es jetzt eine zweite Liste, die
nur `clearScene()` leert.


## 1.6.0 - Mehrere Animationsdateien, Instanzen je Clump

Ausgeloest durch einen Charakter, der deutlich groesser ist als der
bisherige Testfall: Pein bringt sechs Modelldateien mit siebzehn
Skeletten und **vier** Animationsdateien mit zusammen **104
Animationen** mit.

Daran sind zwei Annahmen zerbrochen.

### Nur eine Animationsdatei wurde geladen

Der Import nahm `aAnimFiles[1]` und liess den Rest liegen. Bei Haku
gab es nur eine, also fiel es nicht auf; bei Pein waeren 67 von 104
Animationen stillschweigend verschwunden.

Jetzt werden alle geladen. `parseAnimsAppend()` haengt an, statt zu
ersetzen, und `open()` wirft die bereits geladenen Animationen nicht
mehr weg - sie beschreiben nicht die Datei, sondern den geladenen
Bestand, genauso wie `sceneClumps_` die Szene beschreibt und nicht die
Datei. Derselbe Unterschied wie in 0.5.1 und 1.1.0, diesmal eine Ebene
weiter.

Geprueft: die 104 Animationsnamen sind ueber alle vier Dateien hinweg
eindeutig. Fuer die Note Tracks im Sequenzmodus heisst das, dass es
keine Namenskollisionen gibt.

### Ein Instanzwert je Datei reicht nicht

Bisher bekam `buildSkeletonN` eine Kopienzahl fuer die ganze Datei. Das
ging, solange eine Datei ein Skelett enthielt. `2peaacc2` enthaelt
vier, und der Bedarf ist unterschiedlich:

| Clump | Instanzen |
|---|---|
| `2kycbod2` | 3 |
| `2enmbod1`, `2enmhand`, `2kyfbod1` | je 1 |

Mit einem gemeinsamen Wert waeren zwangslaeufig zwei ungenutzte Kopien
der uebrigen drei entstanden.

`copies = 0` heisst jetzt: das Plugin sieht fuer JEDEN Clump selbst
nach, wie viele Exemplare die geladenen Animationen erwarten.
`buildMeshesN` folgt dem und baut ein Modell nur fuer die Instanzen,
die es auch gibt. Der ganze Umweg ueber `fileClumpName()` in MaxScript
entfaellt damit - die Information liegt im Plugin, dort gehoert die
Entscheidung auch hin.

### Reihenfolge im Import

Erst alle Animationsdateien, dann die Modelle. Vorher war es
umgekehrt, und dann steht beim Anlegen der Skelette noch nicht fest,
wie viele Exemplare gebraucht werden.

### Was dabei auffiel

Dreizehn der angesprochenen Clumps haben in diesem Ordner gar kein
Modell - `2pesword`, `1efc_dmy01`, `wrinkles`, mehrere
`2peaeff1_*` und andere. Das sind Effekte und Figuren aus anderen
Dateien; ihre Eintraege werden wie gehabt uebersprungen und gemeldet.
Vier Skelette aus `2peaacc3` bis `2peaacc5` werden von keiner
Animation angesprochen (Bosskampf-Gegner) und stehen nach dem Import
in Ruhelage.


## 1.5.2 - /permissive- wird ermittelt, nicht geraten

2016 bis 2018 bauten durch, 2019 nicht. Alle Fehler standen dabei in
den Headern von Autodesk, keiner im eigenen Quelltext:

    winutil.h(744)       C2216  "friend" nicht zusammen mit "static"
    ParamDimension.h(150) C2440  const wchar_t[1] nach wchar_t*
    inode.h(1709)        C2102  "&" erwartet L-Wert
    iTreeVw.h, maxapi.h  C2102  dasselbe, achtmal

Die Microsoft-Doku erklaert das Muster: `/permissive-` schaltet
`/Zc:referenceBinding`, `/Zc:strictStrings` und `/Zc:rvalueCast` auf
konformes Verhalten. `strictStrings` erzeugt die C2440,
`rvalueCast` die C2102. Aeltere SDK-Header sind aelter als diese
Regeln.

### Warum keine Jahresgrenze

In 1.5.0 stand `/permissive-` ab Max 2019 an - geraten, und falsch.
Ich haette jetzt auf 2020 raten koennen, dann auf 2021. Wo die Grenze
wirklich liegt, steht nirgends verbindlich; Autodesk nennt in den
Release Notes nur, mit welchem Visual Studio ein SDK gebaut wurde.

Also wird nicht mehr geraten. CMake uebersetzt beim Konfigurieren
einmal ein `#include <max.h>` mit `/permissive-` und schaut, ob es
klappt:

    check_cxx_source_compiles("#include <max.h>\nint main(){return 0;}"
                              XFBIN_SDK_LIKES_PERMISSIVE)

Klappt es, wird der Schalter gesetzt, sonst bleibt es bei `/W4`. Die
Konfigurationsausgabe sagt, was gewaehlt wurde. Das Ergebnis stimmt
fuer jedes SDK - auch fuer die, die es noch nicht gibt.

Zwei Eigenschaften, die dafuer sprechen: das Ergebnis liegt im
CMake-Cache, kostet also nur beim ersten Konfigurieren je Version ein
paar Sekunden. Und wenn die Pruefung aus einem ganz anderen Grund
fehlschlaegt, geht nur die Konformitaetspruefung verloren - der Build
laeuft trotzdem. Ein Fehlschlag in die sichere Richtung.

Erzwingen laesst es sich weiterhin:
`-DXFBIN_SDK_LIKES_PERMISSIVE=OFF` oder ganz ohne mit
`set XFBIN_STRICT=OFF`.


## 1.5.1 - Zwei API-Aenderungen zwischen 2016 und 2027

Der erste Lauf gegen das 2016er SDK meldete zwei echte Fehler und ein
Dutzend Folgefehler daraus.

### ExecuteMAXScriptScript hat mit Max 2022 die Signatur gewechselt

    error C2653: "MAXScript": Keine Klasse oder Namespace

Bis Max 2021 lautet sie laut Doku:

    BOOL ExecuteMAXScriptScript(MCHAR* s, BOOL quietErrors, FPValue* fpv)

Ab 2022 steht an zweiter Stelle ein `MAXScript::ScriptSource` - ein
Namensraum, den es vorher gar nicht gab. Jetzt eine Weiche ueber
`MAX_RELEASE` aus `plugapi.h`: 2016 = 18000, je Jahrgang 1000 mehr,
2022 = 24000. Der Wert kommt aus dem SDK selbst und ist damit immer
die Wahrheit ueber das, wogegen gerade uebersetzt wird - im Gegensatz
zu einem Jahr, das man von aussen hineinreicht.

### ClassDesc2 braucht sein Include

    error C2504: "ClassDesc2": Basisklasse undefiniert

Danach meldete jede der zwoelf `override`-Methoden einen eigenen
Fehler - alles Folgefehler derselben fehlenden Basisklasse.
`ClassDesc2` steht in `iparamb2.h`; die neueren SDKs ziehen den Header
ueber `max.h` mit herein, 2016 nicht. Include ergaenzt.

Dabei gleich mitgeguckt: `NonLocalizedClassName()` gibt es in
`ClassDesc` erst ab Max 2022. Davor waere `override` darauf ebenfalls
ein Fehler, also steht die Methode jetzt in derselben Weiche.

### Zusammenfassung des Buildscripts

Sie meldete zwoelfmal "FEHLT", auch fuer Jahrgaenge ohne installiertes
SDK. Ursache war ein geschachtelter `if`-Block innerhalb der
`for`-Schleife mit einem Pfad voller Leerzeichen - eine bekannte
Stolperstelle von `cmd`. Die Zeile entsteht jetzt in einem
Unterprogramm, dort gibt es die Schachtelung nicht.


## 1.5.0 - Max 2016 bis 2027

Der Versionsbereich ist von sechs auf zwoelf Jahrgaenge gewachsen.
Vorbild war das Buildscript von WhiteoutDex, das denselben Bereich
bereits abdeckt.

### Was dafuer noetig war

**C++17 statt C++20.** Gebraucht wird von C++17 nur `std::to_chars`
fuer die locale-feste Zahlenausgabe; alles andere kommt mit weniger
aus. C++17 laesst sich auch gegen die Header der aelteren SDKs
uebersetzen. Geprueft: die Dumps sind mit C++17 zeichengleich mit
denen aus einem C++20-Build - also keine stille Verhaltensaenderung.

**`/permissive-` erst ab Max 2019.** Die Header von 2016 bis 2018
stammen aus der Zeit vor der strengen Konformitaetspruefung und gehen
damit nicht durch. Das ist kein Fehler im eigenen Code, deshalb wird
der Schalter fuer diese Jahrgaenge weggelassen und `/W4` bleibt.
Gesteuert ueber `-DMAX_VERSION=<jahr>`.

**Generator ueber vswhere.** Bisher stand `Visual Studio 18 2026` fest
im Script und musste beim naechsten VS-Sprung angefasst werden. Jetzt
wird gefragt, wie es das WhiteoutDex-Script auch tut: vswhere liegt bei
jeder VS-Installation an derselben Stelle. `XFBIN_GENERATOR`
ueberschreibt weiterhin.

**Eine einzelne Version bauen:** `BAUE_ALLE.bat 2019`. Bei zwoelf
Jahrgaengen dauert ein Komplettlauf sonst unnoetig lang, wenn man nur
eine Version prueft.

### Weiteres

- `PackageContents.xml` hat jetzt zwoelf `plugins parts`-Bloecke; die
  Oberflaeche und die Menue-Skripte decken 2016-2027 ab.
- `XfbinMenu_2022_2024.ms` heisst jetzt `XfbinMenu_2016_2024.ms` - der
  `menuMan`-Weg gilt fuer alle Jahrgaenge bis 2024.
- `INSTALLIERE.bat` und `DEINSTALLIERE.bat` kennen alle zwoelf Ordner.
- Die Zusammenfassung unterscheidet jetzt "FEHLT" von "kein SDK
  installiert" - sonst sieht ein Lauf auf einem Rechner mit vier
  installierten Versionen nach acht Fehlschlaegen aus.


## 1.4.1 - Bone-Groesse

Max legt Bone-Objekte mit Breite und Hoehe 4 an. Bei 222 Bones wird
daraus ein Teppich aus kleinen Kaesten, der das Modell verdeckt.

Neuer Spinner **"Bone size"** in den Optionen, Vorgabe **0** - dann
zeichnen sich die Bones als blosse Linien, so wie man es bei einem
Spielrig will. Die Aenderung wirkt sofort, also ohne neuen Import; man
kann einen Wert ausprobieren und wieder zurueckdrehen.

Angewendet wird sie nur im Modus "Bone objects"; Point-Helper haben
keine Breite und Hoehe.

### Warum in MaxScript und nicht im Plugin

Das Bone-Objekt haelt Breite und Hoehe in einem Parameterblock.
Von C++ aus hiesse das fest verdrahtete Indizes, die sich zwischen
SDK-Versionen unterscheiden koennen - genau die Sorte Annahme, die in
diesem Projekt schon zweimal einen Build gekostet hat. Von MaxScript
aus sind es benannte Eigenschaften.

**Kein Neubau noetig** - nur die `.mcr` hat sich geaendert. Das Fenster
zeigt dann weiter "Plugin 1.4.0", das ist richtig so: die `.dlu` ist
unveraendert.


## 1.4.0 - Note Tracks auf den Szenen-Wurzelknoten

### Der Fehler

Im Animation Merge Tool steht:

    while(numNoteTracks rootNode != 0) do deleteNoteTrack rootNode ...
    local NT = notetrack "animations"; addNoteTrack rootNode NT

`rootNode` wird dort **nirgends zugewiesen** - und genau das habe ich
falsch gelesen. Es ist keine vergessene Variable, sondern eine
MAXScript-Konstante: der Wurzelknoten der SZENE. Ich hatte daraus
"irgendein Wurzelknoten" gemacht und den Track an den Wurzel-BONE der
Figur gehaengt (`1hak00t0 trall`). Dort sucht der Exporter ihn nicht.

Jetzt auf `rootNode`, wie im Vorbild. Die Funktion `sceneRootName()`
bleibt in der API, wird dafuer aber nicht mehr gebraucht.

### Neu: tools/NOTE_CHECK.ms

Listet die Note Tracks des Szenen-Wurzelknotens mit Namen, Anzahl und
den ersten Keys, und prueft zusaetzlich alle Objekte - falls doch mal
einer woanders landet. Dazu der Animationsbereich.

### Zur Waffe: kein Fehler, aber erklaerungsbeduerftig

Nachgezaehlt am Anim-Container:

| Animationen | Referenzen auf `1haksbn1` |
|---|---|
| 36 | 2 |
| 1 (`1hakteam_satk00`) | **8** |

`requiredInstances` nimmt das Maximum, also werden acht Exemplare
angelegt - und das ist richtig, eine Animation wirft tatsaechlich acht
Klingen.

Warum sie trotzdem nicht auffallen: das Modell hat **8 Vertices und 6
Dreiecke**, ist rund 27 Einheiten lang und knapp eine breit - ein
duennes Effektband, kein Schwert. Und sein Mesh-Bone `1hak00t0 sbn01`
steht in der Bind-Pose auf 0,0,0, alle acht Exemplare liegen dort also
uebereinander am Ursprung. Erst die Animation verteilt sie.

Der Import meldet das jetzt je Datei im Listener, mit Bone- und
Objektzahl und der Zahl der Instanzen.


## 1.3.0 - Sequenzen, die fuer sich stehen

Aufgefallen durch die Frage, ob die Waffen im Sequenzmodus an der
richtigen Stelle landen. Zeitlich ja - der Versatz gilt fuer alle
Eintraege einer Animation gleichermassen. Aber daneben lag etwas
anderes im Argen, und es betrifft nicht nur die Waffen.

### Das Problem

Nachgezaehlt an den Daten:

| Animation | Figur-Bones | Waffe 1 | Waffe 2 |
|---|---|---|---|
| `1hakdow0` und die meisten | 112 / 222 | 4 / 5 | 4 / 5 |
| `1hakent0` | 132 / 222 | 4 / 5 | 4 / 5 |
| `1hakdmg0f` | 28 / 222 | **0** | **0** |

Eine typische Animation ruehrt also nur die Haelfte des Skeletts an,
und `1hakdmg0f` laesst die Waffen voellig unberuehrt. Fuer die uebrigen
Knoten gibt es innerhalb der Sequenz gar keinen Key - Max interpoliert
dann quer durch die Luecke zwischen dem letzten Key der vorigen und dem
ersten der naechsten Sequenz. Die vorige Animation blutet in die
naechste.

Fuer eine einzelne Animation faellt das nicht auf. Fuer eine
Zeitleiste mit 37 hintereinandergelegten Sequenzen, wie sie Warcraft 3
braucht, ist es unbrauchbar.

### Die Loesung

Neu: `buildIdleKeys <index> <start> <end>`. Setzt je einen Key auf der
Bind-Pose an beiden Enden der Sequenz - aber **nur fuer die Bones, die
diese Animation nicht anfasst**. Bei den animierten wuerde ein Key am
Ende die Bewegung zurueckreissen.

In der Oberflaeche der Haken **"Rest keys"** neben "Create note track",
standardmaessig an.

### Nebenbei aufgeraeumt

Die Zuordnung "Eintrag -> Szenen-Clump und Bone" stand zweimal im Code:
einmal in `BuildAnimAt`, einmal waere sie fuer `BuildIdleKeys` dazu
gekommen. Beide benutzen jetzt denselben Helfer `ResolveEntryTarget`.
Zwei Kopien einer Zuordnung, die Instanznummern und Rueckfallebenen
kennt, waeren zwei Stellen zum Auseinanderlaufen gewesen.


## 1.2.0 - Zwei gleiche Waffen

### Was in den Daten steht

Ja, die Animationen sagen, welche Waffe wann bewegt wird - und zwar
weil die Waffen-Bones INNERHALB jeder Animation mitanimiert werden,
nicht ueber eine Verknuepfung.

Ueber die 37 Animationen:

| | |
|---|---|
| mit Waffen-Eintraegen | 36 |
| ohne | 1 (`1hakdmg0f`) |
| nur teilweise | 2 (`1haknut0f`, `1haknut1f`, je 2 statt 10) |

Die `coord_parents`-Tabelle bleibt dabei durchgehend innerhalb ihres
Clumps - die Waffe wird also NICHT an einen Hand-Bone gehaengt,
sondern frei animiert. Man muss sie also nicht verknuepfen.

### Zwei Instanzen desselben Modells

Der Anim-Container nennt `1haksbn1` **zweimal**, und die beiden stehen
an verschiedenen Orten:

    clumpIdx=0  1hak00t0 bone   -39.70  -29.24  125.87
    clumpIdx=1  1hak00t0 bone   -15.97   -1.68   87.76

Der Charakter traegt also zwei Stueck derselben Waffe, beide getrennt
animiert. Bis 1.1.0 waeren beide auf dasselbe Skelett geschrieben
worden - die zweite haette die erste ueberschrieben.

Jetzt fragt der Import die Animationen, wie viele Exemplare sie
erwarten, und legt entsprechend viele an. Ab der zweiten Instanz
bekommen Bones und Objekte den Zusatz ` #2`.

Neue API: `fileClumpName()`, `requiredInstances <name>`,
`buildSkeletonN <mode> <scale> <copies>`,
`buildMeshesN <skipLod> <normals> <skin> <scale> <copies>`. Die alten
Signaturen bleiben und rufen mit `copies = 1`.

### Zuordnung ueber die laufende Nummer

Welche Instanz ein Eintrag meint, ergibt sich daraus, wie viele
gleichnamige Clump-Referenzen VOR ihr stehen. Findet sich die
gewuenschte Instanz nicht, faellt der Code auf die erste zurueck -
besser eine Waffe animiert als keine.

### Reihenfolge im Import

Die Animationsdatei wird jetzt ZUERST gelesen, damit beim Anlegen der
Modelle schon feststeht, wie viele Exemplare gebraucht werden. Dafuer
gibt es `fileClumpName()`: den Clump-Namen der geoeffneten DATEI, im
Unterschied zu `sceneClumpName()` fuer die Szene.


## 1.1.0 - Mehrere Skelette in einer Szene

### Waffen und Zubehoer

`1hakacc1.xfbin` enthaelt den Clump `1haksbn1` mit genau vier Bones:
`1hak00t0 bone`, `sbn01`, `bone01`, `bone02`.

Das sind exakt die Ziele, die seit 0.5.4 als "gehoeren zu Clumps, die
nicht in der Szene stehen" uebersprungen wurden - **282 Eintraege ueber
alle 37 Animationen**. Die Warnung war also kein Schoenheitsfehler,
sondern ein Hinweis auf eine fehlende Datei.

### Was falsch war

`buildSkeleton` hat den Szenenzustand ERSETZT statt ihn zu erweitern.
Beim Laden der Waffe waere das Figurenskelett vergessen worden, obwohl
es weiter in der Szene steht - derselbe Denkfehler wie in 0.5.0, nur
eine Ebene hoeher: diesmal nicht "Datei gegen Szene", sondern "eine
Datei gegen mehrere".

Jetzt wird angehaengt. Ein Skelett gleichen Namens wird ueberschrieben
statt verdoppelt, sonst sammeln sich bei mehrfachem Import
Karteileichen an.

Damit hing mehr zusammen als erwartet: `boneHandles_` war ueber den
Index in `clumps_` angesprochen, also ueber die Reihenfolge in der
GERADE GEOEFFNETEN Datei. Sobald zwei Dateien im Spiel sind, ist das
eine andere Reihenfolge als die im Szenenzustand. Alle drei Stellen -
Skelett anlegen, Meshes anhaengen, Skin setzen - suchen den Platz jetzt
ueber den Clump-Namen.

### Reihenfolge der Dateien

Die Modelldateien werden vor dem Import nach Bone-Anzahl sortiert, die
groesste zuerst. Alphabetisch kommt `1hakacc1` vor `1hakbod1`, und dann
haette die Waffe den ersten Platz und damit den Note Track bekommen.
`sceneRootName()` nimmt aus demselben Grund das Skelett mit den meisten
Bones, nicht einfach das erste.

### DXT1-Pfad bestaetigt

Die Waffentextur `1haksbn` ist 64x64 im Pixelformat 0, also DXT1 - der
erste echte Test dieses Pfads. Die erzeugte DDS ist **byteweise
identisch** mit der Referenz aus der Python-Lib (2176 Byte: 128 Kopf
plus 2048 Daten, was fuer 64x64 DXT1 genau stimmt).

Damit sind zwei der sieben Pixelformate an echten Daten geprueft: 8
(R5G6B5) und 0 (DXT1).


## 1.0.0 - Sequenzmodus, aufgeraeumte Oberflaeche

### Oberflaeche

Auf Englisch und um alles erleichtert, was zum Einkreisen von Fehlern
da war und seinen Zweck erfuellt hat:

- Die Knoepfe "Bones anlegen" und "Meshes anlegen" sind raus. Der
  Import macht beides, und beides ist gegen die Python-Referenz
  geprueft.
- Die Kanalhaken Pos/Rot/Scl sind raus. Sie haben ihren Dienst getan,
  als die Ueberdehnung eingekreist wurde; `buildAnimEx` bleibt in der
  API, falls doch mal wieder etwas zu trennen ist.
- Der Haken "Rotation konjugieren" ist raus. `setQuatMode` bleibt
  ebenfalls in der API.
- Die verbliebenen Einstellungen stehen jetzt in einer Gruppe
  "Options": Knotentyp, Skalierung, LOD, Normalen, Skin.

### Sequenzmodus

Neuer Knopf **"Load all as sequence"**, gebaut nach dem Vorbild des
Animation Merge Tools:

- Frame 0 traegt die **Bind-Pose als Key**, damit ein Exporter eine
  definierte Ausgangslage vorfindet und die erste Animation nicht aus
  dem Nichts einsetzt.
- Danach folgt eine Animation nach der anderen, mit einem einstellbaren
  Abstand dazwischen (Standard 10 Frames).
- Auf dem Wurzel-Bone entsteht ein Note Track "animations" mit zwei
  Keys je Sequenz - am Anfang und am Ende, jeweils mit dem
  Animationsnamen. Genau die Form, die `setAnimKey` im Animation Merge
  Tool schreibt, damit ein Exporter, der das eine liest, auch das
  andere liest.

Neue API dafuer:

| Funktion | Zweck |
|---|---|
| `buildAnimAt <index> <startFrame> <mask> <scale>` | wie `buildAnimEx`, mit Zeitversatz |
| `animFrames <index>` | Laenge einer Animation in Frames |
| `buildBindPoseKey <frame>` | Ruhelage als Key setzen |
| `sceneRootName()` | Name des Wurzel-Bones, fuer den Note Track |

Die Zeitleiste wird dabei nicht mehr ersetzt, sondern waechst mit -
sonst haette jede weitere Animation die vorherige abgeschnitten.

`buildBindPoseKey` nimmt dieselbe lokale Lage, die auch
`buildSkeleton` benutzt: Position, Euler-Rotation und Skalierung aus
dem `nuccChunkCoord`. Der Key ist damit per Konstruktion genau die
Lage, in der das Skelett angelegt wurde - keine zweite Quelle, die
auseinanderlaufen kann.

### Note Track auf dem Wurzel-Bone

Vorhandene Note Tracks des Wurzel-Bones werden vorher geloescht, wie im
Animation Merge Tool auch. Sonst sammeln sich bei mehrfachem Laden
Sequenznamen an, die es nicht mehr gibt.


## 0.9.1 - Bitmap laden, Material-Editor fuellen

Drei Nachtraege aus der Recherche zur Materialzuweisung.

### ReloadBitmapAndUpdate

`SetMapName` merkt sich nur den Pfad - geladen wird die Bitmap
dadurch nicht. In den Autodesk-Beispielen steht
`ReloadBitmapAndUpdate()` direkt dahinter. Ohne den Aufruf zeigt der
Material-Editor eine leere Vorschau, bis man die Datei von Hand neu
waehlt.

### Material-Editor fuellen

Laut MAXScript-Referenz legt `setMeditMaterial <index> <material>` ein
Material in einen der 24 Slots des Compact Material Editors.

Neuer Haken **"Materialien in den Material-Editor"**, standardmaessig
an. Nach dem Import werden die Materialien der angelegten Objekte
eingesammelt, Duplikate entfernt und die ersten 24 in die Slots
gelegt. Die Statuszeile nennt die Anzahl.

Das laeuft bewusst in MaxScript und nicht im Plugin: die Materialien
haengen nach dem Import ohnehin an den Objekten, von dort holt man sie
in drei Zeilen. Ein C++-Weg waere mehr Code fuer dasselbe Ergebnis.

### Warnung bei zu wenigen Materialien

Die Material-ID eines Submeshes ist sein Index in der Gruppe. Die
SDK-Doku weist ausdruecklich darauf hin, dass eine Face-Material-ID
hoeher sein kann als die Zahl der Untermaterialien - Max faellt dann
auf einen leeren Platz zurueck. Statt das still hinzunehmen gibt es
jetzt einen Hinweis mit Modellnamen und beiden Zahlen.

## 0.9.0


## 0.9.0 - Stufe 4: Texturen und Materialien

Wertet `nuccChunkTexture` samt eingebettetem NUT-Container und
`nuccChunkMaterial` aus, schreibt die Texturen als DDS und haengt die
Materialien an die Objekte.

### Was drin ist

- **`src/xfbin_tex.h/.cpp`** - NUT-Container, Materialien, DDS-Ausgabe.
  Wie die anderen Parser ohne Max-SDK.
- **MaxScript-API**: `parseTextures`, `textureCount`, `materialCount`,
  `textureSummary`, `exportTextures`, `buildMaterials`.
- **Oberflaeche**: Haken "Texturen mit importieren", standardmaessig an.
  Die DDS landen in einem Unterordner `textures` neben der Quelldatei.
- `xfbindump.exe` kennt `--tex` und `--tex-o <ordner>`.

### Verifikation

Alle fuenf DDS-Dateien **byteweise identisch** mit dem, was die
DDS-Ausgabe der Python-Lib erzeugt - Kopf und Bilddaten:

| Datei | Groesse |
|---|---|
| `1hakbody_0.dds`, `1hakbody_1.dds` | je 524.416 |
| `1hakeye.dds` | 524.416 |
| `1hakmask_0.dds`, `1hakmask_1.dds` | je 262.272 |

Auch die Zuordnung stimmt: 3 Texturen, 6 Materialien, und jedes
Material zeigt auf dieselbe Textur wie in der Python-Lib.

Ein Byte wich zuerst ab: `dwDepth` im DDS-Kopf. Die Lib schreibt 1, ich
0 - beides ist laut Spezifikation zulaessig, das Feld gilt nur fuer
Volumentexturen. Angeglichen, damit "0 Abweichungen" das Pruefkriterium
bleibt statt "eine erklaerte Abweichung".

### Bewusste Entscheidungen

**DDS statt Umwandlung.** Ein NUT enthaelt genau die Pixeldaten einer
DDS-Datei, nur ohne deren Kopf. Fuer DXT1, DXT3 und DXT5 ist nichts zu
rechnen. Bei den unkomprimierten Formaten kommt eine einzige Arbeit
dazu: NUT legt sie in Big-Endian ab, DDS erwartet Little-Endian - also
16- oder 32-bitweise drehen. Das spart einen kompletten
DXT-Dekomprimierer und deckt trotzdem alle sieben Formate ab, die das
Format kennt.

Die Testdatei benutzt durchgehend Format 8 (R5G6B5, 2 Byte je Pixel);
die DXT-Pfade sind implementiert, aber an diesen Daten nicht geprueft.

**Fehlende Texturen sind kein Fehler.** Alle sechs Materialien
verweisen auf `celshade`, und diese Textur liegt NICHT in
`1hakbod1.xfbin` - sie kommt aus einem gemeinsamen XFBIN. Das Material
wird trotzdem gebaut, nur ohne diese Karte, und es gibt einen Hinweis
mit dem Namen.

**Ein Material wird einmal gebaut.** In dieser Datei teilen sich acht
Modelle das Material `1hakbody1`.

**Multi/Sub-Object ohne Antasten des Parameterblocks.** Modelle mit
mehreren Materialien bekommen ein Multi/Sub-Object; die Material-ID der
Faces ist der Index des Submeshes in seiner Gruppe, also genau die
Reihenfolge der Materialliste. Die Anzahl der Unterplaetze wird nicht
gesetzt - das Standardmaterial bringt zehn mit, belegt werden die
ersten. Ein ungenutzter Platz stoert nicht, das Antasten des
Parameterblocks schon.

**Glanzlichter aus.** Cel-Shading-Modelle bringen ihre Beleuchtung in
der Textur mit; ein Spiegelfleck darauf sieht falsch aus.

**Textur im Viewport sichtbar.** `MTL_TEX_DISPLAY_ENABLED` und
`SetActiveTexmap` - ohne das bleibt das Modell grau und man haelt den
Import fuer gescheitert.

**Pfadtrenner plattformabhaengig.** Beim ersten Lauf entstanden unter
Linux Dateien mit einem Backslash im Namen statt in einem Unterordner.
Ein Werkzeug, das sich auch ausserhalb von Max bauen laesst, muss auf
beiden Seiten das Richtige tun.


## 0.8.0 - Ordner statt Datei, ein Knopf statt fuenf

Die Einzelschritte waren zum Einkreisen von Fehlern da. Bones, Meshes
und Skinning sind gegen die Python-Referenz geprueft; sie einzeln
anstossen zu muessen ist seitdem nur noch Arbeit.

### Ordner statt Datei

**Ordner...** waehlt ein Verzeichnis, alle `.xfbin` darin werden
eingelesen und sortiert. Die Zuordnung laeuft ueber den INHALT, nicht
ueber den Dateinamen: eine Datei mit `nuccChunkClump` ist ein Modell,
eine mit `nuccChunkAnm` eine Animationsdatei. Namen wie `1hakbod1` und
`1hakbod1c` sind eine Konvention, auf die man sich nicht verlassen
muss.

Angezeigt wird, was gefunden wurde: "1 Modell(e), 1
Animationsdatei(en) in 2 Datei(en)".

### Ein Knopf

**Importieren** macht in einem Zug: Modell oeffnen, Bones anlegen,
Meshes anlegen, Skinning setzen, danach die Animationsdatei oeffnen
und die Auswahlliste fuellen. Danach nur noch Animation waehlen und
setzen.

Dazu **"Szene vorher leeren"** - beim Ausprobieren spart das jedes Mal
ein manuelles Aufraeumen. Standardmaessig aus.

Die Einzelschritte stehen weiterhin unter "Einzelschritte", zusammen
mit Knotentyp, Skalierung und den Schaltern fuer LOD, Normalen und
Skin.

### Dumps in einem Zug

**"Dumps schreiben..."** schreibt fuer JEDE Datei des Ordners die
passenden Dumps in einen gewaehlten Ordner - Container immer, Bones
und Meshes bei Modelldateien, Animationen bei Animationsdateien.
Benannt nach der Quelldatei. Bisher war das vier Dialoge pro Datei.

### Linter erweitert

`tools/PRUEFE_SCRIPTS.py` prueft jetzt zusaetzlich:

- Ereignishandler fuer Controls, die es nicht gibt - meist ein
  Tippfehler oder ein umbenanntes Control
- Aufrufe von `XfbinCpp.*`, die die Plugin-API nicht kennt

Beides faellt sonst erst zur Laufzeit auf, und die API-Sache auch nur,
wenn man den betreffenden Knopf drueckt. Die bekannte API steht als
Liste im Linter und will beim Erweitern des Plugins mitgepflegt
werden.

Gegengeprueft, dass der Linter das auch wirklich faengt: ein
absichtlich verdrehter Handler-Name, ein Tippfehler im API-Aufruf und
ein `//`-Kommentar werden alle drei gemeldet.

## 0.7.2


## 0.7.2 - ignoreBoneScale

### Der Befund

`BONE_CHECK.ms` hat die Frage entschieden: alle vier Skin-Bones der
Zunge sind kerngesund - Position rund [-5, -6, 96], Skalierung 1,0,
identisch auf Frame 0 und auf dem Testframe, und die gesamte
Elternkette bis `trall` ebenfalls. Trotzdem misst das Mesh 45782.

Bones heil, Verformung absurd: der Fehler sitzt im Skin-Modifier.

### Die Aenderung

In einem Importer-Beispiel steht ueber genau diesem Parameter der
Kommentar "Can get some truly bizarre animations without this in MAX".
Das beschreibt das Symptom.

    IParamBlock2* advanced = skinMod->GetParamBlockByID(2);
    advanced->SetValue(0x0E, 0, TRUE);   // ignoreBoneScale
    advanced->SetValue(0x07, 0, 4);      // bone_Limit

Es passt auch zu den Daten. Die Bones dieses Rigs haben keine exakte
Einheitsskalierung, sondern Werte wie 0,999993 und 1,00001 - so stehen
sie im `nuccChunkCoord`, und `BONE_CHECK.ms` zeigt sie genauso in Max.
Skin rechnet die Bone-Skalierung standardmaessig mit. Ueber eine Kette
hinweg baut sich daraus etwas auf, das mit der eigentlichen Verformung
nichts mehr zu tun hat.

`bone_Limit` auf 4 passt zum Format: NUD speichert genau vier Gewichte
je Vertex.

Dazu wird die Bind-Lage des Meshes jetzt ausdruecklich gesetzt statt
sie Skin herleiten zu lassen:

    imp->SetSkinTm(node, node->GetObjectTM(t), node->GetNodeTM(t));

### Falls es nicht reicht

`BONE_CHECK.ms` vergleicht jetzt zusaetzlich die im Skin HINTERLEGTE
Bind-Matrix (`skinUtils.GetBoneBindTM`) gegen die tatsaechliche Lage
des Bones auf Frame 0. Weicht sie ab, ist das die Ursache, und dann
muss die Bind-Matrix beim Anlegen ausdruecklich gesetzt werden statt
sich auf `AddBoneEx` zu verlassen.

## 0.7.1


## Zwischenstand - Ueberdehnung weiter offen

Kein Versionssprung, nur ein Diagnoseskript. Zwei Thesen sind
widerlegt, und das ist festgehalten, damit sie nicht ein drittes Mal
geprueft werden.

### Widerlegt: die Skalierung

Ein Lauf mit abgeschaltetem Skalierungskanal (`Kanaele: Pos Rot`)
zeigt dieselbe Ueberdehnung. Der `ScaleValue`-Fix aus 0.7.1 bleibt
trotzdem drin - die Begruendung aus der SDK-Doku stimmt, es war nur
nicht diese Ursache.

### Widerlegt: die Verformungsrechnung

Aus der beobachteten Box 25019 x 131 x 1068 rueckwaerts gerechnet.
Fuenf Fehlerannahmen durchprobiert - richtige Rechnung,
Bind-Matrizen als Identitaet, Mesh nicht vortransformiert, lokale
statt Weltmatrizen, Inversion vergessen. Groesstes Ergebnis: 307.
Keine davon kommt auch nur in die Naehe von 25019.

Daraus folgt: es ist kein Rechenfehler in der Zuordnung, sondern in
Max steht in mindestens einer Bone-Matrix ein Wert, den die Daten
nicht hergeben.

### Nebenbefund

`1hak00t0 tongue01` haengt nicht am Mesh-Bone `1hak00t0 tongue`,
sondern an `1hak00t0 kuti_down`. Mesh-Bone und Skin-Kette sind also
verschiedene Aeste - das ist normal, war mir aber nicht klar.

### Neu: tools/BONE_CHECK.ms

Sucht das groesste geskinnte Objekt, holt ueber `skinOps` dessen
Skin-Bones und gibt deren Weltmatrizen auf Frame 0 und auf dem
aktuellen Frame aus, dazu die Elternkette. Markiert alles, was NaN
ist oder ueber 10000 liegt.

Das trennt die verbliebenen Moeglichkeiten:

| Befund | Ursache liegt in |
|---|---|
| Zahlen auf beiden Frames normal | Skin selbst - Bind-Matrizen oder Gewichtszuordnung |
| Muellwerte auf dem aktuellen Frame | dem Setzen der Keys |
| Muellwerte schon auf Frame 0 | dem Anlegen von Skelett oder Skin, nicht in der Animation |


## 0.7.1 - ScaleValue: die Doku bestaetigt den Verdacht

Nachgelesen statt vermutet. Zwei Stellen der SDK-Referenz zusammen
ergeben die Ursache:

**ScaleValue:** "A ScaleValue describes an arbitrary non-uniform
scaling in an arbitrary axis system. The Point3 s gives the scaling
along the x, y, and z axes, and the quaternion q defines the axis
system in which scaling is to be applied."

`q` legt also fest, ENTLANG WELCHER Achsen gestreckt wird - das ist
kein Beiwerk, sondern die halbe Information.

**Quat:** "Constructor. No initialization is performed."

Der Standardkonstruktor von `Quat` setzt nichts. Verlaesst sich
`ScaleValue(const Point3&)` auf den Member-Konstruktor von `q`, steht
dort Speichermuell - und dann wird in einer zufaellig gedrehten
Richtung gestreckt. Genau eine anisotrope Ueberdehnung wie
25019 x 131 x 1068.

Jetzt der Zwei-Argument-Konstruktor mit ausdruecklicher
Einheitsquaternion:

    ScaleValue sv(Point3(...), Quat(0.0f, 0.0f, 0.0f, 1.0f));

Kein Verlass auf einen Standardwert, weder bei `ScaleValue` noch bei
`Quat`. 0.7.0 hatte stattdessen `sv.q.Identity()` nach einem
Standardkonstruktor - das haette gereicht, verlaesst sich aber immer
noch darauf, dass `ScaleValue` selbst `s` sinnvoll belegt.

### Nebenbefund aus derselben Recherche

Zur `Quat`-Klasse steht dort auch: "The rotation convention in the
3ds Max API is the left-hand-rule. Note that this is different from
the right-hand-rule used in the 3ds Max user interface." Das ist der
Satz, auf den ich mich in 0.6.0 gestuetzt hatte. Er stimmt - er
rechtfertigt aber keine Konjugation beim Setzen eines
Rotations-Controllers, weil dort laut `Control::SetValue` der Wert
schlicht gespeichert wird. Die Ruecknahme in 0.6.1 bleibt also
richtig.

Ausserdem interessant fuer spaeter: `Quat::MakeMatrix(Matrix3&, BOOL)`
hat einen zweiten Parameter, der die Matrix transponiert erzeugt. Falls
die Rotationskonvention doch noch Aerger macht, ist das die
vorgesehene Stellschraube.

## 0.7.0 - Kanalmaske, und ein wahrscheinlicher Treffer

### Der Uebeltaeter hat einen Namen

`sceneReport()` hat ihn benannt: `1hak00t0 tongue`, bbox
**25019 x 131 x 1068**, `skin=4`. Alles andere in der Szene ist normal
gross - `1hak00t0 body` misst 136.

Alle 85 Vertices der Zunge haengen an `tongue01` bis `tongue04`, einer
Viererkette. Und diese vier haben in `1hakent0` **gar keine eigenen
Kurven**; nur ihr Elternteil, der Mesh-Bone `1hak00t0 tongue`, bewegt
sich (bis -783 cm). Eine starre Kette darf das Mesh nicht dehnen.

### Offline nachgerechnet

Mit Bind-Pose, Animationskurven und Vertexgewichten die Verformung bei
Frame 5 selbst gerechnet: die Zunge bleibt **3,4 x 4,2 x 2,1** und
sitzt am richtigen Ort. Die Deutung der Daten stimmt also; der Fehler
liegt ausschliesslich in der Anwendung in Max.

### Wahrscheinliche Ursache: ScaleValue

`ScaleValue` traegt neben dem Faktor auch eine Quaternion - die Achsen,
ENTLANG derer skaliert wird. Bisher stand da
`ScaleValue sv(Point3(...))` und damit die Annahme, der Konstruktor
setze diese Quaternion auf Identitaet. Tut er das nicht, wird in einer
zufaelligen Richtung gestreckt - und das ergibt genau eine
anisotrope Ueberdehnung wie 25019 x 131 x 1068.

Jetzt ausdruecklich:

    ScaleValue sv;
    sv.s = Point3(...);
    sv.q.Identity();

Das passt auch dazu, dass der Mesh-Bone der Zunge eine Skalierungskurve
hat (ein Key, Wert 1,000005) und die Kette darunter alles erbt.

### Neu: Kanalmaske

Damit die naechste Eingrenzung nicht wieder ueber Screenshots laeuft:

    XfbinCpp.buildAnimEx <index> <mask> <scale>

`mask` ist Bit 1 = Position, 2 = Rotation, 4 = Skalierung. In der
Oberflaeche drei Haken **Pos / Rot / Scl**, alle standardmaessig an.
`buildAnim` bleibt unveraendert und ruft intern mit Maske 7.

Wenn die Verzerrung bleibt: nacheinander **Scl aus**, dann **Rot aus**,
dann **Pos aus**. Der Kanal, bei dem sie verschwindet, ist der
schuldige - eine Minute statt einer Raterunde.

## 0.6.2 - Build-Fehler in sceneReport

    error C2102: "&" erwartet L-Wert

`INode::GetObjectTM()` liefert eine `Matrix3` ALS WERT. Von einem
Temporary gibt es keine Adresse, `&n->GetObjectTM(t)` ist also
ungueltig. Jetzt erst in eine lokale Variable, dann deren Adresse.

Nachgeprueft mit einem minimalen Stub der benutzten SDK-Typen: die alte
Form scheitert erwartungsgemaess mit "taking address of rvalue", die
neue uebersetzt sauber. Der ganze Quelltext liess sich damit nicht
pruefen - dafuer braucht es die echten Header - aber diese eine Klasse
von Fehler laesst sich so ohne 3ds Max einkreisen.

Zwei weitere Stellen im selben neuen Code vorsorglich entschaerft:

- Der ternaere Ausdruck mit `std::to_wstring(...).c_str()` fuer die
  Skin-Spalte ist jetzt eine eigene Variable. Die Lebensdauer war
  zwar korrekt, aber nur zufaellig offensichtlich.
- `GetName()` ist je nach SDK-Version mal `const MCHAR*`, mal
  `MCHAR*`. Der ternaere Ausdruck dagegen ist jetzt ein
  ausgeschriebenes if/else - so gibt es nichts aufzuloesen.

`#include <locale>` und `#include <algorithm>` ergaenzt.

## 0.6.1 - Konjugation zurueckgenommen

Die Aenderung aus 0.6.0 hat die Animation sichtbar verschlechtert und
ist raus. Das Verhalten entspricht wieder 0.5.4.

### Warum sie falsch war

Ich hatte die Konjugation aus dem MDLX-Importer uebernommen, wo genau
das die Loesung war. Das war eine falsche Uebertragung: dort ging es um
die Quaternion-Konvention des **MDX-Formats**, nicht um eine Eigenheit
von 3ds Max. Ein Fix aus einem anderen Format ist keine Begruendung fuer
dieses.

Die SDK-Referenz zu `Control::SetValue` ist an der Stelle eindeutig:
bei `CTRL_ABSOLUTE` zeigt `val` auf ein `Quat`, und "the controller
should simply store the value" - keine Umrechnung. Haette ich das vorher
nachgelesen statt den fremden Fix zu uebertragen, waere die Runde
ausgefallen.

Der Umschalter bleibt als Notausgang, jetzt richtig herum beschriftet
("Rotation konjugieren", Standard aus). Was aus 0.6.0 bleibt: er wirkt
auf BEIDE Rotationsquellen. Zwei Konventionen in einem Skelett - Euler
hier, Quaternion dort - koennen nie beide stimmen.

### Neu: sceneReport()

Die Ueberdehnung eines Meshes ist ein ZWEITES, davon unabhaengiges
Problem und war schon vor 0.6.0 da. Beides gleichzeitig anzufassen war
mein Fehler.

`XfbinCpp.sceneReport()` und der Knopf "Groesste Objekte zeigen" listen
die zwoelf groessten Objekte am aktuellen Frame mit Ausdehnung,
Skalierung, Elternteil und **Anzahl der Skin-Bones**.

Die Skin-Spalte steht da mit Absicht: eine Ueberdehnung, die erst MIT
der Animation auftritt und in der Bind-Pose nicht, ist fast immer ein
Skinning-Problem. Falsch zugeordnete Gewichte fallen in Ruhelage nicht
auf, weil dort nichts verformt wird - erst wenn ein Bone sich dreht,
zieht es die betroffenen Vertices weg.

## 0.6.0 - Rotation: eine Konvention statt zwei

Gefunden ueber einen frueheren Chat zum MDLX-C++-Importer, in dem
derselbe Fehler auftrat. Dort war Fix C:
`rotCtrl->SetValue` brauchte die KONJUGIERTE Quaternion, nachdem die
direkte Variante "in die falsche Richtung" drehte.

### Zwei Rotationsquellen mit verschiedenen Konventionen

Der eigentliche Fehler war schlimmer als eine falsche Richtung. Der
Umschalter `setQuatMode` griff nur in den Quaternion-Pfad. Die
**2.174 Euler-Kurven** (Format 8) liefen daran vorbei und benutzten
immer die andere Konvention.

Damit standen in einem Skelett zwei Rotationsquellen mit
entgegengesetzter Drehrichtung - und keine Einstellung des Umschalters
konnte richtig sein. Ein Bone mit Euler-Kurve und der naechste mit
Quaternion-Kurve drehten gegeneinander. In langen Ketten wie den
Effektbaendern (`1hak_udeanmr01..04`, vier Bones hintereinander)
summiert sich das auf und schleudert die Spitze meterweit weg - genau
der lange duenne Zipfel im Viewport.

### Was jetzt gilt

Beide Rotationsquellen laufen ueber dieselbe Matrix und dieselbe
Umwandlung. Konjugiert wird an genau einer Stelle: beim Uebergang von
der `Matrix3` in den Rotations-Controller.

Dort gehoert es auch hin. Autodesk schreibt selbst, dass Max
Rotationen in Knotenmatrizen intern nach der LINKEN-Hand-Regel
speichert, waehrend die Rotationsfunktionen der rechten folgen - wer
beides mischt, muss invertieren. `SetNodeTM` in `buildSkeleton` nimmt
die Matrix direkt und ist deshalb nicht betroffen; das Skelett stimmt
ja nachweislich gegen Blender. Der Rotations-Controller liegt auf der
anderen Seite dieser Grenze.

Der `transposed`-Parameter in `MakeRotationFromQuat` ist damit
ueberfluessig und raus - ein Umschalter an zwei Stellen war die
Ursache des Problems, nicht seine Loesung.

Beschriftung entsprechend: aus "Quaternion transponiert" wird
"Rotation nicht konjugieren". Standard ist konjugiert.

### Unveraendert geprueft

Skelett- und Animationsdump weiterhin zeichengleich mit der
Python-Referenz - 1.781 bzw. 100.415 Zeilen, je 0 Abweichungen. Die
Aenderung sitzt ausschliesslich auf der Max-Seite.

## 0.5.4 - Fremde Clumps, und ein Diagnoseskript

### Eine Animationsdatei spricht mehrere Modelle an

`1hakbod1c.xfbin` nennt drei Clumps: zweimal `1haksbn1` (die Waffe) und
einmal `1hakbod1` (die Figur), in einzelnen Animationen dazu noch
`wrinkles`, `team_leader` und `1efc_dmy01`. Ueber alle 37 Animationen
gehoeren **282 Bone-Eintraege** zu Clumps, die gar nicht in der Szene
stehen.

Bis 0.5.3 gab es dafuer den Rueckfall "steht nur ein Skelett in der
Szene, dann nimm das". Der hat alle 282 mit auf das Figurenskelett
geworfen. In dieser Datei ohne sichtbare Folgen - nachgerechnet
kollidiert keiner der fremden Bone-Namen mit einem Namen des
Figurenskeletts -, aber das ist Glueck, kein Entwurf. Ein
gleichnamiger Bone in zwei Clumps genuegt, und die Animation landet auf
dem falschen Knochen.

Der Rueckfall greift jetzt nur noch, wenn er eindeutig ist: genau ein
Skelett in der Szene UND genau ein Clump in der Animation. Sonst wird
ueber den Namen zugeordnet, und die uebersprungenen Eintraege werden mit
den Clump-Namen gemeldet - als Hinweis, nicht als Fehler, denn das ist
der Normalfall.

Das erklaert auch die Differenz beim ersten Lauf: `1hakgit1` hat 919
Keys fuer Position, Rotation und Skalierung, gesetzt wurden 851. Die
fehlenden 68 sind genau die vier `1haksbn1`-Bones der Waffe.

### Neu: tools/GROESSTE_OBJEKTE.ms

Listet die zehn groessten Objekte der Szene am aktuellen Frame mit
Ausdehnung, Skalierung und Elternteil, dazu alle Knoten mit
auffaelliger Skalierung. Ein Charakter ist rund 180 Einheiten hoch -
was deutlich darueber liegt, ist der Uebeltaeter, und man muss ihn
nicht im Viewport suchen.

### Was NICHT die Ursache war

Nachgerechnet an `anim_dumb.txt`, damit es nicht nochmal geprueft wird:

- Die Kanalzuordnung stimmt. Bone-Eintraege benutzen ausschliesslich
  ch0/fmt5,6 (Position), ch1/fmt10,17 (Quaternion), ch2/fmt5,16
  (Skalierung), ch3/fmt11,15 (Deckkraft), ch4/fmt8 (Euler).
- Die Skalierungswerte der ganzen Datei liegen zwischen 0,88 und 1,25.
- In `1hakgit1` weicht keine Bone-Position um mehr als Faktor 1,2 von
  der Bind-Pose ab.
- Vier Bones (`1hak_udeanmr/l`, `1hak_asianimer/l`, die Effektbaender)
  haben in `1hakent0`, `1hakitma0`, `1hakitmg0` und `1hakout0` echte
  Auslenkungen bis 870 cm. Das steht so in der Datei und ist kein
  Lesefehler - in `1hakgit1` kommen sie nicht vor.

Die Daten sind also in Ordnung; die Ueberdehnung entsteht erst beim
Setzen in Max.

## 0.5.3 - C-Kommentar in einer MaxScript-Datei, und ein Linter

    -- Syntax error: at /, expected <rollout clause>
    -- Line number: 164

Der Kommentarblock, den 0.5.2 zur Reihenfolge der Funktionen eingefuegt
hat, war mit `//` geschrieben. MAXScript kommentiert mit `--`; `//` ist
schlicht eine Division ohne Operanden. Zwei Fehler hintereinander in
derselben eingefuegten Stelle - beide von der Sorte, die ein Mensch
nicht zuverlaessig sieht, eine Maschine aber sofort.

### Neu: tools/PRUEFE_SCRIPTS.py

Deshalb jetzt ein Linter, der genau die vier Fehlerarten prueft, die in
diesem Projekt tatsaechlich vorgekommen sind:

| Prüfung | Symptom in Max |
|---|---|
| Nicht-ASCII-Zeichen | zerschossene Beschriftungen, je nach Version |
| C-Kommentar `//` | `Syntax error: at /, expected <rollout clause>` |
| Klammerbilanz | `Syntax error` an unpassender Stelle |
| Vorwaertsreferenz auf `fn` | `Call needs function or class, got: undefined` |

Aufruf aus dem Projektordner:

    python tools\PRUEFE_SCRIPTS.py

Rueckgabe 0 = sauber, 1 = Probleme. Laeuft ueber `scripts\*.mcr`,
`scripts\*.ms` und `tools\*.ms`.

Alle fuenf Skriptdateien sind damit geprueft und sauber.

Nur die `.mcr` hat sich geaendert. Kein Neubau noetig.

## 0.5.2 - Vorwaertsreferenz im Rollout

    -- Type error: Call needs function or class, got: undefined
    -- Line number: 277

`RefreshInfo` ruft `RefreshScene` auf, aber `RefreshScene` stand
DARUNTER. MAXScript loest Namen in einem Rollout streng von oben nach
unten auf - eine weiter unten definierte Funktion ist beim Aufruf
schlicht `undefined`, und das faellt erst zur Laufzeit auf.

`RefreshScene` steht jetzt vor `RefreshInfo`. Die Reihenfolge ist mit
einem Kommentar an der Stelle festgehalten, damit sie beim naechsten
Einfuegen nicht wieder verrutscht.

Zusaetzlich alle Rollout-Funktionen gegen Vorwaertsreferenzen geprueft:
`GetPluginVersion`, `GetMaxYear`, `RefreshScene`, `RefreshInfo`,
`ClearInfo` - keine weitere.

Nur die `.mcr` hat sich geaendert. Ein Neubau des Plugins ist nicht
noetig; `INSTALLIERE.bat` genuegt.

## 0.5.1 - Modell und Animation sind zwei Dateien

Beim ersten Test von Stufe 5 sofort aufgelaufen:

    FEHLER: buildAnim: es ist kein Skelett angelegt - erst Bones anlegen.

Und zwar direkt nachdem 222 Bones angelegt worden waren und im Viewport
standen.

### Was falsch war

Modell und Animationen liegen in getrennten Dateien - `1hakbod1.xfbin`
und `1hakbod1c.xfbin`. Der uebliche Ablauf ist also zwangslaeufig zwei
Dateien lang: Modell oeffnen, Bones und Meshes anlegen, dann die
Animationsdatei oeffnen und Animationen setzen.

`open()` hat dabei `clumps_` und `boneHandles_` geleert. Beides
beschreibt aber nicht die Datei, sondern die SZENE - die Knoten standen
weiter da, nur das Plugin wusste nichts mehr von ihnen. Der einzige
Ablauf, fuer den das Ganze gebaut ist, war damit unmoeglich.

Das ist kein Fluechtigkeitsfehler, sondern eine falsche Annahme:
"gelesene Datei" und "Szeneninhalt" waren dasselbe Feld. Solange nur
eine Datei im Spiel war, ist das nie aufgefallen.

### Was jetzt gilt

`sceneClumps_` ist eine Kopie, die `buildSkeleton` anlegt und die einen
Dateiwechsel ueberlebt. `clumps_` beschreibt weiterhin nur die gerade
geoeffnete Datei. `close()` raeumt den Szenenzustand ebenfalls nicht
weg - die Knoten sind ja noch da.

Neu sichtbar in der Oberflaeche: eine Zeile "Szene: 222 Bones
(1hakbod1)" unter der Skelett-Zusammenfassung. "Animation setzen" haengt
jetzt am Skelett in der SZENE, nicht an dem in der Datei - die
Animationsdatei enthaelt naemlich keins.

Neue Funktionen: `sceneBoneCount()`, `sceneClumpName()`, `clearScene()`.
`clearScene()` vergisst nur die Buchfuehrung und loescht keine Knoten -
Knoten loescht man in Max, alles andere waere eine Ueberraschung.

Ausserdem: passt der Clump-Name der Animation nicht zum Skelett in der
Szene, aber es steht genau ein Skelett dort, wird es trotzdem benutzt.
Eine Animationsdatei nennt ihre Clumps nicht immer genauso wie die
Modelldatei.

## 0.5.0 - Stufe 5: Animationen

Wertet `nuccChunkAnm` aus und legt die Keys auf das vorhandene Skelett.

### Was drin ist

- **`src/xfbin_anm.h/.cpp`** - alle 23 Kurvenformate, Quantisierung,
  Zeitumrechnung, Referenzaufloesung. Wie die anderen Parser ohne
  Max-SDK.
- **MaxScript-API**: `parseAnims`, `animCount`, `animName`,
  `animSummary`, `animDump`, `buildAnim`, `setQuatMode`.
- **Oberflaeche**: Gruppe "Animationen" mit Auswahlliste,
  "Animation setzen", "Anim-Dump..." und dem Quaternion-Notausgang.
- **`tools/pydump_anims.py`** + **`tools/VERGLEICHE_ANIMS.bat`**.
- `xfbindump.exe` kennt `--anims`, `--anims-o <datei>`, `--no-keys`.

### Verifikation

Vollstaendiger Animationsdump gegen die Python-Referenz,
`1hakbod1c.xfbin`:

| | |
|---|---|
| Animationen | 37 |
| Entries | 4.670 |
| Kurven | 22.737 |
| **Keyframes** | **72.844** |
| Dumpzeilen | 100.415 |
| **Abweichungen** | **0** |

Verglichen wird jeder Key einzeln - Zeitpunkt und alle Werte - dazu
Kurvenkopf, Kanalzuordnung, Eintragsindizes und die aufgeloesten
Zielnamen.

Tatsaechlich vorkommende Kurvenformate in dieser Datei: 5, 6, 8, 10,
11, 12, 15, 16, 17, 20, 22. Die uebrigen sind implementiert, aber an
diesen Daten nicht geprueft.

Eine Abweichung trat auf und wurde behoben: die Eintraege mit
`clump = -1` (Kamera, Licht) loesen ihren Namen NICHT ueber die
Referenztabelle auf, sondern ueber die Indexliste der Page - der Wert
in `other_entry_indices` ist eine Position in dieser Liste, und erst
der Eintrag dort ist ein Chunk-Map-Index. Ueber die Referenztabelle
gelesen kamen Bone-Namen statt "camera001" und "direct01" heraus.

### Bewusste Entscheidungen

**Die Quaternion-Konvention ist hergeleitet, nicht geraten.** Ein
Messversuch gegen die Bind-Pose war ergebnislos - die Animationen
starten nicht in Ruhelage, beide Deutungen lagen rund 120 Grad daneben.
Stattdessen aus dem Blender-Code hergeleitet: dort ist die lokale
Rotation eines Bones `conj(q)`, und der Umweg ueber die Bind-Rotation
kuerzt sich zu genau dem weg. Blender rechnet in Spalten, Max in
Zeilen; fuer Rotationen gilt `ColMat(conj(q)) = ColMat(q)^T`, also
ergibt die gesuchte Zeilenmatrix genau die gewohnte Spaltenformel.
Falls sich das an echten Daten nicht bestaetigt: `setQuatMode 1`
transponiert.

**Die Rotation geht ueber eine Matrix3, nicht direkt als Quat.** Max
wandelt selbst um. Damit erbt die Rotation die Konvention aus der
Bind-Pose, die gegen Blender bereits zeichengleich geprueft ist -
statt einer zweiten, unabhaengigen Annahme darueber, wie Max
Quaternionen in Matrizen umrechnet.

**`SetValue` im `AnimateOn`-Block statt `IKeyControl`.** SetValue kommt
mit jedem Controllertyp zurecht, den Max dem Knoten gegeben hat, und
braucht keine Annahme ueber die Achsenreihenfolge eines
Euler-Controllers - genau die Annahme hat in AnimMerge mehrere Anlaeufe
gekostet. Der Preis: die Tangenten sind Max' Standard. Zeigt sich
Ueberschwingen zwischen gleichen Keys, ist der Wechsel auf
`IKeyControl` mit ausdruecklichen Tangenten der naechste Schritt - das
Muster dafuer steht in AnimMerge.

**Zwei Formate lesen anders, als ihr Name vermuten laesst.**
`kEulerInterpolate` liest drei Floats, deutet aber den ersten als
Zeitwert; `kFloatLinear` liest Zeitwert und Wert, legt aber beide als
Wert ab und benutzt die implizite Zeit. Beides ist so in der
Python-Lib, und beim Abgleich muss dasselbe herauskommen - deshalb
uebernommen und kommentiert, nicht "korrigiert".

**Nach JEDER Kurve wird auf 4 Bytes ausgerichtet**, nicht erst am Ende
eines Eintrags.

## 0.4.1 - Skin: Build-Fehler und eine stille Falle

Zwei Korrekturen aus der Nachrecherche zum Skin-Modifier. Die erste
verhindert den Build, die zweite waere ein Laufzeitfehler geworden, den
man erst beim Bewegen eines Bones bemerkt haette.

### Interface::AddModifier gibt es nicht

Build-Fehler in allen sechs Max-Versionen:

    error C2039: "AddModifier" ist kein Member von "Interface"

`AddModifier` sitzt nicht auf `Interface`, sondern auf `Interface12`.
Der Aufruf war aus einem Forumsbeispiel uebernommen, das
`GetCoreInterface12()` benutzt - der Zusatz ist mir durchgegangen.

Statt auf `Interface12` auszuweichen geht es jetzt ueber den
dokumentierten und versionsunabhaengigen Weg: Max haengt beim ersten
Modifier ein `IDerivedObject` zwischen Knoten und Basisobjekt, und dort
wird der Modifier eingetragen. Traegt der Knoten schon eines, wird es
weiterverwendet - sonst entstuenden zwei verschachtelte Stapel.
Braucht `#include <modstack.h>`.

### Die Gewichtssumme muss exakt 1.0 sein

Die SDK-Dokumentation zu `AddWeights` ist an der Stelle
unmissverstaendlich: die Summe aller Gewichte MUSS 1.0 betragen, sonst
schlaegt der Aufruf fehl.

0.4.0 hat nur normalisiert, wenn die Summe um mehr als 1e-5 daneben lag.
Eine Summe von 0,99999994 aus der Datei haette also gereicht, um den
Vertex ungewichtet zu lassen - und `AddWeights` gibt in dem Fall nur
`FALSE` zurueck, ohne Meldung. Aufgefallen waere das erst beim ersten
Bewegen eines Bones, als "ein paar Vertices haengen".

Jetzt wird immer normalisiert, und der letzte Eintrag wird so gesetzt,
dass die Summe exakt `1.0f` ergibt statt nur ungefaehr. Abgelehnte
Aufrufe werden gezaehlt und als Warnung gemeldet, statt still zu
verschwinden.

Bestaetigt hat sich dabei auch die Entscheidung aus 0.4.0, nur benutzte
Bones einzutragen: laut geloestem Autodesk-Forumsfall schlaegt
`AddWeights` fehl, sobald ein Bone verwendet wird, der nicht vorher
ueber `AddBoneEx` registriert wurde.

## 0.4.0 - Stufe 3: Skinning

Setzt einen Skin-Modifier auf die geskinnten Modelle und traegt die
Vertexgewichte aus dem NUD-Block ein.

### Was drin ist

- `ApplySkin()` in `xfbinimport.cpp`, ueber **`ISkinImportData`**.
- Neue Funktion `buildMeshesSkinned <skipLod> <normals> <skin> <scale>`.
  `buildMeshes` bleibt gueltig und ruft sie mit `skin=1` auf.
- Schalter "Skin" in der Oberflaeche.
- Zeitmessung `skin=` in `timings()`.

### Bewusste Entscheidungen

**ISkinImportData statt skinOps.** Der Unterschied ist nicht
kosmetisch: `skinOps` verlangt, dass der Modifier im Modify-Panel aktiv
ist, und wird bei mehreren hundert Bones sehr langsam - es gibt
Berichte von etwa einer Operation pro Sekunde bei 500 Bones. Bei 22.601
Vertices und 222 Bones waere das der Unterschied zwischen Sekunden und
Minuten.

**Die Reihenfolge ist Pflicht, nicht Geschmack.** Modifier anhaengen,
`EvalWorldState`, Bones eintragen (nur beim letzten `update=TRUE`),
erneut `EvalWorldState`, dann die Gewichte. Ohne die Auswertungsschritte
kennt der Modifier das Mesh noch nicht und die Gewichte gehen ins Leere.

**Geskinnte Modelle werden NICHT mehr an den Mesh-Bone gehaengt.** Sonst
wirkt dessen Bewegung zweimal - einmal ueber die Elternbeziehung und
einmal ueber den Skin. Der Blender-Importer setzt zwar beides, aber
Blenders Bone-Parenting rechnet die Ruhelage heraus; Max' nicht.
Ungeskinnte Modelle (Zaehne, Augen) bleiben an ihrem Bone gehaengt und
folgen ihm dadurch.

**Nur benutzte Bones landen in der Skin-Liste.** Alle 222 einzutragen
macht den Modifier unuebersichtlich und das Gewichtewerkzeug traege.

**Gewichte werden normalisiert.** Die Werte in der Datei summieren sich
meist auf 1, aber nicht exakt - und wenn ein Bone-Index aus dem Clump
herauszeigt, fehlt sein Anteil ganz.

**Die Bone-IDs im NUD zeigen direkt in die Coord-Liste des Clumps**, in
Dateireihenfolge, ohne Versatz durch `boneStart`. Genau so nutzt sie
auch der Blender-Importer fuer seine Vertexgruppen.

## 0.3.0 - Stufe 2: Meshes

Wertet den in `nuccChunkModel` eingebetteten NUD-Block aus und legt die
Geometrie in der Szene an. Skinning folgt in Stufe 3.

### Was drin ist

- **`src/xfbin_nud.h/.cpp`** - Modellkopf, NUD-Block, Vertexformat-
  Decoder, Streifenaufloeser. Wie die anderen Parser ohne Max-SDK.
- **MaxScript-API**: `parseMeshes`, `modelCount`, `meshSummary`,
  `meshDump`, `buildMeshes`.
- **Oberflaeche**: Gruppe "Meshes" mit LOD-Schalter, Normalen-Schalter,
  "Meshes anlegen" und "Mesh-Dump...".
- **`tools/pydump_meshes.py`** + **`tools/VERGLEICHE_MESHES.bat`**.
- `xfbindump.exe` kennt `--meshes`, `--meshes-o <datei>`, `--no-verts`.

### Verifikation

Vollstaendiger Meshdump gegen die Python-Referenz, `1hakbod1.xfbin`:

| | |
|---|---|
| Modelle | 19 |
| Submeshes | 21 |
| Vertices | 22.601 |
| Dreiecke | 29.894 |
| geskinnte Submeshes | 14 |
| Dumpzeilen | 52.558 |
| **Abweichungen** | **0** |

Verglichen wird jeder Vertex einzeln: Position, Normale, Farbe, alle
UV-Kanaele, Bone-IDs und Gewichte, dazu jedes Dreieck sowie Streifen-
und Entartungszaehler je Submesh.

Eine Abweichung trat zuerst auf und wurde in der Referenz behoben, nicht
im Parser: die Python-Lib liest `singleBind` als `uint16` (65535), der
Wert ist aber vorzeichenbehaftet. `0xFFFF` heisst "keine Einzelbindung",
und der Exporter der Lib schreibt dort selbst `write_int16(-1)`. Die
Deutung als `int16` ist also die richtige.

### Bewusste Entscheidungen

**Struktur von Arrays statt Array von Strukturen.** Die Vertexdaten
gehen als Block weiter an Max; pro Vertex ein Objekt anzulegen waere bei
22.601 Vertices reine Verschwendung.

**Der Split zwischen vertClump und vertAddClump ist explizit
ausgeschrieben.** Bei geskinnten Meshes liegen Position, Normale und
Gewichte im vertAddClump, UVs und Farben getrennt davon im vertClump;
bei ungeskinnten steht alles verschachtelt im vertClump. Innerhalb des
UV-Blocks kommt die Farbe zuerst. Diese drei Saetze sind der Grund,
warum das Format als schwierig gilt - sie stehen deshalb als
Kommentarblock im Header, nicht verstreut im Code.

**Bit 0x04 dreht die Bedeutung von 0x01 und 0x02 um.** Im
Vollgleitkomma-Modus ist 0x01 die Normale und 0x02 sind die Tangenten,
im Halbgleitkomma-Modus umgekehrt. Wer das uebersieht, liest Tangenten
als Normalen und wundert sich ueber die Schattierung.

**Ein Objekt je Modell, nicht je Submesh.** Die Submeshes eines Modells
werden zusammengefasst und bekommen ueber die Material-ID ihre Zuordnung
- dieselbe Aufteilung wie im Blender-Importer, wo alle Submeshes in ein
bmesh gehen.

**Vertices werden transformiert, der Knoten bleibt auf Identitaet.**
Die NUD-Daten liegen im lokalen Raum des Mesh-Bones. Blender rechnet sie
mit `blender_mesh.transform(node.matrix)` in den Weltraum; hier passiert
dasselbe beim Aufbau. Der Knoten wird danach mit `keepTM` an den
Mesh-Bone gehaengt, sodass er ihm folgt, ohne die Lage zu veraendern.

**LOD-Modelle standardmaessig aus.** In dieser Datei sind 5 von 19
Modellen LOD-Varianten. Sie liegen in einer eigenen Modellgruppe des
Clumps und wuerden sonst deckungsgleich ueber dem Original liegen.

**Explizite Normalen, abschaltbar.** Ohne sie rechnet Max die Normalen
aus den Glaettungsgruppen neu, und die Kantenrundung eines
Cel-Shading-Modells geht verloren. `MeshNormalSpec` ist der
empfindlichste SDK-Teil dieser Stufe, deshalb ein eigener Schalter.

## 0.2.1 - Dezimaltrennzeichen im Dump

Fehler beim ersten Testlauf in 3ds Max 2027 auf einem deutschen Windows
gefunden. Der Skelett-Dump enthielt `0,000000` statt `0.000000` und war
damit gegen die Python-Referenz nicht mehr vergleichbar - obwohl jede
einzelne Zahl stimmte. Nach dem Ersetzen der Kommata: null Abweichungen
ueber alle 1781 Zeilen.

Ursache: die Zahlen liefen ueber `snprintf("%.6f")`, und das folgt der
Prozess-Locale. 3ds Max stellt die auf die Systemsprache um. Im
Standalone-Werkzeug trat es nicht auf, weil eine Konsolenanwendung in
der C-Locale startet - der Fehler war also nur im Plugin sichtbar. Das
ist auch der Grund, warum mein Offline-Abgleich ihn nicht gefunden hat.

Behoben ueber `std::to_chars`, das per Definition locale-unabhaengig
ist. Zusaetzlich werden beide Dump-Streams mit `std::locale::classic()`
imbued, damit auch Ganzzahlen keine Tausendertrennung abbekommen
koennen; ein Austausch Komma-zu-Punkt bleibt als Sicherheitsnetz stehen.

Merksatz fuer die naechsten Stufen: alles, was verglichen oder wieder
eingelesen wird, muss locale-fest formatiert werden. Fuer Text, den nur
ein Mensch liest - Statuszeile, Zeitmessung, Protokoll - ist das
deutsche Komma dagegen richtig und bleibt.

## 0.2.0 — Stufe 1: Skelett

Wertet `nuccChunkClump` und `nuccChunkCoord` aus und legt das Skelett in
der Szene an. Der Container-Teil (0.1.0) ist unveraendert.

### Was drin ist

- **`src/xfbin_clump.h/.cpp`** — Clump- und Coord-Auswertung,
  Hierarchie, lokale und Weltmatrizen. Wie der Container-Parser ohne
  Max-SDK-Abhaengigkeit.
- **MaxScript-API**: `parseSkeleton`, `clumpCount`, `boneCount`,
  `boneSummary`, `boneDump`, `buildSkeleton`.
- **Oberflaeche**: Gruppe "Skelett" mit Knotentyp, Skalierung, "Bones
  anlegen" und "Bone-Dump...".
- **`tools/pydump_bones.py`** + **`tools/VERGLEICHE_BONES.bat`** —
  Referenzrechnung nach Blenders Weg und Vergleich.
- `xfbindump.exe` kennt jetzt `--bones` und `--bones-o <datei>`.

### Verifikation

Skelett-Dump gegen die Python-Referenz, `1hakbod1.xfbin`:

| | |
|---|---|
| Bones | 222 |
| Wurzeln | 1 |
| Hierarchietiefe | 14 |
| Dumpzeilen | 1781 |
| **Abweichungen** | **0** |

Verglichen werden Name, Elternindex, Tiefe, Flags, Position, Rotation,
Skalierung, Deckkraft und die vollstaendige 4x3-Weltmatrix jedes Bones.

Zusaetzlich vorab geprueft: die Chunk-Versionen stimmen mit der
Python-Lib ueberein. `nuccChunkCoord` steht durchgehend auf 121 (0x79),
liegt also ueber der `0x66`-Schwelle — das `flags`-Feld wird gelesen,
kein Byte-Versatz.

### Bewusste Entscheidungen

**Zeilenvektoren, und zwar von Anfang an.** Blender rechnet mit
Spaltenvektoren (`world = parent @ local`), Max mit Zeilenvektoren
(`world = local * parent`). Die Matrizen werden deshalb gleich in
Max-Konvention gerechnet, statt am Ende zu transponieren. Der
Konventionsblock oben in `xfbin_clump.h` erklaert das einmal
ausfuehrlich; die Python-Referenz gibt zum Vergleich transponiert aus.

**Euler-Reihenfolge ZYX.** Der Blender-Importer liest die Rotation im
Coord-Chunk als `Euler(rot, 'ZYX')`, also Z zuerst. In Zeilenschreibweise
ist das `Rz * Ry * Rx` — das zuerst Angewendete steht links.

**Intern double, obwohl Matrix3 float ist.** Bei 14 Hierarchieebenen
weicht eine float-Rechnung in der sechsten Nachkommastelle ab. Praktisch
egal, aber es macht den Abgleich gegen die Referenz unscharf: statt
"identisch" haette man nur noch "nah dran" und damit kein brauchbares
Pruefkriterium. Umgewandelt wird erst beim Uebergang in die `Matrix3`.

**Mat43 ist zeilenweise wie Matrix3 angeordnet.** Zeilen 0..2 Basis,
Zeile 3 Translation. Der Uebergang ins SDK ist dadurch eine reine
double-nach-float-Wandlung ohne Umsortieren — also ohne die Stelle, an
der man sich vertut.

**Skalierungsvorzeichen getrennt gemerkt.** Wie im Blender-Importer:
eine negative Skalierung direkt anzuwenden zerstoert die Rotation, und
Max geht damit noch unfreundlicher um als Blender. Fuer einen spaeteren
Export muss das Vorzeichen aber erhalten bleiben.

**Point-Helper als Standard, Bone-Objekte als Option.** Der Point-Helper
braucht keine Kenntnis der Bone-Parameterbloecke, sieht mit `ShowBone(1)`
wie ein Knochen aus, und der Skin-Modifier akzeptiert ihn in Stufe 3
genauso. Schlaegt das Anlegen eines Bone-Objekts fehl, faellt der Code
selbst auf den Point-Helper zurueck.

**Reihenfolge ueber eine Tiefensuche.** `Clump::depthFirst` liefert eine
Reihenfolge, in der jeder Elternteil vor seinen Kindern steht. Damit
laesst sich stumpf durchlaufen - keine Rekursion beim Anlegen der
Knoten, und die Weltmatrix des Elternteils ist garantiert schon
gerechnet. Nicht erreichbare Knoten werden gemeldet und trotzdem
angehaengt, statt still zu verschwinden.

**Doppelte Bone-Namen werden gemeldet.** Fuer den Skin-Modifier in
Stufe 3 muessen sie eindeutig sein. Lieber jetzt eine Warnung als
spaeter eine Suche.

**Die Guards sind jetzt in Benutzung.** 222 Knoten ergeben ohne
`HoldSuspendGuard` und `SceneRedrawGuard` 222 Undo-Records und 222
Viewport-Updates.

## 0.1.0 — Stufe 0: Container lesen

Erste Fassung. Liest den NUCC-Container vollständig ein und gibt ihn als
Text-Dump aus. Chunk-Nutzdaten bleiben roh, in der Szene passiert nichts.

### Was drin ist

- **Container-Parser** (`src/xfbin_reader.*`), ohne Max-SDK-Abhängigkeit:
  Header, Chunk-Tabelle (Typen, Pfade, Namen, Maps, Referenzen,
  Map-Indices), Pages und Chunks inklusive Typauflösung über
  `maps[mapIndices[pageStart + localMapIndex]]`.
- **Plugin** `XfbinImport.dlu` mit `FPStaticInterface` unter `XfbinCpp.*`.
- **Werkzeug** `xfbindump.exe`, baut ohne Max SDK.
- **Referenz-Dumper** `tools/pydump.py` plus `tools/VERGLEICHE.bat`.
- Baubar für Max 2022–2027 über `BAUE_ALLE.bat`.

### Verifikation

Beide Testdateien zeilenweise gegen die Python-Lib geprüft:

| Datei | Pages | Chunks | Abweichung zum Python-Dump |
|---|---|---|---|
| `1hakbod1.xfbin` | 5 | 263 | 1 Zeile, erklärt |
| `1hakbod1c.xfbin` | 39 | 121 | 1 Zeile, erklärt |

Die eine Abweichung ist der doppelte `nuccChunkNull` in `page[0]`, den die
Python-Lib in ihrem page-lokalen dict verliert. Details in `README.md`.

### Bewusste Entscheidungen

**Der Parser hängt nicht am Max SDK.** Nur `<cstdint>`, `<string>`,
`<vector>`, `<iosfwd>` und Standard-Streams. Deshalb lässt er sich als
CLI-Werkzeug bauen und gegen die Python-Lib diffen, ohne dass 3ds Max
überhaupt startet. Der Preis ist eine cp932-Umwandlung an der
Plugin-Grenze statt im Parser — das ist der richtige Ort, weil die
Codepage-Umwandlung unter Windows über `MultiByteToWideChar` läuft und
den Parser sonst plattformabhängig machen würde.

**Strings bleiben rohe Bytes.** `RawString` ist ein `std::string` mit
cp932-Inhalt, nicht dekodiert. Erst `XfbinImportInterface` wandelt nach
UTF-16. Im Dump werden Nicht-ASCII-Bytes als `\xNN` escaped — dadurch ist
der Dump unabhängig von der Codepage des Terminals und trotzdem exakt
vergleichbar.

**Keine Exceptions nach außen.** Der `BeReader` setzt bei einem
Grenzverstoß ein Fehlerflag und liefert danach Nullen, statt zu werfen.
Das Plugin läuft im Max-Prozess; eine durchgereichte Exception dort ist
ein Absturz, kein Fehlerdialog.

**Plausibilitätsprüfung vor dem Reservieren.** Die Tabellen-Kopfzeile
wird gegen die Dateigröße geprüft, bevor `reserve()` läuft. Ohne das
könnte eine beschädigte Datei mit vier Milliarden Einträgen den
Speicher wegnehmen.

**Der doppelte Null-Chunk wird behalten.** Er steht so in der Datei. Ein
späterer Exporter muss ihn wieder schreiben — die erste Page hat ihn
immer.

### Aus AnimMerge übernommen

- `SceneRedrawGuard` und `HoldSuspendGuard` (in Stufe 0 noch ungenutzt)
- Diagnose-Muster: `lastError()` / `warnings()` / `log()` / `timings()`
  getrennt, `scratch_`-Member für `TYPE_STRING`-Rückgaben
- CMake-Gerüst: `/Zp8`, `MSVC_RUNTIME_LIBRARY` auf `MultiThreadedDLL`,
  x64-Pflichtcheck, `/external:W0`, abschaltbares `/W4 /permissive-`
- `.def`-Datei gegen die dekorierten Export-Namen
- `BAUE_ALLE.bat` mit Fehlerzeilen-Extraktion aus dem Build-Log
- ApplicationPlugins-Paket mit `PackageContents.xml`

### Nicht übernommen

Die `BoneFit`/`VerifyCurveFit`/`CalibrateAgainstBindPose`-Maschinerie aus
AnimMerge. Sie existiert dort, weil die FBX→Max-Abbildung mehrdeutig ist
und Max nur Root-Objekte dreht. XFBIN hat dieses Problem nicht: Position,
Euler in Grad und Scale stehen explizit und parent-relativ im
`nuccChunkCoord`. Da gibt es nichts zu messen.

### Nachtrag: Paketierung und Oberflaeche

Die erste Fassung liess sich nur ueber den Listener bedienen. Ergaenzt,
ohne dass das Plugin neu gebaut werden muss — die `.dlu` bleibt
unveraendert:

- `scripts/XfbinImport.mcr` — MacroScript mit Rollout: Datei waehlen,
  oeffnen, Inhalt anzeigen, Dump schreiben. Kategorie `DH Tools`.
- `scripts/XfbinMenu_2022_2024.ms` — Menueeintrag ueber `menuMan`.
- `scripts/XfbinMenu_2025_2027.ms` — Menueeintrag ueber den Callback
  `#cuiRegisterMenus` am neuen `CuiMenuManager`.
- `PackageContents.xml` um `macroscripts parts` und zwei
  `post-start-up scripts parts` erweitert.
- `INSTALLIERE.bat` baut `Contents\` jetzt aus `output\` **und**
  `scripts\`.

**Warum zwei Menue-Skripte.** 3ds Max 2025 hat das Menuesystem
umgebaut und `menuMan` abgeloest. Ein einziges Skript kann beide Wege
nicht bedienen. Die Versionsbereiche in der `PackageContents.xml`
ueberschneiden sich bewusst nicht — laufen beide, entsteht das Menue
doppelt.

**Warum post-start-up und nicht pre-start-up.** Zum Pre-Zeitpunkt gibt
es noch keine Menueleiste.

**Warum die Skripte nicht im Paketordner liegen.** `INSTALLIERE.bat`
loescht `package\XfbinImport\Contents\` bei jedem Lauf, damit keine
Reste aus einem frueheren Build ueberleben. Die Quellen liegen deshalb
unter `scripts\` und werden jedes Mal frisch hineinkopiert.

### Nachtrag 2: Starter-Skript und echter Sperrtest

- `scripts/XFBIN_Import.ms` — Starter zum Reinziehen ins Viewport, wie
  man es vom Animation Merge Tool kennt. Er enthaelt die Oberflaeche
  bewusst nicht selbst, sondern sucht `XfbinImport.mcr`, laedt sie per
  `fileIn` und ruft das Makro. Damit bleibt die Rollout-Definition an
  genau einer Stelle. Noetig ist der Starter nicht: die
  `PackageContents.xml` meldet die Oberflaeche als `macroscripts parts`
  bereits selbst an.

- `INSTALLIERE.bat` und `DEINSTALLIERE.bat`: die `tasklist`-Abfrage auf
  `3dsmax.exe` ist raus. Sie hat die falsche Bedingung geprueft — ob ein
  Prozess laeuft statt ob die Datei schreibbar ist — und schlug auch bei
  einer Erstinstallation an, bei der es ueberhaupt nichts zu sperren
  gibt. Jetzt wird eine vorhandene `.dlu` zum Anhaengen geoeffnet, ohne
  etwas zu schreiben; scheitert das, ist sie belegt. Existiert sie noch
  nicht, entfaellt die Pruefung. Notausgang: `set XFBIN_SKIP_LOCKCHECK=1`.

- `INSTALLIERE.bat` prueft am Ende, ob `PackageContents.xml`, die `.mcr`
  und die Menue-Skripte wirklich im Zielordner liegen. `xcopy` meldet
  nicht jeden Teilausfall.

- Kommentar in der `PackageContents.xml`: `UpgradeCode` bleibt fuer immer
  gleich, `ProductCode` gehoert laut Autodesk-Doku bei jeder Aenderung
  von `AppVersion` neu vergeben.

### Nachtrag 3: Startmeldung und Diagnose

Ein Listener-Protokoll mit mehreren geladenen Werkzeugen liess nicht
erkennen, ob XfbinImport ueberhaupt geladen wurde: die Menue-Skripte
schwiegen bei Erfolg, und die `.dlu` meldet sich erst, wenn man ihr
Utilities-Panel oeffnet. Fehlende Meldung hiess also weder ja noch nein.

- Beide Menue-Skripte schreiben jetzt beim Start eine Zeile in den
  Listener und melden dabei gleich mit, ob `XfbinCpp` erreichbar ist.
  Damit unterscheidet sich "Paket nicht geladen" von "Paket geladen,
  DLU fehlt".
- Neu: `tools/DIAGNOSE.ms` — prueft in einem Durchgang Paketordner,
  Max-Version samt zutreffendem Menueweg, DLU, Makro-Registrierung und
  Menue.

### Nachtrag 4: Oberflaeche ueberarbeitet

Die erste Fassung des Rollouts hatte zwei handfeste Layoutfehler.

**Der "..."-Knopf lag auf dem Eingabefeld.** Ursache: `across:2` teilt
den Rollout in zwei *gleich breite* Spalten. Das 330px-Feld in Spalte 1
ragte weit in Spalte 2 hinein, und der Knopf stand mit
Standardausrichtung mittig in Spalte 2 — also mitten auf dem Feld. Der
Knopf hat jetzt `align:#right`.

**Die Inhaltsliste war ein riesiger leerer Kasten.** `height` zaehlt
bei `listBox` Textzeilen, nicht Pixel; `height:14` waren also 14
Zeilen. Jetzt 11, passend zur laengsten bisher gesehenen
Typverteilung.

Weiter ueberarbeitet:

- Alles in `group`-Bloecke gefasst (Datei / Inhalt / Status) statt
  loser Controls.
- Kopfzeile zeigt Plugin- und Max-Version.
- Beschriftungen umlautfrei formuliert statt in Behelfsschreibweise:
  "Laden" / "Freigeben" statt "Oeffnen" / "Schliessen".
- Neu: Checkbox fuer das Listener-Protokoll (ruft `setDebug`),
  Knopf "Protokoll zeigen" (`log()`), Knopf "In die Zwischenablage".
- `windows.processPostedMessages()` vor dem Einlesen, damit die
  "Lese ..."-Zeile auch waehrend des Lesens steht und nicht erst
  danach.
- Statuszeile nennt jetzt den Dateinamen statt nur "Geladen."

Die Regeln dahinter stehen im README unter "MAXScript-UI".
