# XFBIN Import — Entwicklungsnotizen

Interne Notizen zum Aufbau, zu den Entscheidungen und zu den
Prüfwerkzeugen. Die Beschreibung für Benutzer steht in `README.md`.


Importer für CyberConnect2-XFBIN-Dateien (NUCC-Container).
Portierung des Blender-XFBIN-Importers 2.5.2 nach 3ds Max.

**Version 1.5.2 — Stufe 1.** Container, Skelett und Geometrie. `nuccChunkClump` und
`nuccChunkCoord` ergeben die Knochenhierarchie samt Bind-Pose, der in
`nuccChunkModel` eingebettete NUD-Block die Meshes mit UVs,
Vertexfarben und Normalen. Geskinnte Modelle bekommen einen Skin-Modifier mit
den Vertexgewichten aus dem NUD-Block. Texturen und Animationen kommen
in den Stufen 4 und 5.

Baut für 3ds Max **2016 bis 2027**.

---

## Ablauf

Modell und Animationen liegen in **getrennten Dateien**. Deshalb waehlt
man den ORDNER, nicht die Datei:

1. **Ordner...** -> das Verzeichnis mit den `.xfbin` waehlen
2. **Importieren** -> Bones, Meshes und Skinning in einem Zug
3. Animation aus der Liste waehlen -> **Apply selected**
   oder **Load all as sequence** fuer alle hintereinander

Welche Datei was ist, entscheidet der Inhalt: `nuccChunkClump` heisst
Modell, `nuccChunkAnm` heisst Animation. Der Dateiname spielt keine
Rolle.

**Mehrere Modelldateien sind der Normalfall.** Ein Charakter besteht
aus der Figur (`1hakbod1.xfbin`) und Zubehoer wie Waffen
(`1hakacc1.xfbin`). Alle werden geladen, jedes bringt sein eigenes
Skelett mit, und die Animationen sprechen beide an. Liegt das Zubehoer
nicht im Ordner, meldet der Import die uebersprungenen Eintraege mit
dem Namen des fehlenden Clumps.

**Dasselbe Modell kann mehrfach vorkommen.** Nennt der Anim-Container
einen Clump zweimal - zwei gleiche Waffen -, wird das Modell zweimal
angelegt. Bones und Objekte der zweiten Instanz bekommen den Zusatz
` #2`.

Das Skelett bleibt beim Dateiwechsel erhalten - es steht in der Szene,
nicht in der Datei. Die Zeile "Szene: 222 Bones (...)" unter Status
zeigt, was das Plugin gerade als Skelett kennt.

### Sequenzmodus

**Load all as sequence** legt Frame 0 als Bind-Pose an, haengt dann
alle Animationen mit einem einstellbaren Abstand hintereinander und
schreibt einen Note Track "animations" auf den Szenen-Wurzelknoten - zwei Keys
je Sequenz, am Anfang und am Ende. Dieselbe Form wie beim Animation
Merge Tool.

**Rest keys** sorgt dafuer, dass jede Sequenz fuer sich steht: Bones,
die eine Animation nicht anfasst, bekommen an beiden Enden einen Key
auf der Bind-Pose. Ohne das interpoliert Max quer durch die Luecke und
die vorige Animation blutet in die naechste - eine typische Animation
dieser Datei ruehrt nur 112 der 222 Bones an.

---

## Schnellstart

```bat
BAUE_ALLE.bat        :: baut Werkzeug + alle gefundenen Max-Versionen
INSTALLIERE.bat      :: baut das Paket und legt es in ApplicationPlugins ab
```

Installiert wird nach

```
%APPDATA%\Autodesk\Applicationplugins\XfbinImport
```

also `C:\Users\<du>\AppData\Roaming\Autodesk\ApplicationPlugins\XfbinImport`.
Das ist laut Autodesk-Doku der Ausweich-Ort fuer Benutzer ohne
Adminrechte; das Plugin ist dann nur fuer diesen Benutzer sichtbar.
3ds Max durchsucht den Ordner beim Start automatisch. Die beiden
anderen unterstuetzten Orte waeren `%ALLUSERSPROFILE%\Autodesk\ApplicationPlugins`
(fuer alle Benutzer, braucht Adminrechte) und
`%PROGRAMFILES%\Autodesk\ApplicationPlugins`.

