-- ============================================================
--  XfbinMenu_2016_2024.ms
--  Menueeintraege fuer 3ds Max 2016 bis 2024 (altes menuMan-API)
--  Version 3.0.0
--
--  Ab 2025 ist menuMan abgeloest - dafuer gibt es
--  XfbinMenu_2025_2027.ms. Die PackageContents.xml sorgt dafuer,
--  dass jede Max-Version nur ihr passendes Skript bekommt.
--
--  Ergebnis: Hauptmenue -> "DH Tools" -> "XFBIN Import"
--                                     -> "XFBIN Animations"
-- ============================================================

(
  local sMenuName = "DH Tools"
  local sMacroCat = "DH Tools"
  local aItems = #(#("XFBIN Import", "XfbinImport_Open"), #("XFBIN Animations", "XfbinImport_Anim"))

  -- "DH Tools" kann auch von einem anderen Werkzeug stammen -
  -- dann kommen nur die fehlenden Eintraege dazu.
  local mnu = menuMan.findMenu sMenuName

  if (mnu == undefined) then
  (
    local mnuBar = menuMan.getMainMenuBar()
    mnu = menuMan.createMenu sMenuName
    local itmSub = menuMan.createSubMenuItem sMenuName mnu
    -- Ans Ende der Menueleiste, also hinter "Help".
    mnuBar.addItem itmSub (mnuBar.numItems())
  ) -- end of menu exists check

  for aItem in aItems do
  (
    local bFound = false
    for i = 1 to mnu.numItems() while (not bFound) do
    (
      if (((mnu.getItem i).getTitle()) == aItem[1]) then
      (
        bFound = true
      ) -- end of title check
    ) -- end of item loop

    if (not bFound) then
    (
      local actNew = menuMan.createActionItem aItem[2] sMacroCat
      actNew.setTitle aItem[1]
      actNew.setUseCustomTitle true
      mnu.addItem actNew -1
    ) -- end of not found
  ) -- end of entry loop

  menuMan.updateMenuBar()

  -- Startmeldung. Ohne die laesst sich im Listener nicht
  -- unterscheiden, ob das Paket geladen wurde und still war, oder
  -- ob es gar nicht erst angefasst wurde.
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
    format "XFBIN Import: Menue registriert, aber XfbinImport.dlu FEHLT.\n"
  )
  else
  (
    format "XFBIN Import %: Plugin geladen, Menue registriert (menuMan).\n" sVer
  ) -- end of plugin check
)
