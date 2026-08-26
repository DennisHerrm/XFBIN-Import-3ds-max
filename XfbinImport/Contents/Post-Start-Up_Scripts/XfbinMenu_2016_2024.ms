-- ============================================================
--  XfbinMenu_2016_2024.ms
--  Menueeintrag fuer 3ds Max 2016 bis 2024 (altes menuMan-API)
--
--  Ab 2025 ist menuMan abgeloest - dafuer gibt es
--  XfbinMenu_2025_2027.ms. Die PackageContents.xml sorgt dafuer,
--  dass jede Max-Version nur ihr passendes Skript bekommt.
--
--  Ergebnis: Hauptmenue -> "DH Tools" -> "XFBIN Import"
-- ============================================================

(
  local sMenuName  = "DH Tools"
  local sItemTitle = "XFBIN Import"
  local sMacroName = "XfbinImport_Open"
  local sMacroCat  = "DH Tools"

  -- Ohne das C++ Plugin ist der Menueeintrag sinnlos. Er wird
  -- trotzdem angelegt: das Fenster meldet dann selbst, dass die
  -- DLU fehlt - das ist verstaendlicher als ein fehlender Eintrag.

  local mnuExisting = menuMan.findMenu sMenuName

  if (mnuExisting != undefined) then
  (
    -- Schon vorhanden. Nur pruefen, ob unser Eintrag drinsteht -
    -- "DH Tools" koennte auch von einem anderen Werkzeug stammen.
    local bFound = false

    for i = 1 to mnuExisting.numItems() while (not bFound) do
    (
      local itm = mnuExisting.getItem i
      if ((itm.getTitle()) == sItemTitle) then
      (
        bFound = true
      ) -- end of title check
    ) -- end of item loop

    if (not bFound) then
    (
      local actNew = menuMan.createActionItem sMacroName sMacroCat
      actNew.setTitle sItemTitle
      actNew.setUseCustomTitle true
      mnuExisting.addItem actNew -1
      menuMan.updateMenuBar()
    ) -- end of not found
  )
  else
  (
    local mnuBar = menuMan.getMainMenuBar()
    local mnuNew = menuMan.createMenu sMenuName

    local actNew = menuMan.createActionItem sMacroName sMacroCat
    actNew.setTitle sItemTitle
    actNew.setUseCustomTitle true
    mnuNew.addItem actNew -1

    local itmSub = menuMan.createSubMenuItem sMenuName mnuNew
    -- Ans Ende der Menueleiste, also hinter "Help".
    mnuBar.addItem itmSub (mnuBar.numItems())

    menuMan.updateMenuBar()
  ) -- end of menu exists check

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