**Ohne Installieren testen:** die Umgebungsvariable
`ADSK_APPLICATION_PLUGINS` auf den Paketordner setzen, dann Max aus
demselben Kommandozeilenfenster starten. Max liest den Pfad ab
Version 2019 und laedt das Paket von dort — praktisch, wenn du am
`.mcr` schraubst und nicht bei jeder Aenderung neu installieren
willst.

Nach dem Neustart von Max:

```
Hauptmenue  ->  DH Tools  ->  XFBIN Import
```

Der Eintrag steht ausserdem unter Customize -> Customize User Interface ->
Category **DH Tools** und laesst sich von dort auf eine Toolbar oder einen
Shortcut legen.

### Geht es? — Diagnose

Beim Start schreiben die Menue-Skripte jetzt eine Zeile in den Listener:

```
XFBIN Import 0.1.0: Plugin geladen, Menue-Callback angemeldet.
```

Steht die nicht da, wurde das Paket gar nicht angefasst. Steht dort
statt dessen `... aber XfbinImport.dlu FEHLT`, laufen die Skripte, aber
das Plugin nicht.

Fuer den vollstaendigen Durchgang `tools\DIAGNOSE.ms` ins Viewport
ziehen — Paketordner, Max-Version und Menueweg, DLU, Makro, Menue.

### Wenn du lieber ein Skript startest

`scripts\XFBIN_Import.ms` per Drag&Drop ins Viewport ziehen oder ueber
Scripting -> Run Script ausfuehren. Das ist derselbe Weg wie beim
Animation Merge Tool.

Der Starter enthaelt die Oberflaeche **nicht** selbst — er sucht
`XfbinImport.mcr` (neben sich, im Paketordner, unter
ApplicationPlugins), laedt sie und ruft das Makro auf. Zwei Kopien
derselben Rollout-Definition waeren genau die Sorte Duplikat, die
spaeter auseinanderlaeuft.

Alternativ alles im Listener:

```maxscript
XfbinCpp.version()
XfbinCpp.setDebug 1
XfbinCpp.open @"D:\xfbin\1hakbod1.xfbin"
format "%\n" (XfbinCpp.summary())
format "%\n" (XfbinCpp.timings())
```

Erwartete Ausgabe fuer `1hakbod1.xfbin`:

```
Pages: 5 | Chunks: 263 | Typen: 9 | Namen: 241
  nuccChunkClump = 1
  nuccChunkCoord = 222
  nuccChunkDynamics = 1
  nuccChunkMaterial = 6
  nuccChunkModel = 19
  nuccChunkNull = 6
  nuccChunkPage = 5
  nuccChunkTexture = 3
```

---

## Das Paket

Was nach der Installation im Zielordner liegt:

```
XfbinImport/
  PackageContents.xml
  Contents/
    2022/XfbinImport.dlu ... 2027/XfbinImport.dlu
    MacroScripts/XfbinImport.mcr
    MacroScripts/XFBIN_Import.ms
    Post-Start-Up_Scripts/XfbinMenu_2016_2024.ms
    Post-Start-Up_Scripts/XfbinMenu_2025_2027.ms
```

`PackageContents.xml` meldet drei Arten von Bestandteilen an:

| Abschnitt | Inhalt | Versionen |
|---|---|---|
| `plugins parts` | die `.dlu`, ein Block pro Max-Version | je 2016 … 2027 einzeln |
| `macroscripts parts` | `XfbinImport.mcr` — die Oberflaeche | 2016–2027 gemeinsam |
| `post-start-up scripts parts` | Menueeintrag | **zwei** Skripte, siehe unten |

**Warum zwei Menue-Skripte.** 3ds Max 2025 hat das Menuesystem
umgebaut: das alte `menuMan`-API ist dort abgeloest, Menues werden
ueber den Callback `#cuiRegisterMenus` beim neuen `CuiMenuManager`
registriert. `XfbinMenu_2016_2024.ms` benutzt den alten Weg,
`XfbinMenu_2025_2027.ms` den neuen. Die Versionsbereiche in der
`PackageContents.xml` ueberschneiden sich nicht — laeuft beides,
entsteht das Menue doppelt.

