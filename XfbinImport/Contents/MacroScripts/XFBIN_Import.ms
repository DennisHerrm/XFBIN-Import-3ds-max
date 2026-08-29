/*
    XFBIN Import  --  Starter fuer die Oberflaeche
    ==============================================
    Version 2.1.6

    Diese Datei brauchst du im Normalfall NICHT.

    Anders als beim Animation Merge Tool meldet die
    PackageContents.xml von XfbinImport die Oberflaeche selbst an
    ("macroscripts parts"). Nach INSTALLIERE.bat und einem
    Max-Neustart steht sie also schon im Menue:

        Hauptmenue  ->  DH Tools  ->  XFBIN Import

    und ausserdem unter
        Customize -> Customize User Interface -> Category "DH Tools",
    von wo aus du sie auf eine Toolbar oder einen Shortcut legen
    kannst.

    Dieser Starter ist fuer die Faelle, in denen das nicht greift:
      - der Menue-Callback ist beim ersten Start noch nicht durch
        (betrifft Max 2025+, siehe README)
      - du arbeitest direkt aus dem Quellordner, ohne zu installieren
      - du willst es einfach so machen wie beim FBX-Tool

    Die Oberflaeche selbst steht NICHT hier drin, sondern nur in
    XfbinImport.mcr. Zwei Kopien derselben Rollout-Definition waeren
    genau die Sorte Duplikat, die spaeter auseinanderlaeuft. Dieser
    Starter sucht die .mcr, laedt sie und ruft das Makro auf.

    Benoetigt das C++ Plugin XfbinImport.dlu.
*/

(
  local sMacroName = "XfbinImport_Open"
  local sMacroCat  = "DH Tools"
  local sMcrName   = "XfbinImport.mcr"

  -- ----------------------------------------------------------
  --  Laeuft das Makro schon? Dann direkt starten.
  -- ----------------------------------------------------------
  fn XfbinRunMacro =
  (
    local bOk = false
    try
    (
      macros.run sMacroCat sMacroName
      bOk = true
    )
    catch
    (
      bOk = false
    ) -- end of try/catch
    bOk
  ) -- end of XfbinRunMacro

  -- ----------------------------------------------------------
  --  Kandidatenpfade fuer die .mcr, in dieser Reihenfolge:
  --    1. neben dieser .ms          (Quellordner scripts\)
  --    2. Paketordner im Quellbaum  (nach INSTALLIERE.bat)
  --    3. installiertes Paket, Benutzerpfad
  --    4. installiertes Paket, alle Benutzer
  -- ----------------------------------------------------------
  fn XfbinFindMcr =
  (
    local sHere = ""
    try
    (
      sHere = getFilenamePath (getThisScriptFilename())
    )
    catch
    (
      sHere = ""
    ) -- end of try/catch

    local sAppData = systemTools.getEnvVariable "APPDATA"
    local sAllUser = systemTools.getEnvVariable "ALLUSERSPROFILE"

    local aCandidates = #()

    if (sHere != "" and sHere != undefined) then
    (
      append aCandidates (sHere + sMcrName)
      append aCandidates (sHere + "..\\package\\XfbinImport\\Contents\\MacroScripts\\" + sMcrName)
    ) -- end of script path check

    if (sAppData != undefined and sAppData != "") then
    (
      append aCandidates (sAppData + \
        "\\Autodesk\\ApplicationPlugins\\XfbinImport\\Contents\\MacroScripts\\" + sMcrName)
    ) -- end of appdata check

    if (sAllUser != undefined and sAllUser != "") then
    (
      append aCandidates (sAllUser + \
        "\\Autodesk\\ApplicationPlugins\\XfbinImport\\Contents\\MacroScripts\\" + sMcrName)
    ) -- end of allusers check

    local sFound = undefined

    for sPath in aCandidates while (sFound == undefined) do
    (
      if (doesFileExist sPath) then
      (
        sFound = sPath
      ) -- end of exists check
    ) -- end of candidate loop

    sFound
  ) -- end of XfbinFindMcr

  -- ----------------------------------------------------------
  --  Ablauf
  -- ----------------------------------------------------------
  format "=== XFBIN Import - Starter ===\n"

  -- Plugin-Hinweis vorweg. Das Fenster meldet es zwar selbst, aber
  -- im Listener steht es dann auch fuer spaeter.
  local sPluginVer = undefined
  try
  (
    sPluginVer = XfbinCpp.version()
  )
  catch
  (
    sPluginVer = undefined
  ) -- end of try/catch

  if (sPluginVer == undefined) then
  (
    format "  WARNUNG: XfbinImport.dlu ist nicht geladen.\n"
    format "  Das Fenster oeffnet sich, kann aber nichts lesen.\n"
    format "  Pruefen unter: Customize -> Plugin Manager\n"
  )
  else
  (
    format "  Plugin % geladen\n" sPluginVer
  ) -- end of plugin check

  if (XfbinRunMacro()) then
  (
    format "  Oberflaeche geoeffnet (Makro war bereits registriert).\n"
  )
  else
  (
    local sMcr = XfbinFindMcr()

    if (sMcr == undefined) then
    (
      format "  FEHLER: % wurde nicht gefunden.\n" sMcrName
      format "  Gesucht wurde neben dieser Datei, im Paketordner und\n"
      format "  unter ApplicationPlugins. Bitte INSTALLIERE.bat laufen\n"
      format "  lassen oder diese .ms neben die .mcr legen.\n"
      messageBox ("XfbinImport.mcr nicht gefunden.\n\n" + \
                  "Bitte INSTALLIERE.bat ausfuehren oder diese Datei\n" + \
                  "neben XfbinImport.mcr legen.") \
                 title:"XFBIN Import"
    )
    else
    (
      format "  Lade %\n" sMcr
      fileIn sMcr

      if (XfbinRunMacro()) then
      (
        format "  Oberflaeche geoeffnet.\n"
      )
      else
      (
        format "  FEHLER: Makro liess sich nach dem Laden nicht starten.\n"
      ) -- end of second run check
    ) -- end of mcr found check
  ) -- end of first run check
)
