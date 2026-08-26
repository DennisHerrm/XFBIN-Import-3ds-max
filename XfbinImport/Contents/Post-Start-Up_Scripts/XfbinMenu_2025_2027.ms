-- ============================================================
--  XfbinMenu_2025_2027.ms
--  Menueeintrag fuer 3ds Max 2025 und neuer
--
--  Ab 2025 ist das alte menuMan-API abgeloest. Menues werden
--  ueber einen Callback registriert, den der neue
--  CuiMenuManager ausloest (#cuiRegisterMenus). Das Skript
--  haengt sich dort ein, statt das Menue direkt zu bauen.
--
--  Die GUIDs unten sind fest und einmalig. Nicht aendern -
--  Max erkennt den Eintrag darueber wieder.
--
--  647394 ist die Action-Table-ID fuer MacroScripts. Der
--  "permanentId" ist der Makroname, ein Backtick und die
--  Kategorie.
--
--  Ergebnis: Hauptmenue -> "DH Tools" -> "XFBIN Import"
-- ============================================================

(
  local sSubMenuGuid   = "9676AC20-4B2E-48CC-8220-2BAE0D3E240A"
  local sActionGuid    = "F2181A1E-FE99-47D6-A1D9-54F9D9B0D102"
  local sMenuName      = "DH Tools"
  local sMacroName     = "XfbinImport_Open"
  local sMacroCategory = "DH Tools"

  -- MacroScript-Action-Table. Fest vorgegeben von Max.
  local iMacroActionTable = 647394

  fn XfbinRegisterMenu =
  (
    local mgr = callbacks.notificationParam()

    if (mgr == undefined) then
    (
      format "[XFBIN] Menue: kein MenuManager im Callback.\n"
    )
    else
    (
      try
      (
        local barMain = mgr.mainMenuBar
        local mnuSub  = barMain.CreateSubMenu sSubMenuGuid sMenuName

        local sPermanentId = sMacroName + "`" + sMacroCategory
        mnuSub.CreateAction sActionGuid iMacroActionTable sPermanentId

        format "XFBIN Import: Menue registriert (cuiRegisterMenus)\n"
      )
      catch
      (
        format "[XFBIN] Menue konnte nicht angelegt werden: %\n" \
               (getCurrentException())
      ) -- end of try/catch
    ) -- end of manager check
  ) -- end of XfbinRegisterMenu

  -- Erst abmelden, dann anmelden. Sonst haengt der Callback nach
  -- einem erneuten Ausfuehren des Skripts doppelt drin und das
  -- Menue entsteht zweimal.
  callbacks.removeScripts id:#xfbinImportMenu
  callbacks.addScript #cuiRegisterMenus XfbinRegisterMenu id:#xfbinImportMenu

  -- Startmeldung. Ohne die laesst sich im Listener nicht
  -- unterscheiden, ob das Paket geladen wurde und still war, oder
  -- ob es gar nicht erst angefasst wurde. Deshalb wird hier auch
  -- gleich gemeldet, ob die DLU da ist.
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
    format "XFBIN Import: Skript geladen, aber XfbinImport.dlu FEHLT.\n"
  )
  else
  (
    format "XFBIN Import %: Plugin geladen, Menue-Callback angemeldet.\n" sVer
  ) -- end of plugin check
)
