/*
    Are the note tracks there?
    ==========================
    Drop into the viewport after "Load all as sequence".

    Lists the note tracks of the scene root - that is where the
    Warcraft 3 exporters look for them, and where the Animation
    Merge tool writes them.

    Also checks a few bones, in case a track ended up on the wrong
    node.
*/
(
  fn DumpTracks n label =
  (
    local iCount = numNoteTracks n
    format "% : % note track(s)\n" label iCount

    for i = 1 to iCount do
    (
      local nt = getNoteTrack n i
      format "   track % '%' : % key(s)\n" i nt.name nt.keys.count

      local iShow = amin 6 nt.keys.count
      for k = 1 to iShow do
      (
        format "      % @ %\n" nt.keys[k].value (nt.keys[k].time as string)
      ) -- end of key loop

      if (nt.keys.count > iShow) then
      (
        format "      ... % more\n" (nt.keys.count - iShow)
      ) -- end of rest check
    ) -- end of track loop
  ) -- end of DumpTracks

  format "\n=== Note track check ===\n"
  DumpTracks rootNode "scene root"

  format "\nBones carrying note tracks:\n"
  local iFound = 0
  for o in objects do
  (
    if ((numNoteTracks o) > 0) then
    (
      DumpTracks o ("  " + o.name)
      iFound += 1
    ) -- end of track check
  ) -- end of object loop

  if (iFound == 0) then
  (
    format "  none - correct, they belong on the scene root\n"
  ) -- end of none check

  format "\nAnimation range: % to %\n" \
         (animationRange.start as string) (animationRange.end as string)
  format "=== end ===\n\n"
)
