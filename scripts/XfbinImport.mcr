-- ============================================================
--  XfbinImport.mcr - Menue "DH Tools"
--  Version 3.0.0
--
--  Seit 3.0 sind beide Fenster C++ (XfbinImport.dlu). Diese Datei
--  oeffnet sie nur noch:
--
--    "XFBIN Import"      das Figurenfenster     (XfbinCpp.showDialog)
--    "XFBIN Animations"  das Animationsfenster  (XfbinCpp.showAnimDialog)
--
--  Die Makronamen sind dieselben wie bis 2.x - Menues und
--  Toolbar-Knoepfe, die darauf zeigen, funktionieren weiter.
--
--  ASCII only. Before shipping: python tools\PRUEFE_SCRIPTS.py
-- ============================================================

macroScript XfbinImport_Open
  category:"DH Tools"
  buttonText:"XFBIN Import"
  toolTip:"XFBIN Import - Naruto Storm characters straight from the game"
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
      messageBox "XfbinImport.dlu is not loaded.\n\nRun the setup again and restart 3ds Max." title:"XFBIN Import"
    ) -- end of plugin check
  ) -- end of execute
) -- end of XfbinImport_Open

macroScript XfbinImport_Anim
  category:"DH Tools"
  buttonText:"XFBIN Animations"
  toolTip:"XFBIN Import - load the animations of the imported character"
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
      messageBox "XfbinImport.dlu is not loaded.\n\nRun the setup again and restart 3ds Max." title:"XFBIN Import"
    ) -- end of plugin check
  ) -- end of execute
) -- end of XfbinImport_Anim
