/*
    XFBIN Import  --  Diagnose
    ==========================
    Ins Viewport ziehen oder ueber Scripting -> Run Script starten.
    Beantwortet in einem Durchgang: ist das Paket da, ist die DLU
    geladen, ist das Makro registriert, steht das Menue.
*/
(
  format "\n=== XFBIN Import Diagnose ===\n"

  -- 1. Paketordner
  local sAppData = systemTools.getEnvVariable "APPDATA"
  local sPkg = sAppData + "\\Autodesk\\ApplicationPlugins\\XfbinImport"

  format "1) Paketordner\n"
  if (doesFileExist (sPkg + "\\PackageContents.xml")) then
  (
    format "   OK   %\n" sPkg
    local iDlu = 0
    for v in #(2022, 2023, 2024, 2025, 2026, 2027) do
    (
      if (doesFileExist (sPkg + "\\Contents\\" + (v as string) + "\\XfbinImport.dlu")) then
      (
        iDlu += 1
      ) -- end of dlu check
    ) -- end of version loop
    format "   %  DLU-Ordner gefunden\n" iDlu

    if (doesFileExist (sPkg + "\\Contents\\MacroScripts\\XfbinImport.mcr")) then
    (
      format "   OK   XfbinImport.mcr liegt im Paket\n"
    )
    else
    (
      format "   FEHLT  XfbinImport.mcr - INSTALLIERE.bat erneut laufen lassen\n"
    ) -- end of mcr check
  )
  else
  (
    format "   FEHLT  %\n" sPkg
    format "          INSTALLIERE.bat wurde nicht ausgefuehrt.\n"
  ) -- end of package check

  -- 2. Max-Version
  format "2) 3ds Max %  (Menueweg: %)\n" \
    (maxVersion())[8] \
    (if ((maxVersion())[8] >= 2025) then "cuiRegisterMenus" else "menuMan")

  -- 3. C++ Plugin
  format "3) Plugin\n"
  local sVer = undefined
  try
  (
    sVer = XfbinCpp.version()
  )
  catch
  (
    sVer = undefined
  ) -- end of try/catch

  if (sVer == undefined) then
  (
    format "   FEHLT  XfbinCpp ist nicht da - XfbinImport.dlu wurde nicht geladen.\n"
    format "          Pruefen unter: Customize -> Plugin Manager, nach 'Xfbin' suchen.\n"
  )
  else
  (
    format "   OK   XfbinCpp.version() = \"%\"\n" sVer
  ) -- end of plugin check

  -- 4. Makro
  format "4) Makro\n"
  local bMacro = false
  try
  (
    bMacro = (macros.exists "DH Tools" "XfbinImport_Open")
  )
  catch
  (
    bMacro = false
  ) -- end of try/catch

  if (bMacro) then
  (
    format "   OK   XfbinImport_Open in Kategorie 'DH Tools' registriert\n"
  )
  else
  (
    format "   FEHLT  Makro nicht registriert.\n"
    format "          scripts\\XFBIN_Import.ms starten oder Max neu starten.\n"
  ) -- end of macro check

  -- 5. Menue (nur bis 2024 abfragbar - ab 2025 gibt es menuMan nicht mehr)
  format "5) Menue\n"
  if ((maxVersion())[8] < 2025) then
  (
    if ((menuMan.findMenu "DH Tools") != undefined) then
    (
      format "   OK   'DH Tools' vorhanden\n"
    )
    else
    (
      format "   FEHLT  'DH Tools' nicht in der Menueleiste\n"
    ) -- end of menu check
  )
  else
  (
    format "   Ab Max 2025 nicht per Skript abfragbar - bitte in der\n"
    format "   Menueleiste nach 'DH Tools' schauen.\n"
  ) -- end of version check

  format "=== Ende ===\n\n"
)
