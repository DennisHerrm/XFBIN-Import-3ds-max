/*
    Welches Objekt ist zu gross?
    ============================
    Ins Viewport ziehen oder in den Listener einfuegen, waehrend
    die Animation gesetzt ist. Listet die zehn groessten Objekte
    der Szene mit ihrer Ausdehnung am AKTUELLEN Frame, dazu
    Skalierung und Elternteil.

    Ein Charakter ist rund 180 Einheiten hoch. Alles, was
    deutlich darueber liegt, ist der Uebeltaeter.
*/
(
  local aRows = #()

  for o in objects do
  (
    local bb = nodeGetBoundingBox o (matrix3 1)
    local sz = bb[2] - bb[1]
    local mx = amax #(sz.x, sz.y, sz.z)
    append aRows #(mx, o.name, sz, o.scale, (if o.parent != undefined then o.parent.name else "-"))
  ) -- end of object loop

  fn CompareBySize a b = (if a[1] < b[1] then 1 else if a[1] > b[1] then -1 else 0)
  qsort aRows CompareBySize

  format "\n=== Groesste Objekte auf Frame % ===\n" (currentTime as string)
  for i = 1 to (amin 10 aRows.count) do
  (
    local r = aRows[i]
    format "% : max=%  bbox=%  scale=%  parent=%\n" \
           r[2] (r[1] as integer) r[3] r[4] r[5]
  ) -- end of row loop

  format "\n=== Bones mit auffaelliger Skalierung ===\n"
  local iOdd = 0
  for o in objects do
  (
    local s = o.scale
    if (abs (s.x - 1.0) > 0.5 or abs (s.y - 1.0) > 0.5 or abs (s.z - 1.0) > 0.5) then
    (
      format "% : scale=%\n" o.name s
      iOdd += 1
    ) -- end of scale check
  ) -- end of object loop
  if (iOdd == 0) then format "keine\n"
  format "\n"
)
