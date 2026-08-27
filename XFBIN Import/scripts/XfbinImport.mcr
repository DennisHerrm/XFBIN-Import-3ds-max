-- ============================================================
--  XfbinImport.mcr - user interface for the XFBIN Import plugin
--  Version 1.9.3
--
--  Three steps:
--    1. pick the FOLDER holding the .xfbin files
--    2. Import - bones, meshes, skinning, textures in one go
--    3. either apply a single animation, or load them all as a
--       sequence with a note track
--
--  1.9.3: hybrid files (clump+anm) also feed the anim list;
--  sequence skips non-skeletal clips; import always clears
--  plugin scene state before building.
--
--  ------------------------------------------------------------
--  LAYOUT RULES kept here
--  ------------------------------------------------------------
--  * across:N splits the rollout into N EQUALLY WIDE columns. A
--    wide field in column 1 reaches into column 2, so whatever
--    sits next to it MUST be align:#right.
--  * height counts TEXT ROWS on listBox and dropDownList, not
--    pixels.
--  * No pos:[x,y] - absolute positions do not scale with the
--    Windows display scaling. Fine-tuning only via offset:.
--  * Comments with --, never //.
--  * ASCII only.
--  * Functions come BEFORE their callers. MAXScript resolves
--    names in a rollout strictly top to bottom.
--
--  Before shipping: python tools\PRUEFE_SCRIPTS.py
-- ============================================================

macroScript XfbinImport_Open
  category:"DH Tools"
  buttonText:"XFBIN Import"
  toolTip:"XFBIN Import - model and animations"