Beide Skripte laufen als **post**-start-up, nicht pre: vorher gibt es
noch keine Menueleiste.

Die `.dlu` bekommt pro Max-Version einen eigenen Block, weil jede
Version gegen ihr eigenes SDK gebaut ist. Eine DLU aus 2024 in 2026 zu
laden endet im Absturz, nicht in einer Meldung.

Die Skripte liegen im Quellbaum unter `scripts\` und werden bei jedem
`INSTALLIERE.bat` frisch ins Paket kopiert. Ein Bearbeiten von
`scripts\XfbinImport.mcr` ueberlebt also den naechsten Installationslauf.

---


## Was gebaut wird

| Ziel | Datei | Braucht Max SDK? |
|---|---|---|
| Plugin | `output\<Version>\XfbinImport.dlu` | ja |
| Werkzeug | `output\tools\xfbindump.exe` | **nein** |

`BAUE_ALLE.bat` baut das Werkzeug zuerst. Steckt ein Fehler im Parser,
fällt er dort sofort auf, statt nach sechs SDK-Builds.

Nur das Werkzeug bauen, ohne installiertes Max SDK:

```bat
cmake -B build_tool -A x64 -DXFBIN_BUILD_PLUGIN=OFF
cmake --build build_tool --config Release
```

### Einstellungen

Alle über Umgebungsvariablen, vor `BAUE_ALLE.bat` gesetzt:

| Variable | Standard | Zweck |
|---|---|---|
| `XFBIN_GENERATOR` | wird über `vswhere` ermittelt | z. B. `Visual Studio 17 2022` |
| `XFBIN_STRICT` | `ON` | `/W4 /permissive-`; auf `OFF`, falls ältere SDK-Header damit nicht durchgehen |
| `STOP_ON_ERROR` | `1` | `0` = trotz Fehler alle Versionen durchbauen |
| erstes Argument | – | `BAUE_ALLE.bat 2019` baut nur diese eine Version |

Die SDK-Pfade folgen dem Autodesk-Standard
`C:\Program Files\Autodesk\3ds Max <Jahr> SDK\maxsdk`. Nicht
installierte Versionen werden übersprungen und in der Zusammenfassung
als "kein SDK installiert" ausgewiesen — nicht als Fehler.

**Warum C++17 und nicht C++20:** gebraucht wird davon nur
`std::to_chars` für die locale-feste Zahlenausgabe. C++17 lässt sich
auch gegen die Header der älteren SDKs übersetzen. Geprüft: die Dumps
sind mit C++17 zeichengleich mit denen aus einem C++20-Build.

**`/permissive-` wird ermittelt, nicht geraten.** Ältere SDK-Header
gehen unter der Konformitätsprüfung nicht durch — das liegt an den
Headern, nicht am eigenen Code. Wo die Grenze liegt, steht nirgends
verbindlich, also übersetzt CMake beim Konfigurieren einmal ein
`#include <max.h>` mit `/permissive-` und entscheidet danach. Die
Konfigurationsausgabe sagt, was gewählt wurde. `/W4` gilt immer.

---

## MaxScript-API

Alles unter `XfbinCpp.*`.

| Funktion | Rückgabe | Bedeutung |
|---|---|---|
| `open <path>` | int | Anzahl Chunks, `-1` bei Fehler |
| `close()` | int | gibt die geladene Datei frei |
| `isOpen()` | int | 0/1 |
| `dump <outPath> <includeTables>` | int | 1 = geschrieben |
| `pageCount()` | int | |
| `chunkCount()` | int | |
| `countOfType <typeName>` | int | z. B. `"nuccChunkModel"` |
| `namesOfType <typeName>` | string | Namen, `\n`-getrennt |
| `summary()` | string | Zählwerte und Typverteilung |
| `version()` | string | |
| `lastError()` | string | `""` = kein Fehler |
| `warnings()` | string | nicht-fatale Hinweise |
| `log()` | string | gesammeltes Protokoll |
| `timings()` | string | Phasenzeiten in ms |
| `setDebug <0\|1>` | int | 1 = Protokoll zusätzlich in den Listener |

