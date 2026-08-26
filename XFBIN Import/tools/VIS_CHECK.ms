/*
    Hat jede Sequenz je Mesh zwei Sichtbarkeits-Keys?
    =================================================
    Ins Viewport ziehen, nachdem "Load all as sequence" gelaufen
    ist.

    Liest die Sequenzen aus dem Note Track des Szenen-Wurzel-
    knotens und prueft fuer jedes geskinnte oder ungeskinnte
    Mesh, ob im Bereich jeder Sequenz mindestens zwei
    Sichtbarkeits-Keys liegen - einer am Anfang, einer am Ende.

    Genau das braucht der Warcraft-3-Export: auch eine Sequenz,
    in der ein Objekt durchgehend unsichtbar ist, muss die beiden
    Keys haben.
*/
(
  local nt = undefined

  if ((numNoteTracks rootNode) > 0) then
  (
    nt = getNoteTrack rootNode 1
  ) -- end of track check

  if (nt == undefined) then
  (
    format "\nKein Note Track auf dem Szenen-Wurzelknoten.\n"
    format "Erst 'Load all as sequence' laufen lassen.\n\n"
  )
  else
  (
    -- Aus den Keys Paare bilden: je Sequenz Anfang und Ende
    local aSeq = #()
    local i = 1

    while (i < nt.keys.count) do
    (
      local k0 = nt.keys[i]
      local k1 = nt.keys[i + 1]

      if (k0.value == k1.value) then
      (
        append aSeq #(k0.value, k0.time, k1.time)
        i += 2
      )
      else
      (
        i += 1
      ) -- end of pair check
    ) -- end of while

    format "\n=== Sichtbarkeit je Sequenz ===\n"
    format "Sequenzen im Note Track: %\n" aSeq.count

    local aMeshes = #()
    for o in geometry do (append aMeshes o)
    format "Meshes: %\n\n" aMeshes.count

    local iBad = 0
    local iChecked = 0

    for s in aSeq do
    (
      for o in aMeshes do
      (
        local vc = try (o.visibility.controller) catch undefined
        local n = 0

        if (vc != undefined) then
        (
          for k in vc.keys do
          (
            if (k.time >= s[2] and k.time <= s[3]) then (n += 1)
          ) -- end of key loop
        ) -- end of controller check

        iChecked += 1

        if (n < 2) then
        (
          iBad += 1
          if (iBad <= 15) then
          (
            format "  FEHLT: % in '%' -> nur % Key(s)\n" o.name s[1] n
          ) -- end of report limit
        ) -- end of missing check
      ) -- end of mesh loop
    ) -- end of sequence loop

    format "\n% Paarungen geprueft.\n" iChecked

    if (iBad == 0) then
    (
      format "Jede Sequenz hat je Mesh mindestens zwei Keys.\n\n"
    )
    else
    (
      format "% Paarungen mit zu wenigen Keys.\n\n" iBad
    ) -- end of result check
  ) -- end of track check
)
