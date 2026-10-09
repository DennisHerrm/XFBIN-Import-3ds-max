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
--  Max erkennt die Eintraege darueber wieder. Seit 3.0 neue
--  GUIDs: das Menue heisst nicht mehr "DH Tools", und mit den
--  alten haette Max womoeglich den alten Namen weiterbenutzt.
--
--  647394 ist die Action-Table-ID fuer MacroScripts. Der
--  "permanentId" ist der Makroname, ein Backtick und die
--  Kategorie.
--
--  Ergebnis: Hauptmenue -> "Ninja Storm Importer" -> "Import Ninja Storm"
--                                                 -> "Ninja Storm Animations"
-- ============================================================

(
  local sSubMenuGuid   = "54C69987-A441-4384-A9BE-27D5DC90A741"
  local sActionGuid    = "253F5695-5D5F-44CD-A2D0-BEDFC1DE790E"
  local sAnimGuid      = "2DEE7480-081A-436B-A9DE-7D6CE175B9DC"
  local sMenuName      = "Ninja Storm Importer"
  local sMacroCategory = "Ninja Storm Importer"

  -- MacroScript-Action-Table. Fest vorgegeben von Max.
  local iMacroActionTable = 647394

  fn XfbinRegisterMenu =
  (
    local mgr = callbacks.notificationParam()

    if (mgr == undefined) then
    (
      format "[NinjaStorm] Menue: kein MenuManager im Callback.\n"
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
          format "[NinjaStorm] CreateSubMenu lieferte undefined - GUID schon vergeben?\n"
        ) -- end of submenu check
      )
      catch
      (
        format "[NinjaStorm] Menue konnte nicht angelegt werden: %\n" \
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
    format "Ninja Storm Importer: Menue-Callback angemeldet (Plugin noch nicht geladen).\n"
  )
  else
  (
    format "Ninja Storm Importer %: Plugin geladen, Menue-Callback angemeldet.\n" sVer
  ) -- end of plugin check
)