### Stufe 1 — Skelett

| Funktion | Rückgabe | Bedeutung |
|---|---|---|
| `parseSkeleton()` | int | Anzahl Bones, `-1` bei Fehler |
| `clumpCount()` | int | |
| `boneCount()` | int | |
| `boneSummary()` | string | Bones, Wurzeln, Tiefe je Clump |
| `boneDump <outPath>` | int | Skelett-Dump zum Diffen |
| `buildSkeleton <mode> <scale>` | int | angelegte Knoten |

`mode`: 0 = Point-Helper (Standard), 1 = Bone-Objekte.
`scale`: 1.0 übernimmt die Zentimeter aus der Datei 1:1.

Die Abfragen werten das Skelett bei Bedarf selbst aus — `parseSkeleton()`
muss man nicht von Hand aufrufen.

`lastError()` bleibt bewusst frei von Warnungen: leer heißt, es ist
wirklich nichts schiefgegangen.

---

## Gegenprüfen gegen die Python-Lib

Das ist der eigentliche Sinn von Stufe 0.

```bat
set XFBIN_ADDON=D:\dev\Blender-XFBIN-Importer-2.5.2
tools\VERGLEICHE.bat D:\xfbin\1hakbod1.xfbin
```

`tools\pydump.py` erzeugt aus der Python-Lib denselben Zeilenaufbau wie
`xfbindump.exe`. Beide Dumps werden mit `fc` verglichen.

**Eine erwartete Abweichung.** Die erste Page jeder Datei enthält zwei
`nuccChunkNull`. Die Python-Lib legt die Chunks einer Page in einem dict
ab, das über den page-lokalen Map-Index geht — beide Nulls haben Index 0,
der zweite überschreibt also den ersten. `xfbindump.exe` zeigt beide.
Der C++-Dump hat deshalb genau eine Zeile mehr, in `page[0]`.

Gemessen an den Testdateien:

| Datei | Zeilen Python | Zeilen C++ | Abweichungen |
|---|---|---|---|
| `1hakbod1.xfbin` | 1056 | 1057 | nur `page[0]` |
| `1hakbod1c.xfbin` | 17983 | 17984 | nur `page[0]` |

Alles andere — Header, alle drei Stringtabellen, Chunk-Maps, Referenzen,
Map-Indices, Page-Größen, Chunk-Versionen und Chunk-Größen — ist
zeichengleich.

### Skelett (Stufe 1)

```bat
tools\VERGLEICHE_BONES.bat D:\xfbin\1hakbod1.xfbin
```

`tools\pydump_bones.py` rechnet die Bone-Matrizen so, wie
`blender/importer.py` es tut — in Spaltenvektor-Konvention mit
`Euler(rot, 'ZYX')` und `world = parent @ local` — und gibt sie
transponiert aus, damit sie sich mit dem Zeilenvektor-Dump von
`xfbindump.exe --bones-o` direkt vergleichen lassen.

Zahlen werden mit `std::to_chars` formatiert, nicht mit `snprintf` -
Letzteres folgt der Prozess-Locale, und 3ds Max stellt die auf die
Systemsprache um. Auf einem deutschen Windows kam sonst `0,000000`
statt `0.000000` heraus.

Hier gibt es **keine** bekannte Abweichung. Gemessen an
`1hakbod1.xfbin`: 1781 Zeilen, 222 Bones, alle Weltmatrizen — null
Unterschiede.

Ein Unterschied ist Absicht und in `pydump_bones.py` dokumentiert:
Blender rechnet Positionen mit `pos_cm_to_m` in Meter um, das
Max-Plugin behält Zentimeter. Beide Dumps sind deshalb in Zentimeter,
also 100× größer als das, was in Blender im N-Panel steht.

---

## Aufbau

