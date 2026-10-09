/*
    XFBIN Import  --  Starter
    =========================
    Version 3.0.0

    Diese Datei brauchst du im Normalfall NICHT - nach der
    Installation steht das Werkzeug im Menue:

        Hauptmenue  ->  Ninja Storm Importer  ->  Import Ninja Storm

    und unter Customize -> Customize User Interface ->
    Category "Ninja Storm Importer".

    Seit 3.0 sind die Fenster Teil des C++ Plugins. Dieser Starter
    oeffnet das Figurenfenster direkt - zum Beispiel per Drag & Drop
    in den Viewport, falls das Menue (noch) fehlt.

    Benoetigt das C++ Plugin XfbinImport.dlu.
*/

(
  local oX = undefined
  try
  (
    oX = XfbinCpp
  )
  catch
  (
    oX = undefined
  ) -- end of try/catch

  if (oX == undefined) then
  (
    format "XFBIN Import: XfbinImport.dlu ist nicht geladen - Customize -> Plugin Manager pruefen.\n"
    messageBox "XfbinImport.dlu is not loaded.\n\nRun the setup again and restart 3ds Max." title:"XFBIN Import"
  )
  else
  (
    format "XFBIN Import %: Fenster wird geoeffnet.\n" (oX.version())
    oX.showDialog()
  ) -- end of plugin check
)