(
  fn GetPluginVersion =
  (
    local sResult = undefined
    try
    (
      sResult = XfbinCpp.version()
    )
    catch
    (
      sResult = undefined
    ) -- end of try/catch
    sResult
  ) -- end of GetPluginVersion

  fn GetMaxYear =
  (
    local iYear = 0
    try
    (
      iYear = (maxVersion())[8]
    )
    catch
    (
      iYear = 0
    ) -- end of try/catch
    iYear
  ) -- end of GetMaxYear

  rollout rltXfbin "XFBIN Import" width:452
  (
    local aModelFiles = #()
    local aAnimFiles  = #()

    label lblPlugin "Plugin: -" align:#left  across:2 offset:[0,3]
    label lblMax    ""          align:#right          offset:[0,3]

    group "Source folder"
    (
      editText edtDir    ""         width:334 align:#left across:2 offset:[-6,0]
      button   btnBrowse "Browse..." width:76 height:21 align:#right offset:[2,-2]

      label lblFound "No folder selected." align:#left width:416 offset:[-4,2]

      checkBox chkClear    "Clear scene first"        align:#left across:3 offset:[-4,4]
      checkBox chkTextures "Import textures" checked:true align:#center offset:[-16,4]
      checkBox chkMedit    "Fill Material Editor" checked:true \
               align:#right offset:[4,4]

      checkBox chkLayers "Sort into layers (per model, meshes and bones apart)" \
               checked:true align:#left offset:[-4,4]

      button btnImport "Import" width:200 height:26 align:#center \
             offset:[0,4] enabled:false
    )

    group "Animation"
    (
      label lblAnims "-" align:#left width:416 offset:[-4,0]

      dropDownList ddlAnim "" width:270 across:2 align:#left offset:[-4,2]
      button btnBuildAnim "Apply selected" width:140 height:23 \
             align:#right offset:[4,4] enabled:false

      checkBox chkNotes "Create note track" checked:true \
               align:#left across:4 offset:[-4,4]
      checkBox chkIdle "Rest keys" checked:true align:#left offset:[-46,4]
      checkBox chkVis  "Visibility" checked:true align:#left offset:[-88,4]
      checkBox chkMatAnim "Material anim" checked:true align:#left offset:[-130,4]
      spinner spnGap "Gap: " range:[0, 500, 10] type:#integer \
              fieldWidth:44 align:#center offset:[42,6]
      button btnSequence "Load all as sequence" width:140 height:23 \
             align:#right offset:[4,4] enabled:false
    )

    group "Options"
    (
      dropDownList ddlMode "" items:#("Point helpers", "Bone objects") \
                   selection:2 width:132 across:3 align:#left offset:[-4,2]
      spinner spnScale "Scale: " range:[0.001, 1000.0, 1.0] \
              type:#float fieldWidth:52 align:#center offset:[0,6]
      checkBox chkSkipLod "Skip LOD" checked:true align:#right offset:[4,6]

      spinner spnBoneSize "Bone size: " range:[0.0, 100.0, 0.0] \
              type:#float fieldWidth:52 align:#left across:2 offset:[-4,4]

      checkBox chkNormals "Explicit normals" checked:true \
               align:#left across:2 offset:[-4,4]
      checkBox chkSkin    "Skin modifier" checked:true align:#left offset:[-72,4]
    )

    group "Status"
    (
      label lblScene  "Scene: no skeleton" align:#left width:416 offset:[-4,0]
      label lblStatus "Ready."             align:#left width:416 offset:[-4,0]
      label lblTime   ""                   align:#left width:416 offset:[-4,0]

      checkBox chkDebug "Log to Listener" align:#left across:2 offset:[-4,4]
      button   btnLog   "Show log" width:150 height:21 align:#right offset:[4,2]

      button btnReport "Show largest objects" width:200 height:21 \
             align:#left across:2 offset:[-4,2]
      button btnDumps  "Write dumps..." width:150 height:21 \
             align:#right offset:[4,2]
    )

    -- --------------------------------------------------------
    --  Helpers. Order matters - see header.
    -- --------------------------------------------------------
    fn RefreshScene =
    (
      local iScene = XfbinCpp.sceneBoneCount()
      if (iScene > 0) then
      (
        lblScene.text = "Scene: " + (iScene as string) + " bones (" + \
                        (XfbinCpp.sceneClumpName()) + ")"
      )
      else
      (
        lblScene.text = "Scene: no skeleton"
      ) -- end of scene check
    ) -- end of RefreshScene

    fn ShowWarnings =
    (
      local sWarn = XfbinCpp.warnings()
      if (sWarn != "") then
      (
        format "[XFBIN] Notes:\n%\n" sWarn
      ) -- end of warnings check
    ) -- end of ShowWarnings

    -- Put the materials of the imported objects into the slots of
    -- the Compact Material Editor. Done here rather than in the
    -- plugin: after the import the materials hang off the objects
    -- anyway, and setMeditMaterial takes three lines.
    fn FillMaterialEditor =
    (
      local aMats = #()

      for o in geometry do
      (
        local m = o.material
        if (m != undefined and (findItem aMats m) == 0) then
        (
          append aMats m
        ) -- end of duplicate check
      ) -- end of object loop

      local iSlots = amin 24 aMats.count

      for i = 1 to iSlots do
      (
        setMeditMaterial i aMats[i]
      ) -- end of slot loop

      iSlots
    ) -- end of FillMaterialEditor

    -- Max creates bone objects with width and height 4. At 222
    -- bones that turns the rig into a carpet of little boxes and
    -- hides the model underneath. Zero makes them render as bare
    -- lines, which is what a game rig wants.
    --
    -- Done here rather than in the plugin: the bone object keeps
    -- these in a parameter block, and reaching into it from C++
    -- means hard-coded indices that differ between SDK versions.
    -- From MAXScript they are named properties.
    fn ResizeBones fSize =
    (
      local iCount = 0

      for o in objects do
      (
        if (classOf o == BoneGeometry) then
        (
          try
          (
            o.width  = fSize
            o.height = fSize
            iCount += 1
          )
          catch
          (
            -- some bone-like object without these properties
          ) -- end of try/catch
        ) -- end of class check
      ) -- end of object loop

      iCount
    ) -- end of ResizeBones

    -- Sort everything into layers: one for the meshes of a
    -- model, one for its bones.
    --
    -- The plugin knows which node belongs to which skeleton and
    -- hands that over as text - one line per node with clump,
    -- instance, kind and handle. Building the layers themselves
    -- is left to MaxScript, where a layer is two named calls;
    -- in C++ it would be ILayerManager and ILayerProperties.
    fn SortIntoLayers =
    (
      local sReport = XfbinCpp.layerReport()
      if (sReport == "") then (return 0)

      local aLines = filterString sReport "\n" splitEmptyTokens:false
      local iMoved = 0

      -- No "continue" here: MAXScript does not have it. Nested
      -- ifs instead - longer, but it runs.
      for sLine in aLines do
      (
        local aPart = filterString sLine "\t" splitEmptyTokens:true

        if (aPart.count >= 4) then
        (
          local sClump  = aPart[1]
          local iInst   = aPart[2] as integer
          local sKind   = aPart[3]
          local iHandle = aPart[4] as integer

          local n = maxOps.getNodeByHandle iHandle

          if (n != undefined) then
          (
            -- "2peabod1 Bones", and " #2" from the second
            -- instance on
            local sName = sClump
            if (iInst > 0) then
            (
              sName += " #" + ((iInst + 1) as string)
            ) -- end of instance check

            if (sKind == "bone") then
            (
              sName += " Bones"
            )
            else
            (
              sName += " Meshes"
            ) -- end of kind check

            local lyr = LayerManager.getLayerFromName sName
            if (lyr == undefined) then
            (
              lyr = LayerManager.newLayerFromName sName
            ) -- end of layer check

            if (lyr != undefined) then
            (
              lyr.addNode n
              iMoved += 1
            ) -- end of add check
          ) -- end of node check
        ) -- end of part check
      ) -- end of line loop

      iMoved
    ) -- end of SortIntoLayers

    -- Safety net for the visibility tangents.
    --
    -- Since 1.9.2 the plugin writes them as step keys itself,
    -- through IKeyControl. This pass costs one walk over the
    -- scene and makes sure nothing slipped through - for
    -- instance keys that some other tool added in between.
    fn StepVisibilityKeys =
    (
      local iKeys = 0

      for o in objects do
      (
        local vc = try (o.visibility.controller) catch undefined

        if (vc != undefined) then
        (
          try
          (
            for k in vc.keys do
            (
              k.inTangentType  = #step
              k.outTangentType = #step
              iKeys += 1
            ) -- end of key loop
          )
          catch
          (
            -- controller without keys, or one that has no
            -- tangent types - nothing to do
          ) -- end of try/catch
        ) -- end of controller check
      ) -- end of object loop

      iKeys
    ) -- end of StepVisibilityKeys

    -- Two note keys per sequence, at its start and its end. Same
    -- shape as the Animation Merge tool uses, so an exporter that
    -- reads one reads the other.
    fn AddNoteKeys nt sName t0 t1 =
    (
      local k0 = addNewNoteKey nt.keys t0
      k0.value = sName
      local k1 = addNewNoteKey nt.keys t1
      k1.value = sName
    ) -- end of AddNoteKeys

    -- Scan a folder and sort its .xfbin files.
    --
    -- The sorting goes by CONTENT, not by file name: a file with
    -- nuccChunkClump is a model, one with nuccChunkAnm is an
    -- animation file. Names like "1hakbod1" and "1hakbod1c" are a
    -- convention one does not have to rely on.
    fn ScanFolder sDir =
    (
      aModelFiles = #()
      aAnimFiles  = #()

      local aFiles = getFiles (sDir + "\\*.xfbin")

      for f in aFiles do
      (
        if ((XfbinCpp.open f) > 0) then
        (
          local iClump = XfbinCpp.countOfType "nuccChunkClump"
          local iAnm   = XfbinCpp.countOfType "nuccChunkAnm"

          -- Unabhaengig, nicht else-if: viele Charakter-Dateien
          -- (Kabuto 2kbxbod1c/l/s, 2kbxspl1, ...) tragen Clump UND
          -- Anm im selben XFBIN. Nur als Modell zu zaehlen hiess,
          -- ihre Animationen nie ueber parseAnimsAppend zu laden.
          if (iClump > 0) then
          (
            append aModelFiles f
          ) -- end of clump check

          if (iAnm > 0) then
          (
            append aAnimFiles f
          ) -- end of anm check
        ) -- end of open check
      ) -- end of file loop

      XfbinCpp.close()

      -- Sort model files by bone count, biggest first. A
      -- character is spread over several files - the body plus
      -- accessories like weapons - and the body should be the
      -- one that defines the scene root. Alphabetically
      -- "1hakacc1" comes before "1hakbod1", so file order will
      -- not do.
      if (aModelFiles.count > 1) then
      (
        local aCounts = #()
        for f in aModelFiles do
        (
          XfbinCpp.open f
          append aCounts (XfbinCpp.boneCount())
        ) -- end of count loop
        XfbinCpp.close()

        for i = 1 to (aModelFiles.count - 1) do
        (
          for j = 1 to (aModelFiles.count - i) do
          (
            if (aCounts[j] < aCounts[j + 1]) then
            (
              local tf = aModelFiles[j]
              aModelFiles[j] = aModelFiles[j + 1]
              aModelFiles[j + 1] = tf
              local tc = aCounts[j]
              aCounts[j] = aCounts[j + 1]
              aCounts[j + 1] = tc
            ) -- end of swap check
          ) -- end of inner loop
        ) -- end of sort loop
      ) -- end of multi model check

      lblFound.text = (aModelFiles.count as string) + " model(s), " + \
                      (aAnimFiles.count as string) + " animation file(s) " + \
                      "in " + (aFiles.count as string) + " file(s)"

      btnImport.enabled = (aModelFiles.count > 0 or aAnimFiles.count > 0)
    ) -- end of ScanFolder

    fn FillAnimList =
    (
      local iCount = XfbinCpp.animCount()
      local aNames = #()

      if (iCount > 0) then
      (
        for i = 0 to (iCount - 1) do
        (
          append aNames ((i as string) + ": " + (XfbinCpp.animName i))
        ) -- end of name loop
      ) -- end of count check

      ddlAnim.items = aNames

      local aLines = filterString (XfbinCpp.animSummary()) "\n" \
                     splitEmptyTokens:false
      if (aLines.count >= 1) then
      (
        lblAnims.text = trimLeft aLines[1]
      )
      else
      (
        lblAnims.text = "-"
      ) -- end of line check

      local bReady = (iCount > 0 and XfbinCpp.sceneBoneCount() > 0)
      btnBuildAnim.enabled = bReady
      btnSequence.enabled  = bReady
    ) -- end of FillAnimList

    -- --------------------------------------------------------
    --  Events
    -- --------------------------------------------------------
    on rltXfbin open do
    (
      local iYear = GetMaxYear()
      if (iYear > 0) then
      (
        lblMax.text = "3ds Max " + (iYear as string)
      ) -- end of year check

      local sVer = GetPluginVersion()

      if (sVer == undefined) then
      (
        lblPlugin.text    = "Plugin MISSING"
        lblStatus.text    = "XfbinImport.dlu is not loaded - check the Plugin Manager."
        btnBrowse.enabled = false
      )
      else
      (
        lblPlugin.text = "Plugin " + sVer

        RefreshScene()
      ) -- end of plugin check
    ) -- end of open handler

    on spnBoneSize changed fVal do
    (
      -- Wirkt sofort, damit man die Groesse ausprobieren kann,
      -- ohne neu zu importieren.
      local n = ResizeBones fVal
      if (n > 0) then
      (
        lblStatus.text = (n as string) + " bones resized."
      ) -- end of count check
    ) -- end of spnBoneSize changed

    on chkDebug changed bState do
    (
      XfbinCpp.setDebug (if bState then 1 else 0)
    ) -- end of chkDebug changed

    on btnBrowse pressed do
    (
      local sPicked = getSavePath caption:"Pick the folder holding the XFBIN files"

      if (sPicked != undefined) then
      (
        edtDir.text    = sPicked
        lblStatus.text = "Scanning folder..."
        windows.processPostedMessages()
        ScanFolder sPicked
        lblStatus.text = "Ready."
      ) -- end of picked check
    ) -- end of btnBrowse pressed

    on edtDir changed sText do
    (
      if (sText != "" and (doesFileExist sText)) then
      (
        ScanFolder sText
      ) -- end of exists check
    ) -- end of edtDir changed

    on btnImport pressed do
    (
      -- Plugin-Zustand ZUERST vergessen, bevor Max die Knoten
      -- loescht: sonst zeigen sceneMaterials_ noch auf Mtl*-
      -- Zeiger, die delete objects gleich ungueltig macht.
      XfbinCpp.clearScene()

      if (chkClear.checked) then
      (
        delete objects
        XfbinCpp.clearScene()
      ) -- end of clear check

      local iMode    = ddlMode.selection - 1
      local fScale   = spnScale.value
      local iSkipLod = if chkSkipLod.checked then 1 else 0
      local iNormals = if chkNormals.checked then 1 else 0
      local iSkin    = if chkSkin.checked    then 1 else 0

      -- Die Bone-Groesse geht ans Plugin, bevor etwas angelegt
      -- wird: dort wird sie beim Erzeugen des Bone-Objekts
      -- gesetzt. Der frueher noetige Nachlauf entfaellt damit -
      -- und mit ihm die Moeglichkeit, ihn zu vergessen.
      XfbinCpp.setBoneSize spnBoneSize.value

      local iBones  = 0
      local iMeshes = 0
      local iTex    = 0

      -- ------------------------------------------------------
      --  Animations first, then models.
      --
      --  The animation files decide how many copies of each
      --  skeleton are needed - a character can carry the same
      --  weapon twice, or summon three of the same creature. So
      --  they are read first; the plugin then works out the
      --  count per clump on its own.
      --
      --  ALL animation files are loaded, not just one. Pein
      --  brings four of them, together 104 animations, and they
      --  address seventeen different skeletons between them.
      --  Hybrid files (clump+anm) are in BOTH lists since 1.9.3.
      -- ------------------------------------------------------
      local iAnims = 0
      XfbinCpp.clearAnims()

      for f in aAnimFiles do
      (
        lblStatus.text = "Reading " + (filenameFromPath f) + " ..."
        windows.processPostedMessages()

        if ((XfbinCpp.open f) > 0) then
        (
          XfbinCpp.parseAnimsAppend()
        ) -- end of open check
      ) -- end of animation file loop

      iAnims = XfbinCpp.animCount()

      for i = 1 to aModelFiles.count do
      (
        local f = aModelFiles[i]

        lblStatus.text = "Loading " + (filenameFromPath f) + " ..."
        windows.processPostedMessages()

        if ((XfbinCpp.open f) > 0) then
        (
          -- 0 = let the plugin decide per clump how many
          -- instances the animations expect.
          local iB = XfbinCpp.buildSkeletonN iMode fScale 0
          local iM = XfbinCpp.buildMeshesN iSkipLod iNormals iSkin fScale 0

          iBones  += iB
          iMeshes += iM

          format "[XFBIN] % : % bone(s), % object(s)\n" \
                 (filenameFromPath f) iB iM

          if (chkTextures.checked) then
          (
            local sTexDir = (getFilenamePath f) + "textures"
            makeDir sTexDir all:true
            iTex += XfbinCpp.exportTextures sTexDir
            -- ACCESS_VIOLATION in der DLU faengt MaxScript nicht
            -- zuverlaessig ab; clearScene oben ist die eigentliche
            -- Absicherung. try/catch fangt nur MAXScript-Fehler.
            try
            (
              XfbinCpp.buildMaterials sTexDir
            )
            catch
            (
              format "[XFBIN] buildMaterials failed: %\n" \
                     (getCurrentException())
            ) -- end of try/catch
          ) -- end of texture check

          ShowWarnings()
        ) -- end of open check
      ) -- end of model loop

      FillAnimList()

      RefreshScene()

      local iLayers = 0
      if (chkLayers.checked and iMeshes > 0) then
      (
        iLayers = SortIntoLayers()
      ) -- end of layer check

      local iSlots = 0
      if (chkMedit.checked and iMeshes > 0) then
      (
        iSlots = FillMaterialEditor()
      ) -- end of medit check

      lblStatus.text = (iBones as string) + " bones, " + \
                       (iMeshes as string) + " objects, " + \
                       (iTex as string) + " textures, " + \
                       (iAnims as string) + " animations, " + \
                       (iSlots as string) + " editor slots, " + \
                       (iLayers as string) + " nodes in layers."
      lblTime.text   = XfbinCpp.timings()
    ) -- end of btnImport pressed

    on btnBuildAnim pressed do
    (
      local iIndex = ddlAnim.selection - 1

      if (iIndex < 0) then
      (
        lblStatus.text = "No animation selected."
      )
      else
      (
        lblStatus.text = "Setting keys..."
        windows.processPostedMessages()

        -- 7 = position, rotation and scale.
        local iKeys = XfbinCpp.buildAnimEx iIndex 7 spnScale.value

        -- Auch hier die Sichtbarkeit: sonst steht bei einer
        -- einzeln gesetzten Animation alles gleichzeitig da.
        if (chkVis.checked and iKeys > 0) then
        (
          XfbinCpp.buildVisibility iIndex 0.0 (XfbinCpp.animFrames iIndex)
          StepVisibilityKeys()
        ) -- end of visibility check

        if (chkMatAnim.checked and iKeys > 0) then
        (
          iKeys += XfbinCpp.buildMaterialAnim iIndex 0.0 \
                   (XfbinCpp.animFrames iIndex)
        ) -- end of material check

        if (iKeys <= 0) then
        (
          lblStatus.text = "ERROR: " + (XfbinCpp.lastError())
        )
        else
        (
          lblStatus.text = (iKeys as string) + " keys set."
          lblTime.text   = XfbinCpp.timings()
          ShowWarnings()
        ) -- end of result check
      ) -- end of index check
    ) -- end of btnBuildAnim pressed

    -- Sequence mode.
    --
    -- Frame 0 holds the bind pose as a key, then every animation
    -- follows in turn with a gap between them, and a note track
    -- on the root bone names the ranges. That is the layout the
    -- Warcraft 3 tools expect, and the same one the Animation
    -- Merge tool produces.
    on btnSequence pressed do
    (
      local iCount = XfbinCpp.animCount()

      if (iCount <= 0) then
      (
        lblStatus.text = "No animations loaded."
      )
      else
      (
        -- ----------------------------------------------------
        --  The note track goes on rootNode - the SCENE root, not
        --  a bone.
        --
        --  rootNode is a MAXScript constant, which is exactly why
        --  the Animation Merge script never assigns it: it is
        --  simply there. I had read that as an undefined variable
        --  and hung the track on the character root bone instead,
        --  where the exporter does not look for it.
        -- ----------------------------------------------------
        local NT = undefined

        if (chkNotes.checked) then
        (
          while ((numNoteTracks rootNode) != 0) do
          (
            deleteNoteTrack rootNode (getNoteTrack rootNode 1)
          ) -- end of delete loop

          NT = notetrack "animations"
          addNoteTrack rootNode NT
        ) -- end of note track setup

        local iGap  = spnGap.value
        local iKeys = XfbinCpp.buildBindPoseKey 0.0

        local fAt = iGap as float

        disableSceneRedraw()
        progressStart "Loading animations..."

        try
        (
          local iSkipped = 0

          for i = 0 to (iCount - 1) do
          (
            progressUpdate (100.0 * (i + 1) / iCount)

            -- Post-process clips (Blur, Glare, DOF, ColorFilter, ...)
            -- have no bone entries. Sequencing them hid every mesh
            -- or poked missing material targets - crash on Kabuto
            -- 2kbxspl1. Keep them in the dropdown for inspection,
            -- but do not put them on the timeline.
            if ((XfbinCpp.animIsSkeletal i) == 0) then
            (
              iSkipped += 1
            )
            else
            (
              local fLen = XfbinCpp.animFrames i

              -- Every sequence must stand on its own. A typical
              -- animation only touches 112 of the 222 character
              -- bones, and some do not touch the weapons at all.
              -- Without a key at both ends those bones have no key
              -- inside the sequence at all, and Max interpolates
              -- straight across the gap - the previous animation
              -- bleeds into the next one.
              if (chkIdle.checked) then
              (
                XfbinCpp.buildIdleKeys i fAt (fAt + fLen)
              ) -- end of idle check

              -- Objects belonging to a model this animation never
              -- mentions are hidden for its stretch of the
              -- timeline. Without it every summoned creature
              -- stands around in all 104 sequences.
              if (chkVis.checked) then
              (
                XfbinCpp.buildVisibility i fAt (fAt + fLen)
              ) -- end of visibility check

              -- UV offset, tiling and opacity of the materials.
              -- In this data it is mostly UV scrolling on eyes,
              -- hair and effect surfaces.
              if (chkMatAnim.checked) then
              (
                iKeys += XfbinCpp.buildMaterialAnim i fAt (fAt + fLen)
              ) -- end of material check

              iKeys += XfbinCpp.buildAnimAt i fAt 7 spnScale.value

              if (NT != undefined) then
              (
                AddNoteKeys NT (XfbinCpp.animName i) fAt (fAt + fLen)
              ) -- end of note check

              fAt = fAt + fLen + iGap
            ) -- end of skeletal check
          ) -- end of animation loop

          if (iSkipped > 0) then
          (
            format "[XFBIN] Sequence: skipped % non-skeletal clip(s)\n" \
                   iSkipped
          ) -- end of skip log
        )
        catch
        (
          format "[XFBIN] Sequence aborted: %\n" (getCurrentException())
        ) -- end of try/catch

        progressEnd()

        -- Once, at the end - not per animation. Walking every
        -- object 104 times would take far longer than the import
        -- itself.
        local iStep = 0
        if (chkVis.checked) then
        (
          iStep = StepVisibilityKeys()
        ) -- end of step check

        enableSceneRedraw()

        animationRange = interval 0 fAt

        lblStatus.text = (iCount as string) + " animations, " + \
                         (iKeys as string) + " keys, " + \
                         ((fAt as integer) as string) + " frames, " + \
                         (iStep as string) + " step keys" + \
                         (if (NT != undefined) then ", note track on scene root." \
                          else ", no note track.")
        lblTime.text   = XfbinCpp.timings()
        ShowWarnings()
      ) -- end of count check
    ) -- end of btnSequence pressed

    on btnDumps pressed do
    (
      local sDir = getSavePath caption:"Pick a folder for the dumps"

      if (sDir != undefined) then
      (
        local iCount = 0

        for f in (aModelFiles + aAnimFiles) do
        (
          if ((XfbinCpp.open f) > 0) then
          (
            local sBase = sDir + "\\" + (getFilenameFile f)

            XfbinCpp.dump (sBase + "_container.txt") 1
            iCount += 1

            if ((XfbinCpp.countOfType "nuccChunkClump") > 0) then
            (
              XfbinCpp.boneDump (sBase + "_bones.txt")
              XfbinCpp.meshDump (sBase + "_meshes.txt") 1
            ) -- end of clump check

            if ((XfbinCpp.countOfType "nuccChunkAnm") > 0) then
            (
              XfbinCpp.animDump (sBase + "_anims.txt") 1
            ) -- end of anim check
          ) -- end of open check
        ) -- end of file loop

        lblStatus.text = "Dumps written for " + (iCount as string) + " file(s)."
      ) -- end of path check
    ) -- end of btnDumps pressed

    on btnReport pressed do
    (
      format "\n----- XFBIN scene report -----\n%------------------------------\n" \
             (XfbinCpp.sceneReport())
      lblStatus.text = "Scene report is in the Listener."
    ) -- end of btnReport pressed

    on btnLog pressed do
    (
      format "\n----- XFBIN log -----\n%\n---------------------\n" \
             (XfbinCpp.log())
      lblStatus.text = "Log is in the Listener."
    ) -- end of btnLog pressed

  ) -- end of rltXfbin

  on execute do
  (
    try
    (
      destroyDialog rltXfbin
    )
    catch
    (
      -- was not open - nothing to do
    ) -- end of try/catch

    createDialog rltXfbin
  ) -- end of execute

) -- end of macroScript