```
src/
  xfbin_reader.h/.cpp     Container-Parser. Haengt NICHT am Max SDK.
  xfbin_clump.h/.cpp      Clump/Coord, Hierarchie, Matrizen. Auch SDK-frei.
  xfbin_nud.h/.cpp        NUD-Meshes, Vertexformat, Streifen. Auch SDK-frei.
  xfbin_anm.h/.cpp        Animationskurven, Quantisierung. Auch SDK-frei.
  xfbin_tex.h/.cpp        NUT-Texturen, Materialien, DDS. Auch SDK-frei.
  xfbinimport.h/.cpp      FPStaticInterface, Diagnose, cp932-Umwandlung.
  xfbinimport_dll.cpp     DLL-Entry, ClassDesc.
scripts/
  XfbinImport.mcr         Oberflaeche (MacroScript + Rollout)
  XFBIN_Import.ms         Starter fuer Drag&Drop / Run Script
  XfbinMenu_2016_2024.ms  Menueeintrag, altes menuMan-API
  XfbinMenu_2025_2027.ms  Menueeintrag, CuiMenuManager-Callback
tools/
  DIAGNOSE.ms             prueft Paket, DLU, Makro und Menue
  GROESSTE_OBJEKTE.ms     nennt das groesste Objekt der Szene
  BONE_CHECK.ms           prueft die Bone-Matrizen auf Muellwerte
  NOTE_CHECK.ms           listet die Note Tracks der Szene
  VIS_CHECK.ms            prueft Sichtbarkeits-Keys je Sequenz
  PRUEFE_SCRIPTS.py       Linter fuer die MaxScript-Dateien
  PRUEFE_API.py           prueft die Plugin-API auf Vollstaendigkeit
  PRUEFE_INCLUDES.py      prueft, ob benutzte std-Typen eingebunden sind
  VERSION_SETZEN.py       setzt alle Versionsstellen auf einmal
  xfbindump_main.cpp      CLI-Werkzeug (ohne Max SDK)
  pydump.py               Referenz-Dump aus der Python-Lib
  pydump_bones.py         Referenz-Skelettdump, Blenders Rechenweg
  pydump_meshes.py        Referenz-Meshdump
  pydump_anims.py         Referenz-Animationsdump
  VERGLEICHE.bat          Container-Dumps erzeugen und diffen
  VERGLEICHE_BONES.bat    Skelett-Dumps erzeugen und diffen
  VERGLEICHE_MESHES.bat   Mesh-Dumps erzeugen und diffen
  VERGLEICHE_ANIMS.bat    Anim-Dumps erzeugen und diffen
package/XfbinImport/
  PackageContents.xml     ApplicationPlugins-Manifest, 2022-2027
```

Die Trennung ist Absicht: `xfbin_reader` kennt nur `<cstdint>`,
`<string>`, `<vector>`, `<iosfwd>` und die Standard-Streams. Dadurch
lässt sich der Parser im Plugin, im CLI-Werkzeug und notfalls auf einem
ganz anderen Rechner bauen. Bitte nicht mit Max-Typen „verbessern".

---

## Bekannte Einschränkungen dieser Stufe

- Ausgewertet sind bisher nur `nuccChunkClump` und `nuccChunkCoord`.
  Alle anderen Chunk-Nutzdaten liegen weiter roh in `XfbinChunk::data`.
- In der Szene entstehen nur die Knoten des Skeletts, keine Geometrie.
- Bone-Objekte (`mode 1`) werden ohne gesetzte Breite/Höhe/Länge
  angelegt — die Standardwerte des Bone-Objekts bleiben stehen.
- Die Datei bleibt komplett im Speicher, solange sie offen ist. Bei den
  Testdateien sind das 3,8 MB bzw. 1,1 MB — unkritisch, aber `close()`
  gibt sie frei.
- CPK-komprimierte XFBINs werden erkannt und mit klarer Meldung
  abgelehnt, nicht entpackt.
- Das Fenster zeigt den Container an, importiert aber nichts in die
  Szene — das kommt ab Stufe 1.
- Unter 2025+ kann es sein, dass das Menue erst nach einem zweiten
  Start von Max erscheint: das Post-Startup-Skript meldet den
  Callback an, und der greift beim naechsten Menueaufbau. Der
  MacroScript-Eintrag unter Customize ist sofort da.

---

## Nächste Stufen

