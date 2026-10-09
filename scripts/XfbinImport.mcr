-- ============================================================
--  XfbinImport.mcr - Menue "Ninja Storm Importer"
--  Version 3.0.0
--
--  Seit 3.0 sind beide Fenster C++ (XfbinImport.dlu). Diese Datei
--  oeffnet sie nur noch:
--
--    "Import Ninja Storm"      das Figurenfenster     (XfbinCpp.showDialog)
--    "Ninja Storm Animations"  das Animationsfenster  (XfbinCpp.showAnimDialog)
--
--  Kategorie und Beschriftung wie bei den anderen Importern
--  ("Dragon Ball Legends Tool", "Shinobi Striker Tool"). Bis 2.x
--  hiess die Kategorie "DH Tools"; die Makronamen sind geblieben.
--
--  ASCII only. Before shipping: python tools\PRUEFE_SCRIPTS.py
-- ============================================================

macroScript XfbinImport_Open
  category:"Ninja Storm Importer"
  buttonText:"Import Ninja Storm"
  toolTip:"Ninja Storm Importer - characters straight from Storm 1, 2, 3, 4 and Connections"
(
  on execute do
  (
    local oX = undefined
    try (if (XfbinCpp != undefined) then oX = XfbinCpp) catch (oX = undefined)
    if (oX != undefined) then
    (
      oX.showDialog()
    )
    else
    (
      messageBox "XfbinImport.dlu is not loaded.\n\nRun the setup again and restart 3ds Max." title:"Ninja Storm Importer"
    ) -- end of plugin check
  ) -- end of execute
) -- end of XfbinImport_Open

macroScript XfbinImport_Anim
  category:"Ninja Storm Importer"
  buttonText:"Ninja Storm Animations"
  toolTip:"Ninja Storm Importer - load the animations of the imported character"
(
  on execute do
  (
    local oX = undefined
    try (if (XfbinCpp != undefined) then oX = XfbinCpp) catch (oX = undefined)
    if (oX != undefined) then
    (
      oX.showAnimDialog()
    )
    else
    (
      messageBox "XfbinImport.dlu is not loaded.\n\nRun the setup again and restart 3ds Max." title:"Ninja Storm Importer"
    ) -- end of plugin check
  ) -- end of execute
) -- end of XfbinImport_Anim
