/*
    Sind die Bone-Matrizen heil?
    ============================
    Ins Viewport ziehen, waehrend die Animation gesetzt ist.

    Sucht das groesste geskinnte Objekt der Szene, holt sich
    dessen Skin-Bones und gibt deren Weltmatrizen aus - einmal
    auf Frame 0 und einmal auf dem aktuellen Frame.

    Damit trennt sich, woran es liegt:

      * Zahlen auf beiden Frames im Bereich -200..200
        -> Bones sind heil, der Fehler steckt im Skin selbst
           (Bind-Matrizen, Gewichtszuordnung).

      * Riesige Zahlen oder NaN/-1.#IND auf dem aktuellen Frame
        -> die Keys erzeugen kaputte Matrizen, der Fehler steckt
           im Setzen der Animation.

      * Riesige Zahlen schon auf Frame 0
        -> es lag nie an der Animation.
*/
(
  fn GetSkinMod obj =
  (
    local mResult = undefined
    for m in obj.modifiers while (mResult == undefined) do
    (
      if (classOf m == Skin) then (mResult = m)
    ) -- end of modifier loop
    mResult
  ) -- end of GetSkinMod

  fn BadNumber f =
  (
    -- NaN ist die einzige Zahl, die sich selbst ungleich ist.
    (f != f) or (abs f > 10000.0)
  ) -- end of BadNumber

  fn DumpNode n label =
  (
    local tm = n.transform
    local r1 = tm.row1
    local r2 = tm.row2
    local r3 = tm.row3
    local r4 = tm.row4

    local bBad = false
    for v in #(r1.x,r1.y,r1.z, r2.x,r2.y,r2.z, r3.x,r3.y,r3.z, r4.x,r4.y,r4.z) do
    (
      if (BadNumber v) then (bBad = true)
    ) -- end of value loop

    format "  % % pos=[%, %, %]  scale=%  %\n" \
           label n.name \
           (r4.x as integer) (r4.y as integer) (r4.z as integer) \
           n.scale \
           (if bBad then "<== KAPUTT" else "")
  ) -- end of DumpNode

  -- Groesstes geskinntes Objekt suchen
  local oWorst = undefined
  local fWorst = 0.0

  for o in geometry do
  (
    local sk = GetSkinMod o
    if (sk != undefined) then
    (
      local bb = nodeGetBoundingBox o (matrix3 1)
      local sz = bb[2] - bb[1]
      local mx = amax #(sz.x, sz.y, sz.z)
      if (mx > fWorst) then (fWorst = mx; oWorst = o)
    ) -- end of skin check
  ) -- end of object loop

  if (oWorst == undefined) then
  (
    format "\nKein geskinntes Objekt gefunden.\n"
  )
  else
  (
    local sk = GetSkinMod oWorst
    local iBones = skinOps.GetNumberBones sk

    format "\n=== Bone-Pruefung: % (Ausdehnung %) ===\n" \
           oWorst.name (fWorst as integer)
    format "Skin-Bones: %\n" iBones

    local aNodes = #()
    for i = 1 to iBones do
    (
      local sName = skinOps.GetBoneName sk i 1
      local n = getNodeByName sName
      if (n != undefined) then (append aNodes n)
      else (format "  Bone '%' nicht in der Szene gefunden!\n" sName)
    ) -- end of bone loop

    -- ------------------------------------------------------
    --  Die im Skin HINTERLEGTE Bind-Lage gegen die
    --  tatsaechliche Lage des Bones auf Frame 0 halten.
    --
    --  Das ist der entscheidende Vergleich: sind die Bones heil
    --  und die Verformung trotzdem absurd, dann stimmt genau
    --  hier etwas nicht. Skin merkt sich beim Hinzufuegen eines
    --  Bones dessen Matrix; weicht die vom Ist-Zustand ab, ist
    --  das Ergebnis Unsinn.
    -- ------------------------------------------------------
    format "\n-- Bind-Matrix im Skin gegen Ist-Lage auf Frame 0 --\n"
    for i = 1 to iBones do
    (
      local sName = skinOps.GetBoneName sk i 1
      local n = getNodeByName sName
      if (n != undefined) then
      (
        local bindTM = undefined
        try
        (
          bindTM = skinUtils.GetBoneBindTM oWorst n
        )
        catch
        (
          bindTM = undefined
        ) -- end of try/catch

        if (bindTM == undefined) then
        (
          format "  % : Bind-Matrix nicht auslesbar\n" sName
        )
        else
        (
          local actual = at time 0 n.transform
          local d = (bindTM.row4 - actual.row4)
          format "  % : bind=[%, %, %]  ist=[%, %, %]  Abstand=%\n" \
                 sName \
                 (bindTM.row4.x as integer) (bindTM.row4.y as integer) \
                 (bindTM.row4.z as integer) \
                 (actual.row4.x as integer) (actual.row4.y as integer) \
                 (actual.row4.z as integer) \
                 ((length d) as integer)
        ) -- end of bind check
      ) -- end of node check
    ) -- end of bind loop

    local tSaved = currentTime

    format "\n-- Frame 0 --\n"
    at time 0 for n in aNodes do (DumpNode n "  ")

    format "\n-- Frame % --\n" (tSaved as string)
    at time tSaved for n in aNodes do (DumpNode n "  ")

    -- Elternkette des ersten Bones, damit man sieht, wo es kippt
    format "\n-- Elternkette von % auf Frame % --\n" \
           aNodes[1].name (tSaved as string)
    local p = aNodes[1]
    local iGuard = 0
    while (p != undefined and iGuard < 30) do
    (
      at time tSaved DumpNode p "  "
      p = p.parent
      iGuard += 1
    ) -- end of parent walk

    format "=== Ende ===\n\n"
  ) -- end of found check
)