| Stufe | Inhalt |
|---|---|
| ~~**1**~~ | ~~`nuccChunkClump` + `nuccChunkCoord`, Bones anlegen~~ — **erledigt in 0.2.0** |
| ~~**2**~~ | ~~NUD-Meshes~~ - **erledigt in 0.3.0** |
| ~~**3**~~ | ~~Skinning über `ISkinImportData`~~ - **erledigt in 0.4.0** |
| ~~**4**~~ | ~~Texturen (NUT/DDS) und Materialien~~ - **erledigt in 0.9.0** |
| ~~**5**~~ | ~~`nuccChunkAnm`~~ - **erledigt in 0.5.0** |
| **6** | Custom Attributes, Round-Trip |

Ab Stufe 1 werden `SceneRedrawGuard` und `HoldSuspendGuard` aus
`xfbinimport.h` gebraucht. Sie stehen schon dort, damit niemand auf die
Idee kommt, `DisableSceneRedraw`/`EnableSceneRedraw` von Hand zu paaren.

---

## MAXScript-UI: die Regeln, die hier gelten

Vor jedem Ausliefern:

```bat
python tools\PRUEFE_SCRIPTS.py
```

Prueft Nicht-ASCII, C-Kommentare, Klammerbilanz und Vorwaertsreferenzen
auf `fn` - also genau die vier Fehlerarten, die in diesem Projekt
vorgekommen sind und die man von Hand nicht zuverlaessig sieht.


Fuer `scripts/XfbinImport.mcr` und alles, was spaeter dazukommt.

**`across:N` teilt in gleich breite Spalten.** Nicht in Spalten nach
Inhaltsbreite. Ein 334px-Feld in Spalte 1 eines 452px-Rollouts ragt
also weit in Spalte 2 hinein. Was daneben steht, braucht zwingend
`align:#right` — sonst sitzt es mittig in seiner Spalte und liegt auf
dem Feld. Genau das war der Fehler in der ersten Fassung.

**`height` zaehlt bei Listen Textzeilen, nicht Pixel.** Fuer genau N
sichtbare Zeilen: `listBox height:N`, aber `comboBox`/`dropDownList`
brauchen `height:N+2`. Ein `height:14` beim ersten Versuch ergab einen
14 Zeilen hohen leeren Kasten.

**Kein `pos:[x,y]`.** Absolute Positionen skalieren nicht mit der
Windows-Anzeigeskalierung mit, der automatische Fluss von MAXScript tut
es. Feinschliff ausschliesslich ueber `offset:`.

**`group "..." ( ... )` statt loser Controls.** Das ist der Rahmen, den
Max in seinen eigenen Panels ueberall benutzt; lose Labels und Knoepfe
sehen sofort fremd aus.

**Kommentare mit `--`, nicht `//`.** MAXScript kennt `//` nicht; es
ergibt `Syntax error: at /, expected <rollout clause>`.

**Keine Umlaute im Quelltext.** Die Kodierung von `.mcr`-Dateien ist
ueber die Max-Versionen hinweg nicht verlaesslich. Beschriftungen
werden umlautfrei formuliert ("Laden", "Freigeben", "Suchen...") statt
mit Behelfsschreibweisen wie "Oeffnen".

**Matrixkonvention gehoert kommentiert, nicht erraten.** Der Block oben
in `xfbin_clump.h` erklaert einmal ausfuehrlich, warum aus Blenders
`world = parent @ local` in Max `world = local * parent` wird. Wer das
spaeter anfasst, soll es nicht neu herleiten muessen.

**Zustand gehoert sichtbar.** Kopfzeile zeigt Plugin-Version und
Max-Version, unten stehen Statuszeile und Zeitmessung. Knoepfe, die
gerade nichts tun koennen, sind `enabled:false` statt zu einer
Fehlermeldung zu fuehren.

**Funktionen stehen vor ihren Aufrufern.** MAXScript loest Namen in
einem Rollout streng von oben nach unten auf. Eine weiter unten
definierte Funktion ist beim Aufruf `undefined` - und das faellt erst
zur Laufzeit auf, als "Call needs function or class, got: undefined".
Beim Einfuegen einer neuen Hilfsfunktion also immer pruefen, wer sie
aufruft.

**Lange Aktionen brauchen `windows.processPostedMessages()`.** Ohne den
Aufruf erscheint eine "Lese ..."-Zeile erst, wenn das Lesen schon
vorbei ist.
