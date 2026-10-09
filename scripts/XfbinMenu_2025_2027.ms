-- ============================================================
--  XfbinMenu_2025_2027.ms
--  Menueeintraege fuer 3ds Max 2025 und neuer
--  Version 3.0.0
--
--  Ab 2025 ist das alte menuMan-API abgeloest. Menues werden
--  ueber einen Callback registriert, den der neue
--  CuiMenuManager ausloest (#cuiRegisterMenus). Das Skript
--  haengt sich dort ein, statt das Menue direkt zu bauen.
--
--  Laeuft als PRE-Start-up-Skript: der Callback muss stehen,
--  bevor Max die Menues aufbaut. Als Post-Start-up kam er zu
--  spaet, und das Menue erschien erst nach einem zweiten Neustart.
--
--  Die GUIDs unten sind fest und einmalig. Nicht aendern -
--  Max erkennt die Eintraege darueber wieder.
--
--  647394 ist die Action-Table-ID fuer MacroScripts. Der
--  "permanentId" ist der Makroname, ein Backtick und die
--  Kategorie.
--
--  Ergebnis: Hauptmenue -> "DH Tools" -> "XFBIN Import"
--                                     -> "XFBIN Animations"
-- ============================================================

(
  local sSubMenuGuid   = "9676AC20-4B2E-48CC-8220-2BAE0D3E240A"
  local sActionGuid    = "F2181A1E-FE99-47D6-A1D9-54F9D9B0D102"
  local sAnimGuid      = "56472014-5994-4568-8CC4-633F3A085AA3"
  local sMenuName      = "DH Tools"
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
        local mnuSub = mgr.mainMenuBar.CreateSubMenu sSubMenuGuid sMenuName
        if (mnuSub != undefined) then
        (
          mnuSub.CreateAction sActionGuid iMacroActionTable ("XfbinImport_Open`" + sMacroCategory)
          mnuSub.CreateAction sAnimGuid iMacroActionTable ("XfbinImport_Anim`" + sMacroCategory)
        )
        else
        (
          format "[XFBIN] CreateSubMenu lieferte undefined - GUID schon vergeben?\n"
        ) -- end of submenu check
      )
      catch
      (
        format "[XFBIN] Menue konnte nicht angelegt werden: %\n" \
               (getCurrentException())
      ) -- end of try/catch
    ) -- end of manager check
  ) -- end of XfbinRegisterMenu

  callbacks.removeScripts id:#xfbinImportMenu
  callbacks.addScript #cuiRegisterMenus XfbinRegisterMenu id:#xfbinImportMenu

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
    format "XFBIN Import: Menue-Callback angemeldet (Plugin noch nicht geladen).\n"
  )
  else
  (
    format "XFBIN Import %: Plugin geladen, Menue-Callback angemeldet.\n" sVer
  ) -- end of plugin check
)
